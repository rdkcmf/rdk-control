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
#ifndef _CTRLM_HAL_BLE_H_
#define _CTRLM_HAL_BLE_H_

#include "ctrlm_hal.h"
#include <string>
#include <semaphore.h>
#include <vector>
#include <map>
#include <glib.h>
#include <stdint.h>
#include <linux/input.h>
#include "ctrlm_ipc.h"
#include "ctrlm_ipc_ble.h"

#define CTRLM_HAL_BLE_DEFAULT_DISCOVERY_TIMEOUT_MS (30 * 1000)

// EGTODO: Sky remote sends 100 byte ADPCM, or 384 byte PCM.  Do I keep this hardcoded?
#define CTRLM_HAL_BLE_MAX_VOICE_PACKET_BYTES   (96*2*2)   //(100)



/// @file ctrlm_hal_ble.h
///
/// @defgroup CTRL_MGR_HAL_BLE Control Manager BLE Network HAL API
/// @{

/// @addtogroup HAL_BLE_Enums   Enumerations
/// @{
/// @brief Enumerated Types
/// @details The Control Manager HAL provides enumerated types for logical groups of values.

/// @brief BleRcuDaemon IR Key Code types (CDI format)
typedef enum {
   CTRLM_HAL_BLE_IR_KEY_POWER    = 0xE000,
   CTRLM_HAL_BLE_IR_KEY_VOL_UP   = 0xE003,
   CTRLM_HAL_BLE_IR_KEY_VOL_DOWN = 0xE004,
   CTRLM_HAL_BLE_IR_KEY_MUTE     = 0xE005,
   CTRLM_HAL_BLE_IR_KEY_INPUT    = 0xE010
} ctrlm_hal_ble_IrKeyCodes_t;

/// @brief Supported voice data encoding types.
typedef enum {
   CTRLM_HAL_BLE_ENCODING_PCM = 0,
   CTRLM_HAL_BLE_ENCODING_ADPCM,
   CTRLM_HAL_BLE_ENCODING_UNKNOWN
} ctrlm_hal_ble_VoiceEncoding_t;

/// @brief Supported voice data encoding types.
typedef enum {
   CTRLM_HAL_BLE_PROPERTY_IEEE_ADDDRESS = 0,
   CTRLM_HAL_BLE_PROPERTY_DEVICE_ID,
   CTRLM_HAL_BLE_PROPERTY_NAME,
   CTRLM_HAL_BLE_PROPERTY_MANUFACTURER,
   CTRLM_HAL_BLE_PROPERTY_MODEL,
   CTRLM_HAL_BLE_PROPERTY_SERIAL_NUMBER,
   CTRLM_HAL_BLE_PROPERTY_HW_REVISION,
   CTRLM_HAL_BLE_PROPERTY_FW_REVISION,
   CTRLM_HAL_BLE_PROPERTY_SW_REVISION,
   CTRLM_HAL_BLE_PROPERTY_TOUCH_MODE,
   CTRLM_HAL_BLE_PROPERTY_TOUCH_MODE_SETTABLE,
   CTRLM_HAL_BLE_PROPERTY_BATTERY_LEVEL,
   CTRLM_HAL_BLE_PROPERTY_IR_CODE,
   CTRLM_HAL_BLE_PROPERTY_CONNECTED,
   CTRLM_HAL_BLE_PROPERTY_IS_PAIRING,
   CTRLM_HAL_BLE_PROPERTY_PAIRING_CODE,
   CTRLM_HAL_BLE_PROPERTY_AUDIO_STREAMING,
   CTRLM_HAL_BLE_PROPERTY_AUDIO_GAIN_LEVEL,
   CTRLM_HAL_BLE_PROPERTY_IR_STATE,
   CTRLM_HAL_BLE_PROPERTY_STATE,
   CTRLM_HAL_BLE_PROPERTY_IS_UPGRADING,
   CTRLM_HAL_BLE_PROPERTY_UPGRADE_PROGRESS,
   CTRLM_HAL_BLE_PROPERTY_UNPAIR_REASON,
   CTRLM_HAL_BLE_PROPERTY_REBOOT_REASON,
   CTRLM_HAL_BLE_PROPERTY_LAST_WAKEUP_KEY,
   CTRLM_HAL_BLE_PROPERTY_UNKNOWN
} ctrlm_hal_ble_RcuProperty_t;
/// @}

/// @addtogroup HAL_BLE_Structs Structures
/// @{
/// @brief Structure Definitions
/// @details The Control Manager HAL provides structures that are used in calls to the HAL networks.

