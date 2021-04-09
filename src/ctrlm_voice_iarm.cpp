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
#include "ctrlm.h"
#include "ctrlm_utils.h"
#include "ctrlm_rcu.h"
#include "rf4ce/ctrlm_rf4ce_network.h"
#include "ctrlm_voice.h"
#include "ctrlm_voice_session.h"
#ifdef USE_VOICE_SDK
#include "ctrlm_voice_obj.h"
#endif

IARM_Result_t ctrlm_voice_handle_settings_from_server(void *arg);

// Keep state since we do not want to service calls on termination
static volatile int running = 0;

gboolean ctrlm_voice_iarm_init(void) {
   IARM_Result_t rc;
   LOG_INFO("%s\n", __FUNCTION__);

   rc = IARM_Bus_RegisterCall(CTRLM_VOICE_IARM_CALL_UPDATE_SETTINGS, ctrlm_voice_handle_settings_from_server);
   if(rc != IARM_RESULT_SUCCESS) {
      LOG_ERROR("%s: CTRLM_VOICE_IARM_CALL_UPDATE_SETTINGS %d\n", __FUNCTION__, rc);
      return(false);
   }

   // Change to running state so we can accept calls
   g_atomic_int_set(&running, 1);

   return(true);
}

void ctrlm_voice_iarm_event_session_begin(ctrlm_voice_iarm_event_session_begin_t *event) {
   if(event == NULL) {
      LOG_ERROR("%s: NULL event\n", __FUNCTION__);
      return;
   }
   LOG_INFO("%s: (%u, %s, %u) Session id %lu Language <%s> mime type <%s> is voice assistant <%s>\n", __FUNCTION__, event->network_id, ctrlm_network_type_str(event->network_type).c_str(), event->controller_id, event->session_id, event->language, event->mime_type, event->is_voice_assistant  ? "true" : "false");
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_BEGIN, event, sizeof(*event));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_voice_iarm_event_session_end(ctrlm_voice_iarm_event_session_end_t *event) {
   if(event == NULL) {
      LOG_ERROR("%s: NULL event\n", __FUNCTION__);
      return;
   }
   LOG_INFO("%s: (%u, %s, %u) Session id %lu Reason <%s> is voice assistant <%s>\n", __FUNCTION__, event->network_id, ctrlm_network_type_str(event->network_type).c_str(), event->controller_id, event->session_id, ctrlm_voice_session_end_reason_str(event->reason), event->is_voice_assistant  ? "true" : "false");
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_END, event, sizeof(*event));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_voice_iarm_event_session_result(ctrlm_voice_iarm_event_session_result_t *event) {
   if(event == NULL) {
      LOG_ERROR("%s: NULL event\n", __FUNCTION__);
      return;
   }
   LOG_INFO("%s: (%u, %s, %u) Session id %lu Result <%s>\n", __FUNCTION__, event->network_id, ctrlm_network_type_str(event->network_type).c_str(), event->controller_id, event->session_id, ctrlm_voice_session_result_str(event->result));
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_RESULT, event, sizeof(*event));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_voice_iarm_event_session_stats(ctrlm_voice_iarm_event_session_stats_t *event) {
   if(event == NULL) {
      LOG_ERROR("%s: NULL event\n", __FUNCTION__);
      return;
   }
   LOG_INFO("%s: (%u, %s, %u) Session id %lu\n", __FUNCTION__, event->network_id, ctrlm_network_type_str(event->network_type).c_str(), event->controller_id, event->session_id);
   if(event->session.available) {
      float lost_percent = (event->session.packets_lost * 100.0) / event->session.packets_total;
      LOG_INFO("%s: SESSION rf %lu buf_watermark %lu Pkt Total %lu Pkt Lost %lu (%4.2f%%) link quality %lu\n", __FUNCTION__, event->session.rf_channel, event->session.buffer_watermark, event->session.packets_total, event->session.packets_lost, lost_percent, event->session.link_quality);
   }
   if(event->reboot.available) {
      LOG_INFO("%s: REBOOT reset type %d voltage %4.2f battery percent %u%%\n", __FUNCTION__, event->reboot.reset_type, (event->reboot.voltage * 4.0) / 255, event->reboot.battery_percentage);
   }
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_STATS, event, sizeof(*event));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_voice_iarm_event_session_abort(ctrlm_voice_iarm_event_session_abort_t *event) {
   if(event == NULL) {
      LOG_ERROR("%s: NULL event\n", __FUNCTION__);
      return;
   }
   LOG_INFO("%s: (%u, %s, %u) Session id %lu Reason <%s>\n", __FUNCTION__, event->network_id, ctrlm_network_type_str(event->network_type).c_str(), event->controller_id, event->session_id, ctrlm_voice_session_abort_reason_str(event->reason));
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_ABORT, event, sizeof(*event));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_voice_iarm_event_session_short(ctrlm_voice_iarm_event_session_short_t *event) {
   if(event == NULL) {
      LOG_ERROR("%s: NULL event\n", __FUNCTION__);
      return;
   }
   LOG_INFO("%s: (%u, %s, %u) Session id %lu Reason <%s>\n", __FUNCTION__, event->network_id, ctrlm_network_type_str(event->network_type).c_str(), event->controller_id, event->session_id, ctrlm_voice_session_end_reason_str(event->reason));
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_SESSION_SHORT, event, sizeof(*event));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

void ctrlm_voice_iarm_event_media_service(ctrlm_voice_iarm_event_media_service_t *event) {
   if(event == NULL) {
      LOG_ERROR("%s: NULL event\n", __FUNCTION__);
      return;
   }
   LOG_INFO("%s: url <%s>\n", __FUNCTION__, event->media_service_url);
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_MEDIA_SERVICE, event, sizeof(*event));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}

#ifdef USE_VOICE_SDK
IARM_Result_t ctrlm_voice_handle_settings_from_server(void *arg) {
   ctrlm_voice_iarm_call_settings_t *voice_settings = (ctrlm_voice_iarm_call_settings_t *)arg;

   if(voice_settings == NULL) {
      LOG_ERROR("%s: voice settings NULL\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }

   LOG_INFO("%s: Rxd CTRLM_VOICE_IARM_EVENT_VOICE_SETTINGS - API Revision %u\n", __FUNCTION__, voice_settings->api_revision);

   if(voice_settings->api_revision != CTRLM_VOICE_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, voice_settings->api_revision, CTRLM_VOICE_IARM_BUS_API_REVISION);
      voice_settings->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_INVALID_PARAM);
   }

   if(!ctrlm_get_voice_obj()->voice_configure(voice_settings, true)) {
      return(IARM_RESULT_INVALID_PARAM);
   }

   return(IARM_RESULT_SUCCESS);
}
#endif

void ctrlm_voice_iarm_terminate(void) {
   // Change to stopped or terminated state, so we do not accept new calls
   g_atomic_int_set(&running, 0);
}
