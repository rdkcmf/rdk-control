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
#include <vector>
#include <semaphore.h>
#include "ctrlm_voice_obj_generic.h"
#include "ctrlm.h"
#include "ctrlm_tr181.h"
#include "ctrlm_utils.h"
#include "ctrlm_database.h"
#include "ctrlm_network.h"
#include "jansson.h"
#include "json_config.h"
#include "ctrlm_voice_ipc_iarm_all.h"
#include "ctrlm_voice_endpoint_http.h"
#include "ctrlm_voice_endpoint_ws.h"
#include "ctrlm_voice_endpoint_ws_nextgen.h"

#ifdef SUPPORT_VOICE_DEST_ALSA
#include "ctrlm_voice_endpoint_sdt.h"
#endif

// Application Interface Implementation
ctrlm_voice_generic_t::ctrlm_voice_generic_t() : ctrlm_voice_t() {
    LOG_INFO("%s: Constructor\n", __FUNCTION__);
    ctrlm_voice_ipc_t *ipc = new ctrlm_voice_ipc_iarm_all_t(this);
    ipc->register_ipc();
    this->voice_set_ipc(ipc);
    this->obj_http       = NULL;
    this->obj_ws         = NULL;
    this->obj_ws_nextgen = NULL;
    #ifdef SUPPORT_VOICE_DEST_ALSA
    this->obj_sdt        = NULL;
    #endif
}

ctrlm_voice_generic_t::~ctrlm_voice_generic_t() {
    LOG_INFO("%s: Destructor\n", __FUNCTION__);

    if(this->voice_state_src_get() != CTRLM_VOICE_STATE_SRC_READY) { // Need to terminate session before destroying endpoints
        LOG_WARN("%s: Voice session in progress.. Terminating..\n", __FUNCTION__);
        xrsr_session_terminate();
    }

    if(this->obj_ws != NULL) {
        delete this->obj_ws;
        this->obj_ws = NULL;
    }
    if(this->obj_http != NULL) {
        delete this->obj_http;
        this->obj_http = NULL;
    }
    if(this->obj_ws_nextgen != NULL) {
        delete this->obj_ws_nextgen;
        this->obj_ws_nextgen = NULL;
    }
    #ifdef SUPPORT_VOICE_DEST_ALSA
    if(this->obj_sdt != NULL) {
        delete this->obj_sdt;
        this->obj_sdt = NULL;
    }
    #endif
}

void ctrlm_voice_generic_t::voice_sdk_open(json_t *json_obj_vsdk) {
    ctrlm_voice_t::voice_sdk_open(json_obj_vsdk);

    this->obj_ws         = new ctrlm_voice_endpoint_ws_t(this);
    this->obj_http       = new ctrlm_voice_endpoint_http_t(this);
    this->obj_ws_nextgen = new ctrlm_voice_endpoint_ws_nextgen_t(this);
    #ifdef SUPPORT_VOICE_DEST_ALSA
    this->obj_sdt        = new ctrlm_voice_endpoint_sdt_t(this);
    #endif
    if(this->obj_ws->open() == false) {
        LOG_ERROR("%s: Failed to open speech WS\n", __FUNCTION__);
        xrsr_close();
        g_assert(0);
    } else if(this->obj_http->open() == false) {
        LOG_ERROR("%s: Failed to open speech HTTP\n", __FUNCTION__);
        xrsr_close();
        g_assert(0);
    } else if(this->obj_ws_nextgen->open() == false) {
        LOG_ERROR("%s: Failed to open speech WS NextGen\n", __FUNCTION__);
        xrsr_close();
        g_assert(0);
    }
    #ifdef SUPPORT_VOICE_DEST_ALSA
    else if(this->obj_sdt->open() == false) {
        LOG_ERROR("%s: Failed to open speech sdt \n", __FUNCTION__);
        xrsr_close();
        g_assert(0);
    }
    #endif
    this->endpoints.push_back(this->obj_ws);
    this->endpoints.push_back(this->obj_http);
    this->endpoints.push_back(this->obj_ws_nextgen);
    #ifdef SUPPORT_VOICE_DEST_ALSA
    this->endpoints.push_back(this->obj_sdt);
    #endif
}

