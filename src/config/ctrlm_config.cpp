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
#include "ctrlm_config.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <errno.h>
#include <string.h>
#include "ctrlm_log.h"
#include "ctrlm_utils.h"

static ctrlm_config_t *_instance = NULL;

static std::string file_to_string(std::string file_path);

ctrlm_config_t *ctrlm_config_t::get_instance() {
    if(_instance == NULL) {
        _instance = new ctrlm_config_t();
    }
    return(_instance);
}

void ctrlm_config_t::destroy_instance() {
    if(_instance != NULL) {
        delete _instance;
        _instance = NULL;
    }
}

ctrlm_config_t::ctrlm_config_t() {
    this->root = NULL;
}

ctrlm_config_t::~ctrlm_config_t() {
    if(this->root) {
        json_decref(this->root);
        this->root = NULL;
    }
}

bool ctrlm_config_t::load_config(std::string file_path) {
    bool ret = false;
    std::string contents = file_to_string(file_path);
    if(this->root) {
        json_decref(this->root);
        this->root = NULL;
    }
    if(!contents.empty()) {
        json_error_t json_error;
        LOG_INFO("%s: Loading Configuration for <%s>:\n", __FUNCTION__, file_path.c_str());
        printf("%s\n", contents.c_str()); // Using printf here to avoid going over buffer limit in XLOGD
        this->root = json_loads(contents.c_str(), JSON_REJECT_DUPLICATES, &json_error);
        if(this->root != NULL) {
            LOG_INFO("%s: config loaded successfully as JSON\n", __FUNCTION__);
            ret = true;
        } else {
            LOG_ERROR("%s: JSON ERROR: Line <%u> Column <%u> Text <%s>\n", __FUNCTION__, json_error.line, json_error.column, json_error.text);
        }
    } else {
        LOG_ERROR("%s: no config file contents\n", __FUNCTION__);
    }
    return(ret);
}

bool ctrlm_config_t::path_exists(std::string path) {
    return(ctrlm_utils_json_from_path(this->root, path, false) != NULL ? true : false);
}

json_t *ctrlm_config_t::json_from_path(std::string path, bool add_ref) {
    return(ctrlm_utils_json_from_path(this->root, path, add_ref));
}

std::string ctrlm_config_t::string_from_path(std::string path) {
    return(ctrlm_utils_json_string_from_path(this->root, path));
}

std::string file_to_string(std::string file_path) {
    std::string ret;
    std::ifstream ifs(file_path.c_str());
    if(ifs) {
        std::ostringstream ss;
        ss << ifs.rdbuf();
        ret = ss.str();
    } else {
        LOG_ERROR("%s: failed to open file <%s>", __FUNCTION__, strerror(errno));
    }
    return(ret);
}