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
#ifndef _CTRLM_RF4CE_CONTROLLER_H_
#define _CTRLM_RF4CE_CONTROLLER_H_

#include <string>
#include <vector>
#include "ctrlm_hal_rf4ce.h"
#include "ctrlm_ipc_device_update.h"
#include "ctrlm_ipc_voice.h"
#include "ctrlm_voice.h"
#ifdef ASB
#include "../cpc/asb/advanced_secure_binding.h"
#endif
#include "ctrlm_voice_session.h"
#ifdef USE_VOICE_SDK
#include "ctrlm_voice_obj.h"
#endif

class ctrlm_obj_network_rf4ce_t;

#define CTRLM_RF4CE_CONST_MAX_KEY_REPEAT_INTERVAL          (120) // maximum time between consecutive user control repeated command frame transmission
#define CTRLM_RF4CE_CONST_BLACKOUT_TIME                    (100) // time at the start of the validation procedure during which packets must not be transmitted

#define CTRLM_RF4CE_CONST_RESPONSE_IDLE_TIME                (50) // time a device must wait after the successful transmission of a request command frame before enabling its receiver to receive a response command frame
#define CTRLM_RF4CE_CONST_RESPONSE_IDLE_TIME_FF             (15) // time a device must wait after the successful transmission of a request command frame before enabling its receiver to receive a response command frame for Far Field Devices
#define CTRLM_RF4CE_CONST_RESPONSE_WAIT_TIME               (100) // The maximum time in ms a device MUST wait after the aplcResponseIdle WaitTime expired, to receive a response command frame following a request command frame
#ifdef ASB
#define CTRLM_RF4CE_CONST_ASB_BLACKOUT_TIME                 (50) // The time dedicated to ASB key derivation during the pairing process.
#endif

#define CTRLM_RF4CE_VENDOR_ID_COMCAST                   (0x109D) //

// This flag creates controller variables to store network wide parameters in the controller object.  This should not be enabled unless there is a need to
// make changes to these attributes on a per controller basis.  And if that happens, only the desired attributes should be added (not all of them like it is now).
//#define CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES

#define IR_RF_DATABASE_STATUS_FORCE_DOWNLOAD            (0x01)
#define IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE  (0x02)
#define IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE (0x04)
#define IR_RF_DATABASE_STATUS_TX_IR_DESCRIPTOR          (0x08)
#define IR_RF_DATABASE_STATUS_CLEAR_ALL_5_DIGIT_CODES   (0x10)
#define IR_RF_DATABASE_STATUS_DB_DOWNLOAD_NO            (0x40)
#define IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES           (0x80)
#define IR_RF_DATABASE_STATUS_RESERVED                  (0x20)
#define IR_RF_DATABASE_STATUS_DEFAULT                   (IR_RF_DATABASE_STATUS_DB_DOWNLOAD_NO)

#define IR_RF_DATABASE_ATTRIBUTE_FLAGS_RF_PRESSED       (0x01)
#define IR_RF_DATABASE_ATTRIBUTE_FLAGS_RF_REPEATED      (0x02)
#define IR_RF_DATABASE_ATTRIBUTE_FLAGS_RF_RELEASED      (0x04)
#define IR_RF_DATABASE_ATTRIBUTE_FLAGS_IR_SPECIFIED     (0x08)
#define IR_RF_DATABASE_ATTRIBUTE_FLAGS_DEVICE_TYPE_MASK (0x30)
#define IR_RF_DATABASE_ATTRIBUTE_FLAGS_USE_DEFAULT      (0x40)
#define IR_RF_DATABASE_ATTRIBUTE_FLAGS_PERMANENT        (0x80)

#define CONTROLLER_IRDB_STATUS_FLAGS_NO_IR_PROGRAMMED             (0x01)
#define CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_TYPE                   (0x02)
#define CONTROLLER_IRDB_STATUS_FLAGS_IR_RF_DB                     (0x04)
#define CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_TV                (0x08)
#define CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR               (0x10)
#define CONTROLLER_IRDB_STATUS_FLAGS_5_DIGIT_CODE_LOAD_SUPPORTED  (0x80)

#define CONTROLLER_IRDB_STATUS_LOAD_STATUS_NONE                           (0)
#define CONTROLLER_IRDB_STATUS_LOAD_STATUS_CODE_CLEARING_SUCCESS          (1)
#define CONTROLLER_IRDB_STATUS_LOAD_STATUS_CODE_SETTING_SUCCESS           (2)
#define CONTROLLER_IRDB_STATUS_LOAD_STATUS_SET_AND_CLEAR_ERROR            (3)
#define CONTROLLER_IRDB_STATUS_LOAD_STATUS_CODE_CLEAR_FAILED              (4)
#define CONTROLLER_IRDB_STATUS_LOAD_STATUS_CODE_SET_FAILED_INVALID_CODE   (5)
#define CONTROLLER_IRDB_STATUS_LOAD_STATUS_CODE_SET_FAILED_UNKNOWN_REASON (6)

#define TARGET_IRDB_STATUS_FLAGS_NO_IR_PROGRAMMED       (0x01)
#define TARGET_IRDB_STATUS_FLAGS_IR_RF_DB               (0x04)
#define TARGET_IRDB_STATUS_FLAGS_IR_DB_CODE_TV          (0x08)
#define TARGET_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR         (0x10)
#define TARGET_IRDB_STATUS_DEFAULT                      (CONTROLLER_IRDB_STATUS_FLAGS_NO_IR_PROGRAMMED)

#define BATTERY_STATUS_FLAGS_REPLACEMENT (0x01)
#define BATTERY_STATUS_FLAGS_CHARGING    (0x02)

#define FAR_FIELD_CONFIGURATION_FLAGS_OPENING_CHIME      (0x01)
#define FAR_FIELD_CONFIGURATION_FLAGS_CLOSING_CHIME      (0x02)
#define FAR_FIELD_CONFIGURATION_FLAGS_PRIVACY_CHIME      (0x04)
#define FAR_FIELD_CONFIGURATION_FLAGS_CONVERSATIONAL     (0x08)

#define FAR_FIELD_CONFIGURATION_CHIME_VOLUME_LOW         (-20)
#define FAR_FIELD_CONFIGURATION_CHIME_VOLUME_MEDIUM      (-10)
#define FAR_FIELD_CONFIGURATION_CHIME_VOLUME_HIGH        (-4)

#define PRIVACY_FLAGS_ENABLED                            (0x01)
#define CONTROLLER_CAPABILITIES_BYTE0_FLAGS_FMR          (0x01)

// Polling Defines
#define POLLING_METHODS_FLAG_HEARTBEAT                   (0x01)
#define POLLING_METHODS_FLAG_MAC                         (0x02)

#define POLLING_TRIGGER_FLAG_TIME                        (0x01) // only supported trigger for mac polling
#define POLLING_TRIGGER_FLAG_KEY_PRESS                   (0x02)
#define POLLING_TRIGGER_FLAG_ACCELEROMETER               (0x04)
#define POLLING_TRIGGER_FLAG_REBOOT                      (0x08)
#define POLLING_TRIGGER_FLAG_VOICE                       (0x10)
#define POLLING_TRIGGER_FLAG_POLL_AGAIN                  (0x20)
#define POLLING_TRIGGER_FLAG_STATUS                      (0x40)
#define POLLING_TRIGGER_FLAG_VOICE_SESSION               (0x80)

#define HEARTBEAT_RESPONSE_FLAG_POLL_AGAIN               (0x01)

#define POLLING_RESPONSE_DATA_LEN                        (0x05)
// End of Polling Defines

#define POLLING_MAC_INTERVAL_MIN                         (1000)  //msec
#define POLLING_MAC_INTERVAL_MAX                         (60000) //msec

