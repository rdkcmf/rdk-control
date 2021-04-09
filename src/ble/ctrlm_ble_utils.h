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
#ifndef _CTRLM_BLE_UTILS_H_
#define _CTRLM_BLE_UTILS_H_

#include <string.h>
#include <jansson.h>
#include "ctrlm_hal_ble.h"
#include "ctrlm_ble_controller.h"

std::string ctrlm_ble_utils_BuildDBusDeviceObjectPath(const char *path_base, unsigned long long ieee_address);
unsigned long long ctrlm_ble_utils_GetIEEEAddressFromObjPath(std::string obj_path);
void ctrlm_ble_utils_PrintRCUStatus(ctrlm_hal_ble_RcuStatusData_t *status);
const char *ctrlm_ble_utils_RcuStateToString(ctrlm_ble_state_t type);
const char *ctrlm_ble_controller_type_str(ctrlm_ble_controller_type_t controller_type);
const char *ctrlm_ble_unpair_reason_str(ctrlm_ble_RcuUnpairReason_t reason);
const char *ctrlm_ble_reboot_reason_str(ctrlm_ble_RcuRebootReason_t reason);
const char *ctrlm_ble_rcu_action_str(ctrlm_ble_RcuAction_t reason);

#endif
