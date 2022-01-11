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
#include "ctrlm_rfc.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include "rfcapi.h"
#include "ctrlm_log.h"

#define RFC_CALLER_ID "controlMgr"

static ctrlm_rfc_t *_instance = NULL;

ctrlm_rfc_t::ctrlm_rfc_t() {

}

ctrlm_rfc_t::~ctrlm_rfc_t() {

}

ctrlm_rfc_t *ctrlm_rfc_t::get_instance() {
    if(_instance == NULL) {
        _instance = new ctrlm_rfc_t();
    }
    return(_instance);
}

void ctrlm_rfc_t::destroy_instance() {
    if(_instance != NULL) {
        delete _instance;
        _instance = NULL;
    }
}

void ctrlm_rfc_t::add_attribute(ctrlm_rfc_attr_t *attr) {
    if(std::find(std::begin(this->attributes), std::end(this->attributes), attr) == std::end(this->attributes)) {
        this->attributes.push_back(attr);
    }
}

void ctrlm_rfc_t::remove_attribute(ctrlm_rfc_attr_t *attr) {
    auto itr = std::find(std::begin(this->attributes), std::end(this->attributes), attr);
    if(itr != std::end(this->attributes)) {
        this->attributes.erase(itr);
    }
}

int ctrlm_rfc_t::fetch_attributes(void *data) {
    int ret = 0;
    ctrlm_rfc_t *rfc = (ctrlm_rfc_t *)data;
    if(rfc) {
        LOG_INFO("%s: fetching RFC attributes\n", __FUNCTION__);
        for(const auto &itr : rfc->attributes) {
            if(itr->is_enabled()) {
                LOG_INFO("%s: fetching <%s>\n", __FUNCTION__, itr->get_tr181_string().c_str());
                std::string value = rfc->tr181_call(itr);
                if(!value.empty()) {
                    itr->rfc_value(value);
                } else {
                    LOG_DEBUG("%s: tr181 value for <%s> is empty\n", __FUNCTION__, itr->get_tr181_string().c_str());
                }
            } else {
                LOG_DEBUG("%s: rfc is disabled for <%s>\n", __FUNCTION__, itr->get_tr181_string().c_str());
            }
        }
    } else {
        LOG_ERROR("%s: ctrlm_rfc_t is NULL\n", __FUNCTION__);
    }
    return(ret);
}

std::string ctrlm_rfc_t::tr181_call(ctrlm_rfc_attr_t *attr) {
    std::string ret;
    if(attr) {
        RFC_ParamData_t param;
        WDMP_STATUS rc = getRFCParameter((char *)RFC_CALLER_ID, attr->get_tr181_string().c_str(), &param);
        if(WDMP_SUCCESS != rc) {
            LOG_ERROR("%s: rfc call failed for <%s> <%d, %s>\n", __FUNCTION__, attr->get_tr181_string().c_str(), rc, getRFCErrorString(rc));
        } else {
            ret = std::string(param.value);
        }
    }
    return(ret);
}