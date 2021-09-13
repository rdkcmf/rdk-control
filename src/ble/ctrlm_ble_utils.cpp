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

#include "ctrlm_ble_utils.h"

#include "../ctrlm.h"
#include "../ctrlm_hal.h"
#include "../ctrlm_utils.h"

using namespace std;

string ctrlm_ble_utils_BuildDBusDeviceObjectPath(const char *path_base, unsigned long long ieee_address)
{
    char objectPath[CTRLM_MAX_PARAM_STR_LEN];
    errno_t safec_rc = sprintf_s(objectPath, sizeof(objectPath), "%s/device_%02X_%02X_%02X_%02X_%02X_%02X", path_base,
                                                            0xFF & (unsigned int)(ieee_address >> 40),
                                                            0xFF & (unsigned int)(ieee_address >> 32),
                                                            0xFF & (unsigned int)(ieee_address >> 24),
                                                            0xFF & (unsigned int)(ieee_address >> 16),
                                                            0xFF & (unsigned int)(ieee_address >> 8),
                                                            0xFF & (unsigned int)(ieee_address));
    if(safec_rc < EOK) {
       ERR_CHK(safec_rc);
    }

    LOG_DEBUG("%s: device_path: <%s>\n", __FUNCTION__, objectPath);
    return string(objectPath);
}

unsigned long long ctrlm_ble_utils_GetIEEEAddressFromObjPath(std::string obj_path)
{
    // Dbus object path is of the form /com/sky/blercu/device_E8_0F_C8_10_33_D8
    // Parse out the IEEE address

    string addr_start_delim("device_");
    size_t addr_start_pos = obj_path.find(addr_start_delim);
    if (string::npos != addr_start_pos) {
        string addr_str = obj_path.substr(addr_start_pos + addr_start_delim.length(), 17);
        // LOG_DEBUG("%s: addr_str: <%s>\n", __FUNCTION__, addr_str.c_str());
        return ctrlm_convert_mac_string_to_long(addr_str.c_str());
    } else {
        return -1;
    }
}

const char *ctrlm_ble_utils_RcuStateToString(ctrlm_ble_state_t state)
{
    switch(state) {
        case CTRLM_BLE_STATE_INITIALIZING:    return("INITIALIZING");
        case CTRLM_BLE_STATE_IDLE:            return("IDLE");
        case CTRLM_BLE_STATE_SEARCHING:       return("SEARCHING");
        case CTRLM_BLE_STATE_PAIRING:         return("PAIRING");
        case CTRLM_BLE_STATE_COMPLETE:        return("COMPLETE");
        case CTRLM_BLE_STATE_FAILED:          return("FAILED");
        case CTRLM_BLE_STATE_WAKEUP_KEY:      return("WAKEUP_KEY");
        case CTRLM_BLE_STATE_UNKNOWN:         return("UNKNOWN");
        default:                              return("INVALID__TYPE");
    }
}

const char *ctrlm_ble_controller_type_str(ctrlm_ble_controller_type_t controller_type) {
    switch(controller_type) {
        case BLE_CONTROLLER_TYPE_IR:      return("INFRARED_CONTROLLER");
        case BLE_CONTROLLER_TYPE_PR1:     return("BLE_CONTROLLER_PR1");
        case BLE_CONTROLLER_TYPE_LC103:   return("BLE_CONTROLLER_LC103");
        case BLE_CONTROLLER_TYPE_EC302:   return("BLE_CONTROLLER_EC302");
        case BLE_CONTROLLER_TYPE_UNKNOWN: return("BLE_CONTROLLER_UNKNOWN");
        case BLE_CONTROLLER_TYPE_INVALID: return("BLE_CONTROLLER_INVALID");
        default:                                return("INVALID__TYPE");
    }
}

const char *ctrlm_ble_unpair_reason_str(ctrlm_ble_RcuUnpairReason_t reason) {
    switch(reason) {
        case CTRLM_BLE_RCU_UNPAIR_REASON_NONE:          return("NONE");
        case CTRLM_BLE_RCU_UNPAIR_REASON_SFM:           return("SPECIAL_FUNCTION_MODE");
        case CTRLM_BLE_RCU_UNPAIR_REASON_FACTORY_RESET: return("FACTORY_RESET");
        case CTRLM_BLE_RCU_UNPAIR_REASON_RCU_ACTION:    return("RCU_ACTION");
        case CTRLM_BLE_RCU_UNPAIR_REASON_INVALID:       return("INVALID");
        default:                                        return("INVALID__TYPE");
    }
}

const char *ctrlm_ble_reboot_reason_str(ctrlm_ble_RcuRebootReason_t reason) {
    switch(reason) {
        case CTRLM_BLE_RCU_REBOOT_REASON_FIRST_BOOT:        return("FIRST_BOOT");
        case CTRLM_BLE_RCU_REBOOT_REASON_FACTORY_RESET:     return("FACTORY_RESET");
        case CTRLM_BLE_RCU_REBOOT_REASON_NEW_BATTERIES:     return("NEW_BATTERIES");
        case CTRLM_BLE_RCU_REBOOT_REASON_ASSERT:            return("ASSERT");
        case CTRLM_BLE_RCU_REBOOT_REASON_BATTERY_INSERTION: return("BATTERY_INSERTION");
        case CTRLM_BLE_RCU_REBOOT_REASON_RCU_ACTION:        return("RCU_ACTION");
        case CTRLM_BLE_RCU_REBOOT_REASON_FW_UPDATE:         return("FW_UPDATE");
        case CTRLM_BLE_RCU_REBOOT_REASON_INVALID:           return("INVALID");
        default:                                            return("INVALID__TYPE");
    }
}

const char *ctrlm_ble_rcu_action_str(ctrlm_ble_RcuAction_t reason) {
    switch(reason) {
        case CTRLM_BLE_RCU_ACTION_REBOOT:           return("REBOOT");
        case CTRLM_BLE_RCU_ACTION_FACTORY_RESET:    return("FACTORY_RESET");
        case CTRLM_BLE_RCU_ACTION_UNPAIR:           return("UNPAIR");
        case CTRLM_BLE_RCU_ACTION_INVALID:          return("INVALID");
        default:                                    return("INVALID__TYPE");
    }
}
