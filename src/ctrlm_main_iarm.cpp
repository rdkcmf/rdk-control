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
#include "libIBus.h"
#include "libIBusDaemon.h"
#include "irMgr.h"
#include "pwrMgr.h"
#include "sysMgr.h"
#include "comcastIrKeyCodes.h"
#include "ctrlm.h"
#include "ctrlm_ipc.h"
#include "ctrlm_network.h"
#include "ctrlm_utils.h"
#include "dsMgr.h"
#include "dsRpc.h"

static IARM_Result_t ctrlm_main_iarm_call_status_get(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_network_status_get(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_property_set(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_property_get(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_discovery_config_set(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_autobind_config_set(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_precommission_config_set(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_factory_reset(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_controller_unbind(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_ir_remote_usage_get(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_last_key_info_get(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_control_service_set_values(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_control_service_get_values(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_control_service_can_find_my_remote(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_control_service_start_pairing_mode(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_control_service_end_pairing_mode(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_pairing_metrics_get(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_voice_session_begin(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_voice_session_end(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_start_pairing(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_start_pair_with_code(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_find_my_remote(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_get_rcu_status(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_chip_status_get(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_audio_capture_start(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_audio_capture_stop(void *arg);
static IARM_Result_t ctrlm_main_iarm_call_power_state_change(void *arg);
#if CTRLM_HAL_RF4CE_API_VERSION >= 10  && !defined(CTRLM_DPI_CONTROL_NOT_SUPPORTED)
static IARM_Result_t ctrlm_event_handler_power_pre_change(void* pArgs);
#endif

typedef struct {
   const char *   name;
   IARM_BusCall_t handler;
} ctrlm_iarm_call_t;

// Keep state since we do not want to service calls on termination
static volatile int running = 0;

// Array to hold the IARM Calls that will be registered by Control Manager
ctrlm_iarm_call_t ctrlm_iarm_calls[] = {
   {CTRLM_MAIN_IARM_CALL_STATUS_GET,                         ctrlm_main_iarm_call_status_get                         },
   {CTRLM_MAIN_IARM_CALL_NETWORK_STATUS_GET,                 ctrlm_main_iarm_call_network_status_get                 },
   {CTRLM_MAIN_IARM_CALL_PROPERTY_SET,                       ctrlm_main_iarm_call_property_set                       },
   {CTRLM_MAIN_IARM_CALL_PROPERTY_GET,                       ctrlm_main_iarm_call_property_get                       },
   {CTRLM_MAIN_IARM_CALL_DISCOVERY_CONFIG_SET,               ctrlm_main_iarm_call_discovery_config_set               },
   {CTRLM_MAIN_IARM_CALL_AUTOBIND_CONFIG_SET,                ctrlm_main_iarm_call_autobind_config_set                },
   {CTRLM_MAIN_IARM_CALL_PRECOMMISSION_CONFIG_SET,           ctrlm_main_iarm_call_precommission_config_set           },
   {CTRLM_MAIN_IARM_CALL_FACTORY_RESET,                      ctrlm_main_iarm_call_factory_reset                      },
   {CTRLM_MAIN_IARM_CALL_CONTROLLER_UNBIND,                  ctrlm_main_iarm_call_controller_unbind                  },
   {CTRLM_MAIN_IARM_CALL_IR_REMOTE_USAGE_GET,                ctrlm_main_iarm_call_ir_remote_usage_get                },
   {CTRLM_MAIN_IARM_CALL_LAST_KEY_INFO_GET,                  ctrlm_main_iarm_call_last_key_info_get                  },
   {CTRLM_MAIN_IARM_CALL_CONTROL_SERVICE_SET_VALUES,         ctrlm_main_iarm_call_control_service_set_values         },
   {CTRLM_MAIN_IARM_CALL_CONTROL_SERVICE_GET_VALUES,         ctrlm_main_iarm_call_control_service_get_values         },
   {CTRLM_MAIN_IARM_CALL_CONTROL_SERVICE_CAN_FIND_MY_REMOTE, ctrlm_main_iarm_call_control_service_can_find_my_remote },
   {CTRLM_MAIN_IARM_CALL_CONTROL_SERVICE_START_PAIRING_MODE, ctrlm_main_iarm_call_control_service_start_pairing_mode },
   {CTRLM_MAIN_IARM_CALL_CONTROL_SERVICE_END_PAIRING_MODE,   ctrlm_main_iarm_call_control_service_end_pairing_mode   },
   {CTRLM_MAIN_IARM_CALL_PAIRING_METRICS_GET,                ctrlm_main_iarm_call_pairing_metrics_get                },
   {CTRLM_VOICE_IARM_CALL_SESSION_BEGIN,                     ctrlm_main_iarm_call_voice_session_begin                },
   {CTRLM_VOICE_IARM_CALL_SESSION_END,                       ctrlm_main_iarm_call_voice_session_end                  },
   {CTRLM_MAIN_IARM_CALL_GET_RCU_STATUS,                     ctrlm_main_iarm_call_get_rcu_status                     },
   {CTRLM_MAIN_IARM_CALL_START_PAIRING,                      ctrlm_main_iarm_call_start_pairing                      },
   {CTRLM_MAIN_IARM_CALL_START_PAIR_WITH_CODE,               ctrlm_main_iarm_call_start_pair_with_code               },
   {CTRLM_MAIN_IARM_CALL_FIND_MY_REMOTE,                     ctrlm_main_iarm_call_find_my_remote                     },
   {CTRLM_MAIN_IARM_CALL_CHIP_STATUS_GET,                    ctrlm_main_iarm_call_chip_status_get                    },
   {CTRLM_MAIN_IARM_CALL_AUDIO_CAPTURE_START,                ctrlm_main_iarm_call_audio_capture_start                },
   {CTRLM_MAIN_IARM_CALL_AUDIO_CAPTURE_STOP,                 ctrlm_main_iarm_call_audio_capture_stop                 },
   {CTRLM_MAIN_IARM_CALL_POWER_STATE_CHANGE,                 ctrlm_main_iarm_call_power_state_change                 },
#if CTRLM_HAL_RF4CE_API_VERSION >= 10 && !defined(CTRLM_DPI_CONTROL_NOT_SUPPORTED)
   {IARM_BUS_COMMON_API_PowerPreChange,                      ctrlm_event_handler_power_pre_change                    },
#endif
};


gboolean ctrlm_main_iarm_init(void) {
   IARM_Result_t result;
   guint index;

   // Register calls that can be invoked by IARM bus clients
   for(index = 0; index < sizeof(ctrlm_iarm_calls)/sizeof(ctrlm_iarm_call_t); index++) {
      result = IARM_Bus_RegisterCall(ctrlm_iarm_calls[index].name, ctrlm_iarm_calls[index].handler);
      if(IARM_RESULT_SUCCESS != result) {
         LOG_FATAL("%s: Unable to register method %s!\n", __FUNCTION__, ctrlm_iarm_calls[index].name);
         return(false);
      }
   }

   // Change to running state so we can accept calls
   g_atomic_int_set(&running, 1);

   return(true);
}

void ctrlm_main_iarm_terminate(void) {
   //IARM_Result_t result;
   //guint index;

   // Change to stopped or terminated state, so we do not accept new calls
   g_atomic_int_set(&running, 0);

   // IARM Events that we are listening to from other processes
   IARM_Bus_RemoveEventHandler(IARM_BUS_IRMGR_NAME, IARM_BUS_IRMGR_EVENT_IRKEY, ctrlm_event_handler_ir);
   IARM_Bus_RemoveEventHandler(IARM_BUS_IRMGR_NAME, IARM_BUS_IRMGR_EVENT_CONTROL, ctrlm_event_handler_ir);
   IARM_Bus_RemoveEventHandler(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_EVENT_KEYCODE_LOGGING_CHANGED, ctrlm_event_handler_system);

   // Unregister calls that can be invoked by IARM bus clients
   //for(index = 0; index < sizeof(ctrlm_iarm_calls)/sizeof(ctrlm_iarm_call_t); index++) {
   //   result = IARM_Bus_UnRegisterCall(ctrlm_iarm_calls[index].name, ctrlm_iarm_calls[index].handler);
   //   if(IARM_RESULT_SUCCESS != result) {
   //      LOG_FATAL("%s: Unable to unregister method %s!\n", __FUNCTION__, ctrlm_iarm_calls[index].name);
   //   }
   //}
}

IARM_Result_t ctrlm_main_iarm_call_status_get(void *arg) {
   ctrlm_main_iarm_call_status_t *status = (ctrlm_main_iarm_call_status_t *) arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(status == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(status->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, status->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      status->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   if(!ctrlm_main_iarm_call_status_get(status)) {
      status->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_ir_remote_usage_get(void *arg) {
   ctrlm_main_iarm_call_ir_remote_usage_t *ir_remote_usage = (ctrlm_main_iarm_call_ir_remote_usage_t *) arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(ir_remote_usage == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(ir_remote_usage->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, ir_remote_usage->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      ir_remote_usage->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   if(!ctrlm_main_iarm_call_ir_remote_usage_get(ir_remote_usage)) {
      ir_remote_usage->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_last_key_info_get(void *arg) {
   ctrlm_main_iarm_call_last_key_info_t *last_key_info = (ctrlm_main_iarm_call_last_key_info_t *) arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(last_key_info == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(last_key_info->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, last_key_info->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      last_key_info->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   if(!ctrlm_main_iarm_call_last_key_info_get(last_key_info)) {
      last_key_info->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_network_status_get(void *arg) {
   ctrlm_main_iarm_call_network_status_t *status = (ctrlm_main_iarm_call_network_status_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(status == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(status->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, status->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      status->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);
   
   if(!ctrlm_main_iarm_call_network_status_get(status)) {
      status->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_property_set(void *arg) {
   ctrlm_main_iarm_call_property_t *property = (ctrlm_main_iarm_call_property_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == property) {
      LOG_ERROR("%s: NULL Property Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   LOG_INFO("%s: \n", __FUNCTION__);
   
   if(!ctrlm_main_iarm_call_property_set(property)) {
      property->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_property_get(void *arg) {
   ctrlm_main_iarm_call_property_t *property = (ctrlm_main_iarm_call_property_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == property) {
      LOG_ERROR("%s: NULL Property Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   LOG_INFO("%s: \n", __FUNCTION__);
   
   if(!ctrlm_main_iarm_call_property_get(property)) {
      property->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_discovery_config_set(void *arg) {
   ctrlm_main_iarm_call_discovery_config_t *config = (ctrlm_main_iarm_call_discovery_config_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == config) {
      LOG_ERROR("%s: NULL Property Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   
   if(config->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, config->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      config->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);
   
   if(!ctrlm_main_iarm_call_discovery_config_set(config)) {
      config->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_autobind_config_set(void *arg) {
   ctrlm_main_iarm_call_autobind_config_t *config = (ctrlm_main_iarm_call_autobind_config_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == config) {
      LOG_ERROR("%s: NULL Property Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(config->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, config->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      config->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   if(!ctrlm_main_iarm_call_autobind_config_set(config)) {
      config->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_precommission_config_set(void *arg) {
   ctrlm_main_iarm_call_precommision_config_t *config = (ctrlm_main_iarm_call_precommision_config_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == config) {
      LOG_ERROR("%s: NULL Property Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(config->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, config->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      config->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   if(!ctrlm_main_iarm_call_precommission_config_set(config)) {
      config->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_factory_reset(void *arg) {
   ctrlm_main_iarm_call_factory_reset_t *reset = (ctrlm_main_iarm_call_factory_reset_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == reset) {
      LOG_ERROR("%s: NULL Property Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(reset->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, reset->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      reset->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   if(!ctrlm_main_iarm_call_factory_reset(reset)) {
      reset->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_controller_unbind(void *arg) {
   ctrlm_main_iarm_call_controller_unbind_t *unbind = (ctrlm_main_iarm_call_controller_unbind_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == unbind) {
      LOG_ERROR("%s: NULL Property Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(unbind->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, unbind->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      unbind->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   if(!ctrlm_main_iarm_call_controller_unbind(unbind)) {
      unbind->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

void ctrlm_main_iarm_event_binding_button(gboolean active) {
   ctrlm_main_iarm_event_binding_button_t event;
   event.api_revision = CTRLM_MAIN_IARM_BUS_API_REVISION;
   event.active       = active ? 1 : 0;
   LOG_INFO("%s: <%s>\n", __FUNCTION__, active ? "ACTIVE" : "INACTIVE");
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_EVENT_BINDING_BUTTON, &event, sizeof(event));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_main_iarm_event_binding_line_of_sight(gboolean active) {
   ctrlm_main_iarm_event_binding_button_t event;
   event.api_revision = CTRLM_MAIN_IARM_BUS_API_REVISION;
   event.active       = active ? 1 : 0;
   LOG_INFO("%s: <%s>\n", __FUNCTION__, active ? "ACTIVE" : "INACTIVE");
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_EVENT_BINDING_LINE_OF_SIGHT, &event, sizeof(event));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_main_iarm_event_autobind_line_of_sight(gboolean active) {
   ctrlm_main_iarm_event_binding_button_t event;
   event.api_revision = CTRLM_MAIN_IARM_BUS_API_REVISION;
   event.active       = active ? 1 : 0;
   LOG_INFO("%s: <%s>\n", __FUNCTION__, active ? "ACTIVE" : "INACTIVE");
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_EVENT_AUTOBIND_LINE_OF_SIGHT, &event, sizeof(event));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

IARM_Result_t ctrlm_main_iarm_call_audio_capture_start(void *arg) {
   ctrlm_main_iarm_call_audio_capture_t *capture_start = (ctrlm_main_iarm_call_audio_capture_t *)arg;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_audio_capture_start_t *msg = (ctrlm_main_queue_msg_audio_capture_start_t *)g_malloc(sizeof(ctrlm_main_queue_msg_audio_capture_start_t));

   if(NULL == capture_start) {
      LOG_ERROR("%s: null parameters\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }

   if(capture_start->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, capture_start->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      capture_start->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }

   LOG_INFO("%s:\n", __FUNCTION__);

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return IARM_RESULT_OOM;
   }
   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_AUDIO_CAPTURE_START;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_INVALID;
   msg->container         = capture_start->container;
   snprintf(msg->file_path, sizeof(msg->file_path), "%s", capture_start->file_path);
   ctrlm_main_queue_msg_push(msg);

   capture_start->result = CTRLM_IARM_CALL_RESULT_SUCCESS;

   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_audio_capture_stop(void *arg) {
   ctrlm_main_iarm_call_audio_capture_t *capture_stop = (ctrlm_main_iarm_call_audio_capture_t *)arg;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_audio_capture_stop_t *msg = (ctrlm_main_queue_msg_audio_capture_stop_t *)g_malloc(sizeof(ctrlm_main_queue_msg_audio_capture_stop_t));

   if(NULL == capture_stop) {
      LOG_ERROR("%s: null parameters\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(capture_stop->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, capture_stop->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      capture_stop->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }

   LOG_INFO("%s:\n", __FUNCTION__);

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return IARM_RESULT_OOM;
   }
   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_AUDIO_CAPTURE_STOP;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_INVALID;
   ctrlm_main_queue_msg_push(msg);

   capture_stop->result = CTRLM_IARM_CALL_RESULT_SUCCESS;

   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_power_state_change(void *arg) {
   //This function will only be reached by ctrlm-testapp
   ctrlm_main_iarm_call_power_state_change_t *power_state = (ctrlm_main_iarm_call_power_state_change_t *)arg;

   if(NULL == power_state) {
      LOG_ERROR("%s: null parameters\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(power_state->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION ) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, power_state->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      power_state->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }

   LOG_INFO("%s: new state %s\n", __FUNCTION__, ctrlm_power_state_str(power_state->new_state));
   if(!ctrlm_power_state_change(power_state->new_state)) {
      return(IARM_RESULT_OOM);
   }

   power_state->result = CTRLM_IARM_CALL_RESULT_SUCCESS;

   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_control_service_set_values(void *arg) {
   ctrlm_main_iarm_call_control_service_settings_t *settings = (ctrlm_main_iarm_call_control_service_settings_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == settings) {
      LOG_ERROR("%s: NULL Settings Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(settings->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, settings->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      settings->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);
   
   if(!ctrlm_main_iarm_call_control_service_set_values(settings)) {
      settings->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_control_service_get_values(void *arg) {
   ctrlm_main_iarm_call_control_service_settings_t *settings = (ctrlm_main_iarm_call_control_service_settings_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == settings) {
      LOG_ERROR("%s: NULL Settings Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(settings->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, settings->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      settings->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   
   if(!ctrlm_main_iarm_call_control_service_get_values(settings)) {
      settings->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_control_service_can_find_my_remote(void *arg) {
   ctrlm_main_iarm_call_control_service_can_find_my_remote_t *can_find_my_remote = (ctrlm_main_iarm_call_control_service_can_find_my_remote_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == can_find_my_remote) {
      LOG_ERROR("%s: NULL Can Find My Remote Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(can_find_my_remote->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, can_find_my_remote->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      can_find_my_remote->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   
   if(!ctrlm_main_iarm_call_control_service_can_find_my_remote(can_find_my_remote)) {
      can_find_my_remote->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_control_service_start_pairing_mode(void *arg) {
   ctrlm_main_iarm_call_control_service_pairing_mode_t *pairing = (ctrlm_main_iarm_call_control_service_pairing_mode_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == pairing) {
      LOG_ERROR("%s: NULL Pairing Mode Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(pairing->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, pairing->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      pairing->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);
   
   if(!ctrlm_main_iarm_call_control_service_start_pairing_mode(pairing)) {
      pairing->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_control_service_end_pairing_mode(void *arg) {
   ctrlm_main_iarm_call_control_service_pairing_mode_t *pairing = (ctrlm_main_iarm_call_control_service_pairing_mode_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(NULL == pairing) {
      LOG_ERROR("%s: NULL Pairing Mode Argument\n", __FUNCTION__);
      g_assert(0);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(pairing->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, pairing->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      pairing->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);
   
   if(!ctrlm_main_iarm_call_control_service_end_pairing_mode(pairing)) {
      pairing->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_pairing_metrics_get(void *arg) {
   ctrlm_main_iarm_call_pairing_metrics_t *pairing_metrics = (ctrlm_main_iarm_call_pairing_metrics_t *) arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(pairing_metrics == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(pairing_metrics->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, pairing_metrics->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      pairing_metrics->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   if(!ctrlm_main_iarm_call_pairing_metrics_get(pairing_metrics)) {
      pairing_metrics->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_chip_status_get(void *arg) {
   ctrlm_main_iarm_call_chip_status_t *status = (ctrlm_main_iarm_call_chip_status_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(status == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(status->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, status->api_revision, CTRLM_MAIN_IARM_BUS_API_REVISION);
      status->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

#if CTRLM_HAL_RF4CE_API_VERSION < 15 || defined(CTRLM_RF4CE_CHIP_CONNECTIVITY_CHECK_NOT_SUPPORTED)
   status->result = CTRLM_IARM_CALL_RESULT_ERROR_NOT_SUPPORTED;
#else
   if(!ctrlm_main_iarm_call_chip_status_get(status)) {
      status->result = CTRLM_IARM_CALL_RESULT_ERROR;
   }
#endif
   return(IARM_RESULT_SUCCESS);
}

#if CTRLM_HAL_RF4CE_API_VERSION >= 10 && !defined(CTRLM_DPI_CONTROL_NOT_SUPPORTED)
IARM_Result_t ctrlm_event_handler_power_pre_change(void* pArgs)
{
    const IARM_Bus_CommonAPI_PowerPreChange_Param_t* pParams = (const IARM_Bus_CommonAPI_PowerPreChange_Param_t*) pArgs;
    if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
    }
    if(pArgs == NULL) {
       LOG_ERROR("%s: Invalid argument\n", __FUNCTION__);
       return IARM_RESULT_INVALID_PARAM;
    }

    ctrlm_main_queue_power_state_change_t *msg = (ctrlm_main_queue_power_state_change_t *)g_malloc(sizeof(ctrlm_main_queue_power_state_change_t));
    if(NULL == msg) {
       LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
       g_assert(0);
       return IARM_RESULT_OOM;
    }
    //ctrlm is on, sleep, or standby. For Llama standby == networked standby power manager transitions from nsm to light sleep to on but ctrlm needs to be "on" during light sleep to process audio
    msg->header.type = CTRLM_MAIN_QUEUE_MSG_TYPE_POWER_STATE_CHANGE;
    switch(pParams->newState) {
       case IARM_BUS_PWRMGR_POWERSTATE_ON:
       //Today STANDBY is just ON. Soon it will be handled as Networked Standby for Llama
       case IARM_BUS_PWRMGR_POWERSTATE_STANDBY:
       case IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP: msg->new_state = CTRLM_POWER_STATE_ON;         break;
       case IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP:
       case IARM_BUS_PWRMGR_POWERSTATE_OFF:                 msg->new_state = CTRLM_POWER_STATE_DEEP_SLEEP; break;
    }

    LOG_INFO("%s: Power State set to %s >\n", __FUNCTION__, ctrlm_power_state_str(msg->new_state));
    ctrlm_main_queue_msg_push(msg);

    return IARM_RESULT_SUCCESS;
}
#endif

IARM_Result_t ctrlm_main_iarm_call_voice_session_begin(void *arg) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   ctrlm_voice_iarm_call_voice_session_t *params = (ctrlm_voice_iarm_call_voice_session_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(params == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }

   LOG_INFO("%s: params->network_id = <%d>\n", __FUNCTION__, params->network_id);

   // Signal completion of the operation
   sem_t semaphore;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_session_t msg;
   memset(&msg, 0, sizeof(msg));

   sem_init(&semaphore, 0, 0);

   msg.params            = params;
   msg.params->result    = CTRLM_IARM_CALL_RESULT_ERROR;
   msg.semaphore         = &semaphore;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_voice_session_begin, &msg, sizeof(msg), NULL, params->network_id);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);

   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_voice_session_end(void *arg) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   ctrlm_voice_iarm_call_voice_session_t *params = (ctrlm_voice_iarm_call_voice_session_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(params == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }

   LOG_INFO("%s: params->network_id = <%d>\n", __FUNCTION__, params->network_id);

   // Signal completion of the operation
   sem_t semaphore;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_session_t msg;
   memset(&msg, 0, sizeof(msg));

   sem_init(&semaphore, 0, 0);

   msg.params            = params;
   msg.params->result    = CTRLM_IARM_CALL_RESULT_ERROR;
   msg.semaphore         = &semaphore;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_voice_session_end, &msg, sizeof(msg), NULL, params->network_id);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);

   return(IARM_RESULT_SUCCESS);
}


IARM_Result_t ctrlm_main_iarm_call_start_pairing(void *arg) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   ctrlm_iarm_call_StartPairing_params_t *params = (ctrlm_iarm_call_StartPairing_params_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(params == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }

   LOG_INFO("%s: params->network_id = <%d>\n", __FUNCTION__, params->network_id);

   // Signal completion of the operation
   sem_t semaphore;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_start_pairing_t msg;
   memset(&msg, 0, sizeof(msg));

   sem_init(&semaphore, 0, 0);

   msg.params            = params;
   msg.params->result    = CTRLM_IARM_CALL_RESULT_ERROR;
   msg.semaphore         = &semaphore;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_start_pairing, &msg, sizeof(msg), NULL, params->network_id);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);

   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_start_pair_with_code(void *arg) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   ctrlm_iarm_call_StartPairWithCode_params_t *params = (ctrlm_iarm_call_StartPairWithCode_params_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(params == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }

   LOG_INFO("%s: params->network_id = <%d>, params->pair_code = 0x%X\n", __FUNCTION__, params->network_id, params->pair_code);

   // Signal completion of the operation
   sem_t semaphore;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_pair_with_code_t msg;
   memset(&msg, 0, sizeof(msg));

   sem_init(&semaphore, 0, 0);

   msg.params            = params;
   msg.params->result    = CTRLM_IARM_CALL_RESULT_ERROR;
   msg.semaphore         = &semaphore;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_pair_with_code, &msg, sizeof(msg), NULL, params->network_id);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);

   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_find_my_remote(void *arg) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   ctrlm_iarm_call_FindMyRemote_params_t *params = (ctrlm_iarm_call_FindMyRemote_params_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(params == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }

   LOG_INFO("%s: params->network_id = <%d>\n", __FUNCTION__, params->network_id);

   // Signal completion of the operation
   sem_t semaphore;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_find_my_remote_t msg;
   memset(&msg, 0, sizeof(msg));

   sem_init(&semaphore, 0, 0);

   msg.params            = params;
   msg.params->result    = CTRLM_IARM_CALL_RESULT_ERROR;
   msg.semaphore         = &semaphore;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_find_my_remote, &msg, sizeof(msg), NULL, params->network_id);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);

   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_main_iarm_call_get_rcu_status(void *arg) {
   LOG_INFO("%s: Enter...\n", __PRETTY_FUNCTION__);
   ctrlm_iarm_RcuStatus_params_t *params = (ctrlm_iarm_RcuStatus_params_t *)arg;

   if(0 == g_atomic_int_get(&running)) {
      LOG_ERROR("%s: IARM Call received when IARM component in stopped/terminated state, reply with ERROR\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_STATE);
   }
   if(params == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }

   LOG_INFO("%s: params->network_id = <%d>\n", __FUNCTION__, params->network_id);

   // Signal completion of the operation
   sem_t semaphore;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_get_rcu_status_t msg;
   memset(&msg, 0, sizeof(msg));

   sem_init(&semaphore, 0, 0);

   msg.params            = params;
   msg.params->result    = CTRLM_IARM_CALL_RESULT_ERROR;
   msg.semaphore         = &semaphore;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_get_rcu_status, &msg, sizeof(msg), NULL, params->network_id);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);

   return(IARM_RESULT_SUCCESS);
}
