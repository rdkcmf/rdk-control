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
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include "include/ctrlm_ipc.h"
#include "include/ctrlm_ipc_voice.h"
#include "ctrlm_voice_obj.h"
#include "ctrlm.h"
#include "ctrlm_tr181.h"
#include "ctrlm_utils.h"
#include "ctrlm_database.h"
#include "ctrlm_network.h"
#include "jansson.h"
#include "xrsr.h"
#include "xrsv_http.h"
#include "xrsv_ws.h"
#include "ctrlm_xraudio_hal.h"
#include "json_config.h"
#include "ctrlm_voice_ipc_iarm_all.h"
#include "ctrlm_voice_endpoint.h"

#define MIN_VAL(x, y) ((x) < (y) ? (x) : (y)) // MIN is already defined, but in GLIB.. Eventually we need to get rid of this dependency

#define PIPE_READ  (0)
#define PIPE_WRITE (1)

#define CTRLM_VOICE_MIMETYPE_ADPCM  "audio/x-adpcm"
#define CTRLM_VOICE_SUBTYPE_ADPCM   "ADPCM"
#define CTRLM_VOICE_LANGUAGE        "eng"

#define CTRLM_CONTROLLER_CMD_STATUS_READ_TIMEOUT (10000)
#define CTRLM_VOICE_KEYWORD_BEEP_TIMEOUT         ( 1500)

#define ADPCM_COMMAND_ID_MIN             (0x20)    ///< Minimum bound of command id as defined by XVP Spec.
#define ADPCM_COMMAND_ID_MAX             (0x3F)    ///< Maximum bound of command id as defined by XVP Spec.

static void ctrlm_voice_session_response_confirm(bool result, ctrlm_timestamp_t *timestamp, void *user_data);
static void ctrlm_voice_data_post_processing_cb (ctrlm_hal_data_cb_params_t *param);

#ifdef BEEP_ON_KWD_ENABLED
static void ctrlm_voice_system_audio_player_event_handler(system_audio_player_event_t event, void *user_data);
#endif

static xrsr_power_mode_t voice_xrsr_power_map(ctrlm_power_state_t ctrlm_power_state);

#ifdef VOICE_BUFFER_STATS
static unsigned long voice_packet_interval_get(ctrlm_voice_format_t format, uint32_t opus_samples_per_packet);
#endif

// Application Interface Implementation
ctrlm_voice_t::ctrlm_voice_t() {
    LOG_INFO("%s: Constructor\n", __FUNCTION__);

    this->state_src                       = CTRLM_VOICE_STATE_SRC_INVALID;
    this->state_dst                       = CTRLM_VOICE_STATE_DST_INVALID;
    this->network_id                      = CTRLM_MAIN_NETWORK_ID_INVALID;
    this->controller_id                   = CTRLM_MAIN_CONTROLLER_ID_INVALID;
    this->hal_input_object                = NULL;
    this->format                          = CTRLM_VOICE_FORMAT_INVALID;
    this->audio_pipe[PIPE_READ]           = -1;
    this->audio_pipe[PIPE_WRITE]          = -1;
    this->audio_sent_bytes                =  0;
    this->audio_sent_samples              =  0;
    this->timeout_packet_tag              =  0;
    this->timeout_ctrl_session_stats_rxd  =  0;
    this->timeout_ctrl_cmd_status_read    =  0;
    this->timeout_keyword_beep            =  0;
    this->session_id                      =  0;
    this->software_version                = "N/A";
    this->prefs.server_url_vrex_src_ptt   = JSON_STR_VALUE_VOICE_URL_SRC_PTT;
    this->prefs.server_url_vrex_src_ff    = JSON_STR_VALUE_VOICE_URL_SRC_FF;
    this->prefs.sat_enabled               = JSON_BOOL_VALUE_VOICE_ENABLE_SAT;
    this->prefs.aspect_ratio              = JSON_STR_VALUE_VOICE_ASPECT_RATIO;
    this->prefs.guide_language            = JSON_STR_VALUE_VOICE_LANGUAGE;
    this->prefs.app_id_http               = JSON_STR_VALUE_VOICE_APP_ID_HTTP;
    this->prefs.app_id_ws                 = JSON_STR_VALUE_VOICE_APP_ID_WS;
    this->prefs.timeout_vrex_connect      = JSON_INT_VALUE_VOICE_VREX_REQUEST_TIMEOUT;
    this->prefs.timeout_vrex_session      = JSON_INT_VALUE_VOICE_VREX_RESPONSE_TIMEOUT;
    this->prefs.timeout_stats             = JSON_INT_VALUE_VOICE_TIMEOUT_STATS;
    this->prefs.timeout_packet_initial    = JSON_INT_VALUE_VOICE_TIMEOUT_PACKET_INITIAL;
    this->prefs.timeout_packet_subsequent = JSON_INT_VALUE_VOICE_TIMEOUT_PACKET_SUBSEQUENT;
    this->prefs.bitrate_minimum           = JSON_INT_VALUE_VOICE_BITRATE_MINIMUM;
    this->prefs.time_threshold            = JSON_INT_VALUE_VOICE_TIME_THRESHOLD;
    this->prefs.utterance_save            = JSON_BOOL_VALUE_VOICE_SAVE_LAST_UTTERANCE;
    this->prefs.utterance_file_qty_max    = JSON_INT_VALUE_VOICE_UTTERANCE_FILE_QTY_MAX;
    this->prefs.utterance_file_size_max   = JSON_INT_VALUE_VOICE_UTTERANCE_FILE_SIZE_MAX;
    this->prefs.utterance_path            = JSON_STR_VALUE_VOICE_UTTERANCE_PATH;
    this->prefs.utterance_duration_min    = JSON_INT_VALUE_VOICE_MINIMUM_DURATION;
    this->prefs.ffv_leading_samples       = JSON_INT_VALUE_VOICE_FFV_LEADING_SAMPLES;
    this->prefs.force_voice_settings      = JSON_BOOL_VALUE_VOICE_FORCE_VOICE_SETTINGS;
    this->prefs.keyword_sensitivity       = JSON_INT_VALUE_VOICE_KEYWORD_DETECT_SENSITIVITY;
    this->prefs.vrex_test_flag            = JSON_BOOL_VALUE_VOICE_VREX_TEST_FLAG;
    this->prefs.force_toggle_fallback     = JSON_BOOL_VALUE_VOICE_FORCE_TOGGLE_FALLBACK;
    this->prefs.par_voice_enabled         = false;
    this->prefs.par_voice_eos_method      = JSON_INT_VALUE_VOICE_PAR_VOICE_EOS_METHOD;
    this->prefs.par_voice_eos_timeout     = JSON_INT_VALUE_VOICE_PAR_VOICE_EOS_TIMEOUT;
    this->voice_params_opus_encoder_default();
    this->xrsr_opened                     = false;
    this->voice_ipc                       = NULL;
    this->endpoint_current                = NULL;
    this->end_reason                      = CTRLM_VOICE_SESSION_END_REASON_DONE;
    this->keyword_verified                = false;
    this->is_session_by_text              = false;
    this->transcription_in                = "";
    this->packet_loss_threshold           = JSON_INT_VALUE_VOICE_PACKET_LOSS_THRESHOLD;
    this->vsdk_config                     = NULL;

    #ifdef ENABLE_DEEP_SLEEP
    this->prefs.standby_params.connect_check_interval = JSON_INT_VALUE_VOICE_STANDBY_CONNECT_CHECK_INTERVAL;
    this->prefs.standby_params.timeout_connect        = JSON_INT_VALUE_VOICE_STANDBY_TIMEOUT_CONNECT;
    this->prefs.standby_params.timeout_inactivity     = JSON_INT_VALUE_VOICE_STANDBY_TIMEOUT_INACTIVITY;
    this->prefs.standby_params.timeout_session        = JSON_INT_VALUE_VOICE_STANDBY_TIMEOUT_SESSION;
    this->prefs.standby_params.ipv4_fallback          = JSON_BOOL_VALUE_VOICE_STANDBY_IPV4_FALLBACK;
    this->prefs.standby_params.backoff_delay          = JSON_INT_VALUE_VOICE_STANDBY_BACKOFF_DELAY;
    #endif

    voice_session_info_reset();

    // Device Status initialization
    sem_init(&this->device_status_semaphore, 0, 1);
    this->device_status[CTRLM_VOICE_DEVICE_PTT] = CTRLM_VOICE_DEVICE_STATUS_NONE;
    this->device_status[CTRLM_VOICE_DEVICE_FF]  = CTRLM_VOICE_DEVICE_STATUS_NONE;
#ifdef CTRLM_LOCAL_MIC
    this->device_status[CTRLM_VOICE_DEVICE_MICROPHONE] = CTRLM_VOICE_DEVICE_STATUS_NONE;
#else
    this->device_status[CTRLM_VOICE_DEVICE_MICROPHONE] = CTRLM_VOICE_DEVICE_STATUS_NOT_SUPPORTED;
#endif

    errno_t safec_rc = memset_s(&this->status, sizeof(this->status), 0, sizeof(this->status));
    ERR_CHK(safec_rc);
    this->last_cmd_id               = 0;
    this->next_cmd_id               = 0;
    this->packets_processed         = 0;
    this->packets_lost              = 0;
    this->confidence                = .0;
    this->session_active_server     = false;
    this->session_active_controller = false;
    this->sat_token_required        = false;
    this->network_type              = CTRLM_NETWORK_TYPE_INVALID;
    this->controller_voltage        = .0;
    this->voice_device              = CTRLM_VOICE_DEVICE_INVALID;
    this->server_ret_code           = 0;
    this->has_stream_params         = false;

    safec_rc = memset_s(this->sat_token, sizeof(this->sat_token), 0, sizeof(this->sat_token));
    ERR_CHK(safec_rc);
    safec_rc = memset_s(&this->session_timing, sizeof(this->session_timing), 0, sizeof(this->session_timing));
    ERR_CHK(safec_rc);
    safec_rc = memset_s(&this->stream_params, sizeof(this->stream_params), 0, sizeof(this->stream_params));
    ERR_CHK(safec_rc);
    // These semaphores are used to make sure we have all the data before calling the session begin callback
    sem_init(&this->vsr_semaphore, 0, 0);

    #ifdef BEEP_ON_KWD_ENABLED
    this->obj_sap.add_event_handler(ctrlm_voice_system_audio_player_event_handler, this);
    this->sap_opened = this->obj_sap.open(SYSTEM_AUDIO_PLAYER_AUDIO_TYPE_WAV, SYSTEM_AUDIO_PLAYER_SOURCE_TYPE_FILE, SYSTEM_AUDIO_PLAYER_PLAY_MODE_SYSTEM);
    if(!this->sap_opened) {
       LOG_WARN("%s: unable to open system audio player\n", __FUNCTION__);
    }
    #endif

    // Set audio mode to default
    ctrlm_voice_audio_settings_t settings = CTRLM_VOICE_AUDIO_SETTINGS_INITIALIZER;
    this->set_audio_mode(&settings);

    // Setup RFC callbacks
    ctrlm_rfc_t *rfc = ctrlm_rfc_t::get_instance();
    if(rfc) {
        rfc->add_changed_listener(ctrlm_rfc_t::attrs::VOICE, std::bind(&ctrlm_voice_t::voice_rfc_retrieved_handler, this, std::placeholders::_1));
        rfc->add_changed_listener(ctrlm_rfc_t::attrs::VSDK, std::bind(&ctrlm_voice_t::vsdk_rfc_retrieved_handler, this, std::placeholders::_1));
    }
}

ctrlm_voice_t::~ctrlm_voice_t() {
    LOG_INFO("%s: Destructor\n", __FUNCTION__);

    voice_sdk_close();
    if(this->voice_ipc) {
        this->voice_ipc->deregister_ipc();
        delete this->voice_ipc;
        this->voice_ipc = NULL;
    }
    if(this->vsdk_config) {
        json_decref(this->vsdk_config);
        this->vsdk_config = NULL;
    }
    if(audio_pipe[PIPE_READ] >= 0) {
        close(audio_pipe[PIPE_READ]);
        audio_pipe[PIPE_READ] = -1;
    }
    if(audio_pipe[PIPE_WRITE] >= 0) {
        close(audio_pipe[PIPE_WRITE]);
        audio_pipe[PIPE_WRITE] = -1;
    }

    #ifdef BEEP_ON_KWD_ENABLED
    if(this->sap_opened) {
        if(!this->obj_sap.close()) {
            LOG_WARN("%s: unable to close system audio player\n", __FUNCTION__);
        }
        this->sap_opened = false;
    }
    #endif

    /* Close Voice SDK */
}

bool ctrlm_voice_t::vsdk_is_privacy_enabled(void) {
   bool privacy;

   if(!xrsr_privacy_mode_get(&privacy)) {
      LOG_ERROR("%s: error getting privacy mode, defaulting to ON\n", __FUNCTION__);
      privacy = true;
   }

   return privacy;
}

void ctrlm_voice_t::voice_sdk_open(json_t *json_obj_vsdk) {
   if(this->xrsr_opened) {
      LOG_ERROR("%s: already open\n", __FUNCTION__);
      return;
   }
   xrsr_route_t          routes[1];
   xrsr_keyword_config_t kw_config;
   xrsr_capture_config_t capture_config = {
         .delete_files  = false,
         .enable        = this->prefs.utterance_save,
         .file_qty_max  = (uint32_t)this->prefs.utterance_file_qty_max,
         .file_size_max = (uint32_t)this->prefs.utterance_file_size_max,
         .dir_path      = this->prefs.utterance_path.c_str()
   };

   int ind = -1;
   errno_t safec_rc = strcmp_s(JSON_STR_VALUE_VOICE_UTTERANCE_PATH, strlen(JSON_STR_VALUE_VOICE_UTTERANCE_PATH), capture_config.dir_path, &ind);
   ERR_CHK(safec_rc);
   if((safec_rc == EOK) && (!ind)) {
      capture_config.dir_path = "/opt/logs"; // Default value specifies a file, but now it needs to be a directory
   }

   /* Open Voice SDK */
   routes[0].src                  = XRSR_SRC_INVALID;
   routes[0].dst_qty              = 0;
   kw_config.sensitivity          = this->prefs.keyword_sensitivity;

   char host_name[HOST_NAME_MAX];
   host_name[0] = '\0';

   int rc = gethostname(host_name, sizeof(host_name));
   if(rc != 0) {
       int errsv = errno;
       LOG_ERROR("%s: Failed to get host name <%s>\n", __FUNCTION__, strerror(errsv));
   }

   //HAL is not available to query mute state because xrsr is not open, so use stored status.
   bool privacy = this->voice_is_privacy_enabled();
   ctrlm_power_state_t ctrlm_power_state = ctrlm_main_get_power_state();
   xrsr_power_mode_t   xrsr_power_mode   = voice_xrsr_power_map(ctrlm_power_state);

   if(!xrsr_open(host_name, routes, &kw_config, &capture_config, xrsr_power_mode, privacy, json_obj_vsdk)) {
      LOG_ERROR("%s: Failed to open speech router\n", __FUNCTION__);
      g_assert(0);
   }

   this->xrsr_opened = true;
   this->state_src   = CTRLM_VOICE_STATE_SRC_READY;
   this->state_dst   = CTRLM_VOICE_STATE_DST_READY;
}

void ctrlm_voice_t::voice_sdk_close() {
    if(this->xrsr_opened) {
       xrsr_close();
       this->xrsr_opened = false;
    }
}

