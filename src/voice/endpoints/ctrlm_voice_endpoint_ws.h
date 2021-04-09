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
#ifndef __CTRLM_VOICE_ENDPOINT_WS_H__
#define __CTRLM_VOICE_ENDPOINT_WS_H__

#include "ctrlm_voice_endpoint.h"
#include "xrsv_ws.h"

class ctrlm_voice_endpoint_ws_t : public ctrlm_voice_endpoint_t {
public:
    ctrlm_voice_endpoint_ws_t(ctrlm_voice_t *voice_obj);
    virtual ~ctrlm_voice_endpoint_ws_t();
    bool open();
    bool get_handlers(xrsr_handlers_t *handlers);

public:
    void voice_stb_data_stb_sw_version_set(std::string &sw_version);
    void voice_stb_data_stb_name_set(std::string &stb_name);
    void voice_stb_data_account_number_set(std::string &account_number);
    void voice_stb_data_receiver_id_set(std::string &receiver_id);
    void voice_stb_data_device_id_set(std::string &device_id);
    void voice_stb_data_partner_id_set(std::string &partner_id);
    void voice_stb_data_experience_set(std::string &experience);
    void voice_stb_data_guide_language_set(const char *language);

protected:
    void voice_session_begin_callback_ws(void *data, int size);
    void voice_stream_begin_callback_ws(void *data, int size);
    void voice_stream_end_callback_ws(void *data, int size);
    void voice_session_end_callback_ws(void *data, int size);
    void voice_session_recv_msg_ws(const char *transcription);

protected:
    static void ctrlm_voice_handler_ws_session_begin(const uuid_t uuid, xrsr_src_t src, uint32_t dst_index, xrsr_session_configuration_t *configuration, xrsv_ws_stream_params_t *stream_params, rdkx_timestamp_t *timestamp, void *user_data);
    static void ctrlm_voice_handler_ws_session_end(const uuid_t uuid, xrsr_session_stats_t *stats, rdkx_timestamp_t *timestamp, void *user_data);
    static void ctrlm_voice_handler_ws_stream_begin(const uuid_t uuid, xrsr_src_t src, rdkx_timestamp_t *timestamp, void *user_data);
    static void ctrlm_voice_handler_ws_stream_kwd(const uuid_t uuid, rdkx_timestamp_t *timestamp, void *user_data);
    static void ctrlm_voice_handler_ws_stream_end(const uuid_t uuid, xrsr_stream_stats_t *stats, rdkx_timestamp_t *timestamp, void *user_data);
    static void ctrlm_voice_handler_ws_connected(const uuid_t uuid, rdkx_timestamp_t *timestamp, void *user_data);
    static void ctrlm_voice_handler_ws_disconnected(const uuid_t uuid, bool retry, rdkx_timestamp_t *timestamp, void *user_data);
    static void ctrlm_voice_handler_ws_sent_init(const uuid_t uuid, rdkx_timestamp_t *timestamp, void *user_data);
    static void ctrlm_voice_handler_ws_listening(void *user_data);
    static void ctrlm_voice_handler_ws_processing(const char *str_transcription, void *user_data);
    static void ctrlm_voice_handler_ws_countdown(void *user_data);
    static void ctrlm_voice_handler_ws_mic_close(json_t *obj_reason, void *user_data);
    static void ctrlm_voice_handler_ws_source_error(xrsr_src_t src, void *user_data);
    static void ctrlm_voice_handler_ws_tv_mute(bool mute, void *user_data);
    static void ctrlm_voice_handler_ws_tv_power(bool power, bool toggle, void *user_data);
    static void ctrlm_voice_handler_ws_tv_volume(bool up, uint32_t repeat_count, void *user_data);

protected:
    void       *xrsv_obj_ws;
};

#endif
