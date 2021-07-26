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
#include <ctime>
#include <sstream>
#include <sys/socket.h>
#include <linux/un.h>
#include <unistd.h>
#include "libIBus.h"
#include "jansson.h"
#include "ctrlm.h"
#include "ctrlm_rcu.h"
#include "ctrlm_voice.h"
#include "ctrlm_voice_session.h"
#ifdef USE_VOICE_SDK
#include "ctrlm_voice_obj.h"
#endif
#include "ctrlm_ipc_key_codes.h"
#include "ctrlm_utils.h"

//#define STREAM_END_SYNCHRONOUS
#define TEMP_BUFFER_SIZE (20)
//#define CURL_INSECURE_MODE
//#define CURL_LONG_DELAY_TEST

#define SESSION_STATS_INVALID_VALUE         (~0)

#define ERR_MSG_VOICE_THREAD_CREATE        "Could not create thread to send voice data"
#define ERR_MSG_VOICE_CANCEL_ERROR         "Audio stream aborted due to error from remote"
#define ERR_MSG_VOICE_CANCEL_REMOTE        "Audio stream cancelled from remote"
#define ERR_MSG_VOICE_TIMEOUT_INTERPACKET  "Audio stream ended due to interpacket timeout"
#define ERR_MSG_VOICE_TIMEOUT_MAXIMUM      "Audio stream ended due to maximum utterance length"
#define ERR_MSG_VOICE_COMM_LOST            "Audio stream ended due to communication lost"
#define ERR_MSG_VOICE_ADJACENT_KEY_PRESSED "Audio stream ended due to adjacent key press"
#define ERR_MSG_VOICE_OTHER_KEY_PRESSED    "Audio stream ended due to other key press"
#define ERR_MSG_VOICE_ERROR_UNKNOWN        "Audio stream ended due to unknown error"
#define ERR_MSG_VOICE_REMOTE_REBOOTED      "Audio stream ended due to remote reboot"

// This is the first XR11 firmware revision that supports statistics
#define STATS_VER_MAJOR    (  0)
#define STATS_VER_MINOR    (  1)
#define STATS_VER_REVISION ( 14)
#define STATS_VER_PATCH    (  8)
#define STATS_VERSION      ((STATS_VER_MAJOR << 24) | (STATS_VER_MINOR << 16) | (STATS_VER_REVISION << 8) | STATS_VER_PATCH)

typedef enum {
   // Client error codes
   VREX_SVR_RET_CODE_OK                      =    0, // No Error
   VREX_SVR_RET_CODE_CLIENT_CONNECT          =  101, // Client Can't connect to VREX
   VREX_SVR_RET_CODE_CLIENT_UPGRADE          =  102, // Client must upgrade to the latest version
   VREX_SVR_RET_CODE_INVALID_APP_ID          =  103, // The appId is not recognized
   VREX_SVR_RET_CODE_INVALID_CONVERSATION    =  104, // We could not create a conversation with you
   VREX_SVR_RET_CODE_INVALID_ID              =  105, // The custGuid or deviceId could not be recognized
   // SAT
   VREX_SVR_RET_CODE_SAT_SIGNATURE           =  108, // SAT Invalid Signature
   VREX_SVR_RET_CODE_SAT_MALFORMED           =  109, // SAT Malformed Token
   VREX_SVR_RET_CODE_SAT_PREMATURE           =  110, // SAT Premature Token
   VREX_SVR_RET_CODE_SAT_ERROR               =  111, // SAT ServiceAccess Error
   // Incoming parameters
   VREX_SVR_RET_CODE_AUDIO_EMPTY             =  201, // The audio file is empty
   VREX_SVR_RET_CODE_SPEECH_UNAVAILABLE_REX  =  202, // Speech interface not available, VSP received an invalid User ID from REX
   VREX_SVR_RET_CODE_INVALID_LOCATION        =  203, // The location parameter could not be parsed correctly
   VREX_SVR_RET_CODE_SPEECH_UNAVAILABLE_VSP  =  204, // Speech interface not available, VSP received an invalid HD Filter from REX
   VREX_SVR_RET_CODE_TIME_FILTER             =  205, // The time filter was not understood
   VREX_SVR_RET_CODE_INVALID_HEADER          =  206, // The header could not be read
   VREX_SVR_RET_CODE_STREAMING_HEADER        =  207, // The streaming header is incorrect
   VREX_SVR_RET_CODE_VALUES                  =  208, // The values passed in are incorrect
   VREX_SVR_RET_CODE_PARAMETER_LIST          =  209, // The parameter list is incorrect
   VREX_SVR_RET_CODE_GRAMMAR                 =  210, // The requested grammar is not supported
   VREX_SVR_RET_CODE_DETECTION               =  211, // The Speech interface could not detect what was said
   // Result
   VREX_SVR_RET_CODE_PROGRAM_NOT_PRESENT     =  301, // The request made by the client must contain either a station, channel, or program
   VREX_SVR_RET_CODE_SHOWINGS_15_MINS        =  302, // The request was found, but there are no showings in the next 15 minutes
   VREX_SVR_RET_CODE_SHOWINGS_2_HOURS        =  303, // The request was found, but there are no showings in the next two hours
   VREX_SVR_RET_CODE_PROGRAM_UNAVAILABLE     =  304, // The station, channel, or program is not available for this set-top box
   VREX_SVR_RET_CODE_TRENDING_LIST           =  305, // The trending list is not available
   // Internal Server error
   VREX_SVR_RET_CODE_UNAVAILABLE_ASR_OFFLINE =  401, // Speech interface not available, ASR offline
   VREX_SVR_RET_CODE_UNAVAILABLE_ASR_TIMEOUT =  402, // Speech interface not available, ASR timeout
   VREX_SVR_RET_CODE_UNAVAILABLE_NLP_OFFLINE =  403, // Speech interface not available, NLP offline
   VREX_SVR_RET_CODE_UNAVAILABLE_NLP_TIMEOUT =  404, // Speech interface not available, NLP timeout
   VREX_SVR_RET_CODE_UNAVAILABLE_AR_OFFLINE  =  405, // Speech interface not available, AR offline
   VREX_SVR_RET_CODE_UNAVAILABLE_AR_TIMEOUT  =  406, // Speech interface not available, AR timeout
   VREX_SVR_RET_CODE_REQUEST_RECEIVED        =  407, // Debug/Log Entry: Request Received (Request)
   VREX_SVR_RET_CODE_SPEECH_TO_TEXT          =  408, // Normal: Speech to Text Result
   VREX_SVR_RET_CODE_TOKENS_IDENTIFIED       =  409, // Debug/Log Tokens identified Result
   VREX_SVR_RET_CODE_REX_REQUEST             =  410, // Debug/Log Request as Sent to REX
   VREX_SVR_RET_CODE_REX_RESPONSE            =  411, // Debug/Log Entry: Response from REX
   VREX_SVR_RET_CODE_SESSION_TIMEOUT         =  412, // Debug/Log Entry: Session Timed Out
   // NLP Error
   VREX_SVR_RET_CODE_NLP_501                 =  501, // The text could not be recognized
   VREX_SVR_RET_CODE_NLP_502                 =  502, // The text could not be recognized
   VREX_SVR_RET_CODE_NLP_503                 =  503, // The text could not be recognized
   VREX_SVR_RET_CODE_NLP_504                 =  504, // The text could not be recognized
   VREX_SVR_RET_CODE_NLP_505                 =  505, // The text could not be recognized
   VREX_SVR_RET_CODE_NLP_OBJECT              =  506, // The NLP Response object is not available
   VREX_SVR_RET_CODE_NLP_ENTITY              =  507, // The EntityType of NLP response is not set
   // Channelmap error
   VREX_SVR_RET_CODE_STATION_ID              =  601, // No station Id could be retrieved for this location and channel number
   // REX errors Reserved: 1000-1099
   VREX_SVR_RET_CODE_NO_ERROR                = 1000, // No Error
   VREX_SVR_RET_CODE_INVALID_ENTRY           = 1001, // Invalid Entry into Rex Query Builder
   VREX_SVR_RET_CODE_INVALID_QUERY           = 1002, // Invalid query to REX, no results
   VREX_SVR_RET_CODE_CONNECT                 = 1003, // Cannot connect to REX
   VREX_SVR_RET_CODE_UNDERSTAND              = 1004  // I am Sorry. We did understand you, but are having a problem looking that up right now. (REX 0)
} vrex_server_return_codes_t;

