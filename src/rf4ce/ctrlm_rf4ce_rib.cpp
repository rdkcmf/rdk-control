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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <string>
#include "../ctrlm.h"
#include "../ctrlm_rcu.h"
#include "../ctrlm_validation.h"
#include "ctrlm_rf4ce_network.h"

typedef enum {
   CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS               = 0x00, // Taken from GP RF4CE definitions
   CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER     = 0xE8,
   CTRLM_RF4CE_RIB_RSP_STATUS_UNSUPPORTED_ATTRIBUTE = 0xF4,
   CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX         = 0xF9
} ctrlm_rf4ce_rib_rsp_status_t;

void ctrlm_obj_controller_rf4ce_t::rf4ce_rib_get_target(ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guchar length, guchar *data_len, guchar *data) {
   ctrlm_timestamp_t timestamp;
   errno_t safec_rc = memset_s(&timestamp, sizeof(timestamp), 0, sizeof(timestamp));
   ERR_CHK(safec_rc);
   rf4ce_rib_get(true, timestamp, identifier, index, length, data_len, data);
}

void ctrlm_obj_controller_rf4ce_t::rf4ce_rib_get_controller(ctrlm_timestamp_t timestamp, ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guchar length) {
   rf4ce_rib_get(false, timestamp, identifier, index, length, NULL, NULL);
}

