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

#ifndef _CTRLM_BLE_NETWORK_H_
#define _CTRLM_BLE_NETWORK_H_

// Includes

#include "../ctrlm.h"
#include "../ctrlm_network.h"
#include "ctrlm_hal_ble.h"
#include "ctrlm_ble_controller.h"
#include "ctrlm_ipc_ble.h"
#include <iostream>
#include <string>
#include <map>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <jansson.h>

// End Includes

typedef struct {
   ctrlm_main_queue_msg_header_t               header;
   ctrlm_iarm_call_BleLogLevels_params_t      *params;
   sem_t *                                     semaphore;
} ctrlm_main_queue_msg_ble_log_levels_t;

typedef struct {
   ctrlm_main_queue_msg_header_t               header;
   ctrlm_iarm_call_GetRcuUnpairReason_params_t *params;
   sem_t *                                     semaphore;
} ctrlm_main_queue_msg_get_rcu_unpair_reason_t;

typedef struct {
   ctrlm_main_queue_msg_header_t               header;
   ctrlm_iarm_call_GetRcuRebootReason_params_t *params;
   sem_t *                                     semaphore;
} ctrlm_main_queue_msg_get_rcu_reboot_reason_t;

typedef struct {
   ctrlm_main_queue_msg_header_t          header;
   ctrlm_iarm_call_SendRcuAction_params_t *params;
   sem_t *                                semaphore;
} ctrlm_main_queue_msg_send_rcu_action_t;

typedef struct {
   ctrlm_ble_controller_type_t   controller_type;
   std::string                   path;
   std::string                   image_filename;
   ctrlm_version_t               version_software;
   ctrlm_version_t               version_bootloader_min;
   ctrlm_version_t               version_hardware_min;
   int                           size;
   int                           crc;
   bool                          force_update;
} ctrlm_ble_upgrade_image_info_t;

typedef void *(*ctrlm_hal_ble_network_main_t)(ctrlm_hal_ble_main_init_t *main_init);

// Function Declarations
ctrlm_hal_result_t ctrlm_ble_IndicateHAL_RcuStatus(ctrlm_network_id_t id, ctrlm_hal_ble_RcuStatusData_t *params);
ctrlm_hal_result_t ctrlm_ble_IndicateHAL_Paired(ctrlm_network_id_t id, ctrlm_hal_ble_IndPaired_params_t *params);
ctrlm_hal_result_t ctrlm_ble_IndicateHAL_UnPaired(ctrlm_network_id_t id, ctrlm_hal_ble_IndUnPaired_params_t *params);
ctrlm_hal_result_t ctrlm_ble_IndicateHAL_VoiceData(ctrlm_network_id_t id, ctrlm_hal_ble_VoiceData_t *params);
ctrlm_hal_result_t ctrlm_ble_IndicateHAL_Keypress(ctrlm_network_id_t id, ctrlm_hal_ble_IndKeypress_params_t *params);
ctrlm_hal_result_t ctrlm_ble_Request_VoiceSessionBegin(ctrlm_network_id_t id, ctrlm_hal_ble_ReqVoiceSession_params_t *params);
ctrlm_hal_result_t ctrlm_ble_Request_VoiceSessionEnd(ctrlm_network_id_t id, ctrlm_hal_ble_ReqVoiceSession_params_t *params);

// End Function Declarations

// Class ctrlm_obj_network_ble_t

class ctrlm_obj_network_ble_t : public ctrlm_obj_network_t {
public:
                                 ctrlm_obj_network_ble_t(ctrlm_network_type_t type, ctrlm_network_id_t id, const char *name, gboolean mask_key_codes, json_t *json_obj_net_ip, GThread *original_thread);
   virtual                      ~ctrlm_obj_network_ble_t();
   virtual ctrlm_hal_result_t    hal_init_request(GThread *ctrlm_main_thread);
   void                          hal_init_confirm(ctrlm_hal_ble_cfm_init_params_t params);
   virtual void                  hal_init_complete();
   virtual void                  hal_init_cfm(void *data, int size);

   void                          ind_process_rcu_status(void *data, int size);
   void                          ind_process_paired(void *data, int size);
   void                          ind_process_unpaired(void *data, int size);
   void                          ind_process_keypress(void *data, int size);

   virtual void                  req_process_network_status(void *data, int size);
   virtual void                  req_process_controller_status(void *data, int size);
   
   virtual void                  req_process_voice_session_begin(void *data, int size);
   virtual void                  req_process_voice_session_end(void *data, int size);

