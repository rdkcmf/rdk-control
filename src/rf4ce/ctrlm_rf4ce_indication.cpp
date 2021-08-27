/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2015 RDK Management
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "../ctrlm.h"
#include "../ctrlm_rcu.h"
#include "ctrlm_rf4ce_network.h"
#include "../ctrlm_voice.h"
#ifdef USE_VOICE_SDK
#include "../ctrlm_voice_obj.h"
#endif

static ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_discovery_int(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_disc_params_t params, ctrlm_hal_rf4ce_rsp_disc_params_t *rsp_params, ctrlm_hal_rf4ce_rsp_discovery_t cb, void *cb_data, GCond *cond);
static ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_pair_int(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_pair_params_t params, ctrlm_hal_rf4ce_rsp_pair_params_t *rsp_params, ctrlm_hal_rf4ce_rsp_pair_t cb, void *cb_data, GCond *cond);
static ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_discovery_async(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_disc_params_t params, ctrlm_hal_rf4ce_rsp_discovery_t cb, void *cb_data);
static ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_discovery_sync(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_disc_params_t params, ctrlm_hal_rf4ce_rsp_disc_params_t *rsp_params);
static ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_pair_async(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_pair_params_t params, ctrlm_hal_rf4ce_rsp_pair_t cb, void *cb_data);
static ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_pair_sync(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_pair_params_t params, ctrlm_hal_rf4ce_rsp_pair_params_t *rsp_params);
static ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_unpair_async(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_unpair_params_t params, ctrlm_hal_rf4ce_rsp_unpair_t cb, void *cb_data);
static ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_unpair_sync(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_unpair_params_t params, ctrlm_hal_rf4ce_rsp_unpair_params_t *rsp_params);

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_discovery_int(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_disc_params_t params, ctrlm_hal_rf4ce_rsp_disc_params_t *rsp_params, ctrlm_hal_rf4ce_rsp_discovery_t cb, void *cb_data, GCond *cond) {

   LOG_INFO("%s: Network Id %u\n", __FUNCTION__, id);
   if(!ctrlm_network_id_is_valid(id)) {
      LOG_ERROR("%s: Invalid Network Id %u\n", __FUNCTION__, id);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }

   if(NULL == cond) { // asynchronous
      if(NULL == cb) { // Invalid callback
         LOG_ERROR("%s: Invalid callback\n", __FUNCTION__);
         g_assert(0);
         return(CTRLM_HAL_RESULT_ERROR_INVALID_PARAMS);
      }
   }

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_rf4ce_ind_discovery_t msg = {0};

   msg.cond              = cond;
   msg.cb                = cb;
   msg.cb_data           = cb_data;
   msg.params            = params;
   msg.rsp_params        = rsp_params;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::ind_process_discovery, (void *)&msg, sizeof(msg), NULL, id);

   return(CTRLM_HAL_RESULT_SUCCESS);
}


ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_discovery(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_disc_params_t params, ctrlm_hal_rf4ce_rsp_disc_params_t *rsp_params, ctrlm_hal_rf4ce_rsp_discovery_t cb, void *cb_data) {
   if(NULL != rsp_params) {
      return(ctrlm_hal_rf4ce_ind_discovery_sync(id, params, rsp_params));
   } else if(NULL != cb) {
      return(ctrlm_hal_rf4ce_ind_discovery_async(id, params, cb, cb_data));
   }
   return(CTRLM_HAL_RESULT_ERROR_INVALID_PARAMS);
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_discovery_async(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_disc_params_t params, ctrlm_hal_rf4ce_rsp_discovery_t cb, void *cb_data) {
   return(ctrlm_hal_rf4ce_ind_discovery_int(id, params, NULL, cb, cb_data, NULL));
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_discovery_sync(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_disc_params_t params, ctrlm_hal_rf4ce_rsp_disc_params_t *rsp_params) {
   ctrlm_hal_result_t hal_result;
   GCond cond;
   GMutex mutex;

   g_cond_init(&cond);
   g_mutex_init(&mutex);
   g_mutex_lock(&mutex);

   rsp_params->result = CTRLM_HAL_RESULT_DISCOVERY_PENDING;

   hal_result = ctrlm_hal_rf4ce_ind_discovery_int(id, params, rsp_params, NULL, NULL, &cond);

   if(CTRLM_HAL_RESULT_SUCCESS == hal_result) {
      // Wait for the result condition to be signaled
      while(CTRLM_HAL_RESULT_DISCOVERY_PENDING == rsp_params->result) {
         g_cond_wait(&cond, &mutex);
      }
      g_mutex_unlock(&mutex);
   }
   return(hal_result);
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_pair_int(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_pair_params_t params, ctrlm_hal_rf4ce_rsp_pair_params_t *rsp_params, ctrlm_hal_rf4ce_rsp_pair_t cb, void *cb_data, GCond *cond) {

   LOG_INFO("%s: Network Id %u\n", __FUNCTION__, id);
   if(!ctrlm_network_id_is_valid(id)) {
      LOG_ERROR("%s: Invalid Network Id %u\n", __FUNCTION__, id);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }

   if(NULL == cond) { // asynchronous
      if(NULL == cb) { // Invalid callback
         LOG_ERROR("%s: Invalid callback\n", __FUNCTION__);
         g_assert(0);
         return(CTRLM_HAL_RESULT_ERROR_INVALID_PARAMS);
      }
   }

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_rf4ce_ind_pair_t msg = {0};

   msg.cond              = cond;
   msg.cb                = cb;
   msg.cb_data           = cb_data;
   msg.params            = params;
   msg.rsp_params        = rsp_params;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::ind_process_pair, (void *)&msg, sizeof(msg), NULL, id);

   return(CTRLM_HAL_RESULT_SUCCESS);
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_pair(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_pair_params_t params, ctrlm_hal_rf4ce_rsp_pair_params_t *rsp_params, ctrlm_hal_rf4ce_rsp_pair_t cb, void *cb_data) {
   if(NULL != rsp_params) {
      return(ctrlm_hal_rf4ce_ind_pair_sync(id, params, rsp_params));
   } else if(NULL != cb) {
      return(ctrlm_hal_rf4ce_ind_pair_async(id, params, cb, cb_data));
   }
   return(CTRLM_HAL_RESULT_ERROR_INVALID_PARAMS);
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_pair_async(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_pair_params_t params, ctrlm_hal_rf4ce_rsp_pair_t cb, void *cb_data) {
   return(ctrlm_hal_rf4ce_ind_pair_int(id, params, NULL, cb, cb_data, NULL));
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_pair_sync(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_pair_params_t params, ctrlm_hal_rf4ce_rsp_pair_params_t *rsp_params) {
   ctrlm_hal_result_t hal_result;
   GCond cond;
   GMutex mutex;

   g_cond_init(&cond);
   g_mutex_init(&mutex);
   g_mutex_lock(&mutex);

   rsp_params->result = CTRLM_HAL_RESULT_PAIR_REQUEST_PENDING;

   hal_result = ctrlm_hal_rf4ce_ind_pair_int(id, params, rsp_params, NULL, NULL, &cond);

   if(CTRLM_HAL_RESULT_SUCCESS == hal_result) {
      // Wait for the result condition to be signaled
      while(CTRLM_HAL_RESULT_PAIR_REQUEST_PENDING == rsp_params->result) {
         g_cond_wait(&cond, &mutex);
      }
      g_mutex_unlock(&mutex);
   }
   return(hal_result);
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_pair_result(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_pair_result_params_t params) {
   LOG_INFO("%s: Network Id %u\n", __FUNCTION__, id);
   if(!ctrlm_network_id_is_valid(id)) {
      LOG_ERROR("%s: Invalid Network Id %u\n", __FUNCTION__, id);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_rf4ce_ind_pair_result_t msg = {0};

   msg.result            = params.result;
   msg.controller_id     = params.controller_id;
   msg.ieee_address      = params.dst_ieee_addr;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::ind_process_pair_result, (void *)&msg, sizeof(msg), NULL, id);

   return(CTRLM_HAL_RESULT_SUCCESS);
}


ctrlm_hal_result_t ctrlm_hal_ind_unpair_int(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_unpair_params_t params, ctrlm_hal_rf4ce_rsp_unpair_params_t *rsp_params, ctrlm_hal_rf4ce_rsp_unpair_t cb, void *cb_data, GCond *cond) {

   LOG_INFO("%s: Network Id %u\n", __FUNCTION__, id);
   if(!ctrlm_network_id_is_valid(id)) {
      LOG_ERROR("%s: Invalid Network Id %u\n", __FUNCTION__, id);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }

   if(NULL == cond) { // asynchronous
      if(NULL == cb) { // Invalid callback
         LOG_ERROR("%s: Invalid callback\n", __FUNCTION__);
         g_assert(0);
         return(CTRLM_HAL_RESULT_ERROR_INVALID_PARAMS);
      }
   }

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_rf4ce_ind_unpair_t msg = {0};

   msg.cond              = cond;
   msg.cb                = cb;
   msg.cb_data           = cb_data;
   msg.params            = params;
   msg.rsp_params        = rsp_params;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::ind_process_unpair, (void *)&msg, sizeof(msg), NULL, id);

   return(CTRLM_HAL_RESULT_SUCCESS);
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_unpair(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_unpair_params_t params, ctrlm_hal_rf4ce_rsp_unpair_params_t *rsp_params, ctrlm_hal_rf4ce_rsp_unpair_t cb, void *cb_data) {
   if(NULL != rsp_params) {
      return(ctrlm_hal_rf4ce_ind_unpair_sync(id, params, rsp_params));
   } else if(NULL != cb) {
      return(ctrlm_hal_rf4ce_ind_unpair_async(id, params, cb, cb_data));
   }
   return(CTRLM_HAL_RESULT_ERROR_INVALID_PARAMS);
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_unpair_async(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_unpair_params_t params, ctrlm_hal_rf4ce_rsp_unpair_t cb, void *cb_data) {
   return(ctrlm_hal_ind_unpair_int(id, params, NULL, cb, cb_data, NULL));
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_unpair_sync(ctrlm_network_id_t id, ctrlm_hal_rf4ce_ind_unpair_params_t params, ctrlm_hal_rf4ce_rsp_unpair_params_t *rsp_params) {
   ctrlm_hal_result_t hal_result;
   GCond cond;
   GMutex mutex;

   g_cond_init(&cond);
   g_mutex_init(&mutex);
   g_mutex_lock(&mutex);

   rsp_params->result = CTRLM_HAL_RESULT_UNPAIR_PENDING;

   hal_result = ctrlm_hal_ind_unpair_int(id, params, rsp_params, NULL, NULL, &cond);

   if(CTRLM_HAL_RESULT_SUCCESS == hal_result) {
      // Wait for the result condition to be signaled
      while(CTRLM_HAL_RESULT_UNPAIR_PENDING == rsp_params->result) {
         g_cond_wait(&cond, &mutex);
      }
      g_mutex_unlock(&mutex);
   }
   return(hal_result);
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_ind_data(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_hal_rf4ce_ind_data_params_t params) {
   ctrlm_network_type_t network_type;
   
   LOG_DEBUG("%s: Network Id %u Controller Id %u\n", __FUNCTION__, network_id, controller_id);
   if(!ctrlm_main_successful_init_get()) {
      LOG_WARN("%s: Network Is Not Initialized.  Ignore.\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_NOT_READY);
   }

   if(params.data == NULL && params.cb_data_read == NULL)  {
      LOG_ERROR("%s: Invalid parameters\n", __FUNCTION__);
      g_assert(0);
      return(CTRLM_HAL_RESULT_ERROR);
   }

   if(!ctrlm_network_id_is_valid(network_id)) {
      LOG_ERROR("%s: Invalid Network Id %u\n", __FUNCTION__, network_id);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }
   network_type = ctrlm_network_type_get(network_id);
   
   if(network_type != CTRLM_NETWORK_TYPE_RF4CE) {
      LOG_ERROR("%s: Invalid Network Type %d\n", __FUNCTION__, network_type);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }

   #define RF4CE_RX_FLAG_BROADCAST        (0x1)
   #define RF4CE_RX_FLAG_SECURITY_ENABLED (0x2)
   #define RF4CE_RX_FLAG_VENDOR_SPECIFIC  (0x4)

   LOG_DEBUG("%s: Profile Id 0x%X Vendor Id 0x%X Size %d Data %p Flags 0x%X\n", __FUNCTION__, params.profile_id, params.vendor_id, params.length, params.data, params.flags);

   // Ensure that we support the profile id
   if(params.profile_id != CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU && params.profile_id != CTRLM_RF4CE_PROFILE_ID_VOICE && params.profile_id != CTRLM_RF4CE_PROFILE_ID_DEVICE_UPDATE) {
      LOG_ERROR("%s: Unsupported profile id 0x%02X\n", __FUNCTION__, params.profile_id);
      return(CTRLM_HAL_RESULT_ERROR);
   }
   // Ensure that we support the vendor id
   if(params.vendor_id != CTRLM_RF4CE_VENDOR_ID_COMCAST) {
      LOG_ERROR("%s: Unsupported vendor id 0x%04X\n", __FUNCTION__, params.vendor_id);
      return(CTRLM_HAL_RESULT_ERROR);
   }
   // Check the flags
   if(!(params.flags & RF4CE_RX_FLAG_VENDOR_SPECIFIC)) {
      LOG_ERROR("%s: Invalid flags 0x%02X\n", __FUNCTION__, params.flags);
      return(CTRLM_HAL_RESULT_ERROR);
   }
   // Check for Network Payload
   if(params.length == 0) {
      LOG_WARN("%s: Packet with NWK Payload length 0 (Profile ID 0x%02X)!\n", __FUNCTION__, params.profile_id);
      return(CTRLM_HAL_RESULT_ERROR);
   } else if(params.length > CTRLM_RF4CE_MAX_PAYLOAD_LEN) {
      LOG_WARN("%s: Packet with NWK Payload length greater than max! (Profile ID 0x%02X, Length %d)\n", __FUNCTION__, params.profile_id, params.length);
      return(CTRLM_HAL_RESULT_ERROR);
   }

   if(params.profile_id == CTRLM_RF4CE_PROFILE_ID_VOICE) {
      ctrlm_hal_frequency_agility_t frequency_agility = CTRLM_HAL_FREQUENCY_AGILITY_NO_CHANGE;
      ctrlm_hal_result_t result = ctrlm_voice_ind_data_rf4ce(network_id, controller_id, params.timestamp, params.command_id, params.length, params.data, params.cb_data_read, params.cb_data_param, params.lqi, &frequency_agility);

      if(frequency_agility != CTRLM_HAL_FREQUENCY_AGILITY_NO_CHANGE) {
         // Change the frequency agility state
         ctrlm_hal_network_property_frequency_agility_t property;
         property.state = frequency_agility;
         ctrlm_network_property_set(network_id, CTRLM_HAL_NETWORK_PROPERTY_FREQUENCY_AGILITY, (void *)&property, sizeof(property));
      }

      // Voice profile indications are handled in the context of the HAL driver indication
      return(result);
   }

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_rf4ce_ind_data_t msg = {0};

   msg.controller_id     = controller_id;
   msg.timestamp         = params.timestamp;
   msg.profile_id        = params.profile_id;
   msg.length            = params.length;
   if(params.data != NULL) {
      errno_t safec_rc = memcpy_s(msg.data, msg.length, params.data, params.length);
      ERR_CHK(safec_rc);
   } else {
      if(params.length != params.cb_data_read(params.length, msg.data, params.cb_data_param)) {
         LOG_ERROR("%s: unable to read data!\n", __FUNCTION__);
         return(CTRLM_HAL_RESULT_ERROR);
      }
   }

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::ind_process_data, (void *)&msg, sizeof(msg), NULL, network_id);

   return(CTRLM_HAL_RESULT_SUCCESS);
}

// Note this thread is not called in control manager's context so it can't use global state unless a mutex is put in place
ctrlm_hal_result_t ctrlm_voice_ind_data_rf4ce(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_timestamp_t timestamp, guchar command_id, unsigned long data_length, guchar *data, ctrlm_hal_rf4ce_data_read_t cb_data_read, void *cb_data_param, unsigned char lqi, ctrlm_hal_frequency_agility_t *frequency_agility) {
   ctrlm_hal_frequency_agility_t agility_state = CTRLM_HAL_FREQUENCY_AGILITY_NO_CHANGE;
#ifdef USE_VOICE_SDK
   ctrlm_voice_t *voice_obj = ctrlm_get_voice_obj();
   if(NULL == voice_obj) {
#else
   if(ctrlm_voice_is_initialized() == false) {
#endif
      LOG_ERROR("%s: Voice command cannot be processed until initialization is complete.\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR);
   }

   if(!(command_id >= MSO_VOICE_CMD_ID_DATA_ADPCM_BEGIN      && command_id <= MSO_VOICE_CMD_ID_DATA_ADPCM_END) &&
      !(command_id >= MSO_VOICE_CMD_ID_VOICE_SESSION_REQUEST && command_id <= MSO_VOICE_CMD_ID_VOICE_SESSION_STOP)) {
      LOG_ERROR("%s: Unsupported command id 0x%02X\n", __FUNCTION__, command_id);
      return(CTRLM_HAL_RESULT_ERROR);
   }
   if(command_id == MSO_VOICE_CMD_ID_VOICE_SESSION_REQUEST) {
#ifdef USE_VOICE_SDK
      guchar local_data[data_length]            = {'\0'};
      voice_session_type_t         type         = VOICE_SESSION_TYPE_STANDARD;
      voice_session_audio_format_t audio_format = VOICE_SESSION_AUDIO_FORMAT_ADPCM_16K;
      guchar  request_data_len = 0;
      guchar *request_data     = NULL;
      if(data_length >= 3) {
         if(NULL == data) {
            if(data_length != cb_data_read(data_length, local_data, cb_data_param)) {
               LOG_ERROR("%s: unable to read data\n", __FUNCTION__);
            }
            data = local_data;
         }
         type         = (voice_session_type_t) data[1];
         audio_format = (voice_session_audio_format_t) data[2];
         if(type == VOICE_SESSION_TYPE_FAR_FIELD) {
            request_data_len = data_length - 3;
            if(request_data_len > VOICE_SESSION_REQ_DATA_LEN_MAX) {
               LOG_ERROR("%s: data len exceeds max <%u>\n", __FUNCTION__, request_data_len);
               request_data_len = 0;
            } else {
               request_data = &data[3];
            }
         }
      }

      ctrlm_main_queue_msg_voice_session_request_t msg = {0};

      msg.controller_id     = controller_id;
      msg.timestamp         = timestamp;
      msg.type              = type;
      msg.audio_format      = audio_format;
      msg.status            = VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK;
      msg.reason            = CTRLM_VOICE_SESSION_ABORT_REASON_MAX;
      if(request_data_len && request_data != NULL) {
         msg.data_len          = request_data_len;
         errno_t safec_rc = memcpy_s(msg.data, sizeof(msg.data), request_data, request_data_len);
         ERR_CHK(safec_rc);
      }
      ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::ind_process_voice_session_request, &msg, sizeof(msg), NULL, network_id);
#else
      if(ctrlm_voice_ind_voice_session_request(network_id, controller_id, timestamp)) {
         // Need to disable frequency agility (channel hopping)
         agility_state = CTRLM_HAL_FREQUENCY_AGILITY_DISABLE;
      }
#endif
   } else if(command_id == MSO_VOICE_CMD_ID_CHANNEL_CHECK_REQUEST) {
      LOG_INFO("%s: channel check request\n", __FUNCTION__);
      // Need to disable frequency agility (channel hopping)
      agility_state = CTRLM_HAL_FREQUENCY_AGILITY_DISABLE;
   } else if(command_id == MSO_VOICE_CMD_ID_VOICE_SESSION_STOP) {
      if(data_length == 1 || data_length == 3) {  // Remote supports old stop command (TODO: Do not accept 3 byte packet after XR15 is fixed)
#ifdef USE_VOICE_SDK // TODO add stop reason to voice sdk implementation
         voice_obj->voice_session_end(network_id, controller_id, CTRLM_VOICE_SESSION_END_REASON_DONE, &timestamp);
#else
         LOG_INFO("%s: session stop - regular\n", __FUNCTION__);
         agility_state = ctrlm_voice_ind_voice_session_end(network_id, controller_id, CTRLM_VOICE_SESSION_END_REASON_DONE, 0);
#endif
      } else if(data_length == 4) { // Remote supports enhanced stop command
         LOG_INFO("%s: session stop - enhanced\n", __FUNCTION__);
         guchar local_data[data_length];
         if(data == NULL) {
            data = local_data;
            if(data_length != cb_data_read(data_length, local_data, cb_data_param)) {
               LOG_ERROR("%s: unable to read data\n", __FUNCTION__);
               data[1] = CRTLM_VOICE_REMOTE_VOICE_END_MIC_KEY_RELEASE; // assume key release
            }
         }

         ctrlm_voice_remote_voice_end_reason_t reason = (ctrlm_voice_remote_voice_end_reason_t)(data[1]);

         if(reason == CRTLM_VOICE_REMOTE_VOICE_END_SECONDARY_KEY_PRESS) {
#ifdef USE_VOICE_SDK
            voice_obj->voice_session_end(network_id, controller_id, CTRLM_VOICE_SESSION_END_REASON_OTHER_KEY_PRESSED, &timestamp);
#else
            guchar key_code = data[2];
            agility_state = ctrlm_voice_ind_voice_session_end(network_id, controller_id, CTRLM_VOICE_SESSION_END_REASON_OTHER_KEY_PRESSED, key_code);
#endif
         } else { // CRTLM_VOICE_REMOTE_*
            ctrlm_voice_session_end_reason_t session_end_reason = CTRLM_VOICE_SESSION_END_REASON_DONE;

            if(reason == CRTLM_VOICE_REMOTE_VOICE_END_TIMEOUT_MAXIMUM) {
               session_end_reason = CTRLM_VOICE_SESSION_END_REASON_TIMEOUT_MAXIMUM;
            } else if(reason == CRTLM_VOICE_REMOTE_VOICE_END_MINIMUM_QOS) {
               session_end_reason = CTRLM_VOICE_SESSION_END_REASON_MINIMUM_QOS;
            }
#ifdef USE_VOICE_SDK
            voice_obj->voice_session_end(network_id, controller_id, session_end_reason, &timestamp);
#else
            agility_state = ctrlm_voice_ind_voice_session_end(network_id, controller_id, session_end_reason, 0);
#endif
         }
      }
   } else { // Voice fragment
#ifdef USE_VOICE_SDK
   if(NULL == data) {
      unsigned char buf[data_length] = {'\0'};
      if(data_length != cb_data_read(data_length, buf, cb_data_param)) {
         LOG_WARN("%s: Did not read as much data as expected\n", __FUNCTION__);
      }
      voice_obj->voice_session_data(network_id, controller_id, (char *)buf, data_length, &timestamp);
   } else {
      voice_obj->voice_session_data(network_id, controller_id, (char *)data, data_length, &timestamp);
   }
#else
      ctrlm_voice_ind_voice_session_fragment(network_id, controller_id, command_id, data_length, data, cb_data_read, cb_data_param, lqi);
#endif
   }
   if(NULL != frequency_agility) {
      *frequency_agility = agility_state;
   }
   return(CTRLM_HAL_RESULT_SUCCESS);
}