static void *speech_server_thread(void *session){
   voice_session_t *p_vrex = (voice_session_t *)session;
   p_vrex->start_transfer_server();
   LOG_INFO("%s: Exiting speech thread\n", __FUNCTION__);
   return NULL;
}

static size_t vrex_server_voice_response(void *buffer, size_t size, size_t nmemb, void *user_data) {
   voice_session_t *p_vrex = (voice_session_t *)user_data;
   string str;
   str.append((const char *)buffer, size * nmemb);
   LOG_INFO("%s: <%d>\n", __FUNCTION__, size * nmemb);

   p_vrex->speech_response_add(str, size * nmemb);

   return size * nmemb;
}

static size_t vrex_server_voice_data_read(void *buffer, size_t size, size_t nmemb, void *user_data){
   voice_session_t *p_vrex = (voice_session_t *)user_data;
   return p_vrex->server_socket_callback(buffer, size, nmemb);
}

#ifdef PRINT_STATS_LAG_TIME
static unsigned long long time_diff(struct timespec *before, struct timespec *after) {
    if(after->tv_sec - before->tv_sec) {
        return(((after->tv_sec - before->tv_sec)*(unsigned long long)1000000000) - before->tv_nsec + after->tv_nsec);
    } else {
        return(after->tv_nsec - before->tv_nsec);
    }
}
#endif

static int vrex_response_timeout(void *data) {
   voice_session_t *session = (voice_session_t *)data;
   session->abort_vrex_request_set(true);
   session->vrex_response_timeout_reset();
   return false;
}

int vrex_request_progress(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
   voice_session_t *session = (voice_session_t *)clientp;
   if(session->abort_vrex_request_get() && session->voice_data_qty_sent_get() <= ulnow) {
      LOG_INFO("%s: Aborting CURL session due to vrex response timeout\n", __FUNCTION__);
      session->abort_vrex_request_set(false);
      session->vrex_response_timeout_reset();
      return 1;
   }
   else {
      return 0;
   }
}

voice_session_t::voice_session_t(const string& route, const string& aspect_ratio, const string&  language, const string& stb_name, unsigned long vrex_response_timeout, const ctrlm_voice_query_strings_t& query_strings, const string& app_id) :
   m_app_id(app_id),
//   m_receiver_id(receiver_id),
//   m_device_id(device_id),
   m_stb_name(stb_name),
//   m_conversation_id(""),
//   m_service_account_id(service_account_id),
//   m_partner_id(partner_id),
   m_remote_battery_value(0),
   m_remote_battery_percentage(0),
   m_base_route_contains_speech(false),
   m_curl_initialized(false),
   m_base_route(route),
   m_aspect_ratio(aspect_ratio),
   m_guide_language(language),
   m_network_id(CTRLM_HAL_NETWORK_ID_INVALID),
   m_controller_id(CTRLM_HAL_CONTROLLER_ID_INVALID),
   m_log_metrics(true),
   m_voice_data_pipe_read(-1),
   m_data_read_thread(NULL),
   m_voice_data_qty_sent(0),
   m_voice_rsp_qty_rxd(0),
   m_utterance_too_short(0),
   m_return_code_vrex(0),
   m_return_code_curl(CURLE_OK),
   m_return_code_http(0),
   m_session_id_vrex_mgr(0),
   m_notified_results(true),
   m_notified_stats(true),
   m_caught_exception(false),
   m_thread_failed(false),
   m_end_reason(CTRLM_VOICE_SESSION_END_REASON_MAX),
   m_timeout_vrex_request(0),
   m_abort_vrex_request(false),
   m_vrex_response_timeout_tag(0),
   m_vrex_response_timeout(vrex_response_timeout),
   m_do_not_close_read_pipe(false),
   m_terminate_speech_server_thread(false)
{
   errno_t safec_rc = -1;
   CURLcode res;
   LOG_INFO("voice_session_t constructor for controller %u, %s, %s\n", m_controller_id, m_receiver_id.c_str(), m_app_id.c_str());
   res = curl_global_init(CURL_GLOBAL_DEFAULT);

   if(res != CURLE_OK) {
      LOG_INFO("%s: Error - Can't init cURL: %s\n", __FUNCTION__, curl_easy_strerror(res));
   } else {
      m_curl_initialized = true;
   }

   if(m_base_route.find("speech?") != string::npos) {
      m_base_route_contains_speech = true;
   }

   pthread_cond_init(&m_cond, NULL);
   pthread_mutex_init(&m_mutex, NULL);

   user_agent_string_refresh();
   uuid_clear(m_uuid);
   safec_rc = memset_s(m_primary_ip, CTRLM_VOICE_REQUEST_IP_MAX_LENGTH, 0, CTRLM_VOICE_REQUEST_IP_MAX_LENGTH);
   ERR_CHK(safec_rc);
   m_lookup_time  = 0.0;
   m_connect_time = 0.0;

   safec_rc = memcpy_s(&m_query_strings, sizeof(ctrlm_voice_query_strings_t), &query_strings, sizeof(ctrlm_voice_query_strings_t));
   ERR_CHK(safec_rc);
   safec_rc  = memset_s(&m_stats_session, sizeof(m_stats_session), 0, sizeof(m_stats_session));
   ERR_CHK(safec_rc);
   safec_rc = memset_s(&m_stats_reboot, sizeof(m_stats_reboot), 0, sizeof(m_stats_reboot));
   ERR_CHK(safec_rc);
}

voice_session_t::~voice_session_t() {
    curl_global_cleanup();
}

void voice_session_t::server_details_update(const string& server_url_vrex, const string& aspect_ratio, const string& guide_language, const ctrlm_voice_query_strings_t& query_strings, const string& server_url_vrex_src_ff) {
   m_base_route = server_url_vrex;

   if(m_base_route.find("speech?") != string::npos) {
      m_base_route_contains_speech = true;
   } else {
      m_base_route_contains_speech = false;
   }
   LOG_INFO("%s: New base route: <%s>\n", __FUNCTION__, m_base_route.c_str());

   m_aspect_ratio           = aspect_ratio;
   m_guide_language         = guide_language;
   m_server_url_vrex_src_ff = server_url_vrex_src_ff;
   errno_t safec_rc = memcpy_s(&m_query_strings,  sizeof(ctrlm_voice_query_strings_t), &query_strings, sizeof(ctrlm_voice_query_strings_t));
   ERR_CHK(safec_rc);
}