bool ctrlm_voice_t::voice_configure_config_file_json(json_t *obj_voice, json_t *json_obj_vsdk, bool local_conf) {
    json_config                       conf;
    ctrlm_voice_iarm_call_settings_t *voice_settings     = NULL;
    uint32_t                          voice_settings_len = 0;
    std::string                       init;

    LOG_INFO("%s: Configuring voice\n", __FUNCTION__);
    ctrlm_voice_audio_settings_t audio_settings = {this->audio_mode, this->audio_timing, this->audio_confidence_threshold, this->audio_ducking_type, this->audio_ducking_level, this->audio_ducking_beep_enabled};

    if (conf.config_object_set(obj_voice)){
        bool enabled = true;

        conf.config_value_get(JSON_BOOL_NAME_VOICE_ENABLE,                      enabled);
        conf.config_value_get(JSON_STR_NAME_VOICE_URL_SRC_PTT,                  this->prefs.server_url_vrex_src_ptt);
        conf.config_value_get(JSON_STR_NAME_VOICE_URL_SRC_FF,                   this->prefs.server_url_vrex_src_ff);
        conf.config_value_get(JSON_INT_NAME_VOICE_VREX_REQUEST_TIMEOUT,         this->prefs.timeout_vrex_connect,0);
        conf.config_value_get(JSON_INT_NAME_VOICE_VREX_RESPONSE_TIMEOUT,        this->prefs.timeout_vrex_session,0);
        conf.config_value_get(JSON_INT_NAME_VOICE_TIMEOUT_STATS,                this->prefs.timeout_stats);
        conf.config_value_get(JSON_INT_NAME_VOICE_TIMEOUT_PACKET_INITIAL,       this->prefs.timeout_packet_initial);
        conf.config_value_get(JSON_INT_NAME_VOICE_TIMEOUT_PACKET_SUBSEQUENT,    this->prefs.timeout_packet_subsequent);
        conf.config_value_get(JSON_INT_NAME_VOICE_BITRATE_MINIMUM,              this->prefs.bitrate_minimum);
        conf.config_value_get(JSON_INT_NAME_VOICE_TIME_THRESHOLD,               this->prefs.time_threshold);
        conf.config_value_get(JSON_BOOL_NAME_VOICE_ENABLE_SAT,                  this->sat_token_required);
        conf.config_value_get(JSON_BOOL_NAME_VOICE_SAVE_LAST_UTTERANCE,         this->prefs.utterance_save);
        conf.config_value_get(JSON_INT_NAME_VOICE_UTTERANCE_FILE_QTY_MAX,       this->prefs.utterance_file_qty_max, 1);
        conf.config_value_get(JSON_INT_NAME_VOICE_UTTERANCE_FILE_SIZE_MAX,      this->prefs.utterance_file_size_max, 4 * 1024);
        conf.config_value_get(JSON_STR_NAME_VOICE_UTTERANCE_PATH,               this->prefs.utterance_path);
        conf.config_value_get(JSON_INT_NAME_VOICE_MINIMUM_DURATION,             this->prefs.utterance_duration_min);
        conf.config_value_get(JSON_INT_NAME_VOICE_FFV_LEADING_SAMPLES,          this->prefs.ffv_leading_samples, 0);
        conf.config_value_get(JSON_STR_NAME_VOICE_APP_ID_HTTP,                  this->prefs.app_id_http);
        conf.config_value_get(JSON_STR_NAME_VOICE_APP_ID_WS,                    this->prefs.app_id_ws);
        conf.config_value_get(JSON_STR_NAME_VOICE_LANGUAGE,                     this->prefs.guide_language);
        conf.config_value_get(JSON_BOOL_NAME_VOICE_FORCE_VOICE_SETTINGS,        this->prefs.force_voice_settings);
        conf.config_value_get(JSON_INT_NAME_VOICE_KEYWORD_DETECT_SENSITIVITY,   this->prefs.keyword_sensitivity);
        conf.config_value_get(JSON_BOOL_NAME_VOICE_VREX_TEST_FLAG,              this->prefs.vrex_test_flag);
        conf.config_value_get(JSON_BOOL_NAME_VOICE_FORCE_TOGGLE_FALLBACK,       this->prefs.force_toggle_fallback);
        conf.config_value_get(JSON_STR_NAME_VOICE_OPUS_ENCODER_PARAMS,          this->prefs.opus_encoder_params_str);
        conf.config_value_get(JSON_INT_NAME_VOICE_PAR_VOICE_EOS_METHOD,         this->prefs.par_voice_eos_method);
        conf.config_value_get(JSON_INT_NAME_VOICE_PAR_VOICE_EOS_TIMEOUT,        this->prefs.par_voice_eos_timeout);
        conf.config_value_get(JSON_INT_NAME_VOICE_PACKET_LOSS_THRESHOLD,        this->packet_loss_threshold, 0);
        conf.config_value_get(JSON_INT_NAME_VOICE_AUDIO_MODE,                   audio_settings.mode);
        conf.config_value_get(JSON_INT_NAME_VOICE_AUDIO_TIMING,                 audio_settings.timing);
        conf.config_value_get(JSON_FLOAT_NAME_VOICE_AUDIO_CONFIDENCE_THRESHOLD, audio_settings.confidence_threshold, 0.0, 1.0);
        conf.config_value_get(JSON_INT_NAME_VOICE_AUDIO_DUCKING_TYPE,           audio_settings.ducking_type, CTRLM_VOICE_AUDIO_DUCKING_TYPE_ABSOLUTE, CTRLM_VOICE_AUDIO_DUCKING_TYPE_RELATIVE);
        conf.config_value_get(JSON_FLOAT_NAME_VOICE_AUDIO_DUCKING_LEVEL,        audio_settings.ducking_level, 0.0, 1.0);
        conf.config_value_get(JSON_BOOL_NAME_VOICE_AUDIO_DUCKING_BEEP,          audio_settings.ducking_beep);

        #ifdef ENABLE_DEEP_SLEEP
        conf.config_value_get(JSON_INT_NAME_VOICE_STANDBY_CONNECT_CHECK_INTERVAL, this->prefs.standby_params.connect_check_interval);
        conf.config_value_get(JSON_INT_NAME_VOICE_STANDBY_TIMEOUT_CONNECT,        this->prefs.standby_params.timeout_connect);
        conf.config_value_get(JSON_INT_NAME_VOICE_STANDBY_TIMEOUT_INACTIVITY,     this->prefs.standby_params.timeout_inactivity);
        conf.config_value_get(JSON_INT_NAME_VOICE_STANDBY_TIMEOUT_SESSION,        this->prefs.standby_params.timeout_session);
        conf.config_value_get(JSON_BOOL_NAME_VOICE_STANDBY_IPV4_FALLBACK,         this->prefs.standby_params.ipv4_fallback);
        conf.config_value_get(JSON_INT_NAME_VOICE_STANDBY_BACKOFF_DELAY,          this->prefs.standby_params.backoff_delay);
        #endif

        this->voice_params_opus_encoder_validate();

        // Check if enabled
        if(!enabled) {
            for(int i = CTRLM_VOICE_DEVICE_PTT; i < CTRLM_VOICE_DEVICE_INVALID; i++) {
                this->voice_device_disable((ctrlm_voice_device_t)i, false, NULL);
            }
        }
    }

    // Get voice settings
    if(ctrlm_db_voice_valid()) {
        if(this->prefs.force_voice_settings) {
            LOG_INFO("%s: Ignoring vrex settings from DB due to force_voice_settings being set to TRUE\n", __FUNCTION__);
        } else {
            uint8_t query_string_qty = 0;
            LOG_INFO("%s: Reading voice settings from Voice DB\n", __FUNCTION__);
            for(int i = CTRLM_VOICE_DEVICE_PTT; i < CTRLM_VOICE_DEVICE_INVALID; i++) {
                uint8_t voice_device_status = CTRLM_VOICE_DEVICE_STATUS_NONE;
                ctrlm_db_voice_read_device_status(i, (int *)&voice_device_status);
                if((voice_device_status & CTRLM_VOICE_DEVICE_STATUS_LEGACY) && (voice_device_status != CTRLM_VOICE_DEVICE_STATUS_DISABLED)) { // Convert from legacy to current (some value other than DISABLED set)
                    LOG_INFO("%s: Converting legacy device status value 0x%02X\n", __FUNCTION__, voice_device_status);
                    voice_device_status = CTRLM_VOICE_DEVICE_STATUS_NONE;
                    ctrlm_db_voice_write_device_status((ctrlm_voice_device_t)i, (voice_device_status & CTRLM_VOICE_DEVICE_STATUS_MASK_DB));
                }
                if(voice_device_status & CTRLM_VOICE_DEVICE_STATUS_DISABLED) {
                   this->voice_device_disable((ctrlm_voice_device_t)i, false, NULL);
                }
            }
            ctrlm_db_voice_read_url_ptt(this->prefs.server_url_vrex_src_ptt);
            ctrlm_db_voice_read_url_ff(this->prefs.server_url_vrex_src_ff);
            ctrlm_db_voice_read_sat_enable(this->sat_token_required);
            ctrlm_db_voice_read_guide_language(this->prefs.guide_language);
            ctrlm_db_voice_read_aspect_ratio(this->prefs.aspect_ratio);
            ctrlm_db_voice_read_utterance_duration_min(this->prefs.utterance_duration_min);
            ctrlm_db_voice_read_query_string_ptt_count(query_string_qty);
            LOG_INFO("%s: Voice DB contains %u query strings\n", __FUNCTION__, query_string_qty);
            // Create query strings
            for(unsigned int i = 0; i < query_string_qty; i++) {
                std::string key, value;
                ctrlm_db_voice_read_query_string_ptt(i, key, value);
                this->query_strs_ptt.push_back(std::make_pair(key, value));
                LOG_INFO("%s: Query String %u : key <%s> value <%s>\n", __FUNCTION__, i, key.c_str(), value.c_str());
            }
        }
        ctrlm_db_voice_read_init_blob(init);
        ctrlm_db_voice_read_audio_ducking_beep_enable(this->audio_ducking_beep_enabled);
        audio_settings.ducking_beep = this->audio_ducking_beep_enabled;

        ctrlm_db_voice_read_par_voice_status(this->prefs.par_voice_enabled);
    } else {
        LOG_WARN("%s: Reading voice settings from old style DB\n", __FUNCTION__);
        ctrlm_db_voice_settings_read((guchar **)&voice_settings, &voice_settings_len);
        if(voice_settings_len == 0) {
            LOG_INFO("%s: no voice settings in DB\n", __FUNCTION__);
        } else if(voice_settings_len != sizeof(ctrlm_voice_iarm_call_settings_t)) {
            LOG_WARN("%s: voice iarm settings is not the correct length, throwing away!\n", __FUNCTION__);
        } else if(voice_settings != NULL) {
            this->voice_configure(voice_settings, true); // We want to write this to the database now, as this now writes to the new style DB
            free(voice_settings);
        }
    }

    this->set_audio_mode(&audio_settings);
    this->process_xconf(&json_obj_vsdk, local_conf);

    // Disable muting/ducking to recover in case ctrlm restarts while muted/ducked.
    this->audio_state_set(false);
    this->vsdk_config = json_incref(json_obj_vsdk);
    this->voice_sdk_open(this->vsdk_config);

    // Update routes
    this->voice_sdk_update_routes();
    
    #ifdef CTRLM_LOCAL_MIC
    // Read privacy mode state from the hardware (not the DB)
    bool privacy_enabled = this->vsdk_is_privacy_enabled();
    if(privacy_enabled != this->voice_is_privacy_enabled()) {
        privacy_enabled ? this->voice_privacy_enable(false) : this->voice_privacy_disable(false);
    }
    #endif

    // Set init message if read from DB
    if(!init.empty()) {
        this->voice_init_set(init.c_str(), false);
    }

    // Update query strings
    this->query_strings_updated();

    return(true);
}

bool ctrlm_voice_t::voice_configure(ctrlm_voice_iarm_call_settings_t *settings, bool db_write) {
    bool update_routes = false;
    if(settings == NULL) {
        LOG_ERROR("%s: settings are null\n", __FUNCTION__);
        return(false);
    }

    if(this->prefs.force_voice_settings) {
        LOG_INFO("%s: Ignoring vrex settings from XRE due to force_voice_settings being set to TRUE\n", __FUNCTION__);
        return(true);
    }

    if(this->state_src == CTRLM_VOICE_STATE_SRC_STREAMING) {
        LOG_WARN("%s: Ignoring vrex settings from XRE due to voice session in progress.\n", __FUNCTION__);
        return(false);
    }

    if(settings->available & CTRLM_VOICE_SETTINGS_VOICE_ENABLED) {
        LOG_INFO("%s: Voice Control is <%s> : %u\n", __FUNCTION__, settings->voice_control_enabled ? "ENABLED" : "DISABLED", settings->voice_control_enabled);
        for(int i = CTRLM_VOICE_DEVICE_PTT; i < CTRLM_VOICE_DEVICE_INVALID; i++) {
            if(settings->voice_control_enabled) {
                this->voice_device_enable((ctrlm_voice_device_t)i, db_write, &update_routes);
            } else {
                this->voice_device_disable((ctrlm_voice_device_t)i, db_write, &update_routes);
            }
        }
    }

    if(settings->available & CTRLM_VOICE_SETTINGS_VREX_SERVER_URL) {
        settings->vrex_server_url[CTRLM_VOICE_SERVER_URL_MAX_LENGTH - 1] = '\0';
        LOG_INFO("%s: vrex URL <%s>\n", __FUNCTION__, settings->vrex_server_url);
        this->prefs.server_url_vrex_src_ptt = settings->vrex_server_url;
        update_routes = true;
        if(db_write) {
            ctrlm_db_voice_write_url_ptt(this->prefs.server_url_vrex_src_ptt);
        }
    }

    if(settings->available & CTRLM_VOICE_SETTINGS_VREX_SAT_ENABLED) {
        LOG_INFO("%s: Vrex SAT is <%s> : (%d)\n", __FUNCTION__, settings->vrex_sat_enabled ? "ENABLED" : "DISABLED", settings->vrex_sat_enabled);
        this->sat_token_required = (settings->vrex_sat_enabled) ? true : false;
        if(db_write) {
            ctrlm_db_voice_write_sat_enable(this->sat_token_required);
        }
    }

    if(settings->available & CTRLM_VOICE_SETTINGS_GUIDE_LANGUAGE) {
        settings->guide_language[CTRLM_VOICE_GUIDE_LANGUAGE_MAX_LENGTH - 1] = '\0';
        LOG_INFO("%s: Guide Language <%s>\n", __FUNCTION__, settings->guide_language);
        voice_stb_data_guide_language_set(settings->guide_language);
        if(db_write) {
            ctrlm_db_voice_write_guide_language(this->prefs.guide_language);
        }
    }

    if(settings->available & CTRLM_VOICE_SETTINGS_ASPECT_RATIO) {
        settings->aspect_ratio[CTRLM_VOICE_ASPECT_RATIO_MAX_LENGTH - 1] = '\0';
        LOG_INFO("%s: Aspect Ratio <%s>\n", __FUNCTION__, settings->aspect_ratio);
        this->prefs.aspect_ratio = settings->aspect_ratio;
        if(db_write) {
            ctrlm_db_voice_write_aspect_ratio(this->prefs.aspect_ratio);
        }
    }

    if(settings->available & CTRLM_VOICE_SETTINGS_UTTERANCE_DURATION) {
        LOG_INFO("%s: Utterance Duration Minimum <%lu ms>\n", __FUNCTION__, settings->utterance_duration_minimum);
        if(settings->utterance_duration_minimum > CTRLM_VOICE_MIN_UTTERANCE_DURATION_MAXIMUM) {
            this->prefs.utterance_duration_min = CTRLM_VOICE_MIN_UTTERANCE_DURATION_MAXIMUM;
        } else {
            this->prefs.utterance_duration_min = settings->utterance_duration_minimum;
        }
        if(db_write) {
            ctrlm_db_voice_write_utterance_duration_min(this->prefs.utterance_duration_min);
        }
        update_routes = true;
    }

    if(settings->available & CTRLM_VOICE_SETTINGS_QUERY_STRINGS) {
        if(settings->query_strings.pair_count > CTRLM_VOICE_QUERY_STRING_MAX_PAIRS) {
            LOG_WARN("%s: Query String Pair Count too high <%d>.  Setting to <%d>.\n", __FUNCTION__, settings->query_strings.pair_count, CTRLM_VOICE_QUERY_STRING_MAX_PAIRS);
            settings->query_strings.pair_count = CTRLM_VOICE_QUERY_STRING_MAX_PAIRS;
        }
        this->query_strs_ptt.clear();
        int query = 0;
        for(; query < settings->query_strings.pair_count; query++) {
            std::string key, value;

            //Make sure the strings are NULL terminated
            settings->query_strings.query_string[query].name[CTRLM_VOICE_QUERY_STRING_MAX_LENGTH - 1] = '\0';
            settings->query_strings.query_string[query].value[CTRLM_VOICE_QUERY_STRING_MAX_LENGTH - 1] = '\0';
            key   = settings->query_strings.query_string[query].name;
            value = settings->query_strings.query_string[query].value;
            LOG_INFO("%s: Query String[%d] name <%s> value <%s>\n", __FUNCTION__, query, key.c_str(), value.c_str());
            this->query_strs_ptt.push_back(std::make_pair(key, value));
            if(db_write) {
                ctrlm_db_voice_write_query_string_ptt(query, key, value);
            }
        }
        if(db_write) {
            ctrlm_db_voice_write_query_string_ptt_count(settings->query_strings.pair_count);
        }
        this->query_strings_updated();
    }

    if(settings->available & CTRLM_VOICE_SETTINGS_FARFIELD_VREX_SERVER_URL) {
        settings->server_url_vrex_src_ff[CTRLM_VOICE_SERVER_URL_MAX_LENGTH - 1] = '\0';
        LOG_INFO("%s: Farfield vrex URL <%s>\n", __FUNCTION__, settings->server_url_vrex_src_ff);
        this->prefs.server_url_vrex_src_ff = settings->server_url_vrex_src_ff;
        update_routes = true;
        if(db_write) {
            ctrlm_db_voice_write_url_ff(this->prefs.server_url_vrex_src_ff);
        }
    }

    if(update_routes && this->xrsr_opened) {
        this->voice_sdk_update_routes();
    }

    return(true);
}

bool ctrlm_voice_t::voice_configure(json_t *settings, bool db_write) {
    json_config conf;
    bool update_routes = false;
    if(NULL == settings) {
        LOG_ERROR("%s: JSON object NULL\n", __FUNCTION__);
        return(false);
    }

    if(this->xrsr_opened == false) { // We are not ready to configure voice settings yet.
        LOG_WARN("%s: Voice object not ready for settings, return failure\n", __FUNCTION__);
        return(false);
    }

    if(this->prefs.force_voice_settings) {
        LOG_INFO("%s: Ignoring vrex settings from configure call due to force_voice_settings being set to TRUE\n", __FUNCTION__);
        return(true);
    }

    if(this->state_src == CTRLM_VOICE_STATE_SRC_STREAMING) {
        LOG_WARN("%s: Ignoring vrex settings from Thunder due to voice session in progress.\n", __FUNCTION__);
        return(false);
    }

    if(conf.config_object_set(settings)) {
        bool enable;
        bool prv_enabled;
        std::string url;
        json_config device_config;

        if(conf.config_value_get("enable", enable)) {
            LOG_INFO("%s: Voice Control is <%s>\n", __FUNCTION__, enable ? "ENABLED" : "DISABLED");
            for(int i = CTRLM_VOICE_DEVICE_PTT; i < CTRLM_VOICE_DEVICE_INVALID; i++) {
                if(enable) {
                    this->voice_device_enable((ctrlm_voice_device_t)i, db_write, &update_routes);
                } else {
                    this->voice_device_disable((ctrlm_voice_device_t)i, db_write, &update_routes);
                }
            }
        }
        if(conf.config_value_get("urlAll", url)) {
            this->prefs.server_url_vrex_src_ptt = url;
            this->prefs.server_url_vrex_src_ff  = url;
            update_routes = true;
        }
        if(conf.config_value_get("urlPtt", url)) {
            this->prefs.server_url_vrex_src_ptt = url;
            update_routes = true;
        }
        if(conf.config_value_get("urlHf", url)) {
            this->prefs.server_url_vrex_src_ff  = url;
            update_routes = true;
        }
        if(conf.config_value_get("prv", prv_enabled)) {
            this->prefs.par_voice_enabled = prv_enabled;
            LOG_INFO("%s: Press and Release Voice is <%s>\n", __FUNCTION__, this->prefs.par_voice_enabled ? "ENABLED" : "DISABLED");
            if(db_write) {
               ctrlm_db_voice_write_par_voice_status(this->prefs.par_voice_enabled);
            }
        }
        if(conf.config_object_get("ptt", device_config)) {
            if(device_config.config_value_get("enable", enable)) {
                LOG_INFO("%s: Voice Control PTT is <%s>\n", __FUNCTION__, enable ? "ENABLED" : "DISABLED");
                if(enable) {
                    this->voice_device_enable(CTRLM_VOICE_DEVICE_PTT, db_write, &update_routes);
                } else {
                    this->voice_device_disable(CTRLM_VOICE_DEVICE_PTT, db_write, &update_routes);
                }
            }
        }
        if(conf.config_object_get("ff", device_config)) {
            if(device_config.config_value_get("enable", enable)) {
                LOG_INFO("%s: Voice Control FF is <%s>\n", __FUNCTION__, enable ? "ENABLED" : "DISABLED");
                if(enable) {
                    this->voice_device_enable(CTRLM_VOICE_DEVICE_FF, db_write, &update_routes);
                } else {
                    this->voice_device_disable(CTRLM_VOICE_DEVICE_FF, db_write, &update_routes);
                }
            }
        }
        if(conf.config_object_get("mic", device_config)) {
            if(device_config.config_value_get("enable", enable)) {
                LOG_INFO("%s: Voice Control MIC is <%s>\n", __FUNCTION__, enable ? "ENABLED" : "DISABLED");
                #ifdef CTRLM_LOCAL_MIC_DISABLE_VIA_PRIVACY
                bool privacy_enabled = this->voice_is_privacy_enabled();
                if(enable) {
                    if(privacy_enabled) {
                        this->voice_privacy_disable(true);
                    }
                } else if(!privacy_enabled) {
                    this->voice_privacy_enable(true);
                }
                #else
                if(enable) {
                    this->voice_device_enable(CTRLM_VOICE_DEVICE_MICROPHONE, db_write, &update_routes);
                } else {
                    this->voice_device_disable(CTRLM_VOICE_DEVICE_MICROPHONE, db_write, &update_routes);
                }
                #endif
            }
        }
        if(conf.config_value_get("wwFeedback", enable)) { // This option will enable / disable the Wake Word feedback (typically an audible beep).
           LOG_INFO("%s: Voice Control kwd feedback is <%s>\n", __FUNCTION__, enable ? "ENABLED" : "DISABLED");
           this->audio_ducking_beep_enabled = enable;

           if(db_write) {
              ctrlm_db_voice_write_audio_ducking_beep_enable(enable);
           }
        }
        if(update_routes && this->xrsr_opened) {
            this->voice_sdk_update_routes();
            if(db_write) {
                ctrlm_db_voice_write_url_ptt(this->prefs.server_url_vrex_src_ptt);
                ctrlm_db_voice_write_url_ff(this->prefs.server_url_vrex_src_ff);
            }
        }
    }
    return(true);
}

bool ctrlm_voice_t::voice_message(const char *message) {
    if(this->endpoint_current) {
        return(this->endpoint_current->voice_message(message));
    }
    return(false);
}

bool ctrlm_voice_t::voice_status(ctrlm_voice_status_t *status) {
    bool ret = false;
    /* TODO 
     * Defaulted Press and Release voice to true/supported
     * Will make this dynamic once PAR EoS is implemented and 
     * by some criteria timeout is not adequate
     */
    ctrlm_voice_status_capabilities_t capabilities = {
       .prv = true,
    #ifdef BEEP_ON_KWD_ENABLED
       .wwFeedback = true,
    #else
       .wwFeedback = false,
    #endif
    };

    if(!this->xrsr_opened) {
        LOG_ERROR("%s: xrsr not opened yet\n", __FUNCTION__);
    } else if(NULL == status) {
        LOG_ERROR("%s: invalid params\n", __FUNCTION__);
    } else {
        status->urlPtt = this->prefs.server_url_vrex_src_ptt;
        status->urlHf  = this->prefs.server_url_vrex_src_ff;
        sem_wait(&this->device_status_semaphore);
        for(int i = CTRLM_VOICE_DEVICE_PTT; i < CTRLM_VOICE_DEVICE_INVALID; i++) {
            status->status[i] = this->device_status[i];
        }
        status->wwFeedback   = this->audio_ducking_beep_enabled;
        status->prv_enabled  = this->prefs.par_voice_enabled;
        status->capabilities = capabilities;
        sem_post(&this->device_status_semaphore);
        ret = true;
    }
    return(ret);
}

