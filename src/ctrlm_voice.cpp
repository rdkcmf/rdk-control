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
#include "ctrlm_voice_session.h"
#include "ctrlm_database.h"
#ifdef AUTH_ENABLED
#include "ctrlm_auth.h"
#endif
#include "ctrlm_voice_packet_analysis.h"
#include "json_config.h"


//#define DYNAMIC_PRIORITY                                       // Increase priority during a voice session
#define MASK_RAPID_KEYPRESS                                    // Don't send short voice key press events to vrex manager
#define VOICE_BUFFER_STATS                                     // Print buffer statistics
//#define TIMING_START_TO_FIRST_FRAGMENT                         // Print lag time from voice session begin to receipt of first voice fragment
//#define TIMING_LAST_FRAGMENT_TO_STOP                           // Print lag time from receipt of the last voice fragment to the voice session end

#ifdef VOICE_BUFFER_STATS
#define VOICE_PACKET_TIME              (11375)                 // amount of time between voice packets in microseconds (91*2/16000)*1000000
#define VOICE_BUFFER_WARNING_THRESHOLD (4 * VOICE_PACKET_TIME) // Number of packets in HW buffer before printing.  Set to zero to print all packets.
#endif

#define VOICE_CHUNK_SIZE_SUBSEQUENT    (1425)                  // This was changed from 4085 which is about 3 complete ethernet packets (do not increase over 4096 as this changes the behavior of the pipe write)

//#define SAT_CORRUPT_TOKEN_TEST_FIRST  // Returns 109 malformed token
//#define SAT_CORRUPT_TOKEN_TEST_SECOND // Returns 109 malformed token
//#define SAT_CORRUPT_TOKEN_TEST_THIRD  // Returns 108 invalid signature
//#define SAT_EXPIRED_TOKEN_TEST        // Returns 111 service access error
#ifdef SAT_EXPIRED_TOKEN_TEST
#error Need to populate g_vrex_token_expired with an expired SAT
const char *g_vrex_token_expired = "";
#endif

#define CTRLM_VOICE_MIMETYPE_ADPCM           "audio/x-adpcm"
#define CTRLM_VOICE_SUBTYPE_ADPCM            "ADPCM"

#define UTTERANCE_BUFFER_SIZE                   (8192 * 12) // 8K x 10 seconds with some room to spare

#define BACKGROUND_UPDATE_TIMEOUT_COMMAND_ID ((unsigned char)0x20) //Update download timer on this Command ID

//#define SEQUENCE_NUM_INVALID  (0xFF)

// Issue locally to test  curl -d '' -X POST http://127.0.0.1:50050/authService/getServiceAccessToken

typedef struct {
   bool                             initialized;
   bool                             voice_enabled;
   gboolean                         device_update_in_progress;
   bool                             voice_server_ready;
   volatile gint                    session_in_progress;
   gboolean                         vrex_req_in_progress; // http request is in progress
   gboolean                         audio_stream_in_progress; // audio stream from the controller is in progress
   gboolean                         session_stats_in_progress;   // Session stats not received from controller (or timeout)
   ctrlm_network_id_t               network_id;
   ctrlm_controller_id_t            controller_id;
   ctrlm_rcu_controller_type_t      controller_type;
   int                              data_pipe_write;
   unsigned long                    data_qty_bufd;
   unsigned long                    vrex_session_id;
   guint                            timeout_tag_inactivity;
   guint                            timeout_tag_stats;
   guint32                          sample_qty;
   guint32                          total_sample_size;
   voice_session_prefs_t            vrex_prefs;
   string                           conversation_id;
   string                           service_access_token;
   time_t                           service_access_token_expiration;
   bool                             curl_initialized;
   unsigned long                    vrex_session_id_global;
   unsigned long                    chunk_size_first;
   ctrlm_voice_session_end_reason_t stop_reason_last;
   ctrlm_controller_id_t            controller_id_last;
   guint32                          utterance_buffer_index_write;
   guint32                          utterance_buffer_index_read;
   guchar                           utterance_buffer[UTTERANCE_BUFFER_SIZE];
} ctrlm_voice_global_t;

static ctrlm_voice_global_t g_ctrlm_voice;

static ctrlm_voice_packet_analysis* voice_packet_analysis_obj = 0;

#ifdef VOICE_BUFFER_STATS
static ctrlm_timestamp_t       first_fragment;
static unsigned char           voice_buffer_warning_triggered = 0;
static unsigned long           voice_buffer_fragment_qty      = 0;
static unsigned long           voice_buffer_chunk_qty         = 0;
static unsigned long long      voice_buffer_frag_time_sum     = 0;
static unsigned long long      voice_buffer_frag_time_max     = 0;
static unsigned long long      voice_buffer_frag_time_min     = 0xFFFFFFFFFFFFFFFF;
static unsigned long long      voice_buffer_chunk_time_sum    = 0;
static unsigned long long      voice_buffer_chunk_time_max    = 0;
static unsigned long long      voice_buffer_chunk_time_min    = 0xFFFFFFFFFFFFFFFF;
static unsigned long           voice_buffer_high_watermark    = 0;
#endif
static unsigned long voice_session_rf_channel            = 0;
static unsigned long voice_session_lqi_total             = 0;

#ifdef TIMING_START_TO_FIRST_FRAGMENT
ctrlm_timestamp_t voice_session_begin_timestamp;
#endif
#ifdef TIMING_LAST_FRAGMENT_TO_STOP
ctrlm_timestamp_t voice_session_last_fragment_timestamp;
#endif

static void ctrlm_voice_service_access_token_invalidate(void);

static void ctrlm_voice_enable_set(bool enable);
static void ctrlm_voice_server_ready_set(bool enable);
#ifndef USE_VOICE_SDK
static gboolean is_voice_enabled(void);
static gboolean is_device_update_in_progress(void);
//static gboolean is_server_ready(void);
static gboolean is_audio_format_supported(void);
#endif


static string g_vrex_authservice_response = "";
static bool ctrlm_voice_service_access_token_get();

static void ctrlm_voice_load_config(json_t *json_obj_voice);
static gboolean ctrlm_voice_timeout_inactivity(gpointer user_data);
#ifndef USE_VOICE_SDK
static void ctrlm_voice_rsp_session_request(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_timestamp_t timestamp, voice_session_response_status_t status, ctrlm_voice_session_abort_reason_t reason);
static gboolean ctrlm_voice_timeout_stats(gpointer user_data);
static void ctrlm_voice_notify_stats_none(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id);
#endif

static void ctrlm_voice_session_begin(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id);
static void ctrlm_voice_session_fragment(ctrlm_controller_id_t controller_id, guchar *data, unsigned long len);
static void ctrlm_voice_iarm_event_session_begin(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, unsigned long session_id);
static void ctrlm_voice_iarm_event_session_end(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, unsigned long session_id, ctrlm_voice_session_end_reason_t reason);

static gboolean ctrlm_req_voice_settings_update(const char *server_url_vrex, const char *aspect_ratio, const char *guide_language, const ctrlm_voice_query_strings_t& query_strings, const char *server_url_vrex_src_ff);

static gboolean is_session_stats_in_progress(void);
static void ctrlm_voice_session_completed(void);
static void ctrlm_voice_audio_stream_completed(void);
static void ctrlm_voice_session_stats_completed(void);
static void ctrlm_voice_load_temp_voice_settings();
static bool ctrlm_voice_handle_settings(ctrlm_voice_iarm_call_settings_t *voice_settings);

void ctrlm_voice_init(json_t *json_obj_voice) {
   g_ctrlm_voice.initialized                          = false;
   g_ctrlm_voice.session_in_progress                  = false;
   g_ctrlm_voice.vrex_req_in_progress                 = false;
   g_ctrlm_voice.audio_stream_in_progress             = false;
   g_ctrlm_voice.session_stats_in_progress            = false;
   g_ctrlm_voice.network_id                           = CTRLM_HAL_NETWORK_ID_INVALID;
   g_ctrlm_voice.controller_id                        = CTRLM_HAL_CONTROLLER_ID_INVALID;
   g_ctrlm_voice.controller_type                      = CTRLM_RCU_CONTROLLER_TYPE_INVALID;
   g_ctrlm_voice.stop_reason_last                     = CTRLM_VOICE_SESSION_END_REASON_MAX;
   g_ctrlm_voice.controller_id_last                   = CTRLM_HAL_CONTROLLER_ID_INVALID;
   g_ctrlm_voice.timeout_tag_inactivity               = TIMEOUT_TAG_INVALID;
   g_ctrlm_voice.timeout_tag_stats                    = TIMEOUT_TAG_INVALID;
   g_ctrlm_voice.sample_qty                           = 0;
   g_ctrlm_voice.total_sample_size                    = 0;
   g_ctrlm_voice.vrex_prefs.server_url_vrex           = JSON_STR_VALUE_VOICE_URL_VREX;
   g_ctrlm_voice.vrex_prefs.server_url_vrex_src_ff    = JSON_STR_VALUE_VOICE_URL_SRC_FF;
   g_ctrlm_voice.vrex_prefs.sat_enabled               = JSON_BOOL_VALUE_VOICE_ENABLE_SAT;
   g_ctrlm_voice.vrex_prefs.timeout_vrex_request      = JSON_INT_VALUE_VOICE_VREX_REQUEST_TIMEOUT;
   g_ctrlm_voice.vrex_prefs.timeout_vrex_response     = JSON_INT_VALUE_VOICE_VREX_RESPONSE_TIMEOUT;
   g_ctrlm_voice.vrex_prefs.timeout_sat_request       = JSON_INT_VALUE_VOICE_SAT_REQUEST_TIMEOUT;
   g_ctrlm_voice.vrex_prefs.timeout_packet_initial    = JSON_INT_VALUE_VOICE_TIMEOUT_PACKET_INITIAL;
   g_ctrlm_voice.vrex_prefs.timeout_packet_subsequent = JSON_INT_VALUE_VOICE_TIMEOUT_PACKET_SUBSEQUENT;
   g_ctrlm_voice.vrex_prefs.timeout_stats             = JSON_INT_VALUE_VOICE_TIMEOUT_STATS;
   g_ctrlm_voice.vrex_prefs.guide_language            = JSON_STR_VALUE_VOICE_LANGUAGE;
   g_ctrlm_voice.vrex_prefs.aspect_ratio              = JSON_STR_VALUE_VOICE_ASPECT_RATIO;
   g_ctrlm_voice.vrex_prefs.utterance_save            = JSON_BOOL_VALUE_VOICE_SAVE_LAST_UTTERANCE;
   g_ctrlm_voice.vrex_prefs.utterance_path            = JSON_STR_VALUE_VOICE_UTTERANCE_PATH;
   g_ctrlm_voice.vrex_prefs.utterance_duration_min    = JSON_INT_VALUE_VOICE_MINIMUM_DURATION;
   g_ctrlm_voice.vrex_prefs.force_voice_settings      = JSON_BOOL_VALUE_VOICE_FORCE_VOICE_SETTINGS;
   g_ctrlm_voice.vrex_prefs.packet_loss_threshold     = JSON_INT_VALUE_VOICE_PACKET_LOSS_THRESHOLD;
   g_ctrlm_voice.voice_enabled                        = JSON_BOOL_VALUE_VOICE_ENABLE;
   g_ctrlm_voice.voice_server_ready                   = false;
   g_ctrlm_voice.device_update_in_progress            = false;
   g_ctrlm_voice.conversation_id                      = "";
   g_ctrlm_voice.curl_initialized                     = false;
   g_ctrlm_voice.vrex_session_id_global               = 0;
   g_ctrlm_voice.chunk_size_first                     = (JSON_INT_VALUE_VOICE_MINIMUM_DURATION * 1000 / VOICE_PACKET_TIME) * 95;
   g_ctrlm_voice.data_pipe_write                      = -1;
   g_ctrlm_voice.data_qty_bufd                        =  0;

   ctrlm_voice_load_config(json_obj_voice);

   ctrlm_voice_server_ready_set(true);

   errno_t safec_rc = memset_s(&g_ctrlm_voice.vrex_prefs.query_strings, sizeof(ctrlm_voice_query_strings_t), 0, sizeof(ctrlm_voice_query_strings_t));
   ERR_CHK(safec_rc);

   // Check to see if temp settings exist in case of crash
   if(FALSE == g_ctrlm_voice.vrex_prefs.force_voice_settings) {
      LOG_INFO("%s: Checking for vrex settings in DB\n", __FUNCTION__);
      ctrlm_voice_load_temp_voice_settings();
   }

   LOG_INFO("%s: Init complete\n", __FUNCTION__);

   // If !sat_enabled, this will stop the polling
   ctrlm_main_sat_enabled_set(g_ctrlm_voice.vrex_prefs.sat_enabled);

   voice_packet_analysis_obj = ctrlm_voice_packet_analysis_factory();

   g_ctrlm_voice.initialized = true;
   
   if(!ctrlm_voice_iarm_init()) {
      LOG_ERROR("%s: Unable to initialize IARM!\n", __FUNCTION__);
   }
}