void voice_session_t::notify_result_success() {
   errno_t safec_rc = -1;
   ctrlm_voice_iarm_event_session_result_t result = {0};

   result.api_revision              = CTRLM_VOICE_IARM_BUS_API_REVISION;
   result.network_id                = m_network_id;
   result.network_type              = ctrlm_network_type_get(m_network_id);
   result.controller_id             = m_controller_id;
   result.session_id                = m_session_id_vrex_mgr;
   result.result                    = CTRLM_VOICE_SESSION_RESULT_SUCCESS;
   result.return_code_http          = m_return_code_http;
   result.return_code_curl          = m_return_code_curl;
   result.return_code_vrex          = m_return_code_vrex;
   result.return_code_internal      = CTRLM_VOICE_INTERNAL_ERROR_NONE;
   result.curl_request_dns_time     = m_lookup_time;
   result.curl_request_connect_time = m_connect_time;

   safec_rc = strncpy_s(result.vrex_session_id, sizeof(result.vrex_session_id), m_session_id_vrex.c_str(), CTRLM_VOICE_SESSION_ID_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   result.vrex_session_id[CTRLM_VOICE_SESSION_ID_MAX_LENGTH - 1] = '\0';

   safec_rc = strncpy_s(result.vrex_session_text, sizeof(result.vrex_session_text), m_session_text_vrex.c_str(), CTRLM_VOICE_SESSION_TEXT_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   result.vrex_session_text[CTRLM_VOICE_SESSION_TEXT_MAX_LENGTH - 1] = '\0';

   safec_rc = strncpy_s(result.vrex_session_message, sizeof(result.vrex_session_message), m_session_message_vrex.c_str(), CTRLM_VOICE_SESSION_MSG_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   result.vrex_session_message[CTRLM_VOICE_SESSION_MSG_MAX_LENGTH - 1] = '\0';

   safec_rc = strncpy_s(result.curl_request_ip, sizeof(result.curl_request_ip), m_primary_ip, CTRLM_VOICE_REQUEST_IP_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   result.curl_request_ip[CTRLM_VOICE_REQUEST_IP_MAX_LENGTH - 1] = '\0';

   safec_rc = strncpy_s(result.session_uuid, sizeof(result.session_uuid), m_uuid_str, CTRLM_VOICE_SESSION_ID_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   result.session_uuid[CTRLM_VOICE_SESSION_ID_MAX_LENGTH - 1] = '\0';

   // Broadcast event to any listeners
   ctrlm_voice_iarm_event_session_result(&result);
}

void voice_session_t::notify_result_error() {
   errno_t safec_rc = -1;
   ctrlm_voice_iarm_event_session_result_t result = {0};

   result.api_revision             = CTRLM_VOICE_IARM_BUS_API_REVISION;
   result.network_id               = m_network_id;
   result.network_type             = ctrlm_network_type_get(m_network_id);
   result.controller_id            = m_controller_id;
   result.session_id               = m_session_id_vrex_mgr;
   result.result                   = CTRLM_VOICE_SESSION_RESULT_FAILURE;
   result.return_code_http         = m_return_code_http;
   result.return_code_curl         = m_return_code_curl;
   result.return_code_vrex         = m_return_code_vrex;
   if(m_thread_failed) {
      result.return_code_internal  = CTRLM_VOICE_INTERNAL_ERROR_THREAD_CREATE;
   } else if(m_caught_exception) {
      result.return_code_internal  = CTRLM_VOICE_INTERNAL_ERROR_EXCEPTION;
   } else {
      result.return_code_internal  = CTRLM_VOICE_INTERNAL_ERROR_NONE;
   }
   result.curl_request_dns_time     = m_lookup_time;
   result.curl_request_connect_time = m_connect_time;

   safec_rc = strncpy_s(result.vrex_session_id, sizeof(result.vrex_session_id), m_session_id_vrex.c_str(), CTRLM_VOICE_SESSION_ID_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   result.vrex_session_id[CTRLM_VOICE_SESSION_ID_MAX_LENGTH - 1] = '\0';

   safec_rc = strncpy_s(result.vrex_session_text, sizeof(result.vrex_session_text), m_session_text_vrex.c_str(), CTRLM_VOICE_SESSION_TEXT_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   result.vrex_session_text[CTRLM_VOICE_SESSION_TEXT_MAX_LENGTH - 1] = '\0';

   safec_rc = strncpy_s(result.vrex_session_message, sizeof(result.vrex_session_message), m_session_message_vrex.c_str(), CTRLM_VOICE_SESSION_MSG_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   result.vrex_session_message[CTRLM_VOICE_SESSION_MSG_MAX_LENGTH - 1] = '\0';

   safec_rc = strncpy_s(result.curl_request_ip, sizeof(result.curl_request_ip), m_primary_ip, CTRLM_VOICE_REQUEST_IP_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   result.curl_request_ip[CTRLM_VOICE_REQUEST_IP_MAX_LENGTH - 1] = '\0';

   safec_rc = strncpy_s(result.session_uuid, sizeof(result.session_uuid), m_uuid_str, CTRLM_VOICE_SESSION_ID_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   result.session_uuid[CTRLM_VOICE_SESSION_ID_MAX_LENGTH - 1] = '\0';

   // Broadcast event to any listeners
   ctrlm_voice_iarm_event_session_result(&result);
}

void voice_session_t::notify_result_short() {
   ctrlm_voice_iarm_event_session_short_t event = {0};

   event.api_revision          = CTRLM_VOICE_IARM_BUS_API_REVISION;
   event.network_id            = m_network_id;
   event.network_type          = ctrlm_network_type_get(m_network_id);
   event.controller_id         = m_controller_id;
   event.session_id            = m_session_id_vrex_mgr;
   event.return_code_internal  = CTRLM_VOICE_INTERNAL_ERROR_NONE;
   event.reason                = m_end_reason;

   // Broadcast event to any listeners
   ctrlm_voice_iarm_event_session_short(&event);

   // Set stats flag to allow stats to be sent for the short utterance
   m_notified_stats = false;
}

void voice_session_t::notify_stats() {
   if(m_notified_stats) {
      LOG_INFO("%s: already sent, ignoring.\n", __FUNCTION__);
      return;
   }
   ctrlm_voice_iarm_event_session_stats_t event;

   event.api_revision              = CTRLM_VOICE_IARM_BUS_API_REVISION;
   event.network_id                = m_network_id;
   event.network_type              = ctrlm_network_type_get(m_network_id);
   event.controller_id             = m_controller_id;
   event.session_id                = m_session_id_vrex_mgr;

   event.session.available         = m_stats_session.available;
   event.session.rf_channel        = m_stats_session.rf_channel;
   event.session.buffer_watermark  = m_stats_session.buffer_watermark;
   event.session.packets_total     = m_stats_session.packets_total;
   event.session.packets_lost      = m_stats_session.packets_lost;
   event.session.dropped_retry     = m_stats_session.dropped_retry;
   event.session.dropped_buffer    = m_stats_session.dropped_buffer;
   event.session.retry_mac         = m_stats_session.retry_mac;
   event.session.retry_network     = m_stats_session.retry_network;
   event.session.cca_sense         = m_stats_session.cca_sense;
   event.session.link_quality      = m_stats_session.link_quality;

   event.reboot.available          = m_stats_reboot.available;
   event.reboot.reset_type         = m_stats_reboot.reset_type;
   event.reboot.voltage            = m_stats_reboot.voltage;
   event.reboot.battery_percentage = m_stats_reboot.battery_percentage;

   // Broadcast event to any listeners
   ctrlm_voice_iarm_event_session_stats(&event);

   m_notified_stats = true;
}

void voice_session_t::speech_response_add(string response_data, unsigned long len) {
   m_voice_response    += response_data;
   m_voice_rsp_qty_rxd += len;
}

void voice_session_t::stream_request(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) {
   m_network_id = network_id;
   m_controller_id = controller_id;
}

bool voice_session_t::stream_begin(const stream_begin_params_t& params) {
   GError *err = NULL;
   m_audio_info = params.audio_info;

   pthread_mutex_lock(&m_mutex);

   // Initialize session variables
   m_conversation_id      = params.conversation_id;
   m_session_id_vrex_mgr  = params.session_id;
   if(!params.service_access_token.empty()) {
      m_service_access_token = "Authorization: Bearer " + params.service_access_token;
   } else {
      m_service_access_token.clear();
   }
   m_voice_response.clear();
   m_session_id_vrex.clear();
   m_session_text_vrex.clear();
   m_session_message_vrex.clear();
   m_voice_data_qty_sent  = 0;
   m_voice_rsp_qty_rxd    = 0;
   if (m_voice_data_pipe_read != -1) {
      close(m_voice_data_pipe_read);
      m_do_not_close_read_pipe = true;
   }

   m_voice_data_pipe_read = params.voice_data_pipe_read;

   m_return_code_vrex        = 0;
   m_return_code_curl        = CURLE_OK;
   m_return_code_http        = 200;
   m_utterance_too_short     = 0;
   m_notified_results        = false;
   m_notified_stats          = false;
   m_caught_exception        = false;
   m_thread_failed           = false;
   m_end_reason              = CTRLM_VOICE_SESSION_END_REASON_DONE;
   m_stats_session.available = true;
   m_stats_reboot.available  = false;
   m_timeout_vrex_request    = params.timeout_vrex_request;
   m_network_id              = params.network_id;
   m_controller_id           = params.controller_id;

   uuid_clear(m_uuid);

   remote_hardware_version_set(params.hw_version.manufacturer, params.hw_version.model, params.hw_version.hw_revision, params.hw_version.lot_code);
   remote_software_version_set(params.sw_version.major, params.sw_version.minor, params.sw_version.revision, params.sw_version.patch);
   remote_battery_voltage_set(params.battery_voltage, params.battery_percentage);
   remote_user_string_set(params.user_string.c_str());

   #ifndef STREAM_END_SYNCHRONOUS
   if(NULL != m_data_read_thread) {
      // Call thread join on thread that already ended.. This ensures that the resources are freed..
      g_thread_join(m_data_read_thread);
   }
   #endif
   
   m_terminate_speech_server_thread = false;

   m_data_read_thread = g_thread_try_new("ctrlm_read_thread", speech_server_thread, (void *)this, &err);
   if(NULL != err) {
      pthread_mutex_unlock(&m_mutex);
      LOG_INFO("%s: Error - %s", __FUNCTION__, ERR_MSG_VOICE_THREAD_CREATE);
      LOG_ERROR("%s: failed error code %d, %s\n", __FUNCTION__, err->code, err->message);
      m_session_message_vrex = ERR_MSG_VOICE_THREAD_CREATE;
      m_thread_failed = true;
      if(err) {
         g_error_free(err);
      }
      return false;
   }
   LOG_INFO("%s: Thread blocked waiting for initialization\n", __FUNCTION__);
   pthread_cond_wait(&m_cond, &m_mutex);
   pthread_mutex_unlock(&m_mutex);
   LOG_INFO("%s: returned\n", __FUNCTION__);
   return true;
}

void voice_session_t::stream_end(ctrlm_voice_session_end_reason_t reason, unsigned long stop_data, int voice_data_pipe_write, unsigned long rf_channel, unsigned long buffer_watermark, unsigned long packets_total, unsigned long packets_lost, unsigned char link_quality, unsigned long session_id) {

   m_terminate_speech_server_thread = (reason == CTRLM_VOICE_SESSION_END_REASON_NEW_SESSION);
   if(voice_data_pipe_write >= 0) {
      pthread_mutex_lock(&m_mutex);
      LOG_INFO("%s: Closing write side of pipe\n", __FUNCTION__);
      close(voice_data_pipe_write);
      pthread_mutex_unlock(&m_mutex);

      #ifdef STREAM_END_SYNCHRONOUS
      LOG_INFO("%s: Waiting for server thread to exit\n", __FUNCTION__);
      g_thread_join(m_data_read_thread);
      LOG_INFO("%s: Server thread exited. Closing read side of pipe\n", __FUNCTION__);
      close(m_voice_data_pipe_read);
      m_voice_data_pipe_read = -1;
      #else
      LOG_INFO("%s: NOT waiting for server thread to exit\n", __FUNCTION__);
      #endif
   } else {
      m_utterance_too_short = 1;
      m_session_id_vrex_mgr = session_id;  // Use the provided session id for short utterances because stream_begin was not called.
      m_conversation_id.clear();
   }

   m_end_reason                  = reason;
   m_stats_session.rf_channel       = rf_channel;
   m_stats_session.buffer_watermark = buffer_watermark;
   m_stats_session.packets_lost     = packets_lost;
   m_stats_session.packets_total    = packets_total; // Use target's packet total (will be overwritten by remote's value when available)
   m_stats_session.link_quality     = link_quality;

   // Set the error code based on the reason
   //switch(reason) {
   //   case CTRLM_VOICE_SESSION_END_REASON_DONE:                   break;
   //   case CTRLM_VOICE_SESSION_END_REASON_ABORT:                  m_session_message_vrex = ERR_MSG_VOICE_CANCEL_REMOTE;          break;
   //   case CTRLM_VOICE_SESSION_END_REASON_ERROR:                  m_session_message_vrex = ERR_MSG_VOICE_CANCEL_ERROR;           break;
   //   case CTRLM_VOICE_SESSION_END_REASON_TIMEOUT_INTERPACKET:    m_session_message_vrex = ERR_MSG_VOICE_TIMEOUT_INTERPACKET;    break;
   //   case CTRLM_VOICE_SESSION_END_REASON_TIMEOUT_MAXIMUM:        m_session_message_vrex = ERR_MSG_VOICE_TIMEOUT_MAXIMUM;        break;
   //   case CTRLM_VOICE_SESSION_END_REASON_COMM_LOST:              m_session_message_vrex = ERR_MSG_VOICE_COMM_LOST;              break;
   //   case CTRLM_VOICE_SESSION_END_REASON_ADJACENT_KEY_PRESSED:   m_session_message_vrex = ERR_MSG_VOICE_ADJACENT_KEY_PRESSED;   break;
   //   case CTRLM_VOICE_SESSION_END_REASON_OTHER_KEY_PRESSED:      m_session_message_vrex = ERR_MSG_VOICE_OTHER_KEY_PRESSED;      break;
   //   case CTRLM_VOICE_SESSION_END_REASON_ABORT_BUSY:             m_session_message_vrex = ERR_MSG_VOICE_ABORT_BUSY;             break;
   //   case CTRLM_VOICE_SESSION_END_REASON_ABORT_SERVER_NOT_READY: m_session_message_vrex = ERR_MSG_VOICE_ABORT_SERVER_NOT_READY; break;
   //   case CTRLM_VOICE_SESSION_END_REASON_ABORT_AUDIO_FORMAT:     m_session_message_vrex = ERR_MSG_VOICE_ABORT_AUDIO_FORMAT;     break;
   //   case CTRLM_VOICE_SESSION_END_REASON_ABORT_FAILURE:          m_session_message_vrex = ERR_MSG_VOICE_ABORT_FAILURE;          break;
   //   case CTRLM_VOICE_SESSION_END_REASON_ABORT_VOICE_DISABLED:   m_session_message_vrex = ERR_MSG_VOICE_ABORT_VOICE_DISABLED;   break;
   //   case CTRLM_VOICE_SESSION_END_REASON_ABORT_PRIVACY_OPT_OUT:  m_session_message_vrex = ERR_MSG_VOICE_ABORT_PRIVACY_OPT_OUT;  break;
   //   default:                                        m_session_message_vrex = ERR_MSG_VOICE_ERROR_UNKNOWN;          break;
   //}

   #ifdef PRINT_STATS_LAG_TIME
   clock_gettime(CLOCK_REALTIME, &m_session_end_time);
   #endif
}

bool voice_session_t::wait_for_statistics() {
   return(true); // Always returns some stats or reboot detection
}

void voice_session_t::stream_stats(ctrlm_voice_stats_session_t session, ctrlm_voice_stats_reboot_t reboot) {
   m_stats_reboot  = reboot;

   // Add or clear the statistics to the session stats
   if(!session.available) {
      // Clear out entries in case they contained values
      m_stats_session.dropped_retry  = SESSION_STATS_INVALID_VALUE;
      m_stats_session.dropped_buffer = SESSION_STATS_INVALID_VALUE;
      m_stats_session.retry_mac      = SESSION_STATS_INVALID_VALUE;
      m_stats_session.retry_network  = SESSION_STATS_INVALID_VALUE;
      m_stats_session.cca_sense      = SESSION_STATS_INVALID_VALUE;
   } else {
      m_stats_session.packets_total  = session.packets_total;
      m_stats_session.dropped_retry  = session.dropped_retry;
      m_stats_session.dropped_buffer = session.dropped_buffer;
      m_stats_session.retry_mac      = session.retry_mac;
      m_stats_session.retry_network  = session.retry_network;
      m_stats_session.cca_sense      = session.cca_sense;
   }

   if(reboot.available && reboot.voltage == 0xFF) { // Voltage was not reported (old remote firmware).  Use locally stored value.
      m_stats_reboot.voltage            = m_remote_battery_value;
      m_stats_reboot.battery_percentage = m_remote_battery_percentage;
   }

   #ifdef PRINT_STATS_LAG_TIME
   struct timespec session_stats_time;
   clock_gettime(CLOCK_REALTIME, &session_stats_time);
   unsigned long long delta_time;
   delta_time = time_diff(&m_session_end_time, &session_stats_time);
   LOG_INFO("%s: Stream end to stats lag time <%llu ms>\n", __FUNCTION__, delta_time / 1000000);
   #endif

   // Send the notification
   notify_stats();
}

void voice_session_t::notify_result() {
   if(m_utterance_too_short) {
      LOG_INFO("%s: Utterance too short\n", __FUNCTION__);
      notify_result_short();
      return;
   }
   if(!m_notified_results) {
      if(m_end_reason != CTRLM_VOICE_SESSION_END_REASON_DONE || m_return_code_curl != CURLE_OK || m_caught_exception || m_thread_failed || (m_return_code_http != 200) || (m_return_code_vrex != 0)) {
         LOG_INFO("%s: Error\n", __FUNCTION__);
         notify_result_error();
      } else {
         LOG_INFO("%s: Success\n", __FUNCTION__);
         notify_result_success();
      }
      m_notified_results = true;
   }
}
size_t voice_session_t::server_socket_callback(void *buffer, size_t size, size_t nmemb) {
   size_t num_bytes;

   if(size * nmemb < 1) {
      LOG_INFO("%s: ERROR - read callback with zero buffer size!!\n", __FUNCTION__);
      return 0;
   }

   //pthread_mutex_lock(&m_mutex);

   num_bytes = read(m_voice_data_pipe_read, buffer, size * nmemb);

   if(num_bytes == 0) {
      LOG_INFO("%s: End of voice transmission\n", __FUNCTION__);
      m_vrex_response_timeout_tag = ctrlm_timeout_create(m_vrex_response_timeout, vrex_response_timeout, this);
   } else {
      //LOG_INFO("%s: voice data req <%i> read <%i> bytes\n", __FUNCTION__, size * nmemb, num_bytes);
      m_voice_data_qty_sent += num_bytes;
      #ifdef CTRLM_VOICE_BUFFER_LEVEL_DEBUG
      LOG_INFO("-<%5d,%5lu>\n", num_bytes, m_voice_data_qty_sent);
      #endif
   }
   //pthread_mutex_unlock(&m_mutex);

   return num_bytes;
}

void voice_session_t::start_transfer_server() {
   LOG_INFO("%s: Enter\n", __FUNCTION__);

   usleep(5 * 1000);  // simple wait to make sure streamStart thread is waiting before broadcast;
   LOG_INFO("%s: Unblocking waiting thread\n", __FUNCTION__);
   // Unblock parent thread
   pthread_mutex_lock(&m_mutex);
   pthread_cond_broadcast(&m_cond);
   pthread_mutex_unlock(&m_mutex);

   LOG_INFO("%s: Thread unblocked\n", __FUNCTION__);

   CURL *curl;
   //CURLcode res; TODO check all curl return codes
   struct curl_slist *chunk = NULL;
   LOG_INFO("%s: Connection accepted\n", __FUNCTION__);

   curl = curl_easy_init();

   if(curl) {
      string url = m_base_route;
      if(!m_base_route_contains_speech) {
         url += "speech?";
      }
      url += "appId=" + m_app_id + "&codec=" + m_audio_info.sub_type;
      if("" != m_device_id) {
        url += "&xboDeviceId=" + m_device_id;
      } else {
        url += "&receiverId=" + m_receiver_id;
      }

      if(!m_conversation_id.empty())    { url += "&cid=" + m_conversation_id;                 }
#if 0 //Temporarily remove service account id until authservice returns unencrypted
      if(!m_service_account_id.empty()) { url += "&serviceAccountId=" + m_service_account_id; }
#endif
      if(!m_partner_id.empty())         { url += "&partnerId=" + m_partner_id;                }
      if(!m_experience.empty())         { url += "&experienceTag=" + m_experience;            }
      if(!m_guide_language.empty())     { url += "&language=" + m_guide_language;             }
      if(!m_aspect_ratio.empty())       { url += "&aspectRatio=" + m_aspect_ratio;            }
      uuid_generate(m_uuid);
      uuid_unparse_lower(m_uuid, m_uuid_str);
      if(!uuid_is_null(m_uuid))         { url += "&trx="; url += m_uuid_str;                    }

      for(int query=0; query<m_query_strings.pair_count; query++) {
         url += "&" + string(m_query_strings.query_string[query].name) + "=" + string(m_query_strings.query_string[query].value);
      }

      printf("%s: SPEECH URL <%s>\n", __FUNCTION__, url.c_str());

      //#define ESCAPE_THE_URL
      #ifdef ESCAPE_THE_URL
      char *url_escape = curl_easy_escape(curl, url.c_str(), 0);

      if(NULL == url_escape) {
         LOG_INFO("%s: Unable to escape URL!\n", __FUNCTION__);
      }
      curl_easy_setopt(curl, CURLOPT_URL, url_escape);
      #else
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      #endif
      //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_READFUNCTION, vrex_server_voice_data_read);
      curl_easy_setopt(curl, CURLOPT_READDATA, this);  // pointer to pass to our read function
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, vrex_server_voice_response);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, this); // pointer to pass to our write function
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout_vrex_request);
      curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
      curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, vrex_request_progress);
      curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
      curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

      //#ifdef CURL_INSECURE_MODE
      //#error Curl is set to insecure mode.  Never set this in production code.
      //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      //#endif

      chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
      chunk = curl_slist_append(chunk, "Content-Type:application/octet-stream");
      if(!m_service_access_token.empty()) {
         LOG_INFO("%s: SAT Header sent to VREX.\n", __FUNCTION__);
         // After obtaining a SAT, an application sets it as 'Authorization' header before making a http API call.
         chunk = curl_slist_append(chunk, m_service_access_token.c_str());
      }
      chunk = curl_slist_append(chunk, "Expect:");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

      //Add the user agent field
      LOG_INFO("Speech USERAGENT: %s\n", m_user_agent.c_str());
      curl_easy_setopt(curl, CURLOPT_USERAGENT, m_user_agent.c_str());

      #ifdef CURL_LONG_DELAY_TEST
      #warning CURL LONG DELAY TEST CODE IS ENABLED!
      LOG_INFO("LONG DELAY START\n");
      sleep(8);
      LOG_INFO("LONG DELAY END\n");
      #endif


      CURLM *multi_handle = curl_multi_init();
      curl_multi_add_handle(multi_handle, curl);

      // Perform the request, res will get the return code

      int running_handles = 0;
      for (;;) {
         curl_multi_perform(multi_handle, &running_handles);
         if (running_handles == 0 || m_terminate_speech_server_thread) {
            if (m_terminate_speech_server_thread) {
               LOG_INFO("%s : Terminate curl transfer\n", __FUNCTION__);
            }
            break;
         } else {
            usleep(100000);
         }
      }

      LOG_INFO("%s: Voice Data Sent <%lu>\n", __FUNCTION__, m_voice_data_qty_sent);

      // Destroy the response timeout
      ctrlm_timeout_destroy(&m_vrex_response_timeout_tag);

      curl_slist_free_all(chunk);
      #ifdef ESCAPE_THE_URL
      if(url_escape) {
         curl_free(url_escape);
      }
      #endif

      if (m_terminate_speech_server_thread) {
         LOG_INFO("%s : Exit thread, do not wait for session result\n", __FUNCTION__);
         curl_multi_cleanup(multi_handle);
         curl_easy_cleanup(curl);
         close(m_voice_data_pipe_read);
         m_voice_data_pipe_read = -1;
         m_do_not_close_read_pipe = false;
         return;
      }

      //Get curl params needed for ctrlm_voice_iarm_event_session_result_t
      curl_info_for_session_result_get(curl);

      // Check for errors
      if(m_return_code_curl != CURLE_OK) {
         std::stringstream sstm;
         sstm << "cURL call failed trx <" << m_uuid_str << ">, code (" << m_return_code_curl << "): " << curl_easy_strerror(m_return_code_curl);
         m_session_message_vrex = sstm.str();
         LOG_INFO("%s: CURL ERROR %s\n", __FUNCTION__, m_session_message_vrex.c_str());
         curl_info_print(curl);
         if(m_return_code_curl == CURLE_ABORTED_BY_CALLBACK) {
            // Simulate a valid vrex response since we do not care about it
            LOG_INFO("%s: Connection with vrex aborted since we do not care about response\n", __FUNCTION__);
            m_return_code_curl = CURLE_OK;
            m_caught_exception = false;
            m_return_code_http = 200;
            m_return_code_vrex = 0;
         }
      } else {
         try {
            LOG_INFO("%s: CURL OK\n", __FUNCTION__);

            // Get HTTP response code
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &m_return_code_http);

            if(m_return_code_http != 200) {
               LOG_INFO("%s: HTTP Error %ld\n", __FUNCTION__, m_return_code_http);
               if(strlen(m_voice_response.c_str()) > 0) {
                  printf("%s: JSON <%s>\n", __FUNCTION__, m_voice_response.c_str());
               }
            } else {
               LOG_INFO("%s: HTTP OK\n", __FUNCTION__);
               printf("%s: JSON <%s>\n", __FUNCTION__, m_voice_response.c_str());

               if(m_voice_rsp_qty_rxd != strlen(m_voice_response.c_str())) {
                  LOG_INFO("%s: JSON incomplete response received <%lu> Len <%d>\n", __FUNCTION__, m_voice_rsp_qty_rxd, strlen(m_voice_response.c_str()));
               }
               json_error_t json_error;
               json_t *json_obj_root = json_loads(m_voice_response.c_str(), JSON_REJECT_DUPLICATES, &json_error);

               if(json_obj_root == NULL) {
                  LOG_ERROR("%s: JSON decoding error at line %u column %u <%s>\n", __FUNCTION__, json_error.line, json_error.column, json_error.text);
               } else if(!json_is_object(json_obj_root)) {
                  LOG_ERROR("%s: JSON decoding error not an object\n", __FUNCTION__);
               } else {
                  json_t *json_obj = json_object_get(json_obj_root, "cid");
                  if(json_obj != NULL && json_is_string(json_obj)) {
                     m_conversation_id = json_string_value(json_obj);
                  } else {
                     m_conversation_id = "";
                  }

                  // Process root values
                  json_obj = json_object_get(json_obj_root, "message");
                  if(json_obj != NULL && json_is_string(json_obj)) {
                     LOG_INFO("%s: onSpeech message: <%s>\n", __FUNCTION__, json_string_value(json_obj));
                     m_session_message_vrex = json_string_value(json_obj);
                  }

                  json_obj = json_object_get(json_obj_root, "code");
                  if(json_obj != NULL && json_is_integer(json_obj)) { // Got a speech response with a server status code (not necessarily an error)
                     LOG_INFO("%s: onSpeech code: %lld\n", __FUNCTION__, json_integer_value(json_obj));
                     m_return_code_vrex = json_integer_value(json_obj);
                  }

                  // Process NLP values
                  json_obj = json_object_get(json_obj_root, "nlp");
                  if(json_obj != NULL && json_is_object(json_obj)) {
                     // Process Response values
                     json_t *json_obj_response = json_object_get(json_obj, "response");
                     if(json_obj_response != NULL && json_is_object(json_obj_response)) {
                        json_t *json_obj_text = json_object_get(json_obj_response, "text");

                        if(json_obj_text != NULL && json_is_string(json_obj_text)) {
                           LOG_INFO("%s: onSpeech text: <%s>\n", __FUNCTION__, json_string_value(json_obj_text));
                           m_session_text_vrex = json_string_value(json_obj_text);
                        }
                     }
                  }

                  // Process Speech values
                  json_obj = json_object_get(json_obj_root, "speech");
                  if(json_obj != NULL && json_is_object(json_obj)) {
                     json_t *json_obj_speech = json_object_get(json_obj, "transcription");
                     if(json_obj_speech != NULL && json_is_string(json_obj_speech)) {
                        LOG_INFO("%s: onSpeech transcription: <%s>\n", __FUNCTION__, json_string_value(json_obj_speech));
                     }
                     json_obj_speech = json_object_get(json_obj, "nuanceSessionId");
                     if(json_obj_speech != NULL && json_is_string(json_obj_speech)) {
                        LOG_INFO("%s: onSpeech nuanceSessionId: <%s>\n", __FUNCTION__, json_string_value(json_obj_speech));
                        m_session_id_vrex = json_string_value(json_obj_speech);
                     }
                  }
                  #ifdef MEDIA_SERVICE_ENABLED
                  json_obj = json_object_get(json_obj_root, "mediaserviceUrl");
                  if(json_obj != NULL && json_is_string(json_obj)) {
                     // Broadcast event to any listeners
                     ctrlm_voice_iarm_event_media_service_t event;
                     event.api_revision = CTRLM_VOICE_IARM_BUS_API_REVISION;
                     errno_t safec_rc = strncpy_s(event.media_service_url, sizeof(event.media_service_url), json_string_value(json_obj), sizeof(event.media_service_url) - 1);
                     ERR_CHK(safec_rc);
                     event.media_service_url[2082] = '\0';
                     ctrlm_voice_iarm_event_media_service(&event);
                  }
                  #endif
               }
               if(json_obj_root != NULL) {
                  json_decref(json_obj_root);
               }
            }
         }
         catch (const std::exception & err ) {
            LOG_INFO("%s: exception in Speech response: %s\n", __FUNCTION__, err.what());
            LOG_INFO("%s: Speech response string for exception: %s\n", __FUNCTION__, m_voice_response.c_str());
            std::stringstream sstm;
            sstm << "Unknown exception:" << err.what() << ": speech response:" << m_voice_response.c_str();
            m_session_message_vrex = sstm.str();
            m_caught_exception = true;
         }
      }
#ifndef USE_VOICE_SDK
      // Send response back to the remote
      if(m_return_code_curl != CURLE_OK || m_caught_exception || (m_return_code_http != 200) || (m_return_code_vrex != 0)) {
         ctrlm_voice_cmd_status_set(m_network_id, m_controller_id, VOICE_COMMAND_STATUS_FAILURE);
      } else {
         ctrlm_voice_cmd_status_set(m_network_id, m_controller_id, VOICE_COMMAND_STATUS_SUCCESS);
      }
#endif

      if(m_log_metrics) {
         char buffer[700];
         LOG_INFO("%s: get log metrics\n", __FUNCTION__);
         request_metrics_get(curl, buffer, 700);
         printf("%s: %s\n", __FUNCTION__, buffer);
      }
      curl_multi_cleanup(multi_handle);
      curl_easy_cleanup(curl);
   }

   #ifndef STREAM_END_SYNCHRONOUS
   if (!m_do_not_close_read_pipe) {
      LOG_INFO("%s: Server thread exited. Closing read side of pipe\n", __FUNCTION__);
      close(m_voice_data_pipe_read);
      m_voice_data_pipe_read = -1;
   }
   #endif
   m_do_not_close_read_pipe = false;

   if(!is_audio_stream_in_progress()) {
      // Send the notification
      notify_result();
   }

   if(m_return_code_curl == CURLE_OK && m_return_code_http == 200 && (m_return_code_vrex >= VREX_SVR_RET_CODE_SAT_SIGNATURE && m_return_code_vrex <= VREX_SVR_RET_CODE_SAT_ERROR)) { // Invalidate the current SAT due to SAT related error
      LOG_INFO("%s: Received SAT related error code <%ld>\n", __FUNCTION__, m_return_code_vrex);
      ctrlm_voice_vrex_req_completed(true, m_conversation_id);
   } else {
      ctrlm_voice_vrex_req_completed(false, m_conversation_id);
   }
}

char *voice_session_t::request_metrics_get(CURL *curl, char *buffer, unsigned long bufsize) {
   double total, connect, start_transfer, resolve, app_connect, pre_transfer, redirect;
   unsigned long response_code, ssl_verify;
   char *url;
   curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL,      &url);
   curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME,    &resolve);
   curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME,       &connect);
   curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME,    &app_connect);
   curl_easy_getinfo(curl, CURLINFO_PRETRANSFER_TIME,   &pre_transfer);
   curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &start_transfer);
   curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME ,        &total);
   curl_easy_getinfo(curl, CURLINFO_REDIRECT_TIME,      &redirect);
   curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,      &response_code);
   curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT,   &ssl_verify);

   errno_t safec_rc = sprintf_s(buffer, bufsize, "\nHttpRequestEnd %s code=%lu times={total=%g, connect=%g startTransfer=%g resolve=%g, appConnect=%g, preTransfer=%g, redirect=%g, sslVerify=%lu}\n", url, response_code, total, connect, start_transfer, resolve, app_connect, pre_transfer, redirect, ssl_verify);
   if(safec_rc < EOK) {
      ERR_CHK(safec_rc);
   }
   buffer[bufsize - 1] = '\0';
   return buffer;
}

