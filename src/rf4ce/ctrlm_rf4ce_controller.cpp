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
#include <time.h>
#include <glib.h>
#include <string.h>
#include <vector>
#include <sstream>
#include <algorithm>
#include <uuid/uuid.h>
#include "../ctrlm.h"
#include "../ctrlm_utils.h"
#include "../ctrlm_rcu.h"
#include "ctrlm_rf4ce_network.h"
#include "../ctrlm_voice.h"
#include "../ctrlm_device_update.h"
#include "../ctrlm_database.h"
#include "../ctrlm_validation.h"
#ifdef ASB
#include "../cpc/asb/advanced_secure_binding.h"
#endif
#include "irMgr.h"

using namespace std;

//#define USE_LOCAL_IR_RF_DB_CODES
#ifdef USE_LOCAL_IR_RF_DB_CODES

static char samsung_0x6B[] = {0x88, 0x00, 0x29, 0x04, 0x13, 0x04, 0x00, 0x22, 0x00, 0xD2, 0x00, 0x90, 0x00, 0x04, 0x2D, 0x65, 0x04, 0x65, 0x04, 0x90, 0x00, 0xA6, 0x01, 0x90, 0x00, 0x8D, 0x00, 0x12, 0x22, 0x33, 0x33, 0x32, 0x22, 0x33, 0x33, 0x33, 0x23, 0x33, 0x33, 0x32, 0x32, 0x22, 0x22, 0x20};
static char samsung_0x6C[] = {0x8C, 0x21, 0x4C, 0x02, 0x31, 0x01, 0x4F, 0x29, 0x04, 0x13, 0x04, 0x00, 0x22, 0x00, 0xD2, 0x00, 0x90, 0x00, 0x04, 0x2D, 0x65, 0x04, 0x65, 0x04, 0x90, 0x00, 0xA6, 0x01, 0x90, 0x00, 0x8D, 0x00, 0x12, 0x22, 0x33, 0x33, 0x32, 0x22, 0x33, 0x33, 0x32, 0x33, 0x22, 0x33, 0x23, 0x22, 0x33, 0x22, 0x30};
static char samsung_0x6D[] = {0x8C, 0x21, 0x4C, 0x02, 0x31, 0x02, 0x4F, 0x29, 0x04, 0x13, 0x04, 0x00, 0x22, 0x00, 0xD2, 0x00, 0x90, 0x00, 0x04, 0x2D, 0x65, 0x04, 0x65, 0x04, 0x90, 0x00, 0xA6, 0x01, 0x90, 0x00, 0x8D, 0x00, 0x12, 0x22, 0x33, 0x33, 0x32, 0x22, 0x33, 0x33, 0x33, 0x33, 0x22, 0x33, 0x22, 0x22, 0x33, 0x22, 0x30};
static char samsung_0x41[] = {0x8C, 0x01, 0x4C, 0x02, 0x31, 0x00, 0x00, 0x29, 0x04, 0x11, 0x04, 0x00, 0x22, 0x00, 0xD2, 0x00, 0x90, 0x00, 0x04, 0x2D, 0x65, 0x04, 0x65, 0x04, 0x90, 0x00, 0xA6, 0x01, 0x90, 0x00, 0x8D, 0x00, 0x12, 0x22, 0x33, 0x33, 0x32, 0x22, 0x33, 0x33, 0x32, 0x22, 0x33, 0x33, 0x33, 0x33, 0x22, 0x22, 0x20};
static char samsung_0x42[] = {0x8C, 0x01, 0x4C, 0x02, 0x31, 0x00, 0x00, 0x29, 0x04, 0x11, 0x04, 0x00, 0x22, 0x00, 0xD2, 0x00, 0x90, 0x00, 0x04, 0x2D, 0x65, 0x04, 0x65, 0x04, 0x90, 0x00, 0xA6, 0x01, 0x90, 0x00, 0x8D, 0x00, 0x12, 0x22, 0x33, 0x33, 0x32, 0x22, 0x33, 0x33, 0x32, 0x23, 0x23, 0x33, 0x33, 0x32, 0x32, 0x22, 0x20};
static char samsung_0x43[] = {0x8C, 0x01, 0x4C, 0x02, 0x31, 0x00, 0x00, 0x29, 0x04, 0x11, 0x04, 0x00, 0x22, 0x00, 0xD2, 0x00, 0x90, 0x00, 0x04, 0x2D, 0x65, 0x04, 0x65, 0x04, 0x90, 0x00, 0xA6, 0x01, 0x90, 0x00, 0x8D, 0x00, 0x12, 0x22, 0x33, 0x33, 0x32, 0x22, 0x33, 0x33, 0x32, 0x22, 0x23, 0x33, 0x33, 0x33, 0x32, 0x22, 0x20};
static char samsung_0x34[] = {0x88, 0x00, 0x29, 0x04, 0x11, 0x04, 0x00, 0x22, 0x00, 0xD2, 0x00, 0x90, 0x00, 0x04, 0x2D, 0x65, 0x04, 0x65, 0x04, 0x90, 0x00, 0xA6, 0x01, 0x90, 0x00, 0x8D, 0x00, 0x12, 0x22, 0x33, 0x33, 0x32, 0x22, 0x33, 0x33, 0x32, 0x33, 0x33, 0x33, 0x33, 0x22, 0x22, 0x22, 0x20};

#endif

#define CTRLM_BATTERY_75_PERCENT_THRESHOLD 75
#define CTRLM_BATTERY_50_PERCENT_THRESHOLD 50
#define CTRLM_BATTERY_25_PERCENT_THRESHOLD 25
#define CTRLM_BATTERY_5_PERCENT_THRESHOLD  5
#define CTRLM_BATTERY_0_PERCENT_THRESHOLD  0

#define NUM_XR11V2_HARDWARE_VERSIONS       1
#define NUM_XR15V1_HARDWARE_VERSIONS       1
#define NUM_XR15V2_HARDWARE_VERSIONS       2
#define NUM_XR11V2_HARDWARE_FIX_INDEX      0
#define NUM_XR15V1_HARDWARE_FIX_INDEX      0
#define NUM_XR15V2_HARDWARE_FIX_INDEX      0
#define XR11V2_IEEE_PREFIX                 0x00124B0000000000
#define XR15V1_IEEE_PREFIX                 0x00155F0000000000
#define XR15V2_IEEE_PREFIX                 0x00155F0000000000
#define XR15V2_IEEE_PREFIX_UP_TO_FEB_2018  0x48D0CF0000000000
#define XR15V2_IEEE_PREFIX_THIRD           0x00CC3F0000000000

#define NUM_OF_PRODUCT_NAME_TYPES (sizeof(Product_name_pair_type__table)/sizeof(Product_name_pair_type__table[0]))

const guchar xr11v2_hardware_versions[NUM_XR11V2_HARDWARE_VERSIONS][CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING] = { {0x22, 0x01, 0x00, 0x00}                           };
const guchar xr15v1_hardware_versions[NUM_XR15V1_HARDWARE_VERSIONS][CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING] = { {0x23, 0x01, 0x00, 0x00}                           };
const guchar xr15v2_hardware_versions[NUM_XR15V2_HARDWARE_VERSIONS][CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING] = { {0x23, 0x02, 0x00, 0x00}, {0x13, 0x02, 0x00, 0x00} };

/* Below structure table and helper function used for better optinamization of multiple if- else cases */
/* table with Product name with corresponding type */
ctrlm_rf4ce_product_name_pair_t Product_name_pair_type__table[] = {
  { "XR2-",    XR2,    RF4CE_CONTROLLER_TYPE_XR2    },
  { "XR5-",    XR5,    RF4CE_CONTROLLER_TYPE_XR5    },
  { "XR11-",   XR11,   RF4CE_CONTROLLER_TYPE_XR11   },
  { "XR15-1",  XR15_1, RF4CE_CONTROLLER_TYPE_XR15   },
  { "XR15-2",  XR15_2, RF4CE_CONTROLLER_TYPE_XR15V2 },
  { "XR16-",   XR16,   RF4CE_CONTROLLER_TYPE_XR16   },
  { "XR18-",   XR18,   RF4CE_CONTROLLER_TYPE_XR18   },
  { "XR19-",   XR19,   RF4CE_CONTROLLER_TYPE_XR19   },
  { "XRA-",    XRA,    RF4CE_CONTROLLER_TYPE_XRA    },
};

/* Function to get the corresponding product name type from the given product name */
bool ctrlm_rf4ce_get_product_type_from_product_name(const char *name, ctrlm_rf4ce_product_name_t *product_type, ctrlm_rf4ce_controller_type_t *controller_type) {
  unsigned int i = 0;

  if((name == NULL) || (product_type == NULL) || (controller_type == NULL) )
     return false;

  for (i = 0 ; i < NUM_OF_PRODUCT_NAME_TYPES ; ++i) {
      if(0 == strncmp(name, Product_name_pair_type__table[i].name, strlen(Product_name_pair_type__table[i].name))) {
         *product_type = Product_name_pair_type__table[i].product_type;
         *controller_type = Product_name_pair_type__table[i].controller_type;
         return true;
      }
  }
  return false;
}

ctrlm_obj_controller_rf4ce_t::ctrlm_obj_controller_rf4ce_t(ctrlm_controller_id_t controller_id, ctrlm_obj_network_rf4ce_t &network, unsigned long long ieee_address, ctrlm_rf4ce_result_validation_t result_validation, ctrlm_rcu_configuration_result_t result_configuration) :
   ctrlm_obj_controller_t(controller_id, network),
   obj_network_rf4ce_(&network),
   loading_db_(false),
   stored_in_db_(false),
   pairing_data_(NULL),
   ieee_address_(ieee_address),
   short_address_(0),
   validation_result_(result_validation),
   configuration_result_(result_configuration),
   version_software_{0,0,0,0},
   version_dsp_{0,0,0,0},
   version_keyword_model_{0,0,0,0},
   version_arm_{0,0,0,0},
   version_irdb_{0,0,0,0},
   version_hardware_{0,0,0,0},
   battery_status_{0,0,RF4CE_BATTERY_STATUS_VOLTAGE_LOADED_DEFAULT,RF4CE_BATTERY_STATUS_VOLTAGE_UNLOADED_DEFAULT,0,0,RF4CE_BATTERY_STATUS_VOLTAGE_PERCENTAGE_DEFAULT},
   voice_command_length_(VOICE_COMMAND_LENGTH_VALUE),
   voice_cmd_count_today_(0),
   voice_cmd_count_yesterday_(0),
   voice_cmd_short_today_(0),
   voice_cmd_short_yesterday_(0),
   today_(time(NULL) / (60 * 60 * 24)),
   voice_packets_sent_today_(0),
   voice_packets_sent_yesterday_(0),
   voice_packets_lost_today_(0),
   voice_packets_lost_yesterday_(0),
   utterances_exceeding_packet_loss_threshold_today_(0),
   utterances_exceeding_packet_loss_threshold_yesterday_(0),
   firmware_updated_(RF4CE_FIRMWARE_UPDATED_NO),
   audio_profiles_ctrl_(VOICE_AUDIO_PROFILE_NONE),
   voice_statistics_{0,0},
   rib_entries_updated_(RIB_ENTRIES_UPDATED_TRUE),
   version_bootloader_{0,0,0,0},
   version_golden_{0,0,0,0},
   version_audio_data_{0,0,0,0},
   product_name_{RF4CE_PRODUCT_NAME_UNKNOWN},
   manufacturer_(),
   chipset_(),
   controller_type_(RF4CE_CONTROLLER_TYPE_UNKNOWN),
   download_rate_(DOWNLOAD_RATE_BACKGROUND),
   controller_irdb_status_{CONTROLLER_IRDB_STATUS_FLAGS_NO_IR_PROGRAMMED,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
   controller_irdb_status_is_new_(false),
   reboot_reason_(CONTROLLER_REBOOT_OTHER),
   reboot_voltage_level_(0),
   reboot_assert_number_(0),
   memory_available_(0),
   memory_largest_(0),
   ir_rf_database_status_(IR_RF_DATABASE_STATUS_DEFAULT),
   download_in_progress_(false),
   download_image_id_(RF4CE_DOWNLOAD_IMAGE_ID_INVALID),
   autobind_in_progress_(false),
   binding_button_in_progress_(false),
   binding_type_(CTRLM_RCU_BINDING_TYPE_INVALID),
   validation_type_(CTRLM_RCU_VALIDATION_TYPE_INVALID),
   binding_security_type_(CTRLM_RCU_BINDING_SECURITY_TYPE_NORMAL),
   backup_binding_type_(CTRLM_RCU_BINDING_TYPE_INVALID),
   backup_validation_type_(CTRLM_RCU_VALIDATION_TYPE_INVALID),
   backup_binding_security_type_(CTRLM_RCU_BINDING_SECURITY_TYPE_NORMAL),
   time_binding_(0),
   time_last_key_(0),
   time_next_flush_(0), // First key will always get flushed to DB
   time_last_checkin_for_device_update_(0),
   last_key_status_(CTRLM_KEY_STATUS_INVALID),
   last_key_code_(CTRLM_KEY_CODE_INVALID),
   manual_poll_firmware_(false),
   manual_poll_audio_data_(false),
   audio_theme_(RF4CE_DEVICE_UPDATE_AUDIO_THEME_INVALID),
   crash_dump_buf_(NULL),
   crash_dump_size_(0),
   crash_dump_expected_size_(0),
   configuration_complete_failure_(false),
   privacy_(0),
   time_metrics_(0),
   polling_methods_(0),
   time_last_heartbeat_(0),
   rib_configuration_complete_status_(RF4CE_RIB_CONFIGURATION_COMPLETE_PAIRING_INCOMPLETE),
#ifdef ASB
   asb_key_derivation_method_used_(ASB_KEY_DERIVATION_NONE),
#endif
#ifdef XR15_704
   needs_reset_(false),
   did_reset_(false),
#endif
   fmr_supported_(false),
   mfg_test_result_(1),
   ota_failures_(0)
{
   LOG_INFO("ctrlm_obj_controller_rf4ce_t constructor - %u\n", controller_id);

#ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   short_rf_retry_period_        = JSON_INT_VALUE_SHORT_RF_RETRY_PERIOD;
   utterance_duration_max_       = JSON_INT_VALUE_UTTERANCE_DURATION_MAX;
   voice_data_retry_max_         = JSON_INT_VALUE_VOICE_DATA_RETRY_MAX;
   voice_csma_backoff_max_       = JSON_INT_VALUE_VOICE_CSMA_BACKOFF_MAX;
   voice_data_backoff_exp_min_   = JSON_INT_VALUE_VOICE_DATA_BACKOFF_EXP_MIN;
   rib_update_check_interval_    = JSON_INT_VALUE_RIB_UPDATE_CHECK_INTERVAL;
   auto_check_validation_period_ = JSON_INT_VALUE_AUTO_CHECK_VALIDATION_PERIOD;
   link_lost_wait_time_          = JSON_INT_VALUE_LINK_LOST_WAIT_TIME;
   update_polling_period_        = JSON_INT_VALUE_UPDATE_POLLING_PERIOD;
   data_request_wait_time_       = JSON_INT_VALUE_DATA_REQUEST_WAIT_TIME;
   voice_command_encryption_     = (voice_command_encryption_t)JSON_INT_VALUE_VOICE_COMMAND_ENCRYPTION;
#endif

   #ifdef USE_LOCAL_IR_RF_DB_CODES
   // These codes will be received over iarm and set to each controller.  store them on the network object.
   property_write_ir_rf_database(0x6B, samsung_0x6B, sizeof(samsung_0x6B));
   property_write_ir_rf_database(0x6C, samsung_0x6C, sizeof(samsung_0x6C));
   property_write_ir_rf_database(0x6D, samsung_0x6D, sizeof(samsung_0x6D));
   property_write_ir_rf_database(0x41, samsung_0x41, sizeof(samsung_0x41));
   property_write_ir_rf_database(0x42, samsung_0x42, sizeof(samsung_0x42));
   property_write_ir_rf_database(0x43, samsung_0x43, sizeof(samsung_0x43));
   property_write_ir_rf_database(0x34, samsung_0x34, sizeof(samsung_0x34));
   #endif

   // Clear out build_id
   errno_t safec_rc = memset_s(&version_build_id_, sizeof(version_build_id_), 0, sizeof(version_build_id_));
   ERR_CHK(safec_rc);
   safec_rc = memset_s(&version_dsp_build_id_, sizeof(version_dsp_build_id_), 0, sizeof(version_dsp_build_id_));
   ERR_CHK(safec_rc);

   print_firmware_on_button_press = true;

   has_battery_                                               = false;
   has_dsp_                                                   = false;
   battery_milestones_.battery_changed_timestamp              = 0;
   battery_milestones_.battery_changed_actual_percent         = 0;
   battery_milestones_.battery_75_percent_timestamp           = 0;
   battery_milestones_.battery_75_percent_actual_percent      = 0;
   battery_milestones_.battery_50_percent_timestamp           = 0;
   battery_milestones_.battery_50_percent_actual_percent      = 0;
   battery_milestones_.battery_25_percent_timestamp           = 0;
   battery_milestones_.battery_25_percent_actual_percent      = 0;
   battery_milestones_.battery_5_percent_timestamp            = 0;
   battery_milestones_.battery_5_percent_actual_percent       = 0;
   battery_milestones_.battery_0_percent_timestamp            = 0;
   battery_milestones_.battery_0_percent_actual_percent       = 0;
   battery_last_good_timestamp_                               = 0;
   battery_last_good_percent_                                 = 0;
   battery_last_good_loaded_voltage_                          = 0;
   battery_last_good_unloaded_voltage_                        = 0;
   battery_changed_unloaded_voltage_                          = 0;
   battery_75_percent_unloaded_voltage_                       = 0;
   battery_50_percent_unloaded_voltage_                       = 0;
   battery_25_percent_unloaded_voltage_                       = 0;
   battery_5_percent_unloaded_voltage_                        = 0;
   battery_0_percent_unloaded_voltage_                        = 0;
   battery_voltage_large_jump_counter_                        = 0;
   battery_voltage_large_decline_detected_                    = false;
   battery_first_write_                                       = true;

   // Far Field
   safec_rc = memset_s(&ff_metrics_, sizeof(ff_metrics_), 0, sizeof(ff_metrics_));
   ERR_CHK(safec_rc);
   safec_rc = memset_s(&dsp_metrics_, sizeof(dsp_metrics_), 0, sizeof(dsp_metrics_));
   ERR_CHK(safec_rc);

   // Uptime / Privacy Mode
   safec_rc = memset_s(&uptime_privacy_info_, sizeof(uptime_privacy_info_), 0, sizeof(uptime_privacy_info_));
   ERR_CHK(safec_rc);
   time_since_last_saved_ = 0;

   // Polling Init
   safec_rc = memset_s(&polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT], sizeof(ctrlm_rf4ce_polling_configuration_t), 0, sizeof(ctrlm_rf4ce_polling_configuration_t));
   ERR_CHK(safec_rc);
   safec_rc = memset_s(&polling_configurations_[RF4CE_POLLING_METHOD_MAC], sizeof(ctrlm_rf4ce_polling_configuration_t), 0, sizeof(ctrlm_rf4ce_polling_configuration_t));
   ERR_CHK(safec_rc);
   polling_actions_ = g_async_queue_new_full(ctrlm_rf4ce_polling_action_free);
   safec_rc = memset_s(&checkin_time_, sizeof(checkin_time_), 0, sizeof(checkin_time_));
   ERR_CHK(safec_rc);

   // Clear Voice Command Status
   safec_rc = memset_s(voice_command_status_, sizeof(voice_command_status_), 0, sizeof(voice_command_status_));
   ERR_CHK(safec_rc);
}

ctrlm_obj_controller_rf4ce_t::ctrlm_obj_controller_rf4ce_t() {
   LOG_INFO("ctrlm_obj_controller_rf4ce_t constructor - default\n");
}

ctrlm_obj_controller_rf4ce_t::~ctrlm_obj_controller_rf4ce_t() {
   LOG_INFO("ctrlm_obj_controller_rf4ce_t deconstructor\n");

   if(pairing_data_ != NULL) {
      ctrlm_hal_free(pairing_data_);
   }
   // check if ctrlm_db_rf4ce_write_file() was never called and free crash_dump_buf_
   if(crash_dump_buf_ != NULL) {
      ctrlm_hal_free(crash_dump_buf_);
   }
   if(polling_actions_ != NULL) {
      g_async_queue_unref(polling_actions_);
   }
}
 
void ctrlm_obj_controller_rf4ce_t::controller_destroy() {
   //Save the uptime/privacy info
   if(ctrlm_is_voice_assistant((ctrlm_rcu_controller_type_t)controller_type_)) {
      ctrlm_db_rf4ce_write_uptime_privacy_info(network_id_get(), controller_id_get(), (guchar *)&uptime_privacy_info_, sizeof(uptime_privacy_info_t)) ;
   }
}

#ifndef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
guint32                    ctrlm_obj_controller_rf4ce_t::short_rf_retry_period_get(void)        { return(obj_network_rf4ce_->short_rf_retry_period_get());        }
guint16                    ctrlm_obj_controller_rf4ce_t::utterance_duration_max_get(void)       { return(obj_network_rf4ce_->utterance_duration_max_get());       }
guchar                     ctrlm_obj_controller_rf4ce_t::voice_data_retry_max_get(void)         { return(obj_network_rf4ce_->voice_data_retry_max_get());         }
guchar                     ctrlm_obj_controller_rf4ce_t::voice_csma_backoff_max_get(void)       { return(obj_network_rf4ce_->voice_csma_backoff_max_get());       }
guchar                     ctrlm_obj_controller_rf4ce_t::voice_data_backoff_exp_min_get(void)   { return(obj_network_rf4ce_->voice_data_backoff_exp_min_get());   }
guint16                    ctrlm_obj_controller_rf4ce_t::rib_update_check_interval_get(void)    { return(obj_network_rf4ce_->rib_update_check_interval_get());    }
guint16                    ctrlm_obj_controller_rf4ce_t::auto_check_validation_period_get(void) { return(obj_network_rf4ce_->auto_check_validation_period_get()); }
guint16                    ctrlm_obj_controller_rf4ce_t::link_lost_wait_time_get(void)          { return(obj_network_rf4ce_->link_lost_wait_time_get());          }
guint16                    ctrlm_obj_controller_rf4ce_t::update_polling_period_get(void)        { return(obj_network_rf4ce_->update_polling_period_get());        }
guint16                    ctrlm_obj_controller_rf4ce_t::data_request_wait_time_get(void)       { return(obj_network_rf4ce_->data_request_wait_time_get());       }
voice_command_encryption_t ctrlm_obj_controller_rf4ce_t::voice_command_encryption_get(void)     { return(obj_network_rf4ce_->voice_command_encryption_get());     }
#endif
guint16                    ctrlm_obj_controller_rf4ce_t::audio_profiles_targ_get(void)          { return(obj_network_rf4ce_->audio_profiles_targ_get());          }

unsigned long long ctrlm_obj_controller_rf4ce_t::ieee_address_get(void) {
   return(ieee_address_);
}
void ctrlm_obj_controller_rf4ce_t::stats_update(void) {
   ctrlm_hal_network_property_controller_stats_t controller_stats;
   controller_stats.controller_id = controller_id_get();
   controller_stats.short_address = 0;

   if(CTRLM_HAL_RESULT_SUCCESS != network_property_get(CTRLM_HAL_NETWORK_PROPERTY_CONTROLLER_STATS, (void **)&controller_stats)) {
      LOG_ERROR("%s: Unable to update controller stats\n", __FUNCTION__);
   } else {
      short_address_ = controller_stats.short_address;
#if CTRLM_HAL_RF4CE_API_VERSION >= 14
      checkin_time_ = controller_stats.checkin_time;    ///< OUT - Timestamp indicating the most recent poll indication of the controller
#endif
   }
}

ctrlm_timestamp_t ctrlm_obj_controller_rf4ce_t::last_mac_poll_checkin_time_get() {
   stats_update();
   return checkin_time_;
}


void ctrlm_obj_controller_rf4ce_t::user_string_set(guchar *user_string) {
   controller_type_ = obj_network_rf4ce_->controller_type_from_user_string(user_string);

   errno_t safec_rc = strcpy_s(product_name_, sizeof(product_name_),(char *)user_string);
   ERR_CHK(safec_rc);

   // If string is comcast, assume XR2
   if(0 == strncmp((const char *)&user_string[7], "COMCAST", 7)) {
      LOG_WARN("%s: Assuming XR2.\n", __FUNCTION__);
      safec_rc = strcpy_s(product_name_, sizeof(product_name_),"XR2-10");
      ERR_CHK(safec_rc);
   } else if (controller_type_ == RF4CE_CONTROLLER_TYPE_UNKNOWN) {
      LOG_WARN("%s: Assuming XR5.\n", __FUNCTION__);
      safec_rc = strcpy_s(product_name_, sizeof(product_name_),"XR5-40");
      ERR_CHK(safec_rc);
   }

   controller_chipset_from_product_name();

   has_battery_ = ctrlm_rf4ce_has_battery(controller_type_);
   has_dsp_     = ctrlm_rf4ce_has_dsp(controller_type_);
}

void ctrlm_obj_controller_rf4ce_t::controller_chipset_from_product_name(void) {
   ctrlm_rf4ce_product_name_t product_type;
   ctrlm_rf4ce_controller_type_t ctrlm_type;
   errno_t safec_rc = -1;

   // Convert the string to controller type
   if(ctrlm_rf4ce_get_product_type_from_product_name(product_name_,&product_type,&ctrlm_type)) {
      controller_type_ = ctrlm_type;

      if(product_type == XR2 || product_type == XR5 || product_type == XR11 ) {
         // XR2-30, XR2-10, XR5-10, XR5-40 , // XR11-20
         safec_rc = strcpy_s(chipset_, sizeof(chipset_), "TI");
         ERR_CHK(safec_rc);
      } else if(product_type == XR15_1 || product_type == XR15_2 || product_type == XR16 || product_type == XR18 || product_type == XR19 || product_type == XRA) {
         // XR15-10 , // XR15-20 , // XR16-10 , // XR18-10 , // XR19-10, // XRA
         safec_rc = strcpy_s(chipset_, sizeof(chipset_), "QORVO");
         ERR_CHK(safec_rc);
      }
   }
   else {
      LOG_ERROR("%s: Unsupported controller type <%s>\n", __FUNCTION__, product_name_);
   }
}

const char *ctrlm_obj_controller_rf4ce_t::product_name_get(void) const {
   return (product_name_);
}

const battery_status_t& ctrlm_obj_controller_rf4ce_t::battery_status_get() const {
   return battery_status_;
}

void ctrlm_obj_controller_rf4ce_t::autobind_in_progress_set(bool in_progress) {
   autobind_in_progress_ = in_progress;
}

bool ctrlm_obj_controller_rf4ce_t::autobind_in_progress_get(void) {
   return(autobind_in_progress_);
}

void ctrlm_obj_controller_rf4ce_t::binding_button_in_progress_set(bool in_progress) {
   binding_button_in_progress_ = in_progress;
}

bool ctrlm_obj_controller_rf4ce_t::binding_button_in_progress_get(void) {
   return(binding_button_in_progress_);
}

void ctrlm_obj_controller_rf4ce_t::screen_bind_in_progress_set(bool in_progress) {
   screen_bind_in_progress_ = in_progress;
}

bool ctrlm_obj_controller_rf4ce_t::screen_bind_in_progress_get(void) {
   return(screen_bind_in_progress_);
}

ctrlm_rf4ce_controller_type_t ctrlm_obj_controller_rf4ce_t::controller_type_get(void) {
   return(controller_type_);
}

ctrlm_rcu_binding_type_t ctrlm_obj_controller_rf4ce_t::binding_type_get(void) {
   return(binding_type_);
}

void ctrlm_obj_controller_rf4ce_t::time_last_key_get(time_t *time) {
   if(time == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return;
   }
   *time = time_last_key_;
}

void ctrlm_obj_controller_rf4ce_t::time_last_key_update(void) {
   time_last_key_ = time(NULL);
   // TODO need to flush the timestamp to the DB periodically
   if(validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS && time_last_key_ > time_next_flush_) {
      LOG_INFO("%s: Flush last key press to the DB\n", __FUNCTION__);
      ctrlm_db_rf4ce_write_time_last_key(network_id_get(), controller_id_get(), time_last_key_);

      time_next_flush_ = time_last_key_ + LAST_KEY_DATABASE_FLUSH_INTERVAL;
   }
}

void ctrlm_obj_controller_rf4ce_t::time_last_heartbeat_update(void) {
    if(validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) {
      ctrlm_rf4ce_polling_generic_config_t controller_generic_polling_configuration  = obj_network_rf4ce_->controller_generic_polling_configuration_get();
      time_t time_current                                                            = time(NULL);
      gint32 diff                                                                    = 0;
	  
      //Account for boxes with already paired remotes before this code was added
      if(time_last_heartbeat_ == 0) {
         time_last_heartbeat_ = time_current;
      }
      //Only count the time diff if the time is valid (startup time is usually off)...
      if(time_current > time_last_heartbeat_) {
         diff = time_current-time_last_heartbeat_;

         //If this is XR19 or some other voice assistant
         if(ctrlm_is_voice_assistant((ctrlm_rcu_controller_type_t)controller_type_)) {
            //Get the heartbeat polling config
            ctrlm_rf4ce_polling_configuration_t controller_polling_configuration_heartbeat = obj_network_rf4ce_->controller_polling_configuration_heartbeat_get(controller_type_);
            guint32 time_interval                                                          = controller_polling_configuration_heartbeat.time_interval / 1000;

            //Make sure there is a start time for uptime.  Should be same as paired time but may be 0 for XR19's that were paired before this code was written
            if(uptime_privacy_info_.time_uptime_start == 0){
              uptime_privacy_info_.time_uptime_start = time_current;
            }
            //If within the allowed heartbeat threshold, increment the uptime and privacy time if in privacy mode
            if(diff <= (gint32)(time_interval * controller_generic_polling_configuration.uptime_multiplier)) {
               uptime_privacy_info_.uptime_seconds += diff;
               if(privacy_) {
                  uptime_privacy_info_.privacy_time_seconds += diff;
               }
               LOG_DEBUG("%s: Uptime Start Time <%ld>, Uptime in seconds <%lu>, Privacy Time in seconds <%lu>\n", 
               __FUNCTION__, uptime_privacy_info_.time_uptime_start, uptime_privacy_info_.uptime_seconds, uptime_privacy_info_.privacy_time_seconds);
            }
         }
	  
         //Update with the new heartbeat
         time_last_heartbeat_ = time_current;

         //Check if it is time to save the data
         time_since_last_saved_ += diff;
         if(time_since_last_saved_ >= controller_generic_polling_configuration.hb_time_to_save) {
            ctrlm_network_id_t       network_id = network_id_get();
            ctrlm_controller_id_t controller_id = controller_id_get();
            ctrlm_db_rf4ce_write_time_last_heartbeat(network_id, controller_id, time_last_heartbeat_);

            //If this is XR19 or some other voice assistant...
            if(ctrlm_is_voice_assistant((ctrlm_rcu_controller_type_t)controller_type_)) {
               LOG_INFO("%s: Saving to DB: Time Last Heartbeat <%ld>, Uptime Start Time <%ld>, Uptime in seconds <%lu>, Privacy Time in seconds <%lu>\n", 
               __FUNCTION__, time_last_heartbeat_, uptime_privacy_info_.time_uptime_start, uptime_privacy_info_.uptime_seconds, uptime_privacy_info_.privacy_time_seconds);
               ctrlm_db_rf4ce_write_uptime_privacy_info(network_id, controller_id, (guchar *)&uptime_privacy_info_, sizeof(uptime_privacy_info_t)) ;
            } else {
               LOG_INFO("%s: Saving to DB: Time Last Heartbeat <%ld>\n", __FUNCTION__, time_last_heartbeat_);
            }
            time_since_last_saved_ = 0;
         }
      }
   }
}