#define CTRLM_RF4CE_RIB_ATTR_LEN_PERIPHERAL_ID              (4)
#define CTRLM_RF4CE_RIB_ATTR_LEN_RF_STATISTICS             (16)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING                 (4)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_BUILD_ID       (92)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_DSP_BUILD_ID   (92)
#define CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS            (11)
#define CTRLM_RF4CE_RIB_ATTR_LEN_SHORT_RF_RETRY_PERIOD      (4)
#define CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_ID_DATA            (40)
#define CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS            (1)
#define CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION      (8)
#define CTRLM_RF4CE_RIB_ATTR_LEN_PRIVACY                    (1)
#define CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_CAPABILITIES    (8)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_STATUS       (5)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_LENGTH       (1)
#define CTRLM_RF4CE_RIB_ATTR_LEN_MAXIMUM_UTTERANCE_LENGTH   (2)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_ENCRYPTION   (1)
#define CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_DATA_RETRY       (1)
#define CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_CSMA_BACKOFF     (1)
#define CTRLM_RF4CE_RIB_ATTR_LEN_MIN_VOICE_DATA_BACKOFF     (1)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES  (2)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_TARG_AUDIO_PROFILES  (2)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS           (8)
#define CTRLM_RF4CE_RIB_ATTR_LEN_RIB_ENTRIES_UPDATED        (1)
#define CTRLM_RF4CE_RIB_ATTR_LEN_RIB_UPDATE_CHECK_INTERVAL  (2)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_STATISTICS  (16)
#define CTRLM_RF4CE_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS       (5)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_QOS          (7)
#define CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING          (4)
#define CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME              (20)
#define CTRLM_RF4CE_RIB_ATTR_LEN_DOWNLOAD_RATE              (1)
#define CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_POLLING_PERIOD      (2)
#define CTRLM_RF4CE_RIB_ATTR_LEN_DATA_REQUEST_WAIT_TIME     (2)
#define CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS      (1)
#define CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE            (92)
#define CTRLM_RF4CE_RIB_ATTR_LEN_VALIDATION_CONFIGURATION   (4)
#define CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS    (15)
#define CTRLM_RF4CE_RIB_ATTR_LEN_IRDB_ENTRY_ID_NAME        (CTRLM_MAX_PARAM_STR_LEN)
#define CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS        (13)
#define CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_CONFIGURATION   (20)
#define CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_METRICS         (20)
#define CTRLM_RF4CE_RIB_ATTR_LEN_DSP_CONFIGURATION         (20)
#define CTRLM_RF4CE_RIB_ATTR_LEN_DSP_METRICS               (20)
#define CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_DUMP               (16)
#define CTRLM_RF4CE_RIB_ATTR_LEN_GENERAL_PURPOSE           (16)
#define CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATS              (16)
#define CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_STATS              (16)
#define CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST                   (8)
#define CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_DIAGNOSTICS        (20)
#define CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATISTICS         (16)
#define CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_MILESTONES        (sizeof(battery_voltage_milestones_t)) 
#define CTRLM_RF4CE_RIB_ATTR_LEN_UPTIME_PRIVACY_INFO       (12)

#define CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS_MINUS_LOAD_STATUS_BYTES (13)

#define CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_SOFTWARE          (0x00)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_HARDWARE          (0x01)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_IRDB              (0x02)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_BUILD_ID          (0x03)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_DSP               (0x04)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_KEYWORD_MODEL     (0x05)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_ARM               (0x06)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_DSP_BUILD_ID      (0x07)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_UPDATE_VERSIONING_BOOTLOADER (0x01)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_UPDATE_VERSIONING_GOLDEN     (0x02)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_UPDATE_VERSIONING_AUDIO_DATA (0x10)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_TARGET_ID_DATA_RECEIVER_ID   (0x01)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_TARGET_ID_DATA_DEVICE_ID     (0x02)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_POLLING_CONFIGURATION_MAC    (0x01)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_POLLING_CONFIGURATION_HEARTBEAT (0x01)
#define CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL                      (0x00)


#define CTRLM_RF4CE_LEN_VOICE_METRICS                      (44)
#define CTRLM_RF4CE_LEN_FIRMWARE_UPDATED                    (1)
#define CTRLM_RF4CE_LEN_TIME_LAST_CHECKIN_FOR_DEVICE_UPDATE (4)

#define CTRLM_RF4CE_LEN_UUID_STRING                        (64)

#define RF4CE_BINDING_CONFIG_FULL_ROLLBACK_ENABLED         (0x01)

#define RF4CE_PRODUCT_NAME_UNKNOWN                         ("UNKNOWN")
#define RF4CE_PRODUCT_NAME_IMPORT_ERROR                    ("IMPORT")
#define RF4CE_PRODUCT_NAME_XR11                            ("XR11-20")
#define RF4CE_PRODUCT_NAME_XR15                            ("XR15-10")
#define RF4CE_PRODUCT_NAME_XR15V2                          ("XR15-20")
#define RF4CE_PRODUCT_NAME_XR16                            ("XR16-10")
#define RF4CE_PRODUCT_NAME_XR19                            ("XR19-10")
#define RF4CE_PRODUCT_NAME_XR5                             ("XR5-40")
#define RF4CE_XR11_VERSION_HARDWARE_MANUFACTURER           (2)
#define RF4CE_XR11_VERSION_HARDWARE_MODEL                  (2)
#define RF4CE_XR11_VERSION_HARDWARE_REVISION_MIN           (1)
#define RF4CE_XR15_VERSION_HARDWARE_MANUFACTURER           (2)
#define RF4CE_XR15_VERSION_HARDWARE_MODEL_REV_0            (0)
#define RF4CE_XR15_VERSION_HARDWARE_MODEL_REV_3            (3)
#define RF4CE_XR15_VERSION_HARDWARE_REVISION_MIN           (1)
#define RF4CE_BATTERY_STATUS_VOLTAGE_LOADED_DEFAULT        (3000*255/4000) //3000 millivolts
#define RF4CE_BATTERY_STATUS_VOLTAGE_UNLOADED_DEFAULT      (3000*255/4000) //3000 millivolts
#define RF4CE_BATTERY_STATUS_VOLTAGE_PERCENTAGE_DEFAULT    (100)

#define RF4CE_CHECKIN_FOR_DEVICE_UPDATE_SECONDS_TO_HOURS   (3600)
#define RF4CE_CHECKIN_FOR_DEVICE_UPDATE_HOURS              (24)

#define RF4CE_DOWNLOAD_IMAGE_ID_INVALID                    (0xFFFF)

#define RF4CE_DSP_CONFIGURATION_FLAG_IC                         (0x01) // Flag for Interference Canceller
#define RF4CE_DSP_CONFIGURATION_MAX_ATTENUATION_PRINT(x)        ((x & 0x0F) * 2)
#define RF4CE_DSP_CONFIGURATION_MAX_ATTENUATION_IS_DEFAULT(x)   ((x & 0x0F) == 0x0F)
#define RF4CE_DSP_CONFIGURATION_UPDATE_THRESHOLD_PRINT(x)       ((((x & 0xF0) >> 4) * 4) + 40)
#define RF4CE_DSP_CONFIGURATION_UPDATE_THRESHOLD_IS_DEFAULT(x)  ((x & 0xF0) == 0xF0)
#define RF4CE_DSP_CONFIGURATION_DETECT_THRESHOLD_PRINT(x)       (((x & 0x0F) * 4) + 40)
#define RF4CE_DSP_CONFIGURATION_DETECT_THRESHOLD_IS_DEFAULT(x)  ((x & 0x0F) == 0x0F)