bool ctrlm_voice_t::server_message(const char *message, unsigned long length) {
    bool ret = false;
    if(this->voice_ipc) {
        ret = this->voice_ipc->server_message(message, length);
    }
    return(ret);
}

bool ctrlm_voice_t::voice_init_set(const char *init, bool db_write) {
    bool ret = false; // Return true if at least one endpoint was set

    if(this->xrsr_opened == false) { // We are not ready for set init call
        LOG_WARN("%s: Voice object not ready for set init, return failure\n", __FUNCTION__);
    } else {
        for(const auto &itr : this->endpoints) {
            if(itr->voice_init_set(init)) {
                ret = true;
            }
        }
        if(db_write) {
            ctrlm_db_voice_write_init_blob(std::string(init));
        }
    }
    return(ret);
}

void ctrlm_voice_t::process_xconf(json_t **json_obj_vsdk, bool local_conf) {
   LOG_INFO("%s: Voice XCONF Settings\n", __FUNCTION__);
   int result;

   char vsdk_config_str[CTRLM_RFC_MAX_PARAM_LEN] = {0}; //MAX_PARAM_LEN from rfcapi.h is 2048

#ifdef CTRLM_NETWORK_RF4CE
   char encoder_params_str[CTRLM_RCU_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS * 2 + 1] = {0};

   result  = ctrlm_tr181_string_get(CTRLM_RF4CE_TR181_RF4CE_OPUS_ENCODER_PARAMS, encoder_params_str, sizeof(encoder_params_str));
   if(result == CTRLM_TR181_RESULT_SUCCESS) {
      this->prefs.opus_encoder_params_str = encoder_params_str;
      this->voice_params_opus_encoder_validate();

      LOG_INFO("%s: opus encoder params <%s>\n", __FUNCTION__, this->prefs.opus_encoder_params_str.c_str());
   }
#endif

   ctrlm_voice_audio_settings_t audio_settings = {this->audio_mode, this->audio_timing, this->audio_confidence_threshold, this->audio_ducking_type, this->audio_ducking_level, this->audio_ducking_beep_enabled};
   bool changed = false;

   result = ctrlm_tr181_int_get(CTRLM_TR181_VOICE_PARAMS_AUDIO_MODE, (int*)&audio_settings.mode);
   if(result == CTRLM_TR181_RESULT_SUCCESS) {
       changed = true;
   }
   result = ctrlm_tr181_int_get(CTRLM_TR181_VOICE_PARAMS_AUDIO_TIMING, (int *)&audio_settings.timing);
   if(result == CTRLM_TR181_RESULT_SUCCESS) {
       changed = true;
   }
   result = ctrlm_tr181_real_get(CTRLM_TR181_VOICE_PARAMS_AUDIO_CONFIDENCE_THRESHOLD, &audio_settings.confidence_threshold);
   if(result == CTRLM_TR181_RESULT_SUCCESS) {
       changed = true;
   }
   result = ctrlm_tr181_int_get(CTRLM_TR181_VOICE_PARAMS_AUDIO_DUCKING_TYPE, (int *)&audio_settings.ducking_type);
   if(result == CTRLM_TR181_RESULT_SUCCESS) {
       changed = true;
   }
   result = ctrlm_tr181_real_get(CTRLM_TR181_VOICE_PARAMS_AUDIO_DUCKING_LEVEL, &audio_settings.ducking_level);
   if(result == CTRLM_TR181_RESULT_SUCCESS) {
       changed = true;
   }

   // CTRLM_TR181_VOICE_PARAMS_AUDIO_DUCKING_BEEP doesn't exist because this is a user configurable setting via configureVoice thunder api

   int keyword_sensitivity = 0;
   result = ctrlm_tr181_int_get(CTRLM_TR181_VOICE_PARAMS_KEYWORD_SENSITIVITY, &keyword_sensitivity);
   if(result == CTRLM_TR181_RESULT_SUCCESS) {
      this->prefs.keyword_sensitivity = (keyword_sensitivity > 0) ? keyword_sensitivity : JSON_INT_VALUE_VOICE_KEYWORD_DETECT_SENSITIVITY;
   }

   result = ctrlm_tr181_string_get(CTRLM_TR181_VOICE_PARAMS_VSDK_CONFIGURATION, &vsdk_config_str[0], CTRLM_RFC_MAX_PARAM_LEN);
   if(result == CTRLM_TR181_RESULT_SUCCESS) {
      json_error_t jerror;
      json_t *jvsdk;
      char *decoded_buf = NULL;
      size_t decoded_buf_len = 0;

      decoded_buf = (char *)g_base64_decode((const gchar*)vsdk_config_str, &decoded_buf_len);
      if(decoded_buf) {
         if(decoded_buf_len > 0 && decoded_buf_len < CTRLM_RFC_MAX_PARAM_LEN) {
            LOG_INFO("%s: VSDK configuration taken from XCONF\n", __FUNCTION__);
            LOG_INFO("%s\n", decoded_buf);

            jvsdk = json_loads(&decoded_buf[0], 0, &jerror);
            do {
               if(NULL == jvsdk) {
                  LOG_ERROR("%s: XCONF has VSDK params but json_loads() failed, line %d: %s \n",
                        __FUNCTION__, jerror.line, jerror.text );
                  break;
               }
               if(!json_is_object(jvsdk))
               {
                  LOG_ERROR("%s: found VSDK in text but invalid object\n", __FUNCTION__);
                  break;
               }

               //If execution reaches here we have XCONF settings to use. If developer has used local conf settings, keep them.
               if(local_conf) {
                  if(!json_object_update(jvsdk, *json_obj_vsdk)) {
                     LOG_ERROR("%s: failed to update json_obj_vsdk\n", __FUNCTION__);
                     break;
                  }
               }

               *json_obj_vsdk = json_deep_copy(jvsdk);
               if(NULL == *json_obj_vsdk)
               {
                  LOG_ERROR("%s: found VSDK object but failed to copy. We have lost any /opt file VSDK parameters\n", __FUNCTION__);
                  /* Nothing to do about this unlikely error. If I copy to a temp pointer to protect the input,
                   * then I have to copy from temp to real, and check that copy for failure. Where would it end?
                   */
                  break;
               }
            }while(0);
         } else {
            LOG_WARN("%s: incorrect length\n", __FUNCTION__);
         }
         free(decoded_buf);
      } else {
         LOG_WARN("%s: failed to decode base64\n", __FUNCTION__);
      }
   }

   int value = 0;
   result = ctrlm_tr181_int_get(CTRLM_RF4CE_TR181_PRESS_AND_RELEASE_EOS_TIMEOUT, &value, 0, UINT16_MAX);
   if(result == CTRLM_TR181_RESULT_SUCCESS) {
      this->prefs.par_voice_eos_timeout = value;
   }

   result = ctrlm_tr181_int_get(CTRLM_RF4CE_TR181_PRESS_AND_RELEASE_EOS_METHOD, &value, 0, UINT8_MAX);
   if(result == CTRLM_TR181_RESULT_SUCCESS) {
      this->prefs.par_voice_eos_method = value;
   }

   if(changed) {
       this->set_audio_mode(&audio_settings);
   }
}

void ctrlm_voice_t::query_strings_updated() {
    // N/A
}

void ctrlm_voice_t::voice_params_qos_get(voice_params_qos_t *params) {
   if(params == NULL) {
      LOG_ERROR("%s: NULL param\n", __FUNCTION__);
      return;
   }
   params->timeout_packet_initial    = this->prefs.timeout_packet_initial;
   params->timeout_packet_subsequent = this->prefs.timeout_packet_subsequent;
   params->bitrate_minimum           = this->prefs.bitrate_minimum;
   params->time_threshold            = this->prefs.time_threshold;
}

void ctrlm_voice_t::voice_params_opus_encoder_get(voice_params_opus_encoder_t *params) {
   if(params == NULL) {
      LOG_ERROR("%s: NULL param\n", __FUNCTION__);
      return;
   }
   errno_t safec_rc = memcpy_s(params->data, sizeof(params->data), this->prefs.opus_encoder_params, CTRLM_RCU_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS);
   ERR_CHK(safec_rc);
}

void ctrlm_voice_t::voice_params_par_get(voice_params_par_t *params) {
   if(params == NULL) {
      LOG_ERROR("%s: NULL param\n", __FUNCTION__);
      return;
   }
   params->par_voice_enabled     = this->prefs.par_voice_enabled;
   params->par_voice_eos_method  = this->prefs.par_voice_eos_method;
   params->par_voice_eos_timeout = this->prefs.par_voice_eos_timeout;
}

void ctrlm_voice_t::voice_params_opus_encoder_default(void) {
   this->prefs.opus_encoder_params_str = JSON_STR_VALUE_VOICE_OPUS_ENCODER_PARAMS;
   this->voice_params_hex_str_to_bytes(this->prefs.opus_encoder_params_str, this->prefs.opus_encoder_params, sizeof(this->prefs.opus_encoder_params));
   this->voice_params_opus_samples_per_packet_set();
}

void ctrlm_voice_t::voice_params_opus_samples_per_packet_set(void) {
   guchar fr_dur = (this->prefs.opus_encoder_params[3] >> 4) & 0xF;
   switch(fr_dur) {
      case 1: { this->opus_samples_per_packet =  8 *   5; break; } // 2.5 ms
      case 2: { this->opus_samples_per_packet = 16 *   5; break; } //   5 ms
      case 3: { this->opus_samples_per_packet = 16 *  10; break; } //  10 ms
      case 5: { this->opus_samples_per_packet = 16 *  40; break; } //  40 ms
      case 6: { this->opus_samples_per_packet = 16 *  60; break; } //  60 ms
      case 7: { this->opus_samples_per_packet = 16 *  80; break; } //  80 ms
      case 8: { this->opus_samples_per_packet = 16 * 100; break; } // 100 ms
      case 9: { this->opus_samples_per_packet = 16 * 120; break; } // 120 ms
      default:{ this->opus_samples_per_packet = 16 * 20;  break; } //  20 ms
   }
}

bool ctrlm_voice_t::voice_params_hex_str_to_bytes(std::string hex_string, guchar *data, guint32 length) {
   if(hex_string.length() != (length << 1)) {
      LOG_ERROR("%s: INVALID hex string length\n", __FUNCTION__);
      return(false);
   }

   // Convert hex string to bytes
   const char *str = hex_string.c_str();
   for(guchar index = 0; index < length; index++) {
      unsigned int value;
      int rc = sscanf(&str[index << 1], "%02X", &value);
      if(rc != 1) {
         LOG_ERROR("%s: INVALID hex digit <%s>\n", __FUNCTION__, str);
         return(false);
      }
      data[index] = (guchar)value;
   }
   return(true);
}

void ctrlm_voice_t::voice_params_opus_encoder_validate(void) {
   bool invalid = false;

   do {
      if(!voice_params_hex_str_to_bytes(this->prefs.opus_encoder_params_str, this->prefs.opus_encoder_params, sizeof(this->prefs.opus_encoder_params))) {
         LOG_ERROR("%s: INVALID hex string <%s>\n", __FUNCTION__, this->prefs.opus_encoder_params_str.c_str());
         invalid = true;
         break;
      }
      LOG_INFO("%s: str <%s> 0x%02X %02X %02X %02X %02X\n", __FUNCTION__, this->prefs.opus_encoder_params_str.c_str(), this->prefs.opus_encoder_params[0], this->prefs.opus_encoder_params[1], this->prefs.opus_encoder_params[2], this->prefs.opus_encoder_params[3], this->prefs.opus_encoder_params[4]);

      // Check for invalid encoder parameters
      guchar flags = this->prefs.opus_encoder_params[0];
      if((flags & 0x3) == 0x3) {
         LOG_ERROR("%s: INVALID application - flags 0x%02X\n", __FUNCTION__, flags);
         invalid = true;
         break;
      }
      if((flags & 0xE0) > 0x80) {
         LOG_ERROR("%s: INVALID max bandwidth - flags 0x%02X\n", __FUNCTION__, flags);
         invalid = true;
         break;
      }
      guchar plp = this->prefs.opus_encoder_params[1] & 0x7F;
      if(plp > 100) {
         LOG_ERROR("%s: INVALID plp %u\n", __FUNCTION__, plp);
         invalid = true;
         break;
      }
      guchar bitrate = this->prefs.opus_encoder_params[2];
      if(bitrate > 128 && bitrate < 255) {
         LOG_ERROR("%s: INVALID bitrate %u\n", __FUNCTION__, bitrate);
         invalid = true;
         break;
      }
      guchar comp = this->prefs.opus_encoder_params[3] & 0xF;
      if(comp > 10) {
         LOG_ERROR("%s: INVALID comp %u\n", __FUNCTION__, comp);
         invalid = true;
         break;
      }
      guchar fr_dur = (this->prefs.opus_encoder_params[3] >> 4) & 0xF;
      if(fr_dur > 9) {
         LOG_ERROR("%s: INVALID frame duration %u\n", __FUNCTION__, fr_dur);
         invalid = true;
         break;
      }
      guchar lsbd = this->prefs.opus_encoder_params[4] & 0x1F;
      if(lsbd < 8 || lsbd > 24) {
         LOG_ERROR("%s: INVALID lsbd %u\n", __FUNCTION__, lsbd);
         invalid = true;
         break;
      }
   } while(0);

   if(invalid) { // Restore default settings
      LOG_WARN("%s: restoring default settings\n", __FUNCTION__);
      this->voice_params_opus_encoder_default();
      LOG_INFO("%s: str <%s> 0x%02X %02X %02X %02X %02X\n", __FUNCTION__, this->prefs.opus_encoder_params_str.c_str(), this->prefs.opus_encoder_params[0], this->prefs.opus_encoder_params[1], this->prefs.opus_encoder_params[2], this->prefs.opus_encoder_params[3], this->prefs.opus_encoder_params[4]);
   } else {
      this->voice_params_opus_samples_per_packet_set();
   }
}

void ctrlm_voice_t::voice_status_set() {
   if(this->network_id != CTRLM_MAIN_NETWORK_ID_INVALID && this->controller_id != CTRLM_MAIN_CONTROLLER_ID_INVALID) {
      ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::voice_command_status_set, &this->status, sizeof(this->status), NULL, this->network_id);

      if(this->status.status != VOICE_COMMAND_STATUS_PENDING) {
         rdkx_timestamp_get_realtime(&this->session_timing.ctrl_cmd_status_wr);

         if(this->controller_command_status && this->timeout_ctrl_cmd_status_read <= 0) { // Set a timeout to clean up in case controller does not read the status
            this->timeout_ctrl_cmd_status_read = g_timeout_add(CTRLM_CONTROLLER_CMD_STATUS_READ_TIMEOUT, ctrlm_voice_controller_command_status_read_timeout, NULL);
         } else {
             if(!this->session_active_server) {
                this->state_src   = CTRLM_VOICE_STATE_SRC_READY;
                voice_session_stats_print();
                voice_session_stats_clear();
             }
         }
      }
   }
}

bool ctrlm_voice_t::voice_device_streaming(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) {
    if(this->network_id == network_id && this->controller_id == controller_id) {
        if(this->state_src == CTRLM_VOICE_STATE_SRC_STREAMING && (this->state_dst >= CTRLM_VOICE_STATE_DST_REQUESTED && this->state_dst <= CTRLM_VOICE_STATE_DST_STREAMING)) {
            return(true);
        }
    }
    return(false);
}

void ctrlm_voice_t::voice_controller_command_status_read(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) {

   // TODO Access to the state variables in this function are not thread safe.  Need to make the entire voice object thread safe.

   if(this->network_id == network_id && this->controller_id == controller_id) {
       this->session_active_controller = false;
       rdkx_timestamp_get_realtime(&this->session_timing.ctrl_cmd_status_rd);

       // Remove controller status read timeout
       if(this->timeout_ctrl_cmd_status_read > 0) {
           g_source_remove(this->timeout_ctrl_cmd_status_read);
           this->timeout_ctrl_cmd_status_read = 0;
       }

       if(!this->session_active_server) {
          this->state_src   = CTRLM_VOICE_STATE_SRC_READY;
          voice_session_stats_print();
          voice_session_stats_clear();

         // Send session stats only if they haven't already been sent
         if(stats_session_id != 0 && this->voice_ipc) {
            ctrlm_voice_ipc_event_session_statistics_t stats;

            stats.common  = this->ipc_common_data;
            stats.session = this->stats_session;
            stats.reboot  = this->stats_reboot;

            this->voice_ipc->session_statistics(stats);
         }
       }
   }
}

void ctrlm_voice_t::voice_session_controller_command_status_read_timeout(void) {
   if(this->network_id == CTRLM_MAIN_NETWORK_ID_INVALID || this->controller_id == CTRLM_MAIN_CONTROLLER_ID_INVALID) {
      LOG_ERROR("%s: no active voice session\n", __FUNCTION__);
   } else if(this->timeout_ctrl_cmd_status_read > 0) {
      this->session_active_controller    = false;
      this->timeout_ctrl_cmd_status_read = 0;
      LOG_INFO("%s: controller failed to read command status\n", __FUNCTION__);
      xrsr_session_terminate(); // Synchronous - this will take a bit of time.  Might need to revisit this down the road.

      if(!this->session_active_server) {
         this->state_src   = CTRLM_VOICE_STATE_SRC_READY;
         voice_session_stats_print();
         voice_session_stats_clear();

         // Send session stats only if they haven't already been sent
         if(stats_session_id != 0 && this->voice_ipc) {
            ctrlm_voice_ipc_event_session_statistics_t stats;

            stats.common  = this->ipc_common_data;
            stats.session = this->stats_session;
            stats.reboot  = this->stats_reboot;

            this->voice_ipc->session_statistics(stats);
         }
      }
   }
}