void voice_session_t::remote_user_string_set(const char *user_string) {
   m_remote_user_string = user_string;
   LOG_INFO("%s: <%s>\n", __FUNCTION__, m_remote_user_string.c_str());
   user_agent_string_refresh();
}

void voice_session_t::remote_software_version_set(guchar major, guchar minor, guchar revision, guchar patch) {
   char temp_buffer[TEMP_BUFFER_SIZE];
   errno_t safec_rc = sprintf_s(temp_buffer, TEMP_BUFFER_SIZE, "%u.%u.%u.%u", major, minor, revision, patch);
   if(safec_rc < EOK) {
      ERR_CHK(safec_rc);
   }
   temp_buffer[TEMP_BUFFER_SIZE-1] = '\0';
   m_remote_software_version = temp_buffer;

   LOG_INFO("%s: <%s>\n", __FUNCTION__, m_remote_software_version.c_str());
   user_agent_string_refresh();
}

void voice_session_t::remote_hardware_version_set(guchar manufacturer, guchar model, guchar revision, guint16 lot_code) {
   char temp_buffer[TEMP_BUFFER_SIZE];
   errno_t safec_rc = sprintf_s(temp_buffer, TEMP_BUFFER_SIZE, "%u.%u.%u.%u", manufacturer, model, revision, lot_code);
   if(safec_rc < EOK) {
      ERR_CHK(safec_rc);
   }
   temp_buffer[TEMP_BUFFER_SIZE-1] = '\0';
   m_remote_hardware_version = temp_buffer;

   LOG_INFO("%s: <%s>\n", __FUNCTION__, m_remote_hardware_version.c_str());
   user_agent_string_refresh();
}

