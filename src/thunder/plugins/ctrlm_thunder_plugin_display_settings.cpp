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
#include "ctrlm_thunder_plugin_display_settings.h"
#include "ctrlm_thunder_log.h"
#include <WPEFramework/core/core.h>
#include <WPEFramework/websocket/websocket.h>
#include <WPEFramework/plugins/plugins.h>
#include <glib.h>

using namespace Thunder;
using namespace DisplaySettings;
using namespace WPEFramework;

static int _on_hotplug_thread(void *data) {
    auto params = (std::pair<ctrlm_thunder_plugin_display_settings_t *, bool> *)data;
    if(params) {
        if(params->first) {
            params->first->on_hotplug(params->second);
        } else {
            THUNDER_LOG_ERROR("%s: Plugin NULL\n", __FUNCTION__);
        }
        delete params;
    } else {
        THUNDER_LOG_ERROR("%s: Params NULL\n", __FUNCTION__);
    }
    return(0);
}

static void _on_hotplug(ctrlm_thunder_plugin_display_settings_t *plugin, JsonObject params) {
    std::string param_str;
    params.ToString(param_str);
    if(params.HasLabel("connectedVideoDisplays")) {
        bool connected = false;
        for(int i = 0; i < params["connectedVideoDisplays"].Array().Length(); i++) {
            if(params["connectedVideoDisplays"].Array()[i].String() == "HDMI0") {
                connected = true;
                break;
            }
        }
        g_idle_add(_on_hotplug_thread, new std::pair<ctrlm_thunder_plugin_display_settings_t *, bool>(plugin, connected));
    } else {
        THUNDER_LOG_ERROR("%s: Malformed hotplug event <%s>\n", __FUNCTION__, param_str.c_str());
    }
}

ctrlm_thunder_plugin_display_settings_t::ctrlm_thunder_plugin_display_settings_t() : ctrlm_thunder_plugin_t("DisplaySettings", "org.rdk.DisplaySettings", 1) {
    sem_init(&this->semaphore, 0, 1);
    this->registered_events = false;
}

ctrlm_thunder_plugin_display_settings_t::~ctrlm_thunder_plugin_display_settings_t() {

}

void ctrlm_thunder_plugin_display_settings_t::get_edid(std::vector<uint8_t> &edid) {
    // Lock sempahore as we are touching EDID cache
    sem_wait(&this->semaphore);
    if(this->edid.size() > 0) {
        edid = this->edid;
    }
    // Unlock semaphore as we are done touching the EDID cache
    sem_post(&this->semaphore);
}

bool ctrlm_thunder_plugin_display_settings_t::_get_edid() {
    bool ret = false;
    JsonObject params, response;

    THUNDER_LOG_INFO("%s: Calling DisplaySettings for EDID data\n", __FUNCTION__);

    // Lock sempahore as we are touching EDID cache
    sem_wait(&this->semaphore);

    if(this->call_plugin("readEDID", (void *)&params, (void *)&response)) {
        if(response.HasLabel("EDID")) {
            std::string edid_str = response["EDID"].String();
            if(!edid_str.empty()) {
                uint8_t *edid_buf  = NULL;
                size_t   edid_size = 0;

                // Base64 decode the string
                edid_buf = g_base64_decode(edid_str.c_str(), &edid_size);
                if(edid_buf && edid_size > 0) {
                    this->edid.clear();
                    for(size_t i = 0; i < edid_size; i++) {
                        this->edid.push_back(edid_buf[i]);
                    }
                    ret = true;
                } else {
                    THUNDER_LOG_ERROR("%s: Failed to decode EDID base64!\n", __FUNCTION__);
                }
            } else {
                THUNDER_LOG_ERROR("%s: EDID string is empty!\n", __FUNCTION__);
            }
        } else {
            std::string response_str;
            response.ToString(response_str);
            THUNDER_LOG_ERROR("%s: DisplaySettings readEDID response malformed: <%s>\n", __FUNCTION__, response_str.c_str());
        }
    } else {
        THUNDER_LOG_ERROR("%s: DisplaySettings readEDID call failed!\n", __FUNCTION__);
    }

    // Unlock semaphore as we are done touching the EDID cache
    sem_post(&this->semaphore);
    return(ret);
}

void ctrlm_thunder_plugin_display_settings_t::_clear_edid() {
    THUNDER_LOG_INFO("%s: Clearing EDID data from cache\n", __FUNCTION__);
    // Lock sempahore as we are touching EDID cache
    sem_wait(&this->semaphore);
    this->edid.clear();
    // Unlock semaphore as we are done touching the EDID cache
    sem_post(&this->semaphore);
}

void ctrlm_thunder_plugin_display_settings_t::on_hotplug(bool connected) {
    THUNDER_LOG_INFO("%s: Hotplug Event <%s>\n", __FUNCTION__, (connected ? "CONNECTED" : "DISCONNECTED"));
    if(connected) {
        THUNDER_LOG_INFO("%s: Acquiring new EDID data\n", __FUNCTION__);
        this->_get_edid();
    } else {
        THUNDER_LOG_INFO("%s: Removing old EDID data\n", __FUNCTION__);
        this->_clear_edid();
    }
}

bool ctrlm_thunder_plugin_display_settings_t::register_events() {
    bool ret = this->registered_events;
    if(ret == false) {
        auto clientObject = (JSONRPC::LinkType<Core::JSON::IElement>*)this->plugin_client;
        if(clientObject) {
            ret = true;
            uint32_t thunderRet = clientObject->Subscribe<JsonObject>(CALL_TIMEOUT, _T("connectedVideoDisplaysUpdated"), &_on_hotplug, this);
            if(thunderRet != Core::ERROR_NONE) {
                THUNDER_LOG_ERROR("%s: Thunder subscribe failed <connectedVideoDisplaysUpdated>\n", __FUNCTION__);
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

void ctrlm_thunder_plugin_display_settings_t::on_initial_activation() {
    // Get initial EDID info
    this->_get_edid();
    
    Thunder::Plugin::ctrlm_thunder_plugin_t::on_initial_activation();
}