typedef enum {
   CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU   = 0xC0,
   CTRLM_RF4CE_PROFILE_ID_VOICE         = 0xC1,
   CTRLM_RF4CE_PROFILE_ID_DEVICE_UPDATE = 0xC2
} ctrlm_rf4ce_profile_id_t;

typedef enum {
   CTRLM_RF4CE_RIB_ATTR_ID_PERIPHERAL_ID             = 0x00,
   CTRLM_RF4CE_RIB_ATTR_ID_RF_STATISTICS             = 0x01,
   CTRLM_RF4CE_RIB_ATTR_ID_VERSIONING                = 0x02,
   CTRLM_RF4CE_RIB_ATTR_ID_BATTERY_STATUS            = 0x03,
   CTRLM_RF4CE_RIB_ATTR_ID_SHORT_RF_RETRY_PERIOD     = 0x04,
   CTRLM_RF4CE_RIB_ATTR_ID_TARGET_ID_DATA            = 0x05,
   CTRLM_RF4CE_RIB_ATTR_ID_POLLING_METHODS           = 0x08,
   CTRLM_RF4CE_RIB_ATTR_ID_POLLING_CONFIGURATION     = 0x09,
   CTRLM_RF4CE_RIB_ATTR_ID_PRIVACY                   = 0x0B,
   CTRLM_RF4CE_RIB_ATTR_ID_CONTROLLER_CAPABILITIES   = 0x0C,
   CTRLM_RF4CE_RIB_ATTR_ID_VOICE_COMMAND_STATUS      = 0x10,
   CTRLM_RF4CE_RIB_ATTR_ID_VOICE_COMMAND_LENGTH      = 0x11,
   CTRLM_RF4CE_RIB_ATTR_ID_MAXIMUM_UTTERANCE_LENGTH  = 0x12,
   CTRLM_RF4CE_RIB_ATTR_ID_VOICE_COMMAND_ENCRYPTION  = 0x13,
   CTRLM_RF4CE_RIB_ATTR_ID_MAX_VOICE_DATA_RETRY      = 0x14,
   CTRLM_RF4CE_RIB_ATTR_ID_MAX_VOICE_CSMA_BACKOFF    = 0x15,
   CTRLM_RF4CE_RIB_ATTR_ID_MIN_VOICE_DATA_BACKOFF    = 0x16,
   CTRLM_RF4CE_RIB_ATTR_ID_VOICE_CTRL_AUDIO_PROFILES = 0x17,
   CTRLM_RF4CE_RIB_ATTR_ID_VOICE_TARG_AUDIO_PROFILES = 0x18,
   CTRLM_RF4CE_RIB_ATTR_ID_VOICE_STATISTICS          = 0x19,
   CTRLM_RF4CE_RIB_ATTR_ID_RIB_ENTRIES_UPDATED       = 0x1A,
   CTRLM_RF4CE_RIB_ATTR_ID_RIB_UPDATE_CHECK_INTERVAL = 0x1B,
   CTRLM_RF4CE_RIB_ATTR_ID_VOICE_SESSION_STATISTICS  = 0x1C,
   CTRLM_RF4CE_RIB_ATTR_ID_OPUS_ENCODING_PARAMS      = 0x1D,
   CTRLM_RF4CE_RIB_ATTR_ID_VOICE_SESSION_QOS         = 0x1E,
   CTRLM_RF4CE_RIB_ATTR_ID_UPDATE_VERSIONING         = 0x31,
   CTRLM_RF4CE_RIB_ATTR_ID_PRODUCT_NAME              = 0x32,
   CTRLM_RF4CE_RIB_ATTR_ID_DOWNLOAD_RATE             = 0x33,
   CTRLM_RF4CE_RIB_ATTR_ID_UPDATE_POLLING_PERIOD     = 0x34,
   CTRLM_RF4CE_RIB_ATTR_ID_DATA_REQUEST_WAIT_TIME    = 0x35,
   CTRLM_RF4CE_RIB_ATTR_ID_IR_RF_DATABASE_STATUS     = 0xDA,
   CTRLM_RF4CE_RIB_ATTR_ID_IR_RF_DATABASE            = 0xDB,
   CTRLM_RF4CE_RIB_ATTR_ID_VALIDATION_CONFIGURATION  = 0xDC,
   CTRLM_RF4CE_RIB_ATTR_ID_CONTROLLER_IRDB_STATUS    = 0xDD,
   CTRLM_RF4CE_RIB_ATTR_ID_TARGET_IRDB_STATUS        = 0xDE,
   CTRLM_RF4CE_RIB_ATTR_ID_FAR_FIELD_CONFIGURATION  = 0xE0,
   CTRLM_RF4CE_RIB_ATTR_ID_FAR_FIELD_METRICS        = 0xE1,
   CTRLM_RF4CE_RIB_ATTR_ID_DSP_CONFIGURATION        = 0xE2,
   CTRLM_RF4CE_RIB_ATTR_ID_DSP_METRICS              = 0xE3,
   CTRLM_RF4CE_RIB_ATTR_ID_MFG_TEST                  = 0xFB,
   CTRLM_RF4CE_RIB_ATTR_ID_MEMORY_DUMP               = 0xFE,
   CTRLM_RF4CE_RIB_ATTR_ID_GENERAL_PURPOSE           = 0xFF
} ctrlm_rf4ce_rib_attr_id_t;

typedef enum {
    CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS          = 0x00,
    CTRLM_RF4CE_RESULT_VALIDATION_PENDING          = 0xC0,
    CTRLM_RF4CE_RESULT_VALIDATION_TIMEOUT          = 0xC1,
    CTRLM_RF4CE_RESULT_VALIDATION_COLLISION        = 0xC2,
    CTRLM_RF4CE_RESULT_VALIDATION_FAILURE          = 0xC3,
    CTRLM_RF4CE_RESULT_VALIDATION_ABORT            = 0xC4,
    CTRLM_RF4CE_RESULT_VALIDATION_FULL_ABORT       = 0xC5,
    CTRLM_RF4CE_RESULT_VALIDATION_FAILED           = 0xC6,
    CTRLM_RF4CE_RESULT_VALIDATION_BIND_TABLE_FULL  = 0xC7,
    CTRLM_RF4CE_RESULT_VALIDATION_IN_PROGRESS      = 0xC8
} ctrlm_rf4ce_result_validation_t;

typedef enum {
   VOICE_COMMAND_LENGTH_CONTROLLER_DEFAULT  = 0,
   VOICE_COMMAND_LENGTH_PROFILE_NEGOTIATION = 1,
   VOICE_COMMAND_LENGTH_VALUE               = 182
} voice_command_length_t;

typedef enum {
   VOICE_COMMAND_ENCRYPTION_DISABLED = 0,
   VOICE_COMMAND_ENCRYPTION_ENABLED  = 1,
   VOICE_COMMAND_ENCRYPTION_DEFAULT  = 2
} voice_command_encryption_t;

typedef enum {
   VOICE_AUDIO_PROFILE_NONE              = 0x0000,
   VOICE_AUDIO_PROFILE_ADPCM_16BIT_16KHZ = 0x0001,
   VOICE_AUDIO_PROFILE_PCM_16BIT_16KHZ   = 0x0002,
   VOICE_AUDIO_PROFILE_OPUS_16BIT_16KHZ  = 0x0004
} voice_audio_profile_t;

typedef enum {
   RIB_ENTRIES_UPDATED_FALSE = 0,
   RIB_ENTRIES_UPDATED_TRUE  = 1,
   RIB_ENTRIES_UPDATED_STOP  = 2
} rib_entries_updated_t;

