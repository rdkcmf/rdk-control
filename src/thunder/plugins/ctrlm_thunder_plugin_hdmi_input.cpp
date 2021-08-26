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
#include "ctrlm_thunder_plugin_hdmi_input.h"
#include "ctrlm_thunder_log.h"
#include <WPEFramework/core/core.h>
#include <WPEFramework/websocket/websocket.h>
#include <WPEFramework/plugins/plugins.h>
#include <glib.h>

using namespace Thunder;
using namespace HDMIInput;
using namespace WPEFramework;

static int _on_hotplug_thread(void *data) {
    auto params = (std::pair<ctrlm_thunder_plugin_hdmi_input_t *, JsonObject> *)data;
    if(params) {
        if(params->first) {
            params->first->check_device_list((void *)&params->second);
        } else {
            THUNDER_LOG_ERROR("%s: Plugin NULL\n", __FUNCTION__);
        }
        delete params;
    } else {
        THUNDER_LOG_ERROR("%s: Params NULL\n", __FUNCTION__);
    }
    return(0);
}

static void _on_hotplug(ctrlm_thunder_plugin_hdmi_input_t *plugin, JsonObject params) {
    g_idle_add(_on_hotplug_thread, new std::pair<ctrlm_thunder_plugin_hdmi_input_t *, JsonObject>(plugin, params));
}

static int _on_device_status_change_thread(void *data) {
    auto params = (std::pair<ctrlm_thunder_plugin_hdmi_input_t *, JsonObject> *)data;
    if(params) {
        if(params->first) {
            if(params->second.HasLabel("id") && params->second.HasLabel("status")) {
                params->first->on_device_status_change(params->second["id"].Number(), params->second["status"].String() == "started");
            } else {
                std::string response_str;
                params->second.ToString(response_str);
                THUNDER_LOG_ERROR("%s: HDMIInput device status changemalformed: <%s>\n", __FUNCTION__, response_str.c_str());
            }
        } else {
            THUNDER_LOG_ERROR("%s: Plugin NULL\n", __FUNCTION__);
        }
        delete params;
    } else {
        THUNDER_LOG_ERROR("%s: Params NULL\n", __FUNCTION__);
    }
    return(0);
}

static void _on_device_status_change(ctrlm_thunder_plugin_hdmi_input_t *plugin, JsonObject params) {
    g_idle_add(_on_device_status_change_thread, new std::pair<ctrlm_thunder_plugin_hdmi_input_t *, JsonObject>(plugin, params));
}

ctrlm_thunder_plugin_hdmi_input_t::ctrlm_thunder_plugin_hdmi_input_t() : ctrlm_thunder_plugin_t("HdmiInput", "org.rdk.HdmiInput", 2) {
    sem_init(&this->semaphore, 0, 1);
    this->registered_events = false;
}

ctrlm_thunder_plugin_hdmi_input_t::~ctrlm_thunder_plugin_hdmi_input_t() {

}

void ctrlm_thunder_plugin_hdmi_input_t::get_infoframes(std::map<int, std::vector<uint8_t> > &infoframes) {
    // Lock sempahore as we are touching infoframe cache
    sem_wait(&this->semaphore);
    infoframes = this->infoframes;
    // Unlock semaphore as we are done touching the infoframe cache
    sem_post(&this->semaphore);
}

bool ctrlm_thunder_plugin_hdmi_input_t::_get_infoframe(int port) {
    bool ret = false;
    JsonObject params, response;

    THUNDER_LOG_INFO("%s: Calling HDMIInput for infoframe data for port %d\n", __FUNCTION__, port);

    params["portId"] = port;
    // Lock sempahore as we are touching infoframe cache
    sem_wait(&this->semaphore);
    this->infoframes[port].clear();

    if(this->call_plugin("getRawHDMISPD", (void *)&params, (void *)&response)) {
        if(response.HasLabel("HDMISPD")) {
            std::string infoframe_str = response["HDMISPD"].String();
            if(!infoframe_str.empty()) {
                uint8_t *infoframe_buf  = NULL;
                size_t   infoframe_size = 0;

                // Base64 decode the string
                infoframe_buf = g_base64_decode(infoframe_str.c_str(), &infoframe_size);
                if(infoframe_buf && infoframe_size > 0) {
                    for(size_t i = 0; i < infoframe_size; i++) {
                        this->infoframes[port].push_back(infoframe_buf[i]); // This checksum byte is not part of the HDMI spec, not sure why dsMgr/TvServer adds it within the infoframe
                    }
#define REMOVE_RSD_BYTE
#ifdef REMOVE_RSD_BYTE
                    this->infoframes[port].erase(this->infoframes[port].begin() + 3);
#endif
                    if(this->infoframes[port].size() < 29) {
                        THUNDER_LOG_INFO("%s: Padding infoframe to complete 29 bytes\n", __FUNCTION__);
                        for(size_t i = this->infoframes[port].size(); i < 29; i++) {
                            this->infoframes[port].push_back(0x00);
                        }
                    } else if(this->infoframes[port].size() > 29) {
                        THUNDER_LOG_INFO("%s: Removing padding from infoframe to 29 bytes\n", __FUNCTION__);
                        for(size_t i = this->infoframes[port].size(); i > 29; i--) {
                            this->infoframes[port].pop_back();
                        }
                    }
                    ret = true;
                } else {
                    THUNDER_LOG_ERROR("%s: Failed to decode Infoframe base64!\n", __FUNCTION__);
                }
            } else {
                THUNDER_LOG_ERROR("%s: Infoframe string is empty!\n", __FUNCTION__);
            }
        } else {
            std::string response_str;
            response.ToString(response_str);
            THUNDER_LOG_ERROR("%s: HDMIInput readINFOFRAME response malformed: <%s>\n", __FUNCTION__, response_str.c_str());
        }
    } else {
        THUNDER_LOG_ERROR("%s: HDMIInput readINFOFRAME call failed!\n", __FUNCTION__);
    }

    // Unlock semaphore as we are done touching the infoframe cache
    sem_post(&this->semaphore);
    return(ret);
}

