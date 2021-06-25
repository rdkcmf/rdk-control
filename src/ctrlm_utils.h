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
#ifndef _CTRLM_UTILS_H_
#define _CTRLM_UTILS_H_

#include <semaphore.h>
#include <string>
#include "ctrlm_ipc.h"
#include "ctrlm_ipc_rcu.h"
#include "ctrlm_ipc_voice.h"
#include "ctrlm_ipc_device_update.h"
#include "ctrlm_rcu.h"
#include "ctrlm_hal.h"
#include "ctrlm_hal_rf4ce.h"
#include "libIBus.h"

#define TIMEOUT_TAG_INVALID (0)

#define Q_NOTATION_TO_DOUBLE(input, bits) ((double)((double)input) / (double)(1 << bits))

typedef enum
{
   CTRLM_IR_REMOTE_TYPE_XR11V2,
   CTRLM_IR_REMOTE_TYPE_XR15V1,
   CTRLM_IR_REMOTE_TYPE_NA,
   CTRLM_IR_REMOTE_TYPE_UNKNOWN,
   CTRLM_IR_REMOTE_TYPE_COMCAST,
   CTRLM_IR_REMOTE_TYPE_PLATCO,
   CTRLM_IR_REMOTE_TYPE_XR15V2,
   CTRLM_IR_REMOTE_TYPE_XR16V1,
   CTRLM_IR_REMOTE_TYPE_XRAV1,
   CTRLM_IR_REMOTE_TYPE_XR20V1,
   CTRLM_IR_REMOTE_TYPE_UNDEFINED
} ctrlm_ir_remote_type;

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef BREAKPAD_SUPPORT
void ctrlm_crash(void);
#endif

const char *ctrlm_invalid_return(int value);

guint ctrlm_timeout_create(guint timeout, GSourceFunc function, gpointer user_data);
//guint ctrlm_timeout_update(guint timeout_tag, guint timeout);
void ctrlm_timeout_destroy(guint *p_timeout_tag);

void ctrlm_print_data_hex(const char *prefix, guchar *data, unsigned int length, unsigned int width);
void ctrlm_print_controller_status(const char *prefix, ctrlm_controller_status_t *status);

const char *ctrlm_main_queue_msg_type_str(ctrlm_main_queue_msg_type_t type);
const char *ctrlm_controller_status_cmd_result_str(ctrlm_controller_status_cmd_result_t result);


const char *ctrlm_key_status_str(ctrlm_key_status_t key_status);
const char *ctrlm_key_code_str(ctrlm_key_code_t key_code);
const char *ctrlm_iarm_call_result_str(ctrlm_iarm_call_result_t result);
const char *ctrlm_iarm_result_str(IARM_Result_t result);
const char *ctrlm_access_type_str(ctrlm_access_type_t access_type);
std::string ctrlm_network_type_str(ctrlm_network_type_t network_type);
std::string ctrlm_controller_name_str(ctrlm_controller_id_t controller);
const char *ctrlm_unbind_reason_str(ctrlm_unbind_reason_t reason);

const char *ctrlm_rcu_validation_result_str(ctrlm_rcu_validation_result_t validation_result);
const char *ctrlm_rcu_configuration_result_str(ctrlm_rcu_configuration_result_t configuration_result);
const char *ctrlm_rcu_rib_attr_id_str(ctrlm_rcu_rib_attr_id_t attribute_id);
const char *ctrlm_rcu_binding_type_str(ctrlm_rcu_binding_type_t binding_type);
const char *ctrlm_rcu_validation_type_str(ctrlm_rcu_validation_type_t validation_type);
const char *ctrlm_rcu_binding_security_type_str(ctrlm_rcu_binding_security_type_t security_type);
const char *ctrlm_rcu_ghost_code_str(ctrlm_rcu_ghost_code_t ghost_code);
const char *ctrlm_rcu_function_str(ctrlm_rcu_function_t function);
const char *ctrlm_rcu_controller_type_str(ctrlm_rcu_controller_type_t controller_type);
const char *ctrlm_rcu_reverse_cmd_result_str(ctrlm_rcu_reverse_cmd_result_t result);
const char *ctrlm_rcu_ir_remote_types_str(ctrlm_ir_remote_type controller_type);

const char *ctrlm_voice_session_result_str(ctrlm_voice_session_result_t result);
const char *ctrlm_voice_session_end_reason_str(ctrlm_voice_session_end_reason_t reason);
const char *ctrlm_voice_session_abort_reason_str(ctrlm_voice_session_abort_reason_t reason);
const char *ctrlm_voice_internal_error_str(ctrlm_voice_internal_error_t error);
const char *ctrlm_voice_reset_type_str(ctrlm_voice_reset_type_t reset_type);

const char *ctrlm_device_update_iarm_load_type_str(ctrlm_device_update_iarm_load_type_t load_type);
const char *ctrlm_device_update_iarm_load_result_str(ctrlm_device_update_iarm_load_result_t load_result);
const char *ctrlm_device_update_image_type_str(ctrlm_device_update_image_type_t image_type);
const char *ctrlm_bind_status_str(ctrlm_bind_status_t bind_status);
const char *ctrlm_close_pairing_window_reason_str(ctrlm_close_pairing_window_reason reason);
const char *ctrlm_battery_event_str(ctrlm_rcu_battery_event_t event);
const char *ctrlm_dsp_event_str(ctrlm_rcu_dsp_event_t event);
const char *ctrlm_rf4ce_reboot_reason_str(controller_reboot_reason_t reboot_reason);

const char *ctrlm_ir_state_str(ctrlm_ir_state_t state);

const char *ctrlm_power_state_str(ctrlm_power_state_t state);

bool        ctrlm_file_copy(const char* src, const char* dst, bool overwrite);
bool        ctrlm_file_exists(const char* path);
bool        ctrlm_file_timestamp_get(const char *path, guint64 *ts);
bool        ctrlm_file_timestamp_set(const char *path, guint64  ts);

char       *ctrlm_get_file_contents(const char *path);
char       *ctrlm_do_regex(char *re, char *str);

bool        ctrlm_dsmgr_init();
bool        ctrlm_dsmgr_mute_audio(bool mute);
bool        ctrlm_dsmgr_duck_audio(bool enable, bool relative, double vol);
bool        ctrlm_dsmgr_LED(bool on);
bool        ctrlm_dsmgr_deinit();

bool        ctrlm_is_voice_assistant(ctrlm_rcu_controller_type_t controller_type);
ctrlm_remote_keypad_config ctrlm_get_remote_keypad_config(const char *remote_type);

unsigned long long ctrlm_convert_mac_string_to_long (const char* ascii_mac);
std::string        ctrlm_convert_mac_long_to_string (const unsigned long long ieee_address);

bool        ctrlm_archive_extract(std::string file_path_archive, std::string tmp_dir_path, std::string archive_file_name);
void        ctrlm_archive_remove(std::string dir);
bool        ctrlm_tar_archive_extract(std::string file_path_archive, std::string dest_path);
bool        ctrlm_utils_rm_rf(std::string path);
void        ctrlm_utils_sem_wait();
void        ctrlm_utils_sem_post();
void        ctrlm_archive_extract_tmp_dir_make(std::string tmp_dir_path);
void        ctrlm_archive_extract_ble_tmp_dir_make(std::string tmp_dir_path);
bool        ctrlm_archive_extract_ble_check_dir_exists(std::string path);
std::string ctrlm_xml_tag_text_get(std::string xml, std::string tag);

#ifdef __cplusplus
}
#endif

#endif
