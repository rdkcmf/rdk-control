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
#include "ctrlm_voice_ipc_iarm_all.h"
#include "ctrlm_voice_ipc_iarm_thunder.h"
#include "ctrlm_voice_ipc_iarm_legacy.h"
#include "jansson.h"

ctrlm_voice_ipc_iarm_all_t::ctrlm_voice_ipc_iarm_all_t(ctrlm_voice_t *obj_voice): ctrlm_voice_ipc_t(obj_voice) {
    this->ipc.push_back(new ctrlm_voice_ipc_iarm_thunder_t(obj_voice));
    this->ipc.push_back(new ctrlm_voice_ipc_iarm_legacy_t(obj_voice));
}

ctrlm_voice_ipc_iarm_all_t::~ctrlm_voice_ipc_iarm_all_t() {
    for(const auto &itr : this->ipc) {
        delete itr;
    }
}

bool ctrlm_voice_ipc_iarm_all_t::register_ipc() const {
    bool ret = true;
    for(const auto &itr : this->ipc) {
        if(itr) {
            bool temp_ret = itr->register_ipc();
            if(false == temp_ret) {
                ret = false;
            }
        }
    }
    return(ret);
}

bool ctrlm_voice_ipc_iarm_all_t::session_begin(const ctrlm_voice_ipc_event_session_begin_t &session_begin) {
    bool ret = true;
    for(const auto &itr : this->ipc) {
        if(itr) {
            bool temp_ret = itr->session_begin(session_begin);
            if(false == temp_ret) {
                ret = false;
            }
        }
    }
    return(ret);
}

bool ctrlm_voice_ipc_iarm_all_t::stream_begin(const ctrlm_voice_ipc_event_stream_begin_t &stream_begin) {
    bool ret = true;
    for(const auto &itr : this->ipc) {
        if(itr) {
            bool temp_ret = itr->stream_begin(stream_begin);
            if(false == temp_ret) {
                ret = false;
            }
        }
    }
    return(ret);
}

bool ctrlm_voice_ipc_iarm_all_t::stream_end(const ctrlm_voice_ipc_event_stream_end_t &stream_end) {
    bool ret = true;
    for(const auto &itr : this->ipc) {
        if(itr) {
            bool temp_ret = itr->stream_end(stream_end);
            if(false == temp_ret) {
                ret = false;
            }
        }
    }
    return(ret);
}

bool ctrlm_voice_ipc_iarm_all_t::session_end(const ctrlm_voice_ipc_event_session_end_t &session_end) {
    bool ret = true;
    for(const auto &itr : this->ipc) {
        if(itr) {
            bool temp_ret = itr->session_end(session_end);
            if(false == temp_ret) {
                ret = false;
            }
        }
    }
    return(ret);
}

bool ctrlm_voice_ipc_iarm_all_t::server_message(const char *message, unsigned long size) {
    bool ret = true;
    for(const auto &itr : this->ipc) {
        if(itr) {
            bool temp_ret = itr->server_message(message, size);
            if(false == temp_ret) {
                ret = false;
            }
        }
    }
    return(ret);
}

bool ctrlm_voice_ipc_iarm_all_t::keyword_verification(const ctrlm_voice_ipc_event_keyword_verification_t &keyword_verification) {
    bool ret = true;
    for(const auto &itr : this->ipc) {
        if(itr) {
            bool temp_ret = itr->keyword_verification(keyword_verification);
            if(false == temp_ret) {
                ret = false;
            }
        }
    }
    return(ret);
}

bool ctrlm_voice_ipc_iarm_all_t::session_statistics(const ctrlm_voice_ipc_event_session_statistics_t &session_stats) {
    bool ret = true;
    for(const auto &itr : this->ipc) {
        if(itr) {
            bool temp_ret = itr->session_statistics(session_stats);
            if(false == temp_ret) {
                ret = false;
            }
        }
    }
    return(ret);
}

void ctrlm_voice_ipc_iarm_all_t::deregister_ipc() const {
    for(const auto &itr : this->ipc) {
        if(itr) {
            itr->deregister_ipc();
        }
    }
}