typedef struct {
   int                           device_id;
   int                           battery_level;
   bool                          connected;
   unsigned long long            ieee_address;
   char                          fw_revision[CTRLM_MAX_PARAM_STR_LEN];
   char                          hw_revision[CTRLM_MAX_PARAM_STR_LEN];
   char                          sw_revision[CTRLM_MAX_PARAM_STR_LEN];
   char                          manufacturer[CTRLM_MAX_PARAM_STR_LEN];
   char                          model[CTRLM_MAX_PARAM_STR_LEN];
   char                          name[CTRLM_MAX_PARAM_STR_LEN];
   char                          serial_number[CTRLM_MAX_PARAM_STR_LEN];
   int                           ir_code;
   uint8_t                       audio_gain_level;
   bool                          audio_streaming;
   unsigned int                  touch_mode;
   bool                          touch_mode_settable;
   bool                          is_upgrading;
   int                           upgrade_progress;
   ctrlm_ble_RcuUnpairReason_t   unpair_reason;
   ctrlm_ble_RcuRebootReason_t   reboot_reason;
   uint8_t                       last_wakeup_key;
} ctrlm_hal_ble_rcu_data_t;

typedef struct {
   ctrlm_ble_state_t             state;
   ctrlm_ir_state_t              ir_state;
   bool                          is_pairing;
   int                           pairing_code;
   ctrlm_hal_ble_RcuProperty_t   property_updated;
   ctrlm_hal_ble_rcu_data_t      rcu_data;
} ctrlm_hal_ble_RcuStatusData_t;

typedef struct {
   unsigned long long         ieee_address;
   ctrlm_controller_id_t      controller_id;
   uint8_t                    data[CTRLM_HAL_BLE_MAX_VOICE_PACKET_BYTES];
   int                        length;
} ctrlm_hal_ble_VoiceData_t;

typedef struct {
   unsigned long long         ieee_address;
   struct input_event         event;
} ctrlm_hal_ble_IndKeypress_params_t;

typedef struct {
   unsigned long long            ieee_address;
} ctrlm_hal_ble_ReqVoiceSession_params_t;

typedef struct {
   ctrlm_hal_ble_rcu_data_t   rcu_data;
} ctrlm_hal_ble_IndPaired_params_t;

typedef struct {
   unsigned long long            ieee_address;
} ctrlm_hal_ble_IndUnPaired_params_t;

typedef struct {
   int timeout_ms;
} ctrlm_hal_ble_StartDiscovery_params_t;

typedef struct {
   unsigned int pair_code;
} ctrlm_hal_ble_PairWithCode_params_t;

typedef struct {
   ctrlm_controller_id_t         controller_id;
   unsigned long long            ieee_address;
   ctrlm_hal_ble_VoiceEncoding_t encoding;
   int                           fd;
} ctrlm_hal_ble_StartAudioStream_params_t;

typedef struct {
   unsigned long long ieee_address;
} ctrlm_hal_ble_StopAudioStream_params_t;

typedef struct {
   unsigned long long ieee_address;
   uint32_t error_status;
   uint32_t packets_received;
   uint32_t packets_expected;
} ctrlm_hal_ble_GetAudioStats_params_t;

typedef struct {
   unsigned long long                              ieee_address;
   std::map<ctrlm_key_code_t, std::vector<char> > *ir_codes;
} ctrlm_hal_ble_IRSetCode_params_t;

typedef struct {
   unsigned long long ieee_address;
} ctrlm_hal_ble_IRClear_params_t;

typedef struct {
   unsigned long long      ieee_address;
   ctrlm_fmr_alarm_level_t level;
   uint32_t                duration;   // Currently duration is ignored
} ctrlm_hal_ble_FindMe_params_t;

typedef struct {
   unsigned long long   ieee_address;
   std::string          file_path;
} ctrlm_hal_ble_FwUpgrade_params_t;

typedef struct {
   unsigned long long            ieee_address;
   ctrlm_ble_RcuUnpairReason_t   reason;
} ctrlm_hal_ble_GetRcuUnpairReason_params_t;
typedef struct {
   unsigned long long            ieee_address;
   ctrlm_ble_RcuRebootReason_t   reason;
} ctrlm_hal_ble_GetRcuRebootReason_params_t;
typedef struct {
   unsigned long long            ieee_address;
   uint8_t                       key;
} ctrlm_hal_ble_GetRcuLastWakeupKey_params_t;
typedef struct {
   unsigned long long      ieee_address;
   ctrlm_ble_RcuAction_t   action;
   bool                    wait_for_reply;
} ctrlm_hal_ble_SendRcuAction_params_t;

