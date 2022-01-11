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
#include "ctrlm_rfc_attr.h"
#include "ctrlm_rfc.h"

ctrlm_rfc_attr_t::ctrlm_rfc_attr_t(std::string tr181_string, bool enabled) {
    this->tr181_string = tr181_string;
    this->enabled      = enabled;
    this->listener     = NULL;
    this->user_data    = NULL;

    // Add to TR181 component
    ctrlm_rfc_t::get_instance()->add_attribute(this);
}

ctrlm_rfc_attr_t::~ctrlm_rfc_attr_t() {
    // Remove from TR181 component
    ctrlm_rfc_t::get_instance()->remove_attribute(this);
}

std::string ctrlm_rfc_attr_t::get_tr181_string() const {
    return(this->tr181_string);
}

bool ctrlm_rfc_attr_t::is_enabled() const {
    return(this->enabled);
}

void ctrlm_rfc_attr_t::set_enabled(bool enabled) {
    this->enabled = enabled;
}

void ctrlm_rfc_attr_t::set_changed_listener(ctrlm_rfc_attr_changed_t listener, void *user_data) {
    this->listener  = listener;
    this->user_data = user_data;
}

void ctrlm_rfc_attr_t::notify_changed() {
    if(this->listener != NULL) {
        this->listener(this, this->user_data);
    }
}