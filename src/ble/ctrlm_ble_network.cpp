/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2014 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
// Includes

#include "../ctrlm.h"
#include "ctrlm_ble_utils.h"
#include "ctrlm_hal_ble.h"
#include "ctrlm_ble_network.h"
#include "ctrlm_ble_controller.h"
#include "ctrlm_ble_iarm.h"
#include "ctrlm_ipc.h"
#include "ctrlm_ipc_key_codes.h"
#include "../ctrlm_voice.h"
#include "../ctrlm_voice_obj.h"
#include "../ctrlm_database.h"
#include "../ctrlm_rcu.h"
#include "../ctrlm_utils.h"
#include "../ctrlm_rfc.h"
#include "../ctrlm_vendor_network_factory.h"
#include "../json_config.h"
#include "../ctrlm_tr181.h"
#include "../ctrlm_rfc.h"
#include "../ctrlm_ipc_device_update.h"
#include <iostream>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <jansson.h>
#include <algorithm>
#include <time.h>
#include <unordered_map>
#include "irMgr.h"   // for IARM_BUS_IRMGR_KEYSRC_

using namespace std;



static gboolean ctrlm_ble_network_continue_upgrade(gpointer user_data);
static bool ctrlm_ble_parse_upgrade_image_info(string filename, ctrlm_ble_upgrade_image_info_t &image_info);


// Network class factory
static int ctrlm_ble_network_factory(unsigned long ignore_mask, json_t *json_config_root, networks_map_t& networks) {

   int num_networks_added = 0;

   #ifdef CTRLM_NETWORK_BLE

   // add network if enabled
   if ( !(ignore_mask & (1 << CTRLM_NETWORK_TYPE_BLUETOOTH_LE)) ) {
      gboolean mask_key_codes = JSON_BOOL_VALUE_CTRLM_GLOBAL_MASK_KEY_CODES;
      ctrlm_network_id_t network_id = network_id_get_next(CTRLM_NETWORK_TYPE_BLUETOOTH_LE);
      LOG_INFO("%s: adding new BLE network object, network_id = %d\n", __FUNCTION__, network_id);
      networks[network_id] = new ctrlm_obj_network_ble_t(CTRLM_NETWORK_TYPE_BLUETOOTH_LE, network_id, "BLE", mask_key_codes, NULL, g_thread_self());
      ++num_networks_added;
   }
   #endif

   return num_networks_added;
}

// Add ctrlm_ble_network_factory to ctrlm_vendor_network_factory_func_chain automatically during init time
static class ctrlm_ble_network_factory_obj_t {
  public:
    ctrlm_ble_network_factory_obj_t(){
      ctrlm_vendor_network_factory_func_add(ctrlm_ble_network_factory);
    }
} _factory_obj;



// Function Implementations

ctrlm_obj_network_ble_t::ctrlm_obj_network_ble_t(ctrlm_network_type_t type, ctrlm_network_id_t id, const char *name, gboolean mask_key_codes, json_t *json_obj_net, GThread *original_thread) :
   ctrlm_obj_network_t(type, id, name, mask_key_codes, original_thread)
{
   LOG_INFO("ctrlm_obj_network_ble_t constructor - Type (%u) Id (%u) Name (%s)\n", type_, id_, name_.c_str());
   version_                     = "unknown";
   init_result_                 = CTRLM_HAL_RESULT_ERROR;
   ready_                       = false;
   state_                       = CTRLM_BLE_STATE_INITIALIZING;
   ir_state_                    = CTRLM_IR_STATE_IDLE;
   upgrade_in_progress_         = false;
   unpair_on_remote_request_    = true;

   hal_thread_                  = NULL;
   hal_api_main_                = NULL;
   hal_api_term_                = NULL;

   // Initialize condition and mutex
   g_mutex_init(&mutex_);
   g_cond_init(&condition_);

   // EGTODO: should BLE have a similar config file?
   // load_config(json_obj_net);

   hal_api_main_ = ctrlm_hal_ble_main;
}

ctrlm_obj_network_ble_t::ctrlm_obj_network_ble_t() {
   LOG_INFO("ctrlm_obj_network_ble_t constructor - default\n");
}

ctrlm_obj_network_ble_t::~ctrlm_obj_network_ble_t() {
   LOG_INFO("ctrlm_obj_network_ble_t destructor - Type (%u) Id (%u) Name (%s)\n", type_, id_, name_.c_str());

   ctrlm_ble_iarm_terminate();
   
   for(auto it = controllers_.begin(); it != controllers_.end(); it++) {
      if(it->second != NULL) {
         delete it->second;
      }
   }

   // Delete the temp directories where upgrade images were extracted
   for (auto const &upgrade_image : upgrade_images_) {
      ctrlm_archive_remove(upgrade_image.second.path);
   }
}

gboolean ctrlm_obj_network_ble_t::load_config(json_t *json_obj_net) {
   if(json_obj_net == NULL || !json_is_object(json_obj_net)) {
      LOG_INFO("%s: Invalid params, use default configuration\n", __FUNCTION__);
   } else {
      LOG_INFO("%s: Not implemented, use default configuration\n", __FUNCTION__);
   }
   return true;
}

// ==================================================================================================================================================================
// BEGIN - Main init functions of the BLE Network and HAL
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------
ctrlm_hal_result_t ctrlm_obj_network_ble_t::hal_init_request(GThread *ctrlm_main_thread) {
   ctrlm_hal_ble_main_init_t main_init;
   ctrlm_main_thread_ = ctrlm_main_thread;
   THREAD_ID_VALIDATE();

   // Initialization parameters
   main_init.api_version               = CTRLM_HAL_BLE_API_VERSION;
   main_init.network_id                = network_id_get();
   main_init.cfm_init                  = ctrlm_hal_ble_cfm_init;
   main_init.ind_status                = ctrlm_ble_IndicateHAL_RcuStatus;
   main_init.ind_paired                = ctrlm_ble_IndicateHAL_Paired;
   main_init.ind_unpaired              = ctrlm_ble_IndicateHAL_UnPaired;
   main_init.ind_voice_data            = ctrlm_ble_IndicateHAL_VoiceData;
   main_init.ind_keypress              = ctrlm_ble_IndicateHAL_Keypress;
   main_init.req_voice_session_begin   = ctrlm_ble_Request_VoiceSessionBegin;
   main_init.req_voice_session_end     = ctrlm_ble_Request_VoiceSessionEnd;

   g_mutex_lock(&mutex_);
   hal_thread_ = g_thread_new(name_get(), (void* (*)(void*))hal_api_main_, &main_init);

   // Block until initialization is complete or a timeout occurs
   LOG_INFO("%s: Waiting for %s initialization...\n", __FUNCTION__, name_get());
   g_cond_wait(&condition_, &mutex_);
   g_mutex_unlock(&mutex_);

   if(CTRLM_HAL_RESULT_SUCCESS == init_result_) {
      if (true == ctrlm_ble_iarm_init()) {
         ready_ = true;
      } else {
         init_result_ = CTRLM_HAL_RESULT_ERROR;
      }
   }

   return(init_result_);
}


ctrlm_hal_result_t ctrlm_hal_ble_cfm_init(ctrlm_network_id_t network_id, ctrlm_hal_ble_cfm_init_params_t params) {
   if(!ctrlm_network_id_is_valid(network_id)) {
      LOG_ERROR("%s: Invalid network id %u\n", __FUNCTION__, network_id);
      g_assert(0);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }
   LOG_INFO("%s: result %s\n", __FUNCTION__, ctrlm_hal_result_str(params.result));
   // Signal completion of the operation
   sem_t semaphore;
   sem_init(&semaphore, 0, 0);

   ctrlm_main_queue_msg_hal_cfm_init_t msg;
   memset(&msg, 0, sizeof(msg));

   msg.params.ble   = params;
   msg.semaphore    = &semaphore;
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_ble_t::hal_init_cfm, &msg, sizeof(msg), NULL, network_id);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);

   return(params.result);
}

void ctrlm_obj_network_ble_t::hal_init_cfm(void *data, int size) {
   ctrlm_main_queue_msg_hal_cfm_init_t *dqm = (ctrlm_main_queue_msg_hal_cfm_init_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_hal_cfm_init_t));

   if(dqm->params.ble.result == CTRLM_HAL_RESULT_SUCCESS) {
      hal_api_set(dqm->params.ble.property_get, dqm->params.ble.property_set, dqm->params.ble.term);
   }

   hal_init_confirm(dqm->params.ble);

   ctrlm_obj_network_t::hal_init_cfm(data, size);
}

void ctrlm_obj_network_ble_t::hal_init_confirm(ctrlm_hal_ble_cfm_init_params_t params) {
   THREAD_ID_VALIDATE();

   init_result_           = params.result;
   version_               = params.version;
   state_                 = CTRLM_BLE_STATE_IDLE;

   hal_api_start_threads_         = params.start_threads;
   hal_api_get_devices_           = params.get_devices;
   hal_api_get_all_rcu_props_     = params.get_all_rcu_props;
   hal_api_discovery_start_       = params.start_discovery;
   hal_api_pair_with_code_        = params.pair_with_code;
   hal_api_unpair_                = params.unpair;
   hal_api_start_audio_stream_    = params.start_audio_stream;
   hal_api_stop_audio_stream_     = params.stop_audio_stream;
   hal_api_get_audio_stats_       = params.get_audio_stats;
   hal_api_set_ir_codes_          = params.set_ir_codes;
   hal_api_clear_ir_codes_        = params.clear_ir_codes;
   hal_api_find_me_               = params.find_me;
   hal_api_get_daemon_log_levels_ = params.get_daemon_log_levels;
   hal_api_set_daemon_log_levels_ = params.set_daemon_log_levels;
   hal_api_fw_upgrade_            = params.fw_upgrade;
   hal_api_get_rcu_unpair_reason_ = params.get_rcu_unpair_reason;
   hal_api_get_rcu_reboot_reason_ = params.get_rcu_reboot_reason;
   hal_api_send_rcu_action_       = params.send_rcu_action;
   hal_api_handle_deepsleep_      = params.handle_deepsleep;

   // Unblock the caller of hal_init
   g_mutex_lock(&mutex_);
   g_cond_broadcast(&condition_);
   g_mutex_unlock(&mutex_);
}