ctrlm_voice_session_response_status_t ctrlm_voice_t::voice_session_req(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, 
                                                                       ctrlm_voice_device_t device_type, ctrlm_voice_format_t format,
                                                                       voice_session_req_stream_params *stream_params,
                                                                       const char *controller_name, const char *sw_version, const char *hw_version, double voltage, bool command_status,
                                                                       ctrlm_timestamp_t *timestamp, ctrlm_voice_session_rsp_confirm_t *cb_confirm, void **cb_confirm_param, bool use_external_data_pipe, const char *l_transcription_in) {
    if(CTRLM_VOICE_STATE_SRC_INVALID == this->state_src) {
        LOG_ERROR("%s: Voice is not ready\n", __FUNCTION__);
        this->voice_session_notify_abort(network_id, controller_id, 0, CTRLM_VOICE_SESSION_ABORT_REASON_SERVER_NOT_READY);
        return(VOICE_SESSION_RESPONSE_SERVER_NOT_READY);
    } 
#ifdef AUTH_ENABLED
    else if(!this->voice_session_has_stb_data()) {
        LOG_ERROR("%s: Authentication Data missing\n", __FUNCTION__);
        this->voice_session_notify_abort(network_id, controller_id, 0, CTRLM_VOICE_SESSION_ABORT_REASON_NO_RECEIVER_ID);
        return(VOICE_SESSION_RESPONSE_SERVER_NOT_READY);
    }
#endif
    else if(!this->voice_session_can_request(device_type)) { // This is an unsafe check but we don't want to block on a semaphore here
        LOG_ERROR("%s: Voice Device <%s> is <%s>\n", __FUNCTION__, ctrlm_voice_device_str(device_type), ctrlm_voice_device_status_str(this->device_status[device_type]).c_str());
        this->voice_session_notify_abort(network_id, controller_id, 0, CTRLM_VOICE_SESSION_ABORT_REASON_VOICE_DISABLED);  // TODO Add other abort reasons
        return(VOICE_SESSION_RESPONSE_FAILURE);
    }

    if(CTRLM_VOICE_STATE_SRC_WAITING == this->state_src) {
        // Remove controller status read timeout
        if(this->timeout_ctrl_cmd_status_read > 0) {
            g_source_remove(this->timeout_ctrl_cmd_status_read);
            this->timeout_ctrl_cmd_status_read = 0;
        }
        uuid_clear(this->uuid);

        // Cancel current speech router session
        LOG_INFO("%s: Waiting on the results from previous session, aborting this and continuing..\n", __FUNCTION__);
        xrsr_session_terminate(); // Synchronous - this will take a bit of time.  Might need to revisit this down the road.
    } else if(this->controller_id == controller_id && (this->state_src == CTRLM_VOICE_STATE_SRC_STREAMING || this->state_dst != CTRLM_VOICE_STATE_DST_READY)) {
        // Cancel current speech router session
        LOG_WARN("%s: Session in progress with same controller - src <%s> dst <%s>, aborting this and continuing..\n", __FUNCTION__, ctrlm_voice_state_src_str(this->state_src), ctrlm_voice_state_dst_str(this->state_dst));
        xrsr_session_terminate(); // Synchronous - this will take a bit of time.  Might need to revisit this down the road.
    }

    bool l_session_by_text = (l_transcription_in != NULL);
    if (l_session_by_text) {
        LOG_INFO("%s: Requesting the speech router start a text-only session with transcription = <%s>\n", __FUNCTION__, l_transcription_in);
        if (false == xrsr_session_request(voice_device_to_xrsr(device_type), l_transcription_in)) {
            LOG_ERROR("%s: Failed to acquire the text-only session from the speech router.\n", __FUNCTION__);
            return VOICE_SESSION_RESPONSE_BUSY;
        }
    }

    ctrlm_hal_input_params_t hal_input_params;
    hal_input_params.device                   = voice_device_to_hal(device_type);
    hal_input_params.fd                       = -1;
    hal_input_params.input_format.container   = XRAUDIO_CONTAINER_NONE;
    hal_input_params.input_format.encoding    = voice_format_to_xraudio(format);
    hal_input_params.input_format.sample_rate = 16000;
    hal_input_params.input_format.sample_size = is_standby_microphone() ? 4 : 1;
    hal_input_params.input_format.channel_qty = 1;

    ctrlm_hal_input_object_t hal_input_object = NULL;
    int fds[2] = { -1, -1 };

    if (!l_session_by_text) {
        if (false == use_external_data_pipe) {
            if(!is_standby_microphone()) {
                errno = 0;
                if(pipe(fds) < 0) {
                    int errsv = errno;
                    LOG_ERROR("%s: Failed to create pipe <%s>\n", __FUNCTION__, strerror(errsv));
                    this->voice_session_notify_abort(network_id, controller_id, 0, CTRLM_VOICE_SESSION_ABORT_REASON_FAILURE);
                    return(VOICE_SESSION_RESPONSE_FAILURE);
                } // set to non-blocking
            }

            hal_input_params.fd = fds[PIPE_READ];
            // request the xraudio session and begin
            hal_input_object = ctrlm_xraudio_hal_input_open(&hal_input_params);
        } else {
            // only request the xraudio session, need to begin it later manually
            hal_input_object = ctrlm_xraudio_hal_input_session_req(&hal_input_params);
        }
        if(hal_input_object == NULL) {
            LOG_ERROR("%s: Failed to acquire voice session\n", __FUNCTION__);
            this->voice_session_notify_abort(network_id, controller_id, 0, CTRLM_VOICE_SESSION_ABORT_REASON_BUSY);
            if (false == use_external_data_pipe) {
                close(fds[PIPE_WRITE]);
                close(fds[PIPE_READ]);
            }
            return(VOICE_SESSION_RESPONSE_BUSY);
        }
    }

    this->is_session_by_text        = l_session_by_text;
    this->transcription_in          = l_session_by_text ? l_transcription_in : "";
    this->hal_input_object          = hal_input_object;
    this->state_src                 = CTRLM_VOICE_STATE_SRC_STREAMING;
    this->state_dst                 = CTRLM_VOICE_STATE_DST_REQUESTED;
    this->voice_device              = device_type;
    this->format                    = format;
    this->audio_pipe[PIPE_READ]     = fds[PIPE_READ];
    this->audio_pipe[PIPE_WRITE]    = fds[PIPE_WRITE];
    this->controller_id             = controller_id;
    this->network_id                = network_id;
    this->network_type              = ctrlm_network_type_get(network_id);
    this->last_cmd_id               = 0;
    this->session_active_server     = true;
    this->session_active_controller = true;
    this->controller_name           = controller_name;
    this->controller_version_sw     = sw_version;
    this->controller_version_hw     = hw_version;
    this->controller_voltage        = voltage;
    this->controller_command_status = command_status;
    this->audio_sent_bytes          = 0;
    this->audio_sent_samples        = 0;
    this->packets_processed         = 0;
    this->packets_lost              = 0;
    if(stream_params == NULL) {
       this->has_stream_params  = false;
    } else {
       this->has_stream_params  = true;
       this->stream_params      = *stream_params;

       xrsr_session_keyword_info_set(XRSR_SRC_RCU_FF, stream_params->pre_keyword_sample_qty, stream_params->keyword_sample_qty);
    }

    errno_t safec_rc = memset_s(&this->status, sizeof(this->status), 0, sizeof(this->status));
    ERR_CHK(safec_rc);
    this->status.controller_id  = this->controller_id;
    this->voice_status_set();
    this->last_cmd_id           = 0;
    this->next_cmd_id           = 0;
    this->lqi_total             = 0;
    this->stats_session_id      = voice_session_id_get() + 1;
    memset(&this->stats_reboot, 0, sizeof(this->stats_reboot));
    memset(&this->stats_session, 0, sizeof(this->stats_session));
    this->stats_session.dropped_retry = ULONG_MAX; // Used to indicate whether controller provides stats or not

    #ifdef VOICE_BUFFER_STATS
    voice_buffer_warning_triggered = 0;
    voice_buffer_high_watermark    = 0;
    voice_packet_interval          = voice_packet_interval_get(this->format, this->opus_samples_per_packet);
    #ifdef TIMING_START_TO_FIRST_FRAGMENT
    ctrlm_timestamp_get(&voice_session_begin_timestamp);
    #endif
    #endif

    // Start packet timeout, but not if this is a voice session by text or standby microphone
    if (!this->is_session_by_text && !is_standby_microphone()) {
        if(this->network_type == CTRLM_NETWORK_TYPE_IP) {
            this->timeout_packet_tag = g_timeout_add(3000, ctrlm_voice_packet_timeout, NULL);
        } else {
            this->timeout_packet_tag = g_timeout_add(this->prefs.timeout_packet_initial, ctrlm_voice_packet_timeout, NULL);
        }
    }
    if(timestamp != NULL) {
       this->session_timing.ctrl_request = *timestamp;

       // set response time to 50 ms in case transmission confirm callback doesn't work
       this->session_timing.ctrl_response = *timestamp;
       rdkx_timestamp_add_ms(&this->session_timing.ctrl_response, 50);
    }
    if(cb_confirm != NULL && cb_confirm_param != NULL) {
       *cb_confirm       = ctrlm_voice_session_response_confirm;
       *cb_confirm_param = NULL;
    }

    // Post
    sem_post(&this->vsr_semaphore);

    LOG_DEBUG("%s: Voice session acquired <%d, %d, %s> pipe wr <%d> rd <%d>\n", __FUNCTION__, network_id, controller_id, ctrlm_voice_format_str(format), this->audio_pipe[PIPE_WRITE], this->audio_pipe[PIPE_READ]);
    return (this->prefs.par_voice_enabled) ? VOICE_SESSION_RESPONSE_AVAILABLE_PAR_VOICE : VOICE_SESSION_RESPONSE_AVAILABLE;
}

void ctrlm_voice_t::voice_session_rsp_confirm(bool result, ctrlm_timestamp_t *timestamp) {
   if(this->state_src != CTRLM_VOICE_STATE_SRC_STREAMING) {
       if(this->power_state == CTRLM_POWER_STATE_DEEP_SLEEP || this->power_state == CTRLM_POWER_STATE_STANDBY) {
           LOG_WARN("%s: missed voice session response window after waking from <%s>\n", __FUNCTION__, ctrlm_power_state_str(this->power_state));
       } else {
           LOG_ERROR("%s: No voice session in progress\n", __FUNCTION__);
       }
       return;
   }
   if(!result) {
       LOG_ERROR("%s: failed to send voice session response\n", __FUNCTION__);
       return;
   }

   // Session response transmission is confirmed
   if(timestamp == NULL) {
      rdkx_timestamp_get_realtime(&this->session_timing.ctrl_response);
   } else {
      this->session_timing.ctrl_response = *timestamp;
   }
}

static void ctrlm_voice_data_post_processing_cb (ctrlm_hal_data_cb_params_t *param) {
    g_assert(param);
    if (NULL != param->user_data) {
        ((ctrlm_voice_t *)param->user_data)->voice_session_data_post_processing(param->bytes_sent, "sent", NULL);
    } else {
        LOG_ERROR("%s: voice object NULL\n", __FUNCTION__);
    }
}

void ctrlm_voice_t::voice_session_data_post_processing(int bytes_sent, const char *action, ctrlm_timestamp_t *timestamp) {
    if(this->timeout_packet_tag > 0) {
        g_source_remove(this->timeout_packet_tag);
        // QOS timeout handled at end of this function as it depends on time stamps and samples transmitted
        if(!this->controller_supports_qos()) {
            if(this->network_type == CTRLM_NETWORK_TYPE_IP) {
                this->timeout_packet_tag = g_timeout_add(3000, ctrlm_voice_packet_timeout, NULL);
            } else {
                 this->timeout_packet_tag = g_timeout_add(this->prefs.timeout_packet_subsequent, ctrlm_voice_packet_timeout, NULL);
            }
        }
    }

    #ifdef VOICE_BUFFER_STATS
    ctrlm_timestamp_t before;
    ctrlm_timestamp_get(&before);
    #ifdef TIMING_LAST_FRAGMENT_TO_STOP
    voice_session_last_fragment_timestamp = before;
    #endif

    if(this->audio_sent_bytes == 0) {
        first_fragment = before; // timestamp for start of utterance voice data
        #ifdef TIMING_START_TO_FIRST_FRAGMENT
        LOG_INFO("Session Start to First Fragment: %8lld ms lag\n", ctrlm_timestamp_subtract_ms(voice_session_begin_timestamp, first_fragment));
        #endif
    }

    unsigned long long session_time = this->voice_packet_interval + ctrlm_timestamp_subtract_us(first_fragment, before); // in microseconds

    // The total packets (received + lost)
    uint32_t packets_total = this->packets_processed + this->packets_lost;
    unsigned long long session_delta = (session_time - ((packets_total - 1) * this->voice_packet_interval)); // in microseconds
    unsigned long watermark = (session_delta / this->voice_packet_interval) + 1;
    if(watermark > voice_buffer_high_watermark) {
        voice_buffer_high_watermark = watermark;
    }
    if(session_delta > (VOICE_BUFFER_WARNING_THRESHOLD * this->voice_packet_interval)) {
        voice_buffer_warning_triggered = 1;
    }
    #endif

    if(timestamp != NULL) {
        if(this->audio_sent_bytes == 0) {
            this->session_timing.ctrl_audio_rxd_first = *timestamp;
            this->session_timing.ctrl_audio_rxd_final = *timestamp;
        } else {
            this->session_timing.ctrl_audio_rxd_final = *timestamp;
        }
    }

    this->audio_sent_bytes += bytes_sent;

    // Handle input format
    if(this->format == CTRLM_VOICE_FORMAT_OPUS_XVP) {
       this->audio_sent_samples += this->opus_samples_per_packet; // From opus encoding parameters
    } else if(this->format == CTRLM_VOICE_FORMAT_ADPCM || this->format == CTRLM_VOICE_FORMAT_ADPCM_SKY) {
       this->audio_sent_samples += (bytes_sent - 4) * 2; // 4 byte header, one nibble per sample
    } else { // PCM
       this->audio_sent_samples += bytes_sent >> 1; // 16 bit samples
    }

    if(this->has_stream_params && !this->stream_params.push_to_talk && !this->session_timing.has_keyword) {
       if(this->audio_sent_samples >= (this->stream_params.pre_keyword_sample_qty + this->stream_params.keyword_sample_qty) && timestamp != NULL) {
          this->session_timing.ctrl_audio_rxd_keyword = *timestamp;
          this->session_timing.has_keyword = true;
       }
    }
    if(controller_supports_qos()) {
       guint32 timeout = (this->audio_sent_samples * 2 * 8) / this->prefs.bitrate_minimum;
       signed long long elapsed = rdkx_timestamp_subtract_ms(this->session_timing.ctrl_audio_rxd_first, this->session_timing.ctrl_audio_rxd_final);

       timeout -= elapsed;

       if(elapsed + timeout < this->prefs.time_threshold) {
          timeout = this->prefs.time_threshold - elapsed;
       } else if(timeout > 2000) { // Cap at 2 seconds - effectively interpacket timeout
          timeout = 2000;
       }
       float rx_rate = (elapsed == 0) ? 0 : (this->audio_sent_samples * 2 * 8) / elapsed;

       this->timeout_packet_tag = g_timeout_add(timeout, ctrlm_voice_packet_timeout, NULL);
       LOG_INFO("%s: Audio %s bytes <%lu> samples <%lu> rate <%6.2f kbps> timeout <%lu ms>\n", __FUNCTION__, action, this->audio_sent_bytes, this->audio_sent_samples, rx_rate, timeout);
    } else {
       #ifdef VOICE_BUFFER_STATS
       if(voice_buffer_warning_triggered) {
          LOG_WARN("%s: Audio %s bytes <%lu> samples <%lu> pkt cnt <%3u> elapsed <%8llu ms> lag <%8llu ms> (%4.2f packets)\n", __FUNCTION__, action, this->audio_sent_bytes, this->audio_sent_samples, packets_total, session_time / 1000, session_delta / 1000, (((float)session_delta) / this->voice_packet_interval));
       } else {
          LOG_INFO("%s: Audio %s bytes <%lu> samples <%lu>\n", __FUNCTION__, action, this->audio_sent_bytes, this->audio_sent_samples);
       }
       #else
       LOG_INFO("%s: Audio %s bytes <%lu> samples <%lu>\n", __FUNCTION__, action, this->audio_sent_bytes, this->audio_sent_samples);
       #endif
    }
}

bool ctrlm_voice_t::voice_session_data(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, int fd) {
    bool ret = false;

    if(network_id != this->network_id || controller_id != this->controller_id) {
        LOG_ERROR("%s: Data from wrong controller, ignoring\n", __FUNCTION__);
        return(false); 
    }
    if(this->hal_input_object != NULL && !this->is_session_by_text) {
        if (false == ctrlm_xraudio_hal_input_set_ctrlm_data_read_cb(this->hal_input_object, ctrlm_voice_data_post_processing_cb, (void*)this)) {
            LOG_ERROR("%s: Failed setting post data read callback to ctrlm\n", __FUNCTION__);
            return(false); 
        }

        if (true == (ret = ctrlm_xraudio_hal_input_use_external_pipe(this->hal_input_object, fd))) {
            if(this->audio_pipe[PIPE_READ] >= 0) {
                LOG_INFO("%s: Closing previous pipe - READ fd <%d>\n", __FUNCTION__, this->audio_pipe[PIPE_READ]);
                close(this->audio_pipe[PIPE_READ]);
                this->audio_pipe[PIPE_READ] = -1;
            }
            if(this->audio_pipe[PIPE_WRITE] >= 0) {
                LOG_INFO("%s: Closing previous pipe - WRITE fd <%d>\n", __FUNCTION__, this->audio_pipe[PIPE_WRITE]);
                close(this->audio_pipe[PIPE_WRITE]);
                this->audio_pipe[PIPE_WRITE] = -1;
            }

            LOG_INFO("%s: Setting read pipe to fd <%d> and beginning session...\n", __FUNCTION__, fd);
            this->audio_pipe[PIPE_READ] = fd;

            ctrlm_hal_input_params_t hal_input_params;
            hal_input_params.device = voice_device_to_hal(this->voice_device);
            hal_input_params.fd     = fd;

            hal_input_params.input_format.container   = XRAUDIO_CONTAINER_NONE;
            hal_input_params.input_format.encoding    = voice_format_to_xraudio(this->format);
            hal_input_params.input_format.sample_rate = 16000;
            hal_input_params.input_format.sample_size = 1;
            hal_input_params.input_format.channel_qty = 1;

            ret = ctrlm_xraudio_hal_input_session_begin(this->hal_input_object, &hal_input_params);
        }
    }
    return ret;
}

bool ctrlm_voice_t::voice_session_data(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, const char *buffer, long unsigned int length, ctrlm_timestamp_t *timestamp, uint8_t *lqi) {
    char              local_buf[length + 1];
    long unsigned int bytes_written = 0;
    if(this->state_src != CTRLM_VOICE_STATE_SRC_STREAMING) {
        LOG_ERROR("%s: No voice session in progress\n", __FUNCTION__);
        return(false);
    } else if(network_id != this->network_id || controller_id != this->controller_id) {
        LOG_ERROR("%s: Data from wrong controller, ignoring\n", __FUNCTION__);
        return(false); 
    } else if(this->audio_pipe[PIPE_WRITE] < 0) {
        LOG_ERROR("%s: Pipe doesn't exist\n", __FUNCTION__);
        return(false);
    } else if(length <= 0) {
        LOG_ERROR("%s: Length is <= 0\n", __FUNCTION__);
        return(false);
    }

    uint8_t cmd_id = buffer[0];

    if(this->network_type == CTRLM_NETWORK_TYPE_RF4CE) {
        if(this->last_cmd_id == cmd_id) {
            LOG_INFO("%s: RF4CE Duplicate Voice Packet\n", __FUNCTION__);
            if(this->timeout_packet_tag > 0 && !controller_supports_qos()) {
               g_source_remove(this->timeout_packet_tag);
               this->timeout_packet_tag = g_timeout_add(this->prefs.timeout_packet_subsequent, ctrlm_voice_packet_timeout, NULL);
            }
            return(true);
        }

        // Maintain a running packet received and lost count (which is overwritten by post session stats)
        this->packets_processed++;
        if(this->next_cmd_id != 0 && this->next_cmd_id != cmd_id) {
            this->packets_lost += (cmd_id > this->next_cmd_id) ? (cmd_id - this->next_cmd_id) : (ADPCM_COMMAND_ID_MAX + 1 - this->next_cmd_id) + (cmd_id - ADPCM_COMMAND_ID_MIN);
        }

        this->last_cmd_id = cmd_id;
        this->next_cmd_id = this->last_cmd_id + 1;
        if(this->next_cmd_id > ADPCM_COMMAND_ID_MAX) {
            this->next_cmd_id = ADPCM_COMMAND_ID_MIN;
        }
    }

    const char *action = "dumped";
    if(this->state_dst != CTRLM_VOICE_STATE_DST_READY) { // destination is accepting more data
       action = "sent";
       if(this->format == CTRLM_VOICE_FORMAT_OPUS || this->format == CTRLM_VOICE_FORMAT_OPUS_XVP) {
           // Copy to local buffer to perform a single write to the pipe.  TODO: have the caller reserve 1 bytes at the beginning
           // of the buffer to eliminate this copy
           local_buf[0] = length;
           errno_t safec_rc = memcpy_s(&local_buf[1], sizeof(local_buf)-1, buffer, length);
           ERR_CHK(safec_rc);
           buffer = local_buf;
           length++;
       }

       bytes_written = write(this->audio_pipe[PIPE_WRITE], buffer, length);
       if(bytes_written != length) {
           LOG_ERROR("%s: Failed to write data to pipe: %s\n", __FUNCTION__, strerror(errno));
           return(false);
       }
    }

    if(lqi != NULL) {
        lqi_total += *lqi;
    }

    voice_session_data_post_processing(length, action, timestamp);

    return(true);
}