void ctrlm_voice_terminate(void) {
   LOG_INFO("%s: clean up\n", __FUNCTION__);
   if(g_ctrlm_voice.session_in_progress) {
      LOG_INFO("%s: Voice Session in Progress, Aborting Session!\n", __FUNCTION__);
      ctrlm_timestamp_t timestamp;
      ctrlm_timestamp_get(&timestamp);
#ifndef USE_VOICE_SDK
      ctrlm_voice_rsp_session_request(g_ctrlm_voice.network_id, g_ctrlm_voice.controller_id, timestamp, VOICE_SESSION_RESPONSE_FAILURE, CTRLM_VOICE_SESSION_ABORT_REASON_APPLICATION_RESTART);
#endif
      ctrlm_voice_session_completed();
   }
   ctrlm_voice_iarm_terminate();
   if (voice_packet_analysis_obj != 0) {
      delete voice_packet_analysis_obj;
   }
}

bool ctrlm_voice_is_initialized() {
   return(g_ctrlm_voice.initialized);
}

void ctrlm_voice_load_config(json_t *json_obj_voice) {
   json_config conf;
   if (conf.config_object_set(json_obj_voice)){
      conf.config_value_get(JSON_BOOL_NAME_VOICE_ENABLE,                   g_ctrlm_voice.voice_enabled);
      conf.config_value_get(JSON_STR_NAME_VOICE_URL_VREX,                  g_ctrlm_voice.vrex_prefs.server_url_vrex);
      conf.config_value_get(JSON_STR_NAME_VOICE_URL_SRC_FF,                g_ctrlm_voice.vrex_prefs.server_url_vrex_src_ff);
      conf.config_value_get(JSON_INT_NAME_VOICE_VREX_REQUEST_TIMEOUT,      g_ctrlm_voice.vrex_prefs.timeout_vrex_request,0);
      conf.config_value_get(JSON_INT_NAME_VOICE_VREX_RESPONSE_TIMEOUT,     g_ctrlm_voice.vrex_prefs.timeout_vrex_response,0);
      conf.config_value_get(JSON_BOOL_NAME_VOICE_ENABLE_SAT,               g_ctrlm_voice.vrex_prefs.sat_enabled);
      conf.config_value_get(JSON_INT_NAME_VOICE_SAT_REQUEST_TIMEOUT,       g_ctrlm_voice.vrex_prefs.timeout_sat_request, 0,500);
      conf.config_value_get(JSON_INT_NAME_VOICE_TIMEOUT_PACKET_INITIAL,    g_ctrlm_voice.vrex_prefs.timeout_packet_initial,0);
      conf.config_value_get(JSON_INT_NAME_VOICE_TIMEOUT_PACKET_SUBSEQUENT, g_ctrlm_voice.vrex_prefs.timeout_packet_subsequent,0);
      conf.config_value_get(JSON_INT_NAME_VOICE_TIMEOUT_STATS,             g_ctrlm_voice.vrex_prefs.timeout_stats,0);
      conf.config_value_get(JSON_STR_NAME_VOICE_ASPECT_RATIO,              g_ctrlm_voice.vrex_prefs.aspect_ratio);
      conf.config_value_get(JSON_STR_NAME_VOICE_LANGUAGE,                  g_ctrlm_voice.vrex_prefs.guide_language);
      conf.config_value_get(JSON_BOOL_NAME_VOICE_SAVE_LAST_UTTERANCE,      g_ctrlm_voice.vrex_prefs.utterance_save);
      conf.config_value_get(JSON_STR_NAME_VOICE_UTTERANCE_PATH,            g_ctrlm_voice.vrex_prefs.utterance_path);
      conf.config_value_get(JSON_BOOL_NAME_VOICE_FORCE_VOICE_SETTINGS,     g_ctrlm_voice.vrex_prefs.force_voice_settings);
      conf.config_value_get(JSON_INT_NAME_VOICE_MINIMUM_DURATION,          g_ctrlm_voice.vrex_prefs.utterance_duration_min,0,2000);
      conf.config_value_get(JSON_INT_NAME_VOICE_PACKET_LOSS_THRESHOLD,     g_ctrlm_voice.vrex_prefs.packet_loss_threshold,0,100);
      g_ctrlm_voice.chunk_size_first                  = (g_ctrlm_voice.vrex_prefs.utterance_duration_min * 1000 / VOICE_PACKET_TIME) * 95;

   }

   LOG_INFO("%s: Voice Enabled             <%s>\n",     __FUNCTION__, g_ctrlm_voice.voice_enabled ? "YES" : "NO");
   LOG_INFO("%s: Vrex URL                  <%s>\n",     __FUNCTION__, g_ctrlm_voice.vrex_prefs.server_url_vrex.c_str());
   LOG_INFO("%s: Farfield Vrex URL         <%s>\n",     __FUNCTION__, g_ctrlm_voice.vrex_prefs.server_url_vrex_src_ff.c_str());
   LOG_INFO("%s: Timeout Vrex Request      %lu secs\n", __FUNCTION__, g_ctrlm_voice.vrex_prefs.timeout_vrex_request);
   LOG_INFO("%s: Timeout Vrex Response     %lu secs\n", __FUNCTION__, g_ctrlm_voice.vrex_prefs.timeout_vrex_response);
   LOG_INFO("%s: SAT Enabled               <%s>\n",     __FUNCTION__, g_ctrlm_voice.vrex_prefs.sat_enabled ? "YES" : "NO");
   LOG_INFO("%s: Timeout SAT Request       %lu secs\n", __FUNCTION__, g_ctrlm_voice.vrex_prefs.timeout_sat_request);
   LOG_INFO("%s: Utterance Duration Min    %lu ms\n",   __FUNCTION__, g_ctrlm_voice.vrex_prefs.utterance_duration_min);
   LOG_INFO("%s: Timeout Packet Initial    %lu ms\n",   __FUNCTION__, g_ctrlm_voice.vrex_prefs.timeout_packet_initial);
   LOG_INFO("%s: Timeout Packet Subsequent %lu ms\n",   __FUNCTION__, g_ctrlm_voice.vrex_prefs.timeout_packet_subsequent);
   LOG_INFO("%s: Timeout Statistics        %lu ms\n",   __FUNCTION__, g_ctrlm_voice.vrex_prefs.timeout_stats);
   LOG_INFO("%s: Aspect Ratio              <%s>\n",     __FUNCTION__, g_ctrlm_voice.vrex_prefs.aspect_ratio.c_str());
   LOG_INFO("%s: Guide Language            <%s>\n",     __FUNCTION__, g_ctrlm_voice.vrex_prefs.guide_language.c_str());
   LOG_INFO("%s: Save Last Utterance       <%s>\n",     __FUNCTION__, g_ctrlm_voice.vrex_prefs.utterance_save ? "YES" : "NO");
   LOG_INFO("%s: Utterance Path            <%s>\n",     __FUNCTION__, g_ctrlm_voice.vrex_prefs.utterance_path.c_str());
   LOG_INFO("%s: Force Voice Settings      <%s>\n",     __FUNCTION__, g_ctrlm_voice.vrex_prefs.force_voice_settings ? "YES" : "NO");
   LOG_INFO("%s: Packet Loss Threshold     <%u>\n",     __FUNCTION__, g_ctrlm_voice.vrex_prefs.packet_loss_threshold);
}

#ifndef USE_VOICE_SDK
IARM_Result_t ctrlm_voice_handle_settings_from_server(void *arg) {
   ctrlm_voice_iarm_call_settings_t *voice_settings = (ctrlm_voice_iarm_call_settings_t *)arg;

   if(FALSE == g_ctrlm_voice.vrex_prefs.force_voice_settings) {
      if(TRUE == ctrlm_voice_handle_settings(voice_settings)) {
         LOG_INFO("%s: Writing settings to temporary DB entry in case of crash\n", __FUNCTION__);
         ctrlm_db_voice_settings_write((guchar *)voice_settings, sizeof(ctrlm_voice_iarm_call_settings_t));
         return(IARM_RESULT_SUCCESS);
      } else {
         return(IARM_RESULT_INVALID_PARAM);
      }
   } else {
      LOG_INFO("%s: Ignoring vrex settings from XRE due to force_voice_settings being set to TRUE\n", __FUNCTION__);
   }
   return IARM_RESULT_SUCCESS;
}
#endif

