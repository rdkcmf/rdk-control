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
#ifndef _CTRLM_NETWORK_H_
#define _CTRLM_NETWORK_H_

#include <string>
#include <map>
#include <vector>
#include <glib.h>
#include "ctrlm.h"
#include "ctrlm_rcu.h"
#include "ctrlm_controller.h"
#include "ctrlm_voice.h"
#include "ctrlm_recovery.h"
#include "ctrlm_validation.h"
#include "ctrlm_irdb.h"
#include "ctrlm_ipc_device_update.h"
#ifdef USE_VOICE_SDK
#include "ctrlm_voice_obj.h"
#endif

#define THREAD_ID_VALIDATE() thread_id_validate(__FUNCTION__)

typedef struct {
   ctrlm_main_queue_msg_header_t header;
   ctrlm_hal_network_property_t  property;
   void *                        value;
} ctrlm_main_queue_msg_network_property_set_t;

typedef struct {
   ctrlm_main_queue_msg_header_t             header;
   ctrlm_voice_iarm_call_voice_session_t    *params;
   sem_t *                                   semaphore;
} ctrlm_main_queue_msg_voice_session_t;

typedef struct {
   ctrlm_main_queue_msg_header_t             header;
   ctrlm_iarm_RcuStatus_params_t            *params;
   sem_t *                                   semaphore;
} ctrlm_main_queue_msg_get_rcu_status_t;

typedef struct {
   ctrlm_main_queue_msg_header_t             header;
   ctrlm_main_iarm_call_last_key_info_t     *params;
   sem_t *                                   semaphore;
} ctrlm_main_queue_msg_get_last_keypress_t;

typedef struct {
   ctrlm_main_queue_msg_header_t             header;
   ctrlm_iarm_call_StartPairing_params_t    *params;
   sem_t *                                   semaphore;
} ctrlm_main_queue_msg_start_pairing_t;

typedef struct {
   ctrlm_main_queue_msg_header_t                header;
   ctrlm_iarm_call_StartPairWithCode_params_t  *params;
   sem_t *                                      semaphore;
} ctrlm_main_queue_msg_pair_with_code_t;

typedef struct {
   ctrlm_main_queue_msg_header_t             header;
   ctrlm_iarm_call_FindMyRemote_params_t    *params;
   sem_t *                                   semaphore;
} ctrlm_main_queue_msg_find_my_remote_t;

typedef struct {
   ctrlm_main_queue_msg_header_t header;
   bool *                        network_managing_upgrade;
   char                          archive_file_path[CTRLM_DEVICE_UPDATE_PATH_LENGTH];
   char                          temp_dir_path[CTRLM_DEVICE_UPDATE_PATH_LENGTH];
   sem_t *                       semaphore;
} ctrlm_main_queue_msg_network_fw_upgrade_t;

typedef struct {
   GCond                *cond;
   ctrlm_hal_req_term_t  term;
   GThread              *hal_thread;
} ctrlm_network_term_hal_t;

void ctrlm_network_property_set(ctrlm_network_id_t network_id, ctrlm_hal_network_property_t property, void *value, guint32 length);
void ctrlm_network_iarm_event_controller_unbind(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_unbind_reason_t reason);

typedef void *(*ctrlm_hal_network_main_t)(ctrlm_hal_main_init_t *main_init);
typedef ctrlm_hal_result_t (*ctrlm_hal_req_network_term_t)(void);

class ctrlm_obj_network_t
{
public:
   ctrlm_obj_network_t(ctrlm_network_type_t type, ctrlm_network_id_t id, const char *name, gboolean mask_key_codes, GThread *original_thread);
   ctrlm_obj_network_t();
   virtual ~ctrlm_obj_network_t();