typedef enum {
   DOWNLOAD_RATE_IMMEDIATE  = 0,
   DOWNLOAD_RATE_BACKGROUND = 1,
} download_rate_t;

typedef enum {
   RF4CE_DEVICE_UPDATE_RESULT_SUCCESS        = 0x00,
   RF4CE_DEVICE_UPDATE_RESULT_ERROR_REQUEST  = 0x01,
   RF4CE_DEVICE_UPDATE_RESULT_ERROR_CRC      = 0x02,
   RF4CE_DEVICE_UPDATE_RESULT_ERROR_ABORTED  = 0x03,
   RF4CE_DEVICE_UPDATE_RESULT_ERROR_REJECTED = 0x04,
   RF4CE_DEVICE_UPDATE_RESULT_ERROR_TIMEOUT  = 0x05
} ctrlm_rf4ce_device_update_result_t;

typedef enum {
   RF4CE_DEVICE_UPDATE_IMAGE_LOAD_RSP_NOW           = 0x01,
   RF4CE_DEVICE_UPDATE_IMAGE_LOAD_RSP_WHEN_INACTIVE = 0x02,
   RF4CE_DEVICE_UPDATE_IMAGE_LOAD_RSP_POLL_KEYPRESS = 0x03,
   RF4CE_DEVICE_UPDATE_IMAGE_LOAD_RSP_POLL_AGAIN    = 0x04,
   RF4CE_DEVICE_UPDATE_IMAGE_LOAD_RSP_CANCEL        = 0x05
} ctrlm_rf4ce_device_update_image_load_rsp_t;

typedef enum {
   RF4CE_DEVICE_UPDATE_AUDIO_THEME_NONE    = 0,
   RF4CE_DEVICE_UPDATE_AUDIO_THEME_DEFAULT = 1,
   RF4CE_DEVICE_UPDATE_AUDIO_THEME_2       = 2,
   RF4CE_DEVICE_UPDATE_AUDIO_THEME_3       = 3,
   RF4CE_DEVICE_UPDATE_AUDIO_THEME_4       = 4,
   RF4CE_DEVICE_UPDATE_AUDIO_THEME_5       = 5,
   RF4CE_DEVICE_UPDATE_AUDIO_THEME_6       = 6,
   RF4CE_DEVICE_UPDATE_AUDIO_THEME_7       = 7,
   RF4CE_DEVICE_UPDATE_AUDIO_THEME_8       = 8,
   RF4CE_DEVICE_UPDATE_AUDIO_THEME_9       = 9,
   RF4CE_DEVICE_UPDATE_AUDIO_THEME_INVALID = 10,
} ctrlm_rf4ce_device_update_audio_theme_t;

typedef enum {
   RF4CE_CONTROLLER_TYPE_XR2     = 0,
   RF4CE_CONTROLLER_TYPE_XR5     = 1,
   RF4CE_CONTROLLER_TYPE_XR11    = 2,
   RF4CE_CONTROLLER_TYPE_XR15    = 3,
   RF4CE_CONTROLLER_TYPE_XR15V2  = 4,
   RF4CE_CONTROLLER_TYPE_XR16    = 5,
   RF4CE_CONTROLLER_TYPE_XR18    = 6,
   RF4CE_CONTROLLER_TYPE_XR19    = 7,
   RF4CE_CONTROLLER_TYPE_UNKNOWN = 8,
   RF4CE_CONTROLLER_TYPE_INVALID = 9
} ctrlm_rf4ce_controller_type_t;

typedef enum {
   RF4CE_DEVICE_UPDATE_IMAGE_TYPE_FIRMWARE      = 0x01,
   RF4CE_DEVICE_UPDATE_IMAGE_TYPE_AUDIO_DATA_1  = 0x02,
   RF4CE_DEVICE_UPDATE_IMAGE_TYPE_DSP           = 0x11,
   RF4CE_DEVICE_UPDATE_IMAGE_TYPE_AUDIO_DATA_2  = 0x12,
   RF4CE_DEVICE_UPDATE_IMAGE_TYPE_KEYWORD_MODEL = 0x13
} ctrlm_rf4ce_device_update_image_type_t;

typedef enum {
   RF4CE_BINDING_INIT_IND_DEDICATED_KEY_COMBO_BIND = 0x00,
   RF4CE_BINDING_INIT_IND_ANY_BUTTON_BIND          = 0x01
} ctrlm_rf4ce_binding_initiation_indicator_t;

typedef enum {
   RF4CE_FIRMWARE_UPDATED_NO  = 0,
   RF4CE_FIRMWARE_UPDATED_YES = 1,
} ctrlm_rf4ce_firmware_updated_t;

typedef enum {
   RF4CE_CONTROLLER_MANUFACTURER_NOBODY          = 0,
   RF4CE_CONTROLLER_MANUFACTURER_REMOTE_SOLUTION = 1,
   RF4CE_CONTROLLER_MANUFACTURER_UEI             = 2,
   RF4CE_CONTROLLER_MANUFACTURER_OMNI            = 3
} ctrlm_rf4ce_controller_manufacturer_t;

typedef enum {
   RF4CE_PRINT_FIRMWARE_LOG_BUTTON_PRESS            = 0,
   RF4CE_PRINT_FIRMWARE_LOG_REBOOT                  = 1,
   RF4CE_PRINT_FIRMWARE_LOG_UPGRADE_CHECK           = 2,
   RF4CE_PRINT_FIRMWARE_LOG_IMAGE_DOWNLOAD_COMPLETE = 3,
   RF4CE_PRINT_FIRMWARE_LOG_IMAGE_DOWNLOAD_STARTED  = 4,
   RF4CE_PRINT_FIRMWARE_LOG_IMAGE_DOWNLOAD_PENDING  = 5,
   RF4CE_PRINT_FIRMWARE_LOG_REBOOT_SCHEDULE         = 6,
   RF4CE_PRINT_FIRMWARE_LOG_PAIRED_REMOTE           = 7
} ctrlm_rf4ce_controller_firmware_log_t;

// Polling Enums
typedef enum {
   RF4CE_POLLING_METHOD_HEARTBEAT = 0x00,
   RF4CE_POLLING_METHOD_MAC       = 0x01,
   RF4CE_POLLING_METHOD_MAX
} ctrlm_rf4ce_polling_method_t;

typedef enum {
   RF4CE_POLLING_ACTION_NONE                = 0x00,
   RF4CE_POLLING_ACTION_REBOOT              = 0x01,
   RF4CE_POLLING_ACTION_REPAIR              = 0x02,
   RF4CE_POLLING_ACTION_CONFIGURATION       = 0x03,
   RF4CE_POLLING_ACTION_OTA                 = 0x04,
   RF4CE_POLLING_ACTION_ALERT               = 0x05,
   RF4CE_POLLING_ACTION_IRDB_STATUS         = 0x06,
   RF4CE_POLLING_ACTION_POLL_CONFIGURATION  = 0x07,
   RF4CE_POLLING_ACTION_VOICE_CONFIGURATION = 0x08,
   RF4CE_POLLING_ACTION_DSP_CONFIGURATION   = 0x09,
   RF4CE_POLLING_ACTION_METRICS             = 0x0A,
   RF4CE_POLLING_ACTION_EOS                 = 0x0B,
   RF4CE_POLLING_ACTION_IRRF_STATUS         = 0x10
} ctrlm_rf4ce_polling_action_t;

typedef enum {
   RF4CE_RIB_CONFIGURATION_COMPLETE_PAIRING_SUCCESS    = 0x00,
   RF4CE_RIB_CONFIGURATION_COMPLETE_PAIRING_INCOMPLETE = 0x01,
   RF4CE_RIB_CONFIGURATION_COMPLETE_REBOOT_SUCCESS     = 0x02,
   RF4CE_RIB_CONFIGURATION_COMPLETE_REBOOT_INCOMPLETE  = 0x03
} ctrlm_rf4ce_rib_configuration_complete_status_t;
// End Polling Enums