bool ctrlm_voice_handle_settings(ctrlm_voice_iarm_call_settings_t *voice_settings) {
      if(NULL == voice_settings) {
      LOG_INFO("%s: Invalid settings <NULL>\n", __FUNCTION__);
      return(FALSE);
   }

   LOG_INFO("%s: Rxd CTRLM_VOICE_IARM_EVENT_VOICE_SETTINGS - API Revision %u\n", __FUNCTION__, voice_settings->api_revision);

   if(voice_settings->api_revision != CTRLM_VOICE_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, voice_settings->api_revision, CTRLM_VOICE_IARM_BUS_API_REVISION);
      voice_settings->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(TRUE);
   }

   //pthread_mutex_lock(&g_vrex_mutex);

   if(voice_settings->available & CTRLM_VOICE_SETTINGS_VOICE_ENABLED) {
      ctrlm_voice_enable_set((voice_settings->voice_control_enabled) ? true : false);
      LOG_INFO("%s: Voice Control is <%s> : %u\n", __FUNCTION__, voice_settings->voice_control_enabled ? "ENABLED" : "DISABLED", voice_settings->voice_control_enabled);
   }

   if(voice_settings->available & CTRLM_VOICE_SETTINGS_VREX_SERVER_URL) {
      voice_settings->vrex_server_url[CTRLM_VOICE_SERVER_URL_MAX_LENGTH - 1] = '\0';
      LOG_INFO("%s: vrex URL <%s>\n", __FUNCTION__, voice_settings->vrex_server_url);
      g_ctrlm_voice.vrex_prefs.server_url_vrex = voice_settings->vrex_server_url;
   }

   if(voice_settings->available & CTRLM_VOICE_SETTINGS_VREX_SAT_ENABLED) {
      g_ctrlm_voice.vrex_prefs.sat_enabled = (voice_settings->vrex_sat_enabled) ? true : false;
      LOG_INFO("%s: Vrex SAT is <%s> : (%d)\n", __FUNCTION__, g_ctrlm_voice.vrex_prefs.sat_enabled ? "ENABLED" : "DISABLED", voice_settings->vrex_sat_enabled);
   }

   if(voice_settings->available & CTRLM_VOICE_SETTINGS_GUIDE_LANGUAGE) {
      voice_settings->guide_language[CTRLM_VOICE_GUIDE_LANGUAGE_MAX_LENGTH - 1] = '\0';
      LOG_INFO("%s: Guide Language <%s>\n", __FUNCTION__, voice_settings->guide_language);
      g_ctrlm_voice.vrex_prefs.guide_language = voice_settings->guide_language;
   }

   if(voice_settings->available & CTRLM_VOICE_SETTINGS_ASPECT_RATIO) {
      voice_settings->aspect_ratio[CTRLM_VOICE_ASPECT_RATIO_MAX_LENGTH - 1] = '\0';
      LOG_INFO("%s: Aspect Ratio <%s>\n", __FUNCTION__, voice_settings->aspect_ratio);
      g_ctrlm_voice.vrex_prefs.aspect_ratio = voice_settings->aspect_ratio;
   }

   if(voice_settings->available & CTRLM_VOICE_SETTINGS_UTTERANCE_DURATION) {
      LOG_INFO("%s: Utterance Duration Minimum <%lu ms>\n", __FUNCTION__, voice_settings->utterance_duration_minimum);
      if(voice_settings->utterance_duration_minimum > CTRLM_VOICE_MIN_UTTERANCE_DURATION_MAXIMUM) {
         ctrlm_voice_minimum_utterance_duration_set(CTRLM_VOICE_MIN_UTTERANCE_DURATION_MAXIMUM);
      } else {
         ctrlm_voice_minimum_utterance_duration_set(voice_settings->utterance_duration_minimum);
      }
   }

   if(voice_settings->available & CTRLM_VOICE_SETTINGS_QUERY_STRINGS) {
      if(voice_settings->query_strings.pair_count > CTRLM_VOICE_QUERY_STRING_MAX_PAIRS) {
         LOG_WARN("%s: Query String Pair Count too high <%d>.  Setting to <%d>.\n", __FUNCTION__, voice_settings->query_strings.pair_count, CTRLM_VOICE_QUERY_STRING_MAX_PAIRS);
         voice_settings->query_strings.pair_count = CTRLM_VOICE_QUERY_STRING_MAX_PAIRS;
      }
      for(int query=0; query<voice_settings->query_strings.pair_count; query++ ) {
         //Make sure the strings are NULL terminated
         voice_settings->query_strings.query_string[query].name[CTRLM_VOICE_QUERY_STRING_MAX_LENGTH - 1] = '\0';
         voice_settings->query_strings.query_string[query].value[CTRLM_VOICE_QUERY_STRING_MAX_LENGTH - 1] = '\0';
         LOG_INFO("%s: Query String[%d] name <%s> value <%s>\n", __FUNCTION__, query, voice_settings->query_strings.query_string[query].name, voice_settings->query_strings.query_string[query].value);
      }
      ctrlm_voice_query_strings_set(voice_settings->query_strings);
   }

   if(voice_settings->available & CTRLM_VOICE_SETTINGS_FARFIELD_VREX_SERVER_URL) {
      voice_settings->server_url_vrex_src_ff[CTRLM_VOICE_SERVER_URL_MAX_LENGTH - 1] = '\0';
      LOG_INFO("%s: Farfield vrex URL <%s>\n", __FUNCTION__, voice_settings->server_url_vrex_src_ff);
      g_ctrlm_voice.vrex_prefs.server_url_vrex_src_ff = voice_settings->server_url_vrex_src_ff;
   }

   if(TRUE == ctrlm_req_voice_settings_update(g_ctrlm_voice.vrex_prefs.server_url_vrex.c_str(),
                                              g_ctrlm_voice.vrex_prefs.aspect_ratio.c_str(), 
                                              g_ctrlm_voice.vrex_prefs.guide_language.c_str(),
                                              g_ctrlm_voice.vrex_prefs.query_strings,
                                              g_ctrlm_voice.vrex_prefs.server_url_vrex_src_ff.c_str())) {
      //pthread_mutex_unlock(&g_vrex_mutex);
      voice_settings->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
      return(TRUE);
   }

   voice_settings->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
   return(FALSE);
}

void ctrlm_voice_load_temp_voice_settings() {
   ctrlm_voice_iarm_call_settings_t *voice_settings;
   guint32                           length;

   if(NULL == (voice_settings = (ctrlm_voice_iarm_call_settings_t *)g_malloc(sizeof(ctrlm_voice_iarm_call_settings_t)))) {
      LOG_ERROR("%s: Failed to allocate memory for voice settings\n", __FUNCTION__);
      return;
   }

   errno_t safec_rc = memset_s(voice_settings, sizeof(ctrlm_voice_iarm_call_settings_t), 0, sizeof(ctrlm_voice_iarm_call_settings_t));
   ERR_CHK(safec_rc);
   ctrlm_db_voice_settings_read((guchar **)&voice_settings, &length);

   if(sizeof(ctrlm_voice_iarm_call_settings_t) != length) {
      LOG_WARN("%s: Length of DB entry invalid, either doesn't exit or corrupt\n", __FUNCTION__);
      g_free(voice_settings);
      return;
   }

   if(FALSE == ctrlm_voice_handle_settings(voice_settings)) {
         LOG_WARN("%s: Unable to update voice settings via crash DB entry\n", __FUNCTION__);
   }

   g_free(voice_settings);

}

gboolean ctrlm_req_voice_settings_update(const char *server_url_vrex, const char *aspect_ratio, const char *guide_language, const ctrlm_voice_query_strings_t& query_strings, const char *server_url_vrex_src_ff) {

   if(strlen(server_url_vrex)        >= CTRLM_VOICE_SERVER_URL_MAX_LENGTH     ||
      strlen(aspect_ratio)           >= CTRLM_VOICE_ASPECT_RATIO_MAX_LENGTH   ||
      strlen(guide_language)         >= CTRLM_VOICE_GUIDE_LANGUAGE_MAX_LENGTH ||
      strlen(server_url_vrex_src_ff) >= CTRLM_VOICE_SERVER_URL_MAX_LENGTH) {
      LOG_ERROR("%s: Parameters length exceed message buffers size\n", __FUNCTION__);
      return(FALSE);
   }

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_settings_update_t *msg = (ctrlm_main_queue_msg_voice_settings_update_t *)g_malloc(sizeof(ctrlm_main_queue_msg_voice_settings_update_t));
   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(FALSE);
   }

   errno_t safec_rc = -1;
   safec_rc = memset_s(msg, sizeof(ctrlm_main_queue_msg_voice_settings_update_t), 0, sizeof(ctrlm_main_queue_msg_voice_settings_update_t));
   ERR_CHK(safec_rc);
   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_VOICE_SETTINGS_UPDATE;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   safec_rc = strcpy_s(msg->server_url_vrex, sizeof(msg->server_url_vrex), server_url_vrex);
   ERR_CHK(safec_rc);
   safec_rc = strcpy_s(msg->server_url_vrex_src_ff, sizeof(msg->server_url_vrex_src_ff), server_url_vrex_src_ff);
   ERR_CHK(safec_rc);
   safec_rc = strcpy_s(msg->aspect_ratio, sizeof(msg->aspect_ratio), aspect_ratio);
   ERR_CHK(safec_rc);
   safec_rc = strcpy_s(msg->guide_language, sizeof(msg->guide_language), guide_language);
   ERR_CHK(safec_rc);
   safec_rc = memcpy_s(&msg->query_strings, sizeof(msg->query_strings), &query_strings, sizeof(ctrlm_voice_query_strings_t));
   ERR_CHK(safec_rc);

   ctrlm_main_queue_msg_push(msg);
   return(TRUE);
}
/*
void ctrlm_obj_network_rf4ce_t::req_process_voice_settings_update(ctrlm_main_queue_msg_voice_settings_update_t *dqm) {
   THREAD_ID_VALIDATE();
   // iterate through all controller objects and update each one
   typedef std::map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it_type;
   for(it_type iterator = controllers_.begin(); iterator != controllers_.end(); iterator++) {
      iterator->second->server_details_update(dqm->server_url_vrex, dqm->aspect_ratio, dqm->guide_language, dqm->query_strings);
   }
}
*/

gboolean is_server_ready(void) {
  return(g_ctrlm_voice.voice_server_ready);
}
#ifndef USE_VOICE_SDK
gboolean is_voice_enabled(void) {
   return(g_ctrlm_voice.voice_enabled);
}

