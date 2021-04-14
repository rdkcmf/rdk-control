/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
#include "ctrlm_voice_endpoint_ws.h"

// Structures
typedef struct {
    sem_t                       *semaphore;
    uuid_t                       uuid;
    xrsr_src_t                   src;
    xrsr_session_configuration_t *configuration;
    void                         *stream_params;
    rdkx_timestamp_t              timestamp;
} ctrlm_voice_session_begin_cb_ws_t;

// Timestamps and stats are not pointers to avoid corruption
typedef struct {
    uuid_t                uuid;
    xrsr_src_t            src;
    rdkx_timestamp_t      timestamp;
} ctrlm_voice_stream_begin_cb_ws_t;

typedef struct {
    uuid_t               uuid;
    xrsr_stream_stats_t  stats;
    rdkx_timestamp_t     timestamp;
} ctrlm_voice_stream_end_cb_ws_t;

typedef struct {
    uuid_t                uuid;
    xrsr_session_stats_t  stats;
    rdkx_timestamp_t      timestamp;
} ctrlm_voice_session_end_cb_ws_t;
// End Structures

// Static Functions
static const char *controller_name_to_vrex_name(const char *controller);
static const char *controller_name_to_vrex_type(const char *controller);
// End Static Functions

// Function Implementations
ctrlm_voice_endpoint_ws_t::ctrlm_voice_endpoint_ws_t(ctrlm_voice_t *voice_obj) : ctrlm_voice_endpoint_t(voice_obj) {
    this->xrsv_obj_ws = NULL;
}

ctrlm_voice_endpoint_ws_t::~ctrlm_voice_endpoint_ws_t() {
    if(this->xrsv_obj_ws) {
        xrsv_ws_destroy(this->xrsv_obj_ws);
        this->xrsv_obj_ws = NULL;
    }
}

bool ctrlm_voice_endpoint_ws_t::open() {
    if(this->voice_obj == NULL) {
        LOG_ERROR("%s: Voice object NULL\n", __FUNCTION__);
        return(false);
    }
    std::string device_id      = this->voice_obj->voice_stb_data_device_id_get();
    std::string receiver_id    = this->voice_obj->voice_stb_data_receiver_id_get();
    std::string partner_id     = this->voice_obj->voice_stb_data_partner_id_get();
    std::string experience     = this->voice_obj->voice_stb_data_experience_get();
    std::string app_id         = this->voice_obj->voice_stb_data_app_id_ws_get();
    std::string language       = this->voice_obj->voice_stb_data_guide_language_get().c_str();
    std::string account_number = this->voice_obj->voice_stb_data_account_number_get();
    std::string sw_version     = this->voice_obj->voice_stb_data_stb_sw_version_get();

    xrsv_ws_params_t      params_ws = {
       .device_id        = device_id.c_str(),
       .receiver_id      = receiver_id.c_str(),
       .device_mac       = "",
       .account_id       = account_number.c_str(),
       .partner_id       = partner_id.c_str(),
       .experience       = experience.c_str(),
       .stb_device_id    = device_id.c_str(),
       .device_name      = "xr18",
       .device_type      = "xr18",
       .app_id           = app_id.c_str(),
       .software_version = sw_version.c_str(),
       .ip_addr_v4       = "",
       .ip_addr_v6       = "",
       .ssid             = "",
       .pstn             = "",
       .language         = language.c_str(),
       .test_flag        = this->voice_obj->voice_stb_data_test_get(),
       .user_data        = (void *)this
   };

   if((this->xrsv_obj_ws = xrsv_ws_create(&params_ws)) == NULL) {
      LOG_ERROR("%s: Failed to open speech vrex WS\n", __FUNCTION__);
      return(false);
   }

   return(true);
}

