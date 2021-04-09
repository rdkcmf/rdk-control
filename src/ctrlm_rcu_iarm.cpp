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
#include <typeinfo>
#include <memory>
#include "libIBus.h"
#include "ctrlm.h"
#include "ctrlm_rcu.h"
#include "ctrlm_utils.h"

static IARM_Result_t ctrlm_rcu_iarm_call_validation_finish(void *arg);
static IARM_Result_t ctrlm_rcu_iarm_call_controller_status(void *arg);
static IARM_Result_t ctrlm_rcu_iarm_call_rib_request_get(void *arg);
static IARM_Result_t ctrlm_rcu_iarm_call_rib_request_set(void *arg);
static IARM_Result_t ctrlm_rcu_iarm_call_controller_link_key(void *arg);
static IARM_Result_t ctrlm_rcu_iarm_call_reverse_cmd(void *arg);
static IARM_Result_t ctrlm_rcu_iarm_call_rf4ce_polling_action(void *arg);

typedef struct {
      const char* iarm_call_name;
      IARM_Result_t (*iarm_call_handler)(void*);
} iarm_call_handler_t;

static iarm_call_handler_t handlers[] = {
      { CTRLM_RCU_IARM_CALL_VALIDATION_FINISH,    &ctrlm_rcu_iarm_call_validation_finish },
      { CTRLM_RCU_IARM_CALL_CONTROLLER_STATUS,    &ctrlm_rcu_iarm_call_controller_status },
      { CTRLM_RCU_IARM_CALL_RIB_REQUEST_GET,      &ctrlm_rcu_iarm_call_rib_request_get },
      { CTRLM_RCU_IARM_CALL_RIB_REQUEST_SET,      &ctrlm_rcu_iarm_call_rib_request_set },
      { CTRLM_RCU_IARM_CALL_CONTROLLER_LINK_KEY,  &ctrlm_rcu_iarm_call_controller_link_key },
      { CTRLM_RCU_IARM_CALL_REVERSE_CMD,          &ctrlm_rcu_iarm_call_reverse_cmd },
      { CTRLM_RCU_IARM_CALL_RF4CE_POLLING_ACTION, &ctrlm_rcu_iarm_call_rf4ce_polling_action }
};

// Keep state since we do not want to service calls on termination
static volatile int running = 0;

gboolean ctrlm_rcu_init_iarm(void) {
   IARM_Result_t rc;
   LOG_INFO("%s\n", __FUNCTION__);

   for (const iarm_call_handler_t& handler :  handlers) {
      rc = IARM_Bus_RegisterCall(handler.iarm_call_name, handler.iarm_call_handler);
   if(rc != IARM_RESULT_SUCCESS) {
         LOG_ERROR("%s: %s %d\n", __FUNCTION__, handler.iarm_call_name, rc);
      return(false);
   }
   }

   // Change to running state so we can accept calls
   g_atomic_int_set(&running, 1);

   return(true);
}

template <typename iarm_event_struct>
static void init_iarm_event_struct(iarm_event_struct& msg,ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) {
   msg.api_revision  = CTRLM_RCU_IARM_BUS_API_REVISION;
   msg.network_id    = network_id;
   msg.network_type  = ctrlm_network_type_get(network_id);
   msg.controller_id = controller_id;
}