   // External methods
   ctrlm_network_id_t   network_id_get() const;
   ctrlm_network_type_t type_get() const;
   const char *         name_get() const;
   const char *         version_get() const;
   void                 receiver_id_set(const std::string& receiver_id);
   std::string          receiver_id_get()  const;
   virtual void         device_id_set(const std::string& device_id);
   std::string          device_id_get()  const;
   void                 service_account_id_set(const std::string& service_account_id);
   std::string          service_account_id_get()  const;
   void                 partner_id_set(const std::string& partner_id);
   std::string          partner_id_get()  const;
   void                 experience_set(const std::string& experience);
   std::string          experience_get()  const;
   void                 mask_key_codes_set(gboolean mask_key_codes);
   gboolean             mask_key_codes_get()  const;
   void                 stb_name_set(const std::string& stb_name);
   std::string          stb_name_get() const;
   virtual ctrlm_hal_result_t network_init(GThread *ctrlm_main_thread);
   virtual void         network_destroy();
   void                 hal_api_main_set(ctrlm_hal_network_main_t main);
   void                 hal_api_set(ctrlm_hal_req_property_get_t property_get,
                                    ctrlm_hal_req_property_set_t property_set,
                                    ctrlm_hal_req_term_t         term);
   virtual ctrlm_hal_result_t   hal_init_request(GThread *ctrlm_main_thread);
   void                 hal_init_confirm(ctrlm_hal_result_t result, const char *version);
   virtual void         hal_init_cfm(void *data, int size);
   ctrlm_hal_result_t   property_get(ctrlm_hal_network_property_t property, void **value);
   ctrlm_hal_result_t   property_set(ctrlm_hal_network_property_t property, void *value);
   void                 thread_monitor_poll(ctrlm_thread_monitor_response_t *response);
   bool                 is_ready() const;
   virtual void         disable_hal_calls();
   virtual bool         message_dispatch(gpointer msg);
   virtual void         hal_init_complete();
   virtual void         controller_list_get(std::vector<ctrlm_controller_id_t>& list) const;
   virtual ctrlm_rcu_controller_type_t ctrlm_controller_type_get(ctrlm_controller_id_t controller_id);
   virtual ctrlm_rcu_binding_type_t    ctrlm_binding_type_get(ctrlm_controller_id_t controller_id);
   virtual void         ctrlm_controller_status_get(ctrlm_controller_id_t controller_id, void *status);
   virtual bool         controller_exists(ctrlm_controller_id_t controller_id);
   virtual void         controller_exists(void *data, int size);
   virtual void         controller_unbind(ctrlm_controller_id_t controller_id, ctrlm_unbind_reason_t reason);
   virtual void         ind_process_voice_session_request(void *data, int size);
   virtual void         ind_process_voice_session_begin(void *data, int size);
   virtual void         ind_process_voice_session_end(void *data, int size);
   virtual void         ind_process_voice_session_result(void *data, int size);
   virtual void         ind_process_voice_session_stats(void *data, int size);
   virtual void         process_voice_controller_metrics(void *data, int size);
   virtual void         voice_command_status_set(void *data, int size);
   virtual gboolean     terminate_voice_session(ctrlm_voice_session_termination_reason_t reason);
   virtual void         set_timers();
   virtual void         xconf_configuration();
   virtual json_t *     xconf_export_controllers();
   virtual bool         is_fmr_supported() const;
   virtual ctrlm_controller_status_cmd_result_t  req_process_reverse_cmd(ctrlm_main_queue_msg_rcu_reverse_cmd_t *dqm);
   virtual void         factory_reset();
   virtual bool         binding_config_set(ctrlm_controller_bind_config_t conf);
   virtual bool         discovery_config_set(ctrlm_controller_discovery_config_t conf);
   virtual void         cs_values_set(const ctrlm_cs_values_t *values, bool db_load);
   virtual void         recovery_set(ctrlm_recovery_type_t recovery);
   virtual bool         backup_hal_nvm();
   virtual void         bind_validation_begin(ctrlm_main_queue_msg_bind_validation_begin_t *dqm);
   virtual void         bind_validation_end(ctrlm_main_queue_msg_bind_validation_end_t *dqm);
   virtual bool         bind_validation_timeout(ctrlm_controller_id_t controller_id);


   virtual void         req_process_network_status(void *data, int size);
   virtual void         req_process_chip_status(void *data, int size);
   virtual void         req_process_controller_link_key(void *data, int size);
   virtual void         req_process_controller_status(void *data, int size);
   virtual void         req_process_controller_product_name(void *data, int size);
   virtual void         req_process_voice_session_begin(void *data, int size);
   virtual void         req_process_voice_session_end(void *data, int size);

   virtual void         req_process_start_pairing(void *data, int size);
   virtual void         req_process_pair_with_code(void *data, int size);
   virtual void         req_process_get_rcu_status(void *data, int size);
   virtual void         req_process_get_last_keypress(void *data, int size);

   virtual void         req_process_ir_set_code(void *data, int size);
   virtual void         req_process_ir_clear_codes(void *data, int size);
   virtual void         req_process_find_my_remote(void *data, int size);

   virtual void         req_process_rib_set(void *data, int size);
   virtual void         req_process_rib_get(void *data, int size);
   virtual void         req_process_polling_action_push(void *data, int size);
   
   virtual void         req_process_network_managed_upgrade(void *data, int size);
   virtual void         req_process_network_continue_upgrade(void *data, int size);

   virtual bool         analyze_assert_reason(const char *assert_info );
   bool                 is_failed_state() const;

   virtual void         power_state_change(ctrlm_main_queue_power_state_change_t *dqm);

   // Internal methods

   std::string              version_;
   ctrlm_hal_result_t       init_result_;
   gboolean                 ready_;
   GThread                  *hal_thread_;
   bool                     failed_state_;

protected:
   GThread *            original_thread_;
   GThread *            ctrlm_main_thread_;
   GCond                condition_;
   GMutex               mutex_;

   ctrlm_network_type_t type_;
   ctrlm_network_id_t   id_;
   std::string          name_;

   const char *         get_thread_name(const GThread *thread_id) const;
   void                 thread_id_validate(const char *pCallingFunction) const;
   virtual gboolean     key_event_hook(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_key_status_t key_status, ctrlm_key_code_t key_code);

private:
   gboolean                     mask_key_codes_;
   std::string                  receiver_id_;
   std::string                  device_id_;
   std::string                  service_account_id_;
   std::string                  partner_id_;
   std::string                  experience_;
   std::string                  stb_name_;
   ctrlm_hal_network_main_t     hal_api_main_;
   ctrlm_hal_req_property_get_t hal_api_property_get_;
   ctrlm_hal_req_property_set_t hal_api_property_set_;
   ctrlm_hal_req_term_t         hal_api_term_;
   gboolean                     mutex_cond_init_;

   static gpointer      terminate_hal(gpointer data);
};

#endif