typedef enum {
   RF4CE_VOICE_NORMAL_UTTERANCE             = 0,
   RF4CE_VOICE_SHORT_UTTERANCE              = 1
} ctrlm_rf4ce_voice_utterance_type_t;

typedef struct {
   gchar  build_id[CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_BUILD_ID];
   guint8 length; 
} version_build_id_t;

typedef struct {
   gchar  build_id[CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_DSP_BUILD_ID];
   guint8 length; 
} version_dsp_build_id_t;

typedef struct {
   // TODO
} peripheral_id_t;

typedef struct {
   // TODO
} rf_statistics_t;

typedef struct {
   time_t  update_time;
   guchar  flags;
   guchar  voltage_loaded;
   guchar  voltage_unloaded;
   guint32 codes_txd_rf;
   guint32 codes_txd_ir;
   guchar  percent;
} battery_status_t;

typedef struct {
   guint32 voice_sessions_activated;
   guint32 audio_data_tx_time;
} voice_statistics_t;

typedef struct {
   guchar flags;
   char   irdb_string_tv[7];
   char   irdb_string_avr[7];
   guchar irdb_load_status_tv;
   guchar irdb_load_status_avr;
} controller_irdb_status_t;

typedef struct {
   guchar flags;
   char   irdb_string_tv[7];
   char   irdb_string_avr[7];
} target_irdb_status_t;

// Polling Structs
typedef struct {
   guint16 trigger;
   guint8  kp_counter;
   guint32 time_interval;
   guint8  reserved;
} ctrlm_rf4ce_polling_configuration_t;

typedef struct {
   guint8  uptime_multiplier;
   guint32 hb_time_to_save;
} ctrlm_rf4ce_polling_generic_config_t;

typedef struct {
   ctrlm_rf4ce_polling_action_t action;
   char                         data[POLLING_RESPONSE_DATA_LEN];
} ctrlm_rf4ce_polling_action_msg_t;
// End Polling Structs

typedef struct {
   time_t battery_changed_timestamp;
   time_t battery_75_percent_timestamp;
   time_t battery_50_percent_timestamp;
   time_t battery_25_percent_timestamp;
   time_t battery_5_percent_timestamp;
   time_t battery_0_percent_timestamp;
   guchar battery_changed_actual_percent;
   guchar battery_75_percent_actual_percent;
   guchar battery_50_percent_actual_percent;
   guchar battery_25_percent_actual_percent;
   guchar battery_5_percent_actual_percent;
   guchar battery_0_percent_actual_percent;
} battery_voltage_milestones_t;

typedef struct {
   uint8_t flags;
   int8_t  chime_volume;
   uint8_t volume_ir_repeats;
   uint8_t reserved[17];
} far_field_configuration_t;

typedef struct {
   uint8_t  flags;
   uint32_t uptime;
   uint32_t privacy_time;
   uint8_t  reserved[11];
} far_field_metrics_t;

typedef struct {
   uint8_t  flags;
   uint8_t  vad_threshold;
   uint8_t  no_vad_threshold;
   uint8_t  vad_hang_time;
   uint8_t  initial_eos_timeout;
   uint8_t  eos_timeout;
   uint8_t  initial_speech_delay;
   uint8_t  primary_keyword_sensitivity;
   uint8_t  secondary_keyword_sensitivity;
   uint8_t  beamformer_type;
   uint8_t  noise_reduction_aggressiveness;
   int8_t   dynamic_gain_target_level;
   uint8_t  ic_config_atten_update;
   uint8_t  ic_config_detect;
   uint8_t  reserved[6];
} dsp_configuration_t;

typedef struct {
   uint16_t dropped_mic_frames;
   uint16_t dropped_speaker_frames;
   uint16_t keyword_detect_count;
   uint8_t  average_snr;
   uint8_t  average_keyword_confidence;
   uint16_t eos_initial_timeout_count;
   uint16_t eos_timeout_count;
   uint8_t  mic_failure_count;
   uint8_t  total_working_mics;
   uint8_t  speaker_failure_count;
   uint8_t  total_working_speakers;
   uint8_t  reserved[4];
} dsp_metrics_t;

typedef struct {
   ctrlm_device_update_session_id_t       session_id;
   guint16                                image_id;
   ctrlm_timestamp_t                      expiration;
   ctrlm_rf4ce_device_update_image_type_t image_type;
   gboolean                               manual_poll;
   int                                    when;
   guint32                                time;
   gboolean                               background_download;
   char                                   file_name[CTRLM_DEVICE_UPDATE_PATH_LENGTH];
} rf4ce_device_update_session_resume_state_t;

typedef struct {
   rf4ce_device_update_session_resume_state_t state;
   ctrlm_network_id_t                         network_id;
   ctrlm_controller_id_t                      controller_id;
   ctrlm_rf4ce_controller_type_t              type;
   version_hardware_t                         version_hardware;
   version_software_t                         version_bootloader;
   version_software_t                         version_software;
   guint32                                    timeout;
} rf4ce_device_update_session_resume_info_t;

class ctrlm_obj_controller_rf4ce_t : public ctrlm_obj_controller_t
{
public:
   ctrlm_obj_controller_rf4ce_t(ctrlm_controller_id_t controller_id, ctrlm_obj_network_rf4ce_t &network, unsigned long long ieee_address, ctrlm_rf4ce_result_validation_t result_validation, ctrlm_rcu_configuration_result_t result_configuration);
   ctrlm_obj_controller_rf4ce_t();
   virtual ~ctrlm_obj_controller_rf4ce_t();

   // External methods
   void db_create();
   void db_destroy();
   void db_load();
   void db_store();

   bool is_bound(void);
   void backup_pairing(void *data);
   void restore_pairing(void);
   unsigned long long ieee_address_get(void);
   void user_string_set(guchar *user_string);
   void controller_chipset_from_product_name(void);
   void autobind_in_progress_set(bool in_progress);
   bool autobind_in_progress_get(void);
   void binding_button_in_progress_set(bool in_progress);
   bool binding_button_in_progress_get(void);
   void screen_bind_in_progress_set(bool in_progress);
   bool screen_bind_in_progress_get(void);
   void stats_update(void);
   void time_last_key_get(time_t *time);
   void validation_result_set(ctrlm_rcu_binding_type_t binding_type, ctrlm_rcu_validation_type_t validation_type, ctrlm_rf4ce_result_validation_t result, time_t time_binding = 0, time_t time_last_key = 0);
   ctrlm_rf4ce_result_validation_t  validation_result_get(void);
   ctrlm_rcu_configuration_result_t configuration_result_get(void);
   ctrlm_rf4ce_controller_type_t    controller_type_get(void);
   ctrlm_rcu_binding_type_t         binding_type_get(void);
   void rf4ce_rib_get_target(ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guchar length, guchar *data_len, guchar *data);
   void rf4ce_rib_get_controller(ctrlm_timestamp_t timestamp, ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guchar length);
   void rf4ce_rib_set_target(ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guchar length, guchar *data);
   void rf4ce_rib_set_controller(ctrlm_timestamp_t timestamp, ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guchar length, guchar *data);
   void rf4ce_controller_status(ctrlm_controller_status_t *status);
   const char *product_name_get(void) const;
   const battery_status_t& battery_status_get() const;
   void manual_poll_firmware(void);
   void manual_poll_audio_data(void);
   void audio_theme_set(ctrlm_rf4ce_device_update_audio_theme_t audio_theme);

