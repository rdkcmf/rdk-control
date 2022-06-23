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
#include "ctrlm_utils.h"
#include "ctrlm_log.h"
#include "ctrlm_config.h"

ctrlm_rfc_attr_t::ctrlm_rfc_attr_t(std::string tr181_string, std::string config_key, bool enabled) {
    this->tr181_string = tr181_string;
    this->config_key   = config_key;
    this->value        = "";
    this->value_json   = NULL;
    this->enabled      = enabled;
}

ctrlm_rfc_attr_t::~ctrlm_rfc_attr_t() {
    if(this->value_json) {
        json_decref(this->value_json);
        this->value_json = NULL;
    }
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

void ctrlm_rfc_attr_t::add_changed_listener(ctrlm_rfc_attr_changed_t listener) {
    this->listeners.push_back(listener);
}

void ctrlm_rfc_attr_t::remove_changed_listener(ctrlm_rfc_attr_changed_t listener) {
    //TODO
}

void ctrlm_rfc_attr_t::set_rfc_value(std::string value) {
    if(value != this->value) {
        // Alright, value changed.. Process and alert the watchers
        this->value = value;
        if(this->value.length() > 0) {
            // Base64 decode
            unsigned char *decoded_buf = NULL;
            size_t decoded_buf_len     = 0;
            decoded_buf = g_base64_decode(this->value.c_str(), &decoded_buf_len);
            if(decoded_buf) {
                // Convert to JSON
                json_t *temp = this->value_json;
                json_error_t json_error;
                this->value_json = json_loads((const char *)decoded_buf, JSON_REJECT_DUPLICATES, &json_error);
                if(this->value_json) {
                    LOG_INFO("%s: successfully got JSON from encoded string, alert the listeners\n", __FUNCTION__);
                    json_decref(temp);
                    temp = NULL;
                } else {
                    LOG_ERROR("%s: failed to parse json\n", __FUNCTION__);
                    this->value_json = temp;
                    return; // return here, we don't want to call listeners on invalid data
                }
                free(decoded_buf);
                decoded_buf = NULL;
            } else {
                LOG_ERROR("%s: failed to decode base64\n", __FUNCTION__);
                return; // return here, we don't want to call listeners on invalid data
            }
        } else {
            LOG_DEBUG("%s: new value is empty, still alerting listeners\n", __FUNCTION__);
        }
        this->notify_changed();
    } else {
        LOG_DEBUG("%s: new value is equal to the old value. do nothing..\n", __FUNCTION__);
    }
}

bool ctrlm_rfc_attr_t::check_config_file(std::string path) const {
    bool ret = false;
    ctrlm_config_t *config = ctrlm_config_t::get_instance();
    if(config) {
        ret = config->path_exists(this->config_key + std::string(JSON_PATH_SEPERATOR) + path);
        if(ret) {
            LOG_DEBUG("%s: %s exists in the config file\n", __FUNCTION__, path.c_str());
        }
    } else {
        LOG_ERROR("%s: config object is NULL\n", __FUNCTION__);
    }
    return(ret);
}

bool ctrlm_rfc_attr_t::get_rfc_json_value(json_t **val) const {
    bool ret = false;
    if(val && this->value_json) {
        json_incref(this->value_json);
        *val = this->value_json;
        ret = true;
    }
    return(ret);
}

bool ctrlm_rfc_attr_t::get_rfc_value(std::string path, bool &val, int index) const {
    bool ret = false;
    if(!this->check_config_file(path)) {
        if(this->value_json) {
            json_t *temp = ctrlm_utils_json_from_path(this->value_json, path, false);
            if(temp) {
                if(index >= 0) { // Handle array index
                    if(!json_is_array(temp)) {
                        LOG_ERROR("%s: %s - not an array\n", __FUNCTION__, path.c_str());
                        temp = 0;
                    } else {
                        json_t *json_element = json_array_get(temp, index);
                        if(json_element == 0) {
                            LOG_ERROR("%s: %s - array index not found.  index <%u> size <%u>\n", __FUNCTION__, path.c_str(), index, json_array_size(temp));
                        }
                        temp = json_element;
                    }
                }

                if(temp && json_is_boolean(temp)) {
                    val = json_is_true(temp);
                    ret = true;
                    LOG_INFO("%s: RFC - <%s, %s>\n", __FUNCTION__, path.c_str(), (val ? "TRUE" : "FALSE"));
                } else {
                    LOG_DEBUG("%s: %s is wrong type in config file\n", __FUNCTION__, path.c_str());
                }
            } else {
                LOG_DEBUG("%s: %s doesn't exist in rfc attribute\n", __FUNCTION__, path.c_str());
            }
        } else {
            LOG_WARN("%s: json object for attr doesn't exist\n", __FUNCTION__);
        }
    } // No need for else, debug log printed in check_config_file
    return(ret);
}

bool ctrlm_rfc_attr_t::get_rfc_value(std::string path, int &val, int min, int max, int index) const {
    bool ret = false;
    if(!this->check_config_file(path)) {
        if(this->value_json) {
            json_t *temp = ctrlm_utils_json_from_path(this->value_json, path, false);
            if(temp) {
                if(index >= 0) { // Handle array index
                    if(!json_is_array(temp)) {
                        LOG_ERROR("%s: %s - not an array\n", __FUNCTION__, path.c_str());
                        temp = 0;
                    } else {
                        json_t *json_element = json_array_get(temp, index);
                        if(json_element == 0) {
                            LOG_ERROR("%s: %s - array index not found.  index <%u> size <%u>\n", __FUNCTION__, path.c_str(), index, json_array_size(temp));
                        }
                        temp = json_element;
                    }
                }

                if(temp && json_is_integer(temp)) {
                    int temp_int = json_integer_value(temp);
                    if(temp_int > max || temp_int < min) {
                        LOG_ERROR("%s: integer out of range (%d)\n", __FUNCTION__, temp_int);
                    } else {
                        val = temp_int;
                        ret = true;
                        LOG_INFO("%s: RFC - <%s, %d>\n", __FUNCTION__, path.c_str(), val);
                    }
                } else {
                    LOG_DEBUG("%s: %s is wrong type in config file\n", __FUNCTION__, path.c_str());
                }
            } else {
                LOG_DEBUG("%s: %s doesn't exist in rfc attribute\n", __FUNCTION__, path.c_str());
            }
        } else {
            LOG_WARN("%s: json object for attr doesn't exist\n", __FUNCTION__);
        }
    } // No need for else, debug log printed in check_config_file
    return(ret);
}

bool ctrlm_rfc_attr_t::get_rfc_value(std::string path, std::string &val) const {
    bool ret = false;
    if(!this->check_config_file(path)) {
        if(this->value_json) {
            json_t *temp = ctrlm_utils_json_from_path(this->value_json, path, false);
            if(temp) {
                if(json_is_string(temp)) {
                    val = json_string_value(temp);
                    ret = true;
                    LOG_INFO("%s: RFC - <%s, %s>\n", __FUNCTION__, path.c_str(), val.c_str());
                } else {
                    LOG_DEBUG("%s: %s is wrong type in config file\n", __FUNCTION__, path.c_str());
                }
            } else {
                LOG_DEBUG("%s: %s doesn't exist in rfc attribute\n", __FUNCTION__, path.c_str());
            }
        } else {
            LOG_WARN("%s: json object for attr doesn't exist\n", __FUNCTION__);
        }
    } // No need for else, debug log printed in check_config_file
    return(ret);
}

bool ctrlm_rfc_attr_t::get_rfc_value(std::string path, double &val, double min, double max) const {
    bool ret = false;
    if(!this->check_config_file(path)) {
        if(this->value_json) {
            json_t *temp = ctrlm_utils_json_from_path(this->value_json, path, false);
            if(temp) {
                if(json_is_real(temp)) {
                    double temp_dbl = json_real_value(temp);
                    if(temp_dbl > max || temp_dbl < min) {
                        LOG_ERROR("%s: float out of range (%f)\n", __FUNCTION__, temp_dbl);
                    } else {
                        val = temp_dbl;
                        ret = true;
                        LOG_INFO("%s: RFC - <%s, %f>\n", __FUNCTION__, path.c_str(), val);
                    }
                } else {
                    LOG_DEBUG("%s: %s is wrong type in config file\n", __FUNCTION__, path.c_str());
                }
            } else {
                LOG_DEBUG("%s: %s doesn't exist in rfc attribute\n", __FUNCTION__, path.c_str());
            }
        } else {
            LOG_WARN("%s: json object for attr doesn't exist\n", __FUNCTION__);
        }
    } // No need for else, debug log printed in check_config_file
    return(ret);
}

bool ctrlm_rfc_attr_t::get_rfc_value(std::string path, std::vector<std::string> &val) const {
    bool ret = false;
    if(!this->check_config_file(path)) {
        if(this->value_json) {
            json_t *temp = ctrlm_utils_json_from_path(this->value_json, path, false);
            if(temp) {
                if(json_is_array(temp)) {
                    size_t index = 0;
                    json_t *value = NULL;

                    // clear old vector contents
                    val.clear();

                    // add new contents
                    json_array_foreach(temp, index, value) {
                        if(json_is_string(value)) {
                            val.push_back(std::string(json_string_value(value)));
                        } else {
                            LOG_ERROR("%s: not a string, skipping..\n", __FUNCTION__);
                        }
                    }
                    ret = true;
                    LOG_INFO("%s: RFC - <%s, %d folders>\n", __FUNCTION__, path.c_str(), val.size());
                }
            } else {
                LOG_DEBUG("%s: %s doesn't exist in rfc attribute\n", __FUNCTION__, path.c_str());
            }
        } else {
            LOG_WARN("%s: json object for attr doesn't exist\n", __FUNCTION__);
        }
    } // No need for else, debug log printed in check_config_file
    return(ret);
}

void ctrlm_rfc_attr_t::notify_changed() {
    for(auto itr : this->listeners) {
        itr(*this);
    }
}
