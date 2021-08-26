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
#ifndef __CTRLM_THUNDER_PLUGIN_CEC_SINK_H__
#define __CTRLM_THUNDER_PLUGIN_CEC_SINK_H__
#include "ctrlm_thunder_plugin.h"
#include <semaphore.h>
#include <map>

namespace Thunder {
namespace CECSink {

class cec_device_t {
public:
    cec_device_t() {
        this->port            = -1;
        this->logical_address = 0xFF;
        this->vendor_id       = 0xFFFF;
        this->osd             = "";
    }

public:
    int         port;
    uint8_t     logical_address;
    std::string osd;
    uint16_t    vendor_id;
};

/**
 * This class is used within ControlMgr to interact with the CECSINK Thunder Plugin.
 */
class ctrlm_thunder_plugin_cec_sink_t : public Thunder::Plugin::ctrlm_thunder_plugin_t {
public:
    /**
     * CECSink Thunder Plugin Default Constructor
     */
    ctrlm_thunder_plugin_cec_sink_t();

    /**
     * CECSINK Thunder Plugin Destructor
     */
    virtual ~ctrlm_thunder_plugin_cec_sink_t();

    /**
     * This function is used to get the HDMI CEC device data from cache.
     * @param cec_devices The vector that the resulting CEC device data will be placed.
     */
    void get_cec_devices(std::vector<cec_device_t> &cec_devices);

public:
    /** 
     * This function is technically used internally but from static function. This is used to handle events in which devices are updated.
     */
    void on_device_updated();

protected:
    /**
     * This function is called when registering for Thunder events.
     * @return True if the events were registered for successfully otherwise False.
     */
    virtual bool register_events();

    /**
     * This function is called when the initial activation of the plugin occurs.
     */
    virtual void on_initial_activation();

    /**
     * This function actually calls the CEC SINK Thunder Plugin to get the CEC device data.
     * @return True if the call was successful and there is data available, otherwise False.
     */
    bool _update_cec_devices();

private:
    std::vector<cec_device_t>            devices;
    sem_t                                semaphore;
    bool                                 registered_events;

public:
    int                                  device_add_timer; // needs to be modified by static functions

};
};
};
#endif