void ctrlm_voice_t::voice_session_end(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_voice_session_end_reason_t reason, ctrlm_timestamp_t *timestamp, ctrlm_voice_session_end_stats_t *stats) {
    LOG_INFO("%s: voice session end < %s >\n", __FUNCTION__, ctrlm_voice_session_end_reason_str(reason));
    if(this->state_src != CTRLM_VOICE_STATE_SRC_STREAMING) {
        LOG_ERROR("%s: No voice session in progress\n", __FUNCTION__);
        return;
    } else if(network_id != this->network_id || controller_id != this->controller_id) {
        LOG_ERROR("%s: session end from wrong controller, ignoring\n", __FUNCTION__);
        return; 
    }

    if(timestamp != NULL) {
       this->session_timing.ctrl_stop = *timestamp;
    } else {
       rdkx_timestamp_get_realtime(&this->session_timing.ctrl_stop);
    }

    // Stop packet timeout
    if(this->timeout_packet_tag > 0) {
        g_source_remove(this->timeout_packet_tag);
        this->timeout_packet_tag = 0;
    }

    if(this->audio_pipe[PIPE_WRITE] >= 0) {
        LOG_INFO("%s: Close write pipe - fd <%d>\n", __FUNCTION__, this->audio_pipe[PIPE_WRITE]);
        close(this->audio_pipe[PIPE_WRITE]);
        this->audio_pipe[PIPE_WRITE] = -1;
    }
    if(this->hal_input_object != NULL) {
        ctrlm_xraudio_hal_input_close(this->hal_input_object);
        this->hal_input_object = NULL;
    }

    // Send main queue message
    ctrlm_main_queue_msg_voice_session_end_t end = {0};

    end.controller_id       = this->controller_id;
    end.reason              = reason;
    end.utterance_too_short = (this->audio_sent_bytes == 0 ? 1 : 0);
    // Don't need to fill out other info
    if(stats != NULL) {
        stats_session.rf_channel       = stats->rf_channel;
        #ifdef VOICE_BUFFER_STATS
        #ifdef TIMING_LAST_FRAGMENT_TO_STOP
        ctrlm_timestamp_t after;
        ctrlm_timestamp_get(&after);
        unsigned long long lag_time = ctrlm_timestamp_subtract_ns(voice_session_last_fragment_timestamp, after);
        LOG_INFO("Last Fragment to Session Stop: %8llu ms lag\n", lag_time / 1000000);
        #endif
        stats_session.buffer_watermark  = voice_buffer_high_watermark;
        #else
        stats_session.buffer_watermark  = ULONG_MAX;
        #endif
    }
    ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::ind_process_voice_session_end, &end, sizeof(end), NULL, this->network_id);

    // clear session_active_controller for controllers that don't support voice command status
    if(!this->controller_command_status) {
       this->session_active_controller = false;
    }

    // Set a timeout for receiving voice session stats from the controller
    this->timeout_ctrl_session_stats_rxd = g_timeout_add(this->prefs.timeout_stats, ctrlm_voice_controller_session_stats_rxd_timeout, NULL);

    LOG_DEBUG("%s,%d: session_active_server = <%d>, session_active_controller = <%d>\n", __FUNCTION__, __LINE__, session_active_server, session_active_controller);

    // Update source state
    if(this->session_active_controller) {
       this->state_src = CTRLM_VOICE_STATE_SRC_WAITING;
    } else {
       this->state_src = CTRLM_VOICE_STATE_SRC_READY;
       if(!this->session_active_server) {
          voice_session_stats_print();
          voice_session_stats_clear();
       }
    }

    if(is_standby_microphone()) {
        if(!ctrlm_power_state_change(CTRLM_POWER_STATE_ON, false)) {
            LOG_ERROR("%s: failed to set power state!\n");
        }
    }
}

void ctrlm_voice_t::voice_session_controller_stats_rxd_timeout() {
   this->timeout_ctrl_session_stats_rxd = 0;
   ctrlm_get_voice_obj()->voice_session_notify_stats();
}

void ctrlm_voice_t::voice_session_stats(ctrlm_voice_stats_session_t session) {
   stats_session.available      = session.available;
   stats_session.packets_total  = session.packets_total;
   stats_session.dropped_retry  = session.dropped_retry;
   stats_session.dropped_buffer = session.dropped_buffer;
   stats_session.retry_mac      = session.retry_mac;
   stats_session.retry_network  = session.retry_network;
   stats_session.cca_sense      = session.cca_sense;

   voice_session_notify_stats();
}

void ctrlm_voice_t::voice_session_stats(ctrlm_voice_stats_reboot_t reboot) {
   stats_reboot = reboot;

   // Send the notification
   voice_session_notify_stats();
}

void ctrlm_voice_t::voice_session_notify_stats() {
   if(stats_session_id == 0) {
      LOG_INFO("%s: already sent, ignoring.\n", __FUNCTION__);
      return;
   }
   if(stats_session_id != this->ipc_common_data.session_id_ctrlm) {
      LOG_INFO("%s: stale session, ignoring.\n", __FUNCTION__);
      return;
   }

   // Stop session stats timeout
   if(this->timeout_ctrl_session_stats_rxd > 0) {
      g_source_remove(this->timeout_ctrl_session_stats_rxd);
      this->timeout_ctrl_session_stats_rxd = 0;
   }

   // Send session stats only if the server voice session has ended
   if(!this->session_active_server && this->voice_ipc) {
      ctrlm_voice_ipc_event_session_statistics_t stats;

      stats.common  = this->ipc_common_data;
      stats.session = this->stats_session;
      stats.reboot  = this->stats_reboot;

      this->voice_ipc->session_statistics(stats);
      stats_session_id = 0;
   }
}

void ctrlm_voice_t::voice_session_timeout() {
   this->timeout_packet_tag = 0;
   ctrlm_voice_session_end_reason_t reason = CTRLM_VOICE_SESSION_END_REASON_TIMEOUT_INTERPACKET;
   if(this->audio_sent_bytes == 0) {
      reason = CTRLM_VOICE_SESSION_END_REASON_TIMEOUT_FIRST_PACKET;
   } else if(controller_supports_qos()) {
      // Bitrate = transmitted PCM data size / elapsed time

      ctrlm_timestamp_t timestamp;
      rdkx_timestamp_get_realtime(&timestamp);
      signed long long elapsed = rdkx_timestamp_subtract_ms(this->session_timing.ctrl_audio_rxd_first, timestamp);
      float rx_rate = (elapsed == 0) ? 0 : (this->audio_sent_samples * 2 * 8) / elapsed;

      LOG_INFO("%s: elapsed time <%llu> ms rx samples <%u> rate <%6.1f> kbps\n", __FUNCTION__, elapsed, this->audio_sent_samples, rx_rate);
      reason = CTRLM_VOICE_SESSION_END_REASON_MINIMUM_QOS;
   }
   LOG_INFO("%s: %s\n", __FUNCTION__, ctrlm_voice_session_end_reason_str(reason));
   this->voice_session_end(this->network_id, this->controller_id, reason);
}

void ctrlm_voice_t::voice_session_stats_clear() {
   ctrlm_voice_session_timing_t *timing = &this->session_timing;

   timing->available              = false;
   timing->connect_attempt        = false;
   timing->connect_success        = false;
   timing->has_keyword            = false;
   timing->ctrl_request           = { 0, 0 };
   timing->ctrl_response          = { 0, 0 };
   timing->ctrl_audio_rxd_first   = { 0, 0 };
   timing->ctrl_audio_rxd_keyword = { 0, 0 };
   timing->ctrl_audio_rxd_final   = { 0, 0 };
   timing->ctrl_stop              = { 0, 0 };
   timing->ctrl_cmd_status_wr     = { 0, 0 };
   timing->ctrl_cmd_status_rd     = { 0, 0 };
   timing->srvr_request           = { 0, 0 };
   timing->srvr_connect           = { 0, 0 };
   timing->srvr_init_txd          = { 0, 0 };
   timing->srvr_audio_txd_first   = { 0, 0 };
   timing->srvr_audio_txd_keyword = { 0, 0 };
   timing->srvr_audio_txd_final   = { 0, 0 };
   timing->srvr_rsp_keyword       = { 0, 0 };
   timing->srvr_disconnect        = { 0, 0 };
}

void ctrlm_voice_t::voice_session_stats_print() {
   ctrlm_voice_session_timing_t *timing = &this->session_timing;
   errno_t safec_rc = -1;
   if(!timing->available) {
      LOG_INFO("%s: not available\n", __FUNCTION__);
      return;
   }

   signed long long ctrl_response, ctrl_audio_rxd_first, ctrl_audio_rxd_final, ctrl_audio_rxd_stop, ctrl_cmd_status_wr;
   ctrl_response        = rdkx_timestamp_subtract_us(timing->ctrl_request,         timing->ctrl_response);
   ctrl_audio_rxd_first = rdkx_timestamp_subtract_us(timing->ctrl_response,        timing->ctrl_audio_rxd_first);

   ctrl_cmd_status_wr   = rdkx_timestamp_subtract_us(timing->ctrl_request,         timing->ctrl_cmd_status_wr);
   

   char str_keyword[40];
   str_keyword[0] = '\0';
   if(timing->has_keyword) { // Far field with keyword
      signed long long ctrl_audio_rxd_kwd;

      ctrl_audio_rxd_kwd   = rdkx_timestamp_subtract_us(timing->ctrl_audio_rxd_first,   timing->ctrl_audio_rxd_keyword);
      ctrl_audio_rxd_final = rdkx_timestamp_subtract_us(timing->ctrl_audio_rxd_keyword, timing->ctrl_audio_rxd_final);
      safec_rc = sprintf_s(str_keyword, sizeof(str_keyword), "keyword <%lld> ", ctrl_audio_rxd_kwd);
      if(safec_rc < EOK) {
        ERR_CHK(safec_rc);
      }
   } else {
      ctrl_audio_rxd_final = rdkx_timestamp_subtract_us(timing->ctrl_audio_rxd_first, timing->ctrl_audio_rxd_final);
   }
   ctrl_audio_rxd_stop = rdkx_timestamp_subtract_us(timing->ctrl_audio_rxd_final, timing->ctrl_stop);

   LOG_INFO("%s: ctrl rsp <%lld> first <%lld> %sfinal <%lld> stop <%lld> us\n", __FUNCTION__, ctrl_response, ctrl_audio_rxd_first, str_keyword, ctrl_audio_rxd_final, ctrl_audio_rxd_stop);
   LOG_INFO("%s: ctrl cmd status wr <%lld> us\n", __FUNCTION__, ctrl_cmd_status_wr);

   if(timing->connect_attempt) { // Attempted connection
      signed long long srvr_request, srvr_init_txd, srvr_disconnect;
      srvr_request = rdkx_timestamp_subtract_us(timing->ctrl_request, timing->srvr_request);

      if(!timing->connect_success) { // Did not connect
         srvr_disconnect = rdkx_timestamp_subtract_us(timing->srvr_request, timing->srvr_disconnect);
         LOG_INFO("%s: srvr request <%lld> disconnect <%lld> us\n", __FUNCTION__, srvr_request, srvr_disconnect);
      } else { // Connected
         signed long long srvr_connect, srvr_audio_txd_first, srvr_audio_txd_final;
         srvr_connect         = rdkx_timestamp_subtract_us(timing->srvr_request,  timing->srvr_connect);
         srvr_init_txd        = rdkx_timestamp_subtract_us(timing->srvr_connect,  timing->srvr_init_txd);
         srvr_audio_txd_first = rdkx_timestamp_subtract_us(timing->srvr_init_txd, timing->srvr_audio_txd_first);

         if(timing->has_keyword) { // Far field with keyword
            signed long long srvr_audio_txd_kwd, srvr_audio_kwd_verify;

            srvr_audio_txd_kwd    = rdkx_timestamp_subtract_us(timing->srvr_audio_txd_first,   timing->srvr_audio_txd_keyword);
            srvr_audio_txd_final  = rdkx_timestamp_subtract_us(timing->srvr_audio_txd_keyword, timing->srvr_audio_txd_final);
            srvr_audio_kwd_verify = rdkx_timestamp_subtract_us(timing->srvr_audio_txd_keyword, timing->srvr_rsp_keyword);
            safec_rc = sprintf_s(str_keyword, sizeof(str_keyword), "keyword <%lld> verify <%lld> ", srvr_audio_txd_kwd, srvr_audio_kwd_verify);
            if(safec_rc < EOK) {
              ERR_CHK(safec_rc);
            }
         } else {
            srvr_audio_txd_final = rdkx_timestamp_subtract_us(timing->srvr_audio_txd_first, timing->srvr_audio_txd_final);
         }
         srvr_disconnect = rdkx_timestamp_subtract_us(timing->srvr_audio_txd_final, timing->srvr_disconnect);

         LOG_INFO("%s: srvr request <%lld> connect <%lld> init <%lld> first <%lld> %sfinal <%lld> disconnect <%lld> us\n", __FUNCTION__, srvr_request, srvr_connect, srvr_init_txd, srvr_audio_txd_first, str_keyword, srvr_audio_txd_final, srvr_disconnect);

         if(timing->has_keyword) {
            signed long long kwd_total, kwd_transmit, kwd_response;

            kwd_total    = rdkx_timestamp_subtract_us(timing->ctrl_request, timing->srvr_rsp_keyword);
            kwd_transmit = rdkx_timestamp_subtract_us(timing->ctrl_request, timing->srvr_audio_txd_keyword);
            kwd_response = rdkx_timestamp_subtract_us(timing->srvr_audio_txd_keyword, timing->srvr_rsp_keyword);
            LOG_INFO("%s: keyword latency total <%lld> transmit <%lld> response <%lld> us\n", __FUNCTION__, kwd_total, kwd_transmit, kwd_response);
         }
      }
   }
}

void ctrlm_voice_t::voice_session_info(ctrlm_voice_session_info_t *data) {
    if(data) {
        data->controller_name       = this->controller_name;
        data->controller_version_sw = this->controller_version_sw;
        data->controller_version_hw = this->controller_version_hw;
        data->controller_voltage    = this->controller_voltage;
        data->stb_name              = this->stb_name;
        data->ffv_leading_samples   = this->prefs.ffv_leading_samples;
        data->has_stream_params     = this->has_stream_params;
        if(data->has_stream_params) {
            data->stream_params = this->stream_params;
        }
    }
}

void ctrlm_voice_t::voice_session_info_reset() {
    this->controller_name        = "";
    this->controller_version_sw  = "";
    this->controller_version_hw  = "";
    this->controller_voltage     = 0.0;
    this->has_stream_params      = false;
}

ctrlm_voice_state_src_t ctrlm_voice_t::voice_state_src_get() const {
    return(this->state_src);
}

ctrlm_voice_state_dst_t ctrlm_voice_t::voice_state_dst_get() const {
    return(this->state_dst);
}

void ctrlm_voice_t::voice_stb_data_stb_sw_version_set(std::string &sw_version) {
    LOG_DEBUG("%s: STB version set to %s\n", __FUNCTION__, sw_version.c_str());
    this->software_version = sw_version;
    for(const auto &itr : this->endpoints) {
        itr->voice_stb_data_stb_sw_version_set(sw_version);
    }
}

std::string ctrlm_voice_t::voice_stb_data_stb_sw_version_get() const {
    return this->software_version;
}

void ctrlm_voice_t::voice_stb_data_stb_name_set(std::string &stb_name) {
    LOG_DEBUG("%s: STB name set to %s\n", __FUNCTION__, stb_name.c_str());
    this->stb_name = stb_name;
    for(const auto &itr : this->endpoints) {
        itr->voice_stb_data_stb_name_set(stb_name);
    }
}

std::string ctrlm_voice_t::voice_stb_data_stb_name_get() const {
    return(this->stb_name);
}

void ctrlm_voice_t::voice_stb_data_account_number_set(std::string &account_number) {
    LOG_DEBUG("%s: Account number set to %s\n", __FUNCTION__, account_number.c_str());
    this->account_number = account_number;
    for(const auto &itr : this->endpoints) {
        itr->voice_stb_data_account_number_set(account_number);
    }
}

std::string ctrlm_voice_t::voice_stb_data_account_number_get() const {
    return(this->account_number);
}

void ctrlm_voice_t::voice_stb_data_receiver_id_set(std::string &receiver_id) {
    LOG_DEBUG("%s: Receiver ID set to %s\n", __FUNCTION__, receiver_id.c_str());
    this->receiver_id = receiver_id;
    for(const auto &itr : this->endpoints) {
        itr->voice_stb_data_receiver_id_set(receiver_id);
    }
}

std::string ctrlm_voice_t::voice_stb_data_receiver_id_get() const {
    return(this->receiver_id);
}

void ctrlm_voice_t::voice_stb_data_device_id_set(std::string &device_id) {
    LOG_DEBUG("%s: Device ID set to %s\n", __FUNCTION__, device_id.c_str());
    this->device_id = device_id;
    for(const auto &itr : this->endpoints) {
        itr->voice_stb_data_device_id_set(device_id);
    }
}

std::string ctrlm_voice_t::voice_stb_data_device_id_get() const {
    return(this->device_id);
}

void ctrlm_voice_t::voice_stb_data_partner_id_set(std::string &partner_id) {
    LOG_DEBUG("%s: Partner ID set to %s\n", __FUNCTION__, partner_id.c_str());
    this->partner_id = partner_id;
    for(const auto &itr : this->endpoints) {
        itr->voice_stb_data_partner_id_set(partner_id);
    }
}

std::string ctrlm_voice_t::voice_stb_data_partner_id_get() const {
    return(this->partner_id);
}
 
void ctrlm_voice_t::voice_stb_data_experience_set(std::string &experience) {
    LOG_DEBUG("%s: Experience Tag set to %s\n", __FUNCTION__, experience.c_str());
    this->experience = experience;
    for(const auto &itr : this->endpoints) {
        itr->voice_stb_data_experience_set(experience);
    }
}

std::string ctrlm_voice_t::voice_stb_data_experience_get() const {
    return(this->experience);
}

std::string ctrlm_voice_t::voice_stb_data_app_id_http_get() const {
    return(this->prefs.app_id_http);
}

std::string ctrlm_voice_t::voice_stb_data_app_id_ws_get() const {
    return(this->prefs.app_id_ws);
}

void ctrlm_voice_t::voice_stb_data_guide_language_set(const char *language) {
   LOG_DEBUG("%s: Guide language set to %s\n", __FUNCTION__, language);
   this->prefs.guide_language = language;
   for(const auto &itr : this->endpoints) {
        itr->voice_stb_data_guide_language_set(language);
   }
}

std::string ctrlm_voice_t::voice_stb_data_guide_language_get() const {
   return(this->prefs.guide_language);
}

void ctrlm_voice_t::voice_stb_data_sat_set(std::string &sat_token) {
    LOG_DEBUG("%s: SAT Token set to %s\n", __FUNCTION__, sat_token.c_str());

    errno_t safec_rc = strcpy_s(this->sat_token, sizeof(this->sat_token), sat_token.c_str());
    ERR_CHK(safec_rc);
}

const char *ctrlm_voice_t::voice_stb_data_sat_get() const {
    return(this->sat_token);
}

bool ctrlm_voice_t::voice_stb_data_test_get() const {
    return(this->prefs.vrex_test_flag);
}

bool ctrlm_voice_t::voice_session_has_stb_data() {
#if defined(AUTH_RECEIVER_ID) || defined(AUTH_DEVICE_ID)
    if(this->receiver_id == "" && this->device_id == "") {
        LOG_INFO("%s: No receiver/device id\n", __FUNCTION__);
        return(false);
    }
#endif
#ifdef AUTH_PARTNER_ID
    if(this->partner_id == "") {
        LOG_INFO("%s: No partner id\n", __FUNCTION__);
        return(false);
    }
#endif
#ifdef AUTH_EXPERIENCE
    if(this->experience == "") {
        LOG_INFO("%s: No experience tag\n", __FUNCTION__);
        return(false);
    }
#endif
#ifdef AUTH_SAT_TOKEN
    if(this->sat_token_required && this->sat_token[0] == '\0') {
        LOG_INFO("%s: No SAT token\n", __FUNCTION__);
        return(false);
    }
#endif
    return(true);
}

unsigned long ctrlm_voice_t::voice_session_id_next() {
    if(this->session_id == 0xFFFFFFFF) {
        this->session_id = 1;
    } else {
        this->session_id++;
    }
    return(this->session_id);
}

