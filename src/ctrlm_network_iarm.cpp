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
#include "ctrlm_rcu.h"
#include "ctrlm_utils.h"

void ctrlm_network_iarm_event_controller_unbind(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_unbind_reason_t reason) {
   ctrlm_main_iarm_event_controller_unbind_t event;
   event.api_revision  = CTRLM_MAIN_IARM_BUS_API_REVISION;
   event.network_id    = network_id;
   event.network_type  = ctrlm_network_type_get(network_id);
   event.controller_id = controller_id;
   event.reason        = reason;
   LOG_INFO("%s: (%u, %u) Reason <%s>\n", __FUNCTION__, network_id, controller_id, ctrlm_unbind_reason_str(reason));
   IARM_Result_t result = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_MAIN_IARM_EVENT_CONTROLLER_UNBIND, &event, sizeof(event));
   if(IARM_RESULT_SUCCESS != result) {
      LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
   }
}