void voice_session_t::remote_battery_voltage_set(guchar voltage, guchar percentage) {
   char temp_buffer[TEMP_BUFFER_SIZE];
   errno_t safec_rc = sprintf_s(temp_buffer, TEMP_BUFFER_SIZE, "%.2fV", (((float)voltage) *  4.0 / 255));
   if(safec_rc < EOK) {
      ERR_CHK(safec_rc);
   }
   temp_buffer[TEMP_BUFFER_SIZE-1] = '\0';
   m_remote_battery_voltage    = temp_buffer;
   m_remote_battery_value      = voltage;
   m_remote_battery_percentage = percentage;

   LOG_INFO("%s: Voltage <%s>  Percentage <%u>\n", __FUNCTION__, m_remote_battery_voltage.c_str(), m_remote_battery_percentage);
   user_agent_string_refresh();
}

void voice_session_t::user_agent_string_refresh() {
   m_user_agent = "rmtType=" + m_remote_user_string + "; rmtSVer=" + m_remote_software_version + "; rmtHVer=" + m_remote_hardware_version + "; stbName=" + m_stb_name + "; rmtVolt=" + m_remote_battery_voltage;
}

unsigned char voice_session_t::get_utterance_too_short() {
   return m_utterance_too_short;
}

bool voice_session_t::abort_vrex_request_get() {
   return m_abort_vrex_request;
}