void ctrlm_obj_controller_rf4ce_t::rf4ce_rib_get(gboolean target, ctrlm_timestamp_t timestamp, ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guchar length, guchar *data_len, guchar *data) {
   ctrlm_rf4ce_rib_rsp_status_t status = CTRLM_RF4CE_RIB_RSP_STATUS_UNSUPPORTED_ATTRIBUTE;
   guchar value_length = 0;
   guchar response[5 + CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE];
   guchar *data_buf;
   if(target) {
      data_buf = data;
   } else {
      data_buf = &response[5];
   }

   switch(identifier) {
      case CTRLM_RF4CE_RIB_ATTR_ID_PERIPHERAL_ID: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_PERIPHERAL_ID) {
            LOG_ERROR("%s: PERIPHERAL ID - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: PERIPHERAL ID - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            LOG_INFO("%s: PERIPHERAL ID\n", __FUNCTION__);
            // add the payload to the response
            value_length = property_read_peripheral_id(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_PERIPHERAL_ID);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_RF_STATISTICS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_RF_STATISTICS) {
            LOG_ERROR("%s: RF STATISTICS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: RF STATISTICS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            LOG_INFO("%s: RF STATISTICS\n", __FUNCTION__);
            // add the payload to the response
            value_length = property_read_rf_statistics(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_RF_STATISTICS);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VERSIONING: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING && (index != CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_BUILD_ID && index != CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_DSP_BUILD_ID)) {
            LOG_ERROR("%s: VERSIONING - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_DSP_BUILD_ID) {
            LOG_ERROR("%s: VERSIONING - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_ARM) {
            value_length = property_read_version_arm(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_KEYWORD_MODEL) {
            value_length = property_read_version_keyword_model(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_DSP) {
            value_length = property_read_version_dsp(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_BUILD_ID) {
            value_length = property_read_version_build_id(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_BUILD_ID);
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_IRDB) {
            // add the payload to the response
            value_length = property_read_version_irdb(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_HARDWARE) {
            // add the payload to the response
            value_length = property_read_version_hardware(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_DSP_BUILD_ID) {
            value_length = property_read_version_dsp_build_id(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_DSP_BUILD_ID);
            LOG_INFO("%s: DSP BUILD ID: Length (%u)\n", __FUNCTION__, length);
         } else {
            // add the payload to the response
            value_length = property_read_version_software(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_BATTERY_STATUS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS) {
            LOG_ERROR("%s: BATTERY STATUS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: BATTERY STATUS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_battery_status(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_SHORT_RF_RETRY_PERIOD: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_SHORT_RF_RETRY_PERIOD) {
            LOG_ERROR("%s: SHORT RF RETRY PERIOD - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: SHORT RF RETRY PERIOD - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_short_rf_retry_period(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_SHORT_RF_RETRY_PERIOD);

            // This rib entry is the last entry read by the remote after binding is completed
            if((controller_type_ == RF4CE_CONTROLLER_TYPE_XR2 || controller_type_ == RF4CE_CONTROLLER_TYPE_XR5) &&
               validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS && configuration_result_ == CTRLM_RCU_CONFIGURATION_RESULT_PENDING) {
               LOG_INFO("%s: (%u, %u) Configuration Complete\n", __FUNCTION__, network_id_get(), controller_id_get());
               configuration_result_ = CTRLM_RCU_CONFIGURATION_RESULT_SUCCESS;
               // Inform control manager that the configuration has completed
               ctrlm_inform_configuration_complete(network_id_get(), controller_id_get(), CTRLM_RCU_CONFIGURATION_RESULT_SUCCESS);
            }
         }
         
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_COMMAND_STATUS: {
         if(length == 0 || length > CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_STATUS) {
            LOG_ERROR("%s: VOICE COMMAND STATUS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE COMMAND STATUS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_voice_command_status(data_buf, length);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_COMMAND_LENGTH: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_LENGTH) {
            LOG_ERROR("%s: VOICE COMMAND LENGTH - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE COMMAND LENGTH - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_voice_command_length(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_LENGTH);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MAXIMUM_UTTERANCE_LENGTH: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAXIMUM_UTTERANCE_LENGTH) {
            LOG_ERROR("%s: MAXIMUM UTTERANCE LENGTH - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: MAXIMUM UTTERANCE LENGTH - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_maximum_utterance_length(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_MAXIMUM_UTTERANCE_LENGTH);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_COMMAND_ENCRYPTION: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_ENCRYPTION) {
            LOG_ERROR("%s: VOICE COMMAND ENCRYPTION - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE COMMAND ENCRYPTION - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_voice_command_encryption(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_ENCRYPTION);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MAX_VOICE_DATA_RETRY: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_DATA_RETRY) {
            LOG_ERROR("%s: MAX VOICE DATA RETRY - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: MAX VOICE DATA RETRY - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_max_voice_data_retry(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_DATA_RETRY);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MAX_VOICE_CSMA_BACKOFF: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_CSMA_BACKOFF) {
            LOG_ERROR("%s: MAX VOICE CSMA BACKOFF - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: MAX VOICE CSMA BACKOFF - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_max_voice_csma_backoff(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_CSMA_BACKOFF);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MIN_VOICE_DATA_BACKOFF: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MIN_VOICE_DATA_BACKOFF) {
            LOG_ERROR("%s: MIN VOICE DATA BACKOFF - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: MIN VOICE DATA BACKOFF - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_min_voice_data_backoff(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_MIN_VOICE_DATA_BACKOFF);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_CTRL_AUDIO_PROFILES: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES) {
            LOG_ERROR("%s: VOICE CTRL AUDIO PROFILES - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE CTRL AUDIO PROFILES - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_voice_ctrl_audio_profiles(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_TARG_AUDIO_PROFILES: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_TARG_AUDIO_PROFILES) {
            LOG_ERROR("%s: VOICE TARG AUDIO PROFILES - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE TARG AUDIO PROFILES - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_voice_targ_audio_profiles(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_TARG_AUDIO_PROFILES);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_STATISTICS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS) {
            LOG_ERROR("%s: VOICE STATISTICS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE STATISTICS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_voice_statistics(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_RIB_ENTRIES_UPDATED: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_RIB_ENTRIES_UPDATED) {
            LOG_ERROR("%s: RIB ENTRIES UPDATED - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: RIB ENTRIES UPDATED - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_rib_entries_updated(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_RIB_ENTRIES_UPDATED);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_RIB_UPDATE_CHECK_INTERVAL: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_RIB_UPDATE_CHECK_INTERVAL) {
            LOG_ERROR("%s: RIB UPDATE CHECK INTERVAL - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: RIB UPDATE CHECK INTERVAL - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_rib_update_check_interval(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_RIB_UPDATE_CHECK_INTERVAL);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_SESSION_STATISTICS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_STATISTICS) {
            LOG_ERROR("%s: VOICE SESSION STATISTICS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE SESSION STATISTICS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_voice_session_statistics(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_STATISTICS);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_OPUS_ENCODING_PARAMS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS) {
            LOG_ERROR("%s: OPUS ENCODING PARAMS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: OPUS ENCODING PARAMS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_opus_encoding_params(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_SESSION_QOS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_QOS) {
            LOG_ERROR("%s: VOICE SESSION QOS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE SESSION QOS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_voice_session_qos(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_QOS);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_UPDATE_VERSIONING: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING) {
            LOG_ERROR("%s: UPDATE VERSIONING - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index != 0x01 && index != 0x02 && index != 0x10) {
            LOG_ERROR("%s: UPDATE VERSIONING - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_UPDATE_VERSIONING_BOOTLOADER) {
            // add the payload to the response
            value_length = property_read_update_version_bootloader(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_UPDATE_VERSIONING_GOLDEN) {
            // add the payload to the response
            value_length = property_read_update_version_golden(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
         } else {
            // add the payload to the response
            value_length = property_read_update_version_audio_data(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_PRODUCT_NAME: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME) {
            LOG_ERROR("%s: PRODUCT NAME - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: PRODUCT NAME - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_product_name(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_DOWNLOAD_RATE: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DOWNLOAD_RATE) {
            LOG_ERROR("%s: DOWNLOAD RATE - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: DOWNLOAD RATE - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_download_rate(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_DOWNLOAD_RATE);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_UPDATE_POLLING_PERIOD: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_POLLING_PERIOD) {
            LOG_ERROR("%s: UPDATE POLLING PERIOD - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: UPDATE POLLING PERIOD - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_update_polling_period(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_POLLING_PERIOD);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_DATA_REQUEST_WAIT_TIME: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DATA_REQUEST_WAIT_TIME) {
            LOG_ERROR("%s: DATA REQUEST WAIT TIME - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: DATA REQUEST WAIT TIME - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_data_request_wait_time(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_DATA_REQUEST_WAIT_TIME);
            if(!target) {
               // This rib entry is the last entry read by the remote after binding is completed
               if((controller_type_ == RF4CE_CONTROLLER_TYPE_XR11 || controller_type_ == RF4CE_CONTROLLER_TYPE_XR15 || controller_type_ == RF4CE_CONTROLLER_TYPE_XR15V2 ||
                   controller_type_ == RF4CE_CONTROLLER_TYPE_XR16 || controller_type_ == RF4CE_CONTROLLER_TYPE_XR18 || controller_type_ == RF4CE_CONTROLLER_TYPE_XRA) &&
                  validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS && configuration_result_ == CTRLM_RCU_CONFIGURATION_RESULT_PENDING) {
                  LOG_INFO("%s: (%u, %u) Configuration Complete\n", __FUNCTION__, network_id_get(), controller_id_get());
                  configuration_result_ = CTRLM_RCU_CONFIGURATION_RESULT_SUCCESS;
                  // Inform control manager that the configuration has completed
                  ctrlm_inform_configuration_complete(network_id_get(), controller_id_get(), CTRLM_RCU_CONFIGURATION_RESULT_SUCCESS);
               }
            }
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_IR_RF_DATABASE_STATUS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS) {
            LOG_ERROR("%s: IR RF DATABASE STATUS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: IR RF DATABASE STATUS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_ir_rf_database_status(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS, target);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_IR_RF_DATABASE: {
         if(target && length != CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE) {
            LOG_ERROR("%s: IR RF DATABASE - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else {
            // add the payload to the response
            value_length = property_read_ir_rf_database(index, data_buf, CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE);

            if(value_length == 0) {
               LOG_ERROR("%s: IR RF DATABASE - Invalid Index (%u)\n", __FUNCTION__, index);
               status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
            }
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VALIDATION_CONFIGURATION: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VALIDATION_CONFIGURATION) {
            LOG_ERROR("%s: VALIDATION CONFIGURATION - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VALIDATION CONFIGURATION - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_validation_configuration(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_VALIDATION_CONFIGURATION);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_CONTROLLER_IRDB_STATUS: {
         if((length != CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS) && (length != CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS_MINUS_LOAD_STATUS_BYTES)) {
            LOG_ERROR("%s: CONTROLLER IRDB STATUS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: CONTROLLER IRDB STATUS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_controller_irdb_status(data_buf, length);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_TARGET_IRDB_STATUS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS) {
            LOG_ERROR("%s: TARGET IRDB STATUS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: TARGET IRDB STATUS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_target_irdb_status(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MEMORY_DUMP: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_DUMP) {
            LOG_ERROR("%s: MEMORY DUMP - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else {
            // add the payload to the response
            value_length = 0;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_TARGET_ID_DATA: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_ID_DATA) {
            LOG_ERROR("%s: TARGET ID DATA - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > CTRLM_RF4CE_RIB_ATTR_INDEX_TARGET_ID_DATA_DEVICE_ID) {
            LOG_ERROR("%s: TARGET ID DATA - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_TARGET_ID_DATA_DEVICE_ID) {
            value_length = property_read_device_id(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_ID_DATA);
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_TARGET_ID_DATA_RECEIVER_ID) {
            value_length = property_read_receiver_id(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_ID_DATA);
         } else {
            LOG_WARN("%s: Account ID not implemented yet\n", __FUNCTION__);
            value_length = 0;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_GENERAL_PURPOSE: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_GENERAL_PURPOSE) {
            LOG_ERROR("%s: GENERAL PURPOSE - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index == 0x00) { // Reset type
            value_length = property_read_reboot_diagnostics(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_DIAGNOSTICS);
         } else if(index == 0x01) { // Memory Stats
            value_length = property_read_memory_statistics(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATISTICS);
         } else {
            LOG_ERROR("%s: GENERAL PURPOSE - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MFG_TEST: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST) {
            LOG_ERROR("%s: MFG Test - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: MFG Test - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // add the payload to the response
            value_length = property_read_mfg_test(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_POLLING_METHODS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS) {
            LOG_ERROR("%s: POLLING METHODS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         }  else if(index > 0x00) {
            LOG_ERROR("%s: POLLING METHODS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            value_length = property_read_polling_methods(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS);
         }
         break;
      }  
      case CTRLM_RF4CE_RIB_ATTR_ID_POLLING_CONFIGURATION: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION) {
            LOG_ERROR("%s: POLLING CONFIGURATION - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > CTRLM_RF4CE_RIB_ATTR_INDEX_POLLING_CONFIGURATION_MAC) {
            LOG_ERROR("%s: POLLING CONFIGURATION - Invalid Index (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_POLLING_CONFIGURATION_MAC) {
            value_length = property_read_polling_configuration_mac(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
         } else {
            value_length = property_read_polling_configuration_heartbeat(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_FAR_FIELD_CONFIGURATION: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_CONFIGURATION) {
            LOG_ERROR("%s: FAR FIELD CONFIGURATION - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: FAR FIELD CONFIGURATION - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            value_length = property_read_far_field_configuration(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_CONFIGURATION);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_FAR_FIELD_METRICS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_METRICS) {
            LOG_ERROR("%s: FAR FIELD METRICS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: FAR FIELD METRICS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            value_length = property_read_far_field_metrics(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_METRICS);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_DSP_CONFIGURATION: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DSP_CONFIGURATION) {
            LOG_ERROR("%s: DSP CONFIGURATION - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: DSP CONFIGURATION - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            value_length = property_read_dsp_configuration(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_DSP_CONFIGURATION);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_DSP_METRICS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DSP_METRICS) {
            LOG_ERROR("%s: DSP METRICS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: DSP METRICS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            value_length = property_read_dsp_metrics(data_buf, CTRLM_RF4CE_RIB_ATTR_LEN_DSP_METRICS);
         }
         break;
      }
      default: {
         LOG_INFO("%s: invalid identifier (0x%02X)\n", __FUNCTION__, identifier);
         break;
      }
   }

   if(target) {
      *data_len = value_length;
   } else {
      if(value_length > 0) {
         status   = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
      }
      // Determine when to send the response (50 ms after receipt)
      ctrlm_timestamp_add_ms(&timestamp, CTRLM_RF4CE_CONST_RESPONSE_IDLE_TIME);
      unsigned long delay = ctrlm_timestamp_until_us(timestamp);

      if(delay == 0) {
         ctrlm_timestamp_t now;
         ctrlm_timestamp_get(&now);

         long diff = ctrlm_timestamp_subtract_ms(timestamp, now);
         if(diff >= CTRLM_RF4CE_CONST_RESPONSE_WAIT_TIME) {
            LOG_WARN("%s: LATE response packet - diff %ld ms\n", __FUNCTION__, diff);
         }
      }

      response[0] = RF4CE_FRAME_CONTROL_GET_ATTRIBUTE_RESPONSE;
      response[1] = identifier;
      response[2] = index;
      response[3] = (guchar) status;
      response[4] = value_length;

      // Send the response back to the controller
      req_data(CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU, timestamp, 5 + value_length, response, NULL, NULL);
      if(identifier != CTRLM_RF4CE_RIB_ATTR_ID_MEMORY_DUMP) { // Send an IARM event for controller RIB read access
         ctrlm_rcu_iarm_event_rib_access_controller(network_id_get(), controller_id_get(), (ctrlm_rcu_rib_attr_id_t)identifier, index, CTRLM_ACCESS_TYPE_READ);
      }
   }
}

void ctrlm_obj_controller_rf4ce_t::rf4ce_rib_set_target(ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guint8 length, guchar *data) {
   ctrlm_timestamp_t timestamp;
   errno_t safec_rc = memset_s(&timestamp, sizeof(timestamp), 0, sizeof(timestamp));
   ERR_CHK(safec_rc);
   rf4ce_rib_set(true, timestamp, identifier, index, length, data);
}

void ctrlm_obj_controller_rf4ce_t::rf4ce_rib_set_controller(ctrlm_timestamp_t timestamp, ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guint8 length, guchar *data) {
   rf4ce_rib_set(false, timestamp, identifier, index, length, data);
}

void ctrlm_obj_controller_rf4ce_t::rf4ce_rib_set(gboolean target, ctrlm_timestamp_t timestamp, ctrlm_rf4ce_rib_attr_id_t identifier, guchar index, guint8 length, guchar *data) {
   ctrlm_rf4ce_rib_rsp_status_t status = CTRLM_RF4CE_RIB_RSP_STATUS_UNSUPPORTED_ATTRIBUTE;
   gboolean dont_send_iarm_message = false;

   switch(identifier) {
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_COMMAND_STATUS: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier VOICE COMMAND STATUS\n", __FUNCTION__);
         } else if(length == 0 || length > CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_STATUS) {
            LOG_ERROR("%s: VOICE COMMAND STATUS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE COMMAND STATUS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_voice_command_status(data, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_COMMAND_LENGTH: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier VOICE COMMAND LENGTH\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_LENGTH) {
            LOG_ERROR("%s: VOICE COMMAND LENGTH - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE COMMAND LENGTH - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_voice_command_length(data, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_LENGTH);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MAXIMUM_UTTERANCE_LENGTH: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier MAXIMUM UTTERANCE LENGTH\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAXIMUM_UTTERANCE_LENGTH) {
            LOG_ERROR("%s: MAXIMUM UTTERANCE LENGTH - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: MAXIMUM UTTERANCE LENGTH - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_maximum_utterance_length(data, CTRLM_RF4CE_RIB_ATTR_LEN_MAXIMUM_UTTERANCE_LENGTH);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_COMMAND_ENCRYPTION: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier VOICE COMMAND ENCRYPTION\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_ENCRYPTION) {
            LOG_ERROR("%s: VOICE COMMAND ENCRYPTION - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE COMMAND ENCRYPTION - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_voice_command_encryption(data, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_ENCRYPTION);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MAX_VOICE_DATA_RETRY: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier MAX VOICE DATA RETRY\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_DATA_RETRY) {
            LOG_ERROR("%s: MAX VOICE DATA RETRY - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: MAX VOICE DATA RETRY - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_max_voice_data_retry(data, CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_DATA_RETRY);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MAX_VOICE_CSMA_BACKOFF: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier MAX VOICE CSMA BACKOFF\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_CSMA_BACKOFF) {
            LOG_ERROR("%s: MAX VOICE CSMA BACKOFF - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: MAX VOICE CSMA BACKOFF - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_max_voice_csma_backoff(data, CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_CSMA_BACKOFF);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MIN_VOICE_DATA_BACKOFF: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier MIN VOICE DATA BACKOFF\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MIN_VOICE_DATA_BACKOFF) {
            LOG_ERROR("%s: MIN VOICE DATA BACKOFF - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: MIN VOICE DATA BACKOFF - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_min_voice_data_backoff(data, CTRLM_RF4CE_RIB_ATTR_LEN_MIN_VOICE_DATA_BACKOFF);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_TARG_AUDIO_PROFILES: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier VOICE TARG AUDIO PROFILES\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_TARG_AUDIO_PROFILES) {
            LOG_ERROR("%s: VOICE TARG AUDIO PROFILES - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE TARG AUDIO PROFILES - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            LOG_ERROR("%s: VOICE TARG AUDIO PROFILES - NOT SUPPORTED\n", __FUNCTION__);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_RIB_ENTRIES_UPDATED: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier RIB ENTRIES UPDATED\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_RIB_ENTRIES_UPDATED) {
            LOG_ERROR("%s: RIB ENTRIES UPDATED - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: RIB ENTRIES UPDATED - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            LOG_ERROR("%s: RIB ENTRIES UPDATED - Not Supported\n", __FUNCTION__);
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_RIB_UPDATE_CHECK_INTERVAL: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier RIB UPDATE CHECK INTERVAL\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_RIB_UPDATE_CHECK_INTERVAL) {
            LOG_ERROR("%s: RIB UPDATE CHECK INTERVAL - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: RIB UPDATE CHECK INTERVAL - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_rib_update_check_interval(data, CTRLM_RF4CE_RIB_ATTR_LEN_RIB_UPDATE_CHECK_INTERVAL);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_DOWNLOAD_RATE: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier DOWNLOAD RATE\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DOWNLOAD_RATE) {
            LOG_ERROR("%s: DOWNLOAD RATE - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: DOWNLOAD RATE - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_download_rate(data, CTRLM_RF4CE_RIB_ATTR_LEN_DOWNLOAD_RATE);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_UPDATE_POLLING_PERIOD: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier UPDATE POLLING PERIOD\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_POLLING_PERIOD) {
            LOG_ERROR("%s: UPDATE POLLING PERIOD - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: UPDATE POLLING PERIOD - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_update_polling_period(data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_POLLING_PERIOD);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_DATA_REQUEST_WAIT_TIME: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier DATA REQUEST WAIT TIME\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DATA_REQUEST_WAIT_TIME) {
            LOG_ERROR("%s: DATA REQUEST WAIT TIME - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: DATA REQUEST WAIT TIME - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_data_request_wait_time(data, CTRLM_RF4CE_RIB_ATTR_LEN_DATA_REQUEST_WAIT_TIME);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_IR_RF_DATABASE_STATUS: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier IR RF DATABASE STATUS\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS) {
            LOG_ERROR("%s: IR RF DATABASE STATUS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: IR RF DATABASE STATUS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_ir_rf_database_status(data[0]);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_IR_RF_DATABASE: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier IR RF DATABASE\n", __FUNCTION__);
         } else if(length > CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE) {
            LOG_ERROR("%s: IR RF DATABASE - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else {
            // Store this data in the object
            property_write_ir_rf_database(index, data, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_SHORT_RF_RETRY_PERIOD: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier SHORT RF RETRY PERIOD\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_SHORT_RF_RETRY_PERIOD) {
            LOG_ERROR("%s: SHORT RF RETRY PERIOD - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: SHORT RF RETRY PERIOD - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_short_rf_retry_period(data, CTRLM_RF4CE_RIB_ATTR_LEN_SHORT_RF_RETRY_PERIOD);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VALIDATION_CONFIGURATION: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier VALIDATION CONFIGURATION\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VALIDATION_CONFIGURATION) {
            LOG_ERROR("%s: VALIDATION CONFIGURATION - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VALIDATION CONFIGURATION - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_validation_configuration(data, CTRLM_RF4CE_RIB_ATTR_LEN_VALIDATION_CONFIGURATION);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_TARGET_IRDB_STATUS: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier TARGET IRDB STATUS\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS) {
            LOG_ERROR("%s: TARGET IRDB STATUS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: TARGET IRDB STATUS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_target_irdb_status(data, CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_PERIPHERAL_ID: {
         if(target && !ctrlm_is_production_build()) {
            LOG_ERROR("%s: target failed to write to controller attribute identifier PERIPHERAL ID\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_PERIPHERAL_ID) {
            LOG_ERROR("%s: PERIPHERAL ID - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: PERIPHERAL ID - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_peripheral_id(data, CTRLM_RF4CE_RIB_ATTR_LEN_PERIPHERAL_ID);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_RF_STATISTICS: {
         if(target && !ctrlm_is_production_build()) {
            LOG_ERROR("%s: target failed to write to controller attribute identifier RF STATISTICS\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_RF_STATISTICS) {
            LOG_ERROR("%s: RF STATISTICS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: RF STATISTICS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_rf_statistics(data, CTRLM_RF4CE_RIB_ATTR_LEN_RF_STATISTICS);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VERSIONING: {
         if(target && !ctrlm_is_production_build()) {
            LOG_ERROR("%s: target failed to write to controller attribute identifier VERSIONING\n", __FUNCTION__);
         } else if(((index != CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_BUILD_ID && index != CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_DSP_BUILD_ID) && length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) || 
            (length > CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_BUILD_ID)) { // Build ID can be any length, so exclude it
            LOG_ERROR("%s: VERSIONING - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_DSP_BUILD_ID) {
            LOG_ERROR("%s: VERSIONING - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_ARM) { // ARM
            // Store this data in the object
            property_write_version_arm(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_KEYWORD_MODEL) { // Keyword Model
            // Store this data in the object
            property_write_version_keyword_model(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_DSP) { // Hardware
            // Store this data in the object
            property_write_version_dsp(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_BUILD_ID) {
            property_write_version_build_id(data, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_IRDB) { // IR Database
            // Store this data in the object
            property_write_version_irdb(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_HARDWARE) { // Hardware
            // Store this data in the object
            property_write_version_hardware(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_DSP_BUILD_ID) {
            property_write_version_dsp_build_id(data, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         } else {                   // Software
            // Store this data in the object
            property_write_version_software(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_BATTERY_STATUS: {
         if(target && !ctrlm_is_production_build()) {
            LOG_ERROR("%s: target failed to write to controller attribute identifier BATTERY STATUS\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS) {
            LOG_ERROR("%s: BATTERY STATUS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: BATTERY STATUS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            if(property_write_battery_status(data, CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS) == 0) {
               dont_send_iarm_message = true;
            }
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_CTRL_AUDIO_PROFILES: {
         if(target && !ctrlm_is_production_build()) {
            LOG_ERROR("%s: target failed to write to controller attribute identifier VOICE CONTROLLER AUDIO PROFILES\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES) {
            LOG_ERROR("%s: VOICE CTRL AUDIO PROFILES - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE CTRL AUDIO PROFILES - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_voice_ctrl_audio_profiles(data, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_STATISTICS: {
         if(target && !ctrlm_is_production_build()) {
            LOG_ERROR("%s: target failed to write to controller attribute identifier VOICE STATISTICS\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS) {
            LOG_ERROR("%s: VOICE STATISTICS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE STATISTICS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_voice_statistics(data, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_SESSION_STATISTICS: {
         if(target && !ctrlm_is_production_build()) {
            LOG_ERROR("%s: target failed to write to controller attribute identifier VOICE SESSION STATISTICS\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_STATISTICS) {
            LOG_ERROR("%s: VOICE SESSION STATISTICS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE SESSION STATISTICS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_voice_session_statistics(data, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_STATISTICS);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_OPUS_ENCODING_PARAMS: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier OPUS ENCODING PARAMS\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS) {
            LOG_ERROR("%s: OPUS ENCODING PARAMS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: OPUS ENCODING PARAMS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_opus_encoding_params(data, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_SESSION_QOS: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier VOICE SESSION QOS\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_QOS) {
            LOG_ERROR("%s: VOICE SESSION QOS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: VOICE SESSION QOS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_voice_session_qos(data, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_UPDATE_VERSIONING: {
         if(target && !ctrlm_is_production_build()) {
            LOG_ERROR("%s: target failed to write to controller attribute identifier UPDATE VERSIONING\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING) {
            LOG_ERROR("%s: UPDATE VERSIONING - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index != 0x01 && index != 0x02 && index != 0x10) {
            LOG_ERROR("%s: UPDATE VERSIONING - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            if(index == 0x01){
               // Store this data in the object
               property_write_update_version_bootloader(data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
            } else if(index == 0x02) {
               // Store this data in the object
               property_write_update_version_golden(data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
            } else {
               // Store this data in the object
               property_write_update_version_audio_data(data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
            }
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_PRODUCT_NAME: {
         if(target && !ctrlm_is_production_build()) {
            LOG_ERROR("%s: target failed to write to controller attribute identifier PRODUCT NAME\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME) {
            LOG_ERROR("%s: PRODUCT NAME - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: PRODUCT NAME - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_product_name(data, CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME);

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_CONTROLLER_IRDB_STATUS: {
         if(target && !ctrlm_is_production_build()) {
            LOG_ERROR("%s: target failed to write to controller attribute identifier CONTROLLER IRDB STATUS \n", __FUNCTION__);
         } else if((length != CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS) && (length != CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS_MINUS_LOAD_STATUS_BYTES)) {
            LOG_ERROR("%s: CONTROLLER IRDB STATUS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: CONTROLLER IRDB STATUS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_controller_irdb_status(data, length);

            //If we're not loading the db, the controller irdb status is new, and this is not XR19
            if((!loading_db_) && (controller_irdb_status_is_new_) && (RF4CE_CONTROLLER_TYPE_XR19 != controller_type_)) {
               // Set the global target irdb status based on this controller irdb status
               obj_network_rf4ce_->target_irdb_status_set(controller_irdb_status_);
               controller_irdb_status_is_new_ = false;
            }

            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MEMORY_DUMP: {
         if(target && !ctrlm_is_production_build()) {
            LOG_ERROR("%s: target failed to write to controller attribute identifier MEMORY DUMP\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_DUMP) {
            LOG_ERROR("%s: MEMORY DUMP - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else {
            // Store this data in the object
            property_write_memory_dump(index, data, CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_DUMP);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_TARGET_ID_DATA: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier TARGET ID DATA\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_ID_DATA) {
            LOG_ERROR("%s: TARGET ID DATA - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > CTRLM_RF4CE_RIB_ATTR_INDEX_TARGET_ID_DATA_DEVICE_ID) {
            LOG_ERROR("%s: TARGET ID DATA - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_TARGET_ID_DATA_DEVICE_ID) {
            property_write_device_id(data, CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_ID_DATA);
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_TARGET_ID_DATA_RECEIVER_ID) {
            property_write_receiver_id(data, CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_ID_DATA);
         } else {
            LOG_WARN("%s: Account ID not implemented yet\n", __FUNCTION__);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_UNSUPPORTED_ATTRIBUTE;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MFG_TEST: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only identifier MFG TEST\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST) {
            LOG_ERROR("%s: MFG TEST - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: MFG TEST - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            // Store this data in the object
            property_write_mfg_test(data, CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_POLLING_METHODS: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only indentifier POLLING METHODS\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS) {
            LOG_ERROR("%s: POLLING METHODS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         }  else if(index > 0x00) {
            LOG_ERROR("%s: POLLING METHODS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            property_write_polling_methods(data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_POLLING_CONFIGURATION: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only indentifier POLLING CONFIGURATION\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION) {
            LOG_ERROR("%s: POLLING CONFIGURATION - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > CTRLM_RF4CE_RIB_ATTR_INDEX_POLLING_CONFIGURATION_MAC) {
            LOG_ERROR("%s: POLLING CONFIGURATION - Invalid Index (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else if(index == CTRLM_RF4CE_RIB_ATTR_INDEX_POLLING_CONFIGURATION_MAC) {
            property_write_polling_configuration_mac(data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         } else {
            property_write_polling_configuration_heartbeat(data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_PRIVACY: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_PRIVACY) {
            LOG_ERROR("%s: PRIVACY - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: PRIVACY - Invalid Index (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            property_write_privacy(data, CTRLM_RF4CE_RIB_ATTR_LEN_PRIVACY);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_CONTROLLER_CAPABILITIES: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_CAPABILITIES) {
            LOG_ERROR("%s: CONTROLLER_CAPABILITIES - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: CONTROLLER_CAPABILITIES - Invalid Index (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            property_write_controller_capabilities(data, CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_CAPABILITIES);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_FAR_FIELD_CONFIGURATION: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only indentifier FAR FIELD CONFIGURATION\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_CONFIGURATION) {
            LOG_ERROR("%s: FAR FIELD CONFIGURATION - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: FAR FIELD CONFIGURATION - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            property_write_far_field_configuration(data, CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_CONFIGURATION);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_FAR_FIELD_METRICS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_METRICS) {
            LOG_ERROR("%s: FAR FIELD METRICS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: FAR FIELD METRICS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            property_write_far_field_metrics(data, CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_METRICS);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_DSP_CONFIGURATION: {
         if(!target) {
            LOG_ERROR("%s: controller write to read only indentifier DSP CONFIGURATION\n", __FUNCTION__);
         } else if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DSP_CONFIGURATION) {
            LOG_ERROR("%s: DSP CONFIGURATION - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: DSP CONFIGURATION - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            property_write_dsp_configuration(data, CTRLM_RF4CE_RIB_ATTR_LEN_DSP_CONFIGURATION);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_DSP_METRICS: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DSP_METRICS) {
            LOG_ERROR("%s: DSP METRICS - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x00) {
            LOG_ERROR("%s: DSP METRICS - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else {
            property_write_dsp_metrics(data, CTRLM_RF4CE_RIB_ATTR_LEN_DSP_METRICS);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_GENERAL_PURPOSE: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_GENERAL_PURPOSE) {
            LOG_ERROR("%s: GENERAL PURPOSE - Invalid Length (%u)\n", __FUNCTION__, length);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_PARAMETER;
         } else if(index > 0x01) {
            LOG_ERROR("%s: GENERAL PURPOSE - Invalid Index (%u)\n", __FUNCTION__, index);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_INVALID_INDEX;
         } else if(index == 0x01) { // Memory Stats
            // Store this data in the object
            property_write_memory_stats(data, CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATS);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         } else { // Reboot Stats
            // Store this data in the object
            property_write_reboot_stats(data, CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_STATS);
            status = CTRLM_RF4CE_RIB_RSP_STATUS_SUCCESS;
         }
         break;
      }
      default: {
         LOG_INFO("%s: invalid identifier (0x%02X)\n", __FUNCTION__, identifier);
         break;
      }
   }

   if(!target) { // Send the response back to the controller
      guchar response[4];

      // Determine when to send the response (50 ms after receipt)
      ctrlm_timestamp_add_ms(&timestamp, CTRLM_RF4CE_CONST_RESPONSE_IDLE_TIME);
      unsigned long delay = ctrlm_timestamp_until_us(timestamp);

      if(delay == 0) {
         ctrlm_timestamp_t now;
         ctrlm_timestamp_get(&now);

         long diff = ctrlm_timestamp_subtract_ms(timestamp, now);
         if(diff >= CTRLM_RF4CE_CONST_RESPONSE_WAIT_TIME) {
            LOG_WARN("%s: LATE response packet - diff %ld ms\n", __FUNCTION__, diff);
         }
      }

      response[0] = RF4CE_FRAME_CONTROL_SET_ATTRIBUTE_RESPONSE;
      response[1] = identifier;
      response[2] = index;
      response[3] = (guchar) status;

      req_data(CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU, timestamp, 4, response, NULL, NULL);
      if(dont_send_iarm_message) {
         LOG_INFO("%s: Skipping iarm message.\n", __FUNCTION__);
         return;
      }
      if(identifier != CTRLM_RF4CE_RIB_ATTR_ID_MEMORY_DUMP) { // Send an IARM event for controller RIB write access
         ctrlm_rcu_iarm_event_rib_access_controller(network_id_get(), controller_id_get(), (ctrlm_rcu_rib_attr_id_t)identifier, index, CTRLM_ACCESS_TYPE_WRITE);
      }
   }
}
