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
#ifndef _CTRLM_VOICE_H_
#define _CTRLM_VOICE_H_

#include "ctrlm.h"
#include "ctrlm_ipc_voice.h"
#include "ctrlm_voice_iarm.h"
#include <string>

typedef enum {
   VOICE_SESSION_TYPE_STANDARD  = 0x00,
   VOICE_SESSION_TYPE_SIGNALING = 0x01,
   VOICE_SESSION_TYPE_FAR_FIELD = 0x02,
} voice_session_type_t;

typedef enum {
   VOICE_SESSION_RESPONSE_STREAM_BUF_BEGIN     = 0x00,
   VOICE_SESSION_RESPONSE_STREAM_KEYWORD_BEGIN = 0x01,
   VOICE_SESSION_RESPONSE_STREAM_KEYWORD_END   = 0x02,
} voice_session_response_stream_t;

typedef enum {
   CRTLM_VOICE_REMOTE_VOICE_END_MIC_KEY_RELEASE      =  1,
   CRTLM_VOICE_REMOTE_VOICE_END_EOS_DETECTION        =  2,
   CRTLM_VOICE_REMOTE_VOICE_END_SECONDARY_KEY_PRESS  =  3,
   CRTLM_VOICE_REMOTE_VOICE_END_TIMEOUT_MAXIMUM      =  4,
   CRTLM_VOICE_REMOTE_VOICE_END_TIMEOUT_TARGET       =  5,
   CRTLM_VOICE_REMOTE_VOICE_END_OTHER_ERROR          =  6,
   CRTLM_VOICE_REMOTE_VOICE_END_MINIMUM_QOS          =  7,
   CRTLM_VOICE_REMOTE_VOICE_END_TIMEOUT_FIRST_PACKET =  8,
   CRTLM_VOICE_REMOTE_VOICE_END_TIMEOUT_INTERPACKET  =  9
} ctrlm_voice_remote_voice_end_reason_t;

typedef struct {
   ctrlm_controller_id_t         controller_id;
   ctrlm_voice_stats_session_t   session;
   ctrlm_voice_stats_reboot_t    reboot;
} ctrlm_main_queue_msg_voice_session_stats_t;

typedef enum {
   VOICE_COMMAND_VOICE_SESSION_TERMINATE_SPECIAL_KEY_PRESS = 0,
   VOICE_COMMAND_VOICE_SESSION_TERMINATE_UNKNOWN = 5
} ctrlm_voice_session_termination_reason_t;


typedef struct {
   ctrlm_main_queue_msg_header_t            header;
   ctrlm_controller_id_t                    controller_id;
   ctrlm_voice_session_termination_reason_t reason;
} ctrlm_main_queue_msg_terminate_voice_session_t;

#ifdef __cplusplus
extern "C"
{
#endif

void ctrlm_voice_notify_stats_reboot(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_voice_reset_type_t type, unsigned char voltage, unsigned char battery_percentage);
void ctrlm_voice_notify_stats_session(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, guint16 total_packets, guint16 drop_retry, guint16 drop_buffer, guint16 mac_retries, guint16 network_retries, guint16 cca_sense);
void ctrlm_voice_device_update_in_progress_set(bool in_progress);

#ifdef __cplusplus
}
#endif

#endif
