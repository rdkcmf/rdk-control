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
#ifndef _CTRLM_VOICE_SESSION_H_
#define _CTRLM_VOICE_SESSION_H_

#include <map>
#include <string>
#include <utility>
#include <curl/curl.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include "ctrlm_network.h"

// Time in seconds before curl request times out
#define DEFAULT_VREX_TIMEOUT    (30)

//#define PRINT_STATS_LAG_TIME

using namespace std;

typedef struct {
   string mime_type;
   string sub_type;
   string language;
} vrex_mgr_audio_info_t;

typedef struct {
   ctrlm_network_id_t network_id;
   ctrlm_controller_id_t controller_id;
   vrex_mgr_audio_info_t audio_info;
   string conversation_id;
   int voice_data_pipe_read;
   string service_access_token;
   unsigned long session_id;
   unsigned long timeout_vrex_request;
   string user_string;
   version_software_t sw_version;
   version_hardware_t hw_version;
   guchar battery_voltage;
   guchar battery_percentage;
} stream_begin_params_t;

class voice_session_t
{
public:
   voice_session_t(const string& route, const string& aspect_ratio, const string& language, const string& stb_name, unsigned long vrex_response_timeout, const ctrlm_voice_query_strings_t& query_strings, const string& app_id=JSON_STR_VALUE_VOICE_APP_ID_HTTP);

   virtual ~voice_session_t();

   void   stream_request(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id);
   bool   stream_begin(const stream_begin_params_t& params);
   void   stream_end(ctrlm_voice_session_end_reason_t reason, unsigned long stop_data, int voice_data_pipe_write, unsigned long rf_channel, unsigned long buffer_watermark, unsigned long packets_total, unsigned long packets_lost, unsigned char link_quality, unsigned long session_id);
   void   stream_stats(ctrlm_voice_stats_session_t session, ctrlm_voice_stats_reboot_t reboot);
   void   notify_result();

   // Internal methods
   void start_transfer_server();
   size_t server_socket_callback(void *buffer, size_t size, size_t nmemb);
   void speech_response_add(string response_data, unsigned long len);
   void server_details_update(const string& server_url_vrex, const string& aspect_ratio, const string& guide_language, const ctrlm_voice_query_strings_t& query_strings, const string& server_url_vrex_src_ff);

   bool wait_for_statistics();

   void remote_user_string_set(const char *user_string);
   void remote_software_version_set(guchar major, guchar minor, guchar revision, guchar patch);
   void remote_hardware_version_set(guchar manufacturer, guchar model, guchar revision, guint16 lot_code);
   void remote_battery_voltage_set(guchar voltage, guchar percentage);

   unsigned char get_utterance_too_short();

   bool          abort_vrex_request_get();
   void          abort_vrex_request_set(bool abort);
   void          vrex_response_timeout_reset();
   unsigned long voice_data_qty_sent_get();

   void          curl_info_print(CURL *curl);
   void          curl_info_for_session_result_get(CURL *curl);
   void         receiver_id_set(const char* receiver_id);
   void         device_id_set(const char* device_id);
   void         service_account_id_set(const char* service_account_id);
   void         partner_id_set(const char* partner_id);
   void         experience_set(const char* experience);

private:
   char * request_metrics_get(CURL *curl,char *buffer, unsigned long bufsize);
   void   notify_result_success();
   void   notify_result_error();
   void   notify_result_short();
   void   notify_stats();
   void   user_agent_string_refresh();
   void   abort_vrex_request();

private:
   string                            m_app_id;
   string                            m_receiver_id;
   string                            m_device_id;
   string                            m_remote_user_string;
   string                            m_remote_software_version;
   string                            m_remote_hardware_version;
   string                            m_stb_name;
   string                            m_remote_battery_voltage;
   unsigned char                     m_remote_battery_value;
   unsigned char                     m_remote_battery_percentage;
   string                            m_user_agent;
   string                            m_conversation_id;      // The conversation id is returned by the voice call
   string                            m_service_account_id;   // The service account id is returned by the voice call
   string                            m_partner_id;           // The partner id is returned by the voice call
   string                            m_experience;           // The experience tag is returned by the voice call
   string                            m_service_access_token; // service access token will be used if present
   bool                              m_base_route_contains_speech;
   bool                              m_curl_initialized;
   string                            m_base_route;
   string                            m_server_url_vrex_src_ff;
   string                            m_aspect_ratio;
   string                            m_guide_language;
   ctrlm_voice_query_strings_t       m_query_strings;
   ctrlm_network_id_t                m_network_id;
   ctrlm_controller_id_t             m_controller_id;
   vrex_mgr_audio_info_t             m_audio_info;
   bool                              m_log_metrics;
   string                            m_voice_response;
   int                               m_voice_data_pipe_read;
   GThread *                         m_data_read_thread;
   pthread_cond_t                    m_cond;
   pthread_mutex_t                   m_mutex;
   unsigned long                     m_voice_data_qty_sent;
   unsigned long                     m_voice_rsp_qty_rxd;
   unsigned char                     m_utterance_too_short;
   long                              m_return_code_vrex;
   CURLcode                          m_return_code_curl;
   long                              m_return_code_http;
   unsigned long                     m_session_id_vrex_mgr;
   string                            m_session_id_vrex;
   string                            m_session_text_vrex;
   string                            m_session_message_vrex;
   #ifdef PRINT_STATS_LAG_TIME
   struct timespec                   m_session_end_time;
   #endif
   bool                              m_notified_results;
   bool                              m_notified_stats;
   bool                              m_caught_exception;
   bool                              m_thread_failed;
   ctrlm_voice_session_end_reason_t  m_end_reason;
   ctrlm_voice_stats_session_t       m_stats_session;
   ctrlm_voice_stats_reboot_t        m_stats_reboot;
   unsigned long                     m_timeout_vrex_request;
   bool                              m_abort_vrex_request;
   unsigned int                      m_vrex_response_timeout_tag;
   unsigned long                     m_vrex_response_timeout;
   uuid_t                            m_uuid;
   char                              m_primary_ip[CTRLM_VOICE_REQUEST_IP_MAX_LENGTH];
   double                            m_lookup_time;
   double                            m_connect_time;
   char                              m_uuid_str[CTRLM_VOICE_SESSION_ID_MAX_LENGTH];
   volatile bool                     m_do_not_close_read_pipe;
   volatile bool                     m_terminate_speech_server_thread;
};

#endif