void ctrlm_obj_network_ble_t::hal_init_complete()
{
   LOG_DEBUG("%s: ENTER *****************************************************************\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();

   // Get the controllers from DB
   controllers_load();

   // Make sure an IR controller object is present, primarily to keep track of last key press
   ctrlm_controller_id_t ir_controller_id;
   if (false == getControllerId(0, &ir_controller_id)) {
      // IR controller does not exist, so create it
      LOG_INFO("%s: controller <%s> does not exist in DB, adding now...\n", __PRETTY_FUNCTION__, BROADCAST_PRODUCT_NAME_IR_DEVICE);
      ctrlm_hal_ble_rcu_data_t rcu_data;
      memset(&rcu_data, 0, sizeof(rcu_data));
      rcu_data.ieee_address = 0;
      strcpy(rcu_data.name, BROADCAST_PRODUCT_NAME_IR_DEVICE);
      controller_add(rcu_data);
   }

   // The controller list reported by the HAL is the source of truth
   if (hal_api_get_devices_) {
      vector<unsigned long long> hal_devices;
      if (CTRLM_HAL_RESULT_SUCCESS == hal_api_get_devices_(hal_devices)) {
         for(auto const &it : hal_devices) {
            ctrlm_controller_id_t controller_id;
            if (!getControllerId(it, &controller_id)) {
               LOG_WARN("%s: Controller from HAL <0x%llX> doesn't exist in network, adding...\n", __PRETTY_FUNCTION__, it);
               ctrlm_hal_ble_RcuStatusData_t rcu_props;
               memset(&rcu_props, 0, sizeof(rcu_props));
               rcu_props.rcu_data.ieee_address = it;
               if (hal_api_get_all_rcu_props_) {
                  hal_api_get_all_rcu_props_(rcu_props);
               }
               controller_add(rcu_props.rcu_data);
            }
         }
         for(auto const &it : controllers_) {
            if (BLE_CONTROLLER_TYPE_IR != it.second->getControllerType() && 
               (hal_devices.end() == std::find(hal_devices.begin(), hal_devices.end(), it.second->getMacAddress()))) {
               LOG_INFO("%s: Controller (ID = %u), MAC Address: <0x%llX> not paired according to HAL, so removing...\n", __FUNCTION__, it.first, it.second->getMacAddress());
               //remote stored in network database is not a paired remote as reported by the HAL, so remove it.
               controller_remove(it.first);
            }
         }
      }
   }

   // Print out pairing table
   LOG_INFO("%s: -----------------------\n", __FUNCTION__);
   LOG_INFO("%s: %u Bound Controllers\n",    __FUNCTION__, controllers_.size());
   LOG_INFO("%s: -----------------------\n", __FUNCTION__);
   for(auto it = controllers_.begin(); it != controllers_.end(); it++) {
      LOG_INFO("%s: -----------------------\n", __FUNCTION__);
      LOG_INFO("%s: Controller (ID = %u), MAC Address: <0x%llX>\n", __FUNCTION__, it->first, it->second->getMacAddress());
      it->second->print_status();
      LOG_INFO("%s: -----------------------\n", __FUNCTION__);
   }

   if (hal_api_start_threads_) {
      hal_api_start_threads_();
   }
}
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------
// END - Main init functions of the BLE Network and HAL
// ==================================================================================================================================================================


// ==================================================================================================================================================================
// BEGIN - Process Requests to the network in CTRLM Main thread context
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------
void ctrlm_obj_network_ble_t::req_process_network_status(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_main_network_status_t *dqm = (ctrlm_main_queue_msg_main_network_status_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_main_network_status_t));
   g_assert(dqm->cmd_result);

   ctrlm_network_status_ble_t *network_status  = &dqm->status->status.ble;
   strncpy(network_status->version_hal, version_get(), CTRLM_MAIN_VERSION_LENGTH);
   network_status->version_hal[CTRLM_MAIN_VERSION_LENGTH - 1] = '\0';
   network_status->controller_qty = controllers_.size();
   LOG_INFO("%s: HAL Version <%s> Controller Qty %u\n", __FUNCTION__, network_status->version_hal, network_status->controller_qty);
   int index = 0;
   for(auto const &itr : controllers_) {
      network_status->controllers[index] = itr.first;
      index++;
      if(index >= CTRLM_MAIN_MAX_BOUND_CONTROLLERS) {
         break;
      }
   }
   dqm->status->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
   *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;

   ctrlm_obj_network_t::req_process_network_status(data, size);
}

void ctrlm_obj_network_ble_t::req_process_controller_status(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_controller_status_t *dqm = (ctrlm_main_queue_msg_controller_status_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_controller_status_t));
   g_assert(dqm->cmd_result);

   ctrlm_controller_id_t controller_id = dqm->controller_id;
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u NOT present.\n", __FUNCTION__, controller_id);
      *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
   } else {
      controllers_[controller_id]->getStatus(dqm->status);
      *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_SUCCESS;
   }

   ctrlm_obj_network_t::req_process_controller_status(data, size);
}