void ctrlm_voice_t::voice_session_notify_abort(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, unsigned long session_id, ctrlm_voice_session_abort_reason_t reason) {
    LOG_INFO("%s: voice session abort < %s >\n", __FUNCTION__, ctrlm_voice_session_abort_reason_str(reason));
    if(this->voice_ipc) {
        ctrlm_voice_ipc_event_session_end_t end;
        // Do not use ipc_common_data attribute, as this function is called before voice session is acquired so it is not accurate.
        end.common.network_id        = network_id;
        end.common.network_type      = ctrlm_network_type_get(network_id);
        end.common.controller_id     = controller_id;
        end.common.session_id_ctrlm  = session_id;
        end.common.session_id_server = "N/A";
        end.result                   = SESSION_END_ABORT;
        end.reason                   = (int)reason;
        this->voice_ipc->session_end(end);
    }

    //device is INVALID here so cannot use is_standby_microphone
    if(ctrlm_main_get_power_state() == CTRLM_POWER_STATE_STANDBY_VOICE_SESSION) {
        if(!ctrlm_power_state_change(CTRLM_POWER_STATE_ON, false)) {
            LOG_ERROR("%s: failed to set power state!\n");
        }
    }
}
// Application Interface Implementation End

// Callback Interface Implementation
void ctrlm_voice_t::voice_session_begin_callback(ctrlm_voice_session_begin_cb_t *session_begin) {
    if(NULL == session_begin) {
        LOG_ERROR("%s: NULL data\n", __FUNCTION__);
        return;
    }

    if(!uuid_is_null(session_begin->header.uuid)) {
        char uuid_str[37] = {'\0'};
        uuid_unparse_lower(session_begin->header.uuid, uuid_str);
        this->trx  = uuid_str;
        uuid_copy(this->uuid, session_begin->header.uuid);
    }

    uuid_copy(this->uuid, session_begin->header.uuid);
    this->confidence                        = 0;
    this->dual_sensitivity_immediate        = false;
    this->transcription                     = "";
    this->message                           = "";
    this->server_ret_code                   = 0;
    this->voice_device                      = xrsr_to_voice_device(session_begin->src);
    if( CTRLM_VOICE_DEVICE_MICROPHONE == this->voice_device )
    {
       this->network_id = CTRLM_MAIN_NETWORK_ID_DSP;
       this->controller_id = CTRLM_MAIN_CONTROLLER_ID_DSP;
       this->network_type = CTRLM_NETWORK_TYPE_DSP;
       this->keyword_verified = false;
       LOG_INFO("%s setting network and controller to DSP from device type\n",__FUNCTION__);
    }
    this->ipc_common_data.network_id        = this->network_id;
    this->ipc_common_data.network_type      = ctrlm_network_type_get(this->network_id);
    this->ipc_common_data.controller_id     = this->controller_id;
    this->ipc_common_data.session_id_ctrlm  = this->voice_session_id_next();
    this->ipc_common_data.session_id_server = this->trx;
    this->ipc_common_data.voice_assistant   = is_voice_assistant(this->voice_device);
    this->ipc_common_data.device_type       = this->voice_device;
    this->endpoint_current                  = session_begin->endpoint;
    this->end_reason                        = CTRLM_VOICE_SESSION_END_REASON_DONE;

    errno_t safec_rc = memset_s(&this->status, sizeof(this->status), 0 , sizeof(this->status));
    ERR_CHK(safec_rc);
    this->status.controller_id  = this->controller_id;
    this->voice_status_set();

    this->session_timing.srvr_request = session_begin->header.timestamp;
    this->session_timing.connect_attempt = true;

    // Parse the stream params
    if(this->has_stream_params) {
        this->dual_sensitivity_immediate = (!this->stream_params.high_search_pt_support || (this->stream_params.high_search_pt_support && this->stream_params.standard_search_pt_triggered));
        for(int i = 0; i < CTRLM_FAR_FIELD_BEAMS_MAX; i++) {
            if(this->stream_params.beams[i].selected) {
                if(this->stream_params.beams[i].confidence_normalized) {
                    this->confidence                       = this->stream_params.beams[i].confidence;
                }
            }
        }
    }

    LOG_INFO("%s: session begin\n", __FUNCTION__);

    // Check if we should change audio states
    bool kw_verification_required = false;
    if(is_voice_assistant(this->voice_device)) {
        if(this->audio_timing == CTRLM_VOICE_AUDIO_TIMING_VSR || 
          (this->audio_timing == CTRLM_VOICE_AUDIO_TIMING_CONFIDENCE && this->confidence >= this->audio_confidence_threshold) ||
          (this->audio_timing == CTRLM_VOICE_AUDIO_TIMING_DUAL_SENSITIVITY && this->dual_sensitivity_immediate)) {
            this->voice_keyword_verified_action();
        } else { // Await keyword verification
            kw_verification_required = true;
        }
    }

    // Update device status
    this->voice_session_set_active(this->voice_device);

    // IARM Begin Event
    if(this->voice_ipc) {
        ctrlm_voice_ipc_event_session_begin_t begin;
        begin.common                        = this->ipc_common_data;
        begin.mime_type                     = CTRLM_VOICE_MIMETYPE_ADPCM;
        begin.sub_type                      = CTRLM_VOICE_SUBTYPE_ADPCM;
        begin.language                      = this->prefs.guide_language;
        begin.keyword_verification          = session_begin->keyword_verification;
        begin.keyword_verification_required = kw_verification_required;
        this->voice_ipc->session_begin(begin);
    }

    if(this->state_dst != CTRLM_VOICE_STATE_DST_REQUESTED) {
        LOG_WARN("%s: unexpected dst state <%s>\n", __FUNCTION__, ctrlm_voice_state_dst_str(this->state_dst));
    }
    this->state_dst = CTRLM_VOICE_STATE_DST_OPENED;


    if (this->is_session_by_text) {
        LOG_WARN("%s: Ending voice session immediately because this is a text-only session\n", __FUNCTION__);
        this->voice_session_end(this->network_id, this->controller_id, CTRLM_VOICE_SESSION_END_REASON_DONE);
    }
}

void ctrlm_voice_t::voice_session_end_callback(ctrlm_voice_session_end_cb_t *session_end) {
    if(NULL == session_end) {
        LOG_ERROR("%s: NULL data\n", __FUNCTION__);
        return;
    }
    errno_t safec_rc = -1;

    //If this is a voice assistant, unmute the audio
    if(is_voice_assistant(this->voice_device)) {
        this->audio_state_set(false);
    }

    xrsr_session_stats_t *stats = &session_end->stats;

    if(stats == NULL) {
        LOG_ERROR("%s: stats are NULL\n", __FUNCTION__);
        return;
    }

    if(this->status.status == VOICE_COMMAND_STATUS_PENDING) {
        switch(this->voice_device) {
            case CTRLM_VOICE_DEVICE_FF: {
                this->status.status = (stats->reason == XRSR_SESSION_END_REASON_EOS) ? VOICE_COMMAND_STATUS_SUCCESS : VOICE_COMMAND_STATUS_FAILURE;
                this->voice_status_set();
                break;
            }
            case CTRLM_VOICE_DEVICE_MICROPHONE: {
                this->status.status = (session_end->success ? VOICE_COMMAND_STATUS_SUCCESS : VOICE_COMMAND_STATUS_FAILURE);
                // No need to set, as it's not a controller
                break;
            }
            default: {
                break;
            }
        }
    }

    LOG_INFO("%s: audio sent bytes <%u> samples <%u> reason <%s> voice command status <%s>\n", __FUNCTION__, this->audio_sent_bytes, this->audio_sent_samples, xrsr_session_end_reason_str(stats->reason), ctrlm_voice_command_status_str(this->status.status));

    // Update device status
    this->voice_session_set_inactive(this->voice_device);

    if (this->is_session_by_text) {
        // if this is a text only session, we don't get an ASR message from vrex with the transcription so copy it here.
        this->transcription = this->transcription_in;
    }

    // Send Results IARM Event
    if(this->voice_ipc) {
        if(stats->reason == XRSR_SESSION_END_REASON_ERROR_AUDIO_DURATION) {
            ctrlm_voice_ipc_event_session_end_t end;
            end.common = this->ipc_common_data;
            end.result = SESSION_END_SHORT_UTTERANCE;
            end.reason = (int)this->end_reason;
            this->voice_ipc->session_end(end);
        } else {
            ctrlm_voice_ipc_event_session_end_server_stats_t server_stats;
            ctrlm_voice_ipc_event_session_end_t end;
            end.common                       = this->ipc_common_data;
            end.result                       = (session_end->success ? SESSION_END_SUCCESS : SESSION_END_FAILURE);
            end.reason                       = stats->reason;
            end.return_code_protocol         = stats->ret_code_protocol;
            end.return_code_protocol_library = stats->ret_code_library;
            end.return_code_server           = this->server_ret_code;
            end.return_code_server_str       = this->message;
            end.return_code_internal         = stats->ret_code_internal;
            end.transcription                = this->transcription;
            
            if(stats->server_ip[0] != 0) {
                server_stats.server_ip = stats->server_ip;
            }
            server_stats.dns_time = stats->time_dns;
            server_stats.connect_time = stats->time_connect;
            end.server_stats = &server_stats;

            this->voice_ipc->session_end(end);
        }
    }

    // Update controller metrics
    ctrlm_main_queue_msg_controller_voice_metrics_t metrics = {0};
    metrics.controller_id       = this->controller_id;
    metrics.short_utterance     = (stats->reason == XRSR_SESSION_END_REASON_ERROR_AUDIO_DURATION ? 1 : 0);
    metrics.packets_total       = this->packets_processed + this->packets_lost;
    metrics.packets_lost        = this->packets_lost;
    ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::process_voice_controller_metrics, &metrics, sizeof(metrics), NULL, this->network_id);

    // Send results internally to controlMgr
    if(this->network_id != CTRLM_MAIN_NETWORK_ID_INVALID && this->controller_id != CTRLM_MAIN_CONTROLLER_ID_INVALID) {
        ctrlm_main_queue_msg_voice_session_result_t msg = {0};
        msg.controller_id     = this->controller_id;
        safec_rc = strncpy_s(msg.transcription, sizeof(msg.transcription), this->transcription.c_str(), sizeof(msg.transcription)-1);
        ERR_CHK(safec_rc);
        msg.transcription[CTRLM_VOICE_SESSION_TEXT_MAX_LENGTH - 1] = '\0';
        msg.success           =  (session_end->success == true ? 1 : 0);
        ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::ind_process_voice_session_result, &msg, sizeof(msg), NULL, this->network_id);
    }

    if(this->voice_device == CTRLM_VOICE_DEVICE_FF && this->status.status == VOICE_COMMAND_STATUS_PENDING) {
        this->status.status = (session_end->success ? VOICE_COMMAND_STATUS_SUCCESS : VOICE_COMMAND_STATUS_FAILURE);
    }

    if(this->state_dst != CTRLM_VOICE_STATE_DST_OPENED) {
        LOG_WARN("%s: unexpected dst state <%s>\n", __FUNCTION__, ctrlm_voice_state_dst_str(this->state_dst));
    }

    this->session_active_server = false;
    if(this->state_src == CTRLM_VOICE_STATE_SRC_STREAMING) {
        voice_session_end(this->network_id, this->controller_id, CTRLM_VOICE_SESSION_END_REASON_OTHER_ERROR);
    } else if(!this->session_active_controller) {
        this->state_src = CTRLM_VOICE_STATE_SRC_READY;

        // Send session stats only if the session stats have been received from the controller (or it has timed out)
        if(stats_session_id != 0 && this->voice_ipc) {
            ctrlm_voice_ipc_event_session_statistics_t stats;
            stats.common  = this->ipc_common_data;
            stats.session = this->stats_session;
            stats.reboot  = this->stats_reboot;

            this->voice_ipc->session_statistics(stats);
        }
    }

    this->state_dst = CTRLM_VOICE_STATE_DST_READY;
    this->endpoint_current = NULL;
    voice_session_info_reset();

    // Check for incorrect semaphore values
    int val;
    sem_getvalue(&this->vsr_semaphore, &val);
    if(val > 0) {
        LOG_ERROR("%s: VSR semaphore has invalid value... resetting..\n", __FUNCTION__);
        for(int i = 0; i < val; i++) {
            sem_wait(&this->vsr_semaphore);
        }
    }
}

void ctrlm_voice_t::voice_server_message_callback(const char *msg, unsigned long length) {
    if(this->voice_ipc) {
        this->voice_ipc->server_message(msg, length);
    }
}

void ctrlm_voice_t::voice_server_connected_callback(ctrlm_voice_cb_header_t *connected) {
   this->session_timing.srvr_connect           = connected->timestamp;
   this->session_timing.srvr_init_txd          = this->session_timing.srvr_connect;
   this->session_timing.srvr_audio_txd_first   = this->session_timing.srvr_connect;
   this->session_timing.srvr_audio_txd_keyword = this->session_timing.srvr_connect;
   this->session_timing.srvr_audio_txd_final   = this->session_timing.srvr_connect;
   this->session_timing.srvr_rsp_keyword       = this->session_timing.srvr_connect;

   this->session_timing.connect_success = true;
}

void ctrlm_voice_t::voice_server_disconnected_callback(ctrlm_voice_disconnected_cb_t *disconnected) {
    this->session_timing.srvr_disconnect = disconnected->header.timestamp;
    this->session_timing.available = true;
}

void ctrlm_voice_t::voice_server_sent_init_callback(ctrlm_voice_cb_header_t *init) {
    this->session_timing.srvr_init_txd = init->timestamp;
}

void ctrlm_voice_t::voice_stream_begin_callback(ctrlm_voice_stream_begin_cb_t *stream_begin) {
    if(NULL == stream_begin) {
        LOG_ERROR("%s: NULL data\n", __FUNCTION__);
        return;
    }

   this->session_timing.srvr_audio_txd_first   = stream_begin->header.timestamp;

   this->session_timing.srvr_audio_txd_keyword = this->session_timing.srvr_audio_txd_first;
   this->session_timing.srvr_audio_txd_final   = this->session_timing.srvr_audio_txd_first;

   if(this->state_dst != CTRLM_VOICE_STATE_DST_OPENED) {
      LOG_WARN("%s: unexpected dst state <%s>\n", __FUNCTION__, ctrlm_voice_state_dst_str(this->state_dst));
   }
   this->state_dst = CTRLM_VOICE_STATE_DST_STREAMING;
}

void ctrlm_voice_t::voice_stream_kwd_callback(ctrlm_voice_cb_header_t *kwd) {
   if(NULL == kwd) {
       LOG_ERROR("%s: NULL data\n", __FUNCTION__);
       return;
   }

   this->session_timing.srvr_audio_txd_keyword = kwd->timestamp;

   this->session_timing.srvr_audio_txd_final   = this->session_timing.srvr_audio_txd_keyword;
}

void ctrlm_voice_t::voice_stream_end_callback(ctrlm_voice_stream_end_cb_t *stream_end) {
    if(NULL == stream_end) {
        LOG_ERROR("%s: NULL data\n", __FUNCTION__);
        return;
    }

    xrsr_stream_stats_t *stats = &stream_end->stats;
    if(!stats->audio_stats.valid) {
        LOG_INFO("%s: end of stream <%s> audio stats not present\n", __FUNCTION__, (stats->result) ? "SUCCESS" : "FAILURE");
        this->packets_processed           = 0;
        this->packets_lost                = 0;
        this->stats_session.packets_total = 0;
        this->stats_session.packets_lost  = 0;
        this->stats_session.link_quality  = 0;

    } else {
        this->packets_processed           = stats->audio_stats.packets_processed;
        this->packets_lost                = stats->audio_stats.packets_lost;
        this->stats_session.available     = 1;
        this->stats_session.packets_total = this->packets_lost + this->packets_processed;
        this->stats_session.packets_lost  = this->packets_lost;
        this->stats_session.link_quality  = (this->packets_processed > 0) ? (this->lqi_total / this->packets_processed) : 0;

        uint32_t samples_processed    = stats->audio_stats.samples_processed;
        uint32_t samples_lost         = stats->audio_stats.samples_lost;
        uint32_t decoder_failures     = stats->audio_stats.decoder_failures;
        uint32_t samples_buffered_max = stats->audio_stats.samples_buffered_max;

        LOG_INFO("%s: end of stream <%s>\n", __FUNCTION__, (stats->result) ? "SUCCESS" : "FAILURE");

        if(this->packets_processed > 0) {
            LOG_INFO("%s: Packets Lost/Total <%u/%u> %.02f%%\n", __FUNCTION__, this->packets_lost, this->packets_lost + this->packets_processed, 100.0 * ((double)this->packets_lost / (double)(this->packets_lost + this->packets_processed)));
        }
        if(samples_processed > 0) {
            LOG_INFO("%s: Samples Lost/Total <%u/%u> %.02f%% buffered max <%u>\n", __FUNCTION__, samples_lost, samples_lost + samples_processed, 100.0 * ((double)samples_lost / (double)(samples_lost + samples_processed)), samples_buffered_max);
        }
        if(decoder_failures > 0) {
            LOG_WARN("%s: decoder failures <%u>\n", __FUNCTION__, decoder_failures);
        }
    }

    if(CTRLM_VOICE_DEVICE_PTT == this->voice_device && this->status.status == VOICE_COMMAND_STATUS_PENDING) { // Set voice command status
        this->status.status = (stats->result) ? VOICE_COMMAND_STATUS_SUCCESS : VOICE_COMMAND_STATUS_FAILURE;
        this->voice_status_set();
    }

    //If this is a voice assistant, unmute the audio
    if(is_voice_assistant(this->voice_device)) {
        this->audio_state_set(false);
    }

    if(this->voice_ipc) {
        // This is a STREAM end..
        ctrlm_voice_ipc_event_stream_end_t end;
        end.common = this->ipc_common_data;
        end.reason = (int)this->end_reason;
        this->voice_ipc->stream_end(end);
    }

    if(this->state_dst != CTRLM_VOICE_STATE_DST_STREAMING) {
       LOG_WARN("%s: unexpected dst state <%s>\n", __FUNCTION__, ctrlm_voice_state_dst_str(this->state_dst));
    }
    this->state_dst = CTRLM_VOICE_STATE_DST_OPENED;

    this->session_timing.srvr_audio_txd_final = stream_end->header.timestamp;
}

void ctrlm_voice_t::voice_action_tv_mute_callback(bool mute) {
    this->status.status          = VOICE_COMMAND_STATUS_TV_AVR_CMD;
    this->status.data.tv_avr.cmd = CTRLM_VOICE_TV_AVR_CMD_VOLUME_MUTE;
}

void ctrlm_voice_t::voice_action_tv_power_callback(bool power, bool toggle) {
    this->status.status                      = VOICE_COMMAND_STATUS_TV_AVR_CMD;
    this->status.data.tv_avr.cmd             = power ? CTRLM_VOICE_TV_AVR_CMD_POWER_ON : CTRLM_VOICE_TV_AVR_CMD_POWER_OFF;
    this->status.data.tv_avr.toggle_fallback = (this->prefs.force_toggle_fallback ? true : toggle);
}

void ctrlm_voice_t::voice_action_tv_volume_callback(bool up, uint32_t repeat_count) {
    this->status.status                      = VOICE_COMMAND_STATUS_TV_AVR_CMD;
    this->status.data.tv_avr.cmd             = up ? CTRLM_VOICE_TV_AVR_CMD_VOLUME_UP : CTRLM_VOICE_TV_AVR_CMD_VOLUME_DOWN;
    this->status.data.tv_avr.ir_repeats      = repeat_count;
}
void ctrlm_voice_t::voice_action_keyword_verification_callback(bool success, rdkx_timestamp_t timestamp) {
    this->session_timing.srvr_rsp_keyword = timestamp;
    if(this->voice_ipc) {
        ctrlm_voice_ipc_event_keyword_verification_t kw_verification;
        kw_verification.common = this->ipc_common_data;
        kw_verification.verified = success;
        this->voice_ipc->keyword_verification(kw_verification);
    }

    //If the keyword was detected and this is a voice assistant, mute the audio
    if(success && is_voice_assistant(this->voice_device)) {
        if(this->audio_timing == CTRLM_VOICE_AUDIO_TIMING_CLOUD || 
          (this->audio_timing == CTRLM_VOICE_AUDIO_TIMING_CONFIDENCE && this->confidence < this->audio_confidence_threshold) ||
          (this->audio_timing == CTRLM_VOICE_AUDIO_TIMING_DUAL_SENSITIVITY && !this->dual_sensitivity_immediate)) {
            this->voice_keyword_verified_action();
        }
    }

    this->keyword_verified = success;
}