gboolean is_device_update_in_progress(void) {
   return(g_ctrlm_voice.device_update_in_progress);
}

gboolean is_audio_format_supported(void) {
   return(TRUE);
}
#endif

gboolean is_voice_session_in_progress(void) {
   return((gboolean)g_ctrlm_voice.session_in_progress);
}

bool set_voice_session_in_progress(bool set) {
   return g_atomic_int_compare_and_exchange(&g_ctrlm_voice.session_in_progress, !set, set);
}

gboolean is_vrex_req_in_progress(void) {
   return(g_ctrlm_voice.vrex_req_in_progress);
}

gboolean is_audio_stream_in_progress(void) {
   return(g_ctrlm_voice.audio_stream_in_progress);
}

gboolean is_session_stats_in_progress(void) {
   return(g_ctrlm_voice.session_stats_in_progress);
}

void ctrlm_voice_vrex_req_completed(bool invalidate_sat, string conversation_id) {
   g_ctrlm_voice.vrex_req_in_progress = false;

   if(!conversation_id.empty()) {
      g_ctrlm_voice.conversation_id = conversation_id;
   }
   if(g_ctrlm_voice.vrex_prefs.sat_enabled && invalidate_sat) {
      LOG_INFO("%s: invalidate token\n", __FUNCTION__);
      ctrlm_voice_service_access_token_invalidate();
   }

   if(!is_audio_stream_in_progress() && !is_session_stats_in_progress()) {
      ctrlm_voice_session_completed();
   }
}

void ctrlm_voice_audio_stream_completed() {
   g_ctrlm_voice.audio_stream_in_progress = false;

   if(!is_vrex_req_in_progress() && !is_session_stats_in_progress()) {
      ctrlm_voice_session_completed();
   }
}

void ctrlm_voice_session_stats_completed() {
   g_ctrlm_voice.session_stats_in_progress = false;
   if(!is_vrex_req_in_progress() && !is_audio_stream_in_progress()) {
      ctrlm_voice_session_completed();
   }
}

void ctrlm_voice_session_completed() {
   g_ctrlm_voice.session_in_progress = false;
   g_ctrlm_voice.network_id          = CTRLM_HAL_NETWORK_ID_INVALID;
   g_ctrlm_voice.controller_id       = CTRLM_HAL_CONTROLLER_ID_INVALID;
}

gboolean is_voice_session_on_this_controller(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) {
   if(network_id == g_ctrlm_voice.network_id && controller_id == g_ctrlm_voice.controller_id) {
      return(true);
   }
   return(false);
}

void ctrlm_voice_enable_set(bool enable) {
   g_ctrlm_voice.voice_enabled = enable;
}

void ctrlm_voice_server_ready_set(bool enable) {
   g_ctrlm_voice.voice_server_ready = enable;
}

void ctrlm_voice_device_update_in_progress_set(bool in_progress) {

   g_ctrlm_voice.device_update_in_progress = in_progress;

   LOG_INFO("%s: Voice is <%s>\n", __FUNCTION__, g_ctrlm_voice.device_update_in_progress ? "DISABLED" : "ENABLED");
}

unsigned long ctrlm_voice_session_id_get_next() {
   return(++g_ctrlm_voice.vrex_session_id_global); // The calls that access this API all occur in a single context.  If that ever changes, it's semaphore time.
}
#ifndef USE_VOICE_SDK
bool ctrlm_voice_ind_voice_session_request(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_timestamp_t timestamp) {
   ctrlm_voice_session_abort_reason_t reason = CTRLM_VOICE_SESSION_ABORT_REASON_MAX;
   voice_session_response_status_t    status = VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK;

   // Attempt to acquire a session
   if(!set_voice_session_in_progress(true)) { // session in progress
      if(is_voice_session_on_this_controller(network_id, controller_id)) { // Session is with this controller. Tear current session down and start a new one.
         ctrlm_voice_ind_voice_session_end(network_id, controller_id, CTRLM_VOICE_SESSION_END_REASON_NEW_SESSION, 0, true);
//         reason = CTRLM_VOICE_SESSION_ABORT_REASON_NEW_SESSION;
//         status = VOICE_SESSION_RESPONSE_SERVER_NOT_READY;
      } else {
         LOG_WARN("%s: (%u, %u) session request aborted: BUSY\n", __FUNCTION__, network_id, controller_id);
         reason = CTRLM_VOICE_SESSION_ABORT_REASON_BUSY;
         status = VOICE_SESSION_RESPONSE_BUSY;
      }
   }
   #ifdef AUTH_ENABLED
   else if(!ctrlm_main_has_receiver_id_get() && !ctrlm_main_has_device_id_get()) { // has no receiver_id or device id
      LOG_WARN("%s: (%u, %u) session request aborted: NO RECEIVER ID\n", __FUNCTION__, network_id, controller_id);
      reason = CTRLM_VOICE_SESSION_ABORT_REASON_NO_RECEIVER_ID;
      status = VOICE_SESSION_RESPONSE_SERVER_NOT_READY;
   }
   #endif
   else if(!is_voice_enabled()) { // voice is disabled
      LOG_WARN("%s: (%u, %u) session request aborted: VOICE DISABLED\n", __FUNCTION__, network_id, controller_id);
      reason = CTRLM_VOICE_SESSION_ABORT_REASON_VOICE_DISABLED;
      status = VOICE_SESSION_RESPONSE_SERVER_NOT_READY;
   } else if(is_device_update_in_progress()) { // device update is in progress
      LOG_WARN("%s: (%u, %u) session request aborted: DEVICE UPDATE IN PROGRESS\n", __FUNCTION__, network_id, controller_id);
      reason = CTRLM_VOICE_SESSION_ABORT_REASON_DEVICE_UPDATE;
//      status = VOICE_SESSION_RESPONSE_BUSY;
      status = VOICE_SESSION_RESPONSE_SERVER_NOT_READY;
   } else if(!is_audio_format_supported()) { // unsupported audio format
      LOG_WARN("%s: (%u, %u) session request aborted: UNSUPPORTED AUDIO FORMAT\n", __FUNCTION__, network_id, controller_id);
      reason = CTRLM_VOICE_SESSION_ABORT_REASON_AUDIO_FORMAT;
      status = VOICE_SESSION_RESPONSE_UNSUPPORTED_AUDIO_FORMAT;
   } else if(!ctrlm_controller_id_is_valid(network_id, controller_id)) { // controller doesn't exist
      LOG_WARN("%s: (%u, %u) session request aborted: INVALID CONTROLLER ID\n", __FUNCTION__, network_id, controller_id);
      reason = CTRLM_VOICE_SESSION_ABORT_REASON_INVALID_CONTROLLER_ID;
      status = VOICE_SESSION_RESPONSE_FAILURE;
   }

   if(reason != CTRLM_VOICE_SESSION_ABORT_REASON_MAX) {
      ctrlm_voice_rsp_session_request(network_id, controller_id, timestamp, status, reason);
      if(reason != CTRLM_VOICE_SESSION_ABORT_REASON_BUSY) {
         ctrlm_voice_session_completed(); // Don't kill the session in progress
      }

      ctrlm_device_update_timeout_update_activity(network_id, controller_id);
      return(false);
   }

   voice_session_t *voice_session = ctrlm_main_voice_session_get();

   if(voice_session) {
      voice_session->stream_request(network_id, controller_id);
   }

   g_ctrlm_voice.network_id                = network_id;
   g_ctrlm_voice.controller_id             = controller_id;
   g_ctrlm_voice.vrex_req_in_progress      = true;
   g_ctrlm_voice.audio_stream_in_progress  = true;
   g_ctrlm_voice.session_stats_in_progress = true;

   #ifdef TIMING_START_TO_FIRST_FRAGMENT
   ctrlm_timestamp_get(&voice_session_begin_timestamp);
   #endif

   if(g_ctrlm_voice.controller_id_last == controller_id && (g_ctrlm_voice.stop_reason_last == CTRLM_VOICE_SESSION_END_REASON_TIMEOUT_INTERPACKET || g_ctrlm_voice.stop_reason_last == CTRLM_VOICE_SESSION_END_REASON_TIMEOUT_FIRST_PACKET)) { // Only trigger if session was in progress, ended with interpacket timeout, and haven't already sent reboot stats
      ctrlm_voice_notify_stats_reboot(network_id, controller_id, CTRLM_VOICE_RESET_TYPE_OTHER, 0xFF, 0);
   }

   // Set voice command status to pending
#ifndef USE_VOICE_SDK
   ctrlm_voice_cmd_status_set(network_id, controller_id, VOICE_COMMAND_STATUS_PENDING);
#endif

   // Reset to start of buffer
   g_ctrlm_voice.utterance_buffer_index_write = 0;
   g_ctrlm_voice.utterance_buffer_index_read  = 0;

   LOG_INFO("%s: (%u, %u) session request ACCEPTED\n", __FUNCTION__, network_id, controller_id);

   #ifdef VOICE_BUFFER_STATS
   voice_buffer_fragment_qty      = 0;
   voice_buffer_chunk_qty         = 0;
   voice_buffer_frag_time_sum     = 0;
   voice_buffer_frag_time_max     = 0;
   voice_buffer_frag_time_min     = 0xFFFFFFFFFFFFFFFF;
   voice_buffer_chunk_time_sum    = 0;
   voice_buffer_chunk_time_max    = 0;
   voice_buffer_chunk_time_min    = 0xFFFFFFFFFFFFFFFF;
   voice_buffer_warning_triggered = 0;
   voice_buffer_high_watermark    = 0;
   #endif
   voice_session_rf_channel       = 15; //gpRf4ce_GetBaseChannel();
   voice_session_lqi_total        = 0;

   #ifdef DYNAMIC_PRIORITY
   AppVoice_SetPriority(); // Temporarily set a higher priority
   #endif

   voice_packet_analysis_obj->reset();

   #ifndef MASK_RAPID_KEYPRESS
   ctrlm_voice_session_begin(network_id, controller_id);
   #endif

   ctrlm_voice_rsp_session_request(network_id, controller_id, timestamp, status, reason);

   ctrlm_device_update_timeout_update_activity(network_id, controller_id);
   return(true);
}
#endif

#ifndef USE_VOICE_SDK
void ctrlm_voice_rsp_session_request(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_timestamp_t timestamp, voice_session_response_status_t status, ctrlm_voice_session_abort_reason_t reason) {
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_session_request_t msg;
   errno_t safec_rc = memset_s(&msg, sizeof(msg), 0, sizeof(msg));
   ERR_CHK(safec_rc);

   msg.header.network_id = network_id;
   msg.controller_id     = controller_id;
   msg.timestamp         = timestamp;
   msg.status            = status;
   msg.reason            = reason;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::ind_process_voice_session_request, &msg, sizeof(msg), NULL, network_id);
}