typedef struct {
   bool                    waking_up;
} ctrlm_hal_ble_HandleDeepsleep_params_t;


/// @}

/// @addtogroup HAL_BLE_Function_Typedefs   Request Function Prototypes
/// @{
/// @brief Functions that are implemented by the BLE network device
/// @details The Control Manager provides function type definitions for functions that must be implemented by the BLE network.

/// @brief Start HAL threads that need to be started after controllers in the network are already loaded
/// @details This function is called by Control Manager network to start the key monitor thread
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_StartThreads_t)(void);

/// @brief Get Paired Devices Reported by HAL Function Prototype
/// @details This function is called by Control Manager network to get a list of paired devices coming from the HAL (ultimately from Bluez)
/// @param[out] devices - IEEE MAC addresses of paired devices
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_GetDevices_t)(std::vector<unsigned long long> &devices);

/// @brief Get all properties of an RCU
/// @details This function is called by Control Manager network to all properties of an RCU given ieee_addresss
/// @param[in] rcu_props.rcu_data.ieee_address - IEEE MAC address of RCU requested
/// @param[out] rcu_props.rcu_data - all properties of an RCU
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_GetAllRcuProps_t)(ctrlm_hal_ble_RcuStatusData_t &rcu_props);

/// @brief Start BLE Discovery Function Prototype
/// @details This function is called by Control Manager network to enable discovery mode in the bluetooth chip.
/// @param[in] timeout_ms - in milliseconds, discovery mode timeout
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_StartDiscovery_t)(ctrlm_hal_ble_StartDiscovery_params_t params);

/// @brief Pair a specific BLE device given a code
/// @details This function is called by Control Manager network to pair to a specific device with a code.
/// @param[in] pair_code - hash of the device MAC address:
///               The MAC hash is calculated by adding all the bytes of the MAC address
///               together, and masking with 0xFF.
///               e.g., MAC = AA:BB:CC:DD:EE:FF, hash = (AA+BB+CC+DD+EE+FF) & 0xFF
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_PairWithCode_t)(ctrlm_hal_ble_PairWithCode_params_t params);

/// @brief Controller Unpairing Function Prototype - EGTODO: Sky BLE Daemon does not implement this yet.
/// @details This function is called by Control Manager to unpair the specified controller in the HAL network.
/// @param[in] ieee_address - MAC address of the controller
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_Unpair_t)(unsigned long long ieee_address);

/// @brief Controller Start Voice Audio Streaming Function Prototype
/// @details This function is called by Control Manager to start audio streaming on the specified controller.
/// @param[in] ieee_address - MAC address of the controller
/// @param[in] encoding - ADPCM or PCM (100 byte ADPCM from Sky Daemon isn't supported by Voice SDK, so currently only PCM is supported)
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_StartAudioStream_t)(ctrlm_hal_ble_StartAudioStream_params_t *params);

/// @brief Controller Stop Voice Audio Streaming Function Prototype
/// @details This function is called by Control Manager to stop audio streaming on the specified controller.
/// @param[in] ieee_address - MAC address of the controller
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_StopAudioStream_t)(ctrlm_hal_ble_StopAudioStream_params_t params);

/// @brief Controller Get Audio Statistics Function Prototype
/// @details This function is called by Control Manager to get voice stats on the specified controller, like total packets expected and packets received.
/// @param[in] ieee_address - MAC address of the controller
/// @param[out] error_status - 0 for success, 1 means the RCU disconnected while audio was being streamed.
/// @param[out] packets_received - number of packets actually received from the controller
/// @param[out] packets_expected - number of packets expected to be received from the controller
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_GetAudioStats_t)(ctrlm_hal_ble_GetAudioStats_params_t *params);

/// @brief Controller IR Set Code Function Prototype
/// @details Sets the IR 5 digit code to set on a controller
/// @param[in] ieee_address - MAC address of the controller
/// @param[in] type - TV or AVR
/// @param[in] code - IR code to set on the controller.
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_IRSetCode_t)(ctrlm_hal_ble_IRSetCode_params_t params);

/// @brief Controller Clear IR Codes Function Prototype
/// @details Clears all codes currently loaded in the remote control
/// @param[in] ieee_address - MAC address of the controller
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_IRClear_t)(ctrlm_hal_ble_IRClear_params_t params);

