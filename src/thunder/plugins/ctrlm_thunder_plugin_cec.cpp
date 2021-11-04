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
#include "ctrlm_thunder_plugin_cec.h"
#include "ctrlm_thunder_log.h"
#include "ctrlm_thunder_helpers.h"
#include <WPEFramework/core/core.h>
#include <WPEFramework/websocket/websocket.h>
#include <WPEFramework/plugins/plugins.h>
#include <glib.h>
#include <sstream>

using namespace Thunder;
using namespace CEC;
using namespace WPEFramework;

static int _on_device_changed_thread(void *data) {
    ctrlm_thunder_plugin_cec_t *plugin = (ctrlm_thunder_plugin_cec_t *)data;
    if(plugin) {
        plugin->device_add_timer = 0;
        plugin->on_device_updated();
    } else {
        THUNDER_LOG_ERROR("%s: Plugin NULL\n", __FUNCTION__);
    }
    return(0);
}

static void _on_device_added(ctrlm_thunder_plugin_cec_t *plugin, JsonObject params) {
    if(plugin) {
        if(plugin->device_add_timer > 0) {
            g_source_remove(plugin->device_add_timer);
            plugin->device_add_timer = 0;
        }
        plugin->device_add_timer = g_timeout_add_seconds(3, &_on_device_changed_thread, (void *)plugin);
    } else {
        THUNDER_LOG_ERROR("%s: Plugin NULL\n", __FUNCTION__);
    }
}

static void _on_device_removed(ctrlm_thunder_plugin_cec_t *plugin, JsonObject params) {
    if(plugin) {
        if(plugin->device_add_timer > 0) {
            g_source_remove(plugin->device_add_timer);
            plugin->device_add_timer = 0;
        }
        g_idle_add(&_on_device_changed_thread, (void *)plugin);
    } else {
        THUNDER_LOG_ERROR("%s: Plugin NULL\n", __FUNCTION__);
    }
}

static void _on_device_info_changed(ctrlm_thunder_plugin_cec_t *plugin, JsonObject params) {
    if(plugin) {
        if(plugin->device_add_timer > 0) {
            g_source_remove(plugin->device_add_timer);
            plugin->device_add_timer = 0;
        }
        plugin->device_add_timer = g_timeout_add_seconds(3, &_on_device_changed_thread, (void *)plugin);
    } else {
        THUNDER_LOG_ERROR("%s: Plugin NULL\n", __FUNCTION__);
    }
}

ctrlm_thunder_plugin_cec_t::ctrlm_thunder_plugin_cec_t() : ctrlm_thunder_plugin_t("HdmiCec", "org.rdk.HdmiCec", 1) {
    sem_init(&this->semaphore, 0, 1);
    this->registered_events = false;
    this->device_add_timer = 0;
}

ctrlm_thunder_plugin_cec_t::~ctrlm_thunder_plugin_cec_t() {

}

bool ctrlm_thunder_plugin_cec_t::is_activated() {
    bool ret = false;
    if(Thunder::Helpers::is_systemd_process_active("cecdaemon")) {
        ret = ctrlm_thunder_plugin_t::is_activated();
    } else {
        THUNDER_LOG_WARN("%s: Cec daemon is not running...\n", __FUNCTION__);
    }
    return(ret);
}

bool ctrlm_thunder_plugin_cec_t::activate() {
    bool ret = false;
    if(Thunder::Helpers::is_systemd_process_active("cecdaemon")) {
        ret = ctrlm_thunder_plugin_t::activate();
    } else {
        THUNDER_LOG_ERROR("%s: Cec daemon is not running...\n", __FUNCTION__);
    }
    return(ret);
}

bool ctrlm_thunder_plugin_cec_t::enable(bool enable) {
    bool ret = false;
    JsonObject params, response;

    params["enabled"] = enable;
    if(this->call_plugin("setEnabled", (void *)&params, (void *)&response)) {
        if(response.HasLabel("success")) {
            if(response["success"].Boolean()) {
                ret = true;
            } else {
                THUNDER_LOG_ERROR("%s: setEnabled success is false\n", __FUNCTION__);
            }
        } else {
            THUNDER_LOG_ERROR("%s: setEnabled malformed response\n", __FUNCTION__);
        }
    } else {
        THUNDER_LOG_ERROR("%s: CEC setEnabled call failed!\n", __FUNCTION__);
    }
    return(ret);
}

