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
#include "jansson.h"

#define CTRLM_VOICE_MSO_USER_STRING_MAX_LENGTH (10)
#define CTRLM_VOICE_BUFFER_LEVEL_DEBUG

#define CTRLM_VOICE_LANGUAGE                 "eng"

#ifndef USE_VOICE_SDK
#define VOICE_SESSION_REQ_DATA_LEN_MAX (33)
#endif

typedef enum {
   VOICE_SESSION_TYPE_STANDARD  = 0x00,
   VOICE_SESSION_TYPE_SIGNALING = 0x01,
   VOICE_SESSION_TYPE_FAR_FIELD = 0x02,
} voice_session_type_t;

typedef enum {
   VOICE_SESSION_AUDIO_FORMAT_ADPCM_16K      = 0x00,
   VOICE_SESSION_AUDIO_FORMAT_PCM_16K        = 0x01,
   VOICE_SESSION_AUDIO_FORMAT_OPUS_16K       = 0x02,
   VOICE_SESSION_AUDIO_FORMAT_ADPCM_SKY_16K  = 0x03,
   VOICE_SESSION_AUDIO_FORMAT_INVALID        = 0x04
} voice_session_audio_format_t;

#ifndef USE_VOICE_SDK
typedef enum {
   VOICE_SESSION_RESPONSE_AVAILABLE                 = 0x00,
   VOICE_SESSION_RESPONSE_BUSY                      = 0x01,
   VOICE_SESSION_RESPONSE_SERVER_NOT_READY          = 0x02,
   VOICE_SESSION_RESPONSE_UNSUPPORTED_AUDIO_FORMAT  = 0x03,
   VOICE_SESSION_RESPONSE_FAILURE                   = 0x04,
   VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK = 0x05
} voice_session_response_status_t;
#endif

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


typedef enum {
   VREX_VOICE_FEATURE_ENABLED  = 0,
   VREX_VOICE_FEATURE_DISABLED = 1
} vrex_voice_feature_status_t;

typedef enum {
   VREX_VOICE_SERVER_READY     = 0,
   VREX_VOICE_SERVER_NOT_READY = 1
} vrex_voice_server_status_t;

#ifndef USE_VOICE_SDK
typedef enum {
   VOICE_COMMAND_STATUS_PENDING = 0,
   VOICE_COMMAND_STATUS_TIMEOUT = 1,
   VOICE_COMMAND_STATUS_OFFLINE = 2,
   VOICE_COMMAND_STATUS_SUCCESS = 3,
   VOICE_COMMAND_STATUS_FAILURE = 4,
   VOICE_COMMAND_STATUS_NO_CMDS = 5
} voice_command_status_t;
#endif


typedef struct {
   std::string                 remote_name;
   std::string                 server_url_vrex;
   std::string                 server_url_vrex_src_ff;
   bool                        sat_enabled;
   std::string                 aspect_ratio;
   std::string                 guide_language;
   unsigned long               timeout_vrex_request;
   unsigned long               timeout_vrex_response;
   unsigned long               timeout_sat_request;
   unsigned long               timeout_packet_initial;
   unsigned long               timeout_packet_subsequent;
   unsigned long               timeout_stats;
   bool                        utterance_save;
   std::string                 utterance_path;
   unsigned long               utterance_duration_min;
   bool                        force_voice_settings;
   unsigned char               packet_loss_threshold;
   ctrlm_voice_query_strings_t query_strings;
} voice_session_prefs_t;

typedef struct {
   ctrlm_main_queue_msg_header_t header;
   char                         server_url_vrex[CTRLM_VOICE_SERVER_URL_MAX_LENGTH];    ///< The url for the vrex server (null terminated string)
   char                         aspect_ratio[CTRLM_VOICE_ASPECT_RATIO_MAX_LENGTH];     ///< The guide's aspect ratio [pass-thru] (null terminated string)
   char                         guide_language[CTRLM_VOICE_GUIDE_LANGUAGE_MAX_LENGTH]; ///< The guide's language [pass-thru] (null terminated string)
   ctrlm_voice_query_strings_t  query_strings;                                         ///< The query strings (name/value pairs)
   char                         server_url_vrex_src_ff[CTRLM_VOICE_SERVER_URL_MAX_LENGTH];    ///< The url for the fairfield vrex server (null terminated string)
} ctrlm_main_queue_msg_voice_settings_update_t;