void ctrlm_voice_t::voice_keyword_verified_action(void) {
   #ifdef BEEP_ON_KWD_ENABLED
   if(this->audio_ducking_beep_enabled) { // play beep audio before ducking audio
      if(this->audio_ducking_beep_in_progress) {
         LOG_WARN("%s: audio ducking beep already in progress!\n", __FUNCTION__);
         this->obj_sap.close();
         this->audio_ducking_beep_in_progress = false;
         // remove timeout
         if(this->timeout_keyword_beep > 0) {
             g_source_remove(this->timeout_keyword_beep);
             this->timeout_keyword_beep = 0;
         }
      }
      int8_t retry = 1;
      do {
         if(!this->sap_opened) {
            this->sap_opened = this->obj_sap.open(SYSTEM_AUDIO_PLAYER_AUDIO_TYPE_WAV, SYSTEM_AUDIO_PLAYER_SOURCE_TYPE_FILE, SYSTEM_AUDIO_PLAYER_PLAY_MODE_SYSTEM);
            if(!this->sap_opened) {
               LOG_WARN("%s: unable to open system audio player\n", __FUNCTION__);
               retry--;
               continue;
            }
         }

         if(!this->obj_sap.play("file://" BEEP_ON_KWD_FILE)) {
            LOG_WARN("%s: unable to play beep file <%s>\n", __FUNCTION__, BEEP_ON_KWD_FILE);
            if(!this->obj_sap.close()) {
               LOG_WARN("%s: unable to close system audio player\n", __FUNCTION__);
            }
            this->sap_opened = false;
            retry--;
            continue;
         }
         this->audio_ducking_beep_in_progress = true;

         rdkx_timestamp_get(&this->sap_play_timestamp);

         // start a timer in case playback end event is not received
         this->timeout_keyword_beep = g_timeout_add(CTRLM_VOICE_KEYWORD_BEEP_TIMEOUT, ctrlm_voice_keyword_beep_end_timeout, NULL);
         return;
      } while(retry >= 0);
   }
   #endif
   this->audio_state_set(true);
}

#ifdef BEEP_ON_KWD_ENABLED
void ctrlm_voice_t::voice_keyword_beep_completed_normal(void *data, int size) {
   this->voice_keyword_beep_completed_callback(false, false);
}

void ctrlm_voice_t::voice_keyword_beep_completed_error(void *data, int size) {
   this->voice_keyword_beep_completed_callback(false, true);
}

void ctrlm_voice_t::voice_keyword_beep_completed_callback(bool timeout, bool playback_error) {
   if(!this->audio_ducking_beep_in_progress) {
      return;
   }
   if(!timeout) { // remove timeout
      if(this->timeout_keyword_beep > 0) {
         g_source_remove(this->timeout_keyword_beep);
         this->timeout_keyword_beep = 0;
      }
      if(playback_error) {
         LOG_ERROR("%s: playback failure\n", __FUNCTION__);
      }
   } else {
      LOG_ERROR("%s: timeout\n", __FUNCTION__);
   }

   this->audio_ducking_beep_in_progress = false;

   LOG_WARN("%s: duration <%llu> ms dst <%s>\n", __FUNCTION__, rdkx_timestamp_since_ms(this->sap_play_timestamp), ctrlm_voice_state_dst_str(this->state_dst));

   if(this->state_dst >= CTRLM_VOICE_STATE_DST_REQUESTED && this->state_dst <= CTRLM_VOICE_STATE_DST_STREAMING) {
      this->audio_state_set(true);
   }
}
#endif

void ctrlm_voice_t::voice_session_transcription_callback(const char *transcription) {
    if(transcription) {
        this->transcription = transcription;
    } else {
        this->transcription = "";
    }
    LOG_INFO("%s: Voice Session Transcription: \"%s\"\n", __FUNCTION__, this->transcription.c_str());
}

void ctrlm_voice_t::voice_server_return_code_callback(long ret_code) {
    this->server_ret_code = ret_code;
}
// Callback Interface Implementation End

// Helper Functions
const char *ctrlm_voice_format_str(ctrlm_voice_format_t format) {
    switch(format) {
        case CTRLM_VOICE_FORMAT_ADPCM:       return("ADPCM");
        case CTRLM_VOICE_FORMAT_ADPCM_SKY:   return("ADPCM_SKY");
        case CTRLM_VOICE_FORMAT_PCM:         return("PCM");
        case CTRLM_VOICE_FORMAT_OPUS_XVP:    return("OPUS_XVP");
        case CTRLM_VOICE_FORMAT_OPUS:        return("OPUS");
        case CTRLM_VOICE_FORMAT_INVALID:     return("INVALID");
    }
    return("UNKNOWN");
}

const char *ctrlm_voice_state_src_str(ctrlm_voice_state_src_t state) {
    switch(state) {
        case CTRLM_VOICE_STATE_SRC_READY:       return("READY");
        case CTRLM_VOICE_STATE_SRC_STREAMING:   return("STREAMING");
        case CTRLM_VOICE_STATE_SRC_WAITING:     return("WAITING");
        case CTRLM_VOICE_STATE_SRC_INVALID:     return("INVALID");
    }
    return("UNKNOWN");
}

const char *ctrlm_voice_state_dst_str(ctrlm_voice_state_dst_t state) {
    switch(state) {
        case CTRLM_VOICE_STATE_DST_READY:     return("READY");
        case CTRLM_VOICE_STATE_DST_REQUESTED: return("REQUESTED");
        case CTRLM_VOICE_STATE_DST_OPENED:    return("OPENED");
        case CTRLM_VOICE_STATE_DST_STREAMING: return("STREAMING");
        case CTRLM_VOICE_STATE_DST_INVALID:   return("INVALID");
    }
    return("UNKNOWN");
}

const char *ctrlm_voice_device_str(ctrlm_voice_device_t device) {
   switch(device) {
       case CTRLM_VOICE_DEVICE_PTT:        return("PTT");
       case CTRLM_VOICE_DEVICE_FF:         return("FF");
       case CTRLM_VOICE_DEVICE_MICROPHONE: return("MICROPHONE");
       case CTRLM_VOICE_DEVICE_INVALID:    return("INVALID");
   }
   return("UNKNOWN");
}

std::string ctrlm_voice_device_status_str(uint8_t status) {
    std::stringstream ss;
    bool is_first = true;

    if(status == CTRLM_VOICE_DEVICE_STATUS_NONE)          { ss << "NONE"; is_first = false; }
    if(status & CTRLM_VOICE_DEVICE_STATUS_SESSION_ACTIVE) { if(!is_first) { ss << ", "; is_first = false; } ss << "ACTIVE";        }
    if(status & CTRLM_VOICE_DEVICE_STATUS_NOT_SUPPORTED)  { if(!is_first) { ss << ", "; is_first = false; } ss << "NOT SUPPORTED"; }
    if(status & CTRLM_VOICE_DEVICE_STATUS_DEVICE_UPDATE)  { if(!is_first) { ss << ", "; is_first = false; } ss << "DEVICE_UPDATE"; }
    if(status & CTRLM_VOICE_DEVICE_STATUS_DISABLED)       { if(!is_first) { ss << ", "; is_first = false; } ss << "DISABLED";      }
    if(status & CTRLM_VOICE_DEVICE_STATUS_PRIVACY)        { if(!is_first) { ss << ", ";                   } ss << "PRIVACY";       }

    return(ss.str());
}

const char *ctrlm_voice_audio_mode_str(ctrlm_voice_audio_mode_t mode) {
    switch(mode) {
        case CTRLM_VOICE_AUDIO_MODE_OFF:     return("OFF");
        case CTRLM_VOICE_AUDIO_MODE_MUTING:  return("MUTING");
        case CTRLM_VOICE_AUDIO_MODE_DUCKING: return("DUCKING");
    }
    return("UNKNOWN");
}

const char *ctrlm_voice_audio_timing_str(ctrlm_voice_audio_timing_t timing) {
    switch(timing) {
        case CTRLM_VOICE_AUDIO_TIMING_VSR:              return("VSR");
        case CTRLM_VOICE_AUDIO_TIMING_CLOUD:            return("CLOUD");
        case CTRLM_VOICE_AUDIO_TIMING_CONFIDENCE:       return("CONFIDENCE");
        case CTRLM_VOICE_AUDIO_TIMING_DUAL_SENSITIVITY: return("DUAL SENSITIVITY");
    }
    return("UNKNOWN");
}

const char *ctrlm_voice_audio_ducking_type_str(ctrlm_voice_audio_ducking_type_t ducking_type) {
    switch(ducking_type) {
        case CTRLM_VOICE_AUDIO_DUCKING_TYPE_ABSOLUTE: return("ABSOLUTE");
        case CTRLM_VOICE_AUDIO_DUCKING_TYPE_RELATIVE: return("RELATIVE");
    }
    return("UNKNOWN");
}

ctrlm_hal_input_device_t voice_device_to_hal(ctrlm_voice_device_t device) {
    ctrlm_hal_input_device_t ret = CTRLM_HAL_INPUT_INVALID;
    switch(device) {
        case CTRLM_VOICE_DEVICE_PTT: {
            ret = CTRLM_HAL_INPUT_PTT;
            break;
        }
        case CTRLM_VOICE_DEVICE_FF: {
            ret = CTRLM_HAL_INPUT_FF;
            break;
        }
        case CTRLM_VOICE_DEVICE_MICROPHONE: {
            ret = CTRLM_HAL_INPUT_MICS;
            break;
        }
        default: {
            break;
        }
    }
    return(ret);
}

ctrlm_voice_device_t xrsr_to_voice_device(xrsr_src_t device) {
    ctrlm_voice_device_t ret = CTRLM_VOICE_DEVICE_INVALID;
    switch(device) {
        case XRSR_SRC_RCU_PTT: {
            ret = CTRLM_VOICE_DEVICE_PTT;
            break;
        }
        case XRSR_SRC_RCU_FF: {
            ret = CTRLM_VOICE_DEVICE_FF;
            break;
        }
        case XRSR_SRC_MICROPHONE: {
            ret = CTRLM_VOICE_DEVICE_MICROPHONE;
            break;
        }
        default: {
            LOG_ERROR("%s: unrecognized device type %d\n", __FUNCTION__, device);
            break;
        }
    }
    return(ret);
}

xrsr_src_t voice_device_to_xrsr(ctrlm_voice_device_t device) {
    xrsr_src_t ret = XRSR_SRC_INVALID;
    switch(device) {
        case CTRLM_VOICE_DEVICE_PTT: {
            ret = XRSR_SRC_RCU_PTT;
            break;
        }
        case CTRLM_VOICE_DEVICE_FF: {
            ret = XRSR_SRC_RCU_FF;
            break;
        }
        case CTRLM_VOICE_DEVICE_MICROPHONE: {
            ret = XRSR_SRC_MICROPHONE;
            break;
        }
        default: {
            LOG_ERROR("%s: unrecognized device type %d\n", __FUNCTION__, device);
            break;
        }
    }
    return(ret);
}

xraudio_encoding_t voice_format_to_xraudio(ctrlm_voice_format_t format) {
    xraudio_encoding_t ret = XRAUDIO_ENCODING_INVALID;
    switch(format) {
        case CTRLM_VOICE_FORMAT_ADPCM: {
            ret = XRAUDIO_ENCODING_ADPCM_XVP;
            break;
        }
        case CTRLM_VOICE_FORMAT_ADPCM_SKY: {
            ret = XRAUDIO_ENCODING_ADPCM_SKY;
            break;
        }
        case CTRLM_VOICE_FORMAT_OPUS_XVP: {
            ret = XRAUDIO_ENCODING_OPUS_XVP;
            break;
        }
        case CTRLM_VOICE_FORMAT_OPUS: {
            ret = XRAUDIO_ENCODING_OPUS;
            break;
        }
        case CTRLM_VOICE_FORMAT_PCM: {
            ret = XRAUDIO_ENCODING_PCM;
        }
        default: {
            break;
        }
    }
    return(ret);
}

bool ctrlm_voice_t::is_voice_assistant(ctrlm_voice_device_t device) {
    bool voice_assistant = false;
    switch(device) {
        case CTRLM_VOICE_DEVICE_FF: 
        case CTRLM_VOICE_DEVICE_MICROPHONE: {
            voice_assistant = true;
            break;
        }
        case CTRLM_VOICE_DEVICE_PTT: 
        case CTRLM_VOICE_DEVICE_INVALID:
        default: {
            voice_assistant = false;
            break;
        }
    }
    return(voice_assistant);
}

bool ctrlm_voice_t::controller_supports_qos(void) {
   if(this->voice_device == CTRLM_VOICE_DEVICE_FF) {
      return(true);
   }
   return(false);
}

void ctrlm_voice_t::set_audio_mode(ctrlm_voice_audio_settings_t *settings) {
    switch(settings->mode) {
        case CTRLM_VOICE_AUDIO_MODE_OFF:
        case CTRLM_VOICE_AUDIO_MODE_MUTING:
        case CTRLM_VOICE_AUDIO_MODE_DUCKING: {
            this->audio_mode = settings->mode;
            break;
        }
        default: {
            this->audio_mode = (ctrlm_voice_audio_mode_t)JSON_INT_VALUE_VOICE_AUDIO_MODE;
            break;
        }
    }
    switch(settings->timing) {
        case CTRLM_VOICE_AUDIO_TIMING_VSR:
        case CTRLM_VOICE_AUDIO_TIMING_CLOUD:
        case CTRLM_VOICE_AUDIO_TIMING_CONFIDENCE:
        case CTRLM_VOICE_AUDIO_TIMING_DUAL_SENSITIVITY: {
            this->audio_timing = settings->timing;
            break;
        }
        default: {
            this->audio_timing = (ctrlm_voice_audio_timing_t)JSON_INT_VALUE_VOICE_AUDIO_TIMING;
            break;
        }
    }
    switch(settings->ducking_type) {
        case CTRLM_VOICE_AUDIO_DUCKING_TYPE_ABSOLUTE:
        case CTRLM_VOICE_AUDIO_DUCKING_TYPE_RELATIVE: {
            this->audio_ducking_type = settings->ducking_type;
            break;
        }
        default: {
            this->audio_ducking_type = (ctrlm_voice_audio_ducking_type_t)JSON_INT_VALUE_VOICE_AUDIO_DUCKING_TYPE;
            break;
        }
    }

    if(settings->ducking_level >= 0 && settings->ducking_level <= 1) {
        this->audio_ducking_level = settings->ducking_level;
    } else {
        this->audio_ducking_level = JSON_FLOAT_VALUE_VOICE_AUDIO_DUCKING_LEVEL;
    }

    if(settings->confidence_threshold >= 0 && settings->confidence_threshold <= 1) {
        this->audio_confidence_threshold = settings->confidence_threshold;
    } else {
        this->audio_confidence_threshold = JSON_FLOAT_VALUE_VOICE_AUDIO_CONFIDENCE_THRESHOLD;
    }

    this->audio_ducking_beep_enabled = settings->ducking_beep;

    // Print configuration
    std::stringstream ss;

    ss << "Audio Mode < " << ctrlm_voice_audio_mode_str(this->audio_mode) << " >";
    if(this->audio_mode == CTRLM_VOICE_AUDIO_MODE_DUCKING) {
        ss << ", Ducking Type < " << ctrlm_voice_audio_ducking_type_str(this->audio_ducking_type) << " >";
        ss << ", Ducking Level < " << this->audio_ducking_level * 100 << "% >";
        if(this->audio_ducking_beep_enabled) {
           ss << ", Ducking Beep < ENABLED >";
        } else {
           ss << ", Ducking Beep < DISABLED >";
        }
    }
    if(this->audio_mode != CTRLM_VOICE_AUDIO_MODE_OFF) {
        ss << ", Audio Timing < " << ctrlm_voice_audio_timing_str(this->audio_timing) << " >";
        if(this->audio_timing == CTRLM_VOICE_AUDIO_TIMING_CONFIDENCE) {
            ss << ", Confidence Threshold < " << this->audio_confidence_threshold << " >";
        }
    }

    LOG_INFO("%s: %s\n", __FUNCTION__, ss.str().c_str());
    // End print configuration
}

// Helper Functions end

// RF4CE HAL callbacks

void ctrlm_voice_session_response_confirm(bool result, ctrlm_timestamp_t *timestamp, void *user_data) {
   ctrlm_get_voice_obj()->voice_session_rsp_confirm(result, timestamp);
}

// Timeouts
int ctrlm_voice_t::ctrlm_voice_packet_timeout(void *data) {
    ctrlm_get_voice_obj()->voice_session_timeout();
    return(false);
}

int ctrlm_voice_t::ctrlm_voice_controller_session_stats_rxd_timeout(void *data) {
    ctrlm_get_voice_obj()->voice_session_controller_stats_rxd_timeout();
    return(false);
}

int ctrlm_voice_t::ctrlm_voice_controller_command_status_read_timeout(void *data) {
    ctrlm_get_voice_obj()->voice_session_controller_command_status_read_timeout();
    return(false);
}

#ifdef BEEP_ON_KWD_ENABLED
int ctrlm_voice_t::ctrlm_voice_keyword_beep_end_timeout(void *data) {
    ctrlm_get_voice_obj()->voice_keyword_beep_completed_callback(true, false);
    return(false);
}
#endif

// Timeouts end

const char *ctrlm_voice_command_status_str(ctrlm_voice_command_status_t status) {
   switch(status) {
      case VOICE_COMMAND_STATUS_PENDING:    return("PENDING");
      case VOICE_COMMAND_STATUS_TIMEOUT:    return("TIMEOUT");
      case VOICE_COMMAND_STATUS_OFFLINE:    return("OFFLINE");
      case VOICE_COMMAND_STATUS_SUCCESS:    return("SUCCESS");
      case VOICE_COMMAND_STATUS_FAILURE:    return("FAILURE");
      case VOICE_COMMAND_STATUS_NO_CMDS:    return("NO_CMDS");
      case VOICE_COMMAND_STATUS_TV_AVR_CMD: return("TV_AVR_CMD");
      case VOICE_COMMAND_STATUS_MIC_CMD:    return("MIC_CMD");
      case VOICE_COMMAND_STATUS_AUDIO_CMD:  return("AUDIO_CMD");
   }
   return(ctrlm_invalid_return(status));
}

const char *ctrlm_voice_command_status_tv_avr_str(ctrlm_voice_tv_avr_cmd_t cmd) {
    switch(cmd) {
        case CTRLM_VOICE_TV_AVR_CMD_POWER_OFF: return("POWER OFF");
        case CTRLM_VOICE_TV_AVR_CMD_POWER_ON:  return("POWER ON");
        case CTRLM_VOICE_TV_AVR_CMD_VOLUME_UP: return("VOLUME UP");
        case CTRLM_VOICE_TV_AVR_CMD_VOLUME_DOWN: return("VOLUME DOWN");
        case CTRLM_VOICE_TV_AVR_CMD_VOLUME_MUTE: return ("VOLUME MUTE");
    }
    return(ctrlm_invalid_return(cmd));
}

void ctrlm_voice_xrsr_session_capture_start(ctrlm_main_queue_msg_audio_capture_start_t *capture_start) {
   xrsr_audio_container_t xrsr_container;

   if(CTRLM_AUDIO_CONTAINER_WAV == capture_start->container) {
      xrsr_container = XRSR_AUDIO_CONTAINER_WAV;
   } else if(CTRLM_AUDIO_CONTAINER_NONE == capture_start->container) {
      xrsr_container = XRSR_AUDIO_CONTAINER_NONE;
   } else {
      LOG_ERROR("%s: invalid audio container\n", __FUNCTION__);
      return;
   }

   LOG_INFO("%s: container <%s> file path <%s> raw_mic_enable <%d>\n", __FUNCTION__, xrsr_audio_container_str(xrsr_container), capture_start->file_path, capture_start->raw_mic_enable);

   xrsr_session_capture_start(xrsr_container, capture_start->file_path, capture_start->raw_mic_enable);
}

void ctrlm_voice_xrsr_session_capture_stop(void) {
   LOG_INFO("%s: \n", __FUNCTION__);
   xrsr_session_capture_stop();
}