   void device_update_image_check_request(ctrlm_timestamp_t timestamp, ctrlm_rf4ce_device_update_image_type_t image_type);
   void device_update_image_data_request(ctrlm_timestamp_t timestamp, guint16 image_id, guint32 offset, guchar length);
   void device_update_image_load_request(ctrlm_timestamp_t timestamp, guint16 image_id);
   void device_update_image_download_complete(ctrlm_timestamp_t timestamp, guint16 image_id, ctrlm_rf4ce_device_update_result_t result);

   void process_event_key(ctrlm_key_status_t key_status, ctrlm_key_code_t key_code);

   void update_voice_metrics(ctrlm_rf4ce_voice_utterance_type_t voice_utterance_type, guint32 voice_packets_sent, guint32 voice_packets_lost);
   void set_firmware_updated();
   void time_last_checkin_for_device_update_set();
   void time_last_checkin_for_device_update_get(time_t *time);
   bool handle_day_change();
   ctrlm_rcu_battery_event_t get_last_battery_event();

   bool import_check_validation();
   void time_last_key_update(void);

   void push_ir_codes(void);

   // These functions are HACKS for XR15-704
#ifdef XR15_704
   void set_reset();
#endif
   // These functions are HACKS for XR15-704

   version_software_t     version_software_get();
   version_software_t     version_audio_data_get();
   version_software_t     version_dsp_get();
   version_software_t     version_keyword_model_get();
   version_software_t     version_arm_get();
   version_hardware_t     version_hardware_get();
   int                    version_compare(version_software_t first, version_software_t second);
   int                    version_compare(version_hardware_t first, version_hardware_t second);
   gboolean               is_firmeware_updated();
   void                   check_for_update_file_delete(guint16 image_id);
   void                   log_binding_for_telemetry();
   void                   log_unbinding_for_telemetry();
   guchar                 battery_level_percent(unsigned char voltage_loaded);
   gboolean               has_battery();
   gboolean               has_dsp();

   controller_irdb_status_t controller_irdb_status_get(void);
   time_t                   time_binding_get(void);
   guint8                 polling_methods_get() const;
   bool                   is_fmr_supported() const;

   void print_remote_firmware_debug_info(ctrlm_rf4ce_controller_firmware_log_t, string message = "");

   // Polling Functions
   void                   polling_action_push(ctrlm_rf4ce_polling_action_msg_t *action);
   void                   update_polling_configurations(bool add_polling_action = true);
   void                   rf4ce_heartbeat(ctrlm_timestamp_t timestamp, guint16 trigger);
   void                   rib_configuration_complete(ctrlm_timestamp_t timestamp, ctrlm_rf4ce_rib_configuration_complete_status_t status);
   void                   time_last_heartbeat_update(void);
   void                   time_last_heartbeat_get(time_t *time);
   // End Polling Functions
   
   void binding_security_type_set(ctrlm_rcu_binding_security_type_t type);
   ctrlm_rcu_binding_security_type_t binding_security_type_get();
   gboolean version_hardware_valid(guchar *data);
   void controller_fix_hardware_version_by_mac_address(guchar *data);
#ifdef ASB
   void asb_key_derivation_method_set(asb_key_derivation_method_t method);
   asb_key_derivation_method_t asb_key_derivation_method_get();
   void asb_key_derivation_start(ctrlm_network_id_t network_id);
   void asb_key_derivation_perform();
#endif
   ctrlm_timestamp_t last_mac_poll_checkin_time_get();

   void handle_controller_metrics(void *data = NULL, int size = 0);
   void handle_voice_configuration();

   bool device_update_session_resume_support(void);
   bool device_update_session_resume_available(void);
   void device_update_session_resume_store(rf4ce_device_update_session_resume_state_t *info);
   bool device_update_session_resume_load(rf4ce_device_update_session_resume_info_t *info);
   void device_update_session_resume_unload(void);
   void device_update_session_resume_remove(void);
 
   void controller_destroy();

private:
   ctrlm_obj_network_rf4ce_t *             obj_network_rf4ce_;
   bool                                    loading_db_;
   bool                                    stored_in_db_;
   void *                                  pairing_data_;
   unsigned long long                      ieee_address_;
   unsigned short                          short_address_;
   ctrlm_rf4ce_result_validation_t         validation_result_;
   ctrlm_rcu_configuration_result_t        configuration_result_;
   peripheral_id_t                         peripheral_id_;
   rf_statistics_t                         rf_statistics_;
   version_software_t                      version_software_;
   version_software_t                      version_dsp_;
   version_software_t                      version_keyword_model_;
   version_software_t                      version_arm_;
   version_software_t                      version_irdb_;
   version_hardware_t                      version_hardware_;
   version_build_id_t                      version_build_id_;
   version_dsp_build_id_t                  version_dsp_build_id_;
   battery_status_t                        battery_status_;
   unsigned char                           voice_command_status_[CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_STATUS];
   voice_command_length_t                  voice_command_length_;
   guint32                                 voice_cmd_count_today_;
   guint32                                 voice_cmd_count_yesterday_;
   guint32                                 voice_cmd_short_today_;
   guint32                                 voice_cmd_short_yesterday_;
   guint32                                 today_;
   guint32                                 voice_packets_sent_today_;
   guint32                                 voice_packets_sent_yesterday_;
   guint32                                 voice_packets_lost_today_;
   guint32                                 voice_packets_lost_yesterday_;
   guint32                                 utterances_exceeding_packet_loss_threshold_today_;
   guint32                                 utterances_exceeding_packet_loss_threshold_yesterday_;
   ctrlm_rf4ce_firmware_updated_t          firmware_updated_;
   guint16                                 audio_profiles_ctrl_;
   voice_statistics_t                      voice_statistics_;
   rib_entries_updated_t                   rib_entries_updated_;
   version_software_t                      version_bootloader_;
   version_software_t                      version_golden_;
   version_software_t                      version_audio_data_;
   char                                    product_name_[CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME + 1];
   char                                    manufacturer_[CTRLM_RCU_MAX_MANUFACTURER_LENGTH];
   char                                    chipset_[CTRLM_RCU_MAX_CHIPSET_LENGTH];
   ctrlm_rf4ce_controller_type_t           controller_type_;
   guchar                                  download_rate_;
   controller_irdb_status_t                controller_irdb_status_;
   gboolean                                controller_irdb_status_is_new_;
   controller_reboot_reason_t              reboot_reason_;
   guchar                                  reboot_voltage_level_;
   guint32                                 reboot_assert_number_;
   time_t                                  reboot_time_;
   guint16                                 memory_available_;
   guint16                                 memory_largest_;
   guchar                                  ir_rf_database_status_;
   gboolean                                download_in_progress_;
   guint16                                 download_image_id_;
   bool                                    autobind_in_progress_;
   bool                                    binding_button_in_progress_;
   bool                                    screen_bind_in_progress_;
   ctrlm_rcu_binding_type_t                binding_type_;
   ctrlm_rcu_validation_type_t             validation_type_;
   ctrlm_rcu_binding_security_type_t       binding_security_type_;
   ctrlm_rcu_binding_type_t                backup_binding_type_;
   ctrlm_rcu_validation_type_t             backup_validation_type_;
   ctrlm_rcu_binding_security_type_t       backup_binding_security_type_;
   time_t                                  time_binding_;
   time_t                                  time_last_key_;
   time_t                                  time_next_flush_;
   time_t                                  time_last_checkin_for_device_update_;
   ctrlm_key_status_t                      last_key_status_;
   ctrlm_key_code_t                        last_key_code_;
   gboolean                                manual_poll_firmware_;
   gboolean                                manual_poll_audio_data_;
   ctrlm_rf4ce_device_update_audio_theme_t audio_theme_;
   guchar *                                crash_dump_buf_;           // crash data buffer
   guint16                                 crash_dump_size_;          // data received size
   guint16                                 crash_dump_expected_size_; // data allocated size
   bool                                    print_firmware_on_button_press;
   gboolean                                has_battery_;
   gboolean                                has_dsp_;
   battery_voltage_milestones_t            battery_milestones_;
   gboolean                                configuration_complete_failure_;