void ctrlm_thunder_plugin_cec_t::get_cec_devices(std::vector<Thunder::CEC::cec_device_t> &cec_devices) {
    // Lock sempahore as we are touching CEC cache
    sem_wait(&this->semaphore);
    cec_devices = this->devices;
    // Unlock semaphore as we are done touching the CEC cache
    sem_post(&this->semaphore);
}

bool ctrlm_thunder_plugin_cec_t::_update_cec_devices() {
    bool ret = false;
    JsonObject params, response;

    THUNDER_LOG_INFO("%s: Calling CEC for device data\n", __FUNCTION__);

    // Lock sempahore as we are touching CEC cache
    sem_wait(&this->semaphore);

    this->devices.clear();
    if(this->call_plugin("getDeviceList", (void *)&params, (void *)&response)) {
        if(response.HasLabel("deviceList")) {
            JsonArray device_list = response["deviceList"].Array();
            for(int i = 0; i < device_list.Length(); i++) {
                if(device_list[i].Object().HasLabel("logicalAddress") &&
                   device_list[i].Object().HasLabel("osdName")        &&
                   device_list[i].Object().HasLabel("vendorID")) {
                       std::stringstream vendor_parse;
                       Thunder::CEC::cec_device_t device;
                       device.logical_address = device_list[i].Object()["logicalAddress"].Number();
                       device.osd             = device_list[i].Object()["osdName"].String();
                       device.port            = 0; // Not implemented
                       vendor_parse << std::hex << device_list[i].Object()["vendorID"].String();
                       vendor_parse >> device.vendor_id;
                       this->devices.push_back(device);
                   }
            }
        } else {
            std::string response_str;
            response.ToString(response_str);
            THUNDER_LOG_ERROR("%s: CEC getDeviceList response malformed: <%s>\n", __FUNCTION__, response_str.c_str());
        }
    } else {
        THUNDER_LOG_ERROR("%s: CEC getDeviceList call failed!\n", __FUNCTION__);
    }

    // Unlock semaphore as we are done touching the CEC cache
    sem_post(&this->semaphore);
    return(ret);
}

bool ctrlm_thunder_plugin_cec_t::register_events() {
    bool ret = this->registered_events;
    if(ret == false) {
        auto clientObject = (JSONRPC::LinkType<Core::JSON::IElement>*)this->plugin_client;
        if(clientObject) {
            ret = true;
            uint32_t thunderRet = clientObject->Subscribe<JsonObject>(CALL_TIMEOUT, _T("onDeviceAdded"), &_on_device_added, this);
            if(thunderRet != Core::ERROR_NONE) {
                THUNDER_LOG_ERROR("%s: Thunder subscribe failed <onDeviceAdded>\n", __FUNCTION__);
                ret = false;
            }
            thunderRet = clientObject->Subscribe<JsonObject>(CALL_TIMEOUT, _T("onDeviceRemoved"), &_on_device_removed, this);
            if(thunderRet != Core::ERROR_NONE) {
                THUNDER_LOG_ERROR("%s: Thunder subscribe failed <onDeviceRemoved>\n", __FUNCTION__);
                ret = false;
            }
            thunderRet = clientObject->Subscribe<JsonObject>(CALL_TIMEOUT, _T("onDeviceInfoUpdated"), &_on_device_info_changed, this);
            if(thunderRet != Core::ERROR_NONE) {
                THUNDER_LOG_ERROR("%s: Thunder subscribe failed <onDeviceInfoUpdated>\n", __FUNCTION__);
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

void ctrlm_thunder_plugin_cec_t::on_initial_activation() {
    this->_update_cec_devices();

    Thunder::Plugin::ctrlm_thunder_plugin_t::on_initial_activation();
}

void ctrlm_thunder_plugin_cec_t::on_device_updated() {
    THUNDER_LOG_INFO("%s: CEC Device changed\n", __FUNCTION__);
    this->_update_cec_devices();
}