void ctrlm_obj_network_rf4ce_t::ind_process_voice_session_request(void *data, int size) {
   ctrlm_main_queue_msg_voice_session_request_t *dqm = (ctrlm_main_queue_msg_voice_session_request_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_request_t));

   THREAD_ID_VALIDATE();
   if(!controller_exists(dqm->controller_id)) {
      LOG_INFO("%s: invalid controller id (%u)\n", __FUNCTION__, dqm->controller_id);
      return;
   }

   #ifdef AUTH_ENABLED
   if(!ctrlm_main_has_device_id_get()) {
      LOG_INFO("%s: No device id\n", __FUNCTION__);
      // Purposely fall through, it's possible that only receiver id is available
   }
   if(!ctrlm_main_has_receiver_id_get()) {
      LOG_INFO("%s: No receiver id\n", __FUNCTION__);
      return;
   }
   if(!ctrlm_main_has_service_account_id_get()) {
      LOG_INFO("%s: No service account id\n", __FUNCTION__);
      return;
   }
   if(!ctrlm_main_has_partner_id_get()) {
      LOG_INFO("%s: No partner id\n", __FUNCTION__);
      return;
   }
   if(!ctrlm_main_has_experience_get()) {
      LOG_INFO("%s: No experience Tag\n", __FUNCTION__);
      return;
   }
   if(ctrlm_main_needs_service_access_token_get()) {
      LOG_INFO("%s: No service access token\n", __FUNCTION__);
      return;
   }
   #endif

   LOG_INFO("%s: processing session request\n", __FUNCTION__);
   
   ctrlm_device_update_timeout_update_activity(dqm->header.network_id, dqm->controller_id);

   // Create session object
//   if(NULL == controllers_[dqm->controller_id]->voice_session_create()) {
//      LOG_ERROR("%s: Unable to create voice session object!\n", __FUNCTION__);
//      dqm->status = VOICE_SESSION_RESPONSE_FAILURE;
//   }

   g_ctrlm_voice.controller_type = ctrlm_controller_type_get(dqm->controller_id);

   // Send the response back to the HAL device
   guchar response[2];

   response[0] = MSO_VOICE_CMD_ID_VOICE_SESSION_RESPONSE;
   response[1] = dqm->status;

   // Determine when to send the response (50 ms after receipt)
   ctrlm_timestamp_add_ms(&dqm->timestamp, CTRLM_RF4CE_CONST_RESPONSE_IDLE_TIME);

   req_data(CTRLM_RF4CE_PROFILE_ID_VOICE, dqm->controller_id, dqm->timestamp, 2, response, NULL, NULL);

   LOG_INFO("%s: session response delivered\n", __FUNCTION__);

   if(dqm->status != VOICE_SESSION_RESPONSE_AVAILABLE && dqm->status != VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK) { // Session was aborted
      LOG_INFO("%s: voice session abort\n", __FUNCTION__);

      // Broadcast the event over the iarm bus
      ctrlm_voice_iarm_event_session_abort_t event;
      event.api_revision  = CTRLM_VOICE_IARM_BUS_API_REVISION;
      event.network_id    = dqm->header.network_id;
      event.network_type  = ctrlm_network_type_get(dqm->header.network_id);
      event.controller_id = dqm->controller_id;
      event.session_id    = ctrlm_voice_session_id_get_next();
      event.reason        = dqm->reason;

      ctrlm_voice_iarm_event_session_abort(&event);
   } else {
      // Start the first voice fragment timer
      g_ctrlm_voice.timeout_tag_inactivity = ctrlm_timeout_create(g_ctrlm_voice.vrex_prefs.timeout_packet_initial, ctrlm_voice_timeout_inactivity, NULL);

   }
}
#endif

ctrlm_hal_frequency_agility_t ctrlm_voice_ind_voice_session_end(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_voice_session_end_reason_t reason, guint8 key_code, bool force) {
   unsigned char utterance_too_short = 0;
   unsigned char link_quality        = 0;

   LOG_INFO("%s: (%u, %u) reason <%s> key code 0x%02X <%s>\n", __FUNCTION__, network_id, controller_id, ctrlm_voice_session_end_reason_str(reason), key_code, ctrlm_key_code_str((ctrlm_key_code_t)key_code));

   if(!is_voice_session_in_progress() && !force) { // session NOT in progress
      LOG_ERROR("%s: voice session not in progress\n", __FUNCTION__);
      return(CTRLM_HAL_FREQUENCY_AGILITY_ENABLE);
   }
   if(!is_voice_session_on_this_controller(network_id, controller_id)) {
      LOG_ERROR("%s: voice session not on this controller %u\n", __FUNCTION__, controller_id);
      return(CTRLM_HAL_FREQUENCY_AGILITY_NO_CHANGE);
   }
   if(!is_audio_stream_in_progress() && !force) { // session NOT in progress
      LOG_ERROR("%s: voice audio streaming not in progress\n", __FUNCTION__);
      return(CTRLM_HAL_FREQUENCY_AGILITY_NO_CHANGE);
   }


#ifdef TIMING_LAST_FRAGMENT_TO_STOP
   ctrlm_timestamp_t after;
   ctrlm_timestamp_get(&after);
   unsigned long long lag_time = ctrlm_timestamp_subtract_ns(voice_session_last_fragment_timestamp, after);
   LOG_INFO("Last Fragment to Session Stop: %8llu ms lag\n", lag_time / 1000000);
#endif

   // Store the last stop reason
   g_ctrlm_voice.stop_reason_last   = reason;
   g_ctrlm_voice.controller_id_last = controller_id;

   // Check adjacent key press
   if(CTRLM_VOICE_SESSION_END_REASON_OTHER_KEY_PRESSED == g_ctrlm_voice.stop_reason_last && true == ctrlm_is_key_adjacent(network_id, controller_id, key_code)) {
      g_ctrlm_voice.stop_reason_last = CTRLM_VOICE_SESSION_END_REASON_ADJACENT_KEY_PRESSED;
      LOG_INFO("%s: Adjacent key press.  Modifying end reason.\n", __FUNCTION__);
   }

#ifdef MASK_RAPID_KEYPRESS
   if(g_ctrlm_voice.utterance_buffer_index_read == 0) { // Finalize session because vrex manager was not informed of the session
#ifndef USE_VOICE_SDK
      ctrlm_voice_cmd_status_set(network_id, controller_id, VOICE_COMMAND_STATUS_FAILURE);
#endif
      utterance_too_short = 1;
   } else // Only send stop if start was called
#endif
   {
      // Write any remaining data before sending session stop
      guint32 chunk_size = g_ctrlm_voice.utterance_buffer_index_write - g_ctrlm_voice.utterance_buffer_index_read;
      if(chunk_size > 0) {
         ctrlm_voice_session_fragment(controller_id, &g_ctrlm_voice.utterance_buffer[g_ctrlm_voice.utterance_buffer_index_read], chunk_size);
         g_ctrlm_voice.utterance_buffer_index_read += chunk_size;
      }
   }

   ctrlm_voice_packet_analysis_stats_t stats;
   voice_packet_analysis_obj->stats_get(stats);

   if(0 == stats.total_packets) {
      LOG_INFO("%s: short mic keypress\n", __FUNCTION__);
   }

   if(((long)stats.total_packets - (long)stats.lost_packets) > 0) {
      link_quality = (unsigned char) (voice_session_lqi_total / (stats.total_packets - stats.lost_packets));
   }

   ctrlm_voice_audio_stream_completed();

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_session_end_t msg = {0};

   msg.controller_id         = controller_id;
   msg.reason                = reason;
   msg.stop_data             = key_code;
   msg.utterance_too_short   = utterance_too_short;
   #ifdef VOICE_BUFFER_STATS
   msg.buffer_high_watermark = voice_buffer_high_watermark;
   #else
   msg.buffer_high_watermark = ~0;
   #endif
   msg.session_packets_total = stats.total_packets;
   msg.session_packets_lost  = stats.lost_packets;

   msg.session_rf_channel    = voice_session_rf_channel;
   msg.session_link_quality  = link_quality;

   if(0 == utterance_too_short) {  // Utterance was NOT discarded due to minimum length requirements
      if(g_ctrlm_voice.data_pipe_write < 0) {
         LOG_INFO("%s: no voice session in progress\n", __FUNCTION__);
         return(CTRLM_HAL_FREQUENCY_AGILITY_ENABLE);
      }

      LOG_INFO("%s: Voice Data Bufd <%lu>\n", __FUNCTION__, g_ctrlm_voice.data_qty_bufd);

      // Send the write side of the pipe to be closed by control manager
      msg.voice_data_pipe_write = g_ctrlm_voice.data_pipe_write;
      msg.session_id            = 0;

      // Clear global write pipe to prevent further writes
      g_ctrlm_voice.data_pipe_write = -1;
      g_ctrlm_voice.data_qty_bufd   =  0;

      ctrlm_voice_iarm_event_session_end(network_id, controller_id, g_ctrlm_voice.vrex_session_id, g_ctrlm_voice.stop_reason_last);
   } else {
      msg.voice_data_pipe_write = -1;
      msg.session_id            = ctrlm_voice_session_id_get_next(); // Need to increment session id for short utterances so the stats use the same id
      ctrlm_voice_vrex_req_completed(false, "");
   }

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::ind_process_voice_session_end, &msg, sizeof(msg), NULL, network_id);

   // No error code is returned for the message push
   //if(0) {
   //   LOG_INFO("%s: voice stop dropped\n", __FUNCTION__);
   //   if(0 == msg->utterance_too_short) {
   //      // Close the write side of the pipe
   //      close(msg->voice_data_pipe_write);
   //      // Close the read side of the pipe (need to coordinate with session thread)
   //   }
   //}

   #ifdef DYNAMIC_PRIORITY
   AppVoice_RestorePriority(); // Restore normal priority
   #endif

   if(g_ctrlm_voice.vrex_prefs.utterance_save) {
      // Write the contents of the buffer to a file
      if(TRUE != g_file_set_contents(g_ctrlm_voice.vrex_prefs.utterance_path.c_str(), (const gchar *)g_ctrlm_voice.utterance_buffer, g_ctrlm_voice.utterance_buffer_index_write, NULL)) {
         LOG_ERROR("%s: Unable to open/write to file\n", __FUNCTION__);
      }
   }

   LOG_INFO("%s: session stop (%u, %u) : Errors Seq <%u> Dup <%u>\n", __FUNCTION__, network_id, controller_id, stats.sequence_error_count, stats.duplicated_packets);
   LOG_INFO("%s: Packets Total <%u> Lost <%u> RF Chan <%lu> LQI <%u>\n", __FUNCTION__, stats.total_packets, stats.lost_packets, voice_session_rf_channel, link_quality);

   #ifdef VOICE_BUFFER_STATS
   if(voice_buffer_fragment_qty) {
      LOG_INFO("%s: fragment qty %5lu avg %9llu max %9llu min %9llu ns\n", __FUNCTION__, voice_buffer_fragment_qty, (voice_buffer_frag_time_sum / voice_buffer_fragment_qty), voice_buffer_frag_time_max, voice_buffer_frag_time_min);
   }
   if(voice_buffer_chunk_qty) {
      LOG_INFO("%s: chunk    qty %5lu avg %9llu max %9llu min %9llu ns\n", __FUNCTION__, voice_buffer_chunk_qty, (voice_buffer_chunk_time_sum / voice_buffer_chunk_qty), voice_buffer_chunk_time_max, voice_buffer_chunk_time_min);
   }
   #endif

   return(CTRLM_HAL_FREQUENCY_AGILITY_ENABLE);
}