/// @brief Trigger FindMe alarm on the remote
/// @param[in] level - (0): Disable the find me signal. (1): Signal the find me beeping at the 'mid' level. (2): Signal the find me beeping at the 'high' level
/// @param[in] duration - currently ignored
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_FindMe_t)(ctrlm_hal_ble_FindMe_params_t params);

/// @brief Get the log level bitmask of the BLE daemon
/// @param[out] log_levels - 0x020 Debug, 0x010 Info, 0x008 Milestone, 0x004 Warning, 0x002 Error, 0x001 Fatal
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_GetDaemonLogLevels_t)(daemon_logging_t *logging);

/// @brief Set the log level bitmask of the BLE daemon
/// @param[out] log_levels - 0x020 Debug, 0x010 Info, 0x008 Milestone, 0x004 Warning, 0x002 Error, 0x001 Fatal
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_SetDaemonLogLevels_t)(daemon_logging_t logging);

/// @brief Upgrade RCU firmware
/// @param[in] ieee_address - MAC address of the controller
/// @param[in] file_path - path to the firmware binary
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_FwUpgrade_t)(ctrlm_hal_ble_FwUpgrade_params_t params);


/// @brief Read the reason for the last unpairing from the RCU
/// @param[in] ieee_address - MAC address of the controller
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_GetRcuUnpairReason_t)(ctrlm_hal_ble_GetRcuUnpairReason_params_t *params);

/// @brief Read the reason for the last reboot from the RCU
/// @param[in] ieee_address - MAC address of the controller
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_GetRcuRebootReason_t)(ctrlm_hal_ble_GetRcuRebootReason_params_t *params);

/// @brief Read the last key press characteristic from the RCU
/// @param[in] ieee_address - MAC address of the controller
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_GetRcuLastWakeupKey_t)(ctrlm_hal_ble_GetRcuLastWakeupKey_params_t *params);

/// @brief Send an action for the RCU to perform
/// @param[in] ieee_address - MAC address of the controller
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_SendRcuAction_t)(ctrlm_hal_ble_SendRcuAction_params_t params);

/// @brief notify the HAL of a deepsleep transition
/// @param[in] waking_up - true if waking from deepsleep, false if going into deepsleep
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_HandleDeepsleep_t)(ctrlm_hal_ble_HandleDeepsleep_params_t params);
/// @}

/// @addtogroup HAL_BLE_Callback_Functions - Callback Function Prototypes
/// @{
/// @brief Functions that are implemented by the BLE Network device that the HAL layer can call
/// @details ControlMgr provides a set of functions that the BLE HAL layer can call.

/// @brief BLE Confirm Init Parameters Structure
/// @details The structure which is passed from the HAL Network device in the BLE initialization confirmation.
typedef struct {
   ctrlm_hal_result_t                      result;
   char                                    version[CTRLM_HAL_NETWORK_VERSION_STRING_SIZE];
   ctrlm_hal_req_term_t                    term;
   ctrlm_hal_req_property_get_t            property_get;                                   ///< Network Property Get Request
   ctrlm_hal_req_property_set_t            property_set;                                   ///< Network Property Set Request
   ctrlm_hal_ble_req_StartThreads_t        start_threads;
   ctrlm_hal_ble_req_GetDevices_t          get_devices;
   ctrlm_hal_ble_req_GetAllRcuProps_t      get_all_rcu_props;
   ctrlm_hal_ble_req_StartDiscovery_t      start_discovery;
   ctrlm_hal_ble_req_PairWithCode_t        pair_with_code;
   ctrlm_hal_ble_req_Unpair_t              unpair;
   ctrlm_hal_ble_req_StartAudioStream_t    start_audio_stream;
   ctrlm_hal_ble_req_StopAudioStream_t     stop_audio_stream;
   ctrlm_hal_ble_req_GetAudioStats_t       get_audio_stats;
   ctrlm_hal_ble_req_IRSetCode_t           set_ir_codes;
   ctrlm_hal_ble_req_IRClear_t             clear_ir_codes;
   ctrlm_hal_ble_req_FindMe_t              find_me;
   ctrlm_hal_ble_req_GetDaemonLogLevels_t  get_daemon_log_levels;
   ctrlm_hal_ble_req_SetDaemonLogLevels_t  set_daemon_log_levels;
   ctrlm_hal_ble_req_FwUpgrade_t           fw_upgrade;
   ctrlm_hal_ble_req_GetRcuUnpairReason_t  get_rcu_unpair_reason;
   ctrlm_hal_ble_req_GetRcuRebootReason_t  get_rcu_reboot_reason;
   ctrlm_hal_ble_req_GetRcuLastWakeupKey_t get_rcu_last_wakeup_key;
   ctrlm_hal_ble_req_SendRcuAction_t       send_rcu_action;
   ctrlm_hal_ble_req_HandleDeepsleep_t     handle_deepsleep;
} ctrlm_hal_ble_cfm_init_params_t;

