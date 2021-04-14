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
#ifndef _CTRLM_VOICE_IARM_H_
#define _CTRLM_VOICE_IARM_H_

#include "ctrlm.h"
#include "ctrlm_ipc_voice.h"
#include <string>


#ifdef __cplusplus
extern "C"
{
#endif

gboolean ctrlm_voice_iarm_init(void);
void     ctrlm_voice_iarm_event_session_begin(ctrlm_voice_iarm_event_session_begin_t *event);
void     ctrlm_voice_iarm_event_session_end(ctrlm_voice_iarm_event_session_end_t *event);
void     ctrlm_voice_iarm_event_session_result(ctrlm_voice_iarm_event_session_result_t *event);
void     ctrlm_voice_iarm_event_session_stats(ctrlm_voice_iarm_event_session_stats_t *event);
void     ctrlm_voice_iarm_event_session_abort(ctrlm_voice_iarm_event_session_abort_t *event);
void     ctrlm_voice_iarm_event_session_short(ctrlm_voice_iarm_event_session_short_t *event);
void     ctrlm_voice_iarm_event_media_service(ctrlm_voice_iarm_event_media_service_t *event);
void     ctrlm_voice_iarm_set_power_state_on(void);
void     ctrlm_voice_iarm_terminate(void);

#ifdef __cplusplus
}
#endif

#endif