bool ctrlm_voice_endpoint_ws_t::get_handlers(xrsr_handlers_t *handlers) {
    if(handlers == NULL) {
        LOG_ERROR("%s: Handler struct NULL\n", __FUNCTION__);
        return(false);
    }

    if(this->voice_obj == NULL || this->xrsv_obj_ws == NULL) {
        LOG_ERROR("%s: Voice obj or Xrsv obj NULL\n", __FUNCTION__);
        return(false);
    }

    xrsv_ws_handlers_t handlers_xrsv;
    memset(handlers, 0, sizeof(xrsr_handlers_t));
    memset(&handlers_xrsv, 0, sizeof(handlers_xrsv));

    // Set up handlers
    handlers_xrsv.session_begin     = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_session_begin;
    handlers_xrsv.session_end       = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_session_end;
    handlers_xrsv.stream_begin      = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_stream_begin;
    handlers_xrsv.stream_kwd        = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_stream_kwd;
    handlers_xrsv.stream_end        = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_stream_end;
    handlers_xrsv.connected         = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_connected;
    handlers_xrsv.disconnected      = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_disconnected;
    handlers_xrsv.sent_init         = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_sent_init;
    handlers_xrsv.listening         = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_listening;
    handlers_xrsv.processing        = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_processing;
    handlers_xrsv.countdown         = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_countdown;
    handlers_xrsv.mic_close         = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_mic_close;
    handlers_xrsv.source_error      = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_source_error;
    handlers_xrsv.tv_mute           = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_tv_mute;
    handlers_xrsv.tv_power          = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_tv_power;
    handlers_xrsv.tv_volume         = &ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_tv_volume;
    handlers_xrsv.notify            = NULL;
    handlers_xrsv.notify_cancel     = NULL;
    handlers_xrsv.phone_call_answer = NULL;
    handlers_xrsv.phone_call_ignore = NULL;
    handlers_xrsv.phone_call_start  = NULL;

    if(!xrsv_ws_handlers(this->xrsv_obj_ws, &handlers_xrsv, handlers)) {
        LOG_ERROR("%s: failed to get handlers http\n", __FUNCTION__);
        return(false);
    }
    return(true);
}

void ctrlm_voice_endpoint_ws_t::voice_stb_data_stb_sw_version_set(std::string &sw_version) {
    if(this->xrsv_obj_ws) {
        xrsv_ws_update_software_version(this->xrsv_obj_ws, sw_version.c_str());
    }
}

void ctrlm_voice_endpoint_ws_t::voice_stb_data_stb_name_set(std::string &stb_name) {
    if(this->xrsv_obj_ws) {
       xrsv_ws_update_device_name(this->xrsv_obj_ws, "xr18" /*this->stb_name.c_str()*/);
    }
}

void ctrlm_voice_endpoint_ws_t::voice_stb_data_account_number_set(std::string &account_number) {
    if(this->xrsv_obj_ws) {
       xrsv_ws_update_account_id(this->xrsv_obj_ws, account_number.c_str());
    }
}

void ctrlm_voice_endpoint_ws_t::voice_stb_data_receiver_id_set(std::string &receiver_id) {
    if(this->xrsv_obj_ws) {
        xrsv_ws_update_receiver_id(this->xrsv_obj_ws, receiver_id.c_str());
    }
}

void ctrlm_voice_endpoint_ws_t::voice_stb_data_device_id_set(std::string &device_id) {
    if(this->xrsv_obj_ws) {
        xrsv_ws_update_device_id(this->xrsv_obj_ws, device_id.c_str());
        xrsv_ws_update_stb_device_id(this->xrsv_obj_ws, device_id.c_str());
    }
}

void ctrlm_voice_endpoint_ws_t::voice_stb_data_partner_id_set(std::string &partner_id) {
    if(this->xrsv_obj_ws) {
        xrsv_ws_update_partner_id(this->xrsv_obj_ws, partner_id.c_str());
    }
}
 
void ctrlm_voice_endpoint_ws_t::voice_stb_data_experience_set(std::string &experience) {
    if(this->xrsv_obj_ws) {
        xrsv_ws_update_experience(this->xrsv_obj_ws, experience.c_str());
    }
}

void ctrlm_voice_endpoint_ws_t::voice_stb_data_guide_language_set(const char *language) {
   if(this->xrsv_obj_ws) {
       xrsv_ws_update_language(this->xrsv_obj_ws, language);
   }
}

