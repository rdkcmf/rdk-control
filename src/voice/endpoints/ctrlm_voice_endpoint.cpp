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
#include "ctrlm_voice_endpoint.h"

ctrlm_voice_endpoint_t::ctrlm_voice_endpoint_t(ctrlm_voice_t *voice_obj) {
    this->voice_obj     = voice_obj;
    clear_query_strings();
}
ctrlm_voice_endpoint_t::~ctrlm_voice_endpoint_t() {}

bool ctrlm_voice_endpoint_t::add_query_string(const char *key, const char *value) {
    if(this->query_str_qty + 1 >= CTRLM_VOICE_QUERY_STRING_MAX_PAIRS) {
        LOG_ERROR("%s: Too many query strings\n", __FUNCTION__);
        return(false);
    }
    snprintf(this->query_str[this->query_str_qty], sizeof(this->query_str[this->query_str_qty]), "%s=%s", key, value);
    this->query_strs[this->query_str_qty] = this->query_str[this->query_str_qty];
    this->query_str_qty++;
    return(true);
}

void ctrlm_voice_endpoint_t::clear_query_strings() {
    this->query_str_qty = 0;
    memset(&this->query_strs, 0, sizeof (this->query_strs));
    memset(&this->query_str, 0, sizeof (this->query_str));
}

bool ctrlm_voice_endpoint_t::voice_init_set(const char *blob) const {
    LOG_INFO("%s: Endpoint does not support this.\n", __FUNCTION__);
    return(false);
}

bool ctrlm_voice_endpoint_t::voice_message(const char *msg) const {
    LOG_INFO("%s: Endpoint does not support this.\n", __FUNCTION__);
    return(false);
}

void ctrlm_voice_endpoint_t::voice_stb_data_stb_sw_version_set(std::string &sw_version) {}
void ctrlm_voice_endpoint_t::voice_stb_data_stb_name_set(std::string &stb_name) {}
void ctrlm_voice_endpoint_t::voice_stb_data_account_number_set(std::string &account_number) {}
void ctrlm_voice_endpoint_t::voice_stb_data_receiver_id_set(std::string &receiver_id) {}
void ctrlm_voice_endpoint_t::voice_stb_data_device_id_set(std::string &device_id) {}
void ctrlm_voice_endpoint_t::voice_stb_data_partner_id_set(std::string &partner_id) {}
void ctrlm_voice_endpoint_t::voice_stb_data_experience_set(std::string &experience) {}
void ctrlm_voice_endpoint_t::voice_stb_data_guide_language_set(const char *language) {}

sem_t* ctrlm_voice_endpoint_t::voice_session_vsr_semaphore_get() {
    sem_t *sem = NULL;
    if(this->voice_obj) {
        sem = &this->voice_obj->vsr_semaphore;
    }
    return(sem);
}

rdkx_timestamp_t ctrlm_voice_endpoint_t::valid_timestamp_get(rdkx_timestamp_t *t) {
    rdkx_timestamp_t ret;
    if(t) {
        ret = *t;
    } else {
        rdkx_timestamp_get_realtime(&ret);
    }
    return(ret);
}