void ctrlm_voice_iarm_event_session_end(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, unsigned long session_id, ctrlm_voice_session_end_reason_t reason) {
   ctrlm_voice_iarm_event_session_end_t event;
   event.api_revision       = CTRLM_VOICE_IARM_BUS_API_REVISION;
   event.network_id         = network_id;
   event.network_type       = ctrlm_network_type_get(network_id);
   event.controller_id      = controller_id;
   event.session_id         = session_id;
   event.reason             = reason;
   event.is_voice_assistant = ctrlm_is_voice_assistant(g_ctrlm_voice.controller_type);

   ctrlm_voice_iarm_event_session_end(&event);
}

void ctrlm_voice_ind_voice_session_fragment(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, unsigned char command_id, unsigned long data_length, guchar *data, ctrlm_hal_rf4ce_data_read_t cb_data_read, void *cb_data_param, unsigned char lqi) {
   //Updates the background update timeout so background update doesn't stop on long voice commands
   if(command_id == BACKGROUND_UPDATE_TIMEOUT_COMMAND_ID){
      ctrlm_device_update_timeout_update_activity(network_id, controller_id);
   }

   if(!is_voice_session_in_progress()) {
      LOG_ERROR("%s: no voice session in progress\n", __FUNCTION__);
      return;
   }
   if(!is_voice_session_on_this_controller(network_id, controller_id)) {
      LOG_ERROR("%s: voice session is not in progress for this controller %u\n", __FUNCTION__, controller_id);
      return;
   }
   if(!is_audio_stream_in_progress()) {
      LOG_ERROR("%s: no audio streaming in progress\n", __FUNCTION__);
      return;
   }

   guint32 chunk_size = g_ctrlm_voice.utterance_buffer_index_write + data_length - g_ctrlm_voice.utterance_buffer_index_read;
   ctrlm_voice_packet_analysis_result_t analysis_result = voice_packet_analysis_obj->packet_check(&command_id, sizeof (command_id), &g_ctrlm_voice.utterance_buffer[g_ctrlm_voice.utterance_buffer_index_write], chunk_size);

   // Remove the pending timeout and start again at the end of the function
   ctrlm_timeout_destroy(&g_ctrlm_voice.timeout_tag_inactivity);

   if ((CTRLM_VOICE_PACKET_ANALYSIS_ERROR == analysis_result) || (CTRLM_VOICE_PACKET_ANALYSIS_DUPLICATE == analysis_result)) {
      return; // don't propagate repeated voice packets or analysis errors
   }

   #if defined(VOICE_BUFFER_STATS)
   ctrlm_timestamp_t before, after;
   ctrlm_timestamp_get(&before);
   #ifdef TIMING_LAST_FRAGMENT_TO_STOP
   voice_session_last_fragment_timestamp = before;
   #endif

   ctrlm_voice_packet_analysis_stats_t stats;
   voice_packet_analysis_obj->stats_get(stats);
   if(stats.total_packets == 0) {
       first_fragment = before; // timestamp for start of utterance voice data
       #ifdef TIMING_START_TO_FIRST_FRAGMENT
       LOG_INFO("Session Start to First Fragment: %8lld ms lag\n", ctrlm_timestamp_subtract_ms(voice_session_begin_timestamp, first_fragment));
       #endif
   }
   #endif

   voice_session_lqi_total += lqi;

   if(g_ctrlm_voice.utterance_buffer_index_write + data_length > UTTERANCE_BUFFER_SIZE) {
       LOG_INFO("voice data local buffer too small : %lu %d\n", g_ctrlm_voice.utterance_buffer_index_write + data_length, UTTERANCE_BUFFER_SIZE);
       return;
   }
   // Copy the voice packet to the local buffer
   if(data != NULL) {
      errno_t safec_rc = memcpy_s(&g_ctrlm_voice.utterance_buffer[g_ctrlm_voice.utterance_buffer_index_write], sizeof(g_ctrlm_voice.utterance_buffer), data, data_length);
      ERR_CHK(safec_rc);
   } else {
      if(data_length != cb_data_read(data_length, &g_ctrlm_voice.utterance_buffer[g_ctrlm_voice.utterance_buffer_index_write], cb_data_param)) {
         LOG_ERROR("%s: unable to read voice fragment data\n", __FUNCTION__);
      }
   }

   g_ctrlm_voice.utterance_buffer_index_write += data_length;
   // Write chunk when chunk size is reached
   if(g_ctrlm_voice.utterance_buffer_index_read == 0) { // First chunk
       if(chunk_size >= g_ctrlm_voice.chunk_size_first) {
           #ifdef MASK_RAPID_KEYPRESS
           ctrlm_voice_session_begin(network_id, controller_id);
           #endif
           ctrlm_voice_session_fragment(controller_id, &g_ctrlm_voice.utterance_buffer[g_ctrlm_voice.utterance_buffer_index_read], chunk_size);
           g_ctrlm_voice.utterance_buffer_index_read  += chunk_size;
       }
   } else { // subsequent chunks
       if(chunk_size >= VOICE_CHUNK_SIZE_SUBSEQUENT) {
           ctrlm_voice_session_fragment(controller_id, &g_ctrlm_voice.utterance_buffer[g_ctrlm_voice.utterance_buffer_index_read], chunk_size);
           g_ctrlm_voice.utterance_buffer_index_read  += chunk_size;
       }
   }

   #ifdef VOICE_BUFFER_STATS
   ctrlm_timestamp_get(&after);
   unsigned long long actual_time = ctrlm_timestamp_subtract_ns(before, after);
   unsigned long long session_time = VOICE_PACKET_TIME + ctrlm_timestamp_subtract_us(first_fragment, before); // in microseconds

   voice_packet_analysis_obj->stats_get(stats);
   unsigned long long session_delta = (session_time - ((stats.total_packets - 1) * VOICE_PACKET_TIME)); // in microseconds
   unsigned long watermark = (session_delta / VOICE_PACKET_TIME) + 1;
   if(watermark > voice_buffer_high_watermark) {
      voice_buffer_high_watermark = watermark;
   }
   if(session_delta > VOICE_BUFFER_WARNING_THRESHOLD) {
       voice_buffer_warning_triggered = 1;
   }
   if(voice_buffer_warning_triggered) {
       LOG_INFO("BUFL %3u (%8llu ns): %8llu ms lag %8llu ms (%4.2f packets)\n", stats.total_packets, actual_time, session_time / 1000, session_delta / 1000, (((float)session_delta) / VOICE_PACKET_TIME));

   }
   if(g_ctrlm_voice.utterance_buffer_index_read == g_ctrlm_voice.utterance_buffer_index_write) { // Sent a chunk to vrex manager
       voice_buffer_chunk_qty++;
       voice_buffer_chunk_time_sum += actual_time;
       if(actual_time > voice_buffer_chunk_time_max) { voice_buffer_chunk_time_max = actual_time; }
       if(actual_time < voice_buffer_chunk_time_min) { voice_buffer_chunk_time_min = actual_time; }
   } else { // Stored the fragment
       voice_buffer_fragment_qty++;
       voice_buffer_frag_time_sum += actual_time;
       if(actual_time > voice_buffer_frag_time_max) { voice_buffer_frag_time_max = actual_time; }
       if(actual_time < voice_buffer_frag_time_min) { voice_buffer_frag_time_min = actual_time; }
   }
   #endif

   // Start the inactivity timer again
   g_ctrlm_voice.timeout_tag_inactivity = ctrlm_timeout_create(g_ctrlm_voice.vrex_prefs.timeout_packet_subsequent, ctrlm_voice_timeout_inactivity, NULL);
}

void ctrlm_voice_session_fragment(ctrlm_controller_id_t controller_id, guchar *data, unsigned long len) {
   //LOG_INFO("%s: enter", __FUNCTION__);
   if(len == 0) {
      LOG_INFO("%s: ERROR Speech Fragment Event with zero length!\n", __FUNCTION__);
      return;
   }

   g_ctrlm_voice.data_qty_bufd += len;

   // Send voice fragment to the pipe
   if(g_ctrlm_voice.data_pipe_write >= 0) {
      #ifdef CTRLM_VOICE_BUFFER_LEVEL_DEBUG
      LOG_INFO("+<%5lu,%5lu>\n", len, g_ctrlm_voice.data_qty_bufd);
      #endif
      ssize_t rc = write(g_ctrlm_voice.data_pipe_write, data, len);
      if((unsigned long)rc != len) {
         int errsv = errno;
         LOG_INFO("%s: voice data dropped (rc = %d) <%s>\n", __FUNCTION__, rc, (rc < 0) ? strerror(errsv) : "");
      }
   } else {
      #ifdef CTRLM_VOICE_BUFFER_LEVEL_DEBUG
      LOG_INFO("+<%5lu,%5lu> PIPE CLOSED\n", len, g_ctrlm_voice.data_qty_bufd);
      #endif
   }
}