   // Far Field
   uint8_t                                 privacy_;
   far_field_metrics_t                     ff_metrics_;
   dsp_metrics_t                           dsp_metrics_;
   time_t                                  time_metrics_;
   uptime_privacy_info_t                   uptime_privacy_info_;
   guint32                                 time_since_last_saved_;

   // Polling variables
   GAsyncQueue                              *polling_actions_;
   guint8                                    polling_methods_;
   ctrlm_rf4ce_polling_configuration_t       polling_configurations_[RF4CE_POLLING_METHOD_MAX];
   time_t                                    time_last_heartbeat_;
   // End Polling variables
   ctrlm_rf4ce_rib_configuration_complete_status_t rib_configuration_complete_status_;

#ifdef ASB
   // ASB variables
   asb_key_derivation_method_t             asb_key_derivation_method_used_;
   ctrlm_timestamp_t                       asb_key_derivation_ts_start_;
   guint                                   asb_tag_;
   // End ASB variables
#endif

   // HACK for XR15-704
#ifdef XR15_704
   gboolean                                needs_reset_;
   gboolean                                did_reset_;
#endif
   // HACK for XR15-704
   bool                                    fmr_supported_;

   ctrlm_timestamp_t                       checkin_time_;    ///< OUT - Timestamp indicating the most recent poll indication of the controller

   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint32                                 short_rf_retry_period_;
   guint16                                 utterance_duration_max_;
   guchar                                  voice_data_retry_max_;
   guchar                                  voice_csma_backoff_max_;
   guchar                                  voice_data_backoff_exp_min_;
   guint16                                 rib_update_check_interval_;
   guint16                                 auto_check_validation_period_;
   guint16                                 link_lost_wait_time_;
   guint16                                 update_polling_period_;
   guint16                                 data_request_wait_time_;
   voice_command_encryption_t              voice_command_encryption_;
   #else
   guint32                                 short_rf_retry_period_get(void);
   guint16                                 utterance_duration_max_get(void);
   guchar                                  voice_data_retry_max_get(void);
   guchar                                  voice_csma_backoff_max_get(void);
   guchar                                  voice_data_backoff_exp_min_get(void);
   guint16                                 rib_update_check_interval_get(void);
   guint16                                 auto_check_validation_period_get(void);
   guint16                                 link_lost_wait_time_get(void);
   guint16                                 update_polling_period_get(void);
   guint16                                 data_request_wait_time_get(void);
   voice_command_encryption_t              voice_command_encryption_get(void);
   #endif
   guint16                                 audio_profiles_targ_get(void);

   void req_data(ctrlm_rf4ce_profile_id_t profile_id, ctrlm_timestamp_t tx_window_start, unsigned char length, guchar *data, ctrlm_hal_rf4ce_data_read_t cb_data_read, void *cb_data_param, bool tx_indirect=false, bool single_channel=false);
   void rf4ce_rib_get(gboolean target, ctrlm_timestamp_t timestamp, ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guchar length, guchar *data_len, guchar *data);
   void rf4ce_rib_set(gboolean target, ctrlm_timestamp_t timestamp, ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guint8 length, guchar *data);

   ctrlm_hal_result_t network_property_get(ctrlm_hal_network_property_t property, void **value);
   ctrlm_hal_result_t network_property_set(ctrlm_hal_network_property_t property, void *value);

   guchar battery_level_percent(void);
   void property_write_battery_milestones(guchar flags, guchar percent, time_t timestamp);
   gboolean send_battery_milestone_event(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_rcu_battery_event_t battery_event, guchar percent);
   gboolean send_remote_reboot_event(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, guchar voltage, controller_reboot_reason_t reason, guint32 assert_number);

   guchar property_read_peripheral_id(guchar *data, guchar length);
   guchar property_read_rf_statistics(guchar *data, guchar length);
   guchar property_read_version_irdb(guchar *data, guchar length);
   guchar property_read_version_hardware(guchar *data, guchar length);
   guchar property_read_version_software(guchar *data, guchar length);
   guchar property_read_version_dsp(guchar *data, guchar length);
   guchar property_read_version_keyword_model(guchar *data, guchar length);
   guchar property_read_version_arm(guchar *data, guchar length);
   guchar property_read_version_build_id(guchar *data, guchar length);
   guchar property_read_version_dsp_build_id(guchar *data, guchar length);
   guchar property_read_battery_status(guchar *data, guchar length);
   guchar property_read_short_rf_retry_period(guchar *data, guchar length);
   guchar property_read_voice_command_status(guchar *data, guchar length);
   guchar property_read_voice_command_length(guchar *data, guchar length);
   guchar property_read_maximum_utterance_length(guchar *data, guchar length);
   guchar property_read_voice_command_encryption(guchar *data, guchar length);
   guchar property_read_max_voice_data_retry(guchar *data, guchar length);
   guchar property_read_max_voice_csma_backoff(guchar *data, guchar length);
   guchar property_read_min_voice_data_backoff(guchar *data, guchar length);
   guchar property_read_voice_ctrl_audio_profiles(guchar *data, guchar length);
   guchar property_read_voice_targ_audio_profiles(guchar *data, guchar length);
   guchar property_read_voice_statistics(guchar *data, guchar length);
   guchar property_read_rib_entries_updated(guchar *data, guchar length);
   guchar property_read_rib_update_check_interval(guchar *data, guchar length);
   guchar property_read_voice_session_statistics(guchar *data, guchar length);
   guchar property_read_opus_encoding_params(guchar *data, guchar length);
   guchar property_read_voice_session_qos(guchar *data, guchar length);
   guchar property_read_update_version_bootloader(guchar *data, guchar length);
   guchar property_read_update_version_golden(guchar *data, guchar length);
   guchar property_read_update_version_audio_data(guchar *data, guchar length);
   guchar property_read_product_name(guchar *data, guchar length);
   guchar property_read_download_rate(guchar *data, guchar length);
   guchar property_read_update_polling_period(guchar *data, guchar length);
   guchar property_read_data_request_wait_time(guchar *data, guchar length);
   guchar property_read_ir_rf_database_status(guchar *data, guchar length, bool target);
   guchar property_read_ir_rf_database(guchar index, guchar *data, guchar length);
   guchar property_read_validation_configuration(guchar *data, guchar length);
   guchar property_read_controller_irdb_status(guchar *data, guchar length);
   guchar property_read_target_irdb_status(guchar *data, guchar length);
   guchar property_read_irdb_entry_id_name_tv(guchar *data, guchar length);
   guchar property_read_irdb_entry_id_name_avr(guchar *data, guchar length);
   guchar property_read_voice_metrics(guchar *data, guchar length);
   guchar property_read_firmware_updated(guchar *data, guchar length);
   guchar property_read_reboot_diagnostics(guchar *data, guchar length);
   guchar property_read_memory_statistics(guchar *data, guchar length);
   guchar property_read_time_last_checkin_for_device_update(guchar *data, guchar length);
   guchar property_read_receiver_id(guchar *data, guchar length);
   guchar property_read_device_id(guchar *data, guchar length);
   guchar property_read_mfg_test(guchar *data, guchar length);
   guchar property_read_polling_methods(guchar *data, guchar length);
   guchar property_read_polling_configuration_heartbeat(guchar *data, guchar length);
   guchar property_read_polling_configuration_mac(guchar *data, guchar length);
   guchar property_read_privacy(guchar *data, guchar length);
   guchar property_read_controller_capabilities(guchar *data, guchar length);
   guchar property_read_far_field_configuration(guchar *data, guchar length);
   guchar property_read_far_field_metrics(guchar *data, guchar length);
   guchar property_read_dsp_configuration(guchar *data, guchar length);
   guchar property_read_dsp_metrics(guchar *data, guchar length);
   guchar property_read_uptime_privacy_info(guchar *data, guchar length);