void ctrlm_rcu_iarm_event_key_press(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_key_status_t key_status, ctrlm_key_code_t key_code) {

   // Convert discrete power codes to toggle UNLESS the key is from IP Remote, CEDIA community requires discrete for home automation
   if(((key_code == CTRLM_KEY_CODE_POWER_OFF) || (key_code == CTRLM_KEY_CODE_POWER_ON)) && CTRLM_NETWORK_TYPE_IP != ctrlm_network_type_get(network_id)) {
      key_code = CTRLM_KEY_CODE_POWER_TOGGLE;
   }

   // Handle atomic keypresses as key down and key up until atomic is supported in API
   if(CTRLM_KEY_STATUS_ATOMIC == key_status) {
      LOG_INFO("%s: Atomic Keypress\n", __FUNCTION__);
      ctrlm_rcu_iarm_event_key_press(network_id, controller_id, CTRLM_KEY_STATUS_DOWN, key_code);
      ctrlm_rcu_iarm_event_key_press(network_id, controller_id, CTRLM_KEY_STATUS_UP, key_code);
      return;
   }

   ctrlm_rcu_iarm_event_key_press_t msg;
   init_iarm_event_struct(msg, network_id, controller_id);
   msg.key_status    = key_status;
   msg.key_code      = key_code;
   if(ctrlm_is_key_code_mask_enabled()) {
      LOG_INFO("%s: (%u, %u) key %6s <*>\n", __FUNCTION__, network_id, controller_id, ctrlm_key_status_str(key_status));
   } else {
      LOG_INFO("%s: (%u, %u) key %6s (0x%02X) %s\n", __FUNCTION__, network_id, controller_id, ctrlm_key_status_str(key_status), (guchar)key_code, ctrlm_key_code_str(key_code));
   }
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_KEY_PRESS, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_rcu_iarm_event_key_press_validation(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_rcu_controller_type_t controller_type, ctrlm_rcu_binding_type_t binding_type, ctrlm_key_status_t key_status, ctrlm_key_code_t key_code) {
   ctrlm_rcu_iarm_event_key_press_t msg;
   init_iarm_event_struct(msg, network_id, controller_id);
   msg.binding_type  = binding_type;
   msg.key_status    = key_status;
   msg.key_code      = key_code;
   strncpy(msg.controller_type, ctrlm_rcu_controller_type_str(controller_type), CTRLM_RCU_MAX_USER_STRING_LENGTH);
   msg.controller_type[CTRLM_RCU_MAX_USER_STRING_LENGTH - 1] = '\0';
   if(ctrlm_is_key_code_mask_enabled()) {
      LOG_INFO("%s: (%u, %u) Controller Type <%s> key %s *\n", __FUNCTION__, network_id, controller_id, msg.controller_type, ctrlm_key_status_str(key_status));
   } else {
      LOG_INFO("%s: (%u, %u) Controller Type <%s> key %s (0x%02X) %s\n", __FUNCTION__, network_id, controller_id, msg.controller_type, ctrlm_key_status_str(key_status), (guchar)key_code, ctrlm_key_code_str(key_code));
   }
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_VALIDATION_KEY_PRESS, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_rcu_iarm_event_validation_begin(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_rcu_controller_type_t controller_type, ctrlm_rcu_binding_type_t binding_type, ctrlm_rcu_validation_type_t validation_type, ctrlm_key_code_t *validation_keys) {
   ctrlm_rcu_iarm_event_validation_begin_t msg;
   init_iarm_event_struct(msg, network_id, controller_id);
   msg.binding_type    = binding_type;
   msg.validation_type = validation_type;
   strncpy(msg.controller_type, ctrlm_rcu_controller_type_str(controller_type), CTRLM_RCU_MAX_USER_STRING_LENGTH);
   msg.controller_type[CTRLM_RCU_MAX_USER_STRING_LENGTH - 1] = '\0';
   if(validation_keys != NULL) {
      for(unsigned long index = 0; index < CTRLM_RCU_VALIDATION_KEY_QTY; index++) {
         msg.validation_keys[index] = validation_keys[index];
      }
   } else {
      for(unsigned long index = 0; index < CTRLM_RCU_VALIDATION_KEY_QTY; index++) {
         msg.validation_keys[index] = CTRLM_KEY_CODE_OK;
      }
   }

   LOG_INFO("%s: (%u, %u) Binding type <%s> Validation Type <%s> Controller Type <%s>\n", __FUNCTION__, network_id, controller_id, ctrlm_rcu_binding_type_str(binding_type), ctrlm_rcu_validation_type_str(validation_type), msg.controller_type);
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_VALIDATION_BEGIN, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_rcu_iarm_event_validation_end(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_rcu_controller_type_t controller_type, ctrlm_rcu_binding_type_t binding_type, ctrlm_rcu_validation_type_t validation_type, ctrlm_rcu_validation_result_t validation_result) {
   ctrlm_rcu_iarm_event_validation_end_t msg;
   init_iarm_event_struct(msg, network_id, controller_id);
   msg.binding_type    = binding_type;
   msg.validation_type = validation_type;
   msg.result          = validation_result;
   strncpy(msg.controller_type, ctrlm_rcu_controller_type_str(controller_type), CTRLM_RCU_MAX_USER_STRING_LENGTH);
   msg.controller_type[CTRLM_RCU_MAX_USER_STRING_LENGTH - 1] = '\0';

   LOG_INFO("%s: (%u, %u) Binding Type <%s> Validation Type <%s> Controller Type <%s> Result <%s>\n", __FUNCTION__, network_id, controller_id, ctrlm_rcu_binding_type_str(binding_type), ctrlm_rcu_validation_type_str(validation_type), msg.controller_type, ctrlm_rcu_validation_result_str(validation_result));
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_VALIDATION_END, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_rcu_iarm_event_configuration_complete(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_rcu_controller_type_t controller_type, ctrlm_rcu_binding_type_t binding_type, ctrlm_rcu_configuration_result_t configuration_result) {
   ctrlm_rcu_iarm_event_configuration_complete_t msg;
   init_iarm_event_struct(msg, network_id, controller_id);
   msg.binding_type    = binding_type;
   msg.result          = configuration_result;
   strncpy(msg.controller_type, ctrlm_rcu_controller_type_str(controller_type), CTRLM_RCU_MAX_USER_STRING_LENGTH);
   msg.controller_type[CTRLM_RCU_MAX_USER_STRING_LENGTH - 1] = '\0';

   LOG_INFO("%s: (%u, %u) Controller Type <%s> Binding Type <%s> Result <%s>\n", __FUNCTION__, network_id, controller_id, msg.controller_type, ctrlm_rcu_binding_type_str(binding_type), ctrlm_rcu_configuration_result_str(configuration_result));
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_CONFIGURATION_COMPLETE, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_rcu_iarm_event_function(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_rcu_function_t function, unsigned long value) {
   ctrlm_rcu_iarm_event_function_t msg;
   init_iarm_event_struct(msg, network_id, controller_id);
   msg.function      = function;
   msg.value         = value;

   LOG_INFO("%s: (%u, %u) Function <%s> Value %lu\n", __FUNCTION__, network_id, controller_id, ctrlm_rcu_function_str(function), value);
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_FUNCTION, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_rcu_iarm_event_key_ghost(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_remote_keypad_config remote_keypad_config, ctrlm_rcu_ghost_code_t ghost_code) {
   ctrlm_rcu_iarm_event_key_ghost_t msg;
   init_iarm_event_struct(msg, network_id, controller_id);
   msg.ghost_code           = ghost_code;
   msg.remote_keypad_config = remote_keypad_config;

   LOG_INFO("%s: (%u, %u) Ghost Code <%s> Remote Keypad Config <%u>\n", __FUNCTION__, network_id, controller_id, ctrlm_rcu_ghost_code_str(ghost_code), remote_keypad_config);
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_KEY_GHOST, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_rcu_iarm_event_control(int controller_id, const char *event_source, const char *event_type, const char *event_data, int event_value, int spare_value) {
   ctrlm_rcu_iarm_event_control_t msg;
   msg.api_revision  = CTRLM_RCU_IARM_BUS_API_REVISION;
   msg.controller_id = controller_id;
   msg.event_value   = event_value;
   msg.spare_value   = spare_value;
   strncpy(msg.event_source, event_source, CTRLM_RCU_MAX_EVENT_SOURCE_LENGTH);
   msg.event_source[CTRLM_RCU_MAX_EVENT_SOURCE_LENGTH - 1] = '\0';
   strncpy(msg.event_type, event_type, CTRLM_RCU_MAX_EVENT_TYPE_LENGTH);
   msg.event_type[CTRLM_RCU_MAX_EVENT_TYPE_LENGTH - 1] = '\0';
   strncpy(msg.event_data, event_data, CTRLM_RCU_MAX_EVENT_DATA_LENGTH);
   msg.event_data[CTRLM_RCU_MAX_EVENT_DATA_LENGTH - 1] = '\0';

   LOG_INFO("%s: Controller Id <%d> Source <%s> Type <%s> Data <%s> value <%u>\n", __FUNCTION__, controller_id, event_source, event_type, event_data, event_value);
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_CONTROL, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_rcu_iarm_event_rib_access_controller(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_rcu_rib_attr_id_t identifier, guchar index, ctrlm_access_type_t access_type) {
   ctrlm_rcu_iarm_event_rib_entry_access_t msg;
   init_iarm_event_struct(msg, network_id, controller_id);
   msg.identifier    = identifier;
   msg.index         = index;
   msg.access_type   = access_type;

   LOG_INFO("%s: (%u, %u) Attr <%s> Index %u Access Type <%s>\n", __FUNCTION__, network_id, controller_id, ctrlm_rcu_rib_attr_id_str(identifier), index, ctrlm_access_type_str(access_type));
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_RIB_ACCESS_CONTROLLER, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_rcu_iarm_event_reverse_cmd(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_main_iarm_event_t event, ctrlm_rcu_reverse_cmd_result_t cmd_result, int result_data_size, const unsigned char* result_data) {
   if (event != CTRLM_RCU_IARM_EVENT_RCU_REVERSE_CMD_BEGIN && event != CTRLM_RCU_IARM_EVENT_RCU_REVERSE_CMD_END ) {
      LOG_ERROR("%s: Unexpected IARM Event ID %d!\n", __FUNCTION__, event);
      return;
   }
   int msg_size = sizeof (ctrlm_rcu_iarm_event_reverse_cmd_t) + result_data_size;
   if (result_data_size > 1) {
      --msg_size;
   }
   std::unique_ptr<unsigned char[]> msg_buf(new unsigned char[msg_size]);
   ctrlm_rcu_iarm_event_reverse_cmd_t& msg = *(ctrlm_rcu_iarm_event_reverse_cmd_t*)msg_buf.get();
   init_iarm_event_struct(msg, network_id, controller_id);
   msg.action = CTRLM_RCU_REVERSE_CMD_FIND_MY_REMOTE;
   msg.result = cmd_result;
   msg.result_data_size = result_data_size;
   if (result_data_size > 0) {
      memcpy(&msg.result_data[0], result_data, result_data_size);
   }
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, event, &msg, msg_size);
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

template <typename iarm_call_struct>
static IARM_Result_t ctrlm_rcu_iarm_call_dispatch(iarm_call_struct* params, gboolean (*iarm_call_handler)(iarm_call_struct*)) {
   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: (%s) IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__, typeid(iarm_call_struct).name());
      return(IARM_RESULT_INVALID_STATE);
    }

   if(params == NULL) {
      LOG_ERROR("%s: (%s) NULL parameters\n", __FUNCTION__, typeid(iarm_call_struct).name());
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(params->api_revision != CTRLM_RCU_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: (%s) Unsupported API Revision (%u, %u)\n", __FUNCTION__, typeid(iarm_call_struct).name(), params->api_revision, CTRLM_RCU_IARM_BUS_API_REVISION);
      params->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }

   if(!iarm_call_handler(params)) {
      LOG_ERROR("%s: (%s) Error \n", __FUNCTION__, typeid(iarm_call_struct).name());
      params->result = CTRLM_IARM_CALL_RESULT_ERROR;
   } else {
      params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
   }

   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_rcu_iarm_call_validation_finish(void *arg) {
   ctrlm_rcu_iarm_call_validation_finish_t *params = (ctrlm_rcu_iarm_call_validation_finish_t *) arg;
   LOG_INFO("%s: (%u, %u) Validation Result <%s>\n", __FUNCTION__, params->network_id, params->controller_id, ctrlm_rcu_validation_result_str(params->validation_result));
   return ctrlm_rcu_iarm_call_dispatch(params, &ctrlm_rcu_validation_finish);
}

IARM_Result_t ctrlm_rcu_iarm_call_controller_status(void *arg) {
   ctrlm_rcu_iarm_call_controller_status_t *params = (ctrlm_rcu_iarm_call_controller_status_t *) arg;
   return ctrlm_rcu_iarm_call_dispatch(params, &ctrlm_rcu_controller_status);
}

IARM_Result_t ctrlm_rcu_iarm_call_rib_request_get(void *arg) {
   ctrlm_rcu_iarm_call_rib_request_t *params = (ctrlm_rcu_iarm_call_rib_request_t *) arg;
   return ctrlm_rcu_iarm_call_dispatch(params, &ctrlm_rcu_rib_request_get);
}

IARM_Result_t ctrlm_rcu_iarm_call_rib_request_set(void *arg) {
   ctrlm_rcu_iarm_call_rib_request_t *params = (ctrlm_rcu_iarm_call_rib_request_t *) arg;
   return ctrlm_rcu_iarm_call_dispatch(params, &ctrlm_rcu_rib_request_set);
}

IARM_Result_t ctrlm_rcu_iarm_call_controller_link_key(void *arg) {
   ctrlm_rcu_iarm_call_controller_link_key_t *params = (ctrlm_rcu_iarm_call_controller_link_key_t *) arg;
   return ctrlm_rcu_iarm_call_dispatch(params, &ctrlm_rcu_controller_link_key);
    }

IARM_Result_t ctrlm_rcu_iarm_call_reverse_cmd(void *arg) {
   ctrlm_main_iarm_call_rcu_reverse_cmd_t *params = (ctrlm_main_iarm_call_rcu_reverse_cmd_t *) arg;
   return ctrlm_rcu_iarm_call_dispatch(params, &ctrlm_rcu_reverse_cmd);
}

IARM_Result_t ctrlm_rcu_iarm_call_rf4ce_polling_action(void *arg) {
   ctrlm_rcu_iarm_call_rf4ce_polling_action_t *params = (ctrlm_rcu_iarm_call_rf4ce_polling_action_t *) arg;
   return ctrlm_rcu_iarm_call_dispatch(params, &ctrlm_rcu_rf4ce_polling_action);
}

void ctrlm_rcu_iarm_event_battery_milestone(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_rcu_battery_event_t battery_event, guchar percent) {
   ctrlm_rcu_iarm_event_battery_t msg;
   init_iarm_event_struct(msg, network_id, controller_id);
   msg.battery_event = battery_event;
   msg.percent       = percent;
   LOG_INFO("%s: (%u, %u) Battery Event <%s> Percent <%d>\n", __FUNCTION__, network_id, controller_id, ctrlm_battery_event_str(battery_event), percent);
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_BATTERY_MILESTONE, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_rcu_iarm_event_remote_reboot(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, guchar voltage, controller_reboot_reason_t reason, guint32 assert_number) {
   ctrlm_rcu_iarm_event_remote_reboot_t msg;
   init_iarm_event_struct(msg, network_id, controller_id);
   msg.voltage       = voltage;
   msg.reason        = reason;
   msg.assert_number = assert_number;
   if(reason == CONTROLLER_REBOOT_ASSERT_NUMBER) {
      LOG_INFO("%s: (%u, %u) Voltage <%d> Reboot Reason <%s> Assert Number <%u>\n", __FUNCTION__, network_id, controller_id, voltage, ctrlm_rf4ce_reboot_reason_str(reason), assert_number);
   } else {
      LOG_INFO("%s: (%u, %u) Voltage <%d> Reboot Reason <%s>\n", __FUNCTION__, network_id, controller_id, voltage, ctrlm_rf4ce_reboot_reason_str(reason));
   }
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_RCU_IARM_EVENT_REMOTE_REBOOT, &msg, sizeof(msg));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_rcu_terminate_iarm() {
   // Change state to terminated so we do not accept calls
   g_atomic_int_set(&running, 0);
}