void voice_session_t::abort_vrex_request_set(bool abort) {
   m_abort_vrex_request = abort;
}

unsigned long voice_session_t::voice_data_qty_sent_get() {
   return m_voice_data_qty_sent;
}

void voice_session_t::abort_vrex_request() {
   LOG_INFO("%s: Aborting vrex request\n", __FUNCTION__);
   m_abort_vrex_request = true;
}

void voice_session_t::vrex_response_timeout_reset() {
   m_vrex_response_timeout_tag = 0;
}

void voice_session_t::curl_info_print(CURL *curl) {
   double      total_time        = 0;        // Total time in seconds of the previous transfer
   double      lookup_time       = 0;        // Total time in seconds from start until name resolving is done
   double      connect_time      = 0;        // Total time in seconds from start until connection was complete
   double      pretransfer_time  = 0;        // Total time in seconds from start until file transfer starts
   double      redirect_time     = 0;        // Total time in seconds it takes from all redirection steps
   long        redirect_count    = 0;        // Total number of redirections that were followed
   double      upload_size       = 0;        // Total amount of bytes that were uploaded
   double      upload_speed      = 0;        // Average upload speed in bytes/second
   double      download_size     = 0;        // Total amount of bytes that were downloaded
   double      download_speed    = 0;        // Average download speed in bytes/second
   long        request_size      = 0;        // Total size of the issued requests
   double      ul_content_length = 0;        // Size of the upload
   double      dl_content_length = 0;        // Size of the dowload
   long        os_errno          = 0;        // The errno variable from the connect failure
   long        connects          = 0;        // The number of connects that has to occur to achieve transfer
   char       *primary_ip        = NULL;     // IP address of most recent connection with the current curl handle
   long        primary_port      = 0;        // The destination port of the most recent connection with the current curl handle

   curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME,                &total_time);
   curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME,           &lookup_time);
   curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME,              &connect_time);
   curl_easy_getinfo(curl, CURLINFO_PRETRANSFER_TIME,          &pretransfer_time);
   curl_easy_getinfo(curl, CURLINFO_REDIRECT_TIME,             &redirect_time);
   curl_easy_getinfo(curl, CURLINFO_REDIRECT_COUNT,            &redirect_count);
   curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD,               &upload_size);
   curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD,              &upload_speed);
   curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD,             &download_size);
   curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD,            &download_speed);
   curl_easy_getinfo(curl, CURLINFO_REQUEST_SIZE,              &request_size);
   curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_UPLOAD,     &ul_content_length);
   curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD,   &dl_content_length);
   curl_easy_getinfo(curl, CURLINFO_OS_ERRNO,                  &os_errno);
   curl_easy_getinfo(curl, CURLINFO_NUM_CONNECTS,              &connects);
   curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP,                &primary_ip);
   curl_easy_getinfo(curl, CURLINFO_PRIMARY_PORT,              &primary_port);

   // Now print all of this
   LOG_INFO("%s: cURL Information:\n", __FUNCTION__);
   LOG_INFO("%s: IP Address <%s>, Port <%ld>\n", __FUNCTION__, (primary_ip != NULL ? primary_ip : "NULL"), primary_port);
   LOG_INFO("%s: Total Time: <%fs>, Name Lookup Time: <%fs>, Connect Time <%fs>, Pretransfer Time: <%fs>\n", __FUNCTION__, total_time, lookup_time, connect_time, pretransfer_time);
   LOG_INFO("%s: Redirect Time: <%fs>, Redirect Count: <%ld>\n", __FUNCTION__, redirect_time, redirect_count);
   LOG_INFO("%s: Upload Size: <%f b>, Upload Speed: <%f b/s>, Download Size: <%f b>, Download Speed: <%f b/s>\n", __FUNCTION__, upload_size, upload_speed, download_size, download_speed);
   LOG_INFO("%s: Request Size: <%ld>, Content Length Upload: <%f b>, Content Length Download: <%f b>\n", __FUNCTION__, request_size, ul_content_length, dl_content_length);
   LOG_INFO("%s: # of Connects: <%ld>, OS Errno: <%ld>\n", __FUNCTION__, connects, os_errno);
}