   void property_write_peripheral_id();
   void property_write_rf_statistics();
   void property_write_version_irdb(guchar major, guchar minor, guchar revision, guchar patch);
   void property_write_version_hardware(guchar manufacturer, guchar model, guchar hw_revision, guint16 lot_code);
   void property_write_version_software(guchar major, guchar minor, guchar revision, guchar patch);
   void property_write_battery_status(guchar flags, guchar voltage_loaded, guint32 codes_txd_rf, guint32 codes_txd_ir, guchar voltage_unloaded);
   void property_write_short_rf_retry_period(guint32 short_rf_retry_period);
   #ifdef USE_VOICE_SDK
   void property_write_voice_command_status(ctrlm_voice_command_status_t voice_command_status, guint8 flags, guint8 value);
   #else
   void property_write_voice_command_status(voice_command_status_t voice_command_status, guint8 flags, guint8 value);
   #endif
   void property_write_voice_command_length(guchar voice_command_length);
   void property_write_maximum_utterance_length(guint16 maximum_utterance_length);
   void property_write_voice_command_encryption(voice_command_encryption_t voice_command_encryption);
   void property_write_max_voice_data_retry(guchar max_voice_data_retry_attempts);
   void property_write_max_voice_csma_backoff(guchar max_voice_csma_backoffs);
   void property_write_min_voice_data_backoff(guchar min_voice_data_backoff_exp);
   void property_write_voice_ctrl_audio_profiles(guint16 audio_profiles_ctrl);
   void property_write_voice_statistics(guint32 voice_sessions_activated, guint32 audio_data_tx_time);
   void property_write_rib_update_check_interval(guint16 rib_update_check_interval);
   //void property_write_voice_session_statistics();
   void property_write_update_version_bootloader(guchar major, guchar minor, guchar revision, guchar patch);
   void property_write_update_version_golden(guchar major, guchar minor, guchar revision, guchar patch);
   void property_write_update_version_audio_data(guchar major, guchar minor, guchar revision, guchar patch);
   void property_write_product_name(const char *name);
   void property_write_download_rate(download_rate_t download_rate);
   void property_write_update_polling_period(guint16 update_polling_period);
   void property_write_data_request_wait_time(guint16 data_request_wait_time);
   void property_write_ir_rf_database_status(guchar status);
   //void property_write_ir_rf_database();
   void property_write_validation_configuration(guint16 auto_check_validation_period, guint16 link_lost_wait_time);
   //void property_read_controller_irdb_status();
   void property_write_voice_metrics(void);
   void property_write_firmware_updated(void);
   void property_write_reboot_diagnostics(void);
   void property_write_memory_statistics(void);
   void property_write_time_last_checkin_for_device_update(void);

   guchar property_write_peripheral_id(guchar *data, guchar length);
   guchar property_write_rf_statistics(guchar *data, guchar length);
   guchar property_write_version_irdb(guchar *data, guchar length);
   guchar property_write_version_hardware(guchar *data, guchar length);
   guchar property_write_version_software(guchar *data, guchar length);
   guchar property_write_version_dsp(guchar *data, guchar length);
   guchar property_write_version_keyword_model(guchar *data, guchar length);
   guchar property_write_version_arm(guchar *data, guchar length);
   guchar property_write_version_build_id(guchar *data, guchar length);
   guchar property_write_version_dsp_build_id(guchar *data, guchar length);
   guchar property_write_battery_status(guchar *data, guchar length);
   guchar property_write_short_rf_retry_period(guchar *data, guchar length);
   guchar property_write_voice_command_status(guchar *data, guchar length);
   guchar property_write_voice_command_length(guchar *data, guchar length);
   guchar property_write_maximum_utterance_length(guchar *data, guchar length);
   guchar property_write_voice_command_encryption(guchar *data, guchar length);
   guchar property_write_max_voice_data_retry(guchar *data, guchar length);
   guchar property_write_max_voice_csma_backoff(guchar *data, guchar length);
   guchar property_write_min_voice_data_backoff(guchar *data, guchar length);
   guchar property_write_voice_ctrl_audio_profiles(guchar *data, guchar length);
   guchar property_write_voice_statistics(guchar *data, guchar length);
   guchar property_write_rib_update_check_interval(guchar *data, guchar length);
   guchar property_write_voice_session_statistics(guchar *data, guchar length);
   guchar property_write_opus_encoding_params(guchar *data, guchar length);
   guchar property_write_voice_session_qos(guchar *data, guchar length);
   guchar property_write_update_version_bootloader(guchar *data, guchar length);
   guchar property_write_update_version_golden(guchar *data, guchar length);
   guchar property_write_update_version_audio_data(guchar *data, guchar length);
   guchar property_write_product_name(guchar *data, guchar length);
   guchar property_write_download_rate(guchar *data, guchar length);
   guchar property_write_update_polling_period(guchar *data, guchar length);
   guchar property_write_data_request_wait_time(guchar *data, guchar length);
   guchar property_write_ir_rf_database(guchar index, guchar *data, guchar length);
   guchar property_write_validation_configuration(guchar *data, guchar length);
   guchar property_write_controller_irdb_status(guchar *data, guchar length);
   guchar property_write_target_irdb_status(guchar *data, guchar length);
   guchar property_write_irdb_entry_id_name_tv(guchar *data, guchar length);
   guchar property_write_irdb_entry_id_name_avr(guchar *data, guchar length);
   guchar property_write_voice_metrics(guchar *data, guchar length);
   guchar property_write_firmware_updated(guchar *data, guchar length);
   guchar property_write_reboot_diagnostics(guchar *data, guchar length);
   guchar property_write_memory_statistics(guchar *data, guchar length);
   guchar property_write_time_last_checkin_for_device_update(guchar *data, guchar length);

   guchar property_write_memory_dump(guchar index, guchar *data, guchar length);
   guchar property_write_reboot_stats(guchar *data, guchar length);
   guchar property_write_memory_stats(guchar *data, guchar length);
   guchar property_write_receiver_id(guchar *data, guchar length);
   guchar property_write_device_id(guchar *data, guchar length);
   guchar property_write_mfg_test(guchar *data, guchar length);
   guchar property_write_polling_methods(guchar *data, guchar length);
   guchar property_write_polling_configuration_heartbeat(guchar *data, guchar length);
   guchar property_write_polling_configuration_mac(guchar *data, guchar length);
   guchar property_write_battery_milestones(guchar *data, guchar length);
   guchar property_write_privacy(guchar *data, guchar length);
   guchar property_write_controller_capabilities(const guchar *data, guchar length);
   guchar property_write_far_field_configuration(guchar *data, guchar length);
   guchar property_write_far_field_metrics(guchar *data, guchar length);
   guchar property_write_dsp_configuration(guchar *data, guchar length);
   guchar property_write_dsp_metrics(guchar *data, guchar length);
   guchar property_write_uptime_privacy_info(guchar *data, guchar length);

   bool polling_action_pop(ctrlm_rf4ce_polling_action_msg_t **action);
   bool is_ir_code_to_be_cleared(guchar *data, guchar length);

};

#endif