void ctrlm_voice_endpoint_ws_t::voice_session_begin_callback_ws(void *data, int size) {
    ctrlm_voice_session_begin_cb_ws_t *dqm = (ctrlm_voice_session_begin_cb_ws_t *)data;
    if(NULL == data) {
        LOG_ERROR("%s: NULL data\n", __FUNCTION__);
        return;
    }
    xrsr_session_configuration_ws_t *ws = &dqm->configuration->ws;
    ctrlm_voice_session_info_t info;
    std::string sat;
    bool keyword_verification = false;

    this->voice_obj->voice_session_info(&info);
    sat = this->voice_obj->voice_stb_data_sat_get();

    if(sat != "") {
        LOG_INFO("%s: SAT Header sent to VREX.\n", __FUNCTION__);
        strncpy(ws->sat_token, sat.c_str(), sizeof(ws->sat_token));
    } else {
        ws->sat_token[0] = '\0';
    }

    xrsv_ws_update_device_name(this->xrsv_obj_ws, controller_name_to_vrex_name(info.controller_name.c_str()));
    xrsv_ws_update_device_type(this->xrsv_obj_ws, controller_name_to_vrex_type(info.controller_name.c_str()));
    xrsv_ws_update_device_software(this->xrsv_obj_ws, info.controller_version_sw.c_str());

    // Handle stream parameters
    xrsv_ws_stream_params_t *stream_params = (xrsv_ws_stream_params_t *)dqm->stream_params;

    if(stream_params != NULL) {
       if(info.has_stream_params) {
          stream_params->keyword_sample_begin               = info.stream_params.pre_keyword_sample_qty / 16; // in ms
          stream_params->keyword_sample_end                 = (info.stream_params.pre_keyword_sample_qty + info.stream_params.keyword_sample_qty) / 16; // in ms
          stream_params->keyword_doa                        = info.stream_params.doa;
          stream_params->keyword_sensitivity                = info.stream_params.standard_search_pt;
          stream_params->keyword_sensitivity_triggered      = info.stream_params.standard_search_pt_triggered;
          stream_params->keyword_sensitivity_high           = info.stream_params.high_search_pt;
          stream_params->keyword_sensitivity_high_support   = info.stream_params.high_search_pt_support;
          stream_params->keyword_sensitivity_high_triggered = info.stream_params.high_search_pt_triggered;
          stream_params->dynamic_gain                       = info.stream_params.dynamic_gain;
          for(int i = 0; i < CTRLM_FAR_FIELD_BEAMS_MAX; i++) {
              if(info.stream_params.beams[i].selected) {
                  //TODO figure out how to determine linear vs nonlinear
                  if(info.stream_params.beams[i].confidence_normalized) {
                      stream_params->linear_confidence       = info.stream_params.beams[i].confidence;
                      stream_params->nonlinear_confidence    = 0;
                  } else {
                      stream_params->nonlinear_confidence = info.stream_params.beams[i].confidence;
                      stream_params->linear_confidence    = 0;
                  }
                  stream_params->signal_noise_ratio   = info.stream_params.beams[i].snr;
              }
          }
          stream_params->push_to_talk         = info.stream_params.push_to_talk;
          if(stream_params->keyword_sample_begin != stream_params->keyword_sample_end) {
              keyword_verification = true;
          }
       } else {
          if(stream_params->keyword_sample_begin > info.ffv_leading_samples) {
             uint32_t delta = stream_params->keyword_sample_begin - info.ffv_leading_samples;
             stream_params->keyword_sample_begin -= delta;
             stream_params->keyword_sample_end   -= delta;
          }

          stream_params->keyword_sample_begin = stream_params->keyword_sample_begin / 16; // in ms
          stream_params->keyword_sample_end   = stream_params->keyword_sample_end   / 16; // in ms
          stream_params->keyword_doa          = 0;
          stream_params->dynamic_gain         = 0;
          stream_params->push_to_talk         = false;
          if(stream_params->keyword_sample_begin != stream_params->keyword_sample_end) {
              keyword_verification = true;
          }
       }
       ws->keyword_begin = stream_params->keyword_sample_begin;
       ws->keyword_duration = stream_params->keyword_sample_end - stream_params->keyword_sample_begin;
       LOG_INFO("%s: session begin - ptt <%s> keyword begin <%u> end <%u> doa <%u> gain <%4.1f> db\n", __FUNCTION__, (stream_params->push_to_talk ? "TRUE" : "FALSE"), stream_params->keyword_sample_begin, stream_params->keyword_sample_end, stream_params->keyword_doa, stream_params->dynamic_gain);
    }
    // End handle stream parameters

    ctrlm_voice_session_begin_cb_t session_begin;
    uuid_copy(session_begin.header.uuid, dqm->uuid);
    session_begin.header.timestamp     = dqm->timestamp;
    session_begin.src                  = dqm->src;
    session_begin.configuration        = dqm->configuration;
    session_begin.endpoint             = this;
    session_begin.keyword_verification = keyword_verification;

    this->voice_obj->voice_session_begin_callback(&session_begin);
    sem_post(dqm->semaphore);
}

