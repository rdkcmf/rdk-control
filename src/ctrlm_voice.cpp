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