   virtual void                  req_process_start_pairing(void *data, int size);
   virtual void                  req_process_pair_with_code(void *data, int size);
   virtual void                  req_process_get_rcu_status(void *data, int size);
   virtual void                  req_process_get_last_keypress(void *data, int size);
   virtual void                  req_process_ir_set_code(void *data, int size);
   virtual void                  req_process_ir_clear_codes(void *data, int size);
   virtual void                  req_process_find_my_remote(void *data, int size);
   void                          req_process_get_ble_log_levels(void *data, int size);
   void                          req_process_set_ble_log_levels(void *data, int size);
   void                          req_process_get_rcu_unpair_reason(void *data, int size);
   void                          req_process_get_rcu_reboot_reason(void *data, int size);
   void                          req_process_send_rcu_action(void *data, int size);

   virtual void                  req_process_network_managed_upgrade(void *data, int size);
   virtual void                  req_process_network_continue_upgrade(void *data, int size);
   virtual json_t *              xconf_export_controllers();
   void                          addUpgradeImage(ctrlm_ble_upgrade_image_info_t image_info);

   virtual void                  voice_command_status_set(void *data, int size);
   virtual void                  process_voice_controller_metrics(void *data, int size);
   virtual void                  ind_process_voice_session_end(void *data, int size);

   void                          ctrlm_ble_ProcessIndicateHAL_VoiceData(void *data, int size);

   void                          controllers_load();
   virtual void                  controller_list_get(std::vector<ctrlm_controller_id_t>& list) const;
   virtual bool                  controller_exists(ctrlm_controller_id_t controller_id);

   bool                          getControllerId(unsigned long long ieee_address, ctrlm_controller_id_t *controller_id);

   void                          populate_rcu_status_message(ctrlm_iarm_RcuStatus_params_t *status);
   void                          iarm_event_rcu_status();
   void                          printStatus();
   virtual void                  factory_reset();

private:
   ctrlm_obj_network_ble_t();
   ctrlm_hal_ble_network_main_t           hal_api_main_;
   ctrlm_hal_req_term_t                   hal_api_term_;

   ctrlm_hal_ble_req_StartThreads_t       hal_api_start_threads_;
   ctrlm_hal_ble_req_GetDevices_t         hal_api_get_devices_;
   ctrlm_hal_ble_req_GetAllRcuProps_t     hal_api_get_all_rcu_props_;
   ctrlm_hal_ble_req_StartDiscovery_t     hal_api_discovery_start_;
   ctrlm_hal_ble_req_PairWithCode_t       hal_api_pair_with_code_;
   ctrlm_hal_ble_req_Unpair_t             hal_api_unpair_;
   ctrlm_hal_ble_req_StartAudioStream_t   hal_api_start_audio_stream_;
   ctrlm_hal_ble_req_StopAudioStream_t    hal_api_stop_audio_stream_;
   ctrlm_hal_ble_req_GetAudioStats_t      hal_api_get_audio_stats_;
   ctrlm_hal_ble_req_IRSetCode_t          hal_api_set_ir_codes_;
   ctrlm_hal_ble_req_IRClear_t            hal_api_clear_ir_codes_;
   ctrlm_hal_ble_req_FindMe_t             hal_api_find_me_;
   ctrlm_hal_ble_req_GetDaemonLogLevels_t hal_api_get_daemon_log_levels_;
   ctrlm_hal_ble_req_SetDaemonLogLevels_t hal_api_set_daemon_log_levels_;
   ctrlm_hal_ble_req_FwUpgrade_t          hal_api_fw_upgrade_;
   ctrlm_hal_ble_req_GetRcuUnpairReason_t hal_api_get_rcu_unpair_reason_;
   ctrlm_hal_ble_req_GetRcuRebootReason_t hal_api_get_rcu_reboot_reason_;
   ctrlm_hal_ble_req_SendRcuAction_t      hal_api_send_rcu_action_;

   ctrlm_ble_state_t                      state_;
   ctrlm_ir_state_t                       ir_state_;
   int                                    pairing_code_;

   bool                                   upgrade_in_progress_;
   guint                                  upgrade_timer_tag_;

   std::map <ctrlm_controller_id_t, ctrlm_obj_controller_ble_t *> controllers_;

   std::map <ctrlm_ble_controller_type_t, ctrlm_ble_upgrade_image_info_t> upgrade_images_;

   bool                  controller_is_bound(ctrlm_controller_id_t controller_id) const;
   void                  controller_remove(ctrlm_controller_id_t controller_id);
   ctrlm_controller_id_t controller_add(ctrlm_hal_ble_rcu_data_t &rcu_data);
   ctrlm_controller_id_t controller_id_assign(void);
   ctrlm_controller_id_t controller_id_get_least_recently_used(void);

   gboolean              load_config(json_t *json_obj_net);
};

// End Class ctrlm_obj_network_ble_t

#endif