void ctrlm_voice_generic_t::voice_sdk_update_routes() {
    xrsr_route_t routes[XRSR_SRC_INVALID + 1];
    std::vector<std::string> urls_translated;
    int          i = 0;

    errno_t safec_rc = memset_s(&routes, sizeof(routes), 0, sizeof(routes));
    ERR_CHK(safec_rc);

    // iterate over source to url mapping
    for(int j = 0; j < XRSR_SRC_INVALID; j++) {
        xrsr_src_t            src        = (xrsr_src_t)j;
        ctrlm_voice_device_t  src_device = xrsr_to_voice_device(src);
        std::string          *url        = NULL;

        sem_wait(&this->device_status_semaphore);
        if(this->device_status[src_device] != CTRLM_VOICE_DEVICE_STATUS_DISABLED && this->device_status[src_device] != CTRLM_VOICE_DEVICE_STATUS_NOT_SUPPORTED) {
            switch(src_device) {
                case CTRLM_VOICE_DEVICE_PTT: {
                    url = &this->prefs.server_url_vrex_src_ptt;
                    break;
                }
                case CTRLM_VOICE_DEVICE_FF:
                case CTRLM_VOICE_DEVICE_MICROPHONE: {
                    url = &this->prefs.server_url_vrex_src_ff;
                    break;
                }
                default: {
                    break;
                }
            }
        }
        sem_post(&this->device_status_semaphore);

        if(url == NULL) {
            continue;
        }

        xrsr_stream_from_t  stream_from   = XRSR_STREAM_FROM_BEGINNING;
        int32_t             stream_offset = 0;
        xrsr_stream_until_t stream_until  = XRSR_STREAM_UNTIL_END_OF_STREAM;

        if(src == XRSR_SRC_MICROPHONE) {
           stream_from   = XRSR_STREAM_FROM_KEYWORD_BEGIN;
           stream_offset = - this->prefs.ffv_leading_samples;
           stream_until  = XRSR_STREAM_UNTIL_END_OF_SPEECH;
        }

        if(url->rfind("http", 0) == 0) {
            xrsr_handlers_t handlers_xrsr = {0};

            if(!this->obj_http->get_handlers(&handlers_xrsr)) {
                LOG_ERROR("%s: failed to get handlers http\n", __FUNCTION__);
            } else {
                if(std::string::npos == url->find("speech?")) {
                    url->append("speech?");
                }


                routes[i].src                     = src;
                routes[i].dst_qty                 = 1;
                routes[i].dsts[0].url             = url->c_str();
                routes[i].dsts[0].handlers        = handlers_xrsr;
                #ifdef AUDIO_DECODE
                routes[i].dsts[0].format          = XRSR_AUDIO_FORMAT_PCM;
                #else
                routes[i].dsts[0].format          = XRSR_AUDIO_FORMAT_NATIVE;
                #endif
                routes[i].dsts[0].stream_time_min = this->prefs.utterance_duration_min;
                routes[i].dsts[0].stream_from     = stream_from;
                routes[i].dsts[0].stream_offset   = stream_offset;
                routes[i].dsts[0].stream_until    = stream_until;
                #ifdef ENABLE_DEEP_SLEEP
                routes[i].dsts[0].params          = ((src == XRSR_SRC_MICROPHONE) && (ctrlm_main_get_power_state() == CTRLM_POWER_STATE_STANDBY)) ? &this->prefs.standby_params : NULL;
                #else
                routes[i].dsts[0].params           = NULL;
                #endif
                i++;
            }
        } else if(url->rfind("ws", 0) == 0) {
            xrsr_handlers_t    handlers_xrsr = {0};

            if(!this->obj_ws->get_handlers(&handlers_xrsr)) {
                LOG_ERROR("%s: failed to get handlers ws\n", __FUNCTION__);
            } else {

                routes[i].src                     = src;
                routes[i].dst_qty                 = 1;
                routes[i].dsts[0].url             = url->c_str();
                routes[i].dsts[0].handlers        = handlers_xrsr;
                routes[i].dsts[0].format          = XRSR_AUDIO_FORMAT_PCM;
                routes[i].dsts[0].stream_time_min = this->prefs.utterance_duration_min;
                routes[i].dsts[0].stream_from     = stream_from;
                routes[i].dsts[0].stream_offset   = stream_offset;
                routes[i].dsts[0].stream_until    = stream_until;
                #ifdef ENABLE_DEEP_SLEEP
                routes[i].dsts[0].params          = ((src == XRSR_SRC_MICROPHONE) && (ctrlm_main_get_power_state() == CTRLM_POWER_STATE_STANDBY)) ? &this->prefs.standby_params : NULL;
                #else
                routes[i].dsts[0].params          = NULL;
                #endif
                i++;
            }
        } else if(url->rfind("vrng", 0) == 0) {
            xrsr_handlers_t    handlers_xrsr = {0};
            int                translated_index = urls_translated.size();

            urls_translated.push_back("ws" + url->substr(4));

            if(!this->obj_ws_nextgen->get_handlers(&handlers_xrsr)) {
                LOG_ERROR("%s: failed to get handlers ws\n", __FUNCTION__);
            } else {
                routes[i].src                     = src;
                routes[i].dst_qty                 = 1;
                routes[i].dsts[0].url             = urls_translated[translated_index].c_str();
                routes[i].dsts[0].handlers        = handlers_xrsr;
                routes[i].dsts[0].format          = XRSR_AUDIO_FORMAT_PCM;
                routes[i].dsts[0].stream_time_min = this->prefs.utterance_duration_min;
                routes[i].dsts[0].stream_from     = stream_from;
                routes[i].dsts[0].stream_offset   = stream_offset;
                routes[i].dsts[0].stream_until    = stream_until;
                #ifdef ENABLE_DEEP_SLEEP
                routes[i].dsts[0].params          = ((src == XRSR_SRC_MICROPHONE) && (ctrlm_main_get_power_state() == CTRLM_POWER_STATE_STANDBY)) ? &this->prefs.standby_params : NULL;
                #else
                routes[i].dsts[0].params         = NULL;
                #endif
                i++;
                LOG_INFO("%s: url translation from %s to %s\n", __FUNCTION__, url->c_str(), urls_translated[translated_index].c_str());
            }
        }
        #ifdef SUPPORT_VOICE_DEST_ALSA
          else if(url->rfind("sdt", 0) == 0) {
            xrsr_handlers_t    handlers_xrsr;
            memset(&handlers_xrsr, 0, sizeof(handlers_xrsr));

            if(!this->obj_sdt->get_handlers(&handlers_xrsr)) {
                LOG_ERROR("%s: failed to get handlers ws\n", __FUNCTION__);
            } else {

                routes[i].src                     = src;
                routes[i].dst_qty                 = 1;
                routes[i].dsts[0].url             = url->c_str();
                routes[i].dsts[0].handlers        = handlers_xrsr;
                routes[i].dsts[0].format          = XRSR_AUDIO_FORMAT_PCM;
                routes[i].dsts[0].stream_time_min = this->prefs.utterance_duration_min;
                routes[i].dsts[0].stream_from     = stream_from;
                routes[i].dsts[0].stream_offset   = stream_offset;
                routes[i].dsts[0].stream_until    = stream_until;
                routes[i].dsts[0].params          = NULL;
                i++;
            }
        }
        #endif  
        else {
            LOG_ERROR("%s: unsupported url <%s>", __FUNCTION__, url->c_str());
        }
    }
    routes[i].src     = XRSR_SRC_INVALID;
    routes[i].dst_qty = 0;

    if(i == 0) {
        LOG_INFO("%s: no routes available\n", __FUNCTION__);
    } else {
        for(int j = 0; j < i; j++) {
            LOG_INFO("%s: src <%s>\n", __FUNCTION__, xrsr_src_str(routes[j].src));
            for(uint32_t k = 0; k < routes[j].dst_qty; k++) {
               LOG_INFO("%s: dst <%u> url <%s>\n", __FUNCTION__, k, routes[j].dsts[k].url);
            }
        }
    }

    if(!xrsr_route(routes)) {
        LOG_ERROR("%s: failed to set routes\n", __FUNCTION__);
    }
}

void ctrlm_voice_generic_t::query_strings_updated() {
    LOG_INFO("%s\n", __FUNCTION__);
    // update query strings for HTTP only
    if(this->obj_http) {
        this->obj_http->clear_query_strings();
        for(unsigned int i = 0; i < this->query_strs_ptt.size(); i++) {
            this->obj_http->add_query_string(this->query_strs_ptt[i].first.c_str(), this->query_strs_ptt[i].second.c_str());
        }
    }
}