void ctrlm_voice_session_begin(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) {
   LOG_INFO("%s: voice session begin\n", __FUNCTION__);
   errno_t safec_rc = -1;
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_session_begin_t msg = {0};

   unsigned long session_id = ctrlm_voice_session_id_get_next();
   g_ctrlm_voice.vrex_session_id = session_id;
   LOG_INFO("%s: voice session id %lu\n", __FUNCTION__, g_ctrlm_voice.vrex_session_id);

   ctrlm_voice_iarm_event_session_begin(network_id, controller_id, session_id);

   if(g_ctrlm_voice.data_pipe_write >= 0) {
      LOG_INFO("%s: closing stale voice data pipe\n", __FUNCTION__);
      close(g_ctrlm_voice.data_pipe_write);
      g_ctrlm_voice.data_pipe_write = -1;
   }

   // Create the voice data pipe
   int pipefd[2];
   // Create the pipe
   if(pipe(pipefd) < 0) {
      LOG_INFO("%s: Error - unable to create pipe!\n", __FUNCTION__);
      return;
   }
   // Set write side of pipe to non-blocking.  The read side must remain blocking for curl.
   int flags = fcntl(pipefd[1], F_GETFL) | O_NONBLOCK;
   if(fcntl(pipefd[1], F_SETFL, flags) < 0) {
      LOG_INFO("%s: Error - unable to set pipe write as non-blocking!\n", __FUNCTION__);
      close(pipefd[0]);
      close(pipefd[1]);
      return;
   }
   g_ctrlm_voice.data_pipe_write = pipefd[1];

   msg.controller_id        = controller_id;
   msg.voice_data_pipe_read = pipefd[0];
   msg.session_id           = session_id;

   safec_rc = strcpy_s((char *)msg.mime_type, sizeof(msg.mime_type), CTRLM_VOICE_MIMETYPE_ADPCM);
   ERR_CHK(safec_rc);
   safec_rc = strcpy_s((char *)msg.sub_type,  sizeof(msg.sub_type), CTRLM_VOICE_SUBTYPE_ADPCM);
   ERR_CHK(safec_rc);
   safec_rc = strcpy_s((char *)msg.language,  sizeof(msg.language), CTRLM_VOICE_LANGUAGE);
   ERR_CHK(safec_rc);

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::ind_process_voice_session_begin, &msg, sizeof(msg), NULL, network_id);

   // the message push doesn't have a return code...
   //if(0) { // Error sending message
   //   LOG_ERROR("%s: voice begin dropped\n", __FUNCTION__);
   //   close(pipefd[0]);
   //   close(pipefd[1]);
   //   g_ctrlm_voice.data_pipe_write = -1;
   //   g_free(msg);
   //}
}

void ctrlm_voice_iarm_event_session_begin(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, unsigned long session_id) {
   errno_t safec_rc = -1;
   ctrlm_voice_iarm_event_session_begin_t event;

   // create IARM structure
   event.api_revision        = CTRLM_VOICE_IARM_BUS_API_REVISION;
   event.network_id          = network_id;
   event.network_type        = ctrlm_network_type_get(network_id);
   event.controller_id       = controller_id;
   event.session_id          = session_id;
   event.is_voice_assistant  = ctrlm_is_voice_assistant(g_ctrlm_voice.controller_type);

   safec_rc = strcpy_s((char *)event.mime_type, sizeof(event.mime_type), CTRLM_VOICE_MIMETYPE_ADPCM);
   ERR_CHK(safec_rc);
   safec_rc = strcpy_s((char *)event.sub_type,  sizeof(event.sub_type), CTRLM_VOICE_SUBTYPE_ADPCM);
   ERR_CHK(safec_rc);
   safec_rc = strcpy_s((char *)event.language,  sizeof(event.language), CTRLM_VOICE_LANGUAGE);
   ERR_CHK(safec_rc);

   ctrlm_voice_iarm_event_session_begin(&event);
}

void ctrlm_obj_network_rf4ce_t::ind_process_voice_session_begin(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_voice_session_begin_t *dqm = (ctrlm_main_queue_msg_voice_session_begin_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_begin_t));

   LOG_INFO("%s: Voice Begin Event\n", __FUNCTION__);
   ctrlm_controller_id_t controller_id = dqm->controller_id;

   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Controller object doesn't exist for controller id %u!\n", __FUNCTION__, controller_id);
      return;
   }

   voice_session_t *voice_session = ctrlm_main_voice_session_get();

   if(voice_session == NULL) {
      LOG_ERROR("%s: Unable to get voice session object!\n", __FUNCTION__);
      return;
   }

   vrex_mgr_audio_info_t audio_info;
   audio_info.mime_type = (char *)dqm->mime_type;
   audio_info.sub_type  = (char *)dqm->sub_type;
   audio_info.language  = (char *)dqm->language;

   // If not defined set it to English
   if(audio_info.language.empty()) {
      audio_info.language = CTRLM_VOICE_LANGUAGE;
   }
   if(audio_info.sub_type.empty()) {
      audio_info.sub_type = CTRLM_VOICE_SUBTYPE_ADPCM; // This value ends up in the request url so it cannot be empty
   }

   #ifdef AUTH_ENABLED
   if(g_ctrlm_voice.vrex_prefs.sat_enabled) {
      // This call will check validity of the token and request a new one if needed
      if(false == ctrlm_voice_is_service_access_token_valid()) {
         LOG_INFO("%s: unable to acquire the service access token!\n", __FUNCTION__);
      }
   }
   #endif

   auto controller_ptr = controllers_.find(controller_id);
   if (controller_ptr == controllers_.end()) {
      LOG_ERROR("%s: Controller %u doesn't exists!\n", __FUNCTION__, controller_id);
      return;
   }
#ifdef CTRLM_NETWORK_RF4CE
   ctrlm_obj_controller_rf4ce_t& controller = *controller_ptr->second;
#endif

   stream_begin_params_t params;
   params.network_id = network_id_get();
   params.controller_id = controller_id;
   params.audio_info = audio_info;
   params.conversation_id = g_ctrlm_voice.conversation_id;
   params.voice_data_pipe_read = dqm->voice_data_pipe_read;
   params.service_access_token = g_ctrlm_voice.service_access_token;
   params.session_id = dqm->session_id;
   params.timeout_vrex_request = g_ctrlm_voice.vrex_prefs.timeout_vrex_request;
#ifdef CTRLM_NETWORK_RF4CE
   params.user_string = controller.product_name_get();
   params.sw_version = controller.version_software_get();
   params.hw_version = controller.version_hardware_get();
   params.battery_voltage = controller.battery_status_get().voltage_loaded;
   params.battery_percentage = ctrlm_battery_level_percent(controller_id, params.battery_voltage);
#endif


   LOG_INFO("%s: Audio Info: mime: <%s>, sub_type: <%s>, language: <%s>\n", __FUNCTION__, audio_info.mime_type.c_str(), audio_info.sub_type.c_str(), audio_info.language.c_str());
   if(!voice_session->stream_begin(params)) {
      LOG_INFO("%s: stream_begin failed\n", __FUNCTION__);
      // Clear global write pipe
      int voice_data_pipe_write = g_ctrlm_voice.data_pipe_write;
      g_ctrlm_voice.data_pipe_write = -1;
      // Delay a short time in case data was in the process of being written (do NOT want to use any blocking primitives to synchronize this)
      usleep(50000);
      // Close both sides of the pipe
      LOG_INFO("%s: close read and write side of the pipe\n", __FUNCTION__);
      close(voice_data_pipe_write);
      close(dqm->voice_data_pipe_read);
      ctrlm_voice_vrex_req_completed(false, "");
   }
}

#ifndef USE_VOICE_SDK
void ctrlm_obj_network_rf4ce_t::ind_process_voice_session_end(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_voice_session_end_t *dqm = (ctrlm_main_queue_msg_voice_session_end_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_end_t));

   ctrlm_controller_id_t controller_id = dqm->controller_id;
   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Controller object doesn't exist for controller id %u!\n", __FUNCTION__, controller_id);
      return;
   }

   voice_session_t *voice_session = ctrlm_main_voice_session_get();

   if(NULL == voice_session) {
      LOG_INFO("%s: Invalid session\n", __FUNCTION__);
      return;
   }

   // Remove the pending timeout
   ctrlm_timeout_destroy(&g_ctrlm_voice.timeout_tag_inactivity);

   voice_session->stream_end(dqm->reason, dqm->stop_data, dqm->voice_data_pipe_write, dqm->session_rf_channel, dqm->buffer_high_watermark, dqm->session_packets_total, dqm->session_packets_lost, dqm->session_link_quality, dqm->session_id);

   ctrlm_rf4ce_voice_utterance_type_t voice_utterance_type;
   ctrlm_voice_packet_analysis_stats_t stats;
   voice_packet_analysis_obj->stats_get(stats);
   if( voice_session->get_utterance_too_short() == 0) {
      voice_utterance_type = RF4CE_VOICE_NORMAL_UTTERANCE;
   } else {
      voice_utterance_type = RF4CE_VOICE_SHORT_UTTERANCE;
   }
   controllers_[controller_id]->update_voice_metrics(voice_utterance_type, stats.total_packets, stats.lost_packets);
   
   //Voice command will update the last key info
   ctrlm_update_last_key_info(controller_id, IARM_BUS_IRMGR_KEYSRC_RF, KED_PUSH_TO_TALK, controllers_[controller_id]->product_name_get(), false, true);

   //Update the time last key
   controllers_[controller_id]->time_last_key_update();

   if(!is_vrex_req_in_progress()) {
      // Send the notification
      voice_session->notify_result();
   }

   if(dqm->reason != CTRLM_VOICE_SESSION_END_REASON_DONE && dqm->reason != CTRLM_VOICE_SESSION_END_REASON_ADJACENT_KEY_PRESSED && dqm->reason != CTRLM_VOICE_SESSION_END_REASON_OTHER_KEY_PRESSED) {
      ctrlm_hal_network_property_frequency_agility_t property;
      property.state = CTRLM_HAL_FREQUENCY_AGILITY_ENABLE;
      // if the stop command was not received, turn channel hopping back on (should we wait for Maximum Voice Utterance Length RIB entry plus 3,000 ms as the spec says? TODO)
      property_set(CTRLM_HAL_NETWORK_PROPERTY_FREQUENCY_AGILITY, (void *)&property);
   }

   // Schedule the stats timer to send a dummy status message if no statistics are received
   g_ctrlm_voice.timeout_tag_stats = ctrlm_timeout_create(g_ctrlm_voice.vrex_prefs.timeout_stats, ctrlm_voice_timeout_stats, NULL);
}
#endif

void ctrlm_obj_network_rf4ce_t::ind_process_voice_session_stats(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_voice_session_stats_t *dqm = (ctrlm_main_queue_msg_voice_session_stats_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_stats_t));

   ctrlm_controller_id_t controller_id = dqm->controller_id;

   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Controller object doesn't exist for controller id %u!\n", __FUNCTION__, controller_id);
      ctrlm_voice_session_stats_completed();
      return;
   }

   voice_session_t *voice_session = ctrlm_main_voice_session_get();

   if(NULL == voice_session) {
      LOG_INFO("%s: Invalid session\n", __FUNCTION__);
      ctrlm_voice_session_stats_completed();
      return;
   }

   voice_session->stream_stats(dqm->session, dqm->reboot);

   ctrlm_voice_session_stats_completed();
}