void ctrlm_voice_endpoint_ws_t::voice_stream_begin_callback_ws(void *data, int size) {
    ctrlm_voice_stream_begin_cb_ws_t *dqm = (ctrlm_voice_stream_begin_cb_ws_t *)data;
    if(NULL == dqm) {
        LOG_ERROR("%s: NULL data\n", __FUNCTION__);
        return;
    }
    if(false == this->voice_obj->voice_session_id_is_current(dqm->uuid)) {
        return;
    }
    ctrlm_voice_stream_begin_cb_t stream_begin;
    uuid_copy(stream_begin.header.uuid, dqm->uuid);
    stream_begin.header.timestamp = dqm->timestamp;
    stream_begin.src              = dqm->src;
    this->voice_obj->voice_stream_begin_callback(&stream_begin);
}

void ctrlm_voice_endpoint_ws_t::voice_stream_end_callback_ws(void *data, int size) {
    ctrlm_voice_stream_end_cb_ws_t *dqm = (ctrlm_voice_stream_end_cb_ws_t *)data;
    if(NULL == dqm) {
        LOG_ERROR("%s: NULL data\n", __FUNCTION__);
        return;
    }
    if(false == this->voice_obj->voice_session_id_is_current(dqm->uuid)) {
        return;
    }

    ctrlm_voice_stream_end_cb_t stream_end;
    uuid_copy(stream_end.header.uuid, dqm->uuid);
    stream_end.header.timestamp = dqm->timestamp;
    stream_end.stats            = dqm->stats;
    this->voice_obj->voice_stream_end_callback(&stream_end);
}

void ctrlm_voice_endpoint_ws_t::voice_session_end_callback_ws(void *data, int size) {
    ctrlm_voice_session_end_cb_ws_t *dqm = (ctrlm_voice_session_end_cb_ws_t *)data;
    if(NULL == data) {
        LOG_ERROR("%s: NULL data\n", __FUNCTION__);
        return;
    }

    if(false == this->voice_obj->voice_session_id_is_current(dqm->uuid)) {
        return;
    }
    bool success = true;
    xrsr_session_stats_t *stats = &dqm->stats;
    
    if(stats == NULL) {
        LOG_ERROR("%s: stats are NULL\n", __FUNCTION__);
        return;
    }

    // Check if WS was successful
    if(stats->reason != XRSR_SESSION_END_REASON_EOS) {
        success = false;
    }

    ctrlm_voice_session_end_cb_t session_end;
    uuid_copy(session_end.header.uuid, dqm->uuid);
    session_end.header.timestamp = dqm->timestamp;
    session_end.success          = success;
    session_end.stats            = dqm->stats;
    this->voice_obj->voice_session_end_callback(&session_end);
}