#ifndef USE_VOICE_SDK
typedef struct {
   ctrlm_main_queue_msg_header_t      header;
   ctrlm_controller_id_t              controller_id;
   ctrlm_timestamp_t                  timestamp;
   voice_session_type_t               type;
   voice_session_audio_format_t       audio_format;
   unsigned char                      data_len;
   unsigned char                      data[VOICE_SESSION_REQ_DATA_LEN_MAX];
   voice_session_response_status_t    status;
   ctrlm_voice_session_abort_reason_t reason;
} ctrlm_main_queue_msg_voice_session_request_t;

typedef struct {
   ctrlm_controller_id_t         controller_id;
   voice_command_status_t  status;
} ctrlm_main_queue_msg_voice_command_status_t;
#endif

typedef struct {
   ctrlm_controller_id_t         controller_id;
   unsigned char                 mime_type[CTRLM_VOICE_MIME_MAX_LENGTH];
   unsigned char                 sub_type[CTRLM_VOICE_SUBTYPE_MAX_LENGTH];
   unsigned char                 language[CTRLM_VOICE_LANG_MAX_LENGTH];
   int                           voice_data_pipe_read;
   unsigned long                 session_id;
} ctrlm_main_queue_msg_voice_session_begin_t;

#ifndef USE_VOICE_SDK
typedef struct {
   ctrlm_controller_id_t            controller_id;
   ctrlm_voice_session_end_reason_t reason;
   unsigned long                    stop_data;
   unsigned char                    utterance_too_short;
   int                              voice_data_pipe_write;
   unsigned long                    buffer_high_watermark;
   unsigned long                    session_packets_total;
   unsigned long                    session_packets_lost;
   unsigned long                    session_rf_channel;
   unsigned char                    session_link_quality;
   unsigned long                    session_id;
} ctrlm_main_queue_msg_voice_session_end_t;

typedef struct {
   ctrlm_controller_id_t            controller_id;
   char                             transcription[CTRLM_VOICE_SESSION_TEXT_MAX_LENGTH];
   unsigned char                    success;
} ctrlm_main_queue_msg_voice_session_result_t;
#endif

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

void ctrlm_voice_init(json_t *json_obj_voice);
void ctrlm_voice_terminate(void);
bool ctrlm_voice_is_initialized();
void ctrlm_voice_notify_stats_reboot(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_voice_reset_type_t type, unsigned char voltage, unsigned char battery_percentage);
void ctrlm_voice_notify_stats_session(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, guint16 total_packets, guint16 drop_retry, guint16 drop_buffer, guint16 mac_retries, guint16 network_retries, guint16 cca_sense);

void ctrlm_voice_device_update_in_progress_set(bool in_progress);
void ctrlm_voice_minimum_utterance_duration_set(unsigned long duration);
void ctrlm_voice_query_strings_set(ctrlm_voice_query_strings_t query_strings);
#ifndef USE_VOICE_SDK
void ctrlm_voice_cmd_status_set(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, voice_command_status_t status);
bool ctrlm_voice_ind_voice_session_request(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_timestamp_t timestamp);
void ctrlm_voice_service_access_token_set(std::string service_access_token, time_t expiration);
#endif
ctrlm_hal_frequency_agility_t ctrlm_voice_ind_voice_session_end(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_voice_session_end_reason_t reason, guint8 key_code, bool force = false);
void ctrlm_voice_ind_voice_session_fragment(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, unsigned char command_id, unsigned long data_length, guchar *data, ctrlm_hal_rf4ce_data_read_t cb_data_read, void *cb_data_param, unsigned char lqi);


voice_session_prefs_t *ctrlm_voice_vrex_prefs_get();
std::string ctrlm_voice_stb_name_get();
std::string ctrlm_voice_receiver_id_get();
void ctrlm_voice_vrex_req_completed(bool invalidate_sat, std::string conversation_id);
gboolean is_audio_stream_in_progress(void);
unsigned long ctrlm_voice_session_id_get_next(void);
gboolean is_vrex_req_in_progress(void);
bool ctrlm_voice_is_service_access_token_valid(void);
std::string ctrlm_voice_service_access_token_string_get();
unsigned long ctrlm_voice_timeout_vrex_request_get();
bool set_voice_session_in_progress(bool set);
gboolean is_voice_session_in_progress();

#ifdef __cplusplus
}
#endif

#endif