ctrlm_hal_result_t ctrlm_voice_ind_data_btle(unsigned long data_length, unsigned char *p_data, void *p_user) {
   return(CTRLM_HAL_RESULT_ERROR);
}

ctrlm_hal_result_t ctrlm_voice_ind_data_ip(unsigned long data_length, unsigned char *p_data, void *p_user) {
   return(CTRLM_HAL_RESULT_ERROR);
}

gboolean ctrlm_voice_timeout_inactivity(gpointer user_data) {
   if(!is_voice_session_in_progress()) {
      LOG_WARN("%s: Inactivity Timeout - no session in progress\n", __FUNCTION__);
   } else {
      ctrlm_network_id_t network_id = g_ctrlm_voice.network_id;
      LOG_INFO("%s: Inactivity Timeout - Network Id %u\n", __FUNCTION__, network_id);

      // Close the voice session due to timeout
      ctrlm_voice_packet_analysis_stats_t stats;
      voice_packet_analysis_obj->stats_get(stats);
      if(stats.total_packets == 0) {
         ctrlm_voice_ind_voice_session_end(network_id, g_ctrlm_voice.controller_id, CTRLM_VOICE_SESSION_END_REASON_TIMEOUT_FIRST_PACKET, 0);
      } else {
         ctrlm_voice_ind_voice_session_end(network_id, g_ctrlm_voice.controller_id, CTRLM_VOICE_SESSION_END_REASON_TIMEOUT_INTERPACKET, 0);
      }
      // Re-enable channel hopping
      ctrlm_hal_network_property_frequency_agility_t property;
      property.state = CTRLM_HAL_FREQUENCY_AGILITY_ENABLE;
      ctrlm_network_property_set(network_id, CTRLM_HAL_NETWORK_PROPERTY_FREQUENCY_AGILITY, (void *)&property, sizeof(property));
   }
   g_ctrlm_voice.timeout_tag_inactivity = 0;

   return(FALSE);
};

#ifndef USE_VOICE_SDK
gboolean ctrlm_voice_timeout_stats(gpointer user_data) {
   if(!is_voice_session_in_progress()) {
      LOG_WARN("%s: Stats Timeout - no session in progress\n", __FUNCTION__);
   } else {
      LOG_INFO("%s: Stats Timeout \n", __FUNCTION__);
      ctrlm_voice_notify_stats_none(g_ctrlm_voice.network_id, g_ctrlm_voice.controller_id);
   }

   // Unschedule stats timer
   ctrlm_timeout_destroy(&g_ctrlm_voice.timeout_tag_stats);

   return(FALSE);
}

void ctrlm_voice_notify_stats_none(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) {
   LOG_WARN("%s: (%u, %u)\n", __FUNCTION__, network_id, controller_id);

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_session_stats_t msg = {0};

   msg.controller_id          = controller_id;
   msg.reboot.available       = 0;
   msg.session.available      = 0;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::ind_process_voice_session_stats, &msg, sizeof(msg), NULL, network_id);
}
#endif

std::string ctrlm_voice_service_access_token_string_get() {
  return g_ctrlm_voice.service_access_token;
}


bool ctrlm_voice_service_access_token_get() {
   #ifdef AUTH_ENABLED
   ctrlm_auth_t *authservice = ctrlm_main_auth_get();
   if(false == authservice->get_sat(g_ctrlm_voice.service_access_token, g_ctrlm_voice.service_access_token_expiration)) {
       LOG_ERROR("%s: failed to acquire service access token\n", __FUNCTION__);
       return(false);
   }
   #else
   LOG_INFO("%s: auth service is disabled\n", __FUNCTION__);
   return(false);
   #endif

   #if defined(SAT_CORRUPT_TOKEN_TEST_FIRST) ||defined(SAT_CORRUPT_TOKEN_TEST_SECOND) || defined(SAT_CORRUPT_TOKEN_TEST_THIRD)
   LOG_INFO("%s: WARNING we purposely corrupted the token for testing!\n", __FUNCTION__);
   #ifdef SAT_CORRUPT_TOKEN_TEST_FIRST
   g_ctrlm_voice.service_access_token.replace(10, 4, "DDDD");
   #endif
   #ifdef SAT_CORRUPT_TOKEN_TEST_SECOND
   g_ctrlm_voice.service_access_token.replace(60, 4, "DDDD");
   #endif
   #ifdef SAT_CORRUPT_TOKEN_TEST_THIRD
   g_ctrlm_voice.service_access_token.replace(600, 4, "DDDD");
   #endif
   #endif      
   #ifdef SAT_EXPIRED_TOKEN_TEST
   LOG_INFO("%s: WARNING using expired token for testing!\n", __FUNCTION__);
   g_ctrlm_voice.service_access_token = g_vrex_token_expired;
   #endif

   char time_str_buf[24];
   struct tm *current_lt;
   current_lt = localtime(&g_ctrlm_voice.service_access_token_expiration);
   strftime(time_str_buf, 24, "%F at %T", current_lt);
   LOG_INFO("%s: token expires on %s\n", __FUNCTION__, time_str_buf);

   return(true);
}

void ctrlm_voice_service_access_token_set(std::string service_access_token, time_t expiration) {
   g_ctrlm_voice.service_access_token            = service_access_token;
   g_ctrlm_voice.service_access_token_expiration = expiration;

   #if defined(SAT_CORRUPT_TOKEN_TEST_FIRST) ||defined(SAT_CORRUPT_TOKEN_TEST_SECOND) || defined(SAT_CORRUPT_TOKEN_TEST_THIRD)
   LOG_INFO("%s: WARNING we purposely corrupted the token for testing!\n", __FUNCTION__);
   #ifdef SAT_CORRUPT_TOKEN_TEST_FIRST
   g_ctrlm_voice.service_access_token.replace(10, 4, "DDDD");
   #endif
   #ifdef SAT_CORRUPT_TOKEN_TEST_SECOND
   g_ctrlm_voice.service_access_token.replace(60, 4, "DDDD");
   #endif
   #ifdef SAT_CORRUPT_TOKEN_TEST_THIRD
   g_ctrlm_voice.service_access_token.replace(600, 4, "DDDD");
   #endif
   #endif
   #ifdef SAT_EXPIRED_TOKEN_TEST
   LOG_INFO("%s: WARNING using expired token for testing!\n", __FUNCTION__);
   g_ctrlm_voice.service_access_token = g_vrex_token_expired;
   #endif

   char time_str_buf[24];
   struct tm *current_lt;
   current_lt = localtime(&g_ctrlm_voice.service_access_token_expiration);
   strftime(time_str_buf, 24, "%F at %T", current_lt);
   LOG_INFO("%s: token expires on %s\n", __FUNCTION__, time_str_buf);
}

bool ctrlm_voice_is_service_access_token_valid() {
   if(!g_ctrlm_voice.vrex_prefs.sat_enabled) {
     return (false);
   }

   if(time(NULL) < g_ctrlm_voice.service_access_token_expiration) {
      return(true);
   }

   // Need to acquire a new token
   if(ctrlm_voice_service_access_token_get()) {
      if(time(NULL) < g_ctrlm_voice.service_access_token_expiration) {
         return(true);
      }
   }
   g_ctrlm_voice.service_access_token.clear();
   return(false);
}

void ctrlm_voice_service_access_token_invalidate() {
   g_ctrlm_voice.service_access_token_expiration = 0;
   g_ctrlm_voice.service_access_token.clear();
}

void ctrlm_voice_memory_dump(unsigned char remote_id, unsigned char rib_attribute_index, unsigned char length, const unsigned char *pData) {

   // How to get the pieces needed to generate the file name (MAC Address, etc)
   // Need to limit the number of dumps somehow

   // Allocate a buffer to store the incoming dump

   // Confirm that index is not skipped

   // Store data at the specified index

   // if end of dump, write to file and release buffer


}

void ctrlm_voice_minimum_utterance_duration_set(unsigned long duration) {
   g_ctrlm_voice.chunk_size_first = (duration * 1000 / VOICE_PACKET_TIME) * 95;
}

void ctrlm_voice_query_strings_set(ctrlm_voice_query_strings_t query_strings) {
   errno_t safec_rc = memcpy_s(&g_ctrlm_voice.vrex_prefs.query_strings, sizeof(g_ctrlm_voice.vrex_prefs.query_strings), &query_strings, sizeof(ctrlm_voice_query_strings_t));
   ERR_CHK(safec_rc);
}


void ctrlm_voice_notify_stats_reboot(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_voice_reset_type_t type, unsigned char voltage, unsigned char battery_percentage) {
   LOG_WARN("%s: (%u, %u) Reset Type %u voltage %u percentage %u\n", __FUNCTION__, network_id, controller_id, type, voltage, battery_percentage);

   // Unschedule stats timer
   ctrlm_timeout_destroy(&g_ctrlm_voice.timeout_tag_stats);

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_session_stats_t msg = {0};

   msg.controller_id             = controller_id;
   msg.session.available         = 0;
   msg.reboot.available          = 1;
   msg.reboot.reset_type         = type;
   msg.reboot.voltage            = voltage;
   msg.reboot.battery_percentage = battery_percentage;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::ind_process_voice_session_stats, &msg, sizeof(msg), NULL, network_id);

   // Set to done so this only triggers once
   g_ctrlm_voice.stop_reason_last = CTRLM_VOICE_SESSION_END_REASON_MAX;
}

void ctrlm_voice_notify_stats_session(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, guint16 total_packets, guint16 drop_retry, guint16 drop_buffer, guint16 mac_retries, guint16 network_retries, guint16 cca_sense) {
   LOG_WARN("%s: (%u, %u)\n", __FUNCTION__, network_id, controller_id);

   // Unschedule stats timer
   ctrlm_timeout_destroy(&g_ctrlm_voice.timeout_tag_stats);

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

voice_session_prefs_t *ctrlm_voice_vrex_prefs_get() {
   return(&g_ctrlm_voice.vrex_prefs);
}

#ifndef USE_VOICE_SDK
void ctrlm_voice_cmd_status_set(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, voice_command_status_t status) {
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_voice_command_status_t msg;

   msg.controller_id     = controller_id;
   msg.status            = status;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::voice_command_status_set, &msg, sizeof(msg), NULL, network_id);
}
#endif

unsigned long ctrlm_voice_timeout_vrex_request_get(){
  return g_ctrlm_voice.vrex_prefs.timeout_vrex_request;
}

//#error Enable to check for warnings