void ctrlm_voice_endpoint_ws_t::voice_session_recv_msg_ws(const char *transcription) {
    this->voice_obj->voice_session_transcription_callback(transcription);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_session_begin(const uuid_t uuid, xrsr_src_t src, uint32_t dst_index, xrsr_session_configuration_t *configuration, xrsv_ws_stream_params_t *stream_params, rdkx_timestamp_t *timestamp, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    ctrlm_voice_session_begin_cb_ws_t msg;
    sem_t semaphore;
    memset(&msg, 0, sizeof(msg));
    sem_init(&semaphore, 0, 0);

    if(xrsr_to_voice_device(src) != CTRLM_VOICE_DEVICE_MICROPHONE) {
        // This is a controller, make sure session request / controller info is satisfied
        LOG_DEBUG("%s: Checking if VSR is done\n", __FUNCTION__);
        sem_wait(endpoint->voice_session_vsr_semaphore_get());
    }

    uuid_copy(msg.uuid, uuid);
    msg.semaphore     = &semaphore;
    msg.src           = src;
    msg.configuration = configuration;
    msg.stream_params = (void *)stream_params;
    msg.timestamp     = ctrlm_voice_endpoint_t::valid_timestamp_get(timestamp);
    ctrlm_main_queue_handler_push(CTRLM_HANDLER_VOICE, (ctrlm_msg_handler_voice_t)&ctrlm_voice_endpoint_ws_t::voice_session_begin_callback_ws, &msg, sizeof(msg), (void *)endpoint);
    sem_wait(&semaphore);
    sem_destroy(&semaphore);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_session_end(const uuid_t uuid, xrsr_session_stats_t *stats, rdkx_timestamp_t *timestamp, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    ctrlm_voice_session_end_cb_ws_t msg;
    uuid_copy(msg.uuid, uuid);
    SET_IF_VALID(msg.stats, stats);
    msg.timestamp = ctrlm_voice_endpoint_t::valid_timestamp_get(timestamp);
    ctrlm_main_queue_handler_push(CTRLM_HANDLER_VOICE, (ctrlm_msg_handler_voice_t)&ctrlm_voice_endpoint_ws_t::voice_session_end_callback_ws, &msg, sizeof(msg), (void *)endpoint);
    endpoint->voice_obj->voice_status_set();
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_stream_begin(const uuid_t uuid, xrsr_src_t src, rdkx_timestamp_t *timestamp, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    ctrlm_voice_stream_begin_cb_ws_t msg;
    uuid_copy(msg.uuid, uuid);
    msg.src           = src;
    msg.timestamp     = ctrlm_voice_endpoint_t::valid_timestamp_get(timestamp);
    ctrlm_main_queue_handler_push(CTRLM_HANDLER_VOICE, (ctrlm_msg_handler_voice_t)&ctrlm_voice_endpoint_ws_t::voice_stream_begin_callback_ws, &msg, sizeof(msg), (void *)endpoint);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_stream_kwd(const uuid_t uuid, rdkx_timestamp_t *timestamp, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    ctrlm_voice_cb_header_t data;
    uuid_copy(data.uuid, uuid);
    data.timestamp = ctrlm_voice_endpoint_t::valid_timestamp_get(timestamp);
    endpoint->voice_obj->voice_stream_kwd_callback(&data);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_stream_end(const uuid_t uuid, xrsr_stream_stats_t *stats, rdkx_timestamp_t *timestamp, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    ctrlm_voice_stream_end_cb_ws_t msg;
    uuid_copy(msg.uuid, uuid);
    SET_IF_VALID(msg.stats, stats);
    msg.timestamp     = ctrlm_voice_endpoint_t::valid_timestamp_get(timestamp);
    ctrlm_main_queue_handler_push(CTRLM_HANDLER_VOICE, (ctrlm_msg_handler_voice_t)&ctrlm_voice_endpoint_ws_t::voice_stream_end_callback_ws, &msg, sizeof(msg), (void *)endpoint);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_connected(const uuid_t uuid, rdkx_timestamp_t *timestamp, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    ctrlm_voice_cb_header_t data;
    uuid_copy(data.uuid, uuid);
    data.timestamp = ctrlm_voice_endpoint_t::valid_timestamp_get(timestamp);
    endpoint->voice_obj->voice_server_connected_callback(&data);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_disconnected(const uuid_t uuid, bool retry, rdkx_timestamp_t *timestamp, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    ctrlm_voice_disconnected_cb_t data;
    uuid_copy(data.header.uuid, uuid);
    data.retry            = retry;
    data.header.timestamp = ctrlm_voice_endpoint_t::valid_timestamp_get(timestamp);
    endpoint->voice_obj->voice_server_disconnected_callback(&data);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_sent_init(const uuid_t uuid, rdkx_timestamp_t *timestamp, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    ctrlm_voice_cb_header_t data;
    uuid_copy(data.uuid, uuid);
    data.timestamp = ctrlm_voice_endpoint_t::valid_timestamp_get(timestamp);
    endpoint->voice_obj->voice_server_sent_init_callback(&data);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_listening(void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    rdkx_timestamp_t timestamp;
    rdkx_timestamp_get_realtime(&timestamp);
    endpoint->voice_obj->voice_action_keyword_verification_callback(true, timestamp);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_processing(const char *str_transcription, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    endpoint->voice_session_recv_msg_ws(str_transcription);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_countdown(void *user_data) {
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_mic_close(json_t *obj_reason, void *user_data) {
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_source_error(xrsr_src_t src, void *user_data) {
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_tv_mute(bool mute, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    endpoint->voice_obj->voice_action_tv_mute_callback(mute);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_tv_power(bool power, bool toggle, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    endpoint->voice_obj->voice_action_tv_power_callback(power, toggle);
}

void ctrlm_voice_endpoint_ws_t::ctrlm_voice_handler_ws_tv_volume(bool up, uint32_t repeat_count, void *user_data) {
    ctrlm_voice_endpoint_ws_t *endpoint = (ctrlm_voice_endpoint_ws_t *)user_data;
    endpoint->voice_obj->voice_action_tv_volume_callback(up, repeat_count);
}

// End Function Implementations

// Static Helper Functions
const char *controller_name_to_vrex_type(const char *controller) {
    if(!strncmp(controller, "XR19-", 5)) {
        return("HF");
    } else if(!strncmp(controller, "AX062", 5)) { //xi6v
        return("HF");
    }
    return("HF");
}
const char *controller_name_to_vrex_name(const char *controller) {
    if(!strncmp(controller, "XR19-", 5)) {
        return("xr19");
    } else if(!strncmp(controller, "AX062", 5)) { //xi6v
        return("xr18");
    }
    return("xr18");
}
// End Static Helper Functions