void voice_session_t::curl_info_for_session_result_get(CURL *curl) {
   double      lookup_time       = 0;        // Total time in seconds from start until name resolving is done
   double      connect_time      = 0;        // Total time in seconds from start until connection was complete
   char       *primary_ip        = NULL;     // IP address of most recent connection with the current curl handle

   curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &lookup_time);
   curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME,    &connect_time);
   curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP,      &primary_ip);

   m_lookup_time  = lookup_time;
   m_connect_time = connect_time;

   errno_t safec_rc  = strncpy_s(m_primary_ip, sizeof(m_primary_ip), primary_ip, CTRLM_VOICE_REQUEST_IP_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   m_primary_ip[CTRLM_VOICE_REQUEST_IP_MAX_LENGTH - 1] = '\0';
}
//#error Enable to check for warnings


void voice_session_t::receiver_id_set(const char* receiver_id){
   m_receiver_id = receiver_id;
}

void voice_session_t::device_id_set(const char* device_id){
   m_device_id = device_id;
}

void voice_session_t::service_account_id_set(const char* service_account_id){
   m_service_account_id = service_account_id;
}

void voice_session_t::partner_id_set(const char* partner_id){
   m_partner_id = partner_id;
}

void voice_session_t::experience_set(const char* experience){
   m_experience = experience;
}