void ctrlm_obj_controller_rf4ce_t::manual_poll_firmware(void) {
   manual_poll_firmware_ = true;
}

void ctrlm_obj_controller_rf4ce_t::manual_poll_audio_data(void) {
   manual_poll_audio_data_ = true;
}

void ctrlm_obj_controller_rf4ce_t::audio_theme_set(ctrlm_rf4ce_device_update_audio_theme_t audio_theme) {
   if(audio_theme >= RF4CE_DEVICE_UPDATE_AUDIO_THEME_INVALID) {
      LOG_ERROR("%s: Invalid audio theme (%u)\n", __FUNCTION__, audio_theme);
   } else {
      audio_theme_ = audio_theme;
   }
}

void ctrlm_obj_controller_rf4ce_t::process_event_key(ctrlm_key_status_t key_status, ctrlm_key_code_t key_code) {
   time_last_key_update();
   last_key_status_ = key_status;
   last_key_code_   = key_code;
}

void ctrlm_obj_controller_rf4ce_t::rf4ce_controller_status(ctrlm_controller_status_t *status) {
   if(status == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return;
   }

   errno_t safec_rc = -1;

   //If the day has changed, store the values related to today and yesterday
   if(handle_day_change()) {
      property_write_voice_metrics();
   }

   status->ieee_address                                         = ieee_address_;
   status->short_address                                        = short_address_;
   status->time_binding                                         = time_binding_;
   status->binding_type                                         = binding_type_;
   status->validation_type                                      = validation_type_;
   status->security_type                                        = binding_security_type_;
   status->command_count                                        = battery_status_.codes_txd_rf;
   status->link_quality                                         = 0;
   status->link_quality_percent                                 = (status->link_quality * 100) / 255;
   status->time_last_key                                        = time_last_key_;
   status->last_key_status                                      = last_key_status_;
   status->last_key_code                                        = last_key_code_;
   status->voice_cmd_count_today                                = voice_cmd_count_today_;
   status->voice_cmd_count_yesterday                            = voice_cmd_count_yesterday_;
   status->voice_cmd_short_today                                = voice_cmd_short_today_;
   status->voice_cmd_short_yesterday                            = voice_cmd_short_yesterday_;
   status->voice_packets_sent_today                             = voice_packets_sent_today_;
   status->voice_packets_sent_yesterday                         = voice_packets_sent_yesterday_;
   status->voice_packets_lost_today                             = voice_packets_lost_today_;
   status->voice_packets_lost_yesterday                         = voice_packets_lost_yesterday_;
   if(voice_packets_sent_today_>0) {
      status->voice_packet_loss_average_today                   = (float)((float)voice_packets_lost_today_/(float)voice_packets_sent_today_) * 100.0;
   } else {
      status->voice_packet_loss_average_today = 0;
   }
   if(voice_packets_sent_yesterday_>0) {
      status->voice_packet_loss_average_yesterday               = (float)((float)voice_packets_lost_yesterday_/(float)voice_packets_sent_yesterday_) * 100.0;
   } else {
      status->voice_packet_loss_average_yesterday = 0;
   }
   status->utterances_exceeding_packet_loss_threshold_today     = utterances_exceeding_packet_loss_threshold_today_;
   status->utterances_exceeding_packet_loss_threshold_yesterday = utterances_exceeding_packet_loss_threshold_yesterday_;
   status->firmware_updated                                     = firmware_updated_;

   safec_rc = strcpy_s(status->manufacturer, sizeof(status->manufacturer),manufacturer_);
   ERR_CHK(safec_rc);

   safec_rc = strcpy_s(status->chipset, sizeof(status->chipset),chipset_);
   ERR_CHK(safec_rc);

   safec_rc = strncpy_s(status->version_build_id, sizeof(status->version_build_id),version_build_id_.build_id,CTRLM_RCU_VERSION_LENGTH);
   ERR_CHK(safec_rc);
   status->version_build_id[CTRLM_RCU_VERSION_LENGTH-1] = '\0';

   safec_rc = strcpy_s(status->version_dsp_build_id, sizeof(status->version_dsp_build_id),version_dsp_build_id_.build_id);
   ERR_CHK(safec_rc);

   safec_rc = sprintf_s(status->version_software, sizeof(status->version_software), "%u.%u.%u.%u", version_software_.major, version_software_.minor, version_software_.revision, version_software_.patch);
   if(safec_rc < EOK) {
     ERR_CHK(safec_rc);
   }

   safec_rc = sprintf_s(status->version_dsp, sizeof(status->version_dsp), "%u.%u.%u.%u", version_dsp_.major, version_dsp_.minor, version_dsp_.revision, version_dsp_.patch);
   if(safec_rc < EOK) {
     ERR_CHK(safec_rc);
   }

   safec_rc = sprintf_s(status->version_keyword_model, sizeof(status->version_keyword_model), "%u.%u.%u.%u", version_keyword_model_.major, version_keyword_model_.minor, version_keyword_model_.revision, version_keyword_model_.patch);
   if(safec_rc < EOK) {
     ERR_CHK(safec_rc);
   }

   safec_rc = sprintf_s(status->version_arm, sizeof(status->version_arm), "%u.%u.%u.%u", version_arm_.major, version_arm_.minor, version_arm_.revision, version_arm_.patch);
   if(safec_rc < EOK) {
     ERR_CHK(safec_rc);
   }

   safec_rc = sprintf_s(status->version_hardware, sizeof(status->version_hardware), "%u.%u.%u.%u", version_hardware_.manufacturer, version_hardware_.model, version_hardware_.hw_revision, version_hardware_.lot_code);
   if(safec_rc < EOK) {
     ERR_CHK(safec_rc);
   }

   safec_rc = sprintf_s(status->version_irdb, sizeof(status->version_irdb), "%u.%u.%u.%u", version_irdb_.major, version_irdb_.minor, version_irdb_.revision, version_irdb_.patch);
   if(safec_rc < EOK) {
     ERR_CHK(safec_rc);
   }

   safec_rc = sprintf_s(status->version_bootloader, sizeof(status->version_bootloader), "%u.%u.%u.%u", version_bootloader_.major, version_bootloader_.minor, version_bootloader_.revision, version_bootloader_.patch);
   if(safec_rc < EOK) {
     ERR_CHK(safec_rc);
   }

   safec_rc = sprintf_s(status->version_golden, sizeof(status->version_golden), "%u.%u.%u.%u", version_golden_.major, version_golden_.minor, version_golden_.revision, version_golden_.patch);
   if(safec_rc < EOK) {
     ERR_CHK(safec_rc);
   }

   safec_rc = sprintf_s(status->version_audio_data, sizeof(status->version_audio_data), "%u.%u.%u.%u", version_audio_data_.major, version_audio_data_.minor, version_audio_data_.revision, version_audio_data_.patch);
   if(safec_rc < EOK) {
     ERR_CHK(safec_rc);
   }

   safec_rc = strncpy_s(status->type, sizeof(status->type),product_name_, CTRLM_RCU_MAX_USER_STRING_LENGTH-1);
   ERR_CHK(safec_rc);
   status->type[CTRLM_RCU_MAX_USER_STRING_LENGTH - 1] = '\0';

   if(controller_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_TYPE) {
      status->ir_db_type = CTRLM_RCU_IR_DB_TYPE_REMOTEC;
   } else {
      status->ir_db_type = CTRLM_RCU_IR_DB_TYPE_UEI;
   }

   if(controller_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_5_DIGIT_CODE_LOAD_SUPPORTED) {
      status->ir_db_code_download_supported = true;
   } else {
      status->ir_db_code_download_supported = false;
   }

   safec_rc = strcpy_s(status->ir_db_code_tv, sizeof(status->ir_db_code_tv),"00000");
   ERR_CHK(safec_rc);

   safec_rc = strcpy_s(status->ir_db_code_avr, sizeof(status->ir_db_code_avr),"00000");
   ERR_CHK(safec_rc);

   if(controller_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_NO_IR_PROGRAMMED) {
      status->ir_db_state = CTRLM_RCU_IR_DB_STATE_NO_CODES;
   } else if(controller_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_RF_DB) {
      status->ir_db_state = CTRLM_RCU_IR_DB_STATE_IR_RF_DB_CODES;
   } else if((controller_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_TV) && (controller_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR)) {
      status->ir_db_state = CTRLM_RCU_IR_DB_STATE_TV_AVR_CODES;
      safec_rc = strcpy_s(status->ir_db_code_tv, sizeof(status->ir_db_code_tv),controller_irdb_status_.irdb_string_tv);
      ERR_CHK(safec_rc);

      safec_rc = strcpy_s(status->ir_db_code_avr, sizeof(status->ir_db_code_avr),controller_irdb_status_.irdb_string_avr);
      ERR_CHK(safec_rc);
   } else if(controller_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_TV) {
      status->ir_db_state = CTRLM_RCU_IR_DB_STATE_TV_CODE;
      safec_rc = strcpy_s(status->ir_db_code_tv, sizeof(status->ir_db_code_tv),controller_irdb_status_.irdb_string_tv);
      ERR_CHK(safec_rc);
   } else if(controller_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR) {
      status->ir_db_state = CTRLM_RCU_IR_DB_STATE_AVR_CODE;
      safec_rc = strcpy_s(status->ir_db_code_avr, sizeof(status->ir_db_code_avr),controller_irdb_status_.irdb_string_avr);
      ERR_CHK(safec_rc);
   } else {
      status->ir_db_state = CTRLM_RCU_IR_DB_STATE_NO_CODES;
   }

   safec_rc = strcpy_s(status->irdb_entry_id_name_tv, sizeof(status->irdb_entry_id_name_tv), irdb_entry_id_name_tv_.c_str());
   ERR_CHK(safec_rc);

   safec_rc = strcpy_s(status->irdb_entry_id_name_avr, sizeof(status->irdb_entry_id_name_avr), irdb_entry_id_name_avr_.c_str());
   ERR_CHK(safec_rc);

   //Check whether the remote checked in in the last x hours
   time_t now = time(NULL);
   status->checkin_for_device_update = (((now - time_last_checkin_for_device_update_) / RF4CE_CHECKIN_FOR_DEVICE_UPDATE_SECONDS_TO_HOURS) < RF4CE_CHECKIN_FOR_DEVICE_UPDATE_HOURS);

   status->time_last_heartbeat = time_last_heartbeat_;

   //Battery members
   status->has_battery                            = has_battery_;
   status->battery_voltage_loaded                 = (battery_last_good_loaded_voltage_   * 4.0) / 255;
   status->battery_voltage_unloaded               = (battery_last_good_unloaded_voltage_ * 4.0) / 255;
   status->battery_level_percent                  = battery_last_good_percent_;
   get_last_battery_event(status->battery_event, status->time_battery_update);

   status->time_battery_changed                   = battery_milestones_.battery_changed_timestamp;
   status->battery_changed_actual_percentage      = battery_milestones_.battery_changed_actual_percent;
   status->battery_changed_unloaded_voltage       = (battery_changed_unloaded_voltage_   * 4.0) / 255;
   status->time_battery_75_percent                = battery_milestones_.battery_75_percent_timestamp;
   status->battery_75_percent_actual_percentage   = battery_milestones_.battery_75_percent_actual_percent;
   status->battery_75_percent_unloaded_voltage    = (battery_75_percent_unloaded_voltage_   * 4.0) / 255;
   status->time_battery_50_percent                = battery_milestones_.battery_50_percent_timestamp;
   status->battery_50_percent_actual_percentage   = battery_milestones_.battery_50_percent_actual_percent;
   status->battery_50_percent_unloaded_voltage    = (battery_50_percent_unloaded_voltage_   * 4.0) / 255;
   status->time_battery_25_percent                = battery_milestones_.battery_25_percent_timestamp;
   status->battery_25_percent_actual_percentage   = battery_milestones_.battery_25_percent_actual_percent;
   status->battery_25_percent_unloaded_voltage    = (battery_25_percent_unloaded_voltage_   * 4.0) / 255;
   status->time_battery_5_percent                 = battery_milestones_.battery_5_percent_timestamp;
   status->battery_5_percent_actual_percentage    = battery_milestones_.battery_5_percent_actual_percent;
   status->battery_5_percent_unloaded_voltage     = (battery_5_percent_unloaded_voltage_   * 4.0) / 255;
   status->time_battery_0_percent                 = battery_milestones_.battery_0_percent_timestamp;
   status->battery_0_percent_actual_percentage    = battery_milestones_.battery_0_percent_actual_percent;
   status->battery_0_percent_unloaded_voltage     = (battery_0_percent_unloaded_voltage_   * 4.0) / 255;

   status->battery_voltage_large_jump_counter     = battery_voltage_large_jump_counter_;
   status->battery_voltage_large_decline_detected = battery_voltage_large_decline_detected_;

   //DSP members
   status->has_dsp                              = has_dsp_;
   status->average_time_in_privacy_mode         = 0;
   status->in_privacy_mode                      = privacy_ & PRIVACY_FLAGS_ENABLED;
   status->average_snr                          = dsp_metrics_.average_snr;
   status->average_keyword_confidence           = dsp_metrics_.average_keyword_confidence;
   status->total_number_of_mics_working         = dsp_metrics_.total_working_mics;
   status->total_number_of_speakers_working     = dsp_metrics_.total_working_speakers;
   status->end_of_speech_initial_timeout_count  = dsp_metrics_.eos_initial_timeout_count;
   status->end_of_speech_timeout_count          = dsp_metrics_.eos_timeout_count;
   status->time_uptime_start                    = uptime_privacy_info_.time_uptime_start;
   status->uptime_seconds                       = uptime_privacy_info_.uptime_seconds;
   status->privacy_time_seconds                 = uptime_privacy_info_.privacy_time_seconds;

   //reboot members
   status->reboot_reason                        = reboot_reason_;
   status->reboot_voltage                       = reboot_voltage_level_;
   status->reboot_assert_number                 = reboot_assert_number_;
   status->reboot_timestamp                     = reboot_time_;

//   ctrlm_print_controller_status(__FUNCTION__, status);
}

void ctrlm_obj_controller_rf4ce_t::req_data(ctrlm_rf4ce_profile_id_t profile_id, ctrlm_timestamp_t tx_window_start, unsigned char length, guchar *data, ctrlm_hal_rf4ce_data_read_t cb_data_read, void *cb_data_param, bool tx_indirect, bool single_channel) {
   obj_network_rf4ce_->req_data(profile_id, controller_id_get(), tx_window_start, length, data, cb_data_read, cb_data_param, tx_indirect, single_channel);
}

ctrlm_hal_result_t ctrlm_obj_controller_rf4ce_t::network_property_get(ctrlm_hal_network_property_t property, void **value) {
   return(obj_network_rf4ce_->property_get(property, value));
}

ctrlm_hal_result_t ctrlm_obj_controller_rf4ce_t::network_property_set(ctrlm_hal_network_property_t property, void *value) {
   return(obj_network_rf4ce_->property_set(property, value));
}

bool ctrlm_obj_controller_rf4ce_t::is_bound(void) {
   return(stored_in_db_);
}

void ctrlm_obj_controller_rf4ce_t::backup_pairing(void *data) {
   if(pairing_data_ != NULL) {
      ctrlm_hal_free(pairing_data_);
   }

   pairing_data_ = data;
   // Backup previous binding and validation type so they can also be restored
   backup_validation_type_ = validation_type_;
   backup_binding_type_    = binding_type_;
   backup_binding_security_type_ = binding_security_type_;
}

void ctrlm_obj_controller_rf4ce_t::restore_pairing(void) {
   if(pairing_data_ != NULL) {
      ctrlm_hal_network_property_pairing_table_entry_t entry;
      entry.controller_id = controller_id_get();
      entry.pairing_data  = pairing_data_;
      network_property_set(CTRLM_HAL_NETWORK_PROPERTY_PAIRING_TABLE_ENTRY, &entry);

      // Free the pairing data since we no longer need it
      ctrlm_hal_free(pairing_data_);
      pairing_data_ = NULL;

      // Restore previous binding and validation type so they can also be restored
      validation_type_ = backup_validation_type_;
      binding_type_    = backup_binding_type_;
      binding_security_type_ = backup_binding_security_type_;
      validation_result_     = CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS;
   }
}

int ctrlm_obj_controller_rf4ce_t::version_compare(version_software_t first, version_software_t second) {
   guint32 version_first, version_second;

   version_first  = (first.major << 24)  | (first.minor << 16)  | (first.revision << 8)  | (first.patch);
   version_second = (second.major << 24) | (second.minor << 16) | (second.revision << 8) | (second.patch);

   if(version_first > version_second) {
      return(1);
   } else if(version_first < version_second) {
      return(-1);
   }
   return(0);
}

int ctrlm_obj_controller_rf4ce_t::version_compare(version_hardware_t first, version_hardware_t second) {
   if((first.manufacturer != second.manufacturer) || (first.model != second.model)) {
      LOG_WARN("%s: Manufacturer and model don't match. (%u, %u) (%u, %u)\n", __FUNCTION__, first.manufacturer, second.manufacturer, first.model, second.model);
   }

   if(first.hw_revision > second.hw_revision) {
      return(1);
   } else if(first.hw_revision < second.hw_revision) {
      return(-1);
   }
   return(0);
}

void ctrlm_obj_controller_rf4ce_t::db_create() {
   // Create the database entry
   ctrlm_db_rf4ce_controller_create(network_id_get(), controller_id_get());
   stored_in_db_ = true;
}

void ctrlm_obj_controller_rf4ce_t::db_destroy() {
   // Destroy the database entry
   ctrlm_db_rf4ce_controller_destroy(network_id_get(), controller_id_get());
   stored_in_db_ = false;
}