void ctrlm_thunder_plugin_hdmi_input_t::_clear_infoframe(int port) {
    THUNDER_LOG_INFO("%s: Clearing infoframe data from cache for port %d\n", __FUNCTION__, port);
    // Lock sempahore as we are touching infoframe cache
    sem_wait(&this->semaphore);
    this->infoframes[port].clear();
    // Unlock semaphore as we are done touching the infoframe cache
    sem_post(&this->semaphore);
}

void ctrlm_thunder_plugin_hdmi_input_t::on_device_status_change(int port, bool active) {
    THUNDER_LOG_INFO("%s: Device status changed <%s>\n", __FUNCTION__, (active ? "STARTED" : "STOPPED"));
    if(active) {
        THUNDER_LOG_INFO("%s: Acquiring new infoframe data\n", __FUNCTION__);
        this->_get_infoframe(port);
    } else {
        THUNDER_LOG_INFO("%s: Purposely doing nothing when status is stopped\n", __FUNCTION__);
    }
}

bool ctrlm_thunder_plugin_hdmi_input_t::register_events() {
    bool ret = this->registered_events;
    if(ret == false) {
        auto clientObject = (JSONRPC::LinkType<Core::JSON::IElement>*)this->plugin_client;
        if(clientObject) {
            ret = true;
            uint32_t thunderRet = clientObject->Subscribe<JsonObject>(CALL_TIMEOUT, _T("onDevicesChanged"), &_on_hotplug, this);
            if(thunderRet != Core::ERROR_NONE) {
                THUNDER_LOG_ERROR("%s: Thunder subscribe failed <onDevicesChanged>\n", __FUNCTION__);
                ret = false;
            }
            thunderRet = clientObject->Subscribe<JsonObject>(CALL_TIMEOUT, _T("onInputStatusChanged"), &_on_device_status_change, this);
            if(thunderRet != Core::ERROR_NONE) {
                THUNDER_LOG_ERROR("%s: Thunder subscribe failed <onInputStatusChanged>\n", __FUNCTION__);
                ret = false;
            }
            if(ret) {
                this->registered_events = true;
            }
        }
    }
    Thunder::Plugin::ctrlm_thunder_plugin_t::register_events();
    return(ret);
}

void ctrlm_thunder_plugin_hdmi_input_t::on_initial_activation() {
    // Get initial INFOFRAME info
    JsonObject params, response;
    if(this->call_plugin("getHDMIInputDevices", (void *)&params, (void *)&response)) {
        this->check_device_list((void *)&response);
    }
    
    Thunder::Plugin::ctrlm_thunder_plugin_t::on_initial_activation();
}

void ctrlm_thunder_plugin_hdmi_input_t::check_device_list(void *params) {
    JsonObject *root = (JsonObject *)params;
    if(root) {
        std::string response_st;
        root->ToString(response_st);
        THUNDER_LOG_INFO("%s: %s\n", __FUNCTION__, response_st.c_str());
        if(root->HasLabel("devices")) {
            JsonArray devices = (*root)["devices"].Array();
            for(int i = 0; i < devices.Length(); i++) {
                JsonObject device = devices[i].Object();
                if(device.HasLabel("id") && device.HasLabel("connected")) {
                    if(device["connected"].String() == "true") {
                        this->_get_infoframe(device["id"].Number());
                    } else {
                        this->_clear_infoframe(device["id"].Number());
                    }
                } else {
                    std::string response_str;
                    device.ToString(response_str);
                    THUNDER_LOG_WARN("%s: Malformed device object <%s>\n", __FUNCTION__, response_str.c_str());
                }
            }
        } else {
            std::string response_str;
            root->ToString(response_str);
            THUNDER_LOG_WARN("%s: Missing device list! <%s>\n", __FUNCTION__, response_str.c_str());
        }
    } else {
        THUNDER_LOG_WARN("%s: JsonObject is NULL\n", __FUNCTION__);
    }
}