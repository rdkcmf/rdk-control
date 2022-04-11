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
#include <glib/gstdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <map>
#include "jansson.h"
#include "libIBus.h"
#include "comcastIrKeyCodes.h"
#include "irMgr.h"
#include "ctrlm.h"
#include "ctrlm_utils.h"
#include "ctrlm_rcu.h"
#include "rf4ce/ctrlm_rf4ce_network.h"
#include "ctrlm_voice.h"
#include "ctrlm_database.h"
#ifdef AUTH_ENABLED
#include "ctrlm_auth.h"
#endif
#include "ctrlm_voice_packet_analysis.h"
#include "json_config.h"

void ctrlm_voice_device_update_in_progress_set(bool in_progress) {
   // This function was used to disable voice when foreground download is active
   //LOG_INFO("%s: Voice is <%s>\n", __FUNCTION__, in_progress ? "DISABLED" : "ENABLED");
}

void ctrlm_obj_network_rf4ce_t::ind_process_voice_session_stats(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_voice_session_stats_t *dqm = (ctrlm_main_queue_msg_voice_session_stats_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_stats_t));

   ctrlm_controller_id_t controller_id = dqm->controller_id;

   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Controller object doesn't exist for controller id %u!\n", __FUNCTION__, controller_id);
      return;
   }

   ctrlm_voice_t *obj = ctrlm_get_voice_obj();
   if(NULL == obj) {
      LOG_INFO("%s: Invalid session\n", __FUNCTION__);
      return;
   }

   obj->voice_session_stats(dqm->session, dqm->reboot);
}

void ctrlm_voice_notify_stats_reboot(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_voice_reset_type_t type, unsigned char voltage, unsigned char battery_percentage) {
   LOG_WARN("%s: (%u, %u) Reset Type %u voltage %u percentage %u\n", __FUNCTION__, network_id, controller_id, type, voltage, battery_percentage);

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_session_stats_t msg = {0};

   msg.controller_id             = controller_id;
   msg.session.available         = 0;
   msg.reboot.available          = 1;
   msg.reboot.reset_type         = type;
   msg.reboot.voltage            = voltage;
   msg.reboot.battery_percentage = battery_percentage;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::ind_process_voice_session_stats, &msg, sizeof(msg), NULL, network_id);
}

void ctrlm_voice_notify_stats_session(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, guint16 total_packets, guint16 drop_retry, guint16 drop_buffer, guint16 mac_retries, guint16 network_retries, guint16 cca_sense) {
   LOG_WARN("%s: (%u, %u)\n", __FUNCTION__, network_id, controller_id);

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_session_stats_t msg = {0};

   msg.controller_id          = controller_id;
   msg.reboot.available       = 0;
   msg.session.available      = 1;
   msg.session.packets_total  = total_packets;
   msg.session.dropped_retry  = drop_retry;
   msg.session.dropped_buffer = drop_buffer;
   msg.session.retry_mac      = mac_retries;
   msg.session.retry_network  = network_retries;
   msg.session.cca_sense      = cca_sense;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::ind_process_voice_session_stats, &msg, sizeof(msg), NULL, network_id);
}