void ctrlm_obj_network_ble_t::req_process_voice_session_begin(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_voice_session_t *dqm = (ctrlm_main_queue_msg_voice_session_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;
   
   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      ctrlm_controller_id_t controller_id;
      unsigned long long ieee_address = dqm->params->ieee_address;
      if (!getControllerId(ieee_address, &controller_id)) {
         LOG_ERROR("%s: Controller doesn't exist!\n", __PRETTY_FUNCTION__);
         dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      } else {
         // Currently BLE RCUs only support push-to-talk, so hardcoding here for now
         ctrlm_voice_device_t device = CTRLM_VOICE_DEVICE_PTT;
         ctrlm_voice_session_response_status_t voice_status;
         ctrlm_voice_format_t format = CTRLM_VOICE_FORMAT_ADPCM_SKY;

         voice_status = ctrlm_get_voice_obj()->voice_session_req(network_id_get(), controller_id, device, format, NULL, 
                                                                controllers_[controller_id]->getName().c_str(), 
                                                                controllers_[controller_id]->getSwRevision().toString().c_str(), 
                                                                controllers_[controller_id]->getHwRevision().toString().c_str(), 0.0,
                                                                false, NULL, NULL, NULL, true);
         if (VOICE_SESSION_RESPONSE_AVAILABLE != voice_status) {
            LOG_ERROR("%s: Failed opening voice session in ctrlm_voice_t, error = <%d>\n", __FUNCTION__, voice_status);
         } else {
            bool success = false;
            ctrlm_hal_ble_StartAudioStream_params_t start_params;
            start_params.controller_id = controller_id;
            start_params.ieee_address = ieee_address;
            start_params.encoding = (format == CTRLM_VOICE_FORMAT_PCM) ? CTRLM_HAL_BLE_ENCODING_PCM : CTRLM_HAL_BLE_ENCODING_ADPCM;
            start_params.fd = -1;

            if (NULL == hal_api_start_audio_stream_) {
               LOG_FATAL("%s: hal_api_start_audio_stream_ is NULL!\n", __PRETTY_FUNCTION__);
            } else {
               if ( (success = (CTRLM_HAL_RESULT_SUCCESS == hal_api_start_audio_stream_(&start_params))) ) {
                  if (start_params.fd < 0) {
                     LOG_ERROR("%s: Voice streaming pipe invalid (fd = <%d>), aborting voice session\n", __FUNCTION__, start_params.fd);
                     success = false;
                  } else {
                     LOG_INFO("%s: Acquired voice streaming pipe fd = <%d>, sending to voice engine\n", __FUNCTION__, start_params.fd);
                     //Send the fd acquired from bluez to the voice engine
                     success = ctrlm_get_voice_obj()->voice_session_data(network_id_get(), controller_id, start_params.fd);
                  }
               }
            }
            if (false == success) {
               LOG_ERROR("%s: Failed to start voice streaming, ending voice session...\n", __FUNCTION__);
               ctrlm_get_voice_obj()->voice_session_end(network_id_get(), controller_id, CTRLM_VOICE_SESSION_END_REASON_OTHER_ERROR);

               ctrlm_hal_ble_StopAudioStream_params_t stop_params;
               stop_params.ieee_address = ieee_address;
               if (hal_api_stop_audio_stream_) {
                  hal_api_stop_audio_stream_(stop_params);
               }
            } else {
               dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            }
         }
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_ble_t::req_process_voice_session_end(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_voice_session_t *dqm = (ctrlm_main_queue_msg_voice_session_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      ctrlm_controller_id_t controller_id;
      unsigned long long ieee_address = dqm->params->ieee_address;

      if (!getControllerId(ieee_address, &controller_id)) {
         LOG_ERROR("%s: Controller doesn't exist!\n", __PRETTY_FUNCTION__);
         dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      } else {
         ctrlm_get_voice_obj()->voice_session_end(network_id_get(), controller_id, CTRLM_VOICE_SESSION_END_REASON_DONE);

         ctrlm_hal_ble_StopAudioStream_params_t params;
         params.ieee_address = ieee_address;

         if (hal_api_stop_audio_stream_) {
            if (CTRLM_HAL_RESULT_SUCCESS == hal_api_stop_audio_stream_(params)) {
               dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            }
         } else {
            LOG_FATAL("%s: hal_api_stop_audio_stream_ is NULL!\n", __PRETTY_FUNCTION__);
         }
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}


void ctrlm_obj_network_ble_t::req_process_start_pairing(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_start_pairing_t *dqm = (ctrlm_main_queue_msg_start_pairing_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_start_pairing_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      ctrlm_hal_ble_StartDiscovery_params_t params;
      params.timeout_ms = dqm->params->timeout * 1000;

      if (hal_api_discovery_start_) {
         if (CTRLM_HAL_RESULT_SUCCESS == hal_api_discovery_start_(params)) {
            dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         }
      } else {
         LOG_FATAL("%s: hal_api_discovery_start_ is NULL!\n", __PRETTY_FUNCTION__);
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}


void ctrlm_obj_network_ble_t::req_process_pair_with_code(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_pair_with_code_t *dqm = (ctrlm_main_queue_msg_pair_with_code_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_pair_with_code_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      ctrlm_hal_ble_PairWithCode_params_t params;
      params.pair_code = dqm->params->pair_code;

      if (hal_api_pair_with_code_) {
         if (CTRLM_HAL_RESULT_SUCCESS == hal_api_pair_with_code_(params)) {
            dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         }
      } else {
         LOG_FATAL("%s: hal_api_pair_with_code_ is NULL!\n", __PRETTY_FUNCTION__);
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}


void ctrlm_obj_network_ble_t::req_process_ir_set_code(void *data, int size) {
   LOG_DEBUG("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_ir_set_code_t *dqm = (ctrlm_main_queue_msg_ir_set_code_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_ir_set_code_t));

   if(dqm->success) {*(dqm->success) = false;}

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      ctrlm_controller_id_t controller_id = dqm->controller_id;
      if (!controller_exists(controller_id)) {
         LOG_ERROR("%s: Controller doesn't exist!\n", __PRETTY_FUNCTION__);
      } else {
         if(dqm->ir_codes) {
            ctrlm_hal_ble_IRSetCode_params_t params;
            params.ieee_address = controllers_[controller_id]->getMacAddress();
            params.ir_codes = dqm->ir_codes->get_key_map();
            if (hal_api_set_ir_codes_) {
               if (CTRLM_HAL_RESULT_SUCCESS == hal_api_set_ir_codes_(params)) {
                  if(dqm->success) {*(dqm->success) = true;}
                  controllers_[controller_id]->irdb_entry_id_name_set(dqm->ir_codes->get_type(), dqm->ir_codes->get_id());
                  LOG_INFO("%s: irdb_entry_id_name = <%s>\n", __FUNCTION__, dqm->ir_codes->get_id().c_str());
               }
            } else {
               LOG_FATAL("%s: hal_api_set_ir_codes_ is NULL!\n", __PRETTY_FUNCTION__);
            }
         }
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}
void ctrlm_obj_network_ble_t::req_process_ir_clear_codes(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_ir_clear_t *dqm = (ctrlm_main_queue_msg_ir_clear_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_ir_clear_t));

   if(dqm->success) {*(dqm->success) = false;}

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      ctrlm_controller_id_t controller_id = dqm->controller_id;
      if (!controller_exists(controller_id)) {
         LOG_ERROR("%s: Controller doesn't exist!\n", __PRETTY_FUNCTION__);
      } else {
         ctrlm_hal_ble_IRClear_params_t params;
         params.ieee_address = controllers_[controller_id]->getMacAddress();

         if (hal_api_clear_ir_codes_) {
            if (CTRLM_HAL_RESULT_SUCCESS == hal_api_clear_ir_codes_(params)) {
               if(dqm->success) {*(dqm->success) = true;}
               controllers_[controller_id]->irdb_entry_id_name_set(CTRLM_IRDB_DEV_TYPE_TV, "0");
               controllers_[controller_id]->irdb_entry_id_name_set(CTRLM_IRDB_DEV_TYPE_AVR, "0");
            }
         } else {
            LOG_FATAL("%s: hal_api_clear_ir_codes_ is NULL!\n", __PRETTY_FUNCTION__);
         }
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_ble_t::req_process_find_my_remote(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_find_my_remote_t *dqm = (ctrlm_main_queue_msg_find_my_remote_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_find_my_remote_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      ctrlm_controller_id_t controller_id = dqm->params->controller_id;

      if (!controller_exists(controller_id)) {
         LOG_ERROR("%s: Controller doesn't exist!\n", __PRETTY_FUNCTION__);
         dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      } else {
         ctrlm_hal_ble_FindMe_params_t params;
         params.ieee_address = controllers_[controller_id]->getMacAddress();
         params.level = dqm->params->level;
         params.duration = dqm->params->duration;

         if (hal_api_find_me_) {
            if (CTRLM_HAL_RESULT_SUCCESS == hal_api_find_me_(params)) {
               dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            }
         } else {
            LOG_FATAL("%s: hal_api_find_me_ is NULL!\n", __PRETTY_FUNCTION__);
         }
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_ble_t::req_process_get_ble_log_levels(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_ble_log_levels_t *dqm = (ctrlm_main_queue_msg_ble_log_levels_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_ble_log_levels_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      daemon_logging_t logging;
      if (hal_api_get_daemon_log_levels_) {
         if (CTRLM_HAL_RESULT_SUCCESS == hal_api_get_daemon_log_levels_(&logging)) {
            dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            dqm->params->logging = logging;
         }
      } else {
         LOG_FATAL("%s: hal_api_get_daemon_log_levels_ is NULL!\n", __PRETTY_FUNCTION__);
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_ble_t::req_process_set_ble_log_levels(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_ble_log_levels_t *dqm = (ctrlm_main_queue_msg_ble_log_levels_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_ble_log_levels_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      if (hal_api_set_daemon_log_levels_) {
         if (CTRLM_HAL_RESULT_SUCCESS == hal_api_set_daemon_log_levels_(dqm->params->logging)) {
            dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         }
      } else {
         LOG_FATAL("%s: hal_api_set_daemon_log_levels_ is NULL!\n", __PRETTY_FUNCTION__);
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_ble_t::req_process_get_rcu_unpair_reason(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_get_rcu_unpair_reason_t *dqm = (ctrlm_main_queue_msg_get_rcu_unpair_reason_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_get_rcu_unpair_reason_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      ctrlm_controller_id_t controller_id = dqm->params->controller_id;

      if (!controller_exists(controller_id)) {
         LOG_ERROR("%s: Controller doesn't exist!\n", __PRETTY_FUNCTION__);
         dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      } else {
         ctrlm_hal_ble_GetRcuUnpairReason_params_t params;
         params.ieee_address = controllers_[controller_id]->getMacAddress();
         if (hal_api_get_rcu_unpair_reason_) {
            if (CTRLM_HAL_RESULT_SUCCESS == hal_api_get_rcu_unpair_reason_(&params)) {
               dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
               dqm->params->reason = params.reason;
            }
         } else {
            LOG_FATAL("%s: hal_api_get_rcu_unpair_reason_ is NULL!\n", __PRETTY_FUNCTION__);
         }
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_ble_t::req_process_get_rcu_reboot_reason(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_get_rcu_reboot_reason_t *dqm = (ctrlm_main_queue_msg_get_rcu_reboot_reason_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_get_rcu_reboot_reason_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      ctrlm_controller_id_t controller_id = dqm->params->controller_id;

      if (!controller_exists(controller_id)) {
         LOG_ERROR("%s: Controller doesn't exist!\n", __PRETTY_FUNCTION__);
         dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      } else {
         ctrlm_hal_ble_GetRcuRebootReason_params_t params;
         params.ieee_address = controllers_[controller_id]->getMacAddress();
         if (hal_api_get_rcu_reboot_reason_) {
            if (CTRLM_HAL_RESULT_SUCCESS == hal_api_get_rcu_reboot_reason_(&params)) {
               dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
               dqm->params->reason = params.reason;
            }
         } else {
            LOG_FATAL("%s: hal_api_get_rcu_reboot_reason_ is NULL!\n", __PRETTY_FUNCTION__);
         }
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_ble_t::req_process_send_rcu_action(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_send_rcu_action_t *dqm = (ctrlm_main_queue_msg_send_rcu_action_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_send_rcu_action_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      ctrlm_controller_id_t controller_id = dqm->params->controller_id;

      if (!controller_exists(controller_id)) {
         LOG_ERROR("%s: Controller doesn't exist!\n", __PRETTY_FUNCTION__);
         dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      } else {
         ctrlm_hal_ble_SendRcuAction_params_t params;
         params.ieee_address = controllers_[controller_id]->getMacAddress();
         params.action = dqm->params->action;
         params.wait_for_reply = dqm->params->wait_for_reply;
         if (hal_api_send_rcu_action_) {
            if (CTRLM_HAL_RESULT_SUCCESS == hal_api_send_rcu_action_(params)) {
               dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            }
         } else {
            LOG_FATAL("%s: hal_api_send_rcu_action_ is NULL!\n", __PRETTY_FUNCTION__);
         }
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_ble_t::factory_reset(void) {
   THREAD_ID_VALIDATE();

   LOG_INFO("%s: Sending RCU action unpair to all controllers.\n", __FUNCTION__);

   // Since we are factory resetting anyway, don't waste time unpairing the remote after the
   // remote notifies us of unpair reason through RemoteControl service
   this->unpair_on_remote_request_ = false;

   for (auto const &controller : controllers_) {
      if (BLE_CONTROLLER_TYPE_IR != controller.second->getControllerType()) {
         ctrlm_hal_ble_SendRcuAction_params_t params;
         params.ieee_address = controller.second->getMacAddress();
         params.action = CTRLM_BLE_RCU_ACTION_FACTORY_RESET;
         params.wait_for_reply = true;
         if (hal_api_send_rcu_action_) { 
            hal_api_send_rcu_action_(params); 
         } else {
            LOG_FATAL("%s: hal_api_send_rcu_action_ is NULL!\n", __PRETTY_FUNCTION__);
         }
      }
   }
}

void ctrlm_obj_network_ble_t::req_process_get_rcu_status(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_get_rcu_status_t *dqm = (ctrlm_main_queue_msg_get_rcu_status_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_get_rcu_status_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      populate_rcu_status_message(dqm->params);
      dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_ble_t::req_process_get_last_keypress(void *data, int size) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_get_last_keypress_t *dqm = (ctrlm_main_queue_msg_get_last_keypress_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_get_last_keypress_t));

   dqm->params->result = CTRLM_IARM_CALL_RESULT_ERROR;

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      dqm->params->is_screen_bind_mode    = false;
      dqm->params->remote_keypad_config   = CTRLM_REMOTE_KEYPAD_CONFIG_NA;

      //Find the controller ID of the most recent keypress
      time_t lastKeypressTime = 0;
      ctrlm_controller_id_t lastKeypressControllerID = 0;
      for (auto &controller : controllers_) {
         if (controller.second->getLastKeyTime() >= lastKeypressTime) {
            lastKeypressTime = controller.second->getLastKeyTime();
            lastKeypressControllerID = controller.first;
         }
      }
      if (controller_exists(lastKeypressControllerID)) {
         dqm->params->controller_id          = lastKeypressControllerID;
         dqm->params->source_key_code        = controllers_[lastKeypressControllerID]->getLastKeyCode();
         dqm->params->timestamp              = controllers_[lastKeypressControllerID]->getLastKeyTime() * 1000LL; //Not sure why its converted to long long, but I'm staying consistent with rf4ce
         
         strncpy(dqm->params->source_name, controllers_[lastKeypressControllerID]->getName().c_str(), sizeof(dqm->params->source_name));
         dqm->params->source_name[CTRLM_MAIN_SOURCE_NAME_MAX_LENGTH - 1] = '\0';

         if (BLE_CONTROLLER_TYPE_IR == controllers_[lastKeypressControllerID]->getControllerType()) {
            dqm->params->controller_id = -1;
            dqm->params->source_type = IARM_BUS_IRMGR_KEYSRC_IR;
         } else {
            dqm->params->source_type = IARM_BUS_IRMGR_KEYSRC_RF;
         }
         dqm->params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
      } else {
         LOG_ERROR("%s: No controller keypresses found, returning error...\n", __PRETTY_FUNCTION__);
      }
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool ctrlm_ble_parse_upgrade_image_info(string filename, ctrlm_ble_upgrade_image_info_t &image_info) {
   gchar *contents = NULL;
   string xml;

   LOG_INFO("%s: parsing upgrade file <%s>\n", __FUNCTION__, filename.c_str());

   if(!g_file_test(filename.c_str(), G_FILE_TEST_EXISTS) || !g_file_get_contents(filename.c_str(), &contents, NULL, NULL)) {
      LOG_ERROR("%s: unable to get file contents <%s>\n", __FUNCTION__, filename.c_str());
      return false;
   }
   xml = contents;
   g_free(contents);

   /////////////////////////////////////////////////////////////
   // Required parameters in firmware image metadata file
   /////////////////////////////////////////////////////////////
   string product_name = ctrlm_xml_tag_text_get(xml, "image:productName");
   if(product_name.length() == 0) {
      LOG_ERROR("%s: Missing Product Name\n", __FUNCTION__);
      return false;
   }

   ctrlm_ble_controller_type_t controller_type;
   if (product_name == XCONF_PRODUCT_NAME_PR1) {
      controller_type = BLE_CONTROLLER_TYPE_PR1;
   } else if (product_name == XCONF_PRODUCT_NAME_LC103) {
      controller_type = BLE_CONTROLLER_TYPE_LC103;
   } else if (product_name == XCONF_PRODUCT_NAME_EC302) {
      controller_type = BLE_CONTROLLER_TYPE_EC302;
   }  else {
      LOG_ERROR("%s: Unsupported product <%s>\n", __FUNCTION__, product_name.c_str());
      return false;
   }
   image_info.controller_type = controller_type;

   string version_string = ctrlm_xml_tag_text_get(xml, "image:softwareVersion");
   if(version_string.length() == 0) {
      LOG_ERROR("%s: Missing Software Version\n", __FUNCTION__);
      return false;
   }
   image_info.version_software.loadFromString(version_string);

   image_info.image_filename  = ctrlm_xml_tag_text_get(xml, "image:fileName");
   if(image_info.image_filename.length() == 0) {
      LOG_ERROR("%s: Missing File Name\n", __FUNCTION__);
      return false;
   }

   /////////////////////////////////////////////////////////////
   // Optional parameters in firmware image metadata file
   /////////////////////////////////////////////////////////////
   version_string = ctrlm_xml_tag_text_get(xml, "image:bootLoaderVersionMin");
   image_info.version_bootloader_min.loadFromString(version_string);

   version_string = ctrlm_xml_tag_text_get(xml, "image:hardwareVersionMin");
   image_info.version_hardware_min.loadFromString(version_string);

   string size  = ctrlm_xml_tag_text_get(xml, "image:size");
   if(size.length() != 0) {
      image_info.size = atol(size.c_str());
   }

   string crc  = ctrlm_xml_tag_text_get(xml, "image:CRC");
   if(crc.length() != 0) {
      image_info.crc = strtoul(crc.c_str(), NULL, 16);
   }

   string force_update = ctrlm_xml_tag_text_get(xml, "image:force_update");
   if(force_update.length() == 0) {
      LOG_INFO("%s: Missing force update flag\n", __FUNCTION__);
      image_info.force_update = false;
   } else if(force_update == "1"){
      image_info.force_update = true;
   } else {
      LOG_ERROR("%s: Invalid force update flag <%s>\n", __FUNCTION__, force_update.c_str());
      image_info.force_update = false;
   }

   return true;
}

void ctrlm_obj_network_ble_t::addUpgradeImage(ctrlm_ble_upgrade_image_info_t image_info) {
   if (upgrade_images_.end() != upgrade_images_.find(image_info.controller_type)) {
      //compare version, and replace if newer
      if (upgrade_images_[image_info.controller_type].version_software.compare(image_info.version_software) < 0 ||
          image_info.force_update) {
         upgrade_images_[image_info.controller_type] = image_info;
      }
   } else {
      upgrade_images_[image_info.controller_type] = image_info;
   }
}

static gboolean ctrlm_ble_network_continue_upgrade(gpointer user_data) {
   ctrlm_network_id_t* net_id =  (ctrlm_network_id_t*) user_data;
   if (net_id != NULL) {
      // LOG_DEBUG("%s: Enter...\n", __FUNCTION__);
      ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_network_continue_upgrade, NULL, 0, NULL, *net_id);
   }
   return false;
}

void ctrlm_obj_network_ble_t::req_process_network_continue_upgrade(void *data, int size) {
   if (upgrade_images_.empty()) {
      LOG_WARN("%s: No upgrade images in the queue.\n", __PRETTY_FUNCTION__);
      return;
   }
   bool additional_upgrades_required = false;
   if (!upgrade_in_progress_) {
      for (auto const &upgrade_image : upgrade_images_) {
         for (auto &controller : controllers_) {
            if (controller.second->getControllerType() == upgrade_image.first) {
               if (controller.second->swUpgradeRequired(upgrade_image.second.version_software) ||
                   upgrade_image.second.force_update) {
                  additional_upgrades_required = true;
                  ctrlm_hal_ble_FwUpgrade_params_t params;
                  params.file_path = upgrade_image.second.path + "/" + upgrade_image.second.image_filename;
                  params.ieee_address = controller.second->getMacAddress();
                  controller.second->setUpgradeAttempted(true);
                  if (hal_api_fw_upgrade_) {
                     hal_api_fw_upgrade_(params);
                  } else {
                     LOG_FATAL("%s: hal_api_fw_upgrade_ is NULL!\n", __PRETTY_FUNCTION__);
                  }
               } else {
                  LOG_DEBUG("%s: Software for controller <0x%llX> already up to date\n", __PRETTY_FUNCTION__, controller.second->getMacAddress());
               }
            }
         }
      }
   } else {
      additional_upgrades_required = true;
   }
   if (additional_upgrades_required) {
      LOG_INFO("%s: Setting timer to check for more upgrades in 5 minutes.\n", __PRETTY_FUNCTION__);
      upgrade_timer_tag_ = ctrlm_timeout_create(5 * (1000 * 60), ctrlm_ble_network_continue_upgrade, &id_);
   } else {
      // We looped through all the upgrade images and all the controllers and didn't trigger any upgrades, so stop upgrade process.
      LOG_INFO("%s: Software for all controllers already up to date, exiting upgrade procedure.\n", __PRETTY_FUNCTION__);
   }
}

void ctrlm_obj_network_ble_t::req_process_network_managed_upgrade(void *data, int size) {
   ctrlm_main_queue_msg_network_fw_upgrade_t *dqm = (ctrlm_main_queue_msg_network_fw_upgrade_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_network_fw_upgrade_t));
   g_assert(dqm->network_managing_upgrade != NULL);

   LOG_INFO("%s: %s network will manage the RCU firmware upgrade\n", __FUNCTION__, name_get());
   *dqm->network_managing_upgrade = true;

   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }

   if (!ready_) {
      LOG_FATAL("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
   } else {
      string archive_file_path = dqm->archive_file_path;
      size_t idx = archive_file_path.rfind('/');
      string archive_file_name = archive_file_path.substr(idx + 1);

      string dest_path = string(dqm->temp_dir_path) + "ctrlm/" + archive_file_name;

      if (ctrlm_file_exists(dest_path.c_str())) {
         LOG_INFO("%s: dest_path <%s> already exists, removing it...\n", __FUNCTION__, dest_path.c_str());
         ctrlm_archive_remove(dest_path);
      }
      if (ctrlm_archive_extract(archive_file_path, dqm->temp_dir_path, archive_file_name)) {
         //extract metadata from image.xml
         ctrlm_ble_upgrade_image_info_t image_info;
         image_info.path = dest_path;
         string image_xml_filename = dest_path + "/image.xml";
         if (ctrlm_ble_parse_upgrade_image_info(image_xml_filename, image_info)) {
            addUpgradeImage(image_info);
         }
      }

      for (auto &controller : controllers_) {
         controller.second->setUpgradeAttempted(false);
      }

      ctrlm_ble_network_continue_upgrade((gpointer) &id_);
   }
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------
// END - Process Requests to the network in CTRLM Main thread context
// ==================================================================================================================================================================


// ==================================================================================================================================================================
// BEGIN - Process Indications from HAL to the network in CTRLM Main thread context
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------

void ctrlm_obj_network_ble_t::ind_process_rcu_status(void *data, int size) {
   // LOG_DEBUG("%s, Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();
   bool report_status = true;
   bool print_status = true;

   ctrlm_hal_ble_RcuStatusData_t *dqm = (ctrlm_hal_ble_RcuStatusData_t *)data;
   g_assert(dqm);
   if (sizeof(ctrlm_hal_ble_RcuStatusData_t) != size) {
      LOG_ERROR("%s: Invalid size!\n", __PRETTY_FUNCTION__);
      return;
   }
   if (!ready_) {
      LOG_INFO("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
      return;
   }

   switch (dqm->property_updated) {
      // These properties are associated with the network, not a specific RCU
      case CTRLM_HAL_BLE_PROPERTY_IS_PAIRING:
         // don't send up status event for this since CTRLM_HAL_BLE_PROPERTY_STATE handles all pairing states.
         report_status = false;
         print_status = false;
         break;
      case CTRLM_HAL_BLE_PROPERTY_PAIRING_CODE:
         pairing_code_ = dqm->pairing_code;
         report_status = false;  // don't send up status event for this.
         break;
      case CTRLM_HAL_BLE_PROPERTY_STATE:
         state_ = dqm->state;
         break;
      case CTRLM_HAL_BLE_PROPERTY_IR_STATE:
         ir_state_ = dqm->ir_state;
         break;
      default:
      {
         // These properties are associated with a specific RCU, so make sure the controller object exists before continuing
         ctrlm_controller_id_t controller_id;
         if (false == getControllerId(dqm->rcu_data.ieee_address, &controller_id)) {
            LOG_ERROR("%s: Controller (0x%llX) NOT found in the network!!\n", __PRETTY_FUNCTION__, dqm->rcu_data.ieee_address);
            report_status = false;
            print_status = false;
         } else {
            LOG_DEBUG("%s: Controller (0x%llX) FOUND in the network, updating data...\n", __PRETTY_FUNCTION__, dqm->rcu_data.ieee_address);
            auto controller = controllers_[controller_id];

            switch (dqm->property_updated) {
               case CTRLM_HAL_BLE_PROPERTY_IEEE_ADDDRESS:
                  controller->setMacAddress(dqm->rcu_data.ieee_address);
                  break;
               case CTRLM_HAL_BLE_PROPERTY_DEVICE_ID:
                  controller->setDeviceID(dqm->rcu_data.device_id);
                  break;
               case CTRLM_HAL_BLE_PROPERTY_NAME:
                  controller->setName(string(dqm->rcu_data.name));
                  break;
               case CTRLM_HAL_BLE_PROPERTY_MANUFACTURER:
                  controller->setManufacturer(string(dqm->rcu_data.manufacturer));
                  break;
               case CTRLM_HAL_BLE_PROPERTY_MODEL:
                  controller->setModel(string(dqm->rcu_data.model));
                  break;
               case CTRLM_HAL_BLE_PROPERTY_SERIAL_NUMBER:
                  controller->setSerialNumber(string(dqm->rcu_data.serial_number));
                  break;
               case CTRLM_HAL_BLE_PROPERTY_HW_REVISION:
                  controller->setHwRevision(string(dqm->rcu_data.hw_revision));
                  break;
               case CTRLM_HAL_BLE_PROPERTY_FW_REVISION:
                  controller->setFwRevision(string(dqm->rcu_data.fw_revision));
                  break;
               case CTRLM_HAL_BLE_PROPERTY_SW_REVISION: {
                  controller->setSwRevision(string(dqm->rcu_data.sw_revision));
                  // SW Rev updated, make sure xconf config file is updated
                  ctrlm_main_queue_msg_header_t *msg = (ctrlm_main_queue_msg_header_t *)g_malloc(sizeof(ctrlm_main_queue_msg_header_t));
                  if(msg == NULL) {
                     LOG_ERROR("%s: Out of memory\n", __FUNCTION__);
                  } else {
                     msg->type = CTRLM_MAIN_QUEUE_MSG_TYPE_EXPORT_CONTROLLER_LIST;
                     ctrlm_main_queue_msg_push((gpointer)msg);
                  }
                  break;
               }
               case CTRLM_HAL_BLE_PROPERTY_IR_CODE:
                  controller->setIrCode(dqm->rcu_data.ir_code);
                  break;
               case CTRLM_HAL_BLE_PROPERTY_TOUCH_MODE:
                  controller->setTouchMode(dqm->rcu_data.touch_mode);
                  report_status = false;
                  break;
               case CTRLM_HAL_BLE_PROPERTY_TOUCH_MODE_SETTABLE:
                  controller->setTouchModeSettable(dqm->rcu_data.touch_mode_settable);
                  report_status = false;
                  break;
               case CTRLM_HAL_BLE_PROPERTY_BATTERY_LEVEL:
                  controller->setBatteryPercent(dqm->rcu_data.battery_level);
                  print_status = false;
                  break;
               case CTRLM_HAL_BLE_PROPERTY_CONNECTED:
                  controller->setConnected(dqm->rcu_data.connected);
                  break;
               case CTRLM_HAL_BLE_PROPERTY_AUDIO_STREAMING:
                  controller->setAudioStreaming(dqm->rcu_data.audio_streaming);
                  report_status = false;
                  print_status = false;
                  break;
               case CTRLM_HAL_BLE_PROPERTY_AUDIO_GAIN_LEVEL:
                  controller->setAudioGainLevel(dqm->rcu_data.audio_gain_level);
                  break;
               case CTRLM_HAL_BLE_PROPERTY_IS_UPGRADING:
                  LOG_INFO("%s: Controller (0x%llX) firmware upgrading = %s\n", __FUNCTION__, dqm->rcu_data.ieee_address, dqm->rcu_data.is_upgrading ? "TRUE" : "FALSE");
                  upgrade_in_progress_ = dqm->rcu_data.is_upgrading;
                  report_status = false;
                  print_status = false;
                  break;
               case CTRLM_HAL_BLE_PROPERTY_UPGRADE_PROGRESS:
                  LOG_INFO("%s: Controller (0x%llX) firmware upgrade %d%% complete...\n", __FUNCTION__, dqm->rcu_data.ieee_address, dqm->rcu_data.upgrade_progress);
                  report_status = false;
                  print_status = false;
                  break;
               case CTRLM_HAL_BLE_PROPERTY_UNPAIR_REASON:
                  LOG_INFO("%s: Controller (0x%llX) notified reason for unpairing = <%s>\n", __FUNCTION__, dqm->rcu_data.ieee_address, ctrlm_ble_unpair_reason_str(dqm->rcu_data.unpair_reason));
                  report_status = false;
                  print_status = false;
                  if (this->unpair_on_remote_request_ && 
                     (dqm->rcu_data.unpair_reason == CTRLM_BLE_RCU_UNPAIR_REASON_SFM ||
                      dqm->rcu_data.unpair_reason == CTRLM_BLE_RCU_UNPAIR_REASON_FACTORY_RESET ||
                      dqm->rcu_data.unpair_reason == CTRLM_BLE_RCU_UNPAIR_REASON_RCU_ACTION) ) {
                     if (hal_api_unpair_) {
                        // Unpair it on the client side as well
                        LOG_INFO("%s: Unpairing remote on client side to keep in sync...\n", __FUNCTION__);
                        hal_api_unpair_(dqm->rcu_data.ieee_address);
                     }
                  }
                  break;
               case CTRLM_HAL_BLE_PROPERTY_REBOOT_REASON:
                  LOG_INFO("%s: Controller (0x%llX) notified reason for rebooting = <%s>\n", __FUNCTION__, dqm->rcu_data.ieee_address, ctrlm_ble_reboot_reason_str(dqm->rcu_data.reboot_reason));
                  report_status = false;
                  print_status = false;
                  break;
               default:
                  LOG_WARN("%s: Unhandled Property: %d !!!!!!!!!!!!!!!!!!!!!!!!\n", __PRETTY_FUNCTION__, dqm->property_updated);
                  report_status = false;
                  print_status = false;
                  break;
            }
            if (true == report_status) {
               controller->db_store();
            }
         }
         break;
      }
   }

   if (true == print_status) {
      printStatus();
   }
   if (true == report_status) {
      iarm_event_rcu_status();
   }
}


void ctrlm_obj_network_ble_t::populate_rcu_status_message(ctrlm_iarm_RcuStatus_params_t *msg) {
   LOG_DEBUG("%s, Enter...\n", __PRETTY_FUNCTION__);

   msg->api_revision  = CTRLM_BLE_IARM_BUS_API_REVISION;
   msg->status = state_;
   msg->ir_state = ir_state_;

   int i = 0;
   for(auto it = controllers_.begin(); it != controllers_.end(); it++) {
      if (BLE_CONTROLLER_TYPE_IR != it->second->getControllerType()) {
         msg->remotes[i].controller_id = it->second->controller_id_get();
         msg->remotes[i].deviceid      = it->second->getDeviceID();
         msg->remotes[i].batterylevel  = it->second->getBatteryPercent();
         msg->remotes[i].connected     = (it->second->getConnected()) ? 1 : 0;

         strncpy(msg->remotes[i].ieee_address_str, ctrlm_convert_mac_long_to_string(it->second->getMacAddress()).c_str(), CTRLM_MAX_PARAM_STR_LEN);
         strncpy(msg->remotes[i].btlswver, it->second->getFwRevision().c_str(), CTRLM_MAX_PARAM_STR_LEN);
         strncpy(msg->remotes[i].hwrev, it->second->getHwRevision().toString().c_str(), CTRLM_MAX_PARAM_STR_LEN);
         strncpy(msg->remotes[i].rcuswver, it->second->getSwRevision().toString().c_str(), CTRLM_MAX_PARAM_STR_LEN);

         strncpy(msg->remotes[i].make, it->second->getManufacturer().c_str(), CTRLM_MAX_PARAM_STR_LEN);
         strncpy(msg->remotes[i].model, it->second->getModel().c_str(), CTRLM_MAX_PARAM_STR_LEN);
         strncpy(msg->remotes[i].name, it->second->getName().c_str(), CTRLM_MAX_PARAM_STR_LEN);
         strncpy(msg->remotes[i].serialno, it->second->getSerialNumber().c_str(), CTRLM_MAX_PARAM_STR_LEN);

         strncpy(msg->remotes[i].tv_code, it->second->get_irdb_entry_id_name_tv().c_str(), CTRLM_MAX_PARAM_STR_LEN);
         strncpy(msg->remotes[i].avr_code, it->second->get_irdb_entry_id_name_avr().c_str(), CTRLM_MAX_PARAM_STR_LEN);

         i++;
      }
   }
   msg->num_remotes = i;
}

void ctrlm_obj_network_ble_t::iarm_event_rcu_status() {
   LOG_DEBUG("%s, Enter...\n", __PRETTY_FUNCTION__);

   ctrlm_iarm_RcuStatus_params_t msg;
   populate_rcu_status_message(&msg);
   msg.api_revision = CTRLM_BLE_IARM_BUS_API_REVISION;

   LOG_INFO("%s: broadcasting IARM message BLE RCU Status....\n", __FUNCTION__);
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_RCU_STATUS, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

ctrlm_controller_id_t ctrlm_obj_network_ble_t::controller_add(ctrlm_hal_ble_rcu_data_t &rcu_data) {
   ctrlm_controller_id_t id;
   if (false == getControllerId(rcu_data.ieee_address, &id)) {
      LOG_INFO("%s: Controller (0x%llX) doesn't exist in the network, adding...\n", __PRETTY_FUNCTION__, rcu_data.ieee_address);
      
      id = controller_id_assign();
      controllers_[id] = new ctrlm_obj_controller_ble_t(id, *this, CTRLM_BLE_RESULT_VALIDATION_PENDING);
      controllers_[id]->db_create();

      // It already paired successfully if it indicated up to the network.  No further validation needed so we can validate now
      controllers_[id]->validation_result_set(CTRLM_BLE_RESULT_VALIDATION_SUCCESS);
   } else {
      LOG_INFO("%s: Controller (0x%llX) already exists in the network, updating data...\n", __PRETTY_FUNCTION__, rcu_data.ieee_address);
   }
   if (controller_exists(id)) {
      auto controller = controllers_[id];
      controller->setMacAddress(rcu_data.ieee_address);
      controller->setName(string(rcu_data.name));
      controller->setSerialNumber(string(rcu_data.serial_number));
      controller->setManufacturer(string(rcu_data.manufacturer));
      controller->setModel(string(rcu_data.model));
      controller->setFwRevision(string(rcu_data.fw_revision));
      controller->setHwRevision(string(rcu_data.hw_revision));
      controller->setSwRevision(string(rcu_data.sw_revision));
      controller->setAudioGainLevel(rcu_data.audio_gain_level);
      controller->setBatteryPercent(rcu_data.battery_level);
      controller->setConnected(rcu_data.connected);
      controller->db_store();
   }
   return id;
}

void ctrlm_obj_network_ble_t::ind_process_paired(void *data, int size) {
   LOG_INFO("%s, Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();

   ctrlm_hal_ble_IndPaired_params_t *dqm = (ctrlm_hal_ble_IndPaired_params_t *)data;
   g_assert(dqm);
   if (sizeof(ctrlm_hal_ble_IndPaired_params_t) != size) {
      LOG_ERROR("%s: Invalid size!\n", __PRETTY_FUNCTION__);
      return;
   }
   if (!ready_) {
      LOG_INFO("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
      return;
   }

   ctrlm_controller_id_t id = controller_add(dqm->rcu_data);
   if (controller_exists(id)) {
      controllers_[id]->print_status();
   }

   // Export new controller to xconf config file
   ctrlm_main_queue_msg_header_t *msg = (ctrlm_main_queue_msg_header_t *)g_malloc(sizeof(ctrlm_main_queue_msg_header_t));
   if(msg == NULL) {
      LOG_ERROR("%s: Out of memory\n", __FUNCTION__);
   } else {
      msg->type = CTRLM_MAIN_QUEUE_MSG_TYPE_EXPORT_CONTROLLER_LIST;
      ctrlm_main_queue_msg_push((gpointer)msg);
   }
}

void ctrlm_obj_network_ble_t::ind_process_unpaired(void *data, int size) {
   LOG_INFO("%s, Enter...\n", __PRETTY_FUNCTION__);
   THREAD_ID_VALIDATE();

   ctrlm_hal_ble_IndUnPaired_params_t *dqm = (ctrlm_hal_ble_IndUnPaired_params_t *)data;
   g_assert(dqm);
   if (sizeof(ctrlm_hal_ble_IndUnPaired_params_t) != size) {
      LOG_ERROR("%s: Invalid size!\n", __PRETTY_FUNCTION__);
      return;
   }
   if (!ready_) {
      LOG_INFO("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
      return;
   }

   LOG_INFO("%s, Removing controller having ieee_address = 0x%llX\n", __PRETTY_FUNCTION__, dqm->ieee_address);

   ctrlm_controller_id_t id;
   if (true == getControllerId(dqm->ieee_address, &id)) {
      controller_remove(id);
      // report updated controller status to the plugin
      printStatus();
      iarm_event_rcu_status();

      // Update xconf config file
      ctrlm_main_queue_msg_header_t *msg = (ctrlm_main_queue_msg_header_t *)g_malloc(sizeof(ctrlm_main_queue_msg_header_t));
      if(msg == NULL) {
         LOG_ERROR("%s: Out of memory\n", __FUNCTION__);
      } else {
         msg->type = CTRLM_MAIN_QUEUE_MSG_TYPE_EXPORT_CONTROLLER_LIST;
         ctrlm_main_queue_msg_push((gpointer)msg);
      }
   } else {
      LOG_WARN("%s: Controller (0x%llX) doesn't exist in the network, doing nothing...\n", __PRETTY_FUNCTION__, dqm->ieee_address);
   }
}

void ctrlm_obj_network_ble_t::ind_process_keypress(void *data, int size) {
   THREAD_ID_VALIDATE();

   ctrlm_hal_ble_IndKeypress_params_t *dqm = (ctrlm_hal_ble_IndKeypress_params_t *)data;
   g_assert(dqm);
   if (sizeof(ctrlm_hal_ble_IndKeypress_params_t) != size) {
      LOG_ERROR("%s: Invalid size!\n", __PRETTY_FUNCTION__);
      return;
   }
   if (!ready_) {
      LOG_INFO("%s: Network is not ready!\n", __PRETTY_FUNCTION__);
      return;
   }

   ctrlm_key_status_t key_status = CTRLM_KEY_STATUS_INVALID;
   switch (dqm->event.value) {
      case 0: { key_status = CTRLM_KEY_STATUS_UP; break; }
      case 1: { key_status = CTRLM_KEY_STATUS_DOWN; break; }
      case 2: { key_status = CTRLM_KEY_STATUS_REPEAT; break; }
      default: break;
   }

   ctrlm_controller_id_t controller_id;
   if (true == getControllerId(dqm->ieee_address, &controller_id)) {
      LOG_INFO("%s: %s - MAC Address <%s>, code = <0x%X>, status = <%s>\n", __FUNCTION__, ctrlm_ble_controller_type_str(controllers_[controller_id]->getControllerType()), 
         ctrlm_convert_mac_long_to_string(dqm->ieee_address).c_str(), mask_key_codes_get() ? 0xFF : dqm->event.code, ctrlm_key_status_str(key_status));

      controllers_[controller_id]->process_event_key(key_status, dqm->event.code);
   } else {
      LOG_WARN("%s: Controller (0x%llX) doesn't exist in the network, doing nothing...\n", __PRETTY_FUNCTION__, dqm->ieee_address);
   }
}
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------
// END - Process Indications from HAL to the network in CTRLM Main thread context
// ==================================================================================================================================================================

// This is called from voice obj after a voice session in CTRLM Main thread context
void ctrlm_obj_network_ble_t::process_voice_controller_metrics(void *data, int size) {
   ctrlm_main_queue_msg_controller_voice_metrics_t *dqm = (ctrlm_main_queue_msg_controller_voice_metrics_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_controller_voice_metrics_t));

   THREAD_ID_VALIDATE();
   ctrlm_controller_id_t controller_id = dqm->controller_id;

   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Controller object doesn't exist for controller id %u!\n", __FUNCTION__, controller_id);
      return;
   }

   uint32_t packets_total = dqm->packets_total;
   uint32_t packets_lost = dqm->packets_lost;

   if (hal_api_get_audio_stats_) {
      ctrlm_hal_ble_GetAudioStats_params_t stats_params;
      stats_params.ieee_address = controllers_[controller_id]->getMacAddress();
      if (CTRLM_HAL_RESULT_SUCCESS == hal_api_get_audio_stats_(&stats_params)) {
         LOG_INFO("%s: Audio Stats -> error_status = <%u>, packets_received = <%u>, packets_expected = <%u>\n", 
               __PRETTY_FUNCTION__, stats_params.error_status, stats_params.packets_received, stats_params.packets_expected);
         packets_total = stats_params.packets_expected;
         packets_lost = stats_params.packets_expected - stats_params.packets_received;
      }
   }
   controllers_[controller_id]->update_voice_metrics(dqm->short_utterance ? true : false, packets_total, packets_lost);
}


void ctrlm_obj_network_ble_t::ind_process_voice_session_end(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_voice_session_end_t *dqm = (ctrlm_main_queue_msg_voice_session_end_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_end_t));
   
   ctrlm_controller_id_t controller_id = dqm->controller_id;
   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Controller object doesn't exist for controller id %u!\n", __FUNCTION__, controller_id);
      return;
   }
}

// ==================================================================================================================================================================
// BEGIN - Functions registered and called within the HAL to send indications to the network
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------
ctrlm_hal_result_t ctrlm_ble_IndicateHAL_RcuStatus(ctrlm_network_id_t id, ctrlm_hal_ble_RcuStatusData_t *params) {
   LOG_DEBUG("%s: Enter... Network Id = %u\n", __FUNCTION__, id);
   if (!ctrlm_network_id_is_valid(id)) {
      LOG_ERROR("%s: Network ID is not valid.\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }

   ctrlm_hal_ble_RcuStatusData_t dqm;
   memset(&dqm, 0, sizeof(dqm));
   dqm = *params;
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_ble_t::ind_process_rcu_status, &dqm, sizeof(dqm), NULL, id);
   return(CTRLM_HAL_RESULT_SUCCESS);
}

ctrlm_hal_result_t ctrlm_ble_IndicateHAL_Paired(ctrlm_network_id_t id, ctrlm_hal_ble_IndPaired_params_t *params) {
   LOG_INFO("%s: Enter... Network Id = %u\n", __FUNCTION__, id);
   if (!ctrlm_network_id_is_valid(id)) {
      LOG_ERROR("%s: Network ID is not valid.\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }

   ctrlm_hal_ble_IndPaired_params_t dqm;
   memset(&dqm, 0, sizeof(dqm));
   dqm = *params;
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_ble_t::ind_process_paired, &dqm, sizeof(dqm), NULL, id);
   return(CTRLM_HAL_RESULT_SUCCESS);
}
ctrlm_hal_result_t ctrlm_ble_IndicateHAL_UnPaired(ctrlm_network_id_t id, ctrlm_hal_ble_IndUnPaired_params_t *params) {
   LOG_INFO("%s: Enter... Network Id = %u\n", __FUNCTION__, id);
   if (!ctrlm_network_id_is_valid(id)) {
      LOG_ERROR("%s: Network ID is not valid.\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }

   ctrlm_hal_ble_IndUnPaired_params_t dqm;
   memset(&dqm, 0, sizeof(dqm));
   dqm = *params;
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_ble_t::ind_process_unpaired, &dqm, sizeof(dqm), NULL, id);
   return(CTRLM_HAL_RESULT_SUCCESS);
}

ctrlm_hal_result_t ctrlm_ble_Request_VoiceSessionBegin(ctrlm_network_id_t id, ctrlm_hal_ble_ReqVoiceSession_params_t *params) {
   LOG_INFO("%s: Enter... Network Id = %u\n", __FUNCTION__, id);
   if (!ctrlm_network_id_is_valid(id)) {
      LOG_ERROR("%s: Network ID is not valid.\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }

   // Signal completion of the operation
   sem_t semaphore;
   sem_init(&semaphore, 0, 0);

   ctrlm_voice_iarm_call_voice_session_t v_params;
   v_params.ieee_address = params->ieee_address;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_session_t msg;
   memset(&msg, 0, sizeof(msg));
   msg.params            = &v_params;
   msg.params->result    = CTRLM_IARM_CALL_RESULT_ERROR;
   msg.semaphore         = &semaphore;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_voice_session_begin, &msg, sizeof(msg), NULL, id);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);
   return(CTRLM_HAL_RESULT_SUCCESS);
}

ctrlm_hal_result_t ctrlm_ble_Request_VoiceSessionEnd(ctrlm_network_id_t id, ctrlm_hal_ble_ReqVoiceSession_params_t *params) {
   LOG_INFO("%s: Enter... Network Id = %u\n", __FUNCTION__, id);
   if (!ctrlm_network_id_is_valid(id)) {
      LOG_ERROR("%s: Network ID is not valid.\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }

   // Signal completion of the operation
   sem_t semaphore;
   sem_init(&semaphore, 0, 0);

   ctrlm_voice_iarm_call_voice_session_t v_params;
   v_params.ieee_address = params->ieee_address;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_session_t msg;
   memset(&msg, 0, sizeof(msg));
   msg.params            = &v_params;
   msg.params->result    = CTRLM_IARM_CALL_RESULT_ERROR;
   msg.semaphore         = &semaphore;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_voice_session_end, &msg, sizeof(msg), NULL, id);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);
   return(CTRLM_HAL_RESULT_SUCCESS);
}

ctrlm_hal_result_t ctrlm_ble_IndicateHAL_VoiceData(ctrlm_network_id_t network_id, ctrlm_hal_ble_VoiceData_t *params) {
   // LOG_INFO("%s: Enter... Network Id = %u, params->ieee_address = <0x%llX>, params->controller_id = <%d>\n", __FUNCTION__, network_id, params->ieee_address, params->controller_id);
   if (!ctrlm_network_id_is_valid(network_id)) {
      LOG_ERROR("%s: Network ID is not valid.\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }
   LOG_DEBUG("%s: params->length = <%d>, params->data = 0x%02X%02X%02X%02X... \n", __FUNCTION__, params->length,
         params->data[0], params->data[1], params->data[2], params->data[3]);

   ctrlm_get_voice_obj()->voice_session_data(network_id, params->controller_id, (const char*)params->data, params->length);

   return(CTRLM_HAL_RESULT_SUCCESS);
}

ctrlm_hal_result_t ctrlm_ble_IndicateHAL_Keypress(ctrlm_network_id_t network_id, ctrlm_hal_ble_IndKeypress_params_t *params) {
   // LOG_INFO("%s: Enter... Network Id = %u\n", __FUNCTION__, id);
   if (!ctrlm_network_id_is_valid(network_id)) {
      LOG_ERROR("%s: Network ID is not valid.\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }

   ctrlm_hal_ble_IndKeypress_params_t dqm;
   memset(&dqm, 0, sizeof(dqm));
   dqm = *params;
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_ble_t::ind_process_keypress, &dqm, sizeof(dqm), NULL, network_id);
   return(CTRLM_HAL_RESULT_SUCCESS);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------
// END - Functions registered and called within the HAL to send indications to the network
// ==================================================================================================================================================================


// ==================================================================================================================================================================
// BEGIN - Class Functions
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool ctrlm_obj_network_ble_t::getControllerId(unsigned long long ieee_address, ctrlm_controller_id_t *controller_id) {
   bool found = false;
   for(auto it = controllers_.begin(); it != controllers_.end(); it++) {
      if(it->second->getMacAddress() == ieee_address) { 
         LOG_DEBUG("%s: ieee_address (0x%llX) found, controller ID = <%u>\n", __FUNCTION__, ieee_address, it->first);
         found = true;
         *controller_id = it->first;
      }
   }
   if ( !found ) {
      LOG_DEBUG("%s: Controller matching ieee_address (0x%llX) NOT FOUND.\n", __FUNCTION__, ieee_address);
   }
   return found;
}

void ctrlm_obj_network_ble_t::voice_command_status_set(void *data, int size) {
   ctrlm_main_queue_msg_voice_command_status_t *dqm = (ctrlm_main_queue_msg_voice_command_status_t *)data;

   if(size != sizeof(ctrlm_main_queue_msg_voice_command_status_t) || NULL == data) {
      LOG_ERROR("%s: Incorrect parameters\n", __FUNCTION__);
      return;
   } else if(!controller_exists(dqm->controller_id)) {
      LOG_WARN("%s: Controller %u NOT present.\n", __FUNCTION__, dqm->controller_id);
      return;
   }

   switch(dqm->status) {
      case VOICE_COMMAND_STATUS_PENDING: LOG_INFO("%s: PENDING\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_TIMEOUT: LOG_INFO("%s: TIMEOUT\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_OFFLINE: LOG_INFO("%s: OFFLINE\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_SUCCESS: LOG_INFO("%s: SUCCESS\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_FAILURE: LOG_INFO("%s: FAILURE\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_NO_CMDS: LOG_INFO("%s: NO CMDS\n", __FUNCTION__); break;
      default:                           LOG_WARN("%s: UNKNOWN\n", __FUNCTION__); break;
   }
}

bool ctrlm_obj_network_ble_t::controller_is_bound(ctrlm_controller_id_t controller_id) const {
   return (CTRLM_BLE_RESULT_VALIDATION_SUCCESS == controllers_.at(controller_id)->validation_result_get());
}

bool ctrlm_obj_network_ble_t::controller_exists(ctrlm_controller_id_t controller_id) {
   return (controllers_.count(controller_id) > 0);
}

void ctrlm_obj_network_ble_t::controller_remove(ctrlm_controller_id_t controller_id) {
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller id %u doesn't exist\n", __FUNCTION__, controller_id);
      return;
   }
   controllers_[controller_id]->db_destroy();
   delete controllers_[controller_id];
   controllers_.erase(controller_id);
   LOG_INFO("%s: Removed controller %u\n", __FUNCTION__, controller_id);
}

ctrlm_controller_id_t ctrlm_obj_network_ble_t::controller_id_assign() {
   // Get the next available controller id
   for(ctrlm_controller_id_t index = 1; index < CTRLM_MAIN_CONTROLLER_ID_ALL; index++) {
      if(!controller_exists(index)) {
         LOG_INFO("%s: controller id %u\n", __FUNCTION__, index);
         return(index);
      }
   }
   LOG_ERROR("%s: Unable to assign a controller id!\n", __FUNCTION__);
   return(0);
}

ctrlm_controller_id_t ctrlm_obj_network_ble_t::controller_id_get_least_recently_used(void) {
   ctrlm_controller_id_t lru = CTRLM_HAL_CONTROLLER_ID_INVALID;
   time_t lru_time = time(NULL);

   for(std::map<ctrlm_controller_id_t, ctrlm_obj_controller_ble_t *>::iterator it = controllers_.begin(); it != controllers_.end(); it++) {
     if(it->second->getLastKeyTime() < lru_time) {
        lru      = it->first;
        lru_time = it->second->getLastKeyTime();
     }
   }
   return lru;
}

void ctrlm_obj_network_ble_t::controllers_load() {
   std::vector<ctrlm_controller_id_t> controller_ids;
   ctrlm_network_id_t network_id = network_id_get();

   ctrlm_db_ble_controllers_list(network_id, &controller_ids);

   for(std::vector<ctrlm_controller_id_t>::iterator it = controller_ids.begin(); it < controller_ids.end(); it++) {
      controllers_[*it] = new ctrlm_obj_controller_ble_t(*it, *this, CTRLM_BLE_RESULT_VALIDATION_SUCCESS);
      controllers_[*it]->db_load();
   }
}

void ctrlm_obj_network_ble_t::controller_list_get(std::vector<ctrlm_controller_id_t>& list) const {
   THREAD_ID_VALIDATE();
   if(!list.empty()) {
      LOG_WARN("%s: Invalid list.\n", __FUNCTION__);
      return;
   }
   std::vector<ctrlm_controller_id_t>::iterator it_vector = list.begin();

   std::map<ctrlm_controller_id_t, ctrlm_obj_controller_ble_t *>::const_iterator it_map;
   for(it_map = controllers_.begin(); it_map != controllers_.end(); it_map++) {
      if(controller_is_bound(it_map->first)) {
         it_vector = list.insert(it_vector, it_map->first);
      }
      else {
         LOG_WARN("%s: Controller %u NOT bound.\n", __FUNCTION__, it_map->first);
      }
   }
}

void ctrlm_obj_network_ble_t::printStatus() {
   LOG_WARN("%s: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __FUNCTION__);
   // Make sure to ignore the IR controller
   LOG_INFO("%s: Remotes (%d):\n", __FUNCTION__, controllers_.size() - 1);

   for(auto it = controllers_.begin(); it != controllers_.end(); it++) {
      if (BLE_CONTROLLER_TYPE_IR != it->second->getControllerType()) {
         it->second->print_status();
      }
   }
   LOG_INFO("%s: BLE Network Status:    <%s>\n", __FUNCTION__, ctrlm_ble_utils_RcuStateToString(state_));
   LOG_INFO("%s: IR Programming Status: <%s>\n", __FUNCTION__, ctrlm_ir_state_str(ir_state_));
   LOG_WARN("%s: <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n", __FUNCTION__);
}

json_t *ctrlm_obj_network_ble_t::xconf_export_controllers() {
   THREAD_ID_VALIDATE();
   LOG_DEBUG("%s: Enter...\n", __FUNCTION__);

   // map to store unique types and min versions.
   unordered_map<ctrlm_ble_controller_type_t, tuple <ctrlm_version_t, ctrlm_version_t> > minVersions;

   for(auto const &ctr_it : controllers_) {
      ctrlm_obj_controller_ble_t *controller = ctr_it.second;

      // we only want to send one line for each type with min version
      auto type_it = minVersions.find(controller->getControllerType());
      if (type_it != minVersions.end()) {

         ctrlm_version_t revSw = get<0>(type_it->second);
         ctrlm_version_t revHw = get<1>(type_it->second);

         // we already have a product of this type so check for min version
         ctrlm_version_t revController = controller->getSwRevision();
         if (revSw.compare(revController) > 0) {
            // revController is less than what we have in minVersions, so update value
            revSw = revController;
         }
         //Currently we ignore hw version so dont check it

         minVersions[type_it->first] = make_tuple(revSw, revHw);
      }
      else {
         // we dont have type in map so add it;
         tuple <ctrlm_version_t, ctrlm_version_t> revs = make_tuple(controller->getSwRevision(), controller->getHwRevision());
         minVersions[controller->getControllerType()] = revs;
      }
   }
   json_t *ret = json_array();

   for (auto const ctrlType : minVersions) {
      char product_name[CTRLM_MAX_PARAM_STR_LEN];

      // XCONF expects a different name than product name, so need to map here
      switch(ctrlType.first) {
         case BLE_CONTROLLER_TYPE_PR1:
            strncpy(product_name, XCONF_PRODUCT_NAME_PR1, CTRLM_MAX_PARAM_STR_LEN);
            break;
         case BLE_CONTROLLER_TYPE_LC103:
            strncpy(product_name, XCONF_PRODUCT_NAME_LC103, CTRLM_MAX_PARAM_STR_LEN);
            break;
         case BLE_CONTROLLER_TYPE_EC302:
            strncpy(product_name, XCONF_PRODUCT_NAME_EC302, CTRLM_MAX_PARAM_STR_LEN);
            break;
         default:
            LOG_WARN("%s: controller of type %s ignored\n", __FUNCTION__, ctrlm_ble_controller_type_str(ctrlType.first));
            continue;
      }

      ctrlm_version_t revSw = get<0>(ctrlType.second);
      ctrlm_version_t revHw = get<1>(ctrlType.second);

      json_t *temp = json_object();
      LOG_INFO("%s: adding to json - Product = <%s>, FwVer = <%s>, HwVer = <%s>\n", __FUNCTION__, product_name, revSw.toString().c_str(), revHw.toString().c_str());
      json_object_set(temp, "Product", json_string(product_name));
      json_object_set(temp, "FwVer", json_string(revSw.toString().c_str()));
      json_object_set(temp, "HwVer", json_string(revHw.toString().c_str()));

      json_array_append(ret, temp);
   }
   return ret;
}

void ctrlm_obj_network_ble_t::power_state_change(ctrlm_main_queue_power_state_change_t *dqm) {
   g_assert(dqm);

   if ((dqm->old_state != CTRLM_POWER_STATE_DEEP_SLEEP && dqm->new_state == CTRLM_POWER_STATE_DEEP_SLEEP) ||
       (dqm->old_state == CTRLM_POWER_STATE_DEEP_SLEEP && dqm->new_state != CTRLM_POWER_STATE_DEEP_SLEEP))
   {
      ctrlm_hal_ble_HandleDeepsleep_params_t params;
      params.waking_up = dqm->old_state == CTRLM_POWER_STATE_DEEP_SLEEP && dqm->new_state != CTRLM_POWER_STATE_DEEP_SLEEP;
      if (hal_api_handle_deepsleep_) {
         hal_api_handle_deepsleep_(params);
      }
   }
}