void ctrlm_obj_controller_rf4ce_t::db_load() {
   // Set the loading flag so the entries are not rewritten to the DB
   loading_db_ = true;

   // Load the database entry
   guchar *data = NULL;
   guint32 length;
   ctrlm_network_id_t    network_id    = network_id_get();
   ctrlm_controller_id_t controller_id = controller_id_get();
   
   ctrlm_db_rf4ce_read_ieee_address(network_id, controller_id, &ieee_address_);
   ctrlm_db_rf4ce_read_binding_type(network_id, controller_id, &binding_type_);
   ctrlm_db_rf4ce_read_validation_type(network_id, controller_id, &validation_type_);
   ctrlm_db_rf4ce_read_time_binding(network_id, controller_id, &time_binding_);
   ctrlm_db_rf4ce_read_time_last_key(network_id, controller_id, &time_last_key_);
   ctrlm_db_rf4ce_read_binding_security_type(network_id, controller_id, &binding_security_type_);
#ifdef ASB
   ctrlm_db_rf4ce_read_asb_key_derivation_method(network_id, controller_id, &asb_key_derivation_method_used_);
#endif

   ctrlm_db_rf4ce_read_peripheral_id(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Peripheral Id\n", __FUNCTION__);
   } else {
      property_write_peripheral_id(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }
   
   ctrlm_db_rf4ce_read_rf_statistics(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - RF Statistics\n", __FUNCTION__);
   } else {
      property_write_rf_statistics(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_product_name(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Product Name\n", __FUNCTION__);
      // Set controller type to XR2.  Will turn it to an XR5 based on the software revision below.
      controller_type_ = RF4CE_CONTROLLER_TYPE_XR2;
   } else {
      property_write_product_name(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_version_irdb(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Version IRDB\n", __FUNCTION__);
   } else {
      property_write_version_irdb(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_version_software(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Version Software\n", __FUNCTION__);
   } else {
      property_write_version_software(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }
   
   ctrlm_db_rf4ce_read_version_hardware(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Version Hardware\n", __FUNCTION__);
   } else {
      //For XR11 and XR15, check for valid hardware version needed to be able to do device updates
      if((strncmp(product_name_, "XR11", 4) == 0) || (strncmp(product_name_, "XR15", 4) == 0)) {
         if(!version_hardware_valid(data)) {
            controller_fix_hardware_version_by_mac_address(data);
         }
      }
      property_write_version_hardware(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_version_build_id(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Build ID\n", __FUNCTION__);
   } else {
      property_write_version_build_id(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_version_dsp_build_id(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - DSP Build ID\n", __FUNCTION__);
   } else {
      property_write_version_dsp_build_id(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_battery_status(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Battery Status\n", __FUNCTION__);
   } else {
      property_write_battery_status(data, length);
      ctrlm_db_free(data);
      data = NULL;
      ctrlm_db_rf4ce_read_time_battery_status(network_id, controller_id, &battery_status_.update_time);
   }

   ctrlm_db_rf4ce_read_battery_milestones(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Battery Milestones\n", __FUNCTION__);
   } else {
      property_write_battery_milestones(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }
   //battery_last_good... must be set to the last battery_status_... in case they are not currently in the database.
   battery_last_good_timestamp_         = battery_status_.update_time;
   battery_last_good_percent_           = battery_status_.percent;
   battery_last_good_loaded_voltage_    = battery_status_.voltage_loaded;
   battery_last_good_unloaded_voltage_  = battery_status_.voltage_unloaded;
   //If the battery_last_good_timestamp_ is 0 then we have not written battery info yet
   if(battery_last_good_timestamp_ != 0) {
      battery_first_write_ = false;
   }
   ctrlm_db_rf4ce_read_battery_last_good_timestamp(network_id, controller_id, battery_last_good_timestamp_);
   ctrlm_db_rf4ce_read_battery_last_good_percent(network_id, controller_id, battery_last_good_percent_);
   ctrlm_db_rf4ce_read_battery_last_good_loaded_voltage(network_id, controller_id, battery_last_good_loaded_voltage_);
   ctrlm_db_rf4ce_read_battery_last_good_unloaded_voltage(network_id, controller_id, battery_last_good_unloaded_voltage_);
   ctrlm_db_rf4ce_read_battery_voltage_large_jump_counter(network_id, controller_id, battery_voltage_large_jump_counter_);
   ctrlm_db_rf4ce_read_battery_voltage_large_decline_detected(network_id, controller_id, battery_voltage_large_decline_detected_);
   ctrlm_db_rf4ce_read_battery_changed_unloaded_voltage(network_id, controller_id, battery_changed_unloaded_voltage_);
   ctrlm_db_rf4ce_read_battery_75_percent_unloaded_voltage(network_id, controller_id, battery_75_percent_unloaded_voltage_);
   ctrlm_db_rf4ce_read_battery_50_percent_unloaded_voltage(network_id, controller_id, battery_50_percent_unloaded_voltage_);
   ctrlm_db_rf4ce_read_battery_25_percent_unloaded_voltage(network_id, controller_id, battery_25_percent_unloaded_voltage_);
   ctrlm_db_rf4ce_read_battery_5_percent_unloaded_voltage(network_id, controller_id, battery_5_percent_unloaded_voltage_);
   ctrlm_db_rf4ce_read_battery_0_percent_unloaded_voltage(network_id, controller_id, battery_0_percent_unloaded_voltage_);

   ctrlm_db_rf4ce_read_audio_profiles(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Ctrl Audio profiles\n", __FUNCTION__);
   } else {
      property_write_voice_ctrl_audio_profiles(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }
   
   ctrlm_db_rf4ce_read_voice_statistics(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Voice Statistics\n", __FUNCTION__);
   } else {
      property_write_voice_statistics(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }
   
   ctrlm_db_rf4ce_read_update_version_bootloader(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Update version bootloader\n", __FUNCTION__);
   } else {
      property_write_update_version_bootloader(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }
   
   ctrlm_db_rf4ce_read_update_version_golden(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Update version golden image\n", __FUNCTION__);
   } else {
      property_write_update_version_golden(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }
   
   ctrlm_db_rf4ce_read_update_version_audio_data(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Update version audio data\n", __FUNCTION__);
   } else {
      property_write_update_version_audio_data(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_controller_irdb_status(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - IR Database Status\n", __FUNCTION__);
   } else {
      property_write_controller_irdb_status(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_irdb_entry_id_name_tv(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - IRDB ENTRY ID NAME TV\n", __FUNCTION__);
   } else {
      property_write_irdb_entry_id_name_tv(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }
   
   ctrlm_db_rf4ce_read_irdb_entry_id_name_avr(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - IRDB ENTRY ID NAME AVR\n", __FUNCTION__);
   } else {
      property_write_irdb_entry_id_name_avr(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_voice_metrics(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Voice Metrics\n", __FUNCTION__);
   } else {
      property_write_voice_metrics(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_firmware_updated(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Firmware Updated\n", __FUNCTION__);
   } else {
      property_write_firmware_updated(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_reboot_diagnostics(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Reboot Diagnostics\n", __FUNCTION__);
   } else {
      property_write_reboot_diagnostics(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_memory_statistics(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Memory Statistics\n", __FUNCTION__);
   } else {
      property_write_memory_statistics(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_time_last_checkin_for_device_update(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Time Last Checkin for Device Update\n", __FUNCTION__);
   } else {
      property_write_time_last_checkin_for_device_update(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_polling_configuration_heartbeat(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Heartbeat Polling Configuration\n", __FUNCTION__);
   } else {
      property_write_polling_configuration_heartbeat(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_polling_configuration_mac(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Mac Polling Configuration\n", __FUNCTION__);
   } else {
      property_write_polling_configuration_mac(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_version_dsp(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Version Dsp\n", __FUNCTION__);
   } else {
      property_write_version_dsp(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_version_keyword_model(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Version Keyword Model\n", __FUNCTION__);
   } else {
      property_write_version_keyword_model(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_version_arm(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Version ARM\n", __FUNCTION__);
   } else {
      property_write_version_arm(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_privacy(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Privacy\n", __FUNCTION__);
   } else {
      property_write_privacy(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_controller_capabilities(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Controller Capabilities\n", __FUNCTION__);
   } else {
      property_write_controller_capabilities(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_far_field_metrics(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Field Metrics\n", __FUNCTION__);
   } else {
      property_write_far_field_metrics(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_dsp_metrics(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - DSP Metrics\n", __FUNCTION__);
   } else {
      property_write_dsp_metrics(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }
 
   ctrlm_db_rf4ce_read_uptime_privacy_info(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Uptime Privacy Info\n", __FUNCTION__);
   } else {
      property_write_uptime_privacy_info(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_time_metrics(network_id, controller_id, &time_metrics_);

   ctrlm_db_rf4ce_read_polling_methods(network_id, controller_id, &polling_methods_);
   ctrlm_db_rf4ce_read_rib_configuration_complete(network_id, controller_id, (int *)&rib_configuration_complete_status_);
   ctrlm_db_rf4ce_read_time_last_heartbeat(network_id, controller_id, &time_last_heartbeat_);

   ctrlm_db_rf4ce_read_mfg_test_result(network_id, controller_id, &data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Mfg Test Result\n", __FUNCTION__);
   } else {
      property_write_mfg_test_result(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   ctrlm_db_rf4ce_read_ota_failures_count(network_id, controller_id, &data, &length);
   if (data == NULL) {
      LOG_WARN("%s: Not read from DB - OTA failure count\n", __FUNCTION__);
   } else {
      ota_failures_ = (uint8_t) data[0];
      ctrlm_db_free(data);
      data = NULL;
   }

   // Clear the loading flag so subsequent updates are rewritten to the DB
   loading_db_ = false;

   // Indicate that the controller exists in the DB
   stored_in_db_ = true;
}

void ctrlm_obj_controller_rf4ce_t::db_store() {
   // Store the database entries
   guchar data[CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE];
   size_t length = 0;
   ctrlm_network_id_t    network_id    = network_id_get();
   ctrlm_controller_id_t controller_id = controller_id_get();

   LOG_INFO("%s: controller id %u ieee address 0x%016llX\n", __FUNCTION__, controller_id, ieee_address_);

   ctrlm_db_rf4ce_write_ieee_address(network_id, controller_id, ieee_address_);
   ctrlm_db_rf4ce_write_binding_type(network_id, controller_id, binding_type_);
   ctrlm_db_rf4ce_write_validation_type(network_id, controller_id, validation_type_);
   ctrlm_db_rf4ce_write_time_binding(network_id, controller_id, time_binding_);
   ctrlm_db_rf4ce_write_time_last_key(network_id, controller_id, time_last_key_);
   ctrlm_db_rf4ce_write_binding_security_type(network_id, controller_id, binding_security_type_);
#ifdef ASB
   ctrlm_db_rf4ce_write_asb_key_derivation_method(network_id, controller_id, asb_key_derivation_method_used_);
#endif

   if(CTRLM_RF4CE_RIB_ATTR_LEN_PERIPHERAL_ID              == property_read_peripheral_id(data, CTRLM_RF4CE_RIB_ATTR_LEN_PERIPHERAL_ID)) {
      ctrlm_db_rf4ce_write_peripheral_id(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_PERIPHERAL_ID);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_RF_STATISTICS              == property_read_rf_statistics(data, CTRLM_RF4CE_RIB_ATTR_LEN_RF_STATISTICS)) {
      ctrlm_db_rf4ce_write_rf_statistics(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_RF_STATISTICS);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING                 == property_read_version_irdb(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)) {
      ctrlm_db_rf4ce_write_version_irdb(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
      obj_network_rf4ce_->req_process_rib_export(controller_id, CTRLM_HAL_RF4CE_RIB_ATTR_ID_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_IRDB, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING, data);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING                 == property_read_version_hardware(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)) {
      ctrlm_db_rf4ce_write_version_hardware(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
      obj_network_rf4ce_->req_process_rib_export(controller_id, CTRLM_HAL_RF4CE_RIB_ATTR_ID_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_HARDWARE, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING, data);   
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING                 == property_read_version_software(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)) {
      ctrlm_db_rf4ce_write_version_software(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
      obj_network_rf4ce_->req_process_rib_export(controller_id, CTRLM_HAL_RF4CE_RIB_ATTR_ID_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_SOFTWARE, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING, data);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_BUILD_ID        == property_read_version_build_id(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_BUILD_ID)) {
      ctrlm_db_rf4ce_write_version_build_id(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_BUILD_ID);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_DSP_BUILD_ID    == property_read_version_dsp_build_id(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_DSP_BUILD_ID)) {
      ctrlm_db_rf4ce_write_version_dsp_build_id(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_DSP_BUILD_ID);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS             == property_read_battery_status(data, CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS)) {
      ctrlm_db_rf4ce_write_battery_status(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS);
      ctrlm_db_rf4ce_write_time_battery_status(network_id, controller_id, battery_status_.update_time);
      obj_network_rf4ce_->req_process_rib_export(controller_id, CTRLM_HAL_RF4CE_RIB_ATTR_ID_BATTERY_STATUS, CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL, CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS, data);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES  == property_read_voice_ctrl_audio_profiles(data, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES)) {
      ctrlm_db_rf4ce_write_audio_profiles(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES);
      obj_network_rf4ce_->req_process_rib_export(controller_id, CTRLM_HAL_RF4CE_RIB_ATTR_ID_VOICE_CTRL_AUDIO_PROFILES, CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES, data);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS           == property_read_voice_statistics(data, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS)) {
      ctrlm_db_rf4ce_write_voice_statistics(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS);
      obj_network_rf4ce_->req_process_rib_export(controller_id, CTRLM_HAL_RF4CE_RIB_ATTR_ID_VOICE_STATISTICS, CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS, data);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING          == property_read_update_version_bootloader(data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING)) {
      ctrlm_db_rf4ce_write_update_version_bootloader(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
      obj_network_rf4ce_->req_process_rib_export(controller_id, CTRLM_HAL_RF4CE_RIB_ATTR_ID_UPDATE_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_UPDATE_VERSIONING_BOOTLOADER, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING, data);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING          == property_read_update_version_golden(data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING)) {
      ctrlm_db_rf4ce_write_update_version_golden(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
      obj_network_rf4ce_->req_process_rib_export(controller_id, CTRLM_HAL_RF4CE_RIB_ATTR_ID_UPDATE_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_UPDATE_VERSIONING_GOLDEN, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING, data);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING          == property_read_update_version_audio_data(data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING)) {
      ctrlm_db_rf4ce_write_update_version_audio_data(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
      obj_network_rf4ce_->req_process_rib_export(controller_id, CTRLM_HAL_RF4CE_RIB_ATTR_ID_UPDATE_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_UPDATE_VERSIONING_AUDIO_DATA, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING, data);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME               == property_read_product_name(data, CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME)) {
      ctrlm_db_rf4ce_write_product_name(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME);
      obj_network_rf4ce_->req_process_rib_export(controller_id, CTRLM_HAL_RF4CE_RIB_ATTR_ID_PRODUCT_NAME, CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL, CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME, data);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS     == property_read_controller_irdb_status(data, CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS)) {
      ctrlm_db_rf4ce_write_controller_irdb_status(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS);
   }

   length = property_read_irdb_entry_id_name_tv(data, CTRLM_RF4CE_RIB_ATTR_LEN_IRDB_ENTRY_ID_NAME);
   if(length > 0) {
      ctrlm_db_rf4ce_write_irdb_entry_id_name_tv(network_id, controller_id, data, length);
   }

   length = property_read_irdb_entry_id_name_avr(data, CTRLM_RF4CE_RIB_ATTR_LEN_IRDB_ENTRY_ID_NAME);
   if(length > 0) {
      ctrlm_db_rf4ce_write_irdb_entry_id_name_avr(network_id, controller_id, data, length);
   }

   if(CTRLM_RF4CE_LEN_VOICE_METRICS                       == property_read_voice_metrics(data, CTRLM_RF4CE_LEN_VOICE_METRICS)) {
      ctrlm_db_rf4ce_write_voice_metrics(network_id, controller_id, data, CTRLM_RF4CE_LEN_VOICE_METRICS);
   }

   if(CTRLM_RF4CE_LEN_FIRMWARE_UPDATED                    == property_read_firmware_updated(data, CTRLM_RF4CE_LEN_FIRMWARE_UPDATED)) {
      ctrlm_db_rf4ce_write_firmware_updated(network_id, controller_id, data, CTRLM_RF4CE_LEN_FIRMWARE_UPDATED);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_DIAGNOSTICS         == property_read_reboot_diagnostics(data, CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_DIAGNOSTICS)) {
      ctrlm_db_rf4ce_write_reboot_diagnostics(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_DIAGNOSTICS);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATISTICS          == property_read_memory_statistics(data, CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATISTICS)) {
      ctrlm_db_rf4ce_write_memory_statistics(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATISTICS);
   }

   if(CTRLM_RF4CE_LEN_TIME_LAST_CHECKIN_FOR_DEVICE_UPDATE == property_read_time_last_checkin_for_device_update(data, CTRLM_RF4CE_LEN_TIME_LAST_CHECKIN_FOR_DEVICE_UPDATE)) {
      ctrlm_db_rf4ce_write_time_last_checkin_for_device_update(network_id, controller_id, data, CTRLM_RF4CE_LEN_TIME_LAST_CHECKIN_FOR_DEVICE_UPDATE);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS == property_read_polling_methods(data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS)) {
      ctrlm_db_rf4ce_write_polling_methods(network_id, controller_id, (guint8)data[0]);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION == property_read_polling_configuration_heartbeat(data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION)) {
      ctrlm_db_rf4ce_write_polling_configuration_heartbeat(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION == property_read_polling_configuration_mac(data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION)) {
      ctrlm_db_rf4ce_write_polling_configuration_mac(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_PRIVACY == property_read_privacy(data, CTRLM_RF4CE_RIB_ATTR_LEN_PRIVACY)) {
      ctrlm_db_rf4ce_write_privacy(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_PRIVACY);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_CAPABILITIES == property_read_controller_capabilities(data, CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_CAPABILITIES)) {
      ctrlm_db_rf4ce_write_controller_capabilities(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_CAPABILITIES);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_METRICS == property_read_far_field_metrics(data, CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_METRICS)) {
      ctrlm_db_rf4ce_write_far_field_metrics(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_METRICS);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_DSP_METRICS == property_read_dsp_metrics(data, CTRLM_RF4CE_RIB_ATTR_LEN_DSP_METRICS)) {
      ctrlm_db_rf4ce_write_dsp_metrics(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_DSP_METRICS);
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_UPTIME_PRIVACY_INFO == property_read_uptime_privacy_info(data, CTRLM_RF4CE_RIB_ATTR_LEN_UPTIME_PRIVACY_INFO)) {
      ctrlm_db_rf4ce_write_uptime_privacy_info(network_id, controller_id, (guchar *)&uptime_privacy_info_, sizeof(uptime_privacy_info_t)) ;
   }

   ctrlm_db_rf4ce_write_time_metrics(network_id, controller_id, time_metrics_);

   ctrlm_db_rf4ce_write_rib_configuration_complete(network_id, controller_id, (int)rib_configuration_complete_status_);
   ctrlm_db_rf4ce_write_time_last_heartbeat(network_id, controller_id, time_last_heartbeat_);

   if(CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_RESULT == property_read_mfg_test_result(data, CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_RESULT)) {
      ctrlm_db_rf4ce_write_mfg_test_result(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_RESULT);
   }
}

void ctrlm_obj_controller_rf4ce_t::validation_result_set(ctrlm_rcu_binding_type_t binding_type, ctrlm_rcu_validation_type_t validation_type, ctrlm_rf4ce_result_validation_t result, time_t time_binding, time_t time_last_key) {

   if(validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_PENDING && result == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) {
      // Result changed from pending to success
      LOG_INFO("%s: Create and store DB entries\n", __FUNCTION__);

      if(time_binding == 0) {
         time_binding_  = time(NULL);
      } else {
         time_binding_  = time_binding;
      }
      if(time_last_key == 0) {
         time_last_key_ = time_binding_;
      } else {
         time_last_key_ = time_last_key;
      }
      time_metrics_ = time_binding_;
 
      //reset uptime metrics
      uptime_privacy_info_.time_uptime_start    = time_binding_;
      uptime_privacy_info_.uptime_seconds       = 0;
      uptime_privacy_info_.privacy_time_seconds = 0;
      time_since_last_saved_                    = 0;
      time_last_heartbeat_                      = uptime_privacy_info_.time_uptime_start;
      battery_first_write_                      = true;

      binding_type_     = binding_type;
      validation_type_  = validation_type;
      db_create();
      db_store();
      // HACK for XR15-704, possible duplicate pairing
#ifdef XR15_704
      needs_reset_ = false;
      did_reset_   = false;
#endif
      // HACK for XR15-704

      // Telemetry needs to keep track of binding.  
      log_binding_for_telemetry();

      // Update Polling Configuration
      update_polling_configurations(false);

      // ROBIN -- Check Target IRDB status
      if(RF4CE_CONTROLLER_TYPE_XR19 == controller_type_) {
         push_ir_codes();
      }
   }
   if(result == CTRLM_RF4CE_RESULT_VALIDATION_PENDING) {
      // Validation set back to pending due to repairing a controller.  Change configuration back to pending.
      configuration_result_ = CTRLM_RCU_CONFIGURATION_RESULT_PENDING;
   }

   validation_result_ = result;
}

ctrlm_rf4ce_result_validation_t ctrlm_obj_controller_rf4ce_t::validation_result_get() {
   return(validation_result_);
}

ctrlm_rcu_configuration_result_t ctrlm_obj_controller_rf4ce_t::configuration_result_get(void) {
   return(configuration_result_);
}

void ctrlm_obj_controller_rf4ce_t::property_write_peripheral_id() {

   //TODO
   //peripheral_id_

   //if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
   //   ctrlm_db_rf4ce_write_peripheral_id(network_id_get(), controller_id_get(), data, length);
   //}

   LOG_INFO("%s: NOT IMPLEMENTED\n", __FUNCTION__);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_peripheral_id(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_PERIPHERAL_ID) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   //TODO
   //peripheral_id_
   
   //if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
   //   ctrlm_db_rf4ce_write_peripheral_id(network_id_get(), controller_id_get(), data, length);
   //}
   
   LOG_INFO("%s: NOT IMPLEMENTED\n", __FUNCTION__);
   return(0);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_peripheral_id(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_PERIPHERAL_ID) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   LOG_ERROR("%s: NOT IMPLEMENTED\n", __FUNCTION__);
   return(0);
}

void ctrlm_obj_controller_rf4ce_t::property_write_rf_statistics() {
   //TODO
   //rf_statistics_

   //if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
   //   ctrlm_db_rf4ce_write_rf_statistics(network_id_get(), controller_id_get(), data, length);
   //}

   LOG_INFO("%s: NOT IMPLEMENTED\n", __FUNCTION__);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_rf_statistics(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_RF_STATISTICS) {
      LOG_ERROR("%s: RF Statistics - INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   //TODO
   //rf_statistics_
   
   //if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
   //   ctrlm_db_rf4ce_write_rf_statistics(network_id_get(), controller_id_get(), data, length);
   //}
   
   LOG_INFO("%s: NOT IMPLEMENTED\n", __FUNCTION__);
   return(0);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_rf_statistics(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_RF_STATISTICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   LOG_ERROR("%s: NOT IMPLEMENTED\n", __FUNCTION__);
   return(0);
}

void ctrlm_obj_controller_rf4ce_t::property_write_version_irdb(guchar major, guchar minor, guchar revision, guchar patch) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING];
   data[0] = major;
   data[1] = minor;
   data[2] = revision;
   data[3] = patch;

   property_write_version_irdb(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_version_irdb(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   if(version_irdb_.major    != data[0] ||
      version_irdb_.minor    != data[1] ||
      version_irdb_.revision != data[2] ||
      version_irdb_.patch    != data[3]) {
      // Store the data on the controller object
      version_irdb_.major    = data[0];
      version_irdb_.minor    = data[1];
      version_irdb_.revision = data[2];
      version_irdb_.patch    = data[3];
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_version_irdb(network_id_get(), controller_id_get(), data, length);
         obj_network_rf4ce_->req_process_rib_export(controller_id_get(), CTRLM_HAL_RF4CE_RIB_ATTR_ID_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_IRDB, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING, data);
      }
   }

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_irdb_.major, version_irdb_.minor, version_irdb_.revision, version_irdb_.patch);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_version_irdb(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0] = version_irdb_.major;
   data[1] = version_irdb_.minor;
   data[2] = version_irdb_.revision;
   data[3] = version_irdb_.patch;

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_irdb_.major, version_irdb_.minor, version_irdb_.revision, version_irdb_.patch);
   return(CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
}

void ctrlm_obj_controller_rf4ce_t::property_write_version_hardware(guchar manufacturer, guchar model, guchar hw_revision, guint16 lot_code) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING];
   data[0] = (manufacturer << 4) | (model & 0xF);
   data[1] = hw_revision;
   data[2] = (lot_code >> 8) & 0xF;
   data[3] = (guchar)lot_code;

   property_write_version_hardware(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_version_hardware(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   guchar  manufacturer = (data[0] >> 4);
   guchar  model        = (data[0] & 0xF);
   guchar  hw_revision  = data[1];
   guint16 lot_code     = ((data[2] & 0xF) << 8) | data[3];
   
   if(version_hardware_.manufacturer != manufacturer ||
      version_hardware_.model        != model        ||
      version_hardware_.hw_revision  != hw_revision  ||
      version_hardware_.lot_code     != lot_code) {
      // Store the data on the controller object
      version_hardware_.manufacturer = manufacturer;
      version_hardware_.model        = model;
      version_hardware_.hw_revision  = hw_revision;
      version_hardware_.lot_code     = lot_code;
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_version_hardware(network_id_get(), controller_id_get(), data, length);
         obj_network_rf4ce_->req_process_rib_export(controller_id_get(), CTRLM_HAL_RF4CE_RIB_ATTR_ID_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_HARDWARE, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING, data);
      }
   }
   //Set the manufacturer string from version_hardware
   errno_t safec_rc = strcpy_s(manufacturer_, sizeof(manufacturer_),ctrlm_rf4ce_controller_manufacturer(version_hardware_.manufacturer));
   ERR_CHK(safec_rc);

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_hardware_.manufacturer, version_hardware_.model, version_hardware_.hw_revision, version_hardware_.lot_code);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_version_hardware(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0] = (version_hardware_.manufacturer << 4) | (version_hardware_.model & 0xF);
   data[1] = version_hardware_.hw_revision;
   data[2] = version_hardware_.lot_code >> 8;
   data[3] = (char)version_hardware_.lot_code;

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_hardware_.manufacturer, version_hardware_.model, version_hardware_.hw_revision, version_hardware_.lot_code);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
}

void ctrlm_obj_controller_rf4ce_t::property_write_version_software(guchar major, guchar minor, guchar revision, guchar patch) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING];
   data[0] = major;
   data[1] = minor;
   data[2] = revision;
   data[3] = patch;
   property_write_version_software(data, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_version_software(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   if(version_software_.major    != data[0] ||
      version_software_.minor    != data[1] ||
      version_software_.revision != data[2] ||
      version_software_.patch    != data[3]) {
      // Store the data on the controller object
      version_software_.major    = data[0];
      version_software_.minor    = data[1];
      version_software_.revision = data[2];
      version_software_.patch    = data[3];

      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_version_software(network_id_get(), controller_id_get(), data, length);
         obj_network_rf4ce_->req_process_rib_export(controller_id_get(), CTRLM_HAL_RF4CE_RIB_ATTR_ID_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_VERSIONING_SOFTWARE, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING, data);
      }
   }

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_software_.major, version_software_.minor, version_software_.revision, version_software_.patch);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_version_software(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0] = version_software_.major;
   data[1] = version_software_.minor;
   data[2] = version_software_.revision;
   data[3] = version_software_.patch;

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_software_.major, version_software_.minor, version_software_.revision, version_software_.patch);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_version_dsp(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   if(version_dsp_.major    != data[0] ||
      version_dsp_.minor    != data[1] ||
      version_dsp_.revision != data[2] ||
      version_dsp_.patch    != data[3]) {
      // Store the data on the controller object
      version_dsp_.major    = data[0];
      version_dsp_.minor    = data[1];
      version_dsp_.revision = data[2];
      version_dsp_.patch    = data[3];

      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_version_dsp(network_id_get(), controller_id_get(), data, length);
      }
   }

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_dsp_.major, version_dsp_.minor, version_dsp_.revision, version_dsp_.patch);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_version_dsp(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0] = version_dsp_.major;
   data[1] = version_dsp_.minor;
   data[2] = version_dsp_.revision;
   data[3] = version_dsp_.patch;

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_dsp_.major, version_dsp_.minor, version_dsp_.revision, version_dsp_.patch);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_version_keyword_model(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   if(version_keyword_model_.major    != data[0] ||
      version_keyword_model_.minor    != data[1] ||
      version_keyword_model_.revision != data[2] ||
      version_keyword_model_.patch    != data[3]) {
      // Store the data on the controller object
      version_keyword_model_.major    = data[0];
      version_keyword_model_.minor    = data[1];
      version_keyword_model_.revision = data[2];
      version_keyword_model_.patch    = data[3];

      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_version_keyword_model(network_id_get(), controller_id_get(), data, length);
      }
   }

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_keyword_model_.major, version_keyword_model_.minor, version_keyword_model_.revision, version_keyword_model_.patch);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_version_keyword_model(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0] = version_keyword_model_.major;
   data[1] = version_keyword_model_.minor;
   data[2] = version_keyword_model_.revision;
   data[3] = version_keyword_model_.patch;

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_keyword_model_.major, version_keyword_model_.minor, version_keyword_model_.revision, version_keyword_model_.patch);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_version_arm(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   if(version_arm_.major    != data[0] ||
      version_arm_.minor    != data[1] ||
      version_arm_.revision != data[2] ||
      version_arm_.patch    != data[3]) {
      // Store the data on the controller object
      version_arm_.major    = data[0];
      version_arm_.minor    = data[1];
      version_arm_.revision = data[2];
      version_arm_.patch    = data[3];

      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_version_arm(network_id_get(), controller_id_get(), data, length);
      }
   }

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_arm_.major, version_arm_.minor, version_arm_.revision, version_arm_.patch);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_version_arm(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   // Load the data from the controller object
   data[0] = version_arm_.major;
   data[1] = version_arm_.minor;
   data[2] = version_arm_.revision;
   data[3] = version_arm_.patch;

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_arm_.major, version_arm_.minor, version_arm_.revision, version_arm_.patch);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_version_build_id(guchar *data, guchar length) {
   if(data == NULL || length > CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_BUILD_ID) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = -1;
   int ind = -1;
   bool IsValid = false;

   (length != version_build_id_.length)? (IsValid= true): (safec_rc = memcmp_s(version_build_id_.build_id, sizeof(version_build_id_.build_id), data, length, &ind));
   if((IsValid) || ((safec_rc == EOK) && (ind != 0))) {
      // Store the data on the controller object
      safec_rc = memcpy_s(version_build_id_.build_id, sizeof(version_build_id_.build_id), data, length);
      ERR_CHK(safec_rc);
      version_build_id_.length = length;

      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_version_build_id(network_id_get(), controller_id_get(), data, length);
      }
   } else if (safec_rc != EOK) {
     ERR_CHK(safec_rc);
   }

   LOG_INFO("%s: %.*s\n", __FUNCTION__, version_build_id_.length, version_build_id_.build_id);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_version_build_id(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_BUILD_ID) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   // Load the data from the controller object
   errno_t safec_rc = memcpy_s(data, CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE, version_build_id_.build_id, version_build_id_.length);
   ERR_CHK(safec_rc);

   LOG_INFO("%s: %.*s\n", __FUNCTION__, version_build_id_.length, version_build_id_.build_id);

   return(version_build_id_.length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_version_dsp_build_id(guchar *data, guchar length) {
   if(data == NULL || length > CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_DSP_BUILD_ID) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   errno_t safec_rc = -1;
   int ind = -1;
   bool IsValid = false;

   (length != version_dsp_build_id_.length)? (IsValid= true): (safec_rc = memcmp_s(version_dsp_build_id_.build_id, sizeof(version_dsp_build_id_.build_id), data, length, &ind));
   if((IsValid) || ((safec_rc == EOK) && (ind != 0))) {
      // Store the data on the controller object
      safec_rc =  memset_s(version_dsp_build_id_.build_id, sizeof(version_dsp_build_id_.build_id), 0, CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_DSP_BUILD_ID);
      ERR_CHK(safec_rc);
      safec_rc = memcpy_s(version_dsp_build_id_.build_id, sizeof(version_dsp_build_id_.build_id), data, length);
      ERR_CHK(safec_rc);
      version_dsp_build_id_.length = length;

      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_version_dsp_build_id(network_id_get(), controller_id_get(), data, length);
      }
   }
   else if(safec_rc != EOK) {
     ERR_CHK(safec_rc);
   }

   LOG_INFO("%s: %.*s\n", __FUNCTION__, version_dsp_build_id_.length, version_dsp_build_id_.build_id);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_version_dsp_build_id(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING_DSP_BUILD_ID) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   // Load the data from the controller object
   errno_t safec_rc = memcpy_s(data, CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE, version_dsp_build_id_.build_id, version_dsp_build_id_.length);
   ERR_CHK(safec_rc);

   LOG_INFO("%s: %.*s\n", __FUNCTION__, version_dsp_build_id_.length, version_dsp_build_id_.build_id);

   return(version_dsp_build_id_.length);
}


void ctrlm_obj_controller_rf4ce_t::property_write_battery_status(guchar flags, guchar voltage_loaded, guint32 codes_txd_rf, guint32 codes_txd_ir, guchar voltage_unloaded) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS];
   data[0]  = flags;
   data[1]  = voltage_loaded;
   data[2]  = (guchar)(codes_txd_rf);
   data[3]  = (guchar)(codes_txd_rf >> 8);
   data[4]  = (guchar)(codes_txd_rf >> 16);
   data[5]  = (guchar)(codes_txd_rf >> 24);
   data[6]  = (guchar)(codes_txd_ir);
   data[7]  = (guchar)(codes_txd_ir >> 8);
   data[8]  = (guchar)(codes_txd_ir >> 16);
   data[9]  = (guchar)(codes_txd_ir >> 24);
   data[10] = voltage_unloaded;

   property_write_battery_status(data, CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_battery_status(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   bool    update_milestones = true;
   bool    batteries_changed = false;
   guchar  flags             = data[0];
   guchar  voltage_loaded    = data[1];
   guint32 codes_txd_rf      = ((data[5] << 24) | (data[4] << 16) | (data[3] << 8) | data[2]);
   guint32 codes_txd_ir      = ((data[9] << 24) | (data[8] << 16) | (data[7] << 8) | data[6]);
   guchar  voltage_unloaded  = data[10];

   if(flags) {
      //We ignore the flags but log them if any are set
      LOG_WARN("%s: Flags 0x%02X Voltage (old:%4.2fv, new:%4.2fv) Battery Replacement <%u>, Battery Charging <%u>, Impending Doom <%u>\n", 
	  __FUNCTION__, flags, battery_status_.voltage_unloaded * 4.0 / 255, voltage_unloaded * 4.0 / 255, 
	  (flags & BATTERY_STATUS_FLAGS_REPLACEMENT), (flags & BATTERY_STATUS_FLAGS_CHARGING), (flags & BATTERY_STATUS_FLAGS_IMPENDING_DOOM));
   }

   if(!loading_db_) {
      //If this is the first battery status write, this could be a newly paired remote so treat this as batteries changed
      if(battery_first_write_) {
         LOG_WARN("%s: This is the first battery status write so this could be a newly paired remote.  Voltage (%4.2fv)\n", __FUNCTION__, voltage_unloaded * 4.0 / 255);
         batteries_changed = true;
         battery_first_write_ = false;
      } else if((voltage_unloaded > battery_last_good_unloaded_voltage_) || (voltage_loaded > battery_last_good_loaded_voltage_)) {
         if(is_batteries_changed(voltage_unloaded)) {
            LOG_WARN("%s: It appears the batteries have been changed. Voltage (old:%4.2fv, new:%4.2fv)\n", __FUNCTION__, battery_status_.voltage_unloaded * 4.0 / 255, voltage_unloaded * 4.0 / 255);
            batteries_changed = true;
         } else if(is_batteries_large_voltage_jump(voltage_unloaded)) {
            LOG_WARN("%s: The battery voltage haa a large increase but we don't consider the batteries to have changed.  Milestones not updated.  Voltage (old:%4.2fv, new:%4.2fv)\n", __FUNCTION__, battery_status_.voltage_unloaded * 4.0 / 255, voltage_unloaded * 4.0 / 255);
            update_milestones = false;
            battery_voltage_large_jump_counter_++;
            ctrlm_db_rf4ce_write_battery_voltage_large_jump_counter(network_id_get(), controller_id_get(), battery_voltage_large_jump_counter_);
         } else if(((voltage_unloaded < battery_status_.voltage_unloaded) && (battery_status_.voltage_unloaded > battery_last_good_unloaded_voltage_)) ||
                   ((voltage_loaded < battery_status_.voltage_loaded) && (battery_status_.voltage_loaded > battery_last_good_loaded_voltage_))) {
            LOG_WARN("%s: The battery voltage went up previously, has now gone down, but not to it's lowest point.  Milestones not updated.\n", __FUNCTION__);
            update_milestones = false;
         } else {
            LOG_WARN("%s: The battery voltage has gone up but we don't consider the batteries to have changed.  Milestones not updated.  Unloaded Voltage (old:%4.2fv, new:%4.2fv)  Loaded Voltage (old:%4.2fv, new:%4.2fv)\n", __FUNCTION__, battery_last_good_unloaded_voltage_ * 4.0 / 255, voltage_unloaded * 4.0 / 255, battery_last_good_loaded_voltage_ * 4.0 / 255, voltage_loaded * 4.0 / 255);
            update_milestones = false;
         }
      }
   }
   
   if(battery_status_.flags            != flags          ||
      battery_status_.voltage_loaded   != voltage_loaded ||
      battery_status_.codes_txd_rf     != codes_txd_rf   ||
      battery_status_.codes_txd_ir     != codes_txd_ir   ||
      battery_status_.voltage_unloaded != voltage_unloaded) {
      // Store the data on the controller object
      battery_status_.update_time      = time(NULL);
      battery_status_.flags            = flags;
      battery_status_.voltage_loaded   = voltage_loaded;
      battery_status_.codes_txd_rf     = codes_txd_rf;
      battery_status_.codes_txd_ir     = codes_txd_ir;
      battery_status_.voltage_unloaded = voltage_unloaded;
      battery_status_.percent          = battery_level_percent(battery_status_.voltage_unloaded);
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_battery_status(network_id_get(), controller_id_get(), data, length);
         ctrlm_db_rf4ce_write_time_battery_status(network_id_get(), controller_id_get(), battery_status_.update_time);
         if(update_milestones) {
            property_write_battery_milestones(batteries_changed, voltage_loaded, voltage_unloaded, battery_status_.percent, battery_status_.update_time);
         }
         obj_network_rf4ce_->req_process_rib_export(controller_id_get(), CTRLM_HAL_RF4CE_RIB_ATTR_ID_BATTERY_STATUS, CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL, CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS, data);
      }
   }

   LOG_INFO("%s: Flags 0x%02X Voltage (%4.2fv, %4.2fv) Codes Txd (RF,IR) (%u,%u)\n", __FUNCTION__, battery_status_.flags, battery_status_.voltage_loaded * 4.0 / 255, battery_status_.voltage_unloaded * 4.0 / 255, battery_status_.codes_txd_rf, battery_status_.codes_txd_ir);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_battery_milestones(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_MILESTONES) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   errno_t safec_rc = memcpy_s(&battery_milestones_, sizeof(battery_milestones_), data, CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_MILESTONES);
   ERR_CHK(safec_rc);
   LOG_INFO("%s: battery_changed <%ld>, <%d%%> battery_75_percent_ <%ld>, <%d%%> battery_50_percent_ <%ld>, <%d%%> battery_25_percent_ <%ld>, <%d%%> battery_5_percent_ <%ld>, <%d%%> battery_0_percent_ <%ld>, <%d%%> \n",
   __FUNCTION__, battery_milestones_.battery_changed_timestamp, battery_milestones_.battery_changed_actual_percent, battery_milestones_.battery_75_percent_timestamp, battery_milestones_.battery_75_percent_actual_percent, battery_milestones_.battery_50_percent_timestamp, battery_milestones_.battery_50_percent_actual_percent,
   battery_milestones_.battery_25_percent_timestamp, battery_milestones_.battery_25_percent_actual_percent, battery_milestones_.battery_5_percent_timestamp, battery_milestones_.battery_5_percent_actual_percent, battery_milestones_.battery_0_percent_timestamp, battery_milestones_.battery_0_percent_actual_percent);
   return(length);
}
void ctrlm_obj_controller_rf4ce_t::property_write_battery_milestones(bool batteries_changed, guchar voltage_loaded, guchar voltage_unloaded, guchar percent, time_t timestamp) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_MILESTONES];
   ctrlm_rcu_battery_event_t battery_event = CTRLM_RCU_BATTERY_EVENT_NONE;
   ctrlm_network_id_t network_id           = network_id_get();
   ctrlm_controller_id_t controller_id     = controller_id_get();
   guchar threshold_counter                = 0;
   errno_t safec_rc                        = -1;

   if(!loading_db_) { // send the event
      gboolean send_event = false;
      if(batteries_changed) {
         battery_milestones_.battery_changed_actual_percent       = percent;
         battery_milestones_.battery_changed_timestamp            = timestamp;
         battery_milestones_.battery_75_percent_actual_percent    = 0;
         battery_milestones_.battery_75_percent_timestamp         = 0;
         battery_milestones_.battery_50_percent_actual_percent    = 0;
         battery_milestones_.battery_50_percent_timestamp         = 0;
         battery_milestones_.battery_25_percent_actual_percent    = 0;
         battery_milestones_.battery_25_percent_timestamp         = 0;
         battery_milestones_.battery_5_percent_actual_percent     = 0;
         battery_milestones_.battery_5_percent_timestamp          = 0;
         battery_milestones_.battery_0_percent_actual_percent     = 0;
         battery_milestones_.battery_0_percent_timestamp          = 0;
         battery_changed_unloaded_voltage_                        = voltage_unloaded;
         battery_75_percent_unloaded_voltage_                     = 0;
         battery_50_percent_unloaded_voltage_                     = 0;
         battery_25_percent_unloaded_voltage_                     = 0;
         battery_5_percent_unloaded_voltage_                      = 0;
         battery_0_percent_unloaded_voltage_                      = 0;
         battery_voltage_large_jump_counter_                      = 0;
         battery_voltage_large_decline_detected_                  = false;
         threshold_counter                                        = 0;
         battery_event                                            = CTRLM_RCU_BATTERY_EVENT_REPLACED;
         send_event                                               = true;
         //Need to clear battery_voltage_large_jump_counter_ in the database
         ctrlm_db_rf4ce_write_battery_voltage_large_jump_counter(network_id_get(), controller_id_get(), battery_voltage_large_jump_counter_);
      }
      //These check variables are to help us transition from images prior to RDK-32347
      unsigned long battery_75_percent_old_image_check = battery_milestones_.battery_50_percent_timestamp + battery_milestones_.battery_25_percent_timestamp + battery_milestones_.battery_5_percent_timestamp + battery_milestones_.battery_0_percent_timestamp;
      unsigned long battery_50_percent_old_image_check = battery_milestones_.battery_25_percent_timestamp + battery_milestones_.battery_5_percent_timestamp + battery_milestones_.battery_0_percent_timestamp;
      unsigned long battery_25_percent_old_image_check = battery_milestones_.battery_5_percent_timestamp + battery_milestones_.battery_0_percent_timestamp;
      unsigned long battery_5_percent_old_image_check  = battery_milestones_.battery_0_percent_timestamp;
      if((percent <= CTRLM_BATTERY_75_PERCENT_THRESHOLD) && (battery_milestones_.battery_75_percent_timestamp == 0) && (battery_75_percent_old_image_check == 0)) {
         battery_milestones_.battery_75_percent_actual_percent    = percent;
         battery_milestones_.battery_75_percent_timestamp         = timestamp;
         battery_75_percent_unloaded_voltage_                     = voltage_unloaded;
         threshold_counter++;
         battery_event                                            = CTRLM_RCU_BATTERY_EVENT_75_PERCENT;
         send_event                                               = true;
      }
      if((percent <= CTRLM_BATTERY_50_PERCENT_THRESHOLD) && (battery_milestones_.battery_50_percent_timestamp == 0) && (battery_50_percent_old_image_check == 0)) {
         battery_milestones_.battery_50_percent_actual_percent    = percent;
         battery_milestones_.battery_50_percent_timestamp         = timestamp;
         battery_50_percent_unloaded_voltage_                     = voltage_unloaded;
         threshold_counter++;
         battery_event                                            = CTRLM_RCU_BATTERY_EVENT_50_PERCENT;
         send_event                                               = true;
      }
      if((percent <= CTRLM_BATTERY_25_PERCENT_THRESHOLD) && (battery_milestones_.battery_25_percent_timestamp == 0) && (battery_25_percent_old_image_check == 0)) {
         battery_milestones_.battery_25_percent_actual_percent    = percent;
         battery_milestones_.battery_25_percent_timestamp         = timestamp;
         battery_25_percent_unloaded_voltage_                     = voltage_unloaded;
         threshold_counter++;
         battery_event                                            = CTRLM_RCU_BATTERY_EVENT_25_PERCENT;
         send_event                                               = true;
      }
      if((percent <= CTRLM_BATTERY_5_PERCENT_THRESHOLD) && (battery_milestones_.battery_5_percent_timestamp == 0) && (battery_5_percent_old_image_check == 0)) {
         battery_milestones_.battery_5_percent_actual_percent     = percent;
         battery_milestones_.battery_5_percent_timestamp          = timestamp;
         battery_5_percent_unloaded_voltage_                      = voltage_unloaded;
         threshold_counter++;
         battery_event                                            = CTRLM_RCU_BATTERY_EVENT_PENDING_DOOM;
         send_event                                               = true;
      }
      if((percent == CTRLM_BATTERY_0_PERCENT_THRESHOLD)           && (battery_milestones_.battery_0_percent_timestamp == 0)) {
         battery_milestones_.battery_0_percent_actual_percent     = percent;
         battery_milestones_.battery_0_percent_timestamp          = timestamp;
         battery_0_percent_unloaded_voltage_                      = voltage_unloaded;
         threshold_counter++;
         battery_event                                            = CTRLM_RCU_BATTERY_EVENT_0_PERCENT;
         send_event                                               = true;
      }
      battery_last_good_loaded_voltage_                           = voltage_loaded;
      battery_last_good_unloaded_voltage_                         = voltage_unloaded;
      battery_last_good_percent_                                  = percent;
      battery_last_good_timestamp_                                = timestamp;
      if(!batteries_changed && (threshold_counter>1)) {
         battery_voltage_large_decline_detected_ = true;
      }

      ctrlm_db_rf4ce_write_battery_last_good_timestamp(network_id, controller_id, battery_last_good_timestamp_);
      ctrlm_db_rf4ce_write_battery_last_good_percent(network_id, controller_id, battery_last_good_percent_);
      ctrlm_db_rf4ce_write_battery_last_good_loaded_voltage(network_id, controller_id, battery_last_good_loaded_voltage_);
      ctrlm_db_rf4ce_write_battery_last_good_unloaded_voltage(network_id, controller_id, battery_last_good_unloaded_voltage_);
      ctrlm_db_rf4ce_write_battery_voltage_large_decline_detected(network_id, controller_id, battery_voltage_large_decline_detected_);
      ctrlm_db_rf4ce_write_battery_changed_unloaded_voltage(network_id, controller_id, battery_changed_unloaded_voltage_);
      ctrlm_db_rf4ce_write_battery_75_percent_unloaded_voltage(network_id, controller_id, battery_75_percent_unloaded_voltage_);
      ctrlm_db_rf4ce_write_battery_50_percent_unloaded_voltage(network_id, controller_id, battery_50_percent_unloaded_voltage_);
      ctrlm_db_rf4ce_write_battery_25_percent_unloaded_voltage(network_id, controller_id, battery_25_percent_unloaded_voltage_);
      ctrlm_db_rf4ce_write_battery_5_percent_unloaded_voltage(network_id, controller_id, battery_5_percent_unloaded_voltage_);
      ctrlm_db_rf4ce_write_battery_0_percent_unloaded_voltage(network_id, controller_id, battery_0_percent_unloaded_voltage_);

      if(send_event) {
         safec_rc = memcpy_s(data, sizeof(data), &battery_milestones_, CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_MILESTONES);
         ERR_CHK(safec_rc);
         ctrlm_db_rf4ce_write_battery_milestones(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_MILESTONES);
         send_battery_milestone_event(network_id_get(), controller_id_get(), battery_event, percent);
      }
   }

   LOG_INFO("%s: battery_changed <%ld>, <%d%%>, <%4.2fv> battery_75_percent_ <%ld>, <%d%%>, <%4.2fv> battery_50_percent_ <%ld>, <%d%%>, <%4.2fv> battery_25_percent_ <%ld>, <%d%%>, <%4.2fv> battery_5_percent_ <%ld>, <%d%%>, <%4.2fv> battery_0_percent_ <%ld>, <%d%%>, <%4.2fv> \n",
      __FUNCTION__, battery_milestones_.battery_changed_timestamp, battery_milestones_.battery_changed_actual_percent, battery_changed_unloaded_voltage_ * 4.0 / 255, 
      battery_milestones_.battery_75_percent_timestamp, battery_milestones_.battery_75_percent_actual_percent, battery_75_percent_unloaded_voltage_ * 4.0 / 255, 
      battery_milestones_.battery_50_percent_timestamp, battery_milestones_.battery_50_percent_actual_percent, battery_50_percent_unloaded_voltage_ * 4.0 / 255,
      battery_milestones_.battery_25_percent_timestamp, battery_milestones_.battery_25_percent_actual_percent, battery_25_percent_unloaded_voltage_ * 4.0 / 255, 
      battery_milestones_.battery_5_percent_timestamp, battery_milestones_.battery_5_percent_actual_percent, battery_5_percent_unloaded_voltage_ * 4.0 / 255, 
      battery_milestones_.battery_0_percent_timestamp, battery_milestones_.battery_0_percent_actual_percent, battery_0_percent_unloaded_voltage_ * 4.0 / 255);
   LOG_INFO("%s: battery_last_good_timestamp <%ld> battery_last_good_percent_<%d%%> battery_last_good_loaded_voltage <%4.2fv> battery_last_good_unloaded_voltage <%4.2fv> battery_voltage_large_jump_counter <%d> battery_voltage_large_decline_detected_<%d> battery_first_write_ <%d>\n",
      __FUNCTION__, battery_last_good_timestamp_, battery_last_good_percent_, battery_last_good_loaded_voltage_ * 4.0 / 255, battery_last_good_unloaded_voltage_ * 4.0 / 255,
      battery_voltage_large_jump_counter_, battery_voltage_large_decline_detected_, battery_first_write_);
}

gboolean ctrlm_obj_controller_rf4ce_t::send_battery_milestone_event(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_rcu_battery_event_t battery_event, guchar percent) {
   LOG_INFO("%s: \n", __FUNCTION__);
   // Send message to MAIN QUEUE
   ctrlm_main_queue_msg_rf4ce_battery_milestone_t *msg = (ctrlm_main_queue_msg_rf4ce_battery_milestone_t *)g_malloc(sizeof(ctrlm_main_queue_msg_rf4ce_battery_milestone_t));
   if(NULL == msg) {
      LOG_ERROR("%s: Couldn't allocate memory\n", __FUNCTION__);
      g_assert(0);
      return(FALSE);
   }

   msg->type              = CTRLM_MAIN_QUEUE_MSG_TYPE_BATTERY_MILESTONE_EVENT;
   msg->network_id        = network_id;
   msg->controller_id     = controller_id;
   msg->battery_event     = battery_event;
   msg->percent           = percent;
   ctrlm_main_queue_msg_push(msg);
   return(FALSE);
}

gboolean ctrlm_obj_controller_rf4ce_t::send_remote_reboot_event(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, guchar voltage, controller_reboot_reason_t reason, guint32 assert_number) {
   LOG_INFO("%s: \n", __FUNCTION__);
   // Send message to MAIN QUEUE
   ctrlm_main_queue_msg_remote_reboot_t *msg = (ctrlm_main_queue_msg_remote_reboot_t *)g_malloc(sizeof(ctrlm_main_queue_msg_remote_reboot_t));
   if(NULL == msg) {
      LOG_ERROR("%s: Couldn't allocate memory\n", __FUNCTION__);
      g_assert(0);
      return(FALSE);
   }

   msg->type              = CTRLM_MAIN_QUEUE_MSG_TYPE_REMOTE_REBOOT_EVENT;
   msg->network_id        = network_id;
   msg->controller_id     = controller_id;
   msg->voltage           = voltage;
   msg->reason            = reason;
   msg->assert_number     = assert_number;
   ctrlm_main_queue_msg_push(msg);
   return(FALSE);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_battery_status(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0]  = battery_status_.flags;
   data[1]  = battery_status_.voltage_loaded;
   data[2]  = (guchar)(battery_status_.codes_txd_rf);
   data[3]  = (guchar)(battery_status_.codes_txd_rf >> 8);
   data[4]  = (guchar)(battery_status_.codes_txd_rf >> 16);
   data[5]  = (guchar)(battery_status_.codes_txd_rf >> 24);
   data[6]  = (guchar)(battery_status_.codes_txd_ir);
   data[7]  = (guchar)(battery_status_.codes_txd_ir >> 8);
   data[8]  = (guchar)(battery_status_.codes_txd_ir >> 16);
   data[9]  = (guchar)(battery_status_.codes_txd_ir >> 24);
   data[10] = battery_status_.voltage_unloaded;
   
   LOG_INFO("%s: Flags 0x%02X Voltage (%4.2fv, %4.2fv) Codes Txd (RF,IR) (%u,%u)\n", __FUNCTION__, battery_status_.flags, battery_status_.voltage_loaded * 4.0 / 255, battery_status_.voltage_unloaded * 4.0 / 255, battery_status_.codes_txd_rf, battery_status_.codes_txd_ir);
   
   return(CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS);
}

void ctrlm_obj_controller_rf4ce_t::property_write_short_rf_retry_period(guint32 short_rf_retry_period) {
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   // Store the data on the controller object
   short_rf_retry_period_ = short_rf_retry_period;

   LOG_INFO("%s: %u us\n", __FUNCTION__, short_rf_retry_period_);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_short_rf_retry_period(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_SHORT_RF_RETRY_PERIOD) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint32 short_rf_retry_period = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | (data[0]);
   property_write_short_rf_retry_period(short_rf_retry_period);
   return(length);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   return(0);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_short_rf_retry_period(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_SHORT_RF_RETRY_PERIOD) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint32 short_rf_retry_period = short_rf_retry_period_;
   #else
   guint32 short_rf_retry_period = short_rf_retry_period_get();
   #endif
   // Load the data from the controller object
   data[0]  = (guchar) (short_rf_retry_period);
   data[1]  = (guchar) (short_rf_retry_period >>  8);
   data[2]  = (guchar) (short_rf_retry_period >> 16);
   data[3]  = (guchar) (short_rf_retry_period >> 24);

   LOG_INFO("%s: %u us\n", __FUNCTION__, short_rf_retry_period);
   
   return(CTRLM_RF4CE_RIB_ATTR_LEN_SHORT_RF_RETRY_PERIOD);
}

void ctrlm_obj_controller_rf4ce_t::property_write_voice_metrics(void) {
   guchar data[CTRLM_RF4CE_LEN_VOICE_METRICS];
   data[0]  = (guchar)(voice_cmd_count_today_);
   data[1]  = (guchar)(voice_cmd_count_today_ >> 8);
   data[2]  = (guchar)(voice_cmd_count_today_ >> 16);
   data[3]  = (guchar)(voice_cmd_count_today_ >> 24);
   data[4]  = (guchar)(voice_cmd_count_yesterday_);
   data[5]  = (guchar)(voice_cmd_count_yesterday_ >> 8);
   data[6]  = (guchar)(voice_cmd_count_yesterday_ >> 16);
   data[7]  = (guchar)(voice_cmd_count_yesterday_ >> 24);
   data[8]  = (guchar)(voice_cmd_short_today_);
   data[9]  = (guchar)(voice_cmd_short_today_ >> 8);
   data[10] = (guchar)(voice_cmd_short_today_ >> 16);
   data[11] = (guchar)(voice_cmd_short_today_ >> 24);
   data[12] = (guchar)(voice_cmd_short_yesterday_);
   data[13] = (guchar)(voice_cmd_short_yesterday_ >> 8);
   data[14] = (guchar)(voice_cmd_short_yesterday_ >> 16);
   data[15] = (guchar)(voice_cmd_short_yesterday_ >> 24);
   data[16] = (guchar)(today_);
   data[17] = (guchar)(today_ >> 8);
   data[18] = (guchar)(today_ >> 16);
   data[19] = (guchar)(today_ >> 24);
   data[20] = (guchar)(voice_packets_sent_today_);
   data[21] = (guchar)(voice_packets_sent_today_ >> 8);
   data[22] = (guchar)(voice_packets_sent_today_ >> 16);
   data[23] = (guchar)(voice_packets_sent_today_ >> 24);
   data[24] = (guchar)(voice_packets_sent_yesterday_);
   data[25] = (guchar)(voice_packets_sent_yesterday_ >> 8);
   data[26] = (guchar)(voice_packets_sent_yesterday_ >> 16);
   data[27] = (guchar)(voice_packets_sent_yesterday_ >> 24);
   data[28] = (guchar)(voice_packets_lost_today_);
   data[29] = (guchar)(voice_packets_lost_today_ >> 8);
   data[30] = (guchar)(voice_packets_lost_today_ >> 16);
   data[31] = (guchar)(voice_packets_lost_today_ >> 24);
   data[32] = (guchar)(voice_packets_lost_yesterday_);
   data[33] = (guchar)(voice_packets_lost_yesterday_ >> 8);
   data[34] = (guchar)(voice_packets_lost_yesterday_ >> 16);
   data[35] = (guchar)(voice_packets_lost_yesterday_ >> 24);
   data[36] = (guchar)(utterances_exceeding_packet_loss_threshold_today_);
   data[37] = (guchar)(utterances_exceeding_packet_loss_threshold_today_ >> 8);
   data[38] = (guchar)(utterances_exceeding_packet_loss_threshold_today_ >> 16);
   data[39] = (guchar)(utterances_exceeding_packet_loss_threshold_today_ >> 24);
   data[40] = (guchar)(utterances_exceeding_packet_loss_threshold_yesterday_);
   data[41] = (guchar)(utterances_exceeding_packet_loss_threshold_yesterday_ >> 8);
   data[42] = (guchar)(utterances_exceeding_packet_loss_threshold_yesterday_ >> 16);
   data[43] = (guchar)(utterances_exceeding_packet_loss_threshold_yesterday_ >> 24);

   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_voice_metrics(network_id_get(), controller_id_get(), data, CTRLM_RF4CE_LEN_VOICE_METRICS);
   }
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_voice_metrics(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_LEN_VOICE_METRICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   guint32 voice_cmd_count_today                                 = ((data[3]  << 24) | (data[2]  << 16) | (data[1]  << 8) | data[0]);
   guint32 voice_cmd_count_yesterday                             = ((data[7]  << 24) | (data[6]  << 16) | (data[5]  << 8) | data[4]);
   guint32 voice_cmd_short_today                                 = ((data[11] << 24) | (data[10] << 16) | (data[9]  << 8) | data[8]);
   guint32 voice_cmd_short_yesterday                             = ((data[15] << 24) | (data[14] << 16) | (data[13] << 8) | data[12]);
   guint32 today                                                 = ((data[19] << 24) | (data[18] << 16) | (data[17] << 8) | data[16]);
   guint32 voice_packets_sent_today                              = ((data[23] << 24) | (data[22] << 16) | (data[21] << 8) | data[20]);
   guint32 voice_packets_sent_yesterday                          = ((data[27] << 24) | (data[26] << 16) | (data[25] << 8) | data[24]);
   guint32 voice_packets_lost_today                              = ((data[31] << 24) | (data[30] << 16) | (data[29] << 8) | data[28]);
   guint32 voice_packets_lost_yesterday                          = ((data[35] << 24) | (data[34] << 16) | (data[33] << 8) | data[32]);
   guint32 utterances_exceeding_packet_loss_threshold_today      = ((data[39] << 24) | (data[38] << 16) | (data[37] << 8) | data[36]);
   guint32 utterances_exceeding_packet_loss_threshold_yesterday  = ((data[43] << 24) | (data[42] << 16) | (data[41] << 8) | data[40]);
   
   if(voice_cmd_count_today_                                != voice_cmd_count_today     ||
      voice_cmd_count_yesterday_                            != voice_cmd_count_yesterday ||
      voice_cmd_short_today_                                != voice_cmd_short_today     ||
      voice_cmd_short_yesterday_                            != voice_cmd_short_yesterday ||
      voice_packets_sent_today_                             != voice_packets_sent_today ||
      voice_packets_sent_yesterday_                         != voice_packets_sent_yesterday ||
      voice_packets_lost_today_                             != voice_packets_lost_today ||
      voice_packets_lost_yesterday_                         != voice_packets_lost_yesterday ||
      utterances_exceeding_packet_loss_threshold_today_     != utterances_exceeding_packet_loss_threshold_today ||
      utterances_exceeding_packet_loss_threshold_yesterday_ != utterances_exceeding_packet_loss_threshold_yesterday ||
      today_                                                != today) {
      // Store the data on the controller object
      voice_cmd_count_today_                                 = voice_cmd_count_today;
      voice_cmd_count_yesterday_                             = voice_cmd_count_yesterday;
      voice_cmd_short_today_                                 = voice_cmd_short_today;
      voice_cmd_short_yesterday_                             = voice_cmd_short_yesterday;
      voice_packets_sent_today_                              = voice_packets_sent_today;
      voice_packets_sent_yesterday_                          = voice_packets_sent_yesterday;
      voice_packets_lost_today_                              = voice_packets_lost_today;
      voice_packets_lost_yesterday_                          = voice_packets_lost_yesterday;
      utterances_exceeding_packet_loss_threshold_today_      = utterances_exceeding_packet_loss_threshold_today;
      utterances_exceeding_packet_loss_threshold_yesterday_  = utterances_exceeding_packet_loss_threshold_yesterday;
      today_                                                 = today;

      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         property_write_voice_metrics();
      }
   }

   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_voice_metrics(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_LEN_VOICE_METRICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0]  = (guchar)(voice_cmd_count_today_);
   data[1]  = (guchar)(voice_cmd_count_today_ >> 8);
   data[2]  = (guchar)(voice_cmd_count_today_ >> 16);
   data[3]  = (guchar)(voice_cmd_count_today_ >> 24);
   data[4]  = (guchar)(voice_cmd_count_yesterday_);
   data[5]  = (guchar)(voice_cmd_count_yesterday_ >> 8);
   data[6]  = (guchar)(voice_cmd_count_yesterday_ >> 16);
   data[7]  = (guchar)(voice_cmd_count_yesterday_ >> 24);
   data[8]  = (guchar)(voice_cmd_short_today_);
   data[9]  = (guchar)(voice_cmd_short_today_ >> 8);
   data[10] = (guchar)(voice_cmd_short_today_ >> 16);
   data[11] = (guchar)(voice_cmd_short_today_ >> 24);
   data[12] = (guchar)(voice_cmd_short_yesterday_);
   data[13] = (guchar)(voice_cmd_short_yesterday_ >> 8);
   data[14] = (guchar)(voice_cmd_short_yesterday_ >> 16);
   data[15] = (guchar)(voice_cmd_short_yesterday_ >> 24);
   data[16] = (guchar)(today_);
   data[17] = (guchar)(today_ >> 8);
   data[18] = (guchar)(today_ >> 16);
   data[19] = (guchar)(today_ >> 24);
   data[20] = (guchar)(voice_packets_sent_today_);
   data[21] = (guchar)(voice_packets_sent_today_ >> 8);
   data[22] = (guchar)(voice_packets_sent_today_ >> 16);
   data[23] = (guchar)(voice_packets_sent_today_ >> 24);
   data[24] = (guchar)(voice_packets_sent_yesterday_);
   data[25] = (guchar)(voice_packets_sent_yesterday_ >> 8);
   data[26] = (guchar)(voice_packets_sent_yesterday_ >> 16);
   data[27] = (guchar)(voice_packets_sent_yesterday_ >> 24);
   data[28] = (guchar)(voice_packets_lost_today_);
   data[29] = (guchar)(voice_packets_lost_today_ >> 8);
   data[30] = (guchar)(voice_packets_lost_today_ >> 16);
   data[31] = (guchar)(voice_packets_lost_today_ >> 24);
   data[32] = (guchar)(voice_packets_lost_yesterday_);
   data[33] = (guchar)(voice_packets_lost_yesterday_ >> 8);
   data[34] = (guchar)(voice_packets_lost_yesterday_ >> 16);
   data[35] = (guchar)(voice_packets_lost_yesterday_ >> 24);
   data[36] = (guchar)(utterances_exceeding_packet_loss_threshold_today_);
   data[37] = (guchar)(utterances_exceeding_packet_loss_threshold_today_ >> 8);
   data[38] = (guchar)(utterances_exceeding_packet_loss_threshold_today_ >> 16);
   data[39] = (guchar)(utterances_exceeding_packet_loss_threshold_today_ >> 24);
   data[40] = (guchar)(utterances_exceeding_packet_loss_threshold_yesterday_);
   data[41] = (guchar)(utterances_exceeding_packet_loss_threshold_yesterday_ >> 8);
   data[42] = (guchar)(utterances_exceeding_packet_loss_threshold_yesterday_ >> 16);
   data[43] = (guchar)(utterances_exceeding_packet_loss_threshold_yesterday_ >> 24);
   
   LOG_INFO("%s: Voice Cmd Count Today <%u> Voice Cmd Count Yesterday <%u> Voice Cmd Short Today <%u> Voice Cmd Short Yesterday <%u>  Packets Lost Today <%u>  Packets Lost Yesterday <%u>  Utterances Exceeding Pkt Loss Threshold Today <%u>  Utterances Exceeding Pkt Loss Threshold Yesterday <%u>  Today=<%u>\n", __FUNCTION__, voice_cmd_count_today_, voice_cmd_count_yesterday_, voice_cmd_short_today_, voice_cmd_short_yesterday_, voice_packets_lost_today_, voice_packets_lost_yesterday_, utterances_exceeding_packet_loss_threshold_today_, utterances_exceeding_packet_loss_threshold_yesterday_, today_);
   
   return(CTRLM_RF4CE_LEN_VOICE_METRICS);
}

void ctrlm_obj_controller_rf4ce_t::property_write_firmware_updated(void) {
   guchar data[CTRLM_RF4CE_LEN_FIRMWARE_UPDATED];
   data[0]  = (guchar)(firmware_updated_);

   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_firmware_updated(network_id_get(), controller_id_get(), data, CTRLM_RF4CE_LEN_FIRMWARE_UPDATED);
   }
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_firmware_updated(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_LEN_FIRMWARE_UPDATED) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   unsigned char firmware_updated = data[0];
   
   if(firmware_updated_ != firmware_updated) {
      // Store the data on the controller object
      firmware_updated_ = (ctrlm_rf4ce_firmware_updated_t)firmware_updated;
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_firmware_updated(network_id_get(), controller_id_get(), data, length);
      }
   }

   LOG_INFO("%s: Firmware Updated <%s>\n", __FUNCTION__, ctrlm_rf4ce_firmware_updated_str((ctrlm_rf4ce_firmware_updated_t)firmware_updated));
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_firmware_updated(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_LEN_FIRMWARE_UPDATED) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0]  = (char)(firmware_updated_);
   
   LOG_INFO("%s: Firmware Updated <%s>\n", __FUNCTION__, ctrlm_rf4ce_firmware_updated_str((ctrlm_rf4ce_firmware_updated_t)firmware_updated_));
   
   return(CTRLM_RF4CE_LEN_FIRMWARE_UPDATED);
}

void ctrlm_obj_controller_rf4ce_t::property_write_reboot_diagnostics(void) {
   time_t reboot_time_ = time(NULL);
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_DIAGNOSTICS];
   data[0]  = reboot_reason_;
   data[1]  = reboot_voltage_level_;
   data[2]  = (guchar)(reboot_assert_number_);
   data[3]  = (guchar)(reboot_assert_number_ >> 8);
   data[4]  = (guchar)(reboot_assert_number_ >> 16);
   data[5]  = (guchar)(reboot_assert_number_ >> 24);
   data[6]  = (guchar)(reboot_time_);
   data[7]  = (guchar)(reboot_time_ >> 8);
   data[8]  = (guchar)(reboot_time_ >> 16);
   data[9]  = (guchar)(reboot_time_ >> 24);

   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_reboot_diagnostics(network_id_get(), controller_id_get(), data, CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_DIAGNOSTICS);
      send_remote_reboot_event(network_id_get(), controller_id_get(), reboot_voltage_level_, reboot_reason_, reboot_assert_number_);
   }
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_reboot_diagnostics(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_DIAGNOSTICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   guchar  reboot_reason          = data[0];
   guchar  reboot_voltage_level   = data[1];
   guint32 reboot_assert_number   = (data[5] <<24) | (data[4] << 16) | (data[3] << 8) | data[2];
   time_t reboot_time             = (data[9] <<24) | (data[8] << 16) | (data[7] << 8) | data[6];
   
   if((reboot_reason_ != reboot_reason) || (reboot_voltage_level_ != reboot_voltage_level) || (reboot_assert_number_ != reboot_assert_number) || (reboot_time_ != reboot_time)) {
      // Store the data on the controller object
      reboot_reason_          = (controller_reboot_reason_t)reboot_reason;
      reboot_voltage_level_   = reboot_voltage_level;
      reboot_assert_number_   = reboot_assert_number;
      reboot_time_            = reboot_time;
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_reboot_diagnostics(network_id_get(), controller_id_get(), data, length);
      }
   }

   if(reboot_reason_ == CONTROLLER_REBOOT_ASSERT_NUMBER) {
      LOG_INFO("%s: Reboot Reason <%s>, Reboot Voltage Level <%d>, Reboot Time <%lu>, Reboot Assert Number <%u>\n", __FUNCTION__, ctrlm_rf4ce_reboot_reason_str((controller_reboot_reason_t)reboot_reason_), reboot_voltage_level_, reboot_time_, reboot_assert_number_);
   } else {
      LOG_INFO("%s: Reboot Reason <%s>, Reboot Voltage Level <%d>, Reboot Time <%lu>\n", __FUNCTION__, ctrlm_rf4ce_reboot_reason_str((controller_reboot_reason_t)reboot_reason_), reboot_voltage_level_, reboot_time_);
   }
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_reboot_diagnostics(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_DIAGNOSTICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0]  = reboot_reason_;
   data[1]  = reboot_voltage_level_;
   data[2]  = (guchar)(reboot_assert_number_);
   data[3]  = (guchar)(reboot_assert_number_ >> 8);
   data[4]  = (guchar)(reboot_assert_number_ >> 16);
   data[5]  = (guchar)(reboot_assert_number_ >> 24);
   data[6]  = (guchar)(reboot_time_);
   data[7]  = (guchar)(reboot_time_ >> 8);
   data[8]  = (guchar)(reboot_time_ >> 16);
   data[9]  = (guchar)(reboot_time_ >> 24);
   
   if(reboot_reason_ == CONTROLLER_REBOOT_ASSERT_NUMBER) {
      LOG_INFO("%s: Reboot Reason <%s>, Reboot Voltage Level <%d>, Reboot Assert Number <%u>\n", __FUNCTION__, ctrlm_rf4ce_reboot_reason_str((controller_reboot_reason_t)reboot_reason_), reboot_voltage_level_, reboot_assert_number_);
   } else {
      LOG_INFO("%s: Reboot Reason <%s>, Reboot Voltage Level <%d>\n", __FUNCTION__, ctrlm_rf4ce_reboot_reason_str((controller_reboot_reason_t)reboot_reason_), reboot_voltage_level_);
   }
   
   return(CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_DIAGNOSTICS);
}

void ctrlm_obj_controller_rf4ce_t::property_write_memory_statistics(void) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATISTICS];
   data[0]  = (guchar)(memory_available_);
   data[1]  = (guchar)(memory_available_ >> 8);
   data[2]  = (guchar)(memory_largest_);
   data[3]  = (guchar)(memory_largest_ >> 8);

   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_memory_statistics(network_id_get(), controller_id_get(), data, CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATISTICS);
   }
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_memory_statistics(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATISTICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   guint16 memory_available = (data[1] << 8) | data[0];
   guint16 memory_largest   = (data[3] << 8) | data[2];
   
   if((memory_available_ != memory_available) || (memory_largest_ != memory_largest)) {
      // Store the data on the controller object
      memory_available_  = memory_available;
      memory_largest_    = memory_largest;
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_memory_statistics(network_id_get(), controller_id_get(), data, length);
      }
   }

   LOG_INFO("%s: Memory Available <%u> Largest <%u>\n", __FUNCTION__, memory_available_, memory_largest_);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_memory_statistics(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATISTICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0]  = (guchar)(memory_available_);
   data[1]  = (guchar)(memory_available_ >> 8);
   data[2]  = (guchar)(memory_largest_);
   data[3]  = (guchar)(memory_largest_ >> 8);
   
   LOG_INFO("%s: Memory Available <%u> Largest <%u>\n", __FUNCTION__, memory_available_, memory_largest_);
   
   return(CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATISTICS);
}

void ctrlm_obj_controller_rf4ce_t::property_write_time_last_checkin_for_device_update(void) {
   guchar data[CTRLM_RF4CE_LEN_TIME_LAST_CHECKIN_FOR_DEVICE_UPDATE];
   data[0]  = (guchar)(time_last_checkin_for_device_update_);
   data[1]  = (guchar)(time_last_checkin_for_device_update_ >> 8);
   data[2]  = (guchar)(time_last_checkin_for_device_update_ >> 16);
   data[3]  = (guchar)(time_last_checkin_for_device_update_ >> 24);

   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_time_last_checkin_for_device_update(network_id_get(), controller_id_get(), data, CTRLM_RF4CE_LEN_TIME_LAST_CHECKIN_FOR_DEVICE_UPDATE);
   }
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_time_last_checkin_for_device_update(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_LEN_TIME_LAST_CHECKIN_FOR_DEVICE_UPDATE) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   time_t time_last_checkin_for_device_update = ((data[3]  << 24) | (data[2]  << 16) | (data[1]  << 8) | data[0]);
   
   if(time_last_checkin_for_device_update_ != time_last_checkin_for_device_update) {
      // Store the data on the controller object
      time_last_checkin_for_device_update_ = time_last_checkin_for_device_update;

      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         property_write_time_last_checkin_for_device_update();
      }
   }

   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_time_last_checkin_for_device_update(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_LEN_TIME_LAST_CHECKIN_FOR_DEVICE_UPDATE) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0]  = (guchar)(time_last_checkin_for_device_update_);
   data[1]  = (guchar)(time_last_checkin_for_device_update_ >> 8);
   data[2]  = (guchar)(time_last_checkin_for_device_update_ >> 16);
   data[3]  = (guchar)(time_last_checkin_for_device_update_ >> 24);
   
   LOG_INFO("%s: Time Last Checkin For Device Update <%ld>\n", __FUNCTION__, time_last_checkin_for_device_update_);

   return(CTRLM_RF4CE_LEN_TIME_LAST_CHECKIN_FOR_DEVICE_UPDATE);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_voice_command_status(guchar *data, guchar length) {
   if(length > CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_STATUS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   errno_t safec_rc = -1;

   switch(data[0]) {
      case VOICE_COMMAND_STATUS_PENDING: LOG_INFO("%s: PENDING\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_TIMEOUT: LOG_INFO("%s: TIMEOUT\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_OFFLINE: LOG_INFO("%s: OFFLINE\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_SUCCESS: LOG_INFO("%s: SUCCESS\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_FAILURE: LOG_INFO("%s: FAILURE\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_NO_CMDS: LOG_INFO("%s: NO CMDS\n", __FUNCTION__); break;
      #ifdef USE_VOICE_SDK
      case VOICE_COMMAND_STATUS_TV_AVR_CMD: LOG_INFO("%s: TV_AVR_CMD\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_MIC_CMD:    LOG_INFO("%s: MIC_CMD\n",    __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_AUDIO_CMD:  LOG_INFO("%s: AUDIO_CMD\n",  __FUNCTION__); break;
      #endif
      default:                           LOG_ERROR("%s: INVALID STATUS 0x%02X\n", __FUNCTION__, data[0]); return 0;
   }
   #ifdef USE_VOICE_SDK
   if(data[0] >= VOICE_COMMAND_STATUS_TV_AVR_CMD && controller_type_ != RF4CE_CONTROLLER_TYPE_XR19) {
      // Maintain compatibility for controllers that don't support TV/AVR, MIC, AUDIO cmds
      data[0] = VOICE_COMMAND_STATUS_SUCCESS;
      safec_rc = memset_s(data+1, length - 1, 0, length - 1);
      ERR_CHK(safec_rc);
   }
   #endif

   safec_rc = memcpy_s(voice_command_status_, sizeof(voice_command_status_), data,length );
   ERR_CHK(safec_rc);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_voice_command_status(guchar *data, guchar length) {
   if(length > CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_STATUS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   // Load the data from the controller object
   errno_t safec_rc = memcpy_s(data, CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE, voice_command_status_,length );
   ERR_CHK(safec_rc);

#ifdef USE_VOICE_SDK
   LOG_INFO("%s: %s", __FUNCTION__, ctrlm_voice_command_status_str((ctrlm_voice_command_status_t)data[0]));
   if(data[0] == VOICE_COMMAND_STATUS_TV_AVR_CMD) {
      LOG_RAW(" -- cmd <%s>", ctrlm_voice_command_status_tv_avr_str((ctrlm_voice_tv_avr_cmd_t)data[2]));
      switch(data[2]) {
         case CTRLM_VOICE_TV_AVR_CMD_POWER_OFF:
         case CTRLM_VOICE_TV_AVR_CMD_POWER_ON: {
            LOG_RAW(", toggle fallback <%s>", (data[1] & 0x01 ? "YES" : "NO"));
            break;
         }
         case CTRLM_VOICE_TV_AVR_CMD_VOLUME_UP:
         case CTRLM_VOICE_TV_AVR_CMD_VOLUME_DOWN: {
            LOG_RAW(", ir repeats <%u>", data[3]);
            break;
         }
         default: break;
      }
   }
   LOG_RAW("\n");
#else
   switch(voice_command_status_[0]) {
      case VOICE_COMMAND_STATUS_PENDING: LOG_INFO("%s: PENDING\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_TIMEOUT: LOG_INFO("%s: TIMEOUT\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_OFFLINE: LOG_INFO("%s: OFFLINE\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_SUCCESS: LOG_INFO("%s: SUCCESS\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_FAILURE: LOG_INFO("%s: FAILURE\n", __FUNCTION__); break;
      case VOICE_COMMAND_STATUS_NO_CMDS: LOG_INFO("%s: NO CMDS\n", __FUNCTION__); break;
      default:                           LOG_WARN("%s: UNKNOWN 0x%02X\n", __FUNCTION__, data[0]); break;
   }
#endif

   if(voice_command_status_[0] != VOICE_COMMAND_STATUS_PENDING) { // Reset back to "no cmds" after the controller has read the result
      voice_command_status_[0] = VOICE_COMMAND_STATUS_NO_CMDS;
      safec_rc = memset_s(&voice_command_status_[1], sizeof(voice_command_status_)-1, 0, sizeof(voice_command_status_)-1);
      ERR_CHK(safec_rc);
      #ifdef USE_VOICE_SDK
      ctrlm_get_voice_obj()->voice_controller_command_status_read(network_id_get(), controller_id_get());
      #endif
   }

   return(length);
}

void ctrlm_obj_controller_rf4ce_t::property_write_voice_command_length(guchar voice_command_length) {
   if(voice_command_length & 1) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return;
   }
   
   if(voice_command_length_ != voice_command_length) {
      rib_entries_updated_ = RIB_ENTRIES_UPDATED_TRUE;
   }
   
   // Store the data on the controller object
   voice_command_length_ = (voice_command_length_t)voice_command_length;

   switch(voice_command_length_) {
      case VOICE_COMMAND_LENGTH_CONTROLLER_DEFAULT:  LOG_INFO("%s: controller default\n", __FUNCTION__); break;
      case VOICE_COMMAND_LENGTH_PROFILE_NEGOTIATION: LOG_INFO("%s: profile negotiation\n", __FUNCTION__); break;
      case VOICE_COMMAND_LENGTH_VALUE:
      default:                                       LOG_INFO("%s: %u samples\n", __FUNCTION__, voice_command_length_); break;
   }
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_voice_command_length(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_LENGTH) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   guchar voice_command_length = data[0];
   property_write_voice_command_length(voice_command_length);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_voice_command_length(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_LENGTH) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0]  = voice_command_length_;

   switch(voice_command_length_) {
      case VOICE_COMMAND_LENGTH_CONTROLLER_DEFAULT:  LOG_INFO("%s: controller default\n", __FUNCTION__); break;
      case VOICE_COMMAND_LENGTH_PROFILE_NEGOTIATION: LOG_INFO("%s: profile negotiation\n", __FUNCTION__); break;
      case VOICE_COMMAND_LENGTH_VALUE:
      default:                                       LOG_INFO("%s: %u samples\n", __FUNCTION__, voice_command_length_); break;
   }
   
   return(CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_LENGTH);
}

void ctrlm_obj_controller_rf4ce_t::property_write_maximum_utterance_length(guint16 maximum_utterance_length) {
   if(maximum_utterance_length > 0 && maximum_utterance_length < 100) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return;
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint16 utterance_duration_max = utterance_duration_max_;
   // Store the data on the controller object
   utterance_duration_max_ = maximum_utterance_length;
   if(utterance_duration_max != maximum_utterance_length) {
      rib_entries_updated_ = RIB_ENTRIES_UPDATED_TRUE;
   }

   LOG_INFO("%s: %u ms\n", __FUNCTION__, utterance_duration_max_);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_maximum_utterance_length(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAXIMUM_UTTERANCE_LENGTH) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint16 maximum_utterance_length = (data[1] << 8) | (data[0]);

   property_write_maximum_utterance_length(maximum_utterance_length);
   return(length);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   return(0);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_maximum_utterance_length(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAXIMUM_UTTERANCE_LENGTH) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint16 utterance_duration_max = utterance_duration_max_;
   #else
   guint16 utterance_duration_max = utterance_duration_max_get();
   #endif
   // Load the data from the controller object
   data[0]  = (guchar) (utterance_duration_max);
   data[1]  = (guchar) (utterance_duration_max >> 8);

   LOG_INFO("%s: %u ms\n", __FUNCTION__, utterance_duration_max);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_MAXIMUM_UTTERANCE_LENGTH);
}

void ctrlm_obj_controller_rf4ce_t::property_write_voice_command_encryption(voice_command_encryption_t voice_command_encryption) {
   if(voice_command_encryption > VOICE_COMMAND_ENCRYPTION_DEFAULT) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return;
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   if(voice_command_encryption_ != voice_command_encryption) {
      rib_entries_updated_ = RIB_ENTRIES_UPDATED_TRUE;
   }
   
   // Store the data on the controller object
   voice_command_encryption_ = voice_command_encryption;
   LOG_INFO("%s: <%s>\n", __FUNCTION__, ctrlm_rf4ce_voice_command_encryption_str(voice_command_encryption));
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_voice_command_encryption(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_ENCRYPTION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   voice_command_encryption_t voice_command_encryption = (voice_command_encryption_t)data[0];

   property_write_voice_command_encryption(voice_command_encryption);
   return(length);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   return(0);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_voice_command_encryption(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_ENCRYPTION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   voice_command_encryption_t voice_command_encryption = voice_command_encryption_;
   #else
   voice_command_encryption_t voice_command_encryption = voice_command_encryption_get();
   #endif

   // Load the data from the controller object
   data[0] = voice_command_encryption;
   LOG_INFO("%s: <%s>\n", __FUNCTION__, ctrlm_rf4ce_voice_command_encryption_str(voice_command_encryption));

   return(CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_ENCRYPTION);
}

void ctrlm_obj_controller_rf4ce_t::property_write_max_voice_data_retry(guchar max_voice_data_retry_attempts) {
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   if(voice_data_retry_max_ != max_voice_data_retry_attempts) {
      rib_entries_updated_ = RIB_ENTRIES_UPDATED_TRUE;
   }
   
   // Store the data on the controller object
   voice_data_retry_max_ = max_voice_data_retry_attempts;

   LOG_INFO("%s: %u attempts\n", __FUNCTION__, voice_data_retry_max_);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_max_voice_data_retry(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_DATA_RETRY) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guchar max_voice_data_retry_attempts = data[0];
   property_write_max_voice_data_retry(max_voice_data_retry_attempts);
   return(length);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   return(0);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_max_voice_data_retry(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_DATA_RETRY) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guchar voice_data_retry_max = voice_data_retry_max_;
   #else
   guchar voice_data_retry_max = voice_data_retry_max_get();
   #endif
   // Load the data from the controller object
   data[0] = voice_data_retry_max;

   LOG_INFO("%s: %u attempts\n", __FUNCTION__, voice_data_retry_max);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_DATA_RETRY);
}

void ctrlm_obj_controller_rf4ce_t::property_write_max_voice_csma_backoff(guchar max_voice_csma_backoffs) {
   if(max_voice_csma_backoffs > 5) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return;
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   if(voice_csma_backoff_max_ != max_voice_csma_backoffs) {
      rib_entries_updated_ = RIB_ENTRIES_UPDATED_TRUE;
   }
   
   // Store the data on the controller object
   voice_csma_backoff_max_ = max_voice_csma_backoffs;

   LOG_INFO("%s: %u backoffs\n", __FUNCTION__, voice_csma_backoff_max_);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_max_voice_csma_backoff(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_CSMA_BACKOFF) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guchar max_voice_csma_backoffs = data[0];
   property_write_max_voice_csma_backoff(max_voice_csma_backoffs);
   return(length);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   return(0);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_max_voice_csma_backoff(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_CSMA_BACKOFF) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guchar voice_csma_backoff_max = voice_csma_backoff_max_;
   #else
   guchar voice_csma_backoff_max = voice_csma_backoff_max_get();
   #endif
   // Load the data from the controller object
   data[0] = voice_csma_backoff_max;

   LOG_INFO("%s: %u backoffs\n", __FUNCTION__, voice_csma_backoff_max);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_CSMA_BACKOFF);
}

void ctrlm_obj_controller_rf4ce_t::property_write_min_voice_data_backoff(guchar min_voice_data_backoff_exp) {
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   if(voice_data_backoff_exp_min_ != min_voice_data_backoff_exp) {
      rib_entries_updated_ = RIB_ENTRIES_UPDATED_TRUE;
   }
   
   // Store the data on the controller object
   voice_data_backoff_exp_min_ = min_voice_data_backoff_exp;

   LOG_INFO("%s: backoff exponent %u\n", __FUNCTION__, voice_data_backoff_exp_min_);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_min_voice_data_backoff(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MIN_VOICE_DATA_BACKOFF) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guchar min_voice_data_backoff_exp = data[0];
   property_write_min_voice_data_backoff(min_voice_data_backoff_exp);
   return(length);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   return(0);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_min_voice_data_backoff(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MIN_VOICE_DATA_BACKOFF) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guchar voice_data_backoff_exp_min = voice_data_backoff_exp_min_;
   #else
   guchar voice_data_backoff_exp_min = voice_data_backoff_exp_min_get();
   #endif
   // Load the data from the controller object
   data[0] = voice_data_backoff_exp_min;

   LOG_INFO("%s: backoff exponent %u\n", __FUNCTION__, voice_data_backoff_exp_min);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_MIN_VOICE_DATA_BACKOFF);
}

void ctrlm_obj_controller_rf4ce_t::property_write_voice_ctrl_audio_profiles(guint16 audio_profiles_ctrl) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES];
   data[0] = (guchar)audio_profiles_ctrl;
   data[1] = (guchar)(audio_profiles_ctrl >> 8);
   property_write_voice_ctrl_audio_profiles(data, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_voice_ctrl_audio_profiles(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   guint16 audio_profiles_ctrl = ((data[1] << 8) | data[0]);
   
   if(audio_profiles_ctrl_ != audio_profiles_ctrl) {
      // Store the data on the controller object
      audio_profiles_ctrl_ = audio_profiles_ctrl;
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_audio_profiles(network_id_get(), controller_id_get(), data, length);
         obj_network_rf4ce_->req_process_rib_export(controller_id_get(), CTRLM_HAL_RF4CE_RIB_ATTR_ID_VOICE_CTRL_AUDIO_PROFILES, CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES, data);
      }
   }

   LOG_INFO("%s: 0x%04X\n", __FUNCTION__, audio_profiles_ctrl_);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_voice_ctrl_audio_profiles(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0]  = (guchar) (audio_profiles_ctrl_);
   data[1]  = (guchar) (audio_profiles_ctrl_ >> 8);
   
   LOG_INFO("%s: 0x%04X\n", __FUNCTION__, audio_profiles_ctrl_);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_voice_targ_audio_profiles(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   guint16 audio_profiles_ctrl = VOICE_AUDIO_PROFILE_ADPCM_16BIT_16KHZ;

   #ifdef USE_VOICE_SDK
   guint16 audio_profiles_targ = audio_profiles_targ_get(); // RF4CE Network profiles supported

   if(audio_profiles_targ & audio_profiles_ctrl_ & VOICE_AUDIO_PROFILE_OPUS_16BIT_16KHZ)  { // network and controller support OPUS
      audio_profiles_ctrl = VOICE_AUDIO_PROFILE_OPUS_16BIT_16KHZ;
   }
   #endif

   // Tell the controller which format to use
   data[0]  = (guchar) (audio_profiles_ctrl);
   data[1]  = (guchar) (audio_profiles_ctrl >> 8);
   
   LOG_INFO("%s: <%s>\n", __FUNCTION__, voice_audio_profile_str((voice_audio_profile_t)audio_profiles_ctrl));

   return(CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES);
}

void ctrlm_obj_controller_rf4ce_t::property_write_voice_statistics(guint32 voice_sessions_activated, guint32 audio_data_tx_time) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS];
   data[0] = (guchar)(voice_sessions_activated);
   data[1] = (guchar)(voice_sessions_activated >> 8);
   data[2] = (guchar)(voice_sessions_activated >> 16);
   data[3] = (guchar)(voice_sessions_activated >> 24);
   data[4] = (guchar)(audio_data_tx_time);
   data[5] = (guchar)(audio_data_tx_time >> 8);
   data[6] = (guchar)(audio_data_tx_time >> 16);
   data[7] = (guchar)(audio_data_tx_time >> 24);
   property_write_voice_statistics(data, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_voice_statistics(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   guint32 voice_sessions_activated = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
   guint32 audio_data_tx_time       = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];
   
   if(voice_statistics_.voice_sessions_activated != voice_sessions_activated ||
      voice_statistics_.audio_data_tx_time       != audio_data_tx_time) {
      // Store the data on the controller object
      voice_statistics_.voice_sessions_activated = voice_sessions_activated;
      voice_statistics_.audio_data_tx_time       = audio_data_tx_time;
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_voice_statistics(network_id_get(), controller_id_get(), data, length);
         obj_network_rf4ce_->req_process_rib_export(controller_id_get(), CTRLM_HAL_RF4CE_RIB_ATTR_ID_VOICE_STATISTICS, CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS, data);
      }
   }

   LOG_INFO("%s: sessions activated %u\n", __FUNCTION__, voice_statistics_.voice_sessions_activated);
   LOG_INFO("%s: audio data tx time %u\n", __FUNCTION__, voice_statistics_.audio_data_tx_time);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_voice_statistics(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0]  = (guchar) (voice_statistics_.voice_sessions_activated);
   data[1]  = (guchar) (voice_statistics_.voice_sessions_activated >> 8);
   data[2]  = (guchar) (voice_statistics_.voice_sessions_activated >> 16);
   data[3]  = (guchar) (voice_statistics_.voice_sessions_activated >> 24);
   data[4]  = (guchar) (voice_statistics_.audio_data_tx_time);
   data[5]  = (guchar) (voice_statistics_.audio_data_tx_time >> 8);
   data[6]  = (guchar) (voice_statistics_.audio_data_tx_time >> 16);
   data[7]  = (guchar) (voice_statistics_.audio_data_tx_time >> 24);

   LOG_INFO("%s: sessions activated %u\n", __FUNCTION__, voice_statistics_.voice_sessions_activated);
   LOG_INFO("%s: audio data tx time %u\n", __FUNCTION__, voice_statistics_.audio_data_tx_time);
   
   return(CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_rib_entries_updated(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_RIB_ENTRIES_UPDATED) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0] = rib_entries_updated_;

   switch(rib_entries_updated_) {
      case RIB_ENTRIES_UPDATED_FALSE: { 
         LOG_INFO("%s: FALSE\n", __FUNCTION__);
         break;
      }
      case RIB_ENTRIES_UPDATED_TRUE: {
         LOG_INFO("%s: TRUE\n", __FUNCTION__);
         rib_entries_updated_ = RIB_ENTRIES_UPDATED_FALSE;
         break;
      }
      case RIB_ENTRIES_UPDATED_STOP: {
         LOG_INFO("%s: STOP\n", __FUNCTION__);
         break;
      }
      default: {
         LOG_WARN("%s: UNKNOWN 0x%02X\n", __FUNCTION__, rib_entries_updated_);
         rib_entries_updated_ = RIB_ENTRIES_UPDATED_FALSE;
         break;
      }
   }

   return(CTRLM_RF4CE_RIB_ATTR_LEN_RIB_ENTRIES_UPDATED);
}

void ctrlm_obj_controller_rf4ce_t::property_write_rib_update_check_interval(guint16 rib_update_check_interval) {
   if(rib_update_check_interval > 8760) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return;
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   if(rib_update_check_interval_ != rib_update_check_interval) {
      rib_entries_updated_ = RIB_ENTRIES_UPDATED_TRUE;
   }
   
   // Store the data on the controller object
   rib_update_check_interval_ = rib_update_check_interval;

   LOG_INFO("%s: %u hours\n", __FUNCTION__, rib_update_check_interval_);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_rib_update_check_interval(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_RIB_UPDATE_CHECK_INTERVAL) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint16 rib_update_check_interval = (data[1] << 8) | data[0];
   property_write_rib_update_check_interval(rib_update_check_interval);
   return(length);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   return(0);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_rib_update_check_interval(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_RIB_UPDATE_CHECK_INTERVAL) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint16 rib_update_check_interval = rib_update_check_interval_;
   #else
   guint16 rib_update_check_interval = rib_update_check_interval_get();
   #endif
   // Load the data from the controller object
   data[0] = rib_update_check_interval;
   data[1] = (rib_update_check_interval >> 8);

   LOG_INFO("%s: %u hours\n", __FUNCTION__, rib_update_check_interval);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_RIB_UPDATE_CHECK_INTERVAL);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_voice_session_statistics(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_STATISTICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Store the data on the controller object
   guint16 total_packets, total_dropped, drop_retry, drop_buffer, mac_retries, network_retries, cca_sense;
   float drop_percent, retry_percent, buffer_percent;
   total_packets   = (data[1]  << 8) | data[0];
   drop_retry      = (data[3]  << 8) | data[2];
   drop_buffer     = (data[5]  << 8) | data[4];
   total_dropped   = drop_retry + drop_buffer;
   drop_percent    = (100.0 * total_dropped / total_packets);
   retry_percent   = (100.0 * drop_retry    / total_packets);
   buffer_percent  = (100.0 * drop_buffer   / total_packets);
   // Interference indicators
   mac_retries     = (data[7]  << 8) | data[6];
   network_retries = (data[9]  << 8) | data[8];
   cca_sense       = (data[11] << 8) | data[10];
   
   // Write this data to the database??

   LOG_INFO("%s: Total Packets %u Dropped %u (%5.2f%%) due to Retry %u (%5.2f%%) Buffer %u (%5.2f%%)\n", __FUNCTION__, total_packets, total_dropped, drop_percent, drop_retry, retry_percent, drop_buffer, buffer_percent);

   if(mac_retries != 0xFFFF || network_retries != 0xFFFF || cca_sense != 0xFFFF) {
       LOG_INFO("%s: Total MAC Retries %u Network Retries %u CCA Sense Failures %u\n", __FUNCTION__, mac_retries, network_retries, cca_sense);
   }
   // Send data to control manager
   ctrlm_voice_notify_stats_session(network_id_get(), controller_id_get(), total_packets, drop_retry, drop_buffer, mac_retries, network_retries, cca_sense);

   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_voice_session_statistics(guchar *data, guchar length) {
   // Load the data from the controller object
   LOG_INFO("%s: NOT IMPLEMENTED\n", __FUNCTION__);
   return(0);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_opus_encoding_params(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   LOG_INFO("%s: NOT SUPPORTED\n", __FUNCTION__);
   return(0);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_opus_encoding_params(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifndef USE_VOICE_SDK
   LOG_INFO("%s: NOT SUPPORTED\n", __FUNCTION__);
   return(0);
   #else
   voice_params_opus_encoder_t params;
   ctrlm_get_voice_obj()->voice_params_opus_encoder_get(&params);
   errno_t safec_rc = memcpy_s(data, CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE, params.data, CTRLM_RF4CE_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS);
   ERR_CHK(safec_rc);
   return(CTRLM_RF4CE_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_voice_session_qos(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_QOS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   LOG_INFO("%s: NOT SUPPORTED\n", __FUNCTION__);
   return(0);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_voice_session_qos(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_QOS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifndef USE_VOICE_SDK
   LOG_INFO("%s: NOT SUPPORTED\n", __FUNCTION__);
   return(0);
   #else
   voice_params_qos_t params;
   ctrlm_get_voice_obj()->voice_params_qos_get(&params);

   data[0] = (params.timeout_packet_initial);
   data[1] = (params.timeout_packet_initial >> 8);
   data[2] = (params.timeout_packet_subsequent);
   data[3] = (params.timeout_packet_subsequent >> 8);
   data[4] = (params.bitrate_minimum);
   data[5] = (params.time_threshold);
   data[6] = (params.time_threshold >> 8);

   LOG_INFO("%s: Timeout First %u Inter %u Bitrate min %u Time Threshold %u\n", __FUNCTION__, params.timeout_packet_initial, params.timeout_packet_subsequent, params.bitrate_minimum, params.time_threshold);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_SESSION_QOS);
   #endif
}

void ctrlm_obj_controller_rf4ce_t::property_write_update_version_bootloader(guchar major, guchar minor, guchar revision, guchar patch) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING];
   data[0] = major;
   data[1] = minor;
   data[2] = revision;
   data[3] = patch;
   property_write_update_version_bootloader(data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_update_version_bootloader(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   if(version_bootloader_.major    != data[0] ||
      version_bootloader_.minor    != data[1] ||
      version_bootloader_.revision != data[2] ||
      version_bootloader_.patch    != data[3]) {
      // Store the data on the controller object
      version_bootloader_.major    = data[0];
      version_bootloader_.minor    = data[1];
      version_bootloader_.revision = data[2];
      version_bootloader_.patch    = data[3];
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_update_version_bootloader(network_id_get(), controller_id_get(), data, length);
         obj_network_rf4ce_->req_process_rib_export(controller_id_get(), CTRLM_HAL_RF4CE_RIB_ATTR_ID_UPDATE_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_UPDATE_VERSIONING_BOOTLOADER, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING, data);
      }
   }
   
   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_bootloader_.major, version_bootloader_.minor, version_bootloader_.revision, version_bootloader_.patch);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_update_version_bootloader(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0] = version_bootloader_.major;
   data[1] = version_bootloader_.minor;
   data[2] = version_bootloader_.revision;
   data[3] = version_bootloader_.patch;
   
   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_bootloader_.major, version_bootloader_.minor, version_bootloader_.revision, version_bootloader_.patch);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
}

void ctrlm_obj_controller_rf4ce_t::property_write_update_version_golden(guchar major, guchar minor, guchar revision, guchar patch) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING];
   data[0] = major;
   data[1] = minor;
   data[2] = revision;
   data[3] = patch;
   property_write_update_version_golden(data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_update_version_golden(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   if(version_golden_.major    != data[0] ||
      version_golden_.minor    != data[1] ||
      version_golden_.revision != data[2] ||
      version_golden_.patch    != data[3]) {
      // Store the data on the controller object
      version_golden_.major    = data[0];
      version_golden_.minor    = data[1];
      version_golden_.revision = data[2];
      version_golden_.patch    = data[3];
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_update_version_golden(network_id_get(), controller_id_get(), data, length);
         obj_network_rf4ce_->req_process_rib_export(controller_id_get(), CTRLM_HAL_RF4CE_RIB_ATTR_ID_UPDATE_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_UPDATE_VERSIONING_GOLDEN, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING, data);
      }
   }

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_golden_.major, version_golden_.minor, version_golden_.revision, version_golden_.patch);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_update_version_golden(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0] = version_golden_.major;
   data[1] = version_golden_.minor;
   data[2] = version_golden_.revision;
   data[3] = version_golden_.patch;

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_golden_.major, version_golden_.minor, version_golden_.revision, version_golden_.patch);
   
   return(CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
}

void ctrlm_obj_controller_rf4ce_t::property_write_update_version_audio_data(guchar major, guchar minor, guchar revision, guchar patch) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING];
   data[0] = major;
   data[1] = minor;
   data[2] = revision;
   data[3] = patch;
   property_write_update_version_audio_data(data, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_update_version_audio_data(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   if(version_audio_data_.major    != data[0] ||
      version_audio_data_.minor    != data[1] ||
      version_audio_data_.revision != data[2] ||
      version_audio_data_.patch    != data[3]) {
      // Store the data on the controller object
      version_audio_data_.major    = data[0];
      version_audio_data_.minor    = data[1];
      version_audio_data_.revision = data[2];
      version_audio_data_.patch    = data[3];
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_update_version_audio_data(network_id_get(), controller_id_get(), data, length);
         obj_network_rf4ce_->req_process_rib_export(controller_id_get(), CTRLM_HAL_RF4CE_RIB_ATTR_ID_UPDATE_VERSIONING, CTRLM_RF4CE_RIB_ATTR_INDEX_UPDATE_VERSIONING_AUDIO_DATA, CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING, data);

      }
   }

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_audio_data_.major, version_audio_data_.minor, version_audio_data_.revision, version_audio_data_.patch);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_update_version_audio_data(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0] = version_audio_data_.major;
   data[1] = version_audio_data_.minor;
   data[2] = version_audio_data_.revision;
   data[3] = version_audio_data_.patch;

   LOG_INFO("%s: %u.%u.%u.%u\n", __FUNCTION__, version_audio_data_.major, version_audio_data_.minor, version_audio_data_.revision, version_audio_data_.patch);
   
   return(CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING);
}

void ctrlm_obj_controller_rf4ce_t::property_write_product_name(const char *name) {
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME];
   if(name == NULL) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return;
   }

   errno_t safec_rc = strncpy_s((char *)data, sizeof(data), name, CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME-1);
   ERR_CHK(safec_rc);
   data[CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME - 1] = '\0';
   property_write_product_name(data, CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_product_name(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   if(0 != strncmp(product_name_, (const char *)data, CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME)) {
      // Store the data on the controller object
      errno_t safec_rc = strcpy_s(product_name_, sizeof(product_name_), (const char *)data);
      ERR_CHK(safec_rc);
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_product_name(network_id_get(), controller_id_get(), data, length);
         obj_network_rf4ce_->req_process_rib_export(controller_id_get(), CTRLM_HAL_RF4CE_RIB_ATTR_ID_PRODUCT_NAME, CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL, CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME, data);
      }

      controller_chipset_from_product_name();

      has_battery_ = ctrlm_rf4ce_has_battery(controller_type_);
      has_dsp_     = ctrlm_rf4ce_has_dsp(controller_type_);
   }

   LOG_INFO("%s: <%s>\n", __FUNCTION__, product_name_);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_product_name(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   // Load the data from the controller object
   errno_t safec_rc = strcpy_s((char *)data, CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE, product_name_);
   ERR_CHK(safec_rc);

   LOG_INFO("%s: <%s>\n", __FUNCTION__, product_name_);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME);
}

void ctrlm_obj_controller_rf4ce_t::property_write_download_rate(download_rate_t download_rate) {
   switch(download_rate) {
      case DOWNLOAD_RATE_IMMEDIATE:  LOG_INFO("%s: IMMEDIATE\n", __FUNCTION__); break;
      case DOWNLOAD_RATE_BACKGROUND: LOG_INFO("%s: BACKGROUND\n", __FUNCTION__); break;
      default:                       LOG_WARN("%s: UNKNOWN 0x%02X\n", __FUNCTION__, download_rate); return;
   }
   // Store the data on the controller object
   download_rate_ = download_rate;
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_download_rate(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DOWNLOAD_RATE) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   download_rate_t download_rate = (download_rate_t)data[0];
   property_write_download_rate(download_rate);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_download_rate(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DOWNLOAD_RATE) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // Load the data from the controller object
   data[0] = download_rate_;
   
   switch(download_rate_) {
      case DOWNLOAD_RATE_IMMEDIATE:  LOG_INFO("%s: IMMEDIATE\n", __FUNCTION__); break;
      case DOWNLOAD_RATE_BACKGROUND: LOG_INFO("%s: BACKGROUND\n", __FUNCTION__); break;
      default:                       LOG_WARN("%s: UNKNOWN 0x%02X\n", __FUNCTION__, download_rate_); break;
   }

   return(CTRLM_RF4CE_RIB_ATTR_LEN_DOWNLOAD_RATE);
}

void ctrlm_obj_controller_rf4ce_t::property_write_update_polling_period(guint16 update_polling_period) {
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   // Store the data on the controller object
   update_polling_period_ = update_polling_period;
   
   LOG_INFO("%s: %u hours\n", __FUNCTION__, update_polling_period_);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_update_polling_period(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_POLLING_PERIOD) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint16 update_polling_period = (data[1] << 8) | (data[0]);
   property_write_update_polling_period(update_polling_period);
   return(length);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   return(0);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_update_polling_period(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_POLLING_PERIOD) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint16 update_polling_period = update_polling_period_;
   #else
   guint16 update_polling_period = update_polling_period_get();
   #endif
   // Load the data from the controller object
   data[0]  = (guchar)update_polling_period;
   data[1]  = (guchar)(update_polling_period >> 8);
   
   LOG_INFO("%s: %u hours\n", __FUNCTION__, update_polling_period);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_POLLING_PERIOD);
}

void ctrlm_obj_controller_rf4ce_t::property_write_data_request_wait_time(guint16 data_request_wait_time) {
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   // Store the data on the controller object
   data_request_wait_time_ = data_request_wait_time;
   
   LOG_INFO("%s: %u ms\n", __FUNCTION__, data_request_wait_time_);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_data_request_wait_time(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DATA_REQUEST_WAIT_TIME) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint16 data_request_wait_time = (data[1] << 8) | (data[0]);
   property_write_data_request_wait_time(data_request_wait_time);
   return(length);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   return(0);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_data_request_wait_time(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DATA_REQUEST_WAIT_TIME) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint16 data_request_wait_time = data_request_wait_time_;
   #else
   guint16 data_request_wait_time = data_request_wait_time_get();
   #endif
   // Load the data from the controller object
   data[0]  = (guchar)data_request_wait_time;
   data[1]  = (guchar)(data_request_wait_time >> 8);
   
   LOG_INFO("%s: %u ms\n", __FUNCTION__, data_request_wait_time);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_DATA_REQUEST_WAIT_TIME);
}

void ctrlm_obj_controller_rf4ce_t::property_write_ir_rf_database_status(guchar status) {
   // ROBIN -- Add polling method if any bits are set
   if(controller_type_ == RF4CE_CONTROLLER_TYPE_XR19) {
      if((ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES) || (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE) || (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE)) {
         LOG_INFO("%s: IR-RF Database Status has bits set for XR19 (0x%02X), adding polling action\n", __FUNCTION__, ir_rf_database_status_);
         push_ir_codes();
      }
      return;
   }
   if(status & IR_RF_DATABASE_STATUS_RESERVED) {
      LOG_ERROR("%s: INVALID PARAMETERS - Reserved bit is set 0x%02X\n", __FUNCTION__, status);
      return;
   } else if((status & IR_RF_DATABASE_STATUS_DB_DOWNLOAD_NO) && (status & IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES)) {
      LOG_ERROR("%s: Status Download <YES and NO> Tx Ir <%s> Force <%s> Download TV 5-Digit IRDB Code <%s> Download AVR 5-Digit IRDB Code <%s> Clear All 5-Digit IRDB Codes <%s>\n", 
             __FUNCTION__, (status & IR_RF_DATABASE_STATUS_TX_IR_DESCRIPTOR) ? "YES" : "NO", (status & IR_RF_DATABASE_STATUS_FORCE_DOWNLOAD) ? "YES" : "NO", 
            (status & IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE) ? "YES" : "NO", (status & IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE) ? "YES" : "NO",
            (status & IR_RF_DATABASE_STATUS_CLEAR_ALL_5_DIGIT_CODES) ? "YES" : "NO");
      return;
   } else if(0 == (status & (IR_RF_DATABASE_STATUS_DB_DOWNLOAD_NO | IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES))) {
      LOG_ERROR("%s: Status Download <None> Tx Ir <%s> Force <%s> Download TV 5-Digit IRDB Code <%s> Download AVR 5-Digit IRDB Code <%s> Clear All 5-Digit IRDB Codes <%s>\n", 
             __FUNCTION__, (status & IR_RF_DATABASE_STATUS_TX_IR_DESCRIPTOR) ? "YES" : "NO", (status & IR_RF_DATABASE_STATUS_FORCE_DOWNLOAD) ? "YES" : "NO",
            (status & IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE) ? "YES" : "NO", (status & IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE) ? "YES" : "NO",
            (status & IR_RF_DATABASE_STATUS_CLEAR_ALL_5_DIGIT_CODES) ? "YES" : "NO");
      return;
   } else if((status & IR_RF_DATABASE_STATUS_CLEAR_ALL_5_DIGIT_CODES) && ((status & IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE) || (status & IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE))) {
      LOG_ERROR("%s: Status Download <%s> Tx Ir <%s> Force <%s> 5-Digit IRDB Code <SET and CLEAR>\n", 
             __FUNCTION__, (status & IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES) ? "YES" : "NO", (status & IR_RF_DATABASE_STATUS_TX_IR_DESCRIPTOR) ? "YES" : "NO", (status & IR_RF_DATABASE_STATUS_FORCE_DOWNLOAD) ? "YES" : "NO");
      return;
   } else if(((status & IR_RF_DATABASE_STATUS_CLEAR_ALL_5_DIGIT_CODES) || (status & IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE) || (status & IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE)) && (status & IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES)) {
      LOG_ERROR("%s: Status Download <YES> Set OR Clear 5-Digit IRDB Codes <YES> Download TV 5-Digit IRDB Code <%s> Download AVR 5-Digit IRDB Code <%s> Clear All 5-Digit IRDB Codes <%s>\n", __FUNCTION__,
            (status & IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE) ? "YES" : "NO", (status & IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE) ? "YES" : "NO",
            (status & IR_RF_DATABASE_STATUS_CLEAR_ALL_5_DIGIT_CODES) ? "YES" : "NO");
      return;
   }

   // Store the data on the controller object
   ir_rf_database_status_ = status;
   obj_network_rf4ce_->req_process_rib_export(controller_id_get(), CTRLM_HAL_RF4CE_RIB_ATTR_ID_IR_RF_DATABASE_STATUS, CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL, CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS, &status);

   LOG_INFO("%s: Status Download <%s> Tx Ir <%s> Force <%s> Download TV 5-Digit IRDB Code <%s> Download AVR 5-Digit IRDB Code <%s> Clear All 5-Digit IRDB Codes <%s>\n", 
         __FUNCTION__, (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES) ? "YES" : "NO", (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_TX_IR_DESCRIPTOR) ? "YES" : "NO",
         (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_FORCE_DOWNLOAD) ? "YES" : "NO", (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE) ? "YES" : "NO",
         (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE) ? "YES" : "NO",(ir_rf_database_status_ & IR_RF_DATABASE_STATUS_CLEAR_ALL_5_DIGIT_CODES) ? "YES" : "NO");
}

static gboolean ir_rf_database_status_download_timeout(gpointer data) {
   LOG_INFO("%s\n", __FUNCTION__);
   ctrlm_obj_controller_rf4ce_t *rf4ce_controller = (ctrlm_obj_controller_rf4ce_t *)data;
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_CONTROLLER, (ctrlm_msg_handler_controller_t)&ctrlm_obj_controller_rf4ce_t::ir_rf_database_status_download_reset, NULL, 0, rf4ce_controller);
   return(FALSE);
}

void ctrlm_obj_controller_rf4ce_t::ir_rf_database_status_download_reset(void *data, int size) {
   LOG_INFO("%s: Resetting IR RF Status Download flag.\n", __FUNCTION__);
   ir_rf_database_status_ = IR_RF_DATABASE_STATUS_DEFAULT;
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_ir_rf_database_status(guchar *data, guchar length, bool target) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   // ROBIN -- Clear IR-RF database status
   if(controller_type_ == RF4CE_CONTROLLER_TYPE_XR19) {
      if(target) {
         data[0] = IR_RF_DATABASE_STATUS_DEFAULT;
      } else {
         data[0] = ir_rf_database_status_;
         ir_rf_database_status_ = IR_RF_DATABASE_STATUS_DEFAULT;
      }
      return(CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS);
   }

   // Load the data from the controller object
   data[0] = ir_rf_database_status_;
   
   // HACK for XR15-704
#ifdef XR15_704
   if(needs_reset_ && !ctrlm_device_update_is_controller_updating(network_id_get(), controller_id_get(), true)) {
      if(ctrlm_device_update_xr15_crash_update_get()) {
         LOG_INFO("%s: ENTERING XR15 CRASH CODE: XR15-10 running less then 2.0.0.0, need to force reboot for device update... Setting proper IR RF Status bits\n", __FUNCTION__);
         data[0] = IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES | IR_RF_DATABASE_STATUS_FORCE_DOWNLOAD;
         LOG_INFO("%s: MODIFIED Status Download <%s> Tx Ir <%s> Force <%s> Download TV 5-Digit IRDB Code <%s> Download AVR 5-Digit IRDB Code <%s> Clear All 5-Digit IRDB Codes <%s>\n", 
               __FUNCTION__, (data[0] & IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES) ? "YES" : "NO", (data[0] & IR_RF_DATABASE_STATUS_TX_IR_DESCRIPTOR) ? "YES" : "NO",
               (data[0] & IR_RF_DATABASE_STATUS_FORCE_DOWNLOAD) ? "YES" : "NO", (data[0] & IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE) ? "YES" : "NO",
               (data[0] & IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE) ? "YES" : "NO",(data[0] & IR_RF_DATABASE_STATUS_CLEAR_ALL_5_DIGIT_CODES) ? "YES" : "NO");
      }
   }
#endif
   // HACK for XR15-704

   if((ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DB_DOWNLOAD_NO) && (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES)) {
      LOG_WARN("%s: Status Download <YES and NO> Tx Ir <%s> Force <%s> Download TV 5-Digit IRDB Code <%s> Download AVR 5-Digit IRDB Code <%s> Clear All 5-Digit IRDB Codes <%s>\n", 
             __FUNCTION__, (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_TX_IR_DESCRIPTOR) ? "YES" : "NO", (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_FORCE_DOWNLOAD) ? "YES" : "NO", 
            (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE) ? "YES" : "NO", (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE) ? "YES" : "NO",
            (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_CLEAR_ALL_5_DIGIT_CODES) ? "YES" : "NO");
   } else if(0 == (ir_rf_database_status_ & (IR_RF_DATABASE_STATUS_DB_DOWNLOAD_NO | IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES))) {
      LOG_WARN("%s: Status Download <None> Tx Ir <%s> Force <%s> Download TV 5-Digit IRDB Code <%s>Download AVR 5-Digit IRDB Code <%s> Clear All 5-Digit IRDB Codes <%s>\n", 
             __FUNCTION__, (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_TX_IR_DESCRIPTOR) ? "YES" : "NO", (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_FORCE_DOWNLOAD) ? "YES" : "NO",
            (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE) ? "YES" : "NO", (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE) ? "YES" : "NO",
            (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_CLEAR_ALL_5_DIGIT_CODES) ? "YES" : "NO");
   } else {
      LOG_INFO("%s: Status Download <%s> Tx Ir <%s> Force <%s> Download TV 5-Digit IRDB Code <%s> Download AVR 5-Digit IRDB Code <%s> Clear All 5-Digit IRDB Codes <%s>\n", 
            __FUNCTION__, (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES) ? "YES" : "NO", (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_TX_IR_DESCRIPTOR) ? "YES" : "NO",
            (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_FORCE_DOWNLOAD) ? "YES" : "NO", (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE) ? "YES" : "NO",
            (ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE) ? "YES" : "NO",(ir_rf_database_status_ & IR_RF_DATABASE_STATUS_CLEAR_ALL_5_DIGIT_CODES) ? "YES" : "NO");
   }

   if(ir_rf_database_status_ & IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES) {
      LOG_INFO("%s: Creating timer for download flag reset\n", __FUNCTION__);
      ctrlm_timeout_create(200, ir_rf_database_status_download_timeout, (void *)this);
   }
   
   return(CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_ir_rf_database(guchar index, guchar *data, guchar length) {
   // ROBIN -- Only accept ir waveforms that we do not have
   if(controller_type_ == RF4CE_CONTROLLER_TYPE_XR19) {
      guchar temp[CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE] = {'\0'};
      if(obj_network_rf4ce_->property_read_ir_rf_database(index, temp, sizeof(temp)) > 0) {
         if(!(temp[0] & IR_RF_DATABASE_ATTRIBUTE_FLAGS_USE_DEFAULT) && !is_ir_code_to_be_cleared(data, length)) {
            LOG_INFO("%s: XRE set an IR code for XR19 which we have already, ignore..\n", __FUNCTION__);
            return(length);
         }
      }
      ir_rf_database_status_ |=   IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES;
      ir_rf_database_status_ &= ~(IR_RF_DATABASE_STATUS_DB_DOWNLOAD_NO);
   }
   return(obj_network_rf4ce_->property_write_ir_rf_database(index, data, length));
}

bool ctrlm_obj_controller_rf4ce_t::is_ir_code_to_be_cleared(guchar *data, guchar length) {
   guchar *current_ptr = data + 1;
   guchar test_byte;
   if(length > CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE || data == NULL) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(false);
   }
   test_byte = IR_RF_DATABASE_ATTRIBUTE_FLAGS_PERMANENT + IR_RF_DATABASE_ATTRIBUTE_FLAGS_USE_DEFAULT;
   if(data[0] != test_byte) {
      LOG_DEBUG("%s: First byte not 0x%x.  Do not clear.\n", __FUNCTION__, test_byte);
      return false;
   }
   test_byte = 0;
   for(int i=0; i<length-1; i++) {
      if (current_ptr[i] != test_byte) {
         LOG_DEBUG("%s: <%d> byte not 0x%02x.  Do not clear.\n", __FUNCTION__, i, test_byte);
         return false;
      }
   }
   LOG_INFO("%s: IR Code should be cleared\n", __FUNCTION__);
   return true;
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_ir_rf_database(guchar index, guchar *data, guchar length) {
   guchar len = 0;
   // ROBIN -- IR-RF DB Read logic
   if(controller_type_ == RF4CE_CONTROLLER_TYPE_XR19) {
      switch(index) {
         case CTRLM_KEY_CODE_TV_POWER_ON:
         case CTRLM_KEY_CODE_TV_POWER_OFF:
         case CTRLM_KEY_CODE_AVR_POWER_ON:
         case CTRLM_KEY_CODE_AVR_POWER_OFF: {
            LOG_INFO("%s: Allowing XR19 to read discrete ir code\n", __FUNCTION__);
            return(obj_network_rf4ce_->property_read_ir_rf_database(index, data, length));
            break;
         }
         case CTRLM_KEY_CODE_VOL_UP:
         case CTRLM_KEY_CODE_VOL_DOWN:
         case CTRLM_KEY_CODE_MUTE:
         case CTRLM_KEY_CODE_INPUT_SELECT: {
            if(!(this->obj_network_rf4ce_->target_irdb_status_flags_get() & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_TV || this->obj_network_rf4ce_->target_irdb_status_flags_get() & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR)) {
               LOG_INFO("%s: Allowing XR19 to read ir code\n", __FUNCTION__);
               return(obj_network_rf4ce_->property_read_ir_rf_database(index, data, length));
            } else {
               LOG_INFO("%s: Disallowing XR19 to read ir code, already a 5 digit code available\n", __FUNCTION__);
               data[0] = IR_RF_DATABASE_ATTRIBUTE_FLAGS_USE_DEFAULT;
               return(1);
            }
            break;
         }
         case CTRLM_KEY_CODE_TV_POWER: {
            if(!(this->obj_network_rf4ce_->target_irdb_status_flags_get() & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_TV)) {
               LOG_INFO("%s: Allowing XR19 to read ir code\n", __FUNCTION__);
               return(obj_network_rf4ce_->property_read_ir_rf_database(index, data, length));
            } else {
               LOG_INFO("%s: Disallowing XR19 to read ir code, already a 5 digit code available\n", __FUNCTION__);
               data[0] = IR_RF_DATABASE_ATTRIBUTE_FLAGS_USE_DEFAULT;
               return(1);
            }
            break;
         }
         case CTRLM_KEY_CODE_AVR_POWER_TOGGLE: {
            if(!(this->obj_network_rf4ce_->target_irdb_status_flags_get() & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR)) {
               LOG_INFO("%s: Allowing XR19 to read ir code\n", __FUNCTION__);
               return(obj_network_rf4ce_->property_read_ir_rf_database(index, data, length));
            } else {
               LOG_INFO("%s: Disallowing XR19 to read ir code, already a 5 digit code available\n", __FUNCTION__);
               data[0] = IR_RF_DATABASE_ATTRIBUTE_FLAGS_USE_DEFAULT;
               return(1);
            }
            break;
         }
         default: {
            break;
         }
      }
   }
   len = obj_network_rf4ce_->property_read_ir_rf_database(index, data, length);
   // HACK for XR15-704
#ifdef XR15_704
   if(needs_reset_) {
      if(!ctrlm_device_update_is_controller_updating(network_id_get(), controller_id_get(), true)) {
         if(len < 2) {
            len = 0x32;
            // If IR codes do not exist, inject some fake data
            data[0] = 0x8C;
            data[1] = 0x01;
            data[2] = 0x4C;
            data[3] = 0x02;
            data[4] = 0x31;
            data[5] = 0x06;
            data[6] = 0x00;
         }
         // Set the IR code length to 255
         data[7] = 0xFF;
         did_reset_ = true;
      }
      needs_reset_ = false;
      LOG_INFO("%s: EXITING XR15 CRASH CODE: XR15 was reset <%s>\n", __FUNCTION__, (did_reset_ ? "TRUE" : "FALSE"));
   }
#endif
   // HACK for XR15-704
   return(len);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_target_irdb_status(guchar *data, guchar length) {
   return(obj_network_rf4ce_->property_write_target_irdb_status(data, length));
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_target_irdb_status(guchar *data, guchar length) {
   return(obj_network_rf4ce_->property_read_target_irdb_status(data, length));
}

void ctrlm_obj_controller_rf4ce_t::property_write_validation_configuration(guint16 auto_check_validation_period, guint16 link_lost_wait_time) {
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   // Store the data on the controller object
   auto_check_validation_period_ = auto_check_validation_period;
   link_lost_wait_time_          = link_lost_wait_time;

   LOG_INFO("%s: auto check validation period %u ms\n", __FUNCTION__, auto_check_validation_period_);
   LOG_INFO("%s: link lost wait time %u ms\n", __FUNCTION__, link_lost_wait_time_);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_validation_configuration(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VALIDATION_CONFIGURATION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint16 auto_check_validation_period = (data[1] << 8) | (data[0]);
   guint16 link_lost_wait_time          = (data[3] << 8) | (data[2]);
   property_write_validation_configuration(auto_check_validation_period, link_lost_wait_time);
   return(length);
   #else
   LOG_WARN("%s: Can't set network attribute\n", __FUNCTION__);
   return(0);
   #endif
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_validation_configuration(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VALIDATION_CONFIGURATION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   guint16 auto_check_validation_period = auto_check_validation_period_;
   guint16 link_lost_wait_time          = link_lost_wait_time_;
   #else
   guint16 auto_check_validation_period = auto_check_validation_period_get();
   guint16 link_lost_wait_time          = link_lost_wait_time_get();
   #endif
   // Load the data from the controller object
   data[0] = (guchar) (auto_check_validation_period);
   data[1] = (guchar) (auto_check_validation_period >> 8);
   data[2] = (guchar) (link_lost_wait_time);
   data[3] = (guchar) (link_lost_wait_time >> 8);

   LOG_INFO("%s: auto check validation period %u ms\n", __FUNCTION__, auto_check_validation_period);
   LOG_INFO("%s: link lost wait time %u ms\n", __FUNCTION__, link_lost_wait_time);
   
   return(CTRLM_RF4CE_RIB_ATTR_LEN_VALIDATION_CONFIGURATION);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_controller_irdb_status(guchar *data, guchar length) {
   if((length != CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS) && (length != CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS_MINUS_LOAD_STATUS_BYTES)) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   controller_irdb_status_t controller_irdb_status;

   controller_irdb_status.flags                = data[0];
   controller_irdb_status.irdb_string_tv[0]    = data[1];
   controller_irdb_status.irdb_string_tv[1]    = data[2];
   controller_irdb_status.irdb_string_tv[2]    = data[3];
   controller_irdb_status.irdb_string_tv[3]    = data[4];
   controller_irdb_status.irdb_string_tv[4]    = data[5];
   controller_irdb_status.irdb_string_tv[5]    = data[6];
   controller_irdb_status.irdb_string_tv[6]    = '\0';
   controller_irdb_status.irdb_string_avr[0]   = data[7];
   controller_irdb_status.irdb_string_avr[1]   = data[8];
   controller_irdb_status.irdb_string_avr[2]   = data[9];
   controller_irdb_status.irdb_string_avr[3]   = data[10];
   controller_irdb_status.irdb_string_avr[4]   = data[11];
   controller_irdb_status.irdb_string_avr[5]   = data[12];
   controller_irdb_status.irdb_string_avr[6]   = '\0';
   if(length == CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS) {
      controller_irdb_status.irdb_load_status_tv  = data[13];
      controller_irdb_status.irdb_load_status_avr = data[14];
   } else {
      data[13] = controller_irdb_status.irdb_load_status_tv  = 0;
      data[14] = controller_irdb_status.irdb_load_status_avr = 0;
      LOG_INFO("%s: Length <%d> does not include load status bytes.\n", __FUNCTION__, length);
   }

   if(controller_irdb_status_.flags != controller_irdb_status.flags || memcmp(controller_irdb_status_.irdb_string_tv,  controller_irdb_status.irdb_string_tv, 6)
                                                            || memcmp(controller_irdb_status_.irdb_string_avr, controller_irdb_status.irdb_string_avr, 6)
                                                            || (controller_irdb_status_.irdb_load_status_tv != controller_irdb_status.irdb_load_status_tv)
                                                            || (controller_irdb_status_.irdb_load_status_avr != controller_irdb_status.irdb_load_status_avr)
                                                            || obj_network_rf4ce_->is_ir_rf_database_new()) {
      // Store the data on the controller object
      controller_irdb_status_ = controller_irdb_status;
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_controller_irdb_status(network_id_get(), controller_id_get(), data, CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS);

         //Needed by target Irdb status to know if we need to update
         controller_irdb_status_is_new_ = true;
      }
   }

   LOG_INFO("%s: Flags 0x%02X TV %s AVR %s TV_Load_Status <%s> AVR_Load_Status <%s>\n", __FUNCTION__, controller_irdb_status_.flags, controller_irdb_status_.irdb_string_tv, controller_irdb_status_.irdb_string_avr,
                                                                          controller_irdb_status_load_status_str(controller_irdb_status_.irdb_load_status_tv),
                                                                          controller_irdb_status_load_status_str(controller_irdb_status_.irdb_load_status_avr));
   return(CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_controller_irdb_status(guchar *data, guchar length) {
   if((length != CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS) && (length != CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS_MINUS_LOAD_STATUS_BYTES)) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   // Load the data from the controller object
   data[0]  = controller_irdb_status_.flags;
   data[1]  = controller_irdb_status_.irdb_string_tv[0];
   data[2]  = controller_irdb_status_.irdb_string_tv[1];
   data[3]  = controller_irdb_status_.irdb_string_tv[2];
   data[4]  = controller_irdb_status_.irdb_string_tv[3];
   data[5]  = controller_irdb_status_.irdb_string_tv[4];
   data[6]  = controller_irdb_status_.irdb_string_tv[5];
   data[7]  = controller_irdb_status_.irdb_string_avr[0];
   data[8]  = controller_irdb_status_.irdb_string_avr[1];
   data[9]  = controller_irdb_status_.irdb_string_avr[2];
   data[10] = controller_irdb_status_.irdb_string_avr[3];
   data[11] = controller_irdb_status_.irdb_string_avr[4];
   data[12] = controller_irdb_status_.irdb_string_avr[5];
   if(length == CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS) {
      data[13] = controller_irdb_status_.irdb_load_status_tv;
      data[14] = controller_irdb_status_.irdb_load_status_avr;
   } else {
      controller_irdb_status_.irdb_load_status_tv  = 0;
      controller_irdb_status_.irdb_load_status_avr = 0;
      LOG_INFO("%s: Length <%d> does not include load status bytes.\n", __FUNCTION__, length);
   }


   LOG_INFO("%s: Flags 0x%02X TV %s AVR %s TV_Load_Status <%s> AVR_Load_Status <%s>\n", __FUNCTION__, controller_irdb_status_.flags, controller_irdb_status_.irdb_string_tv, controller_irdb_status_.irdb_string_avr,
                                                                          controller_irdb_status_load_status_str(controller_irdb_status_.irdb_load_status_tv),
                                                                          controller_irdb_status_load_status_str(controller_irdb_status_.irdb_load_status_avr));
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_irdb_entry_id_name_tv(guchar *data, guchar length) {
   gchar irdb_entry_id_name_tv[CTRLM_RF4CE_RIB_ATTR_LEN_IRDB_ENTRY_ID_NAME];
   if(length <= 0) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   errno_t safec_rc = -1;
   // Clear the array
   safec_rc = memset_s(irdb_entry_id_name_tv, length, 0, length);
   ERR_CHK(safec_rc);
   // Copy IRDB Code to data buf
   // Can't be replaced with safeC version of this, as safeC string functions have limitations on buffer sizes
   strncpy(irdb_entry_id_name_tv, (gchar *)data, length);
   irdb_entry_id_name_tv[sizeof(irdb_entry_id_name_tv)-1] = 0; // in case length == sizeof(irdb_entry_id_name_tv)

   if(!strncmp(irdb_entry_id_name_tv_.c_str(), irdb_entry_id_name_tv, length)) {
      // Store the data on the controller object
      irdb_entry_id_name_tv_ = std::string(irdb_entry_id_name_tv, length);
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_irdb_entry_id_name_tv(network_id_get(), controller_id_get(), data, length);
      }
   }

   LOG_INFO("%s: TV IRDB Code <%s>\n", __FUNCTION__, irdb_entry_id_name_tv_.c_str());
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_irdb_entry_id_name_tv(guchar *data, guchar length) {
   guchar ret = 0;

   if(irdb_entry_id_name_tv_.length() > 0) {
      errno_t safec_rc = -1;
      // Clear the array
      safec_rc = memset_s(data, length, 0, length);
      ERR_CHK(safec_rc);

      ret = (irdb_entry_id_name_tv_.length() > length ? length : irdb_entry_id_name_tv_.length());
      // Copy IRDB Code to data buf
      // Can't be replaced with safeC version of this, as safeC string functions have limitations on buffer sizes
      strncpy((gchar *)data, irdb_entry_id_name_tv_.c_str(), ret);
      data[length-1] = 0; // in case irdb_entry_id_name_tv_.length() == length
   }

   LOG_INFO("%s: TV IRDB Code <%s>\n", __FUNCTION__, (irdb_entry_id_name_tv_.length() > 0 ? irdb_entry_id_name_tv_.c_str() : "NULL"));
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_irdb_entry_id_name_avr(guchar *data, guchar length) {
   gchar irdb_entry_id_name_avr[CTRLM_RF4CE_RIB_ATTR_LEN_IRDB_ENTRY_ID_NAME];
   if(length <= 0) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   errno_t safec_rc = -1;
   // Clear the array
   safec_rc = memset_s(irdb_entry_id_name_avr, length, 0, length);
   ERR_CHK(safec_rc);
   // Copy IRDB Code to data buf
   // Can't be replaced with safeC version of this, as safeC string functions have limitations on buffer sizes
   strncpy(irdb_entry_id_name_avr, (gchar *)data, length);
   irdb_entry_id_name_avr[sizeof(irdb_entry_id_name_avr)-1] = 0; // in case length == sizeof(irdb_entry_id_name_avr)

   if(!strncmp(irdb_entry_id_name_avr_.c_str(), irdb_entry_id_name_avr, length)) {
      // Store the data on the controller object
      irdb_entry_id_name_avr_ = std::string(irdb_entry_id_name_avr, length);
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_irdb_entry_id_name_avr(network_id_get(), controller_id_get(), data, length);
      }
   }

   LOG_INFO("%s: AVR IRDB Code <%s>\n", __FUNCTION__, irdb_entry_id_name_avr_.c_str());
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_irdb_entry_id_name_avr(guchar *data, guchar length) {
   guchar ret = 0;

   if(irdb_entry_id_name_avr_.length() > 0) {
      errno_t safec_rc = -1;
      // Clear the array
      safec_rc = memset_s(data, length, 0, length);
      ERR_CHK(safec_rc);

      ret = (irdb_entry_id_name_avr_.length() > length ? length : irdb_entry_id_name_avr_.length());
      // Copy IRDB Code to data buf
      // Can't be replaced with safeC version of this, as safeC string functions have limitations on buffer sizes
      strncpy((gchar *)data, irdb_entry_id_name_avr_.c_str(), ret);
      data[length-1] = 0; // in case irdb_entry_id_name_avr_.length() == length
   }

   LOG_INFO("%s: AVR IRDB Code <%s>\n", __FUNCTION__, (irdb_entry_id_name_avr_.length() > 0 ? irdb_entry_id_name_avr_.c_str() : "NULL"));
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_receiver_id(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_ID_DATA) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   // We do not want to support overwriting the receiver id
   LOG_WARN("%s: Wrting the receiver id is NOT allowed\n", __FUNCTION__);
   return(0);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_receiver_id(guchar *data, guchar length) {
   std::string receiver_id;
   guchar      len;

   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_ID_DATA) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   receiver_id = receiver_id_get();
   len = (receiver_id.length() > length ? length : receiver_id.length());

   // Copy receiver id to data buf
   errno_t safec_rc = strncpy_s((gchar *)data, CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE, receiver_id.c_str(),len);
   ERR_CHK(safec_rc);

   return(len);

}

guchar ctrlm_obj_controller_rf4ce_t::property_write_device_id(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_ID_DATA) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   // We do not want to support overwriting the receiver id
   LOG_WARN("%s: Wrting the device id is NOT allowed\n", __FUNCTION__);
   return(0);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_device_id(guchar *data, guchar length) {
   std::string device_id;
   guchar      len;

   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_ID_DATA) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   device_id = device_id_get();
   len = (device_id.length() > length ? length : device_id.length());

   // Copy receiver id to data buf
   errno_t safec_rc = strncpy_s((gchar *)data, CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE, device_id.c_str(),len);
   ERR_CHK(safec_rc);

   return(len);

}

guchar ctrlm_obj_controller_rf4ce_t::property_write_memory_dump(guchar index, guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_DUMP) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   int buf_index = 0;
   guint16 memory_size = 0;
   errno_t safec_rc = -1;

   LOG_INFO("%s: Index %u: %d bytes received\n", __FUNCTION__, index, length);

    // first chunk of data, Index 0 of memory dump RIB
   if(index == 0) {
      // check if ctrlm_db_rf4ce_write_file() was never called and memory was not freed
      if (crash_dump_buf_ != NULL) {
          LOG_WARN("%s: Not all the Remote Memory Dump data has been received!\n", __FUNCTION__);
          ctrlm_hal_free(crash_dump_buf_);
          crash_dump_buf_ = NULL;
      }

      memory_size = (guint16)data[CTRLM_CRASH_DUMP_OFFSET_MEMORY_SIZE + 1] << 8 |
                     (guint16)data[CTRLM_CRASH_DUMP_OFFSET_MEMORY_SIZE];
       // validate if Memory Size is in valid range
       if(memory_size == 0 || memory_size > CTRLM_CRASH_DUMP_MAX_SIZE) {
          LOG_ERROR("%s: Memory Size value %hu is bad\n", __FUNCTION__, memory_size);
          return(0);
       }

       // perform extra checking if Reserved bytes are good
       const guchar* data_end =  data + length - CTRLM_CRASH_DUMP_OFFSET_RESERVED_0xFF;
       if(data_end == (guchar *)find((const char*)data + CTRLM_CRASH_DUMP_OFFSET_RESERVED_0xFF, (const char*)data_end, '\xFF')) {
          LOG_ERROR("%s: Reserved bytes are bad\n", __FUNCTION__ );
          ctrlm_print_data_hex(__FUNCTION__, data + CTRLM_CRASH_DUMP_OFFSET_RESERVED_0xFF, length - CTRLM_CRASH_DUMP_OFFSET_RESERVED_0xFF, 32);
          return(0);
       }

       // normally memory  will be released in ctrlm_db_rf4ce_write_file() after write completes
       crash_dump_buf_ = (guchar *)ctrlm_hal_malloc(memory_size + CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_SIZE);
       if(crash_dump_buf_ == NULL) {
          LOG_ERROR("%s: Crash Dump Memory Allocation failed\n", __FUNCTION__);
          return(0);
       }
       crash_dump_expected_size_ = memory_size;
       crash_dump_size_          = 0;

       guint16 memory_id = (guint16)data[CTRLM_CRASH_DUMP_OFFSET_MEMORY_IDENTIFIER + 1] << 8 |
                           (guint16)data[CTRLM_CRASH_DUMP_OFFSET_MEMORY_IDENTIFIER];

       LOG_INFO("%s: Expected crash dump size %hu bytes. Memory Identifier 0x%04X\n",
                     __FUNCTION__, memory_size,  memory_id);
       // Fill out File Header
       // signature XDMP
       safec_rc = strcpy_s((char*)crash_dump_buf_, memory_size + CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_SIZE, "XDMP");
       if(safec_rc != EOK) {
         ERR_CHK(safec_rc);
         ctrlm_hal_free(crash_dump_buf_);
         crash_dump_buf_ = NULL;
         return(0);
       }
       // header size
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_HDR_SIZE]     = CTRLM_CRASH_DUMP_FILE_HDR_SIZE & 0xFF;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_HDR_SIZE + 1] = CTRLM_CRASH_DUMP_FILE_HDR_SIZE >> 8;
       // device ID
       safec_rc = strncpy_s((char *)crash_dump_buf_ + CTRLM_CRASH_DUMP_FILE_OFFSET_DEVICE_ID, memory_size + CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_SIZE - CTRLM_CRASH_DUMP_FILE_OFFSET_DEVICE_ID, ctrlm_rf4ce_controller_type_str(controller_type_), CTRLM_CRASH_DUMP_FILE_LEN_DEVICE_ID);
       if(safec_rc != EOK) {
         ERR_CHK(safec_rc);
         ctrlm_hal_free(crash_dump_buf_);
         crash_dump_buf_ = NULL;
         return(0);
       }
       // HW Version
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_HW_VER]     = version_hardware_.manufacturer << 4 | version_hardware_.model;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_HW_VER + 1] = version_hardware_.hw_revision;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_HW_VER + 2] = version_hardware_.lot_code >> 8;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_HW_VER + 3] = version_hardware_.lot_code & 0xFF;
       // SW Version
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_SW_VER]     = version_software_.major;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_SW_VER + 1] = version_software_.minor;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_SW_VER + 2] = version_software_.revision;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_SW_VER + 3] = version_software_.patch;
       // Num dumps in file
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_NUM_DUMPS]     = 1; // for now file is written when first dump is received
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_OFFSET_NUM_DUMPS + 1] = 0;
       // Remote Crash FILE Data Header
       // Memory Identifier
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_OFFSET_MEM_ID]     = memory_id & 0xFF;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_OFFSET_MEM_ID + 1] = memory_id >> 8;
       // offset to dump data
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_OFFSET_DATA]     = (CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_SIZE) & 0xFF;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_OFFSET_DATA + 1] = (CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_SIZE) >> 8  & 0xFF;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_OFFSET_DATA + 2] = (CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_SIZE) >> 16 & 0xFF;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_OFFSET_DATA + 3] = (CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_SIZE) >> 24;
       // dump data length
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_OFFSET_DATA_LEN]     = crash_dump_expected_size_ & 0xFF;
       crash_dump_buf_[CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_OFFSET_DATA_LEN + 1] = crash_dump_expected_size_ >> 8;

       return(CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_DUMP);
    }

    // Process Index 1-255
    // Sanity check
    if(crash_dump_buf_ == NULL || crash_dump_expected_size_ <= crash_dump_size_) {
       LOG_ERROR("%s: Unexpected data chunk!\n", __FUNCTION__ );
       return(0); // something is really screwed!
    }
    // If the memory dump is not a multiple of 16, the final index of the memory dump MUST be structured as:
    // Bytes (Memory Size % 16) - Memory Data
    // Bytes 16 - (Memory Size % 16) - Reserved (0xFF)
    if(crash_dump_size_ + length > crash_dump_expected_size_) {
       length = crash_dump_expected_size_ % CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_DUMP;
    }

    buf_index = CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_SIZE + crash_dump_size_;
    safec_rc = memcpy_s(&crash_dump_buf_[buf_index],memory_size + CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_SIZE - buf_index, data, length);
    if(safec_rc != EOK) {
      ERR_CHK(safec_rc);
      ctrlm_hal_free(crash_dump_buf_);
      crash_dump_buf_ = NULL;
      return(0);
    }
    crash_dump_size_ += length;

    // Check if transfer is completed
    if(crash_dump_size_ == crash_dump_expected_size_) {
       // generate binary UUID
       uuid_t uuid_binary_value;
       uuid_generate(uuid_binary_value);
       // convert to string
       char uuid_string[CTRLM_RF4CE_LEN_UUID_STRING];
       uuid_unparse_lower(uuid_binary_value, uuid_string);
       // generate full path name
       ostringstream dump_path;
       dump_path << ctrlm_minidump_path_get() << '/' <<  uuid_string << ".xr";
       // handle writing to file on db thread to release RIB thread
       ctrlm_db_rf4ce_write_file(dump_path.str().c_str(), crash_dump_buf_, crash_dump_size_ + CTRLM_CRASH_DUMP_FILE_HDR_SIZE + CTRLM_CRASH_DUMP_DATA_HDR_SIZE);
       crash_dump_buf_ = NULL;
    }

   return(CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_DUMP);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_reboot_stats(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_REBOOT_STATS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   ctrlm_voice_reset_type_t reboot_reason = CTRLM_VOICE_RESET_TYPE_OTHER;
   reboot_reason_          = (controller_reboot_reason_t)data[0];
   reboot_voltage_level_   = data[1];
   reboot_assert_number_   = (data[5] <<24) | (data[4] << 16) | (data[3] << 8) | data[2];
   reboot_time_            = time(NULL);

   // Store the data on the controller object
   if(data[0] == CONTROLLER_REBOOT_POWER_ON) {
       LOG_INFO("%s: Reset Reason - Power-on reset and brownout detection\n", __FUNCTION__);
       reboot_reason = CTRLM_VOICE_RESET_TYPE_POWER_ON;
   } else if(data[0] == CONTROLLER_REBOOT_EXTERNAL) {
       LOG_INFO("%s: Reset Reason - External reset\n", __FUNCTION__);
       reboot_reason = CTRLM_VOICE_RESET_TYPE_EXTERNAL;
   } else if(data[0] == CONTROLLER_REBOOT_WATCHDOG) {
       LOG_INFO("%s: Reset Reason - Watchdog Timer reset\n", __FUNCTION__);
       reboot_reason = CTRLM_VOICE_RESET_TYPE_WATCHDOG;
   } else if(data[0] == CONTROLLER_REBOOT_CLOCK_LOSS) {
       LOG_INFO("%s: Reset Reason - Clock loss reset\n", __FUNCTION__);
       reboot_reason = CTRLM_VOICE_RESET_TYPE_CLOCK_LOSS;
   } else if(data[0] == CONTROLLER_REBOOT_BROWN_OUT) {
       LOG_INFO("%s: Reset Reason - Brown out reset\n", __FUNCTION__);
       reboot_reason = CTRLM_VOICE_RESET_TYPE_BROWN_OUT;
   } else if(data[0] == CONTROLLER_REBOOT_OTHER) {
       LOG_INFO("%s: Reset Reason - Other reset\n", __FUNCTION__);
   } else if(data[0] == CONTROLLER_REBOOT_ASSERT_NUMBER) {
       LOG_INFO("%s: Reset Reason - Assert Number reset <%u>\n", __FUNCTION__, reboot_assert_number_);
   } else {
       LOG_INFO("%s: Reset Reason - UNKNOWN (%u)\n", __FUNCTION__, data[0]);
   }
   LOG_INFO("%s: voltage %.2fV\n", __FUNCTION__, (((float)data[1]) * 4.0 / 255));

   print_firmware_on_button_press = true;
   print_remote_firmware_debug_info(RF4CE_PRINT_FIRMWARE_LOG_REBOOT);

   // Send data to vrex session
   ctrlm_voice_notify_stats_reboot(network_id_get(), controller_id_get(), reboot_reason, data[1], battery_level_percent(data[1]));
   // Inform device update
   ctrlm_device_update_rf4ce_notify_reboot(network_id_get(), controller_id_get(), device_update_session_resume_support());
   //Save in the database
   property_write_reboot_diagnostics();
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_memory_stats(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_STATS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   // Store the data on the controller object
   memory_available_ = (data[1] << 8) | data[0];
   memory_largest_   = (data[3] << 8) | data[2];
   LOG_INFO("%s: Memory Available <%u> Largest <%u>\n", __FUNCTION__, memory_available_, memory_largest_);
   //Save in the database
   property_write_memory_statistics();
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_mfg_test(guchar *data, guchar length) {
   if((!haptics_feedback_supported_ && length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST) ||
      (haptics_feedback_supported_ && length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_HAPTICS)) {
      LOG_ERROR("%s: INVALID Length for controller with HAPTICS <%s>\n", __FUNCTION__, (haptics_feedback_supported_ ? "ENABLED" : "DISABLED"));
      return(0);
   }

   return(obj_network_rf4ce_->property_read_mfg_test(data, length));
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_mfg_test(guchar *data, guchar length) {
   if((!haptics_feedback_supported_ && length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST) ||
      (haptics_feedback_supported_ && length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_HAPTICS)) {
      LOG_ERROR("%s: INVALID Length for controller with HAPTICS <%s>\n", __FUNCTION__, (haptics_feedback_supported_ ? "ENABLED" : "DISABLED"));
      return(0);
   }

   return(obj_network_rf4ce_->property_write_mfg_test(data, length));
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_mfg_test_result(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_RESULT ) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   if(!obj_network_rf4ce_->mfg_test_enabled()) {
      LOG_ERROR("%s: Manufacturing testing is disabled\n", __FUNCTION__);
      return(0);
   }

   // Load the data from the controller object
   data[0]  = mfg_test_result_;

   LOG_INFO("%s: MFG Security Key Test Result <%u>\n", __FUNCTION__, mfg_test_result_);
   return(CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_RESULT);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_mfg_test_result(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_RESULT) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   if(!obj_network_rf4ce_->mfg_test_enabled()) {
      LOG_ERROR("%s: Manufacturing testing is disabled\n", __FUNCTION__);
      return(0);
   }

   guint8 mfg_test_result = data[0];

   if(mfg_test_result_ != mfg_test_result) {
      // Store the data on the controller object
      mfg_test_result_ = mfg_test_result;
      if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_rf4ce_write_mfg_test_result(network_id_get(), controller_id_get(), data, length);
      }
   }

   LOG_INFO("%s: MFG Security Key Test Result <%u>\n", __FUNCTION__, mfg_test_result_);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_polling_methods(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   polling_methods_ = (guint8)data[0];

   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_polling_methods(network_id_get(), controller_id_get(), polling_methods_);
   }

   LOG_INFO("%s: polling methods <%s><%s>\n", __FUNCTION__, ctrlm_rf4ce_controller_type_str(controller_type_get()), ctrlm_rf4ce_controller_polling_methods_str(polling_methods_));
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_polling_methods(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   // Load the data from the controller object
   data[0]  = polling_methods_;

   LOG_INFO("%s: polling methods <%s><%s>\n", __FUNCTION__, ctrlm_rf4ce_controller_type_str(controller_type_get()), ctrlm_rf4ce_controller_polling_methods_str(polling_methods_));
   return(CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_polling_configuration_heartbeat(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].trigger       = (data[1] << 8) + (data[0]); // Not Little Endian -- Bitfield
   polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].kp_counter    = (data[2]);
   polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].time_interval = (data[3]) + (data[4] << 8) + (data[5] << 16) + (data[6] << 24);
   polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].reserved      = (data[7]);

   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_polling_configuration_heartbeat(network_id_get(), controller_id_get(), data, length);
   }

   ctrlm_rf4ce_controller_polling_configuration_print(__FUNCTION__, "Heartbeat", &polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT]);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_polling_configuration_heartbeat(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   // Load the data from the controller object
   data[0]  = (polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].trigger)              & 0xFF;
   data[1]  = (polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].trigger       >> 8)   & 0xFF;
   data[2]  = (polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].kp_counter)           & 0xFF;
   data[3]  = (polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].time_interval)        & 0xFF;
   data[4]  = (polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].time_interval >> 8)   & 0xFF;
   data[5]  = (polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].time_interval >> 16)  & 0xFF;
   data[6]  = (polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].time_interval >> 24)  & 0xFF;
   data[7]  = (polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT].reserved)             & 0xFF;

   ctrlm_rf4ce_controller_polling_configuration_print(__FUNCTION__, "Heartbeat", &polling_configurations_[RF4CE_POLLING_METHOD_HEARTBEAT]);
   return(CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_polling_configuration_mac(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   
   polling_configurations_[RF4CE_POLLING_METHOD_MAC].trigger       = (data[1] << 8) + (data[0]);
   polling_configurations_[RF4CE_POLLING_METHOD_MAC].kp_counter    = (data[2]);
   polling_configurations_[RF4CE_POLLING_METHOD_MAC].time_interval = (data[3]) + (data[4] << 8) + (data[5] << 16) + (data[6] << 24);
   polling_configurations_[RF4CE_POLLING_METHOD_MAC].reserved      = (data[7]);

   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_polling_configuration_mac(network_id_get(), controller_id_get(), data, length);
   }

   ctrlm_rf4ce_controller_polling_configuration_print(__FUNCTION__, "MAC", &polling_configurations_[RF4CE_POLLING_METHOD_MAC]);
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_polling_configuration_mac(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   // Load the data from the controller object
   data[0]  = (polling_configurations_[RF4CE_POLLING_METHOD_MAC].trigger)              & 0xFF;
   data[1]  = (polling_configurations_[RF4CE_POLLING_METHOD_MAC].trigger       >> 8)   & 0xFF;
   data[2]  = (polling_configurations_[RF4CE_POLLING_METHOD_MAC].kp_counter)           & 0xFF;
   data[3]  = (polling_configurations_[RF4CE_POLLING_METHOD_MAC].time_interval)        & 0xFF;
   data[4]  = (polling_configurations_[RF4CE_POLLING_METHOD_MAC].time_interval >> 8)   & 0xFF;
   data[5]  = (polling_configurations_[RF4CE_POLLING_METHOD_MAC].time_interval >> 16)  & 0xFF;
   data[6]  = (polling_configurations_[RF4CE_POLLING_METHOD_MAC].time_interval >> 24)  & 0xFF;
   data[7]  = (polling_configurations_[RF4CE_POLLING_METHOD_MAC].reserved)             & 0xFF;

   ctrlm_rf4ce_controller_polling_configuration_print(__FUNCTION__, "MAC", &polling_configurations_[RF4CE_POLLING_METHOD_MAC]);
   return(CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_privacy(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_PRIVACY) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   privacy_ = data[0];

   LOG_INFO("%s: Privacy Mode <%s>\n", __FUNCTION__, (privacy_ & PRIVACY_FLAGS_ENABLED ? "ENABLED" : "DISABLED"));

   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_privacy(network_id_get(), controller_id_get(), data, length);
   }
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_privacy(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_PRIVACY) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   data[0]  =  privacy_ ;

   LOG_INFO("%s: Privacy Mode <%s>\n", __FUNCTION__, (privacy_ & PRIVACY_FLAGS_ENABLED ? "ENABLED" : "DISABLED"));

   return(CTRLM_RF4CE_RIB_ATTR_LEN_PRIVACY);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_controller_capabilities(const guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_CAPABILITIES) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   fmr_supported_              = (data[0] & CONTROLLER_CAPABILITIES_BYTE0_FLAGS_FMR);
   par_voice_supported_        = (data[0] & CONTROLLER_CAPABILITIES_BYTE0_FLAGS_PAR);
   haptics_feedback_supported_ = (data[0] & CONTROLLER_CAPABILITIES_BYTE0_FLAGS_HAPTICS);

   LOG_INFO("%s: Controller Capabilities. FMR <%s> PAR <%s> HAPTICS <%s>\n", __FUNCTION__, (fmr_supported_              ? "SUPPORTED" : "NOT SUPPORTED"), 
                                                                                           (par_voice_supported_        ? "SUPPORTED" : "NOT SUPPORTED"),
                                                                                           (haptics_feedback_supported_ ? "SUPPORTED" : "NOT SUPPORTED"));

   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_controller_capabilities(network_id_get(), controller_id_get(), data, length);
   }
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_controller_capabilities(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_CAPABILITIES) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = memset_s(data, length , 0, length);
   ERR_CHK(safec_rc);
   data[0]  = (fmr_supported_              ? CONTROLLER_CAPABILITIES_BYTE0_FLAGS_FMR:0);
   data[0] += (par_voice_supported_        ? CONTROLLER_CAPABILITIES_BYTE0_FLAGS_PAR:0);
   data[0] += (haptics_feedback_supported_ ? CONTROLLER_CAPABILITIES_BYTE0_FLAGS_HAPTICS:0);

   LOG_INFO("%s: Controller Capabilities. FMR <%s> PAR <%s> HAPTICS <%s>\n", __FUNCTION__, (fmr_supported_              ? "ENABLED" : "DISABLED"),
                                                                                           (par_voice_supported_        ? "ENABLED" : "DISABLED"),
                                                                                           (haptics_feedback_supported_ ? "ENABLED" : "DISABLED"));

   return(CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_CAPABILITIES);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_far_field_configuration(guchar *data, guchar length) {
   return(obj_network_rf4ce_->property_write_far_field_configuration(data, length));
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_far_field_configuration(guchar *data, guchar length) {
   return(obj_network_rf4ce_->property_read_far_field_configuration(data, length));
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_far_field_metrics(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_METRICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = memset_s(&ff_metrics_, sizeof(ff_metrics_) , 0, sizeof(ff_metrics_));
   ERR_CHK(safec_rc);
   
   ff_metrics_.flags                = data[0];
   ff_metrics_.uptime               = data[1] + (data[2] << 8) + (data[3] << 16) + (data[4] << 24);
   ff_metrics_.privacy_time         = data[5] + (data[6] << 8) + (data[7] << 16) + (data[8] << 24);

   LOG_INFO("%s: Flags <%02X>, Uptime <%u>, Privacy Time <%u>\n", __FUNCTION__, ff_metrics_.flags, ff_metrics_.uptime, ff_metrics_.privacy_time);

   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_far_field_metrics(network_id_get(), controller_id_get(), data, length);
      time_metrics_ = time(NULL);
      ctrlm_db_rf4ce_write_time_metrics(network_id_get(), controller_id_get(), time_metrics_);
   }
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_far_field_metrics(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_METRICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = memset_s(data, length , 0, length);
   ERR_CHK(safec_rc);

   // Load the data from the controller object
   data[0]  =  ff_metrics_.flags;
   data[1]  =  ff_metrics_.uptime                     & 0xFF;
   data[2]  = (ff_metrics_.uptime       >> 8)         & 0xFF;
   data[3]  = (ff_metrics_.uptime       >> 16)        & 0xFF;
   data[4]  = (ff_metrics_.uptime       >> 24)        & 0xFF;
   data[5]  =  ff_metrics_.privacy_time               & 0xFF;
   data[6]  = (ff_metrics_.privacy_time >> 8)         & 0xFF;
   data[7]  = (ff_metrics_.privacy_time >> 16)        & 0xFF;
   data[8]  = (ff_metrics_.privacy_time >> 24)        & 0xFF;

   LOG_INFO("%s: Flags <%02X>, Uptime <%u>, Privacy Time <%u>\n", __FUNCTION__, ff_metrics_.flags, ff_metrics_.uptime, ff_metrics_.privacy_time);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_METRICS);
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_dsp_configuration(guchar *data, guchar length) {
   return(obj_network_rf4ce_->property_write_dsp_configuration(data, length));
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_dsp_configuration(guchar *data, guchar length) {
   return(obj_network_rf4ce_->property_read_dsp_configuration(data, length));
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_dsp_metrics(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DSP_METRICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = memset_s(&dsp_metrics_, sizeof(dsp_metrics_) , 0, sizeof(dsp_metrics_));
   ERR_CHK(safec_rc);

   dsp_metrics_.dropped_mic_frames = data[0] + (data[1] << 8);
   dsp_metrics_.dropped_speaker_frames = data[2] + (data[3] << 8);
   dsp_metrics_.keyword_detect_count = data[4] + (data[5] << 8);
   dsp_metrics_.average_snr = data[6];
   dsp_metrics_.average_keyword_confidence = data[7];
   dsp_metrics_.eos_initial_timeout_count = data[8] + (data[9] << 8);
   dsp_metrics_.eos_timeout_count = data[10] + (data[11] << 8);
   dsp_metrics_.mic_failure_count = data[12];
   dsp_metrics_.total_working_mics = data[13];
   dsp_metrics_.speaker_failure_count = data[14];
   dsp_metrics_.total_working_speakers = data[15];

   LOG_INFO("%s: Dropped Mic/Speaker Frames <%u/%u>, Keyword Detect Count <%u>, Average SNR <%.02f>, Average Keyword Confidence <%.04f>, Initial EOS / EOS Timeout <%u/%u>, Total Working Mics/Speakers <%u/%u>, Mic/Speaker Failures <%u/%u>\n", 
            __FUNCTION__, dsp_metrics_.dropped_mic_frames, dsp_metrics_.dropped_speaker_frames, dsp_metrics_.keyword_detect_count, Q_NOTATION_TO_DOUBLE(dsp_metrics_.average_snr, 2),
            Q_NOTATION_TO_DOUBLE(dsp_metrics_.average_keyword_confidence, 7), dsp_metrics_.eos_initial_timeout_count, dsp_metrics_.eos_timeout_count, dsp_metrics_.total_working_mics,
            dsp_metrics_.total_working_speakers, dsp_metrics_.mic_failure_count, dsp_metrics_.speaker_failure_count);


   if(!loading_db_ && validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_rf4ce_write_dsp_metrics(network_id_get(), controller_id_get(), data, length);
      time_metrics_ = time(NULL);
      ctrlm_db_rf4ce_write_time_metrics(network_id_get(), controller_id_get(), time_metrics_);
   }
   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_dsp_metrics(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DSP_METRICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = memset_s(data, length , 0, length);
   ERR_CHK(safec_rc);

   data[0]  =  dsp_metrics_.dropped_mic_frames       & 0xFF;
   data[1]  = (dsp_metrics_.dropped_mic_frames >> 8) & 0xFF;
   data[2]  =  dsp_metrics_.dropped_speaker_frames   & 0xFF;
   data[3]  = (dsp_metrics_.dropped_speaker_frames >> 8) & 0XFF;
   data[4]  =  dsp_metrics_.keyword_detect_count     & 0xFF;
   data[5]  = (dsp_metrics_.keyword_detect_count >> 8) & 0xFF;
   data[6]  =  dsp_metrics_.average_snr;
   data[7]  =  dsp_metrics_.average_keyword_confidence;
   data[8]  =  dsp_metrics_.eos_initial_timeout_count & 0xFF;
   data[9]  = (dsp_metrics_.eos_initial_timeout_count >> 8) & 0xFF;
   data[10] =  dsp_metrics_.eos_timeout_count & 0xFF;
   data[11] = (dsp_metrics_.eos_timeout_count >> 8) & 0xFF;
   data[12] =  dsp_metrics_.mic_failure_count;
   data[13] =  dsp_metrics_.total_working_mics;
   data[14] =  dsp_metrics_.speaker_failure_count;
   data[15] =  dsp_metrics_.total_working_speakers;

   LOG_INFO("%s: Dropped Mic/Speaker Frames <%u/%u>, Keyword Detect Count <%u>, Average SNR <%.02f>, Average Keyword Confidence <%.04f>, Initial EOS / EOS Timeout <%u/%u>, Total Working Mics/Speakers <%u/%u>, Mic/Speaker Failures <%u/%u>\n", 
            __FUNCTION__, dsp_metrics_.dropped_mic_frames, dsp_metrics_.dropped_speaker_frames, dsp_metrics_.keyword_detect_count, Q_NOTATION_TO_DOUBLE(dsp_metrics_.average_snr, 2),
            Q_NOTATION_TO_DOUBLE(dsp_metrics_.average_keyword_confidence, 7), dsp_metrics_.eos_initial_timeout_count, dsp_metrics_.eos_timeout_count, dsp_metrics_.total_working_mics,
            dsp_metrics_.total_working_speakers, dsp_metrics_.mic_failure_count, dsp_metrics_.speaker_failure_count);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_DSP_METRICS);
}

void ctrlm_obj_controller_rf4ce_t::update_voice_metrics(ctrlm_rf4ce_voice_utterance_type_t voice_utterance_type, guint32 voice_packets_sent, guint32 voice_packets_lost) {
   voice_session_prefs_t *vrex_prefs = ctrlm_voice_vrex_prefs_get();

   handle_day_change();
   voice_packets_sent_today_       += voice_packets_sent;
   voice_packets_lost_today_       += voice_packets_lost;
   if((((float)voice_packets_lost/(float)voice_packets_sent)*100.0) > (float)(vrex_prefs->packet_loss_threshold)) {
      utterances_exceeding_packet_loss_threshold_today_++;
   }

   if(voice_utterance_type==RF4CE_VOICE_NORMAL_UTTERANCE) {
      voice_cmd_count_today_++;
      LOG_INFO("%s: Voice Cmd Count Today <%u>  Voice Packets Sent Today <%u>  Voice Packets Lost Today <%u>  Utterances Exceeding Packet Loss Threshold Today <%u>\n", __FUNCTION__, voice_cmd_count_today_, voice_packets_sent_today_, voice_packets_lost_today_, utterances_exceeding_packet_loss_threshold_today_);
   } else {
      voice_cmd_short_today_++;
      LOG_INFO("%s: Voice Cmd Short Today <%u>  Voice Packets Sent Today <%u>  Voice Packets Lost Today <%u>  Utterances Exceeding Packet Loss Threshold Today <%u>\n", __FUNCTION__, voice_cmd_short_today_, voice_packets_sent_today_, voice_packets_lost_today_, utterances_exceeding_packet_loss_threshold_today_);
   }

   property_write_voice_metrics();
}

guchar ctrlm_obj_controller_rf4ce_t::property_write_uptime_privacy_info(guchar *data, guchar length) {
   if(length != sizeof(uptime_privacy_info_t)) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = memset_s(&uptime_privacy_info_, sizeof(uptime_privacy_info_t) , 0, sizeof(uptime_privacy_info_t));
   ERR_CHK(safec_rc);

   uptime_privacy_info_.time_uptime_start     = (data[3] << 24) + (data[2] << 16) + (data[1] << 8) + data[0];
   uptime_privacy_info_.uptime_seconds        = (data[7] << 24) + (data[6] << 16) + (data[5] << 8) + data[4];
   uptime_privacy_info_.privacy_time_seconds  = (data[11] << 24) + (data[10] << 16) + (data[9] << 8) + data[8];

   LOG_INFO("%s: Uptime Start Time <%ld>, Uptime in seconds <%lu>, Privacy Time in seconds <%lu>\n", 
         __FUNCTION__, uptime_privacy_info_.time_uptime_start, uptime_privacy_info_.uptime_seconds, uptime_privacy_info_.privacy_time_seconds);

   return(length);
}

guchar ctrlm_obj_controller_rf4ce_t::property_read_uptime_privacy_info(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_UPTIME_PRIVACY_INFO) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = memset_s(data, length , 0, length);
   ERR_CHK(safec_rc);

   data[0]   =  uptime_privacy_info_.time_uptime_start           & 0xFF;
   data[1]   = (uptime_privacy_info_.time_uptime_start >> 8)     & 0xFF;
   data[2]   = (uptime_privacy_info_.time_uptime_start >> 16)    & 0xFF;
   data[3]   = (uptime_privacy_info_.time_uptime_start >> 24)    & 0xFF;
   data[4]   =  uptime_privacy_info_.uptime_seconds              & 0xFF;
   data[5]   = (uptime_privacy_info_.uptime_seconds >> 8)        & 0xFF;
   data[6]   = (uptime_privacy_info_.uptime_seconds >> 16)       & 0xFF;
   data[7]   = (uptime_privacy_info_.uptime_seconds >> 24)       & 0xFF;
   data[8]   =  uptime_privacy_info_.privacy_time_seconds        & 0xFF;
   data[9]   = (uptime_privacy_info_.privacy_time_seconds >> 8)  & 0xFF;
   data[10]  = (uptime_privacy_info_.privacy_time_seconds >> 16) & 0xFF;
   data[11]  = (uptime_privacy_info_.privacy_time_seconds >> 24) & 0xFF;

   LOG_INFO("%s: Uptime Start Time <%ld>, Uptime in seconds <%lu>, Privacy Time in seconds <%lu>\n", 
         __FUNCTION__, uptime_privacy_info_.time_uptime_start, uptime_privacy_info_.uptime_seconds, uptime_privacy_info_.privacy_time_seconds);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_UPTIME_PRIVACY_INFO);
}

void ctrlm_obj_controller_rf4ce_t::set_firmware_updated() {
   if(firmware_updated_ != RF4CE_FIRMWARE_UPDATED_YES) {
      firmware_updated_ = RF4CE_FIRMWARE_UPDATED_YES;
      LOG_INFO("%s: Firmware Updated <YES>\n", __FUNCTION__);
      property_write_firmware_updated();
   }
}

gboolean ctrlm_obj_controller_rf4ce_t::is_firmeware_updated(){
   if(firmware_updated_==RF4CE_FIRMWARE_UPDATED_YES){
      return true;
   }
   return false;
}


void ctrlm_obj_controller_rf4ce_t::time_last_checkin_for_device_update_set() {
   time_last_checkin_for_device_update_ = time(NULL);
   property_write_time_last_checkin_for_device_update();
}

void ctrlm_obj_controller_rf4ce_t::time_last_checkin_for_device_update_get(time_t *time) {
   if(NULL == time) {
      LOG_ERROR("%s: Parameter time is NULL\n", __FUNCTION__);
      return;
   }

   *time = time_last_checkin_for_device_update_;
}

bool ctrlm_obj_controller_rf4ce_t::handle_day_change() {
   time_t time_in_seconds = time(NULL);
   time_t shutdown_time   = ctrlm_shutdown_time_get();
   if(time_in_seconds < shutdown_time) {
      LOG_WARN("%s: Current Time <%ld> is less than the last shutdown time <%ld>.  Wait until time updates.\n", __FUNCTION__, time_in_seconds, shutdown_time);
      return(false);
   }
   guint32 today = time_in_seconds / (60 * 60 * 24);
   guint32 day_change = today - today_;

   //If this is a different day...
   if(day_change > 0) {

      //If this is the next day...
      if(day_change == 1) {
         voice_cmd_count_yesterday_                            = voice_cmd_count_today_;
         voice_cmd_short_yesterday_                            = voice_cmd_short_today_;
         voice_packets_sent_yesterday_                         = voice_packets_sent_today_;
         voice_packets_lost_yesterday_                         = voice_packets_lost_today_;
         utterances_exceeding_packet_loss_threshold_yesterday_ = utterances_exceeding_packet_loss_threshold_today_;
      } else {
         voice_cmd_count_yesterday_                            = 0;
         voice_cmd_short_yesterday_                            = 0;
         voice_packets_sent_yesterday_                         = 0;
         voice_packets_lost_yesterday_                         = 0;
         utterances_exceeding_packet_loss_threshold_yesterday_ = 0;
      }

      voice_cmd_count_today_                                   = 0;
      voice_cmd_short_today_                                   = 0;
      voice_packets_sent_today_                                = 0;
      voice_packets_lost_today_                                = 0;
      utterances_exceeding_packet_loss_threshold_today_        = 0;
      today_                                                   = today;

      return(true);
   }

   return(false);
}

void ctrlm_obj_controller_rf4ce_t::get_last_battery_event(ctrlm_rcu_battery_event_t &battery_event, unsigned long &battery_event_timestamp) {
   if (battery_milestones_.battery_0_percent_timestamp != 0) {
      battery_event           = CTRLM_RCU_BATTERY_EVENT_0_PERCENT;
      battery_event_timestamp = battery_milestones_.battery_0_percent_timestamp;
   } else if (battery_milestones_.battery_5_percent_timestamp != 0) {
      battery_event           = CTRLM_RCU_BATTERY_EVENT_PENDING_DOOM;
      battery_event_timestamp = battery_milestones_.battery_5_percent_timestamp;
   } else if (battery_milestones_.battery_25_percent_timestamp != 0) {
      battery_event           = CTRLM_RCU_BATTERY_EVENT_25_PERCENT;
      battery_event_timestamp = battery_milestones_.battery_25_percent_timestamp;
   } else if (battery_milestones_.battery_50_percent_timestamp != 0) {
      battery_event           = CTRLM_RCU_BATTERY_EVENT_50_PERCENT;
      battery_event_timestamp = battery_milestones_.battery_50_percent_timestamp;
   } else if (battery_milestones_.battery_75_percent_timestamp != 0) {
      battery_event           = CTRLM_RCU_BATTERY_EVENT_75_PERCENT;
      battery_event_timestamp = battery_milestones_.battery_75_percent_timestamp;
   } else if (battery_milestones_.battery_changed_timestamp != 0) {
      battery_event           = CTRLM_RCU_BATTERY_EVENT_REPLACED;
      battery_event_timestamp = battery_milestones_.battery_changed_timestamp;
   } else {
      battery_event           = CTRLM_RCU_BATTERY_EVENT_NONE;
      battery_event_timestamp = battery_last_good_timestamp_;
   }
}

bool ctrlm_obj_controller_rf4ce_t::import_check_validation() {
   bool ret                = TRUE;
   version_software_t sw_version;

   sw_version.major        = 0x00;
   sw_version.minor        = 0x00;
   sw_version.revision     = 0x00;
   sw_version.patch        = 0x00;

   if(!(version_compare(sw_version, version_software_) || version_compare(sw_version, version_irdb_) || version_compare(sw_version, version_bootloader_))) {
      ret = FALSE;
   }

   sw_version.major        = 0xFF;
   sw_version.minor        = 0xFF;
   sw_version.revision     = 0xFF;
   sw_version.patch        = 0xFF;

   if(!(version_compare(sw_version, version_software_) || version_compare(sw_version, version_irdb_) || version_compare(sw_version, version_bootloader_))) {
      ret = FALSE;
   }

   return ret;
}

version_software_t ctrlm_obj_controller_rf4ce_t::version_software_get(){
   return version_software_;
}

version_software_t ctrlm_obj_controller_rf4ce_t::version_dsp_get(){
   return version_dsp_;
}

version_software_t ctrlm_obj_controller_rf4ce_t::version_keyword_model_get(){
   return version_keyword_model_;
}

version_software_t ctrlm_obj_controller_rf4ce_t::version_arm_get(){
   return version_arm_;
}

version_software_t ctrlm_obj_controller_rf4ce_t::version_audio_data_get(){
   return version_audio_data_;
}

version_hardware_t ctrlm_obj_controller_rf4ce_t::version_hardware_get(){
   return version_hardware_;
}

controller_irdb_status_t ctrlm_obj_controller_rf4ce_t::controller_irdb_status_get() {
   return controller_irdb_status_;
}

time_t ctrlm_obj_controller_rf4ce_t::time_binding_get() {
   return time_binding_;
}

gboolean ctrlm_obj_controller_rf4ce_t::has_battery() {
   return has_battery_;
}

gboolean ctrlm_obj_controller_rf4ce_t::has_dsp() {
   return has_dsp_;
}

void ctrlm_obj_controller_rf4ce_t::log_binding_for_telemetry() {
   char time_binding_str[20];
   ctrlm_controller_id_t controller_id = controller_id_get();

   if(time_binding_ == 0) {
      errno_t safec_rc = strcpy_s(time_binding_str, sizeof(time_binding_str), "NEVER");
      ERR_CHK(safec_rc);
   } else {
      time_binding_str[0] = '\0';
      strftime(time_binding_str, 20, "%F %T", localtime((time_t *)&time_binding_));
   }
   LOG_INFO("%s: Model <%s>, Binding <%s>, Remote Bound (%u,%u), Time <%s>\n", __FUNCTION__, product_name_, ctrlm_rcu_binding_type_str(binding_type_), network_id_get(), controller_id_get(), time_binding_str);
   ctrlm_update_last_key_info(controller_id, IARM_BUS_IRMGR_KEYSRC_RF, 0, product_name_, false, true);
}

void ctrlm_obj_controller_rf4ce_t::log_unbinding_for_telemetry() {
   char   time_unbinding_str[20];
   time_t time_unbinding = time(NULL);

   time_unbinding_str[0] = '\0';
   strftime(time_unbinding_str, 20, "%F %T", localtime((time_t *)&time_unbinding));
   LOG_INFO("%s: Model <%s>, Remote Unbound (%u,%u), Time <%s>\n", __FUNCTION__, product_name_, network_id_get(), controller_id_get(), time_unbinding_str);
}

void ctrlm_obj_controller_rf4ce_t::print_remote_firmware_debug_info(ctrlm_rf4ce_controller_firmware_log_t log_type, string message){
   char remote_info[100] = {'\0'};

   errno_t safec_rc = sprintf_s(remote_info, sizeof(remote_info), "%s ID - %u. Current Firmware: <%u.%u.%u.%u>. ", ctrlm_rf4ce_controller_type_str(controller_type_), controller_id_get(), version_software_.major, version_software_.minor, version_software_.revision, version_software_.patch);
   if(safec_rc < EOK) {
      ERR_CHK(safec_rc);
   }

   switch(log_type){
      case RF4CE_PRINT_FIRMWARE_LOG_BUTTON_PRESS:
         if(print_firmware_on_button_press){
            LOG_INFO("%s: %s First key press.\n", __FUNCTION__, remote_info);
            print_firmware_on_button_press = false;
         }
         break;
      case RF4CE_PRINT_FIRMWARE_LOG_REBOOT:
         LOG_INFO("%s: %s Remote reboot. %s\n", __FUNCTION__, remote_info, ctrlm_rf4ce_reboot_reason_str(reboot_reason_));
         break;
      case RF4CE_PRINT_FIRMWARE_LOG_UPGRADE_CHECK:
         LOG_INFO("%s: %s Check for FW upgrade\n", __FUNCTION__, remote_info);
         break;
      case RF4CE_PRINT_FIRMWARE_LOG_IMAGE_DOWNLOAD_STARTED:
         LOG_INFO("%s: %s Image download Started. Downloading: %s\n", __FUNCTION__, remote_info, message.c_str());
         break;
      case RF4CE_PRINT_FIRMWARE_LOG_IMAGE_DOWNLOAD_PENDING:
         LOG_INFO("%s: %s Image download Pending: %s\n", __FUNCTION__, remote_info, message.c_str());
         break;
      case RF4CE_PRINT_FIRMWARE_LOG_IMAGE_DOWNLOAD_COMPLETE:
         LOG_INFO("%s: %s Image download complete. Downloaded: %s\n", __FUNCTION__, remote_info, message.c_str());
         break;
      case RF4CE_PRINT_FIRMWARE_LOG_REBOOT_SCHEDULE:
         LOG_INFO("%s: %s Remote reboot scheduled. Downloaded: %s\n", __FUNCTION__, remote_info, message.c_str());
         break;
      case RF4CE_PRINT_FIRMWARE_LOG_PAIRED_REMOTE:
         LOG_INFO("%s: %s Remote Paired. Battery level: %u\n", __FUNCTION__, remote_info, battery_level_percent(battery_status_.voltage_loaded));
         break;
      default:
         LOG_INFO("%s: INVALID MESSAGE TYPE\n", __FUNCTION__);
         break;
   }
}


// These functions are HACKS for XR15-704
#ifdef XR15_704
void ctrlm_obj_controller_rf4ce_t::set_reset() {
   version_software_t version_bug;
   version_bug.major    = XR15_DEVICE_UPDATE_BUG_FIRMWARE_MAJOR;
   version_bug.minor    = XR15_DEVICE_UPDATE_BUG_FIRMWARE_MINOR;
   version_bug.revision = XR15_DEVICE_UPDATE_BUG_FIRMWARE_REVISION;
   version_bug.patch    = XR15_DEVICE_UPDATE_BUG_FIRMWARE_PATCH;

   if(RF4CE_CONTROLLER_TYPE_XR15 == controller_type_ && -1 == version_compare(version_software_, version_bug)) {
      needs_reset_ = true;
   } else {
      needs_reset_ = false;
   }
}
#endif
// These functions are HACKS for XR15-704

// Polling Functions
void ctrlm_obj_controller_rf4ce_t::polling_action_push(ctrlm_rf4ce_polling_action_msg_t *action) {
   if(action->action != RF4CE_POLLING_ACTION_NONE) {
      LOG_INFO("%s: Adding action %s to the polling action queue for controller %u\n", __FUNCTION__, ctrlm_rf4ce_polling_action_str(action->action), controller_id_get());
      g_async_queue_push_sorted(polling_actions_, (gpointer)action, ctrlm_rf4ce_polling_action_sort_function, NULL);
   }
}

bool ctrlm_obj_controller_rf4ce_t::polling_action_pop(ctrlm_rf4ce_polling_action_msg_t **action) {
   if(action == NULL) {
      LOG_ERROR("%s: Action Pointer NULL\n", __FUNCTION__);
      return(false);
   }

   *action = (ctrlm_rf4ce_polling_action_msg_t *)((guint)g_async_queue_try_pop(polling_actions_)); // Try pop returns 0 which is action NONE
   return(g_async_queue_length(polling_actions_) ? true : false);
}

void ctrlm_obj_controller_rf4ce_t::update_polling_configurations(bool add_polling_action) {
   // Check Network for Polling configuration and if changed, add to polling actions
   if(obj_network_rf4ce_->polling_configuration_by_controller_type(controller_type_get(), &polling_methods_, polling_configurations_)) {
      // Store the new values in DB
      guchar data[CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE];
      ctrlm_network_id_t    network_id    = network_id_get();
      ctrlm_controller_id_t controller_id = controller_id_get();

      if(CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS == property_read_polling_methods(data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_METHODS)) {
         ctrlm_db_rf4ce_write_polling_methods(network_id, controller_id, (guint8)data[0]);
      }
   
      if(CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION == property_read_polling_configuration_heartbeat(data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION)) {
         ctrlm_db_rf4ce_write_polling_configuration_heartbeat(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
      }
   
      if(CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION == property_read_polling_configuration_mac(data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION)) {
         ctrlm_db_rf4ce_write_polling_configuration_mac(network_id, controller_id, data, CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
      }      

      // Add polling action
      if(add_polling_action) {
          ctrlm_rf4ce_polling_action_push(network_id_get(), controller_id_get(), RF4CE_POLLING_ACTION_POLL_CONFIGURATION, NULL, 0);
      }
   }
}

void ctrlm_obj_controller_rf4ce_t::rf4ce_heartbeat(ctrlm_timestamp_t timestamp, guint16 trigger) {
   LOG_DEBUG("%s: Controller %u Heartbeat: Trigger %s\n", __FUNCTION__, controller_id_get(), ctrlm_rf4ce_controller_polling_trigger_str(trigger));
   guint8 flags  = 0x00;
   ctrlm_rf4ce_polling_action_t      action     = RF4CE_POLLING_ACTION_NONE;
   ctrlm_rf4ce_polling_action_msg_t *action_msg = NULL;
   guint8 response[3 + POLLING_RESPONSE_DATA_LEN] = {0};
   guint8 response_len = sizeof(response);
   errno_t safec_rc = -1;

#ifdef ASB
   bool link_key_validation = false;
#endif

   if(CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS == validation_result_) {
      // Set last heartbeat time
      time_last_heartbeat_update();
      switch(trigger) {
         case POLLING_TRIGGER_FLAG_VOICE_SESSION: {
#ifdef USE_VOICE_SDK
            if(false == ctrlm_get_voice_obj()->voice_device_streaming(network_id_get(), controller_id_get())) {
               action = RF4CE_POLLING_ACTION_EOS;
            }
#endif
            break;
         }
         default: {
            // Get the action from the queue
            if(polling_action_pop(&action_msg)) {
               flags |= HEARTBEAT_RESPONSE_FLAG_POLL_AGAIN;
            }
            if(action_msg) {
               action = action_msg->action;
            }
            break;
         }
      }

      // Log Response
      if(action == RF4CE_POLLING_ACTION_NONE) {
         LOG_DEBUG("%s: Controller %u Heartbeat Response: Action <%s> Poll Again <%s>\n", __FUNCTION__, controller_id_get(), ctrlm_rf4ce_polling_action_str(action), (flags & HEARTBEAT_RESPONSE_FLAG_POLL_AGAIN ? "YES" : "NO"));   
      } else {
         LOG_INFO("%s: Controller %u Heartbeat Response: Action <%s> Poll Again <%s>\n", __FUNCTION__, controller_id_get(), ctrlm_rf4ce_polling_action_str(action), (flags & HEARTBEAT_RESPONSE_FLAG_POLL_AGAIN ? "YES" : "NO"));
      }
   } 
#ifdef ASB
   else if(POLLING_TRIGGER_FLAG_STATUS == trigger && CTRLM_RF4CE_RESULT_VALIDATION_PENDING == validation_result_) {
      LOG_INFO("%s: Controller %u Heartbeat for Link Key Validation, respond with NO ACTION\n", __FUNCTION__, controller_id_get());
      // Cancel timeout
      ctrlm_timeout_destroy(&asb_tag_);
      asb_tag_ = 0;
      link_key_validation = true;
      response_len = 3; // Backwards compatiability for XR15v2
   }
#endif
   else {
      LOG_INFO("%s: Heartbeat from controller that is not bound.. Ignore...\n", __FUNCTION__);
      return;
   }

   // Ready the response
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
   // Add Data to response
   response[0] = RF4CE_FRAME_CONTROL_HEARTBEAT_RESPONSE;
   response[1] = flags;
   response[2] = (uint8_t)action;
   if(action_msg) {
      safec_rc = memcpy_s(&response[3], sizeof(response)-3, action_msg->data, POLLING_RESPONSE_DATA_LEN);
      ERR_CHK(safec_rc);
   }

   // Send the response back to the controller
   req_data(CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU, timestamp, response_len, response, NULL, NULL);
#ifdef ASB
   if(link_key_validation) {
      obj_network_rf4ce_->process_pair_result(controller_id_get(), ieee_address_, CTRLM_HAL_RESULT_PAIR_SUCCESS);
   }
#endif
   if(NULL != action_msg) {
      free(action_msg);
      action_msg = NULL;
   }
}
void ctrlm_obj_controller_rf4ce_t::rib_configuration_complete(ctrlm_timestamp_t timestamp, ctrlm_rf4ce_rib_configuration_complete_status_t status) {
   LOG_INFO("%s: Controller %u Configuration Complete: Status <%u>\n", __FUNCTION__, controller_id_get(), status);
   switch(status) {
      case RF4CE_RIB_CONFIGURATION_COMPLETE_PAIRING_SUCCESS: {
         LOG_INFO("%s: Configuration Complete Pairing Success\n", __FUNCTION__);
         ctrlm_inform_configuration_complete(network_id_get(), controller_id_get(), CTRLM_RCU_CONFIGURATION_RESULT_SUCCESS);
         break;
      }
      case RF4CE_RIB_CONFIGURATION_COMPLETE_PAIRING_INCOMPLETE: {
         LOG_INFO("%s: Configuration Complete Pairing Incomplete\n", __FUNCTION__);
         ctrlm_inform_configuration_complete(network_id_get(), controller_id_get(), CTRLM_RCU_CONFIGURATION_RESULT_SUCCESS);
         if(configuration_complete_failure_ == false) {
            ctrlm_rf4ce_polling_action_push(network_id_get(), controller_id_get(), RF4CE_POLLING_ACTION_CONFIGURATION, NULL, 0);
            configuration_complete_failure_ = true;
         }
         break;
      }
      case RF4CE_RIB_CONFIGURATION_COMPLETE_REBOOT_SUCCESS: {
         LOG_INFO("%s: Configuration Complete Reboot Success\n", __FUNCTION__);
         break;
      }
      case RF4CE_RIB_CONFIGURATION_COMPLETE_REBOOT_INCOMPLETE: {
         LOG_INFO("%s: Configuration Complete Reboot Incomplete\n", __FUNCTION__);
         if(configuration_complete_failure_ == false) {
            ctrlm_rf4ce_polling_action_push(network_id_get(), controller_id_get(), RF4CE_POLLING_ACTION_CONFIGURATION, NULL, 0);
            configuration_complete_failure_ = true;
         }
         break;
      }
      default: {
         LOG_ERROR("%s: Unexpected value for status\n", __FUNCTION__);
         break;
      }
   }
   if(rib_configuration_complete_status_ != status) {
      rib_configuration_complete_status_ = status;
      ctrlm_db_rf4ce_write_rib_configuration_complete(network_id_get(), controller_id_get(), rib_configuration_complete_status_);
   }
}

void ctrlm_obj_controller_rf4ce_t::time_last_heartbeat_get(time_t *time) {
   if(time == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return;
   }
   *time = time_last_heartbeat_;
}
// End Polling Functions

void ctrlm_obj_controller_rf4ce_t::binding_security_type_set(ctrlm_rcu_binding_security_type_t type) {
   LOG_INFO("%s: Binding Security Type set to %d\n", __FUNCTION__, type);
   binding_security_type_ = type;
   // TODO need to flush the timestamp to the DB periodically
   if(validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) {
      ctrlm_db_rf4ce_write_binding_security_type(network_id_get(), controller_id_get(), binding_security_type_);
   }
}

ctrlm_rcu_binding_security_type_t ctrlm_obj_controller_rf4ce_t::binding_security_type_get() {
   return(binding_security_type_);
}

gboolean ctrlm_obj_controller_rf4ce_t::version_hardware_valid(guchar *data) {
   guchar   index = 0;
   guchar  *hardware_version      = NULL;
   guchar   num_hardware_versions = 0;
   gboolean is_valid              = false;

   //Check for mac address of an XR11V2
   if((ieee_address_ & 0xFFFFFF0000000000)         == XR11V2_IEEE_PREFIX) {
      hardware_version      = (guchar *)xr11v2_hardware_versions;
      num_hardware_versions = NUM_XR11V2_HARDWARE_VERSIONS;
   } else if((((ieee_address_ & 0xFFFFFF0000000000) == XR15V2_IEEE_PREFIX_UP_TO_FEB_2018) ||
             ((ieee_address_ & 0xFFFFFF0000000000) == XR15V2_IEEE_PREFIX) || 
             ((ieee_address_ & 0xFFFFFF0000000000) == XR15V2_IEEE_PREFIX_THIRD)) && 
             (0 != strncmp(product_name_, "XR15-10", CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME))) {
      hardware_version      = (guchar *)xr15v2_hardware_versions;
      num_hardware_versions = NUM_XR15V2_HARDWARE_VERSIONS;  
   //Check for mac address of an XR15V1
   } else if((ieee_address_ & 0xFFFFFF0000000000)  == XR15V1_IEEE_PREFIX) {
      hardware_version      = (guchar *)xr15v1_hardware_versions;
      num_hardware_versions = NUM_XR15V1_HARDWARE_VERSIONS;
   } else {
      LOG_INFO("%s: Invalid ieee_address <0x%016llX>\n", __FUNCTION__, ieee_address_);
      return(false);
   }

   //If valid mac address, look for valid hardware version
   for (index=0; index<num_hardware_versions; index++) {
      if((((guchar *)(hardware_version+(index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[0] == data[0]) && (((guchar *)(hardware_version+(index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[1] == data[1]) && (((guchar *)(hardware_version+(index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[2] == data[2]) && (((guchar *)(hardware_version+(index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[3] == data[3])) {
         is_valid = true;
         break;
      }
   }

   if(!is_valid) {
      LOG_INFO("%s: Invalid hardware version: ieee_address 0x%016llX, controller_id <%u>, controller_type <%s>, data[0] <%02x>, data[1] <%02x>, data[2] <%02x>, data[3] <%02x>\n", __FUNCTION__, ieee_address_, controller_id_get(), product_name_, data[0], data[1], data[2], data[3]);
   }
   return(is_valid);
}

void ctrlm_obj_controller_rf4ce_t::controller_fix_hardware_version_by_mac_address(guchar *data) {
   guchar *hardware_version   = NULL;
   guchar  hardware_fix_index = 0;

   //Check for mac address of an XR11V2
   if((ieee_address_ & 0xFFFFFF0000000000) == XR11V2_IEEE_PREFIX) {
      hardware_version   = (guchar *)xr11v2_hardware_versions;
      hardware_fix_index = NUM_XR11V2_HARDWARE_FIX_INDEX;
   //Check for mac address of an XR15V1
   } else if((ieee_address_ & 0xFFFFFF0000000000) == XR15V1_IEEE_PREFIX) {
      hardware_version   = (guchar *)xr15v1_hardware_versions;
      hardware_fix_index = NUM_XR15V1_HARDWARE_FIX_INDEX;
   //Check for mac address of an XR15V2
   } else if(((ieee_address_ & 0xFFFFFF0000000000) == XR15V2_IEEE_PREFIX_UP_TO_FEB_2018) ||
             ((ieee_address_ & 0xFFFFFF0000000000) == XR15V2_IEEE_PREFIX) ||
             ((ieee_address_ & 0xFFFFFF0000000000) == XR15V2_IEEE_PREFIX_THIRD)) {
      hardware_version   = (guchar *)xr15v2_hardware_versions;
      hardware_fix_index = NUM_XR15V2_HARDWARE_FIX_INDEX;
   } else {
      LOG_INFO("%s: Invalid ieee_address 0x%016llX\n", __FUNCTION__, ieee_address_);
      return;
   }

   data[0] = ((guchar *)(hardware_version+(hardware_fix_index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[0];
   data[1] = ((guchar *)(hardware_version+(hardware_fix_index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[1];
   data[2] = ((guchar *)(hardware_version+(hardware_fix_index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[2];
   data[3] = ((guchar *)(hardware_version+(hardware_fix_index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[3];

   LOG_INFO("%s: ieee_address 0x%016llX, controller_id <%u>, controller_type <%s>, hardware_version[0] <%02x>, hardware_version[1] <%02x>, hardware_version[2] <%02x>, hardware_version[3] <%02x>\n", __FUNCTION__, ieee_address_, controller_id_get(), product_name_, ((guchar *)(hardware_version+(hardware_fix_index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[0], ((guchar *)(hardware_version+(hardware_fix_index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[1], ((guchar *)(hardware_version+(hardware_fix_index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[2], ((guchar *)(hardware_version+(hardware_fix_index*CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING)))[3]);
}

#ifdef ASB
typedef struct {
   ctrlm_network_id_t    network_id;
   ctrlm_controller_id_t controller_id;
} asb_link_key_validation_timout_t;

static void ctrlm_asb_link_key_validation_timeout_destroy(gpointer user_data) {
   if(NULL != user_data) {
      LOG_INFO("%s: Free timeout data\n", __FUNCTION__);
      free(user_data);
   }
}

static gboolean ctrlm_asb_link_key_validation_timeout(gpointer user_data) {
   LOG_INFO("%s: Timeout\n", __FUNCTION__);
   asb_link_key_validation_timout_t *data = (asb_link_key_validation_timout_t *)user_data;
   // Send message to MAIN QUEUE
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::asb_link_key_validation_timeout, &data->controller_id, sizeof(data->controller_id), NULL, data->network_id);
   return(FALSE);
}

void ctrlm_obj_controller_rf4ce_t::asb_key_derivation_method_set(asb_key_derivation_method_t method) {
   LOG_INFO("%s: ASB Key Derivation method set to %d\n", __FUNCTION__, method);
   asb_key_derivation_method_used_ = method;
   if(validation_result_ == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) {
      ctrlm_db_rf4ce_write_asb_key_derivation_method(network_id_get(), controller_id_get(), asb_key_derivation_method_used_);
   }
}

asb_key_derivation_method_t ctrlm_obj_controller_rf4ce_t::asb_key_derivation_method_get() {
   return(asb_key_derivation_method_used_);
}

void ctrlm_obj_controller_rf4ce_t::asb_key_derivation_start(ctrlm_network_id_t network_id) {
   asb_link_key_validation_timout_t *timeout = (asb_link_key_validation_timout_t *)g_malloc(sizeof(asb_link_key_validation_timout_t));
   ctrlm_controller_id_t             controller_id = controller_id_get();
   // Set up timeout object
   if(NULL == timeout) {
      LOG_ERROR("%s: Couldn't allocate memory\n", __FUNCTION__);
      g_assert(0);
      return;
   }
   timeout->network_id = network_id;
   timeout->controller_id = controller_id;
   // Get timestamp of start of blackout period
   ctrlm_timestamp_get(&asb_key_derivation_ts_start_);
   // Start timeout timer
   asb_tag_ = g_timeout_add_full(G_PRIORITY_DEFAULT, CTRLM_RF4CE_CONST_ASB_BLACKOUT_TIME + CTRLM_RF4CE_CONST_RESPONSE_WAIT_TIME, ctrlm_asb_link_key_validation_timeout, (gpointer)timeout, ctrlm_asb_link_key_validation_timeout_destroy);
   // Send message to MAIN QUEUE
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::asb_link_key_derivation_perform, (void *)&controller_id, sizeof(controller_id), obj_network_rf4ce_);
}

void ctrlm_obj_controller_rf4ce_t::asb_key_derivation_perform() {
   ctrlm_timestamp_t asb_key_derivation_ts_end;
   unsigned char new_aes128_key[CTRLM_HAL_NETWORK_AES128_KEY_SIZE] = {0};
   ctrlm_hal_network_property_encryption_key_t property = {0};
   // Get Link key
   property.controller_id = controller_id_get();
   if(CTRLM_HAL_RESULT_SUCCESS != network_property_get(CTRLM_HAL_NETWORK_PROPERTY_ENCRYPTION_KEY, (void **)&property)) {
      LOG_ERROR("%s: Failed to get Link Key from HAL\n", __FUNCTION__);
      return;
   }
   // Perform link key derivation

   if(asb_key_derivation(property.aes128_key, new_aes128_key, asb_key_derivation_method_used_)) {
      LOG_ERROR("%s: Failed to perform key derivation\n", __FUNCTION__);
      asb_destroy();
      return;
   }
   // Set New Link key
   errno_t safec_rc = memcpy_s(property.aes128_key, sizeof(property.aes128_key), new_aes128_key, sizeof(property.aes128_key));
   ERR_CHK(safec_rc);
   if(CTRLM_HAL_RESULT_SUCCESS != network_property_set(CTRLM_HAL_NETWORK_PROPERTY_ENCRYPTION_KEY, &property)) {
      LOG_ERROR("%s: Failed to set new link key\n", __FUNCTION__);
      return;
   }

   // Print stats
   ctrlm_timestamp_get(&asb_key_derivation_ts_end);
   LOG_INFO("%s: Key Derivation took %lldms\n", __FUNCTION__, ctrlm_timestamp_subtract_ms(asb_key_derivation_ts_start_, asb_key_derivation_ts_end));
   if(ctrlm_timestamp_subtract_ms(asb_key_derivation_ts_start_, asb_key_derivation_ts_end) > CTRLM_RF4CE_CONST_ASB_BLACKOUT_TIME) {
      LOG_ERROR("%s: Key Derivation took longer than blackout time!\n", __FUNCTION__);
   }

   // Destroy ASB
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::rf4ce_asb_destroy, (void *)NULL, 0, obj_network_rf4ce_);
}
#endif

int handle_controller_metrics_timeout(void *data) {
   ctrlm_obj_controller_rf4ce_t *controller = (ctrlm_obj_controller_rf4ce_t *)data;
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_CONTROLLER, (ctrlm_msg_handler_controller_t)&ctrlm_obj_controller_rf4ce_t::handle_controller_metrics, (void *)NULL, 0, (void *)controller);
   return 0;
}

void ctrlm_obj_controller_rf4ce_t::handle_controller_metrics(void *data, int size) {
   time_t current    = time(NULL);
   time_t since_last = current - time_metrics_;
   if(since_last >= 60*60*23) { // It's been 23 hours since last reported metrics.. Have to account for early timer.
      if(controller_type_ == RF4CE_CONTROLLER_TYPE_XR19) {
         ctrlm_rf4ce_polling_action_push(network_id_get(), controller_id_get(), RF4CE_POLLING_ACTION_METRICS, NULL, 0);
      }
      ctrlm_timeout_create((60*60*24)*1000, handle_controller_metrics_timeout, this); // 24 hours in milliseconds
   } else {
      if(controller_type_ == RF4CE_CONTROLLER_TYPE_XR19) {
         LOG_DEBUG("%s: metrics timeout created\n", __FUNCTION__);
         ctrlm_timeout_create(((60*60*24)-since_last)*1000, handle_controller_metrics_timeout, this); // 24 hours - time since in milliseconds
      }
   }
}

void ctrlm_obj_controller_rf4ce_t::handle_voice_configuration() {
   ctrlm_rf4ce_polling_action_push(network_id_get(), controller_id_get(), RF4CE_POLLING_ACTION_VOICE_CONFIGURATION, NULL, 0);
}

void ctrlm_obj_controller_rf4ce_t::handle_controller_battery_status() {
   ctrlm_rf4ce_polling_action_push(network_id_get(), controller_id_get(), RF4CE_POLLING_ACTION_BATTERY_STATUS, NULL, 0);
}

void ctrlm_obj_controller_rf4ce_t::push_ir_codes(void) {
   // ROBIN -- Check Target IRDB status
   guchar flags = this->obj_network_rf4ce_->target_irdb_status_flags_get();
   if(flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_TV) {
      ir_rf_database_status_ |= IR_RF_DATABASE_STATUS_DOWNLOAD_TV_5_DIGIT_CODE;
   }
   if(flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR) {
      ir_rf_database_status_ |= IR_RF_DATABASE_STATUS_DOWNLOAD_AVR_5_DIGIT_CODE;
   }
   if(flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_RF_DB) {
      ir_rf_database_status_ |=   IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES;
      ir_rf_database_status_ &= ~(IR_RF_DATABASE_STATUS_DB_DOWNLOAD_NO);
   }
   if(ir_rf_database_status_ != IR_RF_DATABASE_STATUS_DEFAULT) {
      ctrlm_rf4ce_polling_action_push(network_id_get(), controller_id_get(), RF4CE_POLLING_ACTION_IRRF_STATUS, NULL, 0);
   }
}

guint8 ctrlm_obj_controller_rf4ce_t::polling_methods_get() const {
   return polling_methods_;
}


bool ctrlm_obj_controller_rf4ce_t::is_fmr_supported() const {
   return fmr_supported_;
}

gboolean ctrlm_obj_controller_rf4ce_t::is_batteries_changed(guchar new_voltage) {
   guchar battery_increase = (0.3 * 255 / 4);
   //If the new voltage goes up by 0.3v or more, consider this a batteries changed situation
   return(new_voltage >= (battery_status_.voltage_unloaded + battery_increase));
}

gboolean ctrlm_obj_controller_rf4ce_t::is_batteries_large_voltage_jump(guchar new_voltage) {
   guchar battery_increase = (0.2 * 255 / 4);
   //If the new voltage goes up by 0.2v or more but less than 0.3v, don't set the new voltage but report as a large jump.
   return(new_voltage >= (battery_status_.voltage_unloaded + battery_increase));
}

void ctrlm_obj_controller_rf4ce_t::ota_failure_count_set(uint8_t ota_failures) {
   ota_failures_  = (ota_failures_ >= 4) ? 0 : ota_failures;
   ctrlm_db_rf4ce_write_ota_failures_count(network_id_get(), controller_id_get(), ota_failures_);
}

uint8_t ctrlm_obj_controller_rf4ce_t::ota_failure_count_get(void) {
   return ota_failures_;
}

bool ctrlm_obj_controller_rf4ce_t::is_controller_type_z(void) {
   bool is_type_z = (ota_failures_ >= 2) ? true : false;
   LOG_INFO("%s: Controller id %d (%s) is %s\n", __FUNCTION__, controller_id_get(), ctrlm_rf4ce_controller_type_str(controller_type_), is_type_z ? "TYPE Z" : "NOT TYPE Z");
   return is_type_z;
}