void ctrlm_voice_t::audio_state_set(bool session) {
    switch(this->audio_mode) {
        case CTRLM_VOICE_AUDIO_MODE_MUTING: {
            ctrlm_dsmgr_mute_audio(session);
            break;
        }
        case CTRLM_VOICE_AUDIO_MODE_DUCKING: {
            ctrlm_dsmgr_duck_audio(session, (this->audio_ducking_type == CTRLM_VOICE_AUDIO_DUCKING_TYPE_RELATIVE), this->audio_ducking_level);
            break;
        }
        default: {
            LOG_WARN("%s: invalid audio mode\n", __FUNCTION__);
            break;
        }
    }
}

bool ctrlm_voice_t::voice_session_id_is_current(uuid_t uuid) {
    if(0 != uuid_compare(uuid, this->uuid)) {
       char uuid_str_exp[37] = {'\0'};
       char uuid_str_rxd[37] = {'\0'};
       uuid_unparse_lower(this->uuid, uuid_str_exp);
       uuid_unparse_lower(uuid, uuid_str_rxd);

       LOG_WARN("%s: uuid mismatch exp <%s> rxd <%s> (ignoring)\n", __FUNCTION__, uuid_str_exp, uuid_str_rxd);
       return(false);
    }
    return(true);
}

unsigned long ctrlm_voice_t::voice_session_id_get() {
    return(this->session_id);
}

void ctrlm_voice_t::voice_set_ipc(ctrlm_voice_ipc_t *ipc) {
    if(this->voice_ipc) {
        this->voice_ipc->deregister_ipc();
        delete this->voice_ipc;
        this->voice_ipc = NULL;
    }
    this->voice_ipc = ipc;
}

void ctrlm_voice_t::voice_device_update_in_progress_set(bool in_progress) {
    // This function is used to disable voice when foreground download is active
    sem_wait(&this->device_status_semaphore);
    if(in_progress) {
        this->device_status[CTRLM_VOICE_DEVICE_PTT] |= CTRLM_VOICE_DEVICE_STATUS_DEVICE_UPDATE;
    } else {
        this->device_status[CTRLM_VOICE_DEVICE_PTT] &= ~CTRLM_VOICE_DEVICE_STATUS_DEVICE_UPDATE;
    }
    sem_post(&this->device_status_semaphore);
    LOG_INFO("%s: Voice PTT is <%s>\n", __FUNCTION__, in_progress ? "DISABLED" : "ENABLED");
}

void  ctrlm_voice_t::voice_power_state_change(ctrlm_power_state_t power_state) {

   xrsr_power_mode_t xrsr_power_mode = voice_xrsr_power_map(power_state);

   #ifdef CTRLM_LOCAL_MIC
   if(power_state == CTRLM_POWER_STATE_ON) {
      bool privacy_enabled = this->vsdk_is_privacy_enabled();
      if(privacy_enabled != this->voice_is_privacy_enabled()) {
         privacy_enabled ? this->voice_privacy_enable(false) : this->voice_privacy_disable(false);
      }
   }
   #endif

   if(!xrsr_power_mode_set(xrsr_power_mode)) {
      LOG_ERROR("%s: failed to set xrsr to power state %s\n", __FUNCTION__, ctrlm_power_state_str(power_state));
   }
}

bool ctrlm_voice_t::voice_session_can_request(ctrlm_voice_device_t device) {
   return((this->device_status[device] & CTRLM_VOICE_DEVICE_STATUS_MASK_SESSION_REQ) == 0);
}

void ctrlm_voice_t::voice_session_set_active(ctrlm_voice_device_t device) {
    sem_wait(&this->device_status_semaphore);
    if(this->device_status[device] & CTRLM_VOICE_DEVICE_STATUS_SESSION_ACTIVE) {
        sem_post(&this->device_status_semaphore);
        LOG_WARN("%s: \n", __FUNCTION__);
        return;
    }
    this->device_status[device] &= CTRLM_VOICE_DEVICE_STATUS_SESSION_ACTIVE;
    sem_post(&this->device_status_semaphore);
}

void ctrlm_voice_t::voice_session_set_inactive(ctrlm_voice_device_t device) {
    sem_wait(&this->device_status_semaphore);
    if(!(this->device_status[device] & CTRLM_VOICE_DEVICE_STATUS_SESSION_ACTIVE)) {
        sem_post(&this->device_status_semaphore);
        LOG_WARN("%s: \n", __FUNCTION__);
        return;
    }
    this->device_status[device] |= ~CTRLM_VOICE_DEVICE_STATUS_SESSION_ACTIVE;
    sem_post(&this->device_status_semaphore);
}

bool ctrlm_voice_t::voice_is_privacy_enabled(void) {
   sem_wait(&this->device_status_semaphore);
   bool value = (this->device_status[CTRLM_VOICE_DEVICE_MICROPHONE] & CTRLM_VOICE_DEVICE_STATUS_PRIVACY) ? true : false;
   sem_post(&this->device_status_semaphore);
   return(value);
}

void ctrlm_voice_t::voice_privacy_enable(bool update_vsdk) {
    sem_wait(&this->device_status_semaphore);
    if(this->device_status[CTRLM_VOICE_DEVICE_MICROPHONE] & CTRLM_VOICE_DEVICE_STATUS_PRIVACY) {
        sem_post(&this->device_status_semaphore);
        LOG_WARN("%s: already enabled\n", __FUNCTION__);
        return;
    }

    this->device_status[CTRLM_VOICE_DEVICE_MICROPHONE] |= CTRLM_VOICE_DEVICE_STATUS_PRIVACY;

    #ifdef CTRLM_LOCAL_MIC_DISABLE_VIA_PRIVACY
    if(update_vsdk && this->xrsr_opened && !xrsr_privacy_mode_set(true)) {
        LOG_ERROR("%s: xrsr_privacy_mode_set failed\n", __FUNCTION__);
    }
    #endif

    sem_post(&this->device_status_semaphore);
}

void ctrlm_voice_t::voice_privacy_disable(bool update_vsdk) {
    sem_wait(&this->device_status_semaphore);
    if(!(this->device_status[CTRLM_VOICE_DEVICE_MICROPHONE] & CTRLM_VOICE_DEVICE_STATUS_PRIVACY)) {
        sem_post(&this->device_status_semaphore);
        LOG_WARN("%s: already disabled\n", __FUNCTION__);
        return;
    }

    this->device_status[CTRLM_VOICE_DEVICE_MICROPHONE] &= ~CTRLM_VOICE_DEVICE_STATUS_PRIVACY;

    #ifdef CTRLM_LOCAL_MIC_DISABLE_VIA_PRIVACY
    if(update_vsdk && this->xrsr_opened && !xrsr_privacy_mode_set(false)) {
        LOG_ERROR("%s: xrsr_privacy_mode_set failed\n", __FUNCTION__);
    }
    #endif
    sem_post(&this->device_status_semaphore);
}

void ctrlm_voice_t::voice_device_enable(ctrlm_voice_device_t device, bool db_write, bool *update_routes) {
    sem_wait(&this->device_status_semaphore);
    if(this->device_status[device] & CTRLM_VOICE_DEVICE_STATUS_DISABLED) {
        sem_post(&this->device_status_semaphore);
        LOG_WARN("%s: already enabled\n", __FUNCTION__);
        return;
    }
    this->device_status[device] &= ~CTRLM_VOICE_DEVICE_STATUS_DISABLED;
    if(db_write) {
        ctrlm_db_voice_write_device_status(device, (this->device_status[device] & CTRLM_VOICE_DEVICE_STATUS_MASK_DB));
    }
    if(update_routes != NULL) {
        *update_routes = true;
    }

    sem_post(&this->device_status_semaphore);
}

void ctrlm_voice_t::voice_device_disable(ctrlm_voice_device_t device, bool db_write, bool *update_routes) {
    sem_wait(&this->device_status_semaphore);
    if(!(this->device_status[device] & CTRLM_VOICE_DEVICE_STATUS_DISABLED)) {
        sem_post(&this->device_status_semaphore);
        LOG_WARN("%s: already disabled\n", __FUNCTION__);
        return;
    }
    this->device_status[device] |= CTRLM_VOICE_DEVICE_STATUS_DISABLED;
    if(db_write) {
        ctrlm_db_voice_write_device_status(device, (this->device_status[device] & CTRLM_VOICE_DEVICE_STATUS_MASK_DB));
    }
    if(update_routes != NULL) {
        *update_routes = true;
    }

    sem_post(&this->device_status_semaphore);
}

#ifdef BEEP_ON_KWD_ENABLED
void ctrlm_voice_system_audio_player_event_handler(system_audio_player_event_t event, void *user_data) {
   if(user_data == NULL) {
      return;
   }
   switch(event) {
      case SYSTEM_AUDIO_PLAYER_EVENT_PLAYBACK_STARTED: {
         break;
      }
      case SYSTEM_AUDIO_PLAYER_EVENT_PLAYBACK_FINISHED: {
         // Send event to control manager thread to make this call.
         ctrlm_main_queue_handler_push(CTRLM_HANDLER_VOICE, (ctrlm_msg_handler_voice_t)&ctrlm_voice_t::voice_keyword_beep_completed_normal, NULL, 0, (void *)user_data);
         break;
      }
      case SYSTEM_AUDIO_PLAYER_EVENT_PLAYBACK_PAUSED: {
         break;
      }
      case SYSTEM_AUDIO_PLAYER_EVENT_PLAYBACK_RESUMED: {
         break;
      }
      case SYSTEM_AUDIO_PLAYER_EVENT_NETWORK_ERROR: {
         LOG_ERROR("%s: SYSTEM_AUDIO_PLAYER_EVENT_NETWORK_ERROR\n", __FUNCTION__);
         break;
      }
      case SYSTEM_AUDIO_PLAYER_EVENT_PLAYBACK_ERROR: {
         LOG_ERROR("%s: SYSTEM_AUDIO_PLAYER_EVENT_PLAYBACK_ERROR\n", __FUNCTION__);
         // Send event to control manager thread to make this call.
         ctrlm_main_queue_handler_push(CTRLM_HANDLER_VOICE, (ctrlm_msg_handler_voice_t)&ctrlm_voice_t::voice_keyword_beep_completed_error, NULL, 0, (void *)user_data);
         break;
      }
      case SYSTEM_AUDIO_PLAYER_EVENT_NEED_DATA: {
         LOG_ERROR("%s: SYSTEM_AUDIO_PLAYER_EVENT_NEED_DATA\n", __FUNCTION__);
         break;
      }
      default: {
         LOG_ERROR("%s: INVALID EVENT\n", __FUNCTION__);
         break;
      }
   }
}
#endif

#ifdef ENABLE_DEEP_SLEEP
void ctrlm_voice_t::voice_standby_session_request(void) {
    ctrlm_network_id_t network_id = CTRLM_MAIN_NETWORK_ID_DSP;
    ctrlm_controller_id_t controller_id = CTRLM_MAIN_CONTROLLER_ID_DSP;
    ctrlm_voice_device_t device = CTRLM_VOICE_DEVICE_MICROPHONE;
    ctrlm_voice_format_t format = CTRLM_VOICE_FORMAT_PCM;

    #ifdef CTRLM_LOCAL_MIC_DISABLE_VIA_PRIVACY
    //If the user un-muted the microphones in standby, we must un-mute our components
    if(this->voice_is_privacy_enabled()) {
        this->voice_privacy_disable(true);
    }
    #endif

    voice_session_req(network_id, controller_id, device, format, NULL, "DSP", "1",  "1",  0.0,  false,  NULL,  NULL,  NULL,  false, NULL);

}
#endif

bool ctrlm_voice_t::is_standby_microphone(void) {
   return ((this->voice_device == CTRLM_VOICE_DEVICE_MICROPHONE) && (ctrlm_main_get_power_state() == CTRLM_POWER_STATE_STANDBY_VOICE_SESSION));

}

int ctrlm_voice_t::packet_loss_threshold_get() const {
    return(this->packet_loss_threshold);
}

#ifdef VOICE_BUFFER_STATS
unsigned long voice_packet_interval_get(ctrlm_voice_format_t format, uint32_t opus_samples_per_packet) {
   if(format == CTRLM_VOICE_FORMAT_ADPCM) {
      return(11375); // amount of time between voice packets in microseconds (91*2/16000)*1000000
   } else if (format == CTRLM_VOICE_FORMAT_ADPCM_SKY) {
      return(12000); // amount of time between voice packets in microseconds (96*2/16000)*1000000
   } else if (format == CTRLM_VOICE_FORMAT_PCM) {
      return(20000);
   } else if (format == CTRLM_VOICE_FORMAT_OPUS_XVP || format == CTRLM_VOICE_FORMAT_OPUS) {
      return(opus_samples_per_packet * 1000 / 16); // amount of time between voice packets in microseconds (samples per packet/16000)*1000000
   }
   return(0);
}
#endif

xrsr_power_mode_t voice_xrsr_power_map(ctrlm_power_state_t ctrlm_power_state) {

   xrsr_power_mode_t xrsr_power_mode;

   switch(ctrlm_power_state) {
      case CTRLM_POWER_STATE_STANDBY:
         xrsr_power_mode = XRSR_POWER_MODE_LOW;
        break;
      case CTRLM_POWER_STATE_DEEP_SLEEP:
         xrsr_power_mode = XRSR_POWER_MODE_SLEEP;
         break;
      case CTRLM_POWER_STATE_ON:
         xrsr_power_mode = XRSR_POWER_MODE_FULL;
         break;
      default:
         LOG_WARN("%s defaulting to FULL because %s is unexpected\n", __FUNCTION__, ctrlm_power_state_str(ctrlm_power_state));
         xrsr_power_mode = XRSR_POWER_MODE_FULL;
         break;
   }

   return xrsr_power_mode;
}

void ctrlm_voice_t::voice_rfc_retrieved_handler(const ctrlm_rfc_attr_t& attr) {
    bool enabled = true;

    attr.get_rfc_value(JSON_INT_NAME_VOICE_VREX_REQUEST_TIMEOUT,         this->prefs.timeout_vrex_connect,0);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_VREX_RESPONSE_TIMEOUT,        this->prefs.timeout_vrex_session,0);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_TIMEOUT_PACKET_INITIAL,       this->prefs.timeout_packet_initial);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_TIMEOUT_PACKET_SUBSEQUENT,    this->prefs.timeout_packet_subsequent);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_BITRATE_MINIMUM,              this->prefs.bitrate_minimum);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_TIME_THRESHOLD,               this->prefs.time_threshold);
    attr.get_rfc_value(JSON_BOOL_NAME_VOICE_ENABLE_SAT,                  this->sat_token_required);
    attr.get_rfc_value(JSON_BOOL_NAME_VOICE_SAVE_LAST_UTTERANCE,         this->prefs.utterance_save);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_UTTERANCE_FILE_QTY_MAX,       this->prefs.utterance_file_qty_max, 1);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_UTTERANCE_FILE_SIZE_MAX,      this->prefs.utterance_file_size_max, 4 * 1024);
    attr.get_rfc_value(JSON_STR_NAME_VOICE_UTTERANCE_PATH,               this->prefs.utterance_path);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_MINIMUM_DURATION,             this->prefs.utterance_duration_min);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_FFV_LEADING_SAMPLES,          this->prefs.ffv_leading_samples, 0);
    attr.get_rfc_value(JSON_STR_NAME_VOICE_APP_ID_HTTP,                  this->prefs.app_id_http);
    attr.get_rfc_value(JSON_STR_NAME_VOICE_APP_ID_WS,                    this->prefs.app_id_ws);
    attr.get_rfc_value(JSON_STR_NAME_VOICE_LANGUAGE,                     this->prefs.guide_language);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_KEYWORD_DETECT_SENSITIVITY,   this->prefs.keyword_sensitivity);
    attr.get_rfc_value(JSON_BOOL_NAME_VOICE_VREX_TEST_FLAG,              this->prefs.vrex_test_flag);
    attr.get_rfc_value(JSON_BOOL_NAME_VOICE_FORCE_TOGGLE_FALLBACK,       this->prefs.force_toggle_fallback);
    attr.get_rfc_value(JSON_STR_NAME_VOICE_OPUS_ENCODER_PARAMS,          this->prefs.opus_encoder_params_str);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_PAR_VOICE_EOS_METHOD,         this->prefs.par_voice_eos_method);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_PAR_VOICE_EOS_TIMEOUT,        this->prefs.par_voice_eos_timeout);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_PACKET_LOSS_THRESHOLD,        this->packet_loss_threshold, 0);
    if(attr.get_rfc_value(JSON_INT_NAME_VOICE_AUDIO_MODE, this->audio_mode) |
       attr.get_rfc_value(JSON_INT_NAME_VOICE_AUDIO_TIMING, this->audio_timing) |
       attr.get_rfc_value(JSON_FLOAT_NAME_VOICE_AUDIO_CONFIDENCE_THRESHOLD, this->audio_confidence_threshold, 0.0, 1.0) |
       attr.get_rfc_value(JSON_INT_NAME_VOICE_AUDIO_DUCKING_TYPE, this->audio_ducking_type, CTRLM_VOICE_AUDIO_DUCKING_TYPE_ABSOLUTE, CTRLM_VOICE_AUDIO_DUCKING_TYPE_RELATIVE) |
       attr.get_rfc_value(JSON_FLOAT_NAME_VOICE_AUDIO_DUCKING_LEVEL, this->audio_ducking_level, 0.0, 1.0) |
       attr.get_rfc_value(JSON_BOOL_NAME_VOICE_AUDIO_DUCKING_BEEP, this->audio_ducking_beep_enabled)) {
        ctrlm_voice_audio_settings_t audio_settings = {this->audio_mode, this->audio_timing, this->audio_confidence_threshold, this->audio_ducking_type, this->audio_ducking_level, this->audio_ducking_beep_enabled};
        this->set_audio_mode(&audio_settings);
    }

    #ifdef ENABLE_DEEP_SLEEP
    attr.get_rfc_value(JSON_INT_NAME_VOICE_STANDBY_CONNECT_CHECK_INTERVAL, this->prefs.standby_params.connect_check_interval);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_STANDBY_TIMEOUT_CONNECT,        this->prefs.standby_params.timeout_connect);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_STANDBY_TIMEOUT_INACTIVITY,     this->prefs.standby_params.timeout_inactivity);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_STANDBY_TIMEOUT_SESSION,        this->prefs.standby_params.timeout_session);
    attr.get_rfc_value(JSON_BOOL_NAME_VOICE_STANDBY_IPV4_FALLBACK,         this->prefs.standby_params.ipv4_fallback);
    attr.get_rfc_value(JSON_INT_NAME_VOICE_STANDBY_BACKOFF_DELAY,          this->prefs.standby_params.backoff_delay);
    #endif

    this->voice_params_opus_encoder_validate();

    attr.get_rfc_value(JSON_BOOL_NAME_VOICE_FORCE_VOICE_SETTINGS,        this->prefs.force_voice_settings);
    if(attr.get_rfc_value(JSON_BOOL_NAME_VOICE_FORCE_VOICE_SETTINGS, this->prefs.force_voice_settings) && this->prefs.force_voice_settings) {
        attr.get_rfc_value(JSON_BOOL_NAME_VOICE_ENABLE,                      enabled);
        attr.get_rfc_value(JSON_STR_NAME_VOICE_URL_SRC_PTT,                  this->prefs.server_url_vrex_src_ptt);
        attr.get_rfc_value(JSON_STR_NAME_VOICE_URL_SRC_FF,                   this->prefs.server_url_vrex_src_ff);
        // Check if enabled
        if(!enabled) {
            for(int i = CTRLM_VOICE_DEVICE_PTT; i < CTRLM_VOICE_DEVICE_INVALID; i++) {
                this->voice_device_disable((ctrlm_voice_device_t)i, true, NULL);
            }
        }
        this->voice_sdk_update_routes();
    }
}

void ctrlm_voice_t::vsdk_rfc_retrieved_handler(const ctrlm_rfc_attr_t& attr) {
    json_t *obj_voice = NULL;
    if(attr.get_rfc_json_value(&obj_voice) && obj_voice) {
        LOG_INFO("%s: VSDK values from XCONF, reopening xrsr..\n", __FUNCTION__);
        if(this->vsdk_config) {
            json_object_update_missing(obj_voice, this->vsdk_config);
        }
        // This is temporary until the VSDK supports receiving a config on the fly
        this->voice_sdk_close();
        this->voice_sdk_open(obj_voice);
        this->voice_sdk_update_routes();
        json_decref(obj_voice);
    }
}