/// @brief Network Init Confirmation Function
/// @details This function is called after the HAL network initialization is complete.
/// @param[in] network_id - Identifier for this HAL network device
/// @param[in] params     - Initialization confirmation parameters
/// @return CTRLM_HAL_RESULT_SUCCESS for success.  All other values indicate an error has occurred.
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_cfm_init_t)(ctrlm_network_id_t network_id, ctrlm_hal_ble_cfm_init_params_t params);
/// @brief BLE RCU Status Indication Function
/// @details This function is called by the HAL network when an RCU property or subsystem state changes
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_ind_RcuStatus_t)(ctrlm_network_id_t id, ctrlm_hal_ble_RcuStatusData_t *params);
/// @brief BLE Paired Indication Function
/// @details This function is called by the HAL network when its notified of a device that was just paired
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_ind_Paired_t)(ctrlm_network_id_t id, ctrlm_hal_ble_IndPaired_params_t *params);
/// @brief BLE UnPaired Indication Function
/// @details This function is called by the HAL network when its notified of a device that was just unpaired
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_ind_UnPaired_t)(ctrlm_network_id_t id, ctrlm_hal_ble_IndUnPaired_params_t *params);
/// @brief BLE Voice Data Indication Function
/// @details This function is called by the HAL network upon receipt of a packet of voice data from the controller
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_ind_VoiceData_t)(ctrlm_network_id_t id, ctrlm_hal_ble_VoiceData_t *params);
/// @brief BLE Keypress Indication Function
/// @details This function is called by the HAL network when it receives a BLE keypress
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_ind_Keypress_t)(ctrlm_network_id_t id, ctrlm_hal_ble_IndKeypress_params_t *params);
/// @brief BLE Voice Session Begin
/// @details This function is called by the HAL network to request a voice session to begin
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_VoiceSessionBegin_t)(ctrlm_network_id_t id, ctrlm_hal_ble_ReqVoiceSession_params_t *params);
/// @brief BLE Voice Session End
/// @details This function is called by the HAL network to request the end of a voice session
typedef ctrlm_hal_result_t (*ctrlm_hal_ble_req_VoiceSessionEnd_t)(ctrlm_network_id_t id, ctrlm_hal_ble_ReqVoiceSession_params_t *params);
/// @}

/// @addtogroup HAL_BLE_Functions Function Prototypes
/// @{

/// @brief Network Initialization Structure
/// @details The structure which is passed from Control Manager to the HAL Network device for initialization.
typedef struct {
   unsigned long                          api_version;               ///< Version of the HAL API
   ctrlm_network_id_t                     network_id;                ///< Unique identiifer for the network
   ctrlm_hal_ble_cfm_init_t               cfm_init;                  ///< Network Initialization Confirm
   ctrlm_hal_ble_ind_RcuStatus_t          ind_status;                ///< RCU Status Indication
   ctrlm_hal_ble_ind_Paired_t             ind_paired;                ///< RCU Paired Indication
   ctrlm_hal_ble_ind_UnPaired_t           ind_unpaired;              ///< RCU UnPaired Indication
   ctrlm_hal_ble_ind_VoiceData_t          ind_voice_data;            ///< Voice Audio Data Indication
   ctrlm_hal_ble_ind_Keypress_t           ind_keypress;              ///< Key press indication
   ctrlm_hal_ble_req_VoiceSessionBegin_t  req_voice_session_begin;   ///< Request a voice session
   ctrlm_hal_ble_req_VoiceSessionEnd_t    req_voice_session_end;     ///< Request to end a voice session
} ctrlm_hal_ble_main_init_t;

/// @brief BLE Network Main
/// @details This function is called to begin the BLE HAL Network initialization.  This function must be implemented by the HAL network and directly
/// linked with Control Manager. The provided network id must be used by the network in all communication with the Control Manager.
void *ctrlm_hal_ble_main(ctrlm_hal_ble_main_init_t *main_init);

/// @brief Network Init Confirmation Function
/// @details This function is called after the HAL network initialization is complete.
ctrlm_hal_result_t ctrlm_hal_ble_cfm_init(ctrlm_network_id_t network_id, ctrlm_hal_ble_cfm_init_params_t params);
/// @}

/// @}
#endif
