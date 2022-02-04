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
#include <strings.h>
#include <bsd/string.h>
#if CTRLM_HAL_RF4CE_API_VERSION >= 15 && !defined(CTRLM_HOST_DECRYPTION_NOT_SUPPORTED)
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#endif
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>
#include <algorithm>
#include <regex>
#include <limits.h>
#include <sys/sysinfo.h>
#include "../ctrlm.h"
#include "../ctrlm_utils.h"
#include "../ctrlm_rcu.h"
#include "ctrlm_rf4ce_network.h"
#include "../ctrlm_database.h"
#include "../ctrlm_voice.h"
#include "../ctrlm_recovery.h"
#include "../json_config.h"
#include "../ctrlm_tr181.h"
#include "../ctrlm_rfc.h"
#include "../ctrlm_rcu.h"
#ifdef ASB
#include "../cpc/asb/advanced_secure_binding.h"
#endif
#include <zlib.h>
#ifdef USE_VOICE_SDK
#include "../ctrlm_voice_obj.h"
#include "comcastIrKeyCodes.h"
#include "irMgr.h"
#endif

#if (JSON_INT_VALUE_NETWORK_RF4CE_AUTOBIND_CONFIG_QTY_PASS > 7) || (JSON_INT_VALUE_NETWORK_RF4CE_AUTOBIND_CONFIG_QTY_PASS < 1)
#error RF4CE AUTOBIND PASS THRESHOLD IS OUT OF RANGE
#endif
#if (JSON_INT_VALUE_NETWORK_RF4CE_AUTOBIND_CONFIG_QTY_FAIL > 7) || (JSON_INT_VALUE_NETWORK_RF4CE_AUTOBIND_CONFIG_QTY_FAIL < 1)
#error RF4CE AUTOBIND FAIL THRESHOLD IS OUT OF RANGE
#endif

#if (JSON_INT_VALUE_NETWORK_RF4CE_BINDING_MENU_MODE_AUTOBIND_CONFIG_QTY_PASS > 7) || (JSON_INT_VALUE_NETWORK_RF4CE_BINDING_MENU_MODE_AUTOBIND_CONFIG_QTY_PASS < 1)
#error RF4CE AUTOBIND PASS THRESHOLD IS OUT OF RANGE
#endif
#if (JSON_INT_VALUE_NETWORK_RF4CE_BINDING_MENU_MODE_AUTOBIND_CONFIG_QTY_FAIL > 7) || (JSON_INT_VALUE_NETWORK_RF4CE_BINDING_MENU_MODE_AUTOBIND_CONFIG_QTY_FAIL < 1)
#error RF4CE AUTOBIND FAIL THRESHOLD IS OUT OF RANGE
#endif

// Defines for Regex to parse RFC result
#define CTRLM_RF4CE_BLACKOUT_RFC_FILE                "/opt/RFC/.RFC_rf4ce_pair_blackout.ini"
#define CTRLM_RF4CE_REGEX_BLACKOUT_ENABLE            "(?<=rf4ce_pair_blackout=)(false|true)"
#define CTRLM_RF4CE_REGEX_BLACKOUT_FAIL_THRESHOLD    "(?<=pairing_fail_threshold=)[0-9]*"
#define CTRLM_RF4CE_REGEX_BLACKOUT_REBOOT_THRESHOLD  "(?<=blackout_reboot_threshold=)[0-9]*"
#define CTRLM_RF4CE_REGEX_BLACKOUT_TIME              "(?<=blackout_time=)[0-9]*"
#define CTRLM_RF4CE_REGEX_BLACKOUT_TIME_INCREMENT    "(?<=blackout_time_increment=)[0-9]*"

#define CTRLM_RF4CE_DPI_FRAME_CONTROL                (0x2F)
#define CTRLM_RF4CE_QORVO_BAD_MAC_ADDRESS            (0xA5A5A5A5A5A5A5A5llu)
#define CTRLM_RF4CE_QORVO_MAC_ADDRESS_PATTERN        (0x00155F0000000000llu)


typedef struct controller_type_details_t {
      version_software_t software_version;
      version_software_t audio_version;
      version_hardware_t hw_version;
      version_software_t dsp_version;
      version_software_t keyword_model_version;
      version_software_t arm_version;
} controller_type_details_t;

#if (CTRLM_HAL_RF4CE_API_VERSION >= 11)
static ctrlm_hal_rf4ce_deepsleep_arguments_t dpi_args_field = {3, {{CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU,   CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x01, {RF4CE_FRAME_CONTROL_USER_CONTROL_PRESSED}}, // All XRC key downs
                                                             {CTRLM_RF4CE_PROFILE_ID_VOICE,         CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x00, {0x00}}, // All voice packets
                                                             {CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU,   CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x02, {RF4CE_FRAME_CONTROL_GHOST_CODE, CTRLM_RCU_GHOST_CODE_POWER_ON}}}}; // Power on ghost code
static ctrlm_hal_rf4ce_deepsleep_arguments_t dpi_args_new = {6, {{CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU,   CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x01, {RF4CE_FRAME_CONTROL_USER_CONTROL_PRESSED}}, // All XRC key downs
                                                             {CTRLM_RF4CE_PROFILE_ID_VOICE,         CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x01, {MSO_VOICE_CMD_ID_VOICE_SESSION_REQUEST}}, // Voice session request
                                                             {CTRLM_RF4CE_PROFILE_ID_DEVICE_UPDATE, CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x01, {RF4CE_DEVICE_UPDATE_CMD_IMAGE_CHECK_REQUEST}}, // CFIR
                                                             {CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU,   CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x01, {RF4CE_FRAME_CONTROL_GHOST_CODE}}, // All ghost codes
                                                             {CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU,   CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x01, {RF4CE_FRAME_CONTROL_SET_ATTRIBUTE_REQUEST}}, // Rib Set
                                                             {CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU,   CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x01, {RF4CE_FRAME_CONTROL_GET_ATTRIBUTE_REQUEST}}}}; // Rib Get

static ctrlm_hal_rf4ce_deepsleep_arguments_t dpi_args_test = {3, {{CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU,   CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x02, {RF4CE_FRAME_CONTROL_USER_CONTROL_PRESSED, CTRLM_KEY_CODE_DIGIT_2}}, //  key downs 2
                                                                 {CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU,   CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x02, {RF4CE_FRAME_CONTROL_USER_CONTROL_RELEASED, CTRLM_KEY_CODE_DIGIT_4}}, // key ups 4
                                                                 {CTRLM_RF4CE_PROFILE_ID_DEVICE_UPDATE, CTRLM_RF4CE_DPI_FRAME_CONTROL, 0x01, {RF4CE_DEVICE_UPDATE_CMD_IMAGE_DATA_REQUEST}}}}; // Data requests
#endif


#ifdef USE_VOICE_SDK
static void ctrlm_network_rf4ce_cfm_voice_session_rsp(ctrlm_hal_rf4ce_result_t result, void *user_data);
#endif

using namespace std;

ctrlm_obj_network_rf4ce_t *ctrlm_obj_network_rf4ce_t::instance = NULL;

ctrlm_obj_network_rf4ce_t::ctrlm_obj_network_rf4ce_t(ctrlm_network_type_t type, ctrlm_network_id_t id, const char *name, gboolean mask_key_codes, json_t *json_obj_net_rf4ce, GThread *original_thread) :
   ctrlm_obj_network_t(type, id, name, mask_key_codes, original_thread)
{
   LOG_INFO("ctrlm_obj_network_rf4ce_t constructor - Type (%u) Id (%u) Name (%s)\n", type, id, name);
   hal_api_pair_            = NULL;
   hal_api_unpair_          = NULL;
   hal_api_data_            = NULL;
   hal_api_rib_data_import_ = NULL;
   hal_api_rib_data_export_ = NULL;
   errno_t safec_rc = -1;

   discovery_config_normal_.enabled               = JSON_BOOL_VALUE_NETWORK_RF4CE_DISCOVERY_CONFIG_ENABLE;
   discovery_config_normal_.require_line_of_sight = JSON_BOOL_VALUE_NETWORK_RF4CE_DISCOVERY_CONFIG_REQUIRE_LINE_OF_SIGHT;

   discovery_config_menu_.enabled                 = JSON_BOOL_VALUE_NETWORK_RF4CE_BINDING_MENU_MODE_DISCOVERY_CONFIG_ENABLE;
   discovery_config_menu_.require_line_of_sight   = JSON_BOOL_VALUE_NETWORK_RF4CE_BINDING_MENU_MODE_DISCOVERY_CONFIG_REQUIRE_LINE_OF_SIGHT;

   autobind_config_normal_.enabled        = JSON_BOOL_VALUE_NETWORK_RF4CE_AUTOBIND_CONFIG_ENABLE;
   autobind_config_normal_.threshold_pass = JSON_INT_VALUE_NETWORK_RF4CE_AUTOBIND_CONFIG_QTY_PASS;
   autobind_config_normal_.threshold_fail = JSON_INT_VALUE_NETWORK_RF4CE_AUTOBIND_CONFIG_QTY_FAIL;
   autobind_config_normal_.octet          = CTRLM_RF4CE_AUTOBIND_OCTET;

   autobind_config_menu_.enabled          = JSON_BOOL_VALUE_NETWORK_RF4CE_BINDING_MENU_MODE_AUTOBIND_CONFIG_ENABLE;
   autobind_config_menu_.threshold_pass   = JSON_INT_VALUE_NETWORK_RF4CE_BINDING_MENU_MODE_AUTOBIND_CONFIG_QTY_PASS;
   autobind_config_menu_.threshold_fail   = JSON_INT_VALUE_NETWORK_RF4CE_BINDING_MENU_MODE_AUTOBIND_CONFIG_QTY_FAIL;
   autobind_config_menu_.octet            = CTRLM_RF4CE_AUTOBIND_OCTET_MENU;

   autobind_status_.disc_respond = 0;
   autobind_status_.disc_ignore  = 0;

   blackout_.is_blackout_enabled        = JSON_BOOL_VALUE_NETWORK_RF4CE_PAIRING_BLACKOUT_ENABLE;
   blackout_.pairing_fail_threshold     = JSON_INT_VALUE_NETWORK_RF4CE_PAIRING_BLACKOUT_PAIRING_FAIL_THRESHOLD;
   blackout_.pairing_fail_count         = 0;
   blackout_.blackout_reboot_threshold  = JSON_INT_VALUE_NETWORK_RF4CE_PAIRING_BLACKOUT_BLACKOUT_REBOOT_THRESHOLD;
   blackout_.blackout_count             = 0;
   blackout_.blackout_time              = JSON_INT_VALUE_NETWORK_RF4CE_PAIRING_BLACKOUT_BLACKOUT_TIME;
   blackout_.blackout_time_increment    = 1;
   blackout_.blackout_tag               = 0;
   blackout_.is_blackout                = FALSE;
   blackout_.force_blackout_settings    = JSON_BOOL_VALUE_NETWORK_RF4CE_PAIRING_BLACKOUT_FORCE_BLACKOUT_SETTINGS;

   user_string_                  = JSON_STR_VALUE_NETWORK_RF4CE_USER_STRING;
   class_inc_line_of_sight_      = JSON_INT_VALUE_NETWORK_RF4CE_CLASS_INC_LINE_OF_SIGHT;
   class_inc_recently_booted_    = JSON_INT_VALUE_NETWORK_RF4CE_CLASS_INC_RECENTLY_BOOTED;
   class_inc_binding_button_     = JSON_INT_VALUE_NETWORK_RF4CE_CLASS_INC_BINDING_BUTTON;
   class_inc_xr_                 = JSON_INT_VALUE_NETWORK_RF4CE_CLASS_INC_XR;
   class_inc_bind_table_empty_   = JSON_INT_VALUE_NETWORK_RF4CE_CLASS_INC_BIND_TABLE_EMPTY;
   class_inc_bind_menu_active_   = JSON_INT_VALUE_NETWORK_RF4CE_BINDING_MENU_MODE_CLASS_INCREMENT;
   timeout_key_release_          = JSON_INT_VALUE_NETWORK_RF4CE_TIMEOUT_KEY_RELEASE;

   short_rf_retry_period_        = JSON_INT_VALUE_NETWORK_RF4CE_SHORT_RF_RETRY_PERIOD;
   utterance_duration_max_       = JSON_INT_VALUE_NETWORK_RF4CE_UTTERANCE_DURATION_MAX;
   voice_data_retry_max_         = JSON_INT_VALUE_NETWORK_RF4CE_VOICE_DATA_RETRY_MAX;
   voice_csma_backoff_max_       = JSON_INT_VALUE_NETWORK_RF4CE_VOICE_CSMA_BACKOFF_MAX;
   voice_data_backoff_exp_min_   = JSON_INT_VALUE_NETWORK_RF4CE_VOICE_DATA_BACKOFF_EXP_MIN;
   rib_update_check_interval_    = JSON_INT_VALUE_NETWORK_RF4CE_RIB_UPDATE_CHECK_INTERVAL;
   auto_check_validation_period_ = JSON_INT_VALUE_NETWORK_RF4CE_AUTO_CHECK_VALIDATION_PERIOD;
   link_lost_wait_time_          = JSON_INT_VALUE_NETWORK_RF4CE_LINK_LOST_WAIT_TIME;
   update_polling_period_        = JSON_INT_VALUE_NETWORK_RF4CE_UPDATE_POLLING_PERIOD;
   data_request_wait_time_       = JSON_INT_VALUE_NETWORK_RF4CE_DATA_REQUEST_WAIT_TIME;
   audio_profiles_targ_          = JSON_INT_VALUE_NETWORK_RF4CE_AUDIO_PROFILES_TARGET;
   voice_command_encryption_     = (voice_command_encryption_t)JSON_INT_VALUE_NETWORK_RF4CE_VOICE_COMMAND_ENCRYPTION;
   host_decryption_              = JSON_BOOL_VALUE_NETWORK_RF4CE_HOST_DECRYPTION;
   controller_id_to_remove_      = CTRLM_HAL_CONTROLLER_ID_INVALID;

   mfg_test_.enabled             = JSON_BOOL_VALUE_NETWORK_RF4CE_MFG_TEST_ENABLE;
   mfg_test_.mic_delay           = JSON_INT_VALUE_NETWORK_RF4CE_MFG_TEST_MIC_DELAY;
   mfg_test_.mic_duration        = JSON_INT_VALUE_NETWORK_RF4CE_MFG_TEST_MIC_DURATION;
   mfg_test_.sweep_delay         = JSON_INT_VALUE_NETWORK_RF4CE_MFG_TEST_SWEEP_DELAY;
   mfg_test_.haptic_delay        = JSON_INT_VALUE_NETWORK_RF4CE_MFG_TEST_HAPTIC_DELAY;
   mfg_test_.haptic_duration     = JSON_INT_VALUE_NETWORK_RF4CE_MFG_TEST_HAPTIC_DURATION;
   mfg_test_.reset_delay         = JSON_INT_VALUE_NETWORK_RF4CE_MFG_TEST_RESET_DELAY;
   polling_methods_              = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_TARGET_METHODS;
   max_fmr_controllers_          = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_TARGET_FMR_CONTROLLERS_MAX;

#if (CTRLM_HAL_RF4CE_API_VERSION >= 11)
   dpi_args_                     = &dpi_args_field;
#endif

   default_polling_configuration();

#ifdef ASB
   // ASB Config
   asb_enabled_                  = JSON_BOOL_VALUE_NETWORK_RF4CE_ASB_ENABLE; // DEFAULT FALSE
   asb_key_derivation_methods_   = JSON_INT_VALUE_NETWORK_RF4CE_ASB_DERIVATION_METHODS;
   asb_fallback_count_           = 0;
   asb_fallback_count_threshold_ = JSON_INT_VALUE_NETWORK_RF4CE_ASB_FALLBACK_THRESHOLD;
   asb_force_settings_           = JSON_BOOL_VALUE_NETWORK_RF4CE_ASB_FORCE_SETTINGS; // DEFAULT FALSE
   // End ASB Config
#endif

   #ifdef USE_VOICE_SDK
   voice_session_rsp_confirm_ = NULL;
   voice_session_rsp_confirm_param_ = NULL;
   safec_rc = memset_s(&voice_session_rsp_params_, sizeof(voice_session_rsp_params_), 0, sizeof(voice_session_rsp_params_));
   ERR_CHK(safec_rc);
   voice_session_rsp_params_.network_id = (ctrlm_network_id_t *)malloc(sizeof(ctrlm_network_id_t));
   (*voice_session_rsp_params_.network_id) = network_id_get();
   voice_session_active_count_ = 0;
   #endif

   stream_begin_                 = (voice_session_response_stream_t)JSON_INT_VALUE_NETWORK_RF4CE_VOICE_STREAM_BEGIN;
   stream_offset_                = JSON_INT_VALUE_NETWORK_RF4CE_VOICE_STREAM_OFFSET;

   device_update_session_timeout_ = JSON_INT_VALUE_NETWORK_RF4CE_DEVICE_UPDATE_SESSION_TIMEOUT;

   dsp_configuration_ = {.flags                          = JSON_INT_VALUE_NETWORK_RF4CE_DSP_FLAGS,
                         .vad_threshold                  = JSON_INT_VALUE_NETWORK_RF4CE_DSP_VAD_THRESHOLD,
                         .no_vad_threshold               = JSON_INT_VALUE_NETWORK_RF4CE_DSP_NO_VAD_THRESHOLD,
                         .vad_hang_time                  = JSON_INT_VALUE_NETWORK_RF4CE_DSP_VAD_HANG_TIME,
                         .initial_eos_timeout            = JSON_INT_VALUE_NETWORK_RF4CE_DSP_INITIAL_EOS_TIMEOUT,
                         .eos_timeout                    = JSON_INT_VALUE_NETWORK_RF4CE_DSP_EOS_TIMEOUT,
                         .initial_speech_delay           = JSON_INT_VALUE_NETWORK_RF4CE_DSP_INITIAL_SPEECH_DELAY,
                         .primary_keyword_sensitivity    = JSON_INT_VALUE_NETWORK_RF4CE_DSP_PRIMARY_KEYWORD_SENSITIVITY,
                         .secondary_keyword_sensitivity  = JSON_INT_VALUE_NETWORK_RF4CE_DSP_SECONDARY_KEYWORD_SENSITIVITY,
                         .beamformer_type                = JSON_INT_VALUE_NETWORK_RF4CE_DSP_BEAMFORMER_TYPE,
                         .noise_reduction_aggressiveness = JSON_INT_VALUE_NETWORK_RF4CE_DSP_NOISE_REDUCTION_AGGRESSIVENESS,
                         .dynamic_gain_target_level      = JSON_INT_VALUE_NETWORK_RF4CE_DSP_DYNAMIC_GAIN_TARGET_LEVEL,
                         .ic_config_atten_update         = JSON_INT_VALUE_NETWORK_RF4CE_DSP_IC_ATTEN_UPDATE,
                         .ic_config_detect               = JSON_INT_VALUE_NETWORK_RF4CE_DSP_IC_DETECT};
   force_dsp_configuration_ = JSON_BOOL_VALUE_NETWORK_RF4CE_DSP_FORCE_SETTINGS;
   safec_rc = memset_s(&ff_configuration_, sizeof(ff_configuration_), 0, sizeof(ff_configuration_));
   ERR_CHK(safec_rc);

   load_config(json_obj_net_rf4ce);

   is_import_                    = FALSE;
   recovery_                     = CTRLM_RECOVERY_TYPE_NONE;
   nvm_backup_data_              = NULL;
   nvm_backup_len_               = 0;

   network_stats_is_cached       = FALSE;

   // If blackout settings from config are not forced, get settings from RFC
   if(FALSE == blackout_.force_blackout_settings) {
      blackout_settings_get_rfc();
   }
   g_atomic_int_set(&binding_in_progress_, FALSE);
   binding_in_progress_tag_ = 0;
   binding_in_progress_timeout_ = JSON_INT_VALUE_NETWORK_RF4CE_BINDING_STATE_TIMEOUT;

   safec_rc = memset_s(&target_irdb_status_, sizeof(target_irdb_status_t), 0, sizeof(target_irdb_status_t));
   ERR_CHK(safec_rc);
   target_irdb_status_.flags = TARGET_IRDB_STATUS_DEFAULT;
   g_mutex_init(&reverse_cmd_event_pending_mutex_);
   reverse_cmd_end_event_timer_id_ = 0;
   chime_timeout_ = 0;

   response_idle_time_ff_ = CTRLM_RF4CE_CONST_RESPONSE_IDLE_TIME_FF;
   ir_rf_database_new_    = false;
#if CTRLM_HAL_RF4CE_API_VERSION >= 15 && !defined(CTRLM_HOST_DECRYPTION_NOT_SUPPORTED)
   ctx_ = NULL;
#endif
   instance = this;
}

#ifndef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
guint32                    ctrlm_obj_network_rf4ce_t::short_rf_retry_period_get(void)        { THREAD_ID_VALIDATE(); return(short_rf_retry_period_);        }
guint16                    ctrlm_obj_network_rf4ce_t::utterance_duration_max_get(void)       { THREAD_ID_VALIDATE(); return(utterance_duration_max_);       }
guchar                     ctrlm_obj_network_rf4ce_t::voice_data_retry_max_get(void)         { THREAD_ID_VALIDATE(); return(voice_data_retry_max_);         }
guchar                     ctrlm_obj_network_rf4ce_t::voice_csma_backoff_max_get(void)       { THREAD_ID_VALIDATE(); return(voice_csma_backoff_max_);       }
guchar                     ctrlm_obj_network_rf4ce_t::voice_data_backoff_exp_min_get(void)   { THREAD_ID_VALIDATE(); return(voice_data_backoff_exp_min_);   }
guint16                    ctrlm_obj_network_rf4ce_t::rib_update_check_interval_get(void)    { THREAD_ID_VALIDATE(); return(rib_update_check_interval_);    }
guint16                    ctrlm_obj_network_rf4ce_t::auto_check_validation_period_get(void) { THREAD_ID_VALIDATE(); return(auto_check_validation_period_); }
guint16                    ctrlm_obj_network_rf4ce_t::link_lost_wait_time_get(void)          { THREAD_ID_VALIDATE(); return(link_lost_wait_time_);          }
guint16                    ctrlm_obj_network_rf4ce_t::update_polling_period_get(void)        { THREAD_ID_VALIDATE(); return(update_polling_period_);        }
guint16                    ctrlm_obj_network_rf4ce_t::data_request_wait_time_get(void)       { THREAD_ID_VALIDATE(); return(data_request_wait_time_);       }
voice_command_encryption_t ctrlm_obj_network_rf4ce_t::voice_command_encryption_get(void)     { THREAD_ID_VALIDATE(); return(voice_command_encryption_);     }
#endif
guint16                    ctrlm_obj_network_rf4ce_t::audio_profiles_targ_get(void)          { THREAD_ID_VALIDATE(); return(audio_profiles_targ_);          }

ctrlm_obj_network_rf4ce_t::ctrlm_obj_network_rf4ce_t() {
   LOG_INFO("ctrlm_obj_network_rf4ce_t constructor - default\n");
}

ctrlm_obj_network_rf4ce_t::~ctrlm_obj_network_rf4ce_t() {
   LOG_INFO("ctrlm_obj_network_rf4ce_t deconstructor\n");
   for (auto timeout : bind_validation_failed_timeout_) {
       ctrlm_timeout_destroy(&timeout->timer_id);
       if (controller_exists(timeout->controller_id)){
           LOG_INFO("ctrlm_obj_network_rf4ce_t: clean up failed validation left overs\n");
           controller_unpair(timeout->controller_id);
           controller_remove(timeout->controller_id, true);
           g_free(timeout);
       }
   }
   for(map <ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin(); it != controllers_.end(); it++) {
      if(it->second != NULL) {
         delete it->second;
      }
   }
   g_mutex_clear(&reverse_cmd_event_pending_mutex_);

   #ifdef USE_VOICE_SDK
   if(voice_session_rsp_params_.network_id != NULL) {
      free(voice_session_rsp_params_.network_id);
      voice_session_rsp_params_.network_id = NULL;
   }
   #endif
   #if CTRLM_HAL_RF4CE_API_VERSION >= 15 && !defined(CTRLM_HOST_DECRYPTION_NOT_SUPPORTED)
   sec_deinit();
   #endif
   instance = NULL;
}

void ctrlm_obj_network_rf4ce_t::hal_api_main_set(ctrlm_hal_rf4ce_network_main_t main) {
   THREAD_ID_VALIDATE();
   hal_api_main_ = main;
}

ctrlm_hal_result_t ctrlm_obj_network_rf4ce_t::hal_init_request(GThread *ctrlm_main_thread) {
   ctrlm_hal_rf4ce_main_init_t main_init;
   ctrlm_main_thread_ = ctrlm_main_thread;
   THREAD_ID_VALIDATE();

   // Initialization parameters
   main_init.api_version      = CTRLM_HAL_RF4CE_API_VERSION;
   main_init.network_id       = network_id_get();
   main_init.cfm_init         = ctrlm_hal_rf4ce_cfm_init;
   main_init.ind_discovery    = ctrlm_hal_rf4ce_ind_discovery;
   main_init.ind_pair         = ctrlm_hal_rf4ce_ind_pair;
   main_init.ind_pair_result  = ctrlm_hal_rf4ce_ind_pair_result;
   main_init.ind_unpair       = ctrlm_hal_rf4ce_ind_unpair;
   main_init.ind_data         = ctrlm_hal_rf4ce_ind_data;
#if CTRLM_HAL_RF4CE_API_VERSION >= 9
   main_init.assert           = ctrlm_on_network_assert;
#if CTRLM_HAL_RF4CE_API_VERSION >= 15
   main_init.decrypt           = 0;
   if (voice_command_encryption_ == VOICE_COMMAND_ENCRYPTION_ENABLED && host_decryption_) {
#ifndef CTRLM_HOST_DECRYPTION_NOT_SUPPORTED
      if (sec_init()) {
         LOG_INFO("%s: Using host decryption.\n", __FUNCTION__);
         main_init.decrypt           = hal_rf4ce_decrypt_callback;
      } else {
         LOG_ERROR("%s: Host decryption initialization had failed. Using chip based decryption.\n", __FUNCTION__);
      }
#else
   LOG_ERROR("%s: Host decryption is not supported on this platform.\n", __FUNCTION__);
#endif // CTRLM_HOST_DECRYPTION_NOT_SUPPORTED
   }
#endif // CTRLM_HAL_RF4CE_API_VERSION >= 15
   main_init.nvm_reinit       = 0;
   main_init.nvm_restore_data = NULL;
   main_init.nvm_restore_len  = 0;

   // Set NVM restore parameters based on backup state
   switch(recovery_) {
      case CTRLM_RECOVERY_TYPE_BACKUP:
      {
         // Function to get backup NVM data
         LOG_WARN("%s: In recovery backup mode.. HAL NVM Backup is going to be restored\n", __FUNCTION__);
         main_init.nvm_restore_len = restore_hal_nvm(&main_init.nvm_restore_data);
         break;
      }
      case CTRLM_RECOVERY_TYPE_RESET:
      {
         LOG_WARN("%s: In recovery reset mode.. HAL NVM about to be wiped\n", __FUNCTION__);
         main_init.nvm_reinit = 1;
         break;
      }
      default:
      {
         // Do nothing
         break;
      }
   }
#endif // CTRLM_HAL_RF4CE_API_VERSION >= 9

   // Initialize condition and mutex
   g_cond_init(&condition_);
   g_mutex_init(&mutex_);

   g_mutex_lock(&mutex_);
   hal_thread_ = g_thread_new(name_get(), (void* (*)(void*))hal_api_main_, &main_init);

   // Block until initialization is complete or a timeout occurs
   LOG_INFO("%s: Waiting for %s initialization...\n", __FUNCTION__, name_get());
   g_cond_wait(&condition_, &mutex_);
   g_mutex_unlock(&mutex_);

   ready_ = (CTRLM_HAL_RESULT_SUCCESS == init_result_);

   return(init_result_);
}

void ctrlm_obj_network_rf4ce_t::hal_init_confirm(ctrlm_hal_rf4ce_cfm_init_params_t params) {
   THREAD_ID_VALIDATE();

   init_result_ = params.result;

   version_         = params.version;
   chipset_         = params.chipset;
   pan_id_          = params.pan_id;
   ieee_address_    = params.ieee_address;
   short_address_   = params.short_address;
#if CTRLM_HAL_RF4CE_API_VERSION >= 9
   nvm_backup_data_ = params.nvm_backup_data;
   nvm_backup_len_  = params.nvm_backup_len;
#endif

   // Unblock the caller of hal_init
   g_mutex_lock(&mutex_);
   g_cond_broadcast(&condition_);
   g_mutex_unlock(&mutex_);
}

void ctrlm_obj_network_rf4ce_t::hal_rf4ce_api_set(ctrlm_hal_rf4ce_req_pair_t                   pair,
                                                  ctrlm_hal_rf4ce_req_unpair_t                 unpair,
                                                  ctrlm_hal_rf4ce_req_data_t                   data,
                                                  ctrlm_hal_rf4ce_rib_data_import_t            rib_data_import,
                                                  ctrlm_hal_rf4ce_rib_data_export_t            rib_data_export) {
   THREAD_ID_VALIDATE();
   hal_api_pair_                   = pair;
   hal_api_unpair_                 = unpair;
   hal_api_data_                   = data;
   hal_api_rib_data_import_        = rib_data_import;
   hal_api_rib_data_export_        = rib_data_export;
}

ctrlm_hal_result_t ctrlm_obj_network_rf4ce_t::controller_unpair(ctrlm_controller_id_t controller_id) {
   if(hal_api_unpair_ == NULL) {
      LOG_ERROR("%s: NULL HAL API\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR);
   }
   return(hal_api_unpair_(controller_id));
}

void ctrlm_obj_network_rf4ce_t::controller_unbind(ctrlm_controller_id_t controller_id, ctrlm_unbind_reason_t reason) {
   THREAD_ID_VALIDATE();
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller Id %u does not exist!\n", __FUNCTION__, controller_id);
      return;
   }
   if(reason != CTRLM_UNBIND_REASON_CONTROLLER) {
      // Unpair the controller
      controller_unpair(controller_id);
   }
   // Telemetry needs to keep track of unbinding.  
   controllers_[controller_id]->log_unbinding_for_telemetry();
   // Remove the controller from the controller list and delete the DB entry
   controller_remove(controller_id, true);

   // Send controller unbind event
   ctrlm_network_iarm_event_controller_unbind(network_id_get(), controller_id, reason);
}

ctrlm_rf4ce_controller_type_t ctrlm_obj_network_rf4ce_t::controller_type_get(ctrlm_controller_id_t controller_id) {
   THREAD_ID_VALIDATE();
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller Id %u does not exist!\n", __FUNCTION__, controller_id);
      return(RF4CE_CONTROLLER_TYPE_INVALID);
   }
   return(controllers_[controller_id]->controller_type_get());
}

ctrlm_rcu_controller_type_t ctrlm_obj_network_rf4ce_t::ctrlm_controller_type_get(ctrlm_controller_id_t controller_id) {
   THREAD_ID_VALIDATE();
   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Invalid Controller Id\n", __FUNCTION__);
      return(CTRLM_RCU_CONTROLLER_TYPE_INVALID);
   }

   ctrlm_rf4ce_controller_type_t rf4ce_controller_type = controllers_[controller_id]->controller_type_get();
   LOG_INFO("%s: Controller Id %u - Controller Type <%d>.\n", __FUNCTION__, controller_id, rf4ce_controller_type);
   switch(rf4ce_controller_type) {
      case RF4CE_CONTROLLER_TYPE_XR2:     return(CTRLM_RCU_CONTROLLER_TYPE_XR2);
      case RF4CE_CONTROLLER_TYPE_XR5:     return(CTRLM_RCU_CONTROLLER_TYPE_XR5);
      case RF4CE_CONTROLLER_TYPE_XR11:    return(CTRLM_RCU_CONTROLLER_TYPE_XR11);
      case RF4CE_CONTROLLER_TYPE_XR15:    return(CTRLM_RCU_CONTROLLER_TYPE_XR15);
      case RF4CE_CONTROLLER_TYPE_XR15V2:  return(CTRLM_RCU_CONTROLLER_TYPE_XR15V2);
      case RF4CE_CONTROLLER_TYPE_XR16:    return(CTRLM_RCU_CONTROLLER_TYPE_XR16);
      case RF4CE_CONTROLLER_TYPE_XR18:    return(CTRLM_RCU_CONTROLLER_TYPE_XR18);
      case RF4CE_CONTROLLER_TYPE_XR19:    return(CTRLM_RCU_CONTROLLER_TYPE_XR19);
      case RF4CE_CONTROLLER_TYPE_XRA:     return(CTRLM_RCU_CONTROLLER_TYPE_XRA);
      case RF4CE_CONTROLLER_TYPE_UNKNOWN: return(CTRLM_RCU_CONTROLLER_TYPE_UNKNOWN);
      case RF4CE_CONTROLLER_TYPE_INVALID: return(CTRLM_RCU_CONTROLLER_TYPE_INVALID);
   }
   return(CTRLM_RCU_CONTROLLER_TYPE_INVALID);
}

ctrlm_rcu_binding_type_t ctrlm_obj_network_rf4ce_t::ctrlm_binding_type_get(ctrlm_controller_id_t controller_id) {
   THREAD_ID_VALIDATE();
   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Invalid Controller Id\n", __FUNCTION__);
      return(CTRLM_RCU_BINDING_TYPE_INVALID);
   }

   return(controllers_[controller_id]->binding_type_get());
}

void ctrlm_obj_network_rf4ce_t::ctrlm_controller_status_get(ctrlm_controller_id_t controller_id, void *status) {
   THREAD_ID_VALIDATE();
   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Invalid Controller Id\n", __FUNCTION__);
      return;
   }

   controllers_[controller_id]->rf4ce_controller_status((ctrlm_controller_status_t *)status);
}

ctrlm_rf4ce_controller_type_t ctrlm_obj_network_rf4ce_t::controller_type_from_user_string(guchar *user_string) {
   THREAD_ID_VALIDATE();
   ctrlm_rf4ce_controller_type_t controller_type = RF4CE_CONTROLLER_TYPE_UNKNOWN;
   gchar                         product_name[CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME + 1];
   ctrlm_rf4ce_product_name_t product_type;
   ctrlm_rf4ce_controller_type_t ctrlm_type;

   if(user_string == NULL) {
      LOG_ERROR("%s: NULL User string\n", __FUNCTION__);
      return controller_type;
   }

   LOG_INFO("%s: User string <%s>\n", __FUNCTION__, user_string);
   errno_t safec_rc = strcpy_s(product_name, sizeof(product_name), (const char *)user_string);
   ERR_CHK(safec_rc);

   // Convert the string to controller type
   if(0 == strncmp((const char *)&user_string[7], "COMCAST", 7)) {
      LOG_WARN("%s: Assuming XR2. 0x%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n", __FUNCTION__, user_string[0], user_string[1], user_string[2],  user_string[3],  user_string[4],  user_string[5],  user_string[6],  user_string[7],
                                                                                                                          user_string[8], user_string[9], user_string[10], user_string[11], user_string[12], user_string[13], user_string[14], user_string[15]);
      controller_type = RF4CE_CONTROLLER_TYPE_XR2;
   } else if(ctrlm_rf4ce_get_product_type_from_product_name(product_name,&product_type,&ctrlm_type)) {

      controller_type = ctrlm_type;

   } else {

      LOG_ERROR("%s: Unsupported controller type <%s> 0x%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n", __FUNCTION__, product_name, user_string[0], user_string[1], user_string[2],  user_string[3],  user_string[4],  user_string[5],  user_string[6],  user_string[7], user_string[8], user_string[9], user_string[10], user_string[11], user_string[12], user_string[13], user_string[14], user_string[15]);
   }

   return controller_type;
}

unsigned char ctrlm_obj_network_rf4ce_t::ctrlm_battery_level_percent(ctrlm_controller_id_t controller_id, unsigned char voltage_loaded) {
   THREAD_ID_VALIDATE();
   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Invalid Controller Id\n", __FUNCTION__);
      return(CTRLM_RCU_CONTROLLER_TYPE_INVALID);
   }

   unsigned char battery_percentage = controllers_[controller_id]->battery_level_percent(voltage_loaded);
   LOG_INFO("%s: Controller Id %u - Battery Percentage <%d>.\n", __FUNCTION__, controller_id, battery_percentage);
   return(battery_percentage);
}

gboolean ctrlm_obj_network_rf4ce_t::load_config(json_t *json_obj_net_rf4ce) {

   json_config conf;
#if (CTRLM_HAL_RF4CE_API_VERSION >= 11)
   int dpi_pattern_list = 0;
#endif
  
   if (!conf.config_object_set(json_obj_net_rf4ce)){
      LOG_INFO("%s: use default configuration\n", __FUNCTION__);
   } else {
      conf.config_value_get(JSON_STR_NAME_NETWORK_RF4CE_USER_STRING,user_string_);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_TIMEOUT_KEY_RELEASE, timeout_key_release_, 0);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_SHORT_RF_RETRY_PERIOD,short_rf_retry_period_,0);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_UTTERANCE_DURATION_MAX, utterance_duration_max_,0);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_VOICE_DATA_RETRY_MAX,voice_data_retry_max_,0);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_VOICE_CSMA_BACKOFF_MAX,voice_csma_backoff_max_,0);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_VOICE_DATA_BACKOFF_EXP_MIN,voice_data_backoff_exp_min_,0);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_VOICE_COMMAND_ENCRYPTION,voice_command_encryption_,VOICE_COMMAND_ENCRYPTION_DISABLED,VOICE_COMMAND_ENCRYPTION_DEFAULT);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_RIB_UPDATE_CHECK_INTERVAL,rib_update_check_interval_,0);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_AUTO_CHECK_VALIDATION_PERIOD,auto_check_validation_period_,0);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_LINK_LOST_WAIT_TIME,link_lost_wait_time_,0);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_UPDATE_POLLING_PERIOD,update_polling_period_,0);
      conf.config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_HOST_DECRYPTION,host_decryption_,host_decryption_);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DATA_REQUEST_WAIT_TIME,data_request_wait_time_, 0);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_AUDIO_PROFILES_TARGET,audio_profiles_targ_,0,7);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_CLASS_INC_LINE_OF_SIGHT,class_inc_line_of_sight_,0,15);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_CLASS_INC_RECENTLY_BOOTED,class_inc_recently_booted_,0,15);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_CLASS_INC_BINDING_BUTTON,class_inc_binding_button_,0,15);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_CLASS_INC_XR,class_inc_xr_,0,15);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_CLASS_INC_BIND_TABLE_EMPTY,class_inc_bind_table_empty_,0,15);
      conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_BINDING_STATE_TIMEOUT, binding_in_progress_timeout_, 0);
      json_config sub_conf;
      if (conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_DISCOVERY_CONFIG,sub_conf)) {
         sub_conf.config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_DISCOVERY_CONFIG_ENABLE,discovery_config_normal_.enabled);
         sub_conf.config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_DISCOVERY_CONFIG_REQUIRE_LINE_OF_SIGHT,discovery_config_normal_.require_line_of_sight);
      }
      if (conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_AUTOBIND_CONFIG,sub_conf)) {
         sub_conf.config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_AUTOBIND_CONFIG_ENABLE,autobind_config_normal_.enabled);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_AUTOBIND_CONFIG_QTY_PASS,autobind_config_normal_.threshold_pass,1,7);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_AUTOBIND_CONFIG_QTY_FAIL,autobind_config_normal_.threshold_fail,1,7);
      }
      autobind_config_normal_.octet = ((autobind_config_normal_.threshold_fail << 3) | autobind_config_normal_.threshold_pass);
      if (conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_BINDING_MENU_MODE,sub_conf)) {
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_BINDING_MENU_MODE_CLASS_INCREMENT,class_inc_bind_menu_active_,0,15);
         json_config sub_bind_menu_conf;
         if (sub_conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_BINDING_MENU_MODE_DISCOVERY_CONFIG,sub_bind_menu_conf)) {
            sub_bind_menu_conf.config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_BINDING_MENU_MODE_DISCOVERY_CONFIG_ENABLE,discovery_config_menu_.enabled);
            sub_bind_menu_conf.config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_BINDING_MENU_MODE_DISCOVERY_CONFIG_REQUIRE_LINE_OF_SIGHT,discovery_config_menu_.require_line_of_sight,0,1);
         }
         if (sub_conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_BINDING_MENU_MODE_AUTOBIND_CONFIG,sub_bind_menu_conf)) {
            sub_bind_menu_conf.config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_BINDING_MENU_MODE_AUTOBIND_CONFIG_ENABLE,autobind_config_menu_.enabled);
            sub_bind_menu_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_BINDING_MENU_MODE_AUTOBIND_CONFIG_QTY_PASS,autobind_config_menu_.threshold_pass,1,7);
            sub_bind_menu_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_BINDING_MENU_MODE_AUTOBIND_CONFIG_QTY_FAIL,autobind_config_menu_.threshold_fail,1,7);
         }
       }
      autobind_config_menu_.octet = ((autobind_config_menu_.threshold_fail << 3) | autobind_config_menu_.threshold_pass);
      if (conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_PAIRING_BLACKOUT,sub_conf)) {
         sub_conf.config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_PAIRING_BLACKOUT_ENABLE,blackout_.is_blackout_enabled);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_PAIRING_BLACKOUT_PAIRING_FAIL_THRESHOLD,blackout_.pairing_fail_threshold,1);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_PAIRING_BLACKOUT_BLACKOUT_REBOOT_THRESHOLD,blackout_.blackout_reboot_threshold,1);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_PAIRING_BLACKOUT_BLACKOUT_TIME,blackout_.blackout_time,1);
         sub_conf.config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_PAIRING_BLACKOUT_FORCE_BLACKOUT_SETTINGS,blackout_.force_blackout_settings);
      }
      if(conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_MFG_TEST, sub_conf)) {
        sub_conf.config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_MFG_TEST_ENABLE, mfg_test_.enabled);
        sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_MFG_TEST_MIC_DELAY, mfg_test_.mic_delay);
        sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_MFG_TEST_MIC_DURATION, mfg_test_.mic_duration);
        sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_MFG_TEST_SWEEP_DELAY, mfg_test_.sweep_delay);
        sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_MFG_TEST_HAPTIC_DELAY, mfg_test_.haptic_delay);
        sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_MFG_TEST_HAPTIC_DURATION, mfg_test_.haptic_duration);
        sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_MFG_TEST_RESET_DELAY, mfg_test_.reset_delay);
      }
#if (CTRLM_HAL_RF4CE_API_VERSION >= 11)
      if(conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DPI_PATTERN, dpi_pattern_list, 0)) {
         switch(dpi_pattern_list) {
            case 1:  dpi_args_ = &dpi_args_new;   break;
            case 2:  dpi_args_ = &dpi_args_test;  break;
            default: dpi_args_ = &dpi_args_field; break;
         }
      }
#endif
      if(conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_POLLING, sub_conf)) {
         polling_config_read(&sub_conf);
      }

#ifdef ASB
      if(conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_ASB, sub_conf)) {
         asb_configuration(&sub_conf);
      } else {
         asb_configuration(NULL);
      }
#endif

      if(conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_VOICE, sub_conf)) {
        sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_VOICE_STREAM_BEGIN, stream_begin_, 0);
        sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_VOICE_STREAM_OFFSET, stream_offset_);
      }

      if(conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_DEVICE_UPDATE, sub_conf)) {
        sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DEVICE_UPDATE_SESSION_TIMEOUT, device_update_session_timeout_, 0);
      }

      if(conf.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_DSP, sub_conf)) {
         sub_conf.config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_DSP_FORCE_SETTINGS, force_dsp_configuration_);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_FLAGS, dsp_configuration_.flags, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_VAD_THRESHOLD, dsp_configuration_.vad_threshold, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_NO_VAD_THRESHOLD, dsp_configuration_.no_vad_threshold, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_VAD_HANG_TIME, dsp_configuration_.vad_hang_time, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_INITIAL_EOS_TIMEOUT, dsp_configuration_.initial_eos_timeout, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_EOS_TIMEOUT, dsp_configuration_.eos_timeout, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_INITIAL_SPEECH_DELAY, dsp_configuration_.initial_speech_delay, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_PRIMARY_KEYWORD_SENSITIVITY, dsp_configuration_.primary_keyword_sensitivity, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_SECONDARY_KEYWORD_SENSITIVITY, dsp_configuration_.secondary_keyword_sensitivity, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_BEAMFORMER_TYPE, dsp_configuration_.beamformer_type, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_NOISE_REDUCTION_AGGRESSIVENESS, dsp_configuration_.noise_reduction_aggressiveness, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_DYNAMIC_GAIN_TARGET_LEVEL, dsp_configuration_.dynamic_gain_target_level);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_IC_ATTEN_UPDATE, dsp_configuration_.ic_config_atten_update, 0x00, 0xFF);
         sub_conf.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_DSP_IC_DETECT, dsp_configuration_.ic_config_detect, 0x00, 0xFF);
      }
   }
   //Read tr181 values here. tr181 values will override any config file values.
   polling_config_tr181_read();
   process_xconf();

   LOG_INFO("%s: User String                   <%s>\n",     __FUNCTION__, user_string_.c_str());
   LOG_INFO("%s: Timeout Key Release           %u ms\n",    __FUNCTION__, timeout_key_release_);
   LOG_INFO("%s: Short RF Retry Period         %u ms\n",    __FUNCTION__, short_rf_retry_period_);
   LOG_INFO("%s: Utterance Duration Max        %u ms\n",    __FUNCTION__, utterance_duration_max_);
   LOG_INFO("%s: Voice Data Retry Max          %u\n",       __FUNCTION__, voice_data_retry_max_);
   LOG_INFO("%s: Voice CSMA Backoff Max        %u\n",       __FUNCTION__, voice_csma_backoff_max_);
   LOG_INFO("%s: Voice Data Backoff Exp Min    %u\n",       __FUNCTION__, voice_data_backoff_exp_min_);
   LOG_INFO("%s: Voice Command Encryption      <%s>\n",     __FUNCTION__, ctrlm_rf4ce_voice_command_encryption_str(voice_command_encryption_));
   LOG_INFO("%s: Rib Update Check Interval     %u hours\n", __FUNCTION__, rib_update_check_interval_);
   LOG_INFO("%s: Auto Check Validation Period  %u ms\n",    __FUNCTION__, auto_check_validation_period_);
   LOG_INFO("%s: Link Lost Wait Time           %u ms\n",    __FUNCTION__, link_lost_wait_time_);
   LOG_INFO("%s: Update Polling Period         %u hours\n", __FUNCTION__, update_polling_period_);
   LOG_INFO("%s: RF4CE Packet Host Decryption  <%s>\n",     __FUNCTION__, host_decryption_ ? "YES" : "NO");
   LOG_INFO("%s: Data Request Wait Time        %u ms\n",    __FUNCTION__, data_request_wait_time_);
   LOG_INFO("%s: Audio Profiles Target         0x%04X\n",   __FUNCTION__, audio_profiles_targ_);
   LOG_INFO("%s: Class Inc Line of Sight       %u\n",       __FUNCTION__, class_inc_line_of_sight_);
   LOG_INFO("%s: Class Inc Recently Booted     %u\n",       __FUNCTION__, class_inc_recently_booted_);
   LOG_INFO("%s: Class Inc Binding Button      %u\n",       __FUNCTION__, class_inc_binding_button_);
   LOG_INFO("%s: Class Inc XR                  %u\n",       __FUNCTION__, class_inc_xr_);
   LOG_INFO("%s: Class Inc Bind Table Empty    %u\n",       __FUNCTION__, class_inc_bind_table_empty_);
   LOG_INFO("%s: Normal Discovery Enable       <%s>\n",     __FUNCTION__, discovery_config_normal_.enabled ? "YES" : "NO");
   LOG_INFO("%s: Normal Discovery Require LOS  <%s>\n",     __FUNCTION__, discovery_config_normal_.require_line_of_sight ? "YES" : "NO");
   LOG_INFO("%s: Normal Autobind Enable        <%s>\n",     __FUNCTION__, autobind_config_normal_.enabled ? "YES" : "NO");
   LOG_INFO("%s: Normal Autobind Qty Pass      %u\n",       __FUNCTION__, autobind_config_normal_.threshold_pass);
   LOG_INFO("%s: Normal Autobind Qty Fail      %u\n",       __FUNCTION__, autobind_config_normal_.threshold_fail);
   LOG_INFO("%s: Menu Class Increment          %u\n",       __FUNCTION__, class_inc_bind_menu_active_);
   LOG_INFO("%s: Menu Discovery Enable         <%s>\n",     __FUNCTION__, discovery_config_menu_.enabled ? "YES" : "NO");
   LOG_INFO("%s: Menu Discovery Require LOS    <%s>\n",     __FUNCTION__, discovery_config_menu_.require_line_of_sight ? "YES" : "NO");
   LOG_INFO("%s: Menu Autobind Enable          <%s>\n",     __FUNCTION__, autobind_config_menu_.enabled ? "YES" : "NO");
   LOG_INFO("%s: Menu Autobind Qty Pass        %u\n",       __FUNCTION__, autobind_config_menu_.threshold_pass);
   LOG_INFO("%s: Menu Autobind Qty Fail        %u\n",       __FUNCTION__, autobind_config_menu_.threshold_fail);
   LOG_INFO("%s: Pairing Blackout Enable       <%s>\n",     __FUNCTION__, blackout_.is_blackout_enabled ? "YES" : "NO");
   LOG_INFO("%s: Blackout Fail Threshold       %u\n",       __FUNCTION__, blackout_.pairing_fail_threshold);
   LOG_INFO("%s: Blackout Reboot Threshold     %u\n",       __FUNCTION__, blackout_.blackout_reboot_threshold);
   LOG_INFO("%s: Blackout Time                 %u\n",       __FUNCTION__, blackout_.blackout_time);
   LOG_INFO("%s: Blackout Time Increment       %u\n",       __FUNCTION__, blackout_.blackout_time_increment);
   LOG_INFO("%s: Force Blackout Settings       <%s>\n",     __FUNCTION__, blackout_.force_blackout_settings ? "YES" : "NO");
   LOG_INFO("%s: Mfg Test Enable               <%s>\n",     __FUNCTION__, mfg_test_.enabled ? "YES" : "NO");
   LOG_INFO("%s: Mfg Test Mic Delay            %u\n",       __FUNCTION__, mfg_test_.mic_delay);
   LOG_INFO("%s: Mfg Test Mic Duration         %u\n",       __FUNCTION__, mfg_test_.mic_duration);
   LOG_INFO("%s: Mfg Test Sweep Delay          %u\n",       __FUNCTION__, mfg_test_.sweep_delay);
   LOG_INFO("%s: Mfg Test Haptic Delay         %u\n",       __FUNCTION__, mfg_test_.haptic_delay);
   LOG_INFO("%s: Mfg Test Haptic Duration      %u\n",       __FUNCTION__, mfg_test_.haptic_duration);
   LOG_INFO("%s: Mfg Test Reset Delay          %u\n",       __FUNCTION__, mfg_test_.reset_delay);
#if (CTRLM_HAL_RF4CE_API_VERSION >= 11)
   LOG_INFO("%s: DPI Pattern List              %u\n",       __FUNCTION__, dpi_pattern_list);
#endif
#ifdef ASB
   LOG_INFO("%s: ASB Force Settings            <%s>\n",     __FUNCTION__, (asb_force_settings_ ? "YES" : "NO"));
   LOG_INFO("%s: ASB Enabled                   <%s>\n",     __FUNCTION__, (asb_enabled_ ? "YES" : "NO"));
#endif
   LOG_INFO("%s: FF Voice Stream Begin         %d\n",       __FUNCTION__, (int)stream_begin_);
   LOG_INFO("%s: FF Voice Stream Offset        %d\n",       __FUNCTION__, stream_offset_);

   LOG_INFO("%s: Target Polling Methods        <%s>\n", __FUNCTION__, ctrlm_rf4ce_controller_polling_methods_str(polling_methods_));

   for(int i = 0; i < RF4CE_CONTROLLER_TYPE_INVALID; i++) {
      LOG_INFO("%s: Polling Configuration: type <%-6s> methods <%s>\n", __FUNCTION__, ctrlm_rf4ce_controller_type_str((ctrlm_rf4ce_controller_type_t)i), ctrlm_rf4ce_controller_polling_methods_str(controller_polling_methods_[i]));
      if(controller_polling_methods_[i] & POLLING_METHODS_FLAG_HEARTBEAT) {
         ctrlm_rf4ce_controller_polling_configuration_print(__FUNCTION__, "Heartbeat", &controller_polling_configuration_heartbeat_[i]);
      }
      if(controller_polling_methods_[i] & POLLING_METHODS_FLAG_MAC) {
         ctrlm_rf4ce_controller_polling_configuration_print(__FUNCTION__, "Mac", &controller_polling_configuration_mac_[i]);
      }
   }

   return(true);
}

void ctrlm_hal_rf4ce_cfm_data(ctrlm_hal_rf4ce_result_t result, void *user_data) {

}

ctrlm_hal_result_t ctrlm_obj_network_rf4ce_t::req_data(ctrlm_rf4ce_profile_id_t profile_id, ctrlm_controller_id_t controller_id, ctrlm_timestamp_t tx_window_start, unsigned char length, guchar *data, ctrlm_hal_rf4ce_data_read_t cb_data_read, void *cb_data_param, bool tx_indirect, bool single_channel, ctrlm_hal_rf4ce_cfm_data_t cb_confirm, void *cb_confirm_param) {
   THREAD_ID_VALIDATE();
   ctrlm_hal_rf4ce_req_data_params_t params = {0};

   params.controller_id       = controller_id;
   params.profile_id          = profile_id;
   params.vendor_id           = CTRLM_RF4CE_VENDOR_ID_COMCAST;
   params.flag_broadcast      = 0;
   params.flag_acknowledged   = 1;
   params.flag_single_channel = single_channel?1:0;
#if CTRLM_HAL_RF4CE_API_VERSION >= 14
   params.flag_indirect       = tx_indirect;
#endif
   params.tx_window_start     = tx_window_start;
   params.tx_window_duration  = CTRLM_RF4CE_CONST_RESPONSE_WAIT_TIME * 1000;
   params.length              = length;
   params.data                = data;
   params.cb_data_read        = cb_data_read;
   params.cb_data_param       = cb_data_param;
   params.cb_confirm          = cb_confirm;
   params.cb_confirm_param    = cb_confirm_param;

   return(hal_api_data_(params));
}

void ctrlm_obj_network_rf4ce_t::hal_init_complete() {
   LOG_INFO("%s: \n", __FUNCTION__);
   THREAD_ID_VALIDATE();

   // Reconcile pairing list with controller list read from DB
   ctrlm_hal_network_property_controller_list_t *list = NULL;
   ctrlm_hal_result_t result = property_get(CTRLM_HAL_NETWORK_PROPERTY_CONTROLLER_LIST, (void **)&list);

   LOG_INFO("%s: -----------------------\n", __FUNCTION__);
   LOG_INFO("%s: %u Bound Controllers\n", __FUNCTION__, controllers_.size());
   for(map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin(); it != controllers_.end(); it++) {
      ctrlm_controller_status_t status;
      it->second->rf4ce_controller_status(&status);
      LOG_INFO("%s: --- Controller Id %u ---\n", __FUNCTION__, it->first);
      ctrlm_print_controller_status(__FUNCTION__, &status);
   }
   LOG_INFO("%s: -----------------------\n", __FUNCTION__);

   if(result != CTRLM_HAL_RESULT_SUCCESS || list == NULL) {
      LOG_ERROR("%s: Unable to get controller list!\n", __FUNCTION__);
   } else {
      unsigned long index;
      LOG_INFO("%s: %lu Paired Controllers\n", __FUNCTION__, list->quantity);
      for(index = 0; index < list->quantity; index++) {
         LOG_INFO("%s: %u 0x%016llX\n", __FUNCTION__, list->controller_ids[index], list->ieee_addresses[index]);
      }

      // Delete controllers that are not found in the pairing table
      map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin();
      while(it != controllers_.end()) {
         bool                          found         = false;
         ctrlm_controller_id_t         controller_id = it->first;
         ctrlm_obj_controller_rf4ce_t *controller    = it->second;
         it++;
         LOG_INFO("%s: check controller %u 0x%016llX\n", __FUNCTION__, controller_id, controller->ieee_address_get());
         
         for(index = 0; index < list->quantity; index++) {
            if(list->ieee_addresses[index] == controller->ieee_address_get()) {
               found = true;
               if(list->controller_ids[index] != controller_id) {
                  LOG_WARN("%s: Controller with valid IEEE address but mismatched controller id! Reimporting...\n", __FUNCTION__);
                  // Remove old entry for this IEEE address
                  controller_remove(controller_id, true);
                  // Import controller
                  // Set import flag so we do not export default values
                  is_import_ = TRUE;
                  // Assign a controller id to the entry
                  controller_id = controller_id_assign();
                  // Create the controller object
                  controller_insert(controller_id, list->ieee_addresses[index], true);
                  // Import the controller's data from the HAL's nvm (We know this controller is validated)
                  controller_import(controller_id, list->ieee_addresses[index], true);
                  // We know that the controller was properly validated since it was in our DB
                  // Set import flag so future changes to controller are exported to the HAL
                  is_import_ = FALSE;
               }
               break;
            }
         }
         if(!found) {
            LOG_INFO("%s: removing bound controller %u\n", __FUNCTION__, controller_id);
            controller_remove(controller_id, true);
         } else {
            LOG_INFO("%s: keeping bound controller %u\n", __FUNCTION__, controller_id);
         }
      }

      for(index = 0; index < list->quantity; index++) {
         bool found = false;
         LOG_INFO("%s: check controller from HAL %u 0x%016llX\n", __FUNCTION__, list->controller_ids[index], list->ieee_addresses[index]);
         for(map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin(); it != controllers_.end(); it++) {
            if(list->ieee_addresses[index] == it->second->ieee_address_get()) {
               found = true;
               break;
            }
         }
         if(!found) {
            LOG_INFO("%s: Controller from HAL not found in ctrlm controller list! Checking if it was validated...\n", __FUNCTION__);
            // Set import flag so we do not export default values
            is_import_ = TRUE;
            // Assign a controller id to the entry
            ctrlm_controller_id_t controller_id = controller_id_assign();
            // Create the controller object
            controller_insert(controller_id, list->ieee_addresses[index], true);
            // Import the controller's data from the HAL's nvm (We are unsure if it is validated)
            controller_import(controller_id, list->ieee_addresses[index], false);
            // Set import flag so future changes to controller are exported to the HAL
            is_import_ = FALSE;
            // Check if remote was properly validated
            if(controllers_[controller_id]->import_check_validation()) {
               LOG_INFO("%s: Controller from HAL was bound. Possibly from rollback\n", __FUNCTION__);
               controllers_[controller_id]->validation_result_set(CTRLM_RCU_BINDING_TYPE_INTERACTIVE, CTRLM_RCU_VALIDATION_TYPE_INTERNAL,CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS);
            } else {
               LOG_WARN("%s: Controller from HAL wasn't properly validated! Removing controller...\n", __FUNCTION__);
               controller_unbind(controller_id, CTRLM_UNBIND_REASON_INVALID_VALIDATION);
            }
         }
      }

      // Get controller stats for the consolidated controllers
      for(map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin(); it != controllers_.end(); it++) {
         controller_stats_update(it->first);
      }

      // Free the memory associated with the request
      ctrlm_hal_free(list);
   }

   if(polling_methods_ & POLLING_METHODS_FLAG_MAC) {
      indirect_tx_interval_set();
   }
}

void ctrlm_obj_network_rf4ce_t::indirect_tx_interval_set() {
#if CTRLM_HAL_RF4CE_API_VERSION >= 14
   unsigned int indirect_timeout_msec = controller_polling_configuration_mac_[RF4CE_CONTROLLER_TYPE_XR15V2].time_interval * 3; // actual value is aligned to 15 msec
   ctrlm_hal_result_t property_set_result = property_set(CTRLM_HAL_NETWORK_PROPERTY_INDIRECT_TX_TIMEOUT, &indirect_timeout_msec);
   if (property_set_result != CTRLM_HAL_RESULT_SUCCESS) {
      LOG_ERROR("%s: Setting Indirect TX timeout returned %s.\n", __FUNCTION__, ctrlm_hal_result_str(property_set_result));
   } 
   // read back actual Indirect TX timeout value
   unsigned int actual_indirect_timeout_msec = 0;
   property_get(CTRLM_HAL_NETWORK_PROPERTY_INDIRECT_TX_TIMEOUT, (void**)&actual_indirect_timeout_msec);
   LOG_INFO("%s: Setting Indirect TX timeout to %u msec; Actual %u msec\n", __FUNCTION__, indirect_timeout_msec, actual_indirect_timeout_msec);
#endif
}

void ctrlm_obj_network_rf4ce_t::discovery_config_get(ctrlm_controller_discovery_config_t *config) {
   THREAD_ID_VALIDATE();
   if(config == NULL) {
      LOG_ERROR("%s: NULL config pointer.\n", __FUNCTION__);
   } else {
      config->enabled = discovery_config_normal_.enabled;
      config->require_line_of_sight = discovery_config_normal_.require_line_of_sight;
   }
}

bool ctrlm_obj_network_rf4ce_t::discovery_config_set(ctrlm_controller_discovery_config_t config) {
   THREAD_ID_VALIDATE();

   discovery_config_normal_.enabled = config.enabled;
   discovery_config_normal_.require_line_of_sight = config.require_line_of_sight;
   return(true);
}

void ctrlm_obj_network_rf4ce_t::controller_import(ctrlm_controller_id_t controller_id, unsigned long long ieee_address, bool validated) {
   ctrlm_hal_rf4ce_rib_data_import_params_t params;
   const char *                             attribute_name;
   unsigned char                            audio_profiles[CTRLM_HAL_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES] = {0};     // Needed for determining product name
   unsigned char                            voice_remote_audio_profiles[CTRLM_HAL_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES] = {0x01, 0x00}; // Needed for determining product name
   LOG_INFO("%s: controller id %u ieee address 0x%016llX\n", __FUNCTION__, controller_id, ieee_address);
   // Set binding type and validation type
   ctrlm_rcu_binding_type_t    binding_type    = CTRLM_RCU_BINDING_TYPE_INTERACTIVE;
   ctrlm_rcu_validation_type_t validation_type = CTRLM_RCU_VALIDATION_TYPE_INTERNAL;

   ctrlm_hal_network_property_controller_import_t controller_import;
   errno_t safec_rc = -1;
   int ind = -1;

   controller_import.controller_id = controller_id;
   controller_import.ieee_address  = ieee_address;
   controller_import.autobind      = 0;
   controller_import.time_binding  = 0;
   controller_import.time_last_key = 0;

   if(CTRLM_HAL_RESULT_SUCCESS != property_get(CTRLM_HAL_NETWORK_PROPERTY_CONTROLLER_IMPORT, (void **)&controller_import)) {
      LOG_ERROR("%s: Unable to import controller\n", __FUNCTION__);
      // Set validation result which will create the DB and store it
      controllers_[controller_id]->validation_result_set(binding_type, validation_type, ( validated ? CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS : CTRLM_RF4CE_RESULT_VALIDATION_PENDING ));
   } else {
      if(controller_import.autobind) {
         binding_type    = CTRLM_RCU_BINDING_TYPE_AUTOMATIC;
         validation_type = CTRLM_RCU_VALIDATION_TYPE_AUTOMATIC;
      }
      // Set validation result which will create the DB and store it
      controllers_[controller_id]->validation_result_set(binding_type, validation_type, ( validated ? CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS : CTRLM_RF4CE_RESULT_VALIDATION_PENDING ), controller_import.time_binding, controller_import.time_last_key);
   }

   params.ieee_address = ieee_address;
   if(hal_api_rib_data_import_ == NULL) {
      LOG_ERROR("%s: NULL HAL API\n", __FUNCTION__);
      return;
   }

   // Version irdb/hardware/software
   params.identifier = CTRLM_HAL_RF4CE_RIB_ATTR_ID_VERSIONING;
   params.index      = 0;
   params.length     = CTRLM_HAL_RF4CE_RIB_ATTR_LEN_VERSIONING;
   safec_rc = memset_s(params.data, sizeof(params.data), 0 , params.length);
   ERR_CHK(safec_rc);
   attribute_name = "Version Software";
   if(CTRLM_HAL_RESULT_SUCCESS != (*hal_api_rib_data_import_)(&params)) {
      LOG_ERROR("%s: Unable to import <%s>\n", __FUNCTION__, attribute_name);
   } else if(params.length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_WARN("%s: Invalid size <%u> importing <%s>\n", __FUNCTION__, params.length, attribute_name);
   } else {
      controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)params.identifier, params.index, params.length, params.data);
   }

   params.index      = 1;
   safec_rc = memset_s(params.data, sizeof(params.data), 0 , params.length);
   ERR_CHK(safec_rc);
   attribute_name = "Version Hardware";
   if(CTRLM_HAL_RESULT_SUCCESS != (*hal_api_rib_data_import_)(&params)) {
      LOG_ERROR("%s: Unable to import <%s>\n", __FUNCTION__, attribute_name);
   } else if(params.length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_WARN("%s: Invalid size <%u> importing <%s>\n", __FUNCTION__, params.length, attribute_name);
   } else {
      controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)params.identifier, params.index, params.length, params.data);
   }

   params.index      = 2;
   safec_rc = memset_s(params.data, sizeof(params.data), 0 , params.length);
   ERR_CHK(safec_rc);
   attribute_name = "Version IR DB";
   if(CTRLM_HAL_RESULT_SUCCESS != (*hal_api_rib_data_import_)(&params)) {
      LOG_ERROR("%s: Unable to import <%s>\n", __FUNCTION__, attribute_name);
   } else if(params.length != CTRLM_RF4CE_RIB_ATTR_LEN_VERSIONING) {
      LOG_WARN("%s: Invalid size <%u> importing <%s>\n", __FUNCTION__, params.length, attribute_name);
   } else {
      controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)params.identifier, params.index, params.length, params.data);
   }

   // Audio Profiles

   params.identifier = CTRLM_HAL_RF4CE_RIB_ATTR_ID_VOICE_CTRL_AUDIO_PROFILES;
   params.index      = 0;
   params.length     = CTRLM_HAL_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES;
   safec_rc = memset_s(params.data, sizeof(params.data), 0 , params.length);
   ERR_CHK(safec_rc);
   attribute_name = "Audio Profiles Controller";
   if(CTRLM_HAL_RESULT_SUCCESS != (*hal_api_rib_data_import_)(&params)) {
      LOG_ERROR("%s: Unable to import <%s>\n", __FUNCTION__, attribute_name);
   } else if(params.length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES) {
      LOG_WARN("%s: Invalid size <%u> importing <%s>\n", __FUNCTION__, params.length, attribute_name);
   } else {
      safec_rc = memcpy_s(audio_profiles, sizeof(audio_profiles), params.data, CTRLM_HAL_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES);
      ERR_CHK(safec_rc);
      controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)params.identifier, params.index, params.length, params.data);
   }

   // Product Name
   params.identifier = CTRLM_HAL_RF4CE_RIB_ATTR_ID_PRODUCT_NAME;
   params.index      = 0;
   params.length     = CTRLM_HAL_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME;
   safec_rc = memset_s(params.data, sizeof(params.data), 0 , params.length);
   ERR_CHK(safec_rc);
   attribute_name = "Product Name";
   version_hardware_t version_hardware = controllers_[controller_id]->version_hardware_get();

   //rf4ceMgr product name cannot be trusted so we will use the hw version to determine product name
   // Check audio profile to see if the remote is a voice remote
   safec_rc = memcmp_s(audio_profiles, sizeof(audio_profiles), voice_remote_audio_profiles, CTRLM_HAL_RF4CE_RIB_ATTR_LEN_VOICE_CTRL_AUDIO_PROFILES, &ind);
   ERR_CHK(safec_rc);
   if((safec_rc == EOK) && (ind == 0)) {
      // This is a voice remote according to rf4ceMgr
      if (is_xr11_hardware_version(version_hardware)) {
         safec_rc = strcpy_s((char *)params.data, sizeof(params.data), RF4CE_PRODUCT_NAME_XR11);
         ERR_CHK(safec_rc);
      } else if (is_xr15_hardware_version(version_hardware)) {
         safec_rc = strcpy_s((char *)params.data, sizeof(params.data), RF4CE_PRODUCT_NAME_XR15);
         ERR_CHK(safec_rc);
      } else { // Something is not right, fall back to XR5
         safec_rc = strcpy_s((char *)params.data, sizeof(params.data), RF4CE_PRODUCT_NAME_XR5);
         ERR_CHK(safec_rc);
      }
   } //All others will be treated as XR5
   else {
      safec_rc = strcpy_s((char *)params.data, sizeof(params.data), RF4CE_PRODUCT_NAME_XR5);
      ERR_CHK(safec_rc);
   }
   LOG_INFO("%s: For <%s>, using <%s>\n", __FUNCTION__, attribute_name, (char *)params.data);
   controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)params.identifier, params.index, params.length, params.data);

   // Battery Status
   params.identifier = CTRLM_HAL_RF4CE_RIB_ATTR_ID_BATTERY_STATUS;
   params.index      = 0;
   params.length     = CTRLM_HAL_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS;
   safec_rc = memset_s(params.data, sizeof(params.data), 0 , params.length);
   ERR_CHK(safec_rc);
   attribute_name = "Battery Status";
   if(CTRLM_HAL_RESULT_SUCCESS != (*hal_api_rib_data_import_)(&params)) {
      LOG_ERROR("%s: Unable to import <%s>\n", __FUNCTION__, attribute_name);
   } else if(params.length != CTRLM_RF4CE_RIB_ATTR_LEN_BATTERY_STATUS) {
      LOG_WARN("%s: Invalid size <%u> importing <%s>\n", __FUNCTION__, params.length, attribute_name);
   } else {
      controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)params.identifier, params.index, params.length, params.data);
   }

   // Voice Statistics
   params.identifier = CTRLM_HAL_RF4CE_RIB_ATTR_ID_VOICE_STATISTICS;
   params.index      = 0;
   params.length     = CTRLM_HAL_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS;
   safec_rc = memset_s(params.data, sizeof(params.data), 0 , params.length);
   ERR_CHK(safec_rc);
   attribute_name = "Voice Statistics";
   if(CTRLM_HAL_RESULT_SUCCESS != (*hal_api_rib_data_import_)(&params)) {
      LOG_ERROR("%s: Unable to import <%s>\n", __FUNCTION__, attribute_name);
   } else if(params.length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_STATISTICS) {
      LOG_WARN("%s: Invalid size <%u> importing <%s>\n", __FUNCTION__, params.length, attribute_name);
   } else {
      controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)params.identifier, params.index, params.length, params.data);
   }

   // Update Version Bootloader/Golden/Audio Data
   params.identifier = CTRLM_HAL_RF4CE_RIB_ATTR_ID_UPDATE_VERSIONING;
   params.index      = 1;
   params.length     = CTRLM_HAL_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING;
   safec_rc = memset_s(params.data, sizeof(params.data), 0 , params.length);
   ERR_CHK(safec_rc);
   attribute_name = "Version Bootloader";
   if(CTRLM_HAL_RESULT_SUCCESS != (*hal_api_rib_data_import_)(&params)) {
      LOG_ERROR("%s: Unable to import <%s>\n", __FUNCTION__, attribute_name);
   } else if(params.length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING) {
      LOG_WARN("%s: Invalid size <%u> importing <%s>\n", __FUNCTION__, params.length, attribute_name);
   } else {
      controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)params.identifier, params.index, params.length, params.data);
   }

   params.index      = 2;
   safec_rc = memset_s(params.data, sizeof(params.data), 0 , params.length);
   ERR_CHK(safec_rc);
   attribute_name = "Version Golden";
   if(CTRLM_HAL_RESULT_SUCCESS != (*hal_api_rib_data_import_)(&params)) {
      LOG_ERROR("%s: Unable to import <%s>\n", __FUNCTION__, attribute_name);
   } else if(params.length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING) {
      LOG_WARN("%s: Invalid size <%u> importing <%s>\n", __FUNCTION__, params.length, attribute_name);
   } else {
      controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)params.identifier, params.index, params.length, params.data);
   }

   params.index      = 0x10;
   safec_rc = memset_s(params.data, sizeof(params.data), 0 , params.length);
   ERR_CHK(safec_rc);
   attribute_name = "Version Audio Data";
   if(CTRLM_HAL_RESULT_SUCCESS != (*hal_api_rib_data_import_)(&params)) {
      LOG_ERROR("%s: Unable to import <%s>\n", __FUNCTION__, attribute_name);
   } else if(params.length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_VERSIONING) {
      LOG_WARN("%s: Invalid size <%u> importing <%s>\n", __FUNCTION__, params.length, attribute_name);
   } else {
      controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)params.identifier, params.index, params.length, params.data);
   }

   // IR DB Status
   params.identifier = CTRLM_HAL_RF4CE_RIB_ATTR_ID_IR_RF_DATABASE_STATUS;
   params.index      = 0;
   params.length     = CTRLM_HAL_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS;
   safec_rc = memset_s(params.data, sizeof(params.data), 0 , params.length);
   ERR_CHK(safec_rc);
   attribute_name = "IR RF Database Status";
   if(CTRLM_HAL_RESULT_SUCCESS != (*hal_api_rib_data_import_)(&params)) {
      LOG_ERROR("%s: Unable to import <%s>\n", __FUNCTION__, attribute_name);
   } else if(params.length != CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS) {
      LOG_WARN("%s: Invalid size <%u> importing <%s>\n", __FUNCTION__, params.length, attribute_name);
   } else {
      controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)params.identifier, params.index, params.length, params.data);
   }
}

void ctrlm_obj_network_rf4ce_t::controllers_load() {
   THREAD_ID_VALIDATE();
   vector<ctrlm_controller_id_t> controller_ids;
   ctrlm_network_id_t network_id = network_id_get();

   ctrlm_db_rf4ce_controllers_list(network_id, &controller_ids);

   for(vector<ctrlm_controller_id_t>::iterator it = controller_ids.begin(); it < controller_ids.end(); it++) {
      unsigned long long ieee_address = 0;
      ctrlm_db_rf4ce_read_ieee_address(network_id, *it, &ieee_address);
      controller_insert(*it, ieee_address, false);
   }
}

ctrlm_controller_id_t ctrlm_obj_network_rf4ce_t::controller_id_assign(void) {
   // Get the next available controller id
   for(ctrlm_controller_id_t index = 1; index < 255; index++) {
      if(!controller_exists(index)) {
         LOG_INFO("%s: controller id %u\n", __FUNCTION__, index);
         return(index);
      }
   }

   LOG_ERROR("%s: Unable to assign a controller id!\n", __FUNCTION__);
   return(0);
}

void ctrlm_obj_network_rf4ce_t::controller_insert(ctrlm_controller_id_t controller_id, unsigned long long ieee_address, bool db_create) {

   LOG_INFO("%s: controller id %u ieee address 0x%016llX\n", __FUNCTION__, controller_id, ieee_address);

   if(controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u already present.\n", __FUNCTION__, controller_id);
      return;
   }

   if(db_create) {
      controllers_[controller_id] = new ctrlm_obj_controller_rf4ce_t(controller_id, *this, ieee_address, CTRLM_RF4CE_RESULT_VALIDATION_PENDING, CTRLM_RCU_CONFIGURATION_RESULT_PENDING);
      // Create the DB only on validation success
   } else {
      controllers_[controller_id] = new ctrlm_obj_controller_rf4ce_t(controller_id, *this, ieee_address, CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS, CTRLM_RCU_CONFIGURATION_RESULT_SUCCESS);
      controllers_[controller_id]->db_load();
#ifdef XR15_704
      controllers_[controller_id]->set_reset();
#endif
      controllers_[controller_id]->update_polling_configurations();
   }
}

void ctrlm_obj_network_rf4ce_t::controller_user_string_set(ctrlm_controller_id_t controller_id, guchar *user_string) {
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u doesn't exist.\n", __FUNCTION__, controller_id);
      return;
   }
   controllers_[controller_id]->user_string_set(user_string);
}

void ctrlm_obj_network_rf4ce_t::controller_autobind_in_progress_set(ctrlm_controller_id_t controller_id, bool in_progress) {
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u doesn't exist.\n", __FUNCTION__, controller_id);
      return;
   }
   controllers_[controller_id]->autobind_in_progress_set(in_progress);
}

bool ctrlm_obj_network_rf4ce_t::controller_autobind_in_progress_get(ctrlm_controller_id_t controller_id) {
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u doesn't exist.\n", __FUNCTION__, controller_id);
      return(false);
   }
   return(controllers_[controller_id]->autobind_in_progress_get());
}

void ctrlm_obj_network_rf4ce_t::controller_binding_button_in_progress_set(ctrlm_controller_id_t controller_id, bool in_progress) {
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u doesn't exist.\n", __FUNCTION__, controller_id);
      return;
   }
   controllers_[controller_id]->binding_button_in_progress_set(in_progress);
}

bool ctrlm_obj_network_rf4ce_t::controller_binding_button_in_progress_get(ctrlm_controller_id_t controller_id) {
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u doesn't exist.\n", __FUNCTION__, controller_id);
      return(false);
   }
   return(controllers_[controller_id]->binding_button_in_progress_get());
}

void ctrlm_obj_network_rf4ce_t::controller_screen_bind_in_progress_set(ctrlm_controller_id_t controller_id, bool in_progress) {
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u doesn't exist.\n", __FUNCTION__, controller_id);
      return;
   }
   controllers_[controller_id]->screen_bind_in_progress_set(in_progress);
}

bool ctrlm_obj_network_rf4ce_t::controller_screen_bind_in_progress_get(ctrlm_controller_id_t controller_id) {
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u doesn't exist.\n", __FUNCTION__, controller_id);
      return(false);
   }
   return(controllers_[controller_id]->screen_bind_in_progress_get());
}

void ctrlm_obj_network_rf4ce_t::controller_stats_update(ctrlm_controller_id_t controller_id) {
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u doesn't exist.\n", __FUNCTION__, controller_id);
      return;
   }
   controllers_[controller_id]->stats_update();
}

void ctrlm_obj_network_rf4ce_t::controller_remove(ctrlm_controller_id_t controller_id, bool db_destroy) {

   LOG_INFO("%s: controller id %u\n", __FUNCTION__, controller_id);

   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u NOT present.\n", __FUNCTION__, controller_id);
      return;
   }

   if(db_destroy) {
      controllers_[controller_id]->db_destroy();
   }

   delete controllers_[controller_id];
   controllers_.erase(controller_id);
}

void ctrlm_obj_network_rf4ce_t::controller_backup(ctrlm_controller_id_t controller_id, void *data) {
   LOG_INFO("%s: controller id %u\n", __FUNCTION__, controller_id);

   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u NOT present.\n", __FUNCTION__, controller_id);
      return;
   }

   controllers_[controller_id]->backup_pairing(data);
}

void ctrlm_obj_network_rf4ce_t::controller_restore(ctrlm_controller_id_t controller_id) {
   LOG_INFO("%s: controller id %u\n", __FUNCTION__, controller_id);

   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u NOT present.\n", __FUNCTION__, controller_id);
      return;
   }

   controllers_[controller_id]->restore_pairing();
}

void ctrlm_obj_network_rf4ce_t::controller_list_get(vector<ctrlm_controller_id_t>& list) const {
   THREAD_ID_VALIDATE();
   if(!list.empty()) {
      LOG_WARN("%s: Invalid list.\n", __FUNCTION__);
      return;
   }
   vector<ctrlm_controller_id_t>::iterator it_vector = list.begin();

   map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::const_iterator it_map;
   for(it_map = controllers_.begin(); it_map != controllers_.end(); it_map++) {
      if(it_map->second->is_bound()) {
         it_vector = list.insert(it_vector, it_map->first);
      }
      else {
         LOG_WARN("%s: Controller %u NOT bound.\n", __FUNCTION__, it_map->first);
      }
   }
}

void ctrlm_obj_network_rf4ce_t::pan_id_get(guint16 *pan_id) {
   THREAD_ID_VALIDATE();
   if(pan_id == NULL) {
      LOG_WARN("%s: NULL pan_id.\n", __FUNCTION__);
      return;
   }
   *pan_id = pan_id_;
}

void ctrlm_obj_network_rf4ce_t::ieee_address_get(unsigned long long *ieee_address) {
   THREAD_ID_VALIDATE();
   if(ieee_address == NULL) {
      LOG_WARN("%s: NULL ieee_address.\n", __FUNCTION__);
      return;
   }
   *ieee_address = ieee_address_;
}

void ctrlm_obj_network_rf4ce_t::short_address_get(ctrlm_hal_rf4ce_short_address_t *short_address) {
   THREAD_ID_VALIDATE();
   if(short_address == NULL) {
      LOG_WARN("%s: NULL short_address.\n", __FUNCTION__);
      return;
   }
   *short_address = short_address_;
}

const char * ctrlm_obj_network_rf4ce_t::chipset_get(void) {
   THREAD_ID_VALIDATE();
   return(chipset_.c_str());
}

void ctrlm_obj_network_rf4ce_t::rf_channel_info_get(ctrlm_rf4ce_rf_channel_info_t *rf_channel_info) {
   THREAD_ID_VALIDATE();
   if(rf_channel_info == NULL) {
      LOG_WARN("%s: NULL rf_channel_info.\n", __FUNCTION__);
      return;
   }

   ctrlm_hal_network_property_network_stats_t network_stats;
   ctrlm_hal_result_t result;

   if(is_audio_stream_in_progress() && network_stats_is_cached){ // Read from cache if voice session in progress
      network_stats.rf_channel = network_stats_cache.rf_channel;
      network_stats.rf_quality = network_stats_cache.rf_quality;
      result = CTRLM_HAL_RESULT_SUCCESS;
    }else{
      result = property_get(CTRLM_HAL_NETWORK_PROPERTY_NETWORK_STATS, (void **)&network_stats);

      if(result == CTRLM_HAL_RESULT_SUCCESS) { // Update cache on successful HAL call
            network_stats_cache.rf_channel = network_stats.rf_channel;
            network_stats_cache.rf_quality = network_stats.rf_quality;
            network_stats_is_cached = TRUE;
       }
    }

   if(result != CTRLM_HAL_RESULT_SUCCESS) {
      errno_t safec_rc = memset_s(rf_channel_info, sizeof(ctrlm_rf4ce_rf_channel_info_t), 0 , sizeof(ctrlm_rf4ce_rf_channel_info_t));
      ERR_CHK(safec_rc);
   } else {
      rf_channel_info->rf_channel_number  = network_stats.rf_channel;
      rf_channel_info->rf_channel_quality = network_stats.rf_quality;
   }
}

bool ctrlm_obj_network_rf4ce_t::binding_config_set(ctrlm_controller_bind_config_t conf) {
   THREAD_ID_VALIDATE();
   bool ret = false;
   switch(conf.mode) {
      case CTRLM_PAIRING_MODE_ONE_TOUCH_AUTO_BIND: {
         uint8_t threshold_pass = conf.data.autobind.pass_threshold;
         uint8_t threshold_fail = conf.data.autobind.fail_threshold;
         bool    enable         = conf.data.autobind.enable;
         if(threshold_pass < CTRLM_AUTOBIND_THRESHOLD_MIN || threshold_pass > CTRLM_AUTOBIND_THRESHOLD_MAX) {
            LOG_WARN("%s: Invalid threshold pair.\n", __FUNCTION__);
         } else if(threshold_fail < CTRLM_AUTOBIND_THRESHOLD_MIN || threshold_fail > CTRLM_AUTOBIND_THRESHOLD_MAX) {
            LOG_WARN("%s: Invalid threshold fail.\n", __FUNCTION__);
         } else {
            autobind_config_normal_.enabled        = enable;
            autobind_config_normal_.threshold_pass = threshold_pass;
            autobind_config_normal_.threshold_fail = threshold_fail;
            autobind_config_normal_.octet          = ((threshold_fail << 3) | threshold_pass);
            LOG_INFO("%s: Threshold Pass %u fail %u.\n", __FUNCTION__, threshold_pass, threshold_fail);
            ret = true;
         }
         break;
      }
      default: {
         break;
      }
   }
   return ret;
}

void ctrlm_obj_network_rf4ce_t::factory_reset(void) {
   THREAD_ID_VALIDATE();

   LOG_INFO("%s: Unbind controllers\n", __FUNCTION__);
   // Unbind all the controllers
   map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin();
   while(it != controllers_.end()) {
      // call to unbind removes the element which invalidates the iterator
      auto temp = it++;
      controller_unbind(temp->first, CTRLM_UNBIND_REASON_FACTORY_RESET);
   }

   LOG_INFO("%s: Reset HAL persistent data\n", __FUNCTION__);
   property_set(CTRLM_HAL_NETWORK_PROPERTY_FACTORY_RESET, NULL);

   LOG_INFO("%s: Reset Control Manager persistent data\n", __FUNCTION__);
   // Delete control manager persistent data
   // TODO

}

bool ctrlm_obj_network_rf4ce_t::controller_exists(ctrlm_controller_id_t controller_id) {
   THREAD_ID_VALIDATE();
   return(controllers_.count(controller_id));
}

bool ctrlm_obj_network_rf4ce_t::controller_is_bound(ctrlm_controller_id_t controller_id) {
   THREAD_ID_VALIDATE();
   if(controller_exists(controller_id)) {
      return(controllers_[controller_id]->is_bound());
   }
   return(false);
}

ctrlm_controller_id_t ctrlm_obj_network_rf4ce_t::controller_id_get_by_ieee(unsigned long long ieee_address) {
   for(map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin(); it != controllers_.end(); it++) {
      if(it->second->ieee_address_get() == ieee_address) {
         return(it->first);
      }
   }
   return(0);
}

ctrlm_controller_id_t ctrlm_obj_network_rf4ce_t::controller_id_get_last_recently_used(void) {
   ctrlm_controller_id_t controller_id = CTRLM_HAL_CONTROLLER_ID_INVALID;
   time_t time_last_recent, time_controller;
   time_last_recent = time(NULL);

   for(map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin(); it != controllers_.end(); it++) {
      it->second->time_last_key_get(&time_controller);
      if(time_last_recent > time_controller) {
         time_last_recent = time_controller;
         controller_id    = it->first;
      }
   }
   return(controller_id);
}

void ctrlm_obj_network_rf4ce_t::process_event_key(ctrlm_controller_id_t controller_id, ctrlm_key_status_t key_status, ctrlm_key_code_t key_code) {
   THREAD_ID_VALIDATE();
   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Controller %u NOT present.\n", __FUNCTION__, controller_id);
      return;
   }
   // Inform the controller about the key event
   controllers_[controller_id]->process_event_key(key_status, key_code);
}

void ctrlm_obj_network_rf4ce_t::req_process_rib_set(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_rib_t *dqm = (ctrlm_main_queue_msg_rib_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_rib_t));
   g_assert(dqm->cmd_result);

   *dqm->cmd_result = CTRLM_RIB_REQUEST_SUCCESS;

   ctrlm_controller_id_t controller_id = dqm->controller_id;
   if(controller_id == CTRLM_MAIN_CONTROLLER_ID_ALL) { // set to all controllers
      if(is_attribute_network_wide((ctrlm_rf4ce_rib_attr_id_t)dqm->attribute_id)) { // Some attributes are network wide
         gboolean rib_entries_updated = false;
         if(!rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)dqm->attribute_id, dqm->attribute_index, dqm->length, dqm->data, &rib_entries_updated)) {
            *dqm->cmd_result = CTRLM_RIB_REQUEST_ERROR;
         } else if(rib_entries_updated) {
            guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_RIB_ENTRIES_UPDATED];
            data[0] = RIB_ENTRIES_UPDATED_TRUE;
            for(map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin(); it != controllers_.end(); it++) {
               it->second->rf4ce_rib_set_target(CTRLM_RF4CE_RIB_ATTR_ID_RIB_ENTRIES_UPDATED, 0, CTRLM_RF4CE_RIB_ATTR_LEN_RIB_ENTRIES_UPDATED, data);
            }
         }
      } else {
         for(map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin(); it != controllers_.end(); it++) {
            it->second->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)dqm->attribute_id, dqm->attribute_index, dqm->length, dqm->data);
         }
      }
   } else { // set for specific controller
      if(!controller_exists(controller_id)) {
         LOG_WARN("%s: Controller %u NOT present.\n", __FUNCTION__, controller_id);
         *dqm->cmd_result = CTRLM_RIB_REQUEST_ERROR;
      } else {
         controllers_[controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)dqm->attribute_id, dqm->attribute_index, dqm->length, dqm->data);
      }
   }
   ctrlm_obj_network_t::req_process_rib_set(data, size);
}

gboolean ctrlm_obj_network_rf4ce_t::is_attribute_network_wide(ctrlm_rf4ce_rib_attr_id_t attribute_id) {
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   return(false);
   #else
   switch(attribute_id) {
      case CTRLM_RF4CE_RIB_ATTR_ID_SHORT_RF_RETRY_PERIOD:     return(true);
      case CTRLM_RF4CE_RIB_ATTR_ID_MAXIMUM_UTTERANCE_LENGTH:  return(true);
      case CTRLM_RF4CE_RIB_ATTR_ID_MAX_VOICE_DATA_RETRY:      return(true);
      case CTRLM_RF4CE_RIB_ATTR_ID_MAX_VOICE_CSMA_BACKOFF:    return(true);
      case CTRLM_RF4CE_RIB_ATTR_ID_MIN_VOICE_DATA_BACKOFF:    return(true);
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_TARG_AUDIO_PROFILES: return(true);
      case CTRLM_RF4CE_RIB_ATTR_ID_RIB_UPDATE_CHECK_INTERVAL: return(true);
      case CTRLM_RF4CE_RIB_ATTR_ID_UPDATE_POLLING_PERIOD:     return(true);
      case CTRLM_RF4CE_RIB_ATTR_ID_DATA_REQUEST_WAIT_TIME:    return(true);
      case CTRLM_RF4CE_RIB_ATTR_ID_VALIDATION_CONFIGURATION:  return(true);
      default: return(false);
   }
   #endif
}

gboolean ctrlm_obj_network_rf4ce_t::rf4ce_rib_set_target(ctrlm_rf4ce_rib_attr_id_t attribute_id, guchar index, guchar length, guchar *data, gboolean *rib_entries_updated) {
   #ifdef CONTROLLER_SPECIFIC_NETWORK_ATTRIBUTES
   return(false);
   #else
   switch(attribute_id) {
      case CTRLM_RF4CE_RIB_ATTR_ID_SHORT_RF_RETRY_PERIOD: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_SHORT_RF_RETRY_PERIOD || index > 0) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         short_rf_retry_period_ = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | (data[0]);
         LOG_INFO("%s: %u us\n", __FUNCTION__, short_rf_retry_period_);
         return(true);
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MAXIMUM_UTTERANCE_LENGTH: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAXIMUM_UTTERANCE_LENGTH || index > 0) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         utterance_duration_max_ = (data[1] << 8) | (data[0]);
         return(true);
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MAX_VOICE_DATA_RETRY: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_DATA_RETRY || index > 0) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         if(voice_data_retry_max_ != data[0]) {
            *rib_entries_updated = true;
         }

         voice_data_retry_max_ = data[0];
         LOG_INFO("%s: %u attempts\n", __FUNCTION__, voice_data_retry_max_);
         return(true);
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MAX_VOICE_CSMA_BACKOFF: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MAX_VOICE_CSMA_BACKOFF || index > 0) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         guchar voice_csma_backoff_max = data[0];
         if(voice_csma_backoff_max > 5) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         if(voice_csma_backoff_max_ != voice_csma_backoff_max) {
            *rib_entries_updated = true;
         }
         voice_csma_backoff_max_ = voice_csma_backoff_max;

         LOG_INFO("%s: %u backoffs\n", __FUNCTION__, voice_csma_backoff_max_);
         return(true);
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_MIN_VOICE_DATA_BACKOFF: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_MIN_VOICE_DATA_BACKOFF || index > 0) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         guchar voice_data_backoff_exp_min = data[0];
         if(voice_data_backoff_exp_min_ != voice_data_backoff_exp_min) {
            *rib_entries_updated = true;
         }
         voice_data_backoff_exp_min_ = voice_data_backoff_exp_min;

         LOG_INFO("%s: backoff exponent %u\n", __FUNCTION__, voice_data_backoff_exp_min_);
         return(true);
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_TARG_AUDIO_PROFILES: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_TARG_AUDIO_PROFILES || index > 0) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         } else {
            LOG_ERROR("%s: VOICE TARG AUDIO PROFILES - NOT SUPPORTED\n", __FUNCTION__);
            return(false);
         }
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_RIB_UPDATE_CHECK_INTERVAL: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_RIB_UPDATE_CHECK_INTERVAL || index > 0) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         guint16 rib_update_check_interval = (data[1] << 8) | data[0];
         if(rib_update_check_interval > 8760) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         if(rib_update_check_interval_ != rib_update_check_interval) {
            *rib_entries_updated = true;
         }
         rib_update_check_interval_ = rib_update_check_interval;

         LOG_INFO("%s: %u hours\n", __FUNCTION__, rib_update_check_interval_);
         return(true);
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_UPDATE_POLLING_PERIOD: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_UPDATE_POLLING_PERIOD || index > 0) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         update_polling_period_ = (data[1] << 8) | (data[0]);

         LOG_INFO("%s: %u hours\n", __FUNCTION__, update_polling_period_);
         return(true);
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_DATA_REQUEST_WAIT_TIME: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DATA_REQUEST_WAIT_TIME || index > 0) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         data_request_wait_time_ = (data[1] << 8) | (data[0]);

         LOG_INFO("%s: %u ms\n", __FUNCTION__, data_request_wait_time_);
         return(true);
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VALIDATION_CONFIGURATION: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VALIDATION_CONFIGURATION || index > 0) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         auto_check_validation_period_ = (data[1] << 8) | (data[0]);
         link_lost_wait_time_          = (data[3] << 8) | (data[2]);

         LOG_INFO("%s: auto check validation period %u ms\n", __FUNCTION__, auto_check_validation_period_);
         LOG_INFO("%s: link lost wait time %u ms\n", __FUNCTION__, link_lost_wait_time_);
         return(true);
      }
      case CTRLM_RF4CE_RIB_ATTR_ID_VOICE_COMMAND_ENCRYPTION: {
         if(length != CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_ENCRYPTION || index > 0) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         voice_command_encryption_t voice_command_encryption = (voice_command_encryption_t)data[0];

         if(voice_command_encryption > VOICE_COMMAND_ENCRYPTION_DEFAULT) {
            LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
            return(false);
         }
         if(voice_command_encryption_ != voice_command_encryption) {
            *rib_entries_updated = true;
         }
         voice_command_encryption_ = voice_command_encryption;
         LOG_INFO("%s: <%s>\n", __FUNCTION__, ctrlm_rf4ce_voice_command_encryption_str(voice_command_encryption));
         return(true);
      }
      default: break;
   }
   return(false);
   #endif
}

void ctrlm_obj_network_rf4ce_t::req_process_rib_get(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_rib_t *dqm = (ctrlm_main_queue_msg_rib_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_rib_t));
   g_assert(dqm->cmd_result);

   *dqm->cmd_result = CTRLM_RIB_REQUEST_SUCCESS;

   ctrlm_controller_id_t controller_id = dqm->controller_id;
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u NOT present.\n", __FUNCTION__, controller_id);
      *dqm->cmd_result = CTRLM_RIB_REQUEST_ERROR;
   }
   controllers_[controller_id]->rf4ce_rib_get_target((ctrlm_rf4ce_rib_attr_id_t)dqm->attribute_id, dqm->attribute_index, dqm->length, dqm->length_out, dqm->data);

   ctrlm_obj_network_t::req_process_rib_get(data, size);
}

void ctrlm_obj_network_rf4ce_t::req_process_controller_status(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_controller_status_t *dqm = (ctrlm_main_queue_msg_controller_status_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_controller_status_t));
   g_assert(dqm->cmd_result);

   ctrlm_controller_id_t controller_id = dqm->controller_id;
   if(!controller_exists(controller_id)) {
      LOG_WARN("%s: Controller %u NOT present.\n", __FUNCTION__, controller_id);
      *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
   } else {
      controllers_[controller_id]->rf4ce_controller_status(dqm->status);
      *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_SUCCESS;
   }

   ctrlm_obj_network_t::req_process_controller_status(data, size);
}

void ctrlm_obj_network_rf4ce_t::req_process_controller_product_name(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_product_name_t *dqm = (ctrlm_main_queue_msg_product_name_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_product_name_t));
   g_assert(dqm->cmd_result);
   g_assert(dqm->product_name);

   if(controller_exists(dqm->controller_id)) {
      errno_t safec_rc = strcpy_s(dqm->product_name, CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME, controllers_[dqm->controller_id]->product_name_get());
      ERR_CHK(safec_rc);
      *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
   } else {
      LOG_WARN("%s: controller doesn't exist\n", __FUNCTION__);
      *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_ERROR;
   }

   ctrlm_obj_network_t::req_process_controller_product_name(data, size);
}

void ctrlm_obj_network_rf4ce_t::req_process_controller_link_key(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_controller_link_key_t *dqm = (ctrlm_main_queue_msg_controller_link_key_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_controller_link_key_t));
   g_assert(dqm->cmd_result);

   ctrlm_hal_network_property_encryption_key_t property = {0};

   if(!controller_exists(dqm->controller_id)) {
      LOG_WARN("%s: Controller %u NOT present.\n", __FUNCTION__, dqm->controller_id);
      *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
      ctrlm_obj_network_t::req_process_controller_link_key(data, size);
      return;
   }

   LOG_INFO("%s: Getting Link Key for Controller %u\n", __FUNCTION__, dqm->controller_id);

   // Get Link key
   property.controller_id = dqm->controller_id;
   if(CTRLM_HAL_RESULT_SUCCESS != property_get(CTRLM_HAL_NETWORK_PROPERTY_ENCRYPTION_KEY, (void **)&property)) {
      LOG_ERROR("%s: Failed to get Link Key from HAL\n", __FUNCTION__);
      *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
      ctrlm_obj_network_t::req_process_controller_link_key(data, size);
      return;
   }

   errno_t safec_rc = memcpy_s(dqm->link_key, CTRLM_HAL_NETWORK_AES128_KEY_SIZE, property.aes128_key, CTRLM_HAL_NETWORK_AES128_KEY_SIZE);
   ERR_CHK(safec_rc);
   *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_SUCCESS;
   ctrlm_obj_network_t::req_process_controller_link_key(data, size);
}

ctrlm_rib_request_cmd_result_t ctrlm_obj_network_rf4ce_t::req_process_rib_export(ctrlm_controller_id_t controller_id, ctrlm_hal_rf4ce_rib_attr_id_t identifier, unsigned char index, unsigned char length, unsigned char *data) {
   THREAD_ID_VALIDATE();
   ctrlm_rib_request_cmd_result_t     ret = CTRLM_RIB_REQUEST_SUCCESS;
   ctrlm_hal_rf4ce_rib_data_export_params_t params = {0};

   if(NULL == hal_api_rib_data_export_) {
      LOG_WARN("%s: data export api is null\n", __FUNCTION__);
      ret = CTRLM_RIB_REQUEST_ERROR;
   }
   else if(TRUE == is_import_) {
      LOG_INFO("%s: Currently importing a controller from HAL, do not export\n", __FUNCTION__);
   }
   else {
      params.controller_id = controller_id;
      params.identifier    = identifier;
      params.index         = index;
      params.length        = length;
      errno_t safec_rc = memcpy_s(&params.data, sizeof(params.data), data, length);
      ERR_CHK(safec_rc);

      if(CTRLM_HAL_RESULT_SUCCESS != (*hal_api_rib_data_export_)(&params)) {
         ret = CTRLM_RIB_REQUEST_ERROR;
      }
   }

   return ret;
}

gboolean ctrlm_obj_network_rf4ce_t::is_xr11_hardware_version(version_hardware_t version_hardware) {
   // Compare manufacturer
   if(version_hardware.manufacturer != RF4CE_XR11_VERSION_HARDWARE_MANUFACTURER) {
      return(false);
   }
   // Compare model
   if(version_hardware.model != RF4CE_XR11_VERSION_HARDWARE_MODEL) {
      return(false);
   }
   // Compare hw revision
   if(version_hardware.hw_revision < RF4CE_XR11_VERSION_HARDWARE_REVISION_MIN) {
      return(false);
   }
   return(true);
}

gboolean ctrlm_obj_network_rf4ce_t::is_xr15_hardware_version(version_hardware_t version_hardware) {
   // Compare manufacturer
   if(version_hardware.manufacturer != RF4CE_XR15_VERSION_HARDWARE_MANUFACTURER) {
      return(false);
   }
   // Compare model
   if((version_hardware.model != RF4CE_XR15_VERSION_HARDWARE_MODEL_REV_0) && (version_hardware.model != RF4CE_XR15_VERSION_HARDWARE_MODEL_REV_3)) {
      return(false);
   }
   // Compare hw revision
   if(version_hardware.hw_revision < RF4CE_XR15_VERSION_HARDWARE_REVISION_MIN) {
      return(false);
   }
   return(true);
}

void ctrlm_obj_network_rf4ce_t::recovery_set(ctrlm_recovery_type_t recovery) {
   THREAD_ID_VALIDATE();
   recovery_ = recovery;
}

ctrlm_recovery_type_t ctrlm_obj_network_rf4ce_t::recovery_get() {
   THREAD_ID_VALIDATE();
   return recovery_;
}

bool ctrlm_obj_network_rf4ce_t::backup_hal_nvm() {
   THREAD_ID_VALIDATE();
   LOG_INFO("%s: Backing up HAL NVM data to \"%s\"\n", __FUNCTION__, HAL_NVM_BACKUP);
   
   GFile    *g_file   = g_file_new_for_path(HAL_NVM_BACKUP);
   char     *contents = NULL;
   gsize     length   = 0;
   GError   *error    = NULL;

   if(nvm_backup_data_) {
   // Compare current NVM data with backup
      if(FALSE == g_file_load_contents(g_file, NULL, &contents, &length, NULL, &error)) {
         if( (error != NULL) && (G_FILE_ERROR_NOENT == error->code)) {
            LOG_INFO("%s: HAL NVM backup not found for comparison\n", __FUNCTION__);
         } else {
            LOG_ERROR("%s: HAL NVM exists but failed to get contents\n", __FUNCTION__);
         }   //CID:85557 - Reverse_inull
         if(error) {
            g_error_free(error);
            error = NULL;
         }
      } else {
         if(length == nvm_backup_len_) {
            if(contents && !memcmp(contents, nvm_backup_data_, nvm_backup_len_)) {
               LOG_INFO("%s: Backup of HAL NVM matches current NVM. No need to backup.\n", __FUNCTION__);
               g_free(contents);
               g_object_unref(g_file);
               return(TRUE);
            }
         }
         g_free(contents);
      }

   // Backup NVM is different from current, or doesn't exist. Dump to file.
      if(FALSE == g_file_replace_contents(g_file, (char *)nvm_backup_data_, nvm_backup_len_, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error)) {
         LOG_ERROR("%s: Failed to make backup of HAL NVM due to %s\n", __FUNCTION__, (error != NULL ? error->message : "unknown reason"));
         if(error) {
            g_error_free(error);
         }
      g_file_delete(g_file, NULL, NULL); // Just in case partially written or something.. Sanitity
      g_object_unref(g_file);
      return(FALSE);
   } else {
      LOG_INFO("%s: Backup of HAL NVM successful\n", __FUNCTION__);
      // Free backup data
      if(nvm_backup_data_) {
         free(nvm_backup_data_);
      }
      nvm_backup_len_ = 0;
   }
}

g_object_unref(g_file);


return(TRUE);
}

unsigned int ctrlm_obj_network_rf4ce_t::restore_hal_nvm(unsigned char **nvm_backup_data) {
   LOG_INFO("%s: Restoring HAL NVM from \"%s\"", __FUNCTION__, HAL_NVM_BACKUP);

   // Initialize return
   unsigned int   ret = 0;
   GFile    *g_file   = g_file_new_for_path(HAL_NVM_BACKUP);
   guchar   *contents = NULL;
   GError   *error    = NULL;

   if(FALSE == g_file_load_contents(g_file, NULL, (char **)&contents, &ret, NULL, &error)) {
      LOG_INFO("%s: Could not open HAL NVM backup due to %s\n", __FUNCTION__, (error != NULL ? error->message : "unknown reason"));
      if(error) {
         g_error_free(error);
      }
      g_object_unref(g_file);
      ret = 0;
      return ret;
   }

   if(!contents) {
      LOG_ERROR("%s: Contents of backup NVM file is NULL\n", __FUNCTION__);
      g_object_unref(g_file);
      ret = 0;
      return ret;
   }

   *nvm_backup_data = contents;
   g_object_unref(g_file);

   return ret;
}

void ctrlm_obj_network_rf4ce_t::disable_hal_calls() {
    LOG_INFO("%s: from ctrlm_obj_network_rf4ce_t\n", __FUNCTION__);
    ctrlm_obj_network_t::disable_hal_calls();
    hal_api_pair_                   = NULL;
    hal_api_unpair_                 = NULL;
    hal_api_data_                   = NULL;
    hal_api_rib_data_import_        = NULL;
    hal_api_rib_data_export_        = NULL;
}

// Bastille 37 function implementations

static gboolean blackout_timeout(gpointer data) {
   LOG_INFO("%s\n", __FUNCTION__);
   ctrlm_obj_network_rf4ce_t *rf4ce_net = (ctrlm_obj_network_rf4ce_t *)data;
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::blackout_reset, NULL, 0, rf4ce_net);
   rf4ce_net->blackout_tag_reset();
   return(FALSE);
}

void ctrlm_obj_network_rf4ce_t::blackout_bind_success() {
   THREAD_ID_VALIDATE();
   LOG_INFO("%s\n", __FUNCTION__);
   blackout_.pairing_fail_count      = 0;
   blackout_.blackout_time_increment = 1;
}

void ctrlm_obj_network_rf4ce_t::blackout_bind_fail() {
   THREAD_ID_VALIDATE();
   LOG_INFO("%s\n", __FUNCTION__);
   blackout_.pairing_fail_count += 1;
   // Check against failed pairing threshold
   if(blackout_.pairing_fail_count < blackout_.pairing_fail_threshold) {
      LOG_INFO("%s: Current Count <%u>, Threshold <%u>\n", __FUNCTION__, blackout_.pairing_fail_count, blackout_.pairing_fail_threshold);
      return;
   }
   // Surpassed threshold, start blackout
   if(FALSE == blackout_.is_blackout) {
      blackout_.blackout_count += 1;
      blackout_.is_blackout     = TRUE;
      blackout_.blackout_tag    = ctrlm_timeout_create(blackout_.blackout_time_increment * blackout_.blackout_time * 1000, blackout_timeout, this); // TODO: Implement blackout_timeout
   }
}

void ctrlm_obj_network_rf4ce_t::blackout_reset(void *data, int size) {
   THREAD_ID_VALIDATE();
   blackout_.pairing_fail_count = 0;
   // Check if reboot is nessessary
   if(blackout_.blackout_count >= blackout_.blackout_reboot_threshold) {
      LOG_WARN("%s: blackout_reboot_threshold has been reached.. System reboot required to pair new remotes..\n", __FUNCTION__);
      return;
   }
   // Increment blackout time for next blackout
   blackout_.blackout_time_increment += 1;
   blackout_.is_blackout              = FALSE;
   LOG_INFO("%s: New time interval for blackout is %u seconds\n", __FUNCTION__, blackout_.blackout_time * blackout_.blackout_time_increment);
}

gboolean ctrlm_obj_network_rf4ce_t::blackout_settings_get_rfc() {
   gchar      *contents  = NULL;
   gsize       len       = 0;
   GError     *err       = NULL;
   gchar      *result    = NULL;
   errno_t safec_rc = -1;
   int ind = -1;

   if(FALSE == g_file_get_contents(CTRLM_RF4CE_BLACKOUT_RFC_FILE, &contents, &len, &err)) {
      if(err != NULL) { 
         LOG_INFO("%s: Failed to open RFC file <%s>\n", __FUNCTION__, err->message);
         g_error_free(err);
      }   //CID:127763 - Forward null
      return FALSE;
   }

   result = ctrlm_do_regex((char *)CTRLM_RF4CE_REGEX_BLACKOUT_ENABLE, contents);
   if(NULL != result) {
      safec_rc = strcmp_s("false", strlen("false"), result, &ind);
      ERR_CHK(safec_rc);

      if((safec_rc == EOK) && (!ind)) {
         blackout_.is_blackout_enabled = false;
      } else {
         if(safec_rc != EOK) {
            LOG_WARN("%s: Defaulting to Blackout Enabled due to error in safec %s\n,", __FUNCTION__, strerror(safec_rc));
         }
         blackout_.is_blackout_enabled = true;
      }
      LOG_INFO("%s: Blackout Enabled <%s> from RFC\n", __FUNCTION__, blackout_.is_blackout_enabled ? "YES" : "NO");
      g_free(result);
      result = NULL;
   }

   result = ctrlm_do_regex((char *)CTRLM_RF4CE_REGEX_BLACKOUT_FAIL_THRESHOLD , contents);
   if(NULL != result) {
      guint temp = atoi(result);
      if(temp > 1) {
         blackout_.pairing_fail_threshold = temp;
         LOG_INFO("%s: Blackout Fail Threshold <%u> from RFC\n", __FUNCTION__, blackout_.pairing_fail_threshold);
      }
      g_free(result);
      result = NULL;
   }

   result = ctrlm_do_regex((char *)CTRLM_RF4CE_REGEX_BLACKOUT_REBOOT_THRESHOLD , contents);
   if(NULL != result) {
      guint temp = atoi(result);
      if(temp > 1) {
         blackout_.blackout_reboot_threshold = temp;
         LOG_INFO("%s: Blackout Reboot Threshold <%u> from RFC\n", __FUNCTION__, blackout_.blackout_reboot_threshold);
      }
      g_free(result);
      result = NULL;
   }

   result = ctrlm_do_regex((char *)CTRLM_RF4CE_REGEX_BLACKOUT_TIME , contents);
   if(NULL != result) {
      guint temp = atoi(result);
      if(temp > 1) {
         blackout_.blackout_time = temp;
         LOG_INFO("%s: Blackout Time <%u> from RFC\n", __FUNCTION__, blackout_.blackout_time);
      }
      g_free(result);
      result = NULL;
   }

   return TRUE;
}

json_t *ctrlm_obj_network_rf4ce_t::xconf_export_controllers() {
   THREAD_ID_VALIDATE();
   LOG_INFO("%s: entering\n", __FUNCTION__);
   json_t *ret = json_array();
   char fw_version_str[CTRLM_RCU_VERSION_LENGTH];
   char audio_version_str[CTRLM_RCU_VERSION_LENGTH];
   char hw_version_str[CTRLM_RCU_VERSION_LENGTH];
   char dsp_version_str[CTRLM_RCU_VERSION_LENGTH];
   char keyword_model_version_str[CTRLM_RCU_VERSION_LENGTH];
   char arm_version_str[CTRLM_RCU_VERSION_LENGTH];
   errno_t safec_rc = -1;

   // map to get unique types and versions.  Probably should be full full controller instead
   // of just version so more checks could be done
   map<string, controller_type_details_t *> controller_types;
   map<string, controller_type_details_t *>::iterator type_it;
   char product_name[CTRLM_RF4CE_RIB_ATTR_LEN_PRODUCT_NAME];
   string json_out_line;

   // time_t struct for check for stale entries
   time_t stale_entry_time = time(NULL);
   time_t shutdown_time    = ctrlm_shutdown_time_get();
   //If today's time is before that last shutdown time, then this time is wrong, so use the shutdown time
   if(stale_entry_time < shutdown_time) {
      LOG_WARN("%s: Current Time <%ld> is less than the last shutdown time <%ld>\n", __FUNCTION__, stale_entry_time, shutdown_time);
      stale_entry_time = shutdown_time;
   }
   stale_entry_time -= (604800); // One week from now
   
   time_t last_key         = 0;
   time_t last_check_in    = 0;
   time_t last_heartbeat   = 0;

   LOG_DEBUG("%s: doing xconf json create RF4CE network\n", __FUNCTION__);

   LOG_INFO("%s: iterating controlers\n", __FUNCTION__);
   for(auto it_map = controllers_.begin(); it_map != controllers_.end(); it_map++) {
      ctrlm_obj_controller_rf4ce_t *obj_controller_rf4ce = it_map->second;
      version_software_t version_new = obj_controller_rf4ce->version_software_get();

      // this will fail when new versions come out like XR15-20 but it is the only way to
      // get correct name for a type of controller.  product_name_get returns incorrect name
      // for use with device update
      switch(obj_controller_rf4ce->controller_type_get()) {
         case  RF4CE_CONTROLLER_TYPE_XR11:
            safec_rc = strcpy_s(product_name, sizeof(product_name), RF4CE_PRODUCT_NAME_XR11);
            ERR_CHK(safec_rc);
            break;
         case  RF4CE_CONTROLLER_TYPE_XR15:
            safec_rc = strcpy_s(product_name, sizeof(product_name), RF4CE_PRODUCT_NAME_XR15);
            ERR_CHK(safec_rc);
            break;
         case  RF4CE_CONTROLLER_TYPE_XR15V2:
            safec_rc = strcpy_s(product_name, sizeof(product_name), RF4CE_PRODUCT_NAME_XR15V2);
            ERR_CHK(safec_rc);
            break;
         case  RF4CE_CONTROLLER_TYPE_XR16:
            safec_rc = strcpy_s(product_name, sizeof(product_name), RF4CE_PRODUCT_NAME_XR16);
            ERR_CHK(safec_rc);
            break;
         case  RF4CE_CONTROLLER_TYPE_XR19:
            safec_rc = strcpy_s(product_name, sizeof(product_name), RF4CE_PRODUCT_NAME_XR19);
            ERR_CHK(safec_rc);
            break;
         case  RF4CE_CONTROLLER_TYPE_XRA:
            safec_rc = strcpy_s(product_name, sizeof(product_name), RF4CE_PRODUCT_NAME_XRA);
            ERR_CHK(safec_rc);
            break;
         default:
            // currently no other remotes are downloadable so dont include them in xconf export
            LOG_INFO("%s: controller of type %d ignored\n", __FUNCTION__, obj_controller_rf4ce->controller_type_get());
            continue;
      }
      // Check to see if the controller was used / checked in within the last week. If not, do not report..
      obj_controller_rf4ce->time_last_key_get(&last_key);
      obj_controller_rf4ce->time_last_checkin_for_device_update_get(&last_check_in);
      obj_controller_rf4ce->time_last_heartbeat_get(&last_heartbeat);
      if(last_key < stale_entry_time && last_check_in < stale_entry_time && last_heartbeat < stale_entry_time) {
         LOG_INFO("%s: controller %s <stale entry: YES> <software version: %u.%u.%u.%u> <controller id: %u> <last keypress: %ld> <last check-in: %ld>\n", __FUNCTION__, product_name, version_new.major, version_new.minor, version_new.revision, version_new.patch, it_map->first, (long)last_key, (long)last_check_in);
         continue;
      }
      LOG_INFO("%s: controller %s <stale entry: NO> <software version: %u.%u.%u.%u> <controller id: %u> <last keypress: %ld> <last check-in: %ld>\n", __FUNCTION__, product_name, version_new.major, version_new.minor, version_new.revision, version_new.patch, it_map->first, (long)last_key, (long)last_check_in);

      // checking to see if we have already dont this type of controller as we only want to do a type once
      // and send one line for each type with min version
      type_it = controller_types.find(product_name);
      if (type_it != controller_types.end()) {
         // we already have a product of this type so check for min version
         if (obj_controller_rf4ce->version_compare(type_it->second->software_version, version_new) > 0) {
            // new is less then old so we want to change to new controller
            type_it->second->software_version = version_new;
         }
         version_new = obj_controller_rf4ce->version_audio_data_get();
         if (obj_controller_rf4ce->version_compare(type_it->second->audio_version, version_new) > 0) {
            // new is less then old so we want to change to new controller
            type_it->second->audio_version = version_new;
         }
         if(obj_controller_rf4ce->controller_type_get() == RF4CE_CONTROLLER_TYPE_XR19) {
            version_new = obj_controller_rf4ce->version_dsp_get();
            if(obj_controller_rf4ce->version_compare(type_it->second->dsp_version, version_new) > 0) {
               type_it->second->dsp_version = version_new;
            }

            version_new = obj_controller_rf4ce->version_keyword_model_get();
            if(obj_controller_rf4ce->version_compare(type_it->second->keyword_model_version, version_new) > 0) {
               type_it->second->keyword_model_version = version_new;
            }

            version_new = obj_controller_rf4ce->version_arm_get();
            if(obj_controller_rf4ce->version_compare(type_it->second->arm_version, version_new) > 0) {
               type_it->second->arm_version = version_new;
            }
         }
         //Right now we ignore hw version so dont check it
      }
      else {
         // we dont have type in map so add it;
         controller_type_details_t *new_type = (controller_type_details_t *) g_malloc(sizeof(controller_type_details_t));
         if(new_type==NULL){
            LOG_ERROR("%s: error on mallog aborting\n", __FUNCTION__ );
            // if we could not malloc memory then we need to free our current mallocs and get out
            for (type_it = controller_types.begin(); type_it != controller_types.end(); type_it++) {
               g_free(type_it->second);
               type_it->second = NULL;
            }
            return NULL;
         }
         new_type->software_version = obj_controller_rf4ce->version_software_get();
         new_type->audio_version    = obj_controller_rf4ce->version_audio_data_get();
         new_type->hw_version       = obj_controller_rf4ce->version_hardware_get();
         if(obj_controller_rf4ce->controller_type_get() == RF4CE_CONTROLLER_TYPE_XR19) {
            new_type->dsp_version           = obj_controller_rf4ce->version_dsp_get();
            new_type->keyword_model_version = obj_controller_rf4ce->version_keyword_model_get();
            new_type->arm_version           = obj_controller_rf4ce->version_arm_get();
         }
         controller_types[product_name] = new_type;
      }

   } // end controller loop

   for (type_it = controller_types.begin(); type_it != controller_types.end(); type_it++) {
      json_t *temp = json_object();

      safec_rc = sprintf_s(fw_version_str, sizeof(fw_version_str), "%u.%u.%u.%u", type_it->second->software_version.major,
            type_it->second->software_version.minor, type_it->second->software_version.revision, type_it->second->software_version.patch);
      if(safec_rc < EOK) {
         ERR_CHK(safec_rc);
      }

      safec_rc = sprintf_s(audio_version_str, sizeof(audio_version_str), "%u.%u.%u.%u", type_it->second->audio_version.major, type_it->second->audio_version.minor,
            type_it->second->audio_version.revision, type_it->second->audio_version.patch);
      if(safec_rc < EOK) {
         ERR_CHK(safec_rc);
      }

      safec_rc = sprintf_s(hw_version_str, sizeof(hw_version_str), "%u.%u.%u.%u", type_it->second->hw_version.manufacturer, type_it->second->hw_version.model,
            type_it->second->hw_version.hw_revision, type_it->second->hw_version.lot_code);
      if(safec_rc < EOK) {
         ERR_CHK(safec_rc);
      }

      safec_rc = sprintf_s(dsp_version_str, sizeof(dsp_version_str), "%u.%u.%u.%u", type_it->second->dsp_version.major, type_it->second->dsp_version.minor,
            type_it->second->dsp_version.revision, type_it->second->dsp_version.patch);
      if(safec_rc < EOK) {
         ERR_CHK(safec_rc);
      }

      safec_rc = sprintf_s(keyword_model_version_str, sizeof(keyword_model_version_str), "%u.%u.%u.%u", type_it->second->keyword_model_version.major, type_it->second->keyword_model_version.minor,
            type_it->second->keyword_model_version.revision, type_it->second->keyword_model_version.patch);
      if(safec_rc < EOK) {
         ERR_CHK(safec_rc);
      }

      safec_rc = sprintf_s(arm_version_str, sizeof(arm_version_str), "%u.%u.%u.%u", type_it->second->arm_version.major, type_it->second->arm_version.minor,
            type_it->second->arm_version.revision, type_it->second->arm_version.patch);
      if(safec_rc < EOK) {
         ERR_CHK(safec_rc);
      }

      // we are done with this element so delete the malloc'd memory
      g_free(type_it->second);
      type_it->second = NULL;

      json_object_set(temp, "Product", json_string(type_it->first.c_str()));
      json_object_set(temp, "FwVer", json_string(fw_version_str));
      json_object_set(temp, "HwVer", json_string(hw_version_str));
      json_object_set(temp, "AudioVer", json_string(audio_version_str));
      if(type_it->first == "XR19-10") {
         json_object_set(temp, "DSPVer", json_string(dsp_version_str));
         json_object_set(temp, "KwModelVer", json_string(keyword_model_version_str));
         json_object_set(temp, "ArmVer", json_string(arm_version_str));
      }

      json_array_append(ret, temp);
   }
   LOG_DEBUG("%s: exiting\n", __FUNCTION__);

   return ret;
}

void ctrlm_obj_network_rf4ce_t::check_if_update_file_still_needed(ctrlm_main_queue_msg_update_file_check_t *msg){
   THREAD_ID_VALIDATE();
   // msg consists of
   //   ctrlm_main_queue_msg_header_t                         header;
   //   ctrlm_rf4ce_controller_type_t                         controller_type;
   //   string                                                file_path_archive;
   //   version_software_t                                    update_version;
   //   ctrlm_rf4ce_device_update_image_type_t                update_type;
   //   GCond *                                               cond;
   //   ctrlm_rf4ce_check_update_file_needed_cmd_result_t     *cmd_result;
   gboolean file_is_needed=false;
   LOG_INFO("%s: iterating controlers\n", __FUNCTION__);
   map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it_map;
   for(it_map = controllers_.begin(); it_map != controllers_.end(); it_map++) {
      ctrlm_obj_controller_rf4ce_t *obj_controller_rf4ce = it_map->second;
      ctrlm_rf4ce_controller_type_t controller_type;
      controller_type=obj_controller_rf4ce->controller_type_get();
      LOG_INFO("%s: checking controller %d\n", __FUNCTION__,obj_controller_rf4ce->controller_id_get());

      // check to see if this is the controller that just finished and if so
      // check that the firmware is recorded as updated
      if(msg->controller_id==obj_controller_rf4ce->controller_id_get()){
         LOG_INFO("%s: this is current contoller, continue\n", __FUNCTION__);
         // do we need this check since this only gets called when update is successful?
         if(obj_controller_rf4ce->is_firmeware_updated()){
             continue;
         }
         else{
            LOG_INFO("%s: current controller has not been updated\n", __FUNCTION__);
         }
      }

      if(controller_type!=msg->controller_type){
         LOG_INFO("%s: Not controller type, continue\n", __FUNCTION__);
         continue;
      }
      if(msg->update_type==RF4CE_DEVICE_UPDATE_IMAGE_TYPE_FIRMWARE){
         version_software_t sw_version=obj_controller_rf4ce->version_software_get();
         if(obj_controller_rf4ce->version_compare(sw_version,msg->update_version)<0){
            file_is_needed=true;
            break;
         }
         LOG_INFO("%s: not newer software version\n", __FUNCTION__);
      }else if(msg->update_type==RF4CE_DEVICE_UPDATE_IMAGE_TYPE_AUDIO_DATA_1){
         version_software_t audio_version=obj_controller_rf4ce->version_audio_data_get();
         if(obj_controller_rf4ce->version_compare(audio_version,msg->update_version)<0){
            file_is_needed=true;
            break;
         }
         LOG_INFO("%s: not newer audio version\n", __FUNCTION__);
      }else{
         LOG_ERROR("%s: invalid update type %d\n", __FUNCTION__,msg->update_type);
         // TODO should we delete the file here or not?
         return;
      }
   }
   if(file_is_needed==false){
      LOG_INFO("%s: update file not needed so deleting %s\n", __FUNCTION__,msg->file_path_archive);
      if(!remove((const char *)msg->file_path_archive)) {
          LOG_INFO("%s: file path archieve not removed \n", __FUNCTION__);
      }  //CID:84920 - Checked return
   }else{
      LOG_INFO("%s: update file needed keeping %s\n", __FUNCTION__,msg->file_path_archive);
   }

}

guchar ctrlm_obj_network_rf4ce_t::property_write_ir_rf_database(guchar index, guchar *data, guchar length) {
   THREAD_ID_VALIDATE();
   if(length > CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE || data == NULL) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   ctrlm_rf4ce_ir_rf_db_entry_t *entry = ctrlm_rf4ce_ir_rf_db_entry_t::from_rib_binary(data, length);
   if(entry) {
      LOG_INFO("%s: %s\n", __FUNCTION__, entry->to_string(true).c_str());
      this->ir_rf_database_.add_ir_code_entry(entry, (ctrlm_key_code_t)index);
   } else {
      if(length > 0 && data[0] == 0xC0) {
         LOG_WARN("%s: Index <%s> set to default\n", __FUNCTION__, ctrlm_key_code_str((ctrlm_key_code_t)index));
         this->ir_rf_database_.remove_entry((ctrlm_key_code_t)index);
      } else {
         LOG_ERROR("%s: Invalid ir rf data\n", __FUNCTION__);
         ctrlm_print_data_hex(__FUNCTION__, data, length, 32);
      }
   }
   return(length);
}

guchar ctrlm_obj_network_rf4ce_t::property_read_ir_rf_database(guchar index, guchar *data, guchar length) {
   THREAD_ID_VALIDATE();
   uint16_t ret = 0;
   if(length != CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return((guchar)ret);
   }

   ctrlm_rf4ce_ir_rf_db_entry_t *entry = this->ir_rf_database_.get_ir_code((ctrlm_key_code_t)index);
   if(entry) {
      guchar *ir_data = NULL;
      if(entry->to_binary(&ir_data, &ret)) {
         if(ir_data) {
            errno_t safec_rc = -1;
            safec_rc = memcpy_s(data, length, ir_data, ret);
            ERR_CHK(safec_rc);
            LOG_INFO("%s: Key Code <%s>, %s\n", __FUNCTION__, ctrlm_key_code_str((ctrlm_key_code_t)index), entry->to_string(true).c_str());
            free(ir_data);
            ir_data = NULL;
         } else {
            LOG_ERROR("%s: Key Code <%s> - ir_data is NULL\n", __FUNCTION__, ctrlm_key_code_str((ctrlm_key_code_t)index));
         }
      } else {
         LOG_ERROR("%s: Key Code <%s> - to_binary failed\n", __FUNCTION__, ctrlm_key_code_str((ctrlm_key_code_t)index));
      }
   } else {
      if(length >= 83) { // This is what legacy code would return
         errno_t safec_rc = -1;
         LOG_INFO("%s: Key Code <%s> - DEFAULT - Returning default 83 byte payload\n", __FUNCTION__, ctrlm_key_code_str((ctrlm_key_code_t)index));
         safec_rc = memset_s(data, length, 0, 83 * sizeof(guchar));
         if(safec_rc < 0) ERR_CHK(safec_rc);
         data[0] = 0xC0;
         ret     = 83;
      } else { // Just for sanity
         LOG_WARN("%s: Key Code <%s> - DEFAULT - Length is not as expected, return default 1 byte payload\n", __FUNCTION__, ctrlm_key_code_str((ctrlm_key_code_t)index));
         data[0] = 0xC0;
         ret     = 1;
      }
   }
   return((guchar)ret);
}

void ctrlm_obj_network_rf4ce_t::ir_rf_database_read_from_db() {
   this->ir_rf_database_.load_db();
   LOG_INFO("%s:\n%s\n", __FUNCTION__, this->ir_rf_database_.to_string(true).c_str());
}

guchar ctrlm_obj_network_rf4ce_t::property_write_target_irdb_status(guchar *data, guchar length) {
   THREAD_ID_VALIDATE();
   length = write_target_irdb_status(data, length);
   if(length == 0) {
      return(0);
   }
   ctrlm_db_target_irdb_status_write(data, length);
   return(length);
}

guchar ctrlm_obj_network_rf4ce_t::write_target_irdb_status(guchar *data, guchar length) {
   THREAD_ID_VALIDATE();
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   target_irdb_status_t target_irdb_status;
   //If the bit is set for avr code but the first byte of the avr code is 0
   //then the avr 5 digit code is off by one (old bug) and we need to get it from it's
   //actual stored location as XR19 will need to be able to get this code.
   //Then write it back to the correct location.
   if((data[0] & TARGET_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR) && (data[7] == 0)) {
      guchar target_irdb_status[CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS];
      LOG_WARN("%s: There is an avr code but it is stored in the wrong location.  Get it from the correct location and store in the correct location.\n", __FUNCTION__);
      data[7]  = data[8];
      data[8]  = data[9];
      data[9]  = data[10];
      data[10] = data[11];
      data[11] = data[12];
      data[12] = 0;
      errno_t safec_rc = memcpy_s(target_irdb_status, sizeof(target_irdb_status), data,CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS);
      ERR_CHK(safec_rc);
      ctrlm_db_target_irdb_status_write(target_irdb_status, CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS);
   }

   target_irdb_status.flags              = data[0];
   target_irdb_status.irdb_string_tv[0]  = data[1];
   target_irdb_status.irdb_string_tv[1]  = data[2];
   target_irdb_status.irdb_string_tv[2]  = data[3];
   target_irdb_status.irdb_string_tv[3]  = data[4];
   target_irdb_status.irdb_string_tv[4]  = data[5];
   target_irdb_status.irdb_string_tv[5]  = data[6];
   target_irdb_status.irdb_string_tv[6]  = '\0';
   target_irdb_status.irdb_string_avr[0] = data[7];
   target_irdb_status.irdb_string_avr[1] = data[8];
   target_irdb_status.irdb_string_avr[2] = data[9];
   target_irdb_status.irdb_string_avr[3] = data[10];
   target_irdb_status.irdb_string_avr[4] = data[11];
   target_irdb_status.irdb_string_avr[5] = data[12];
   target_irdb_status.irdb_string_avr[6] = '\0';

   if(target_irdb_status_.flags != target_irdb_status.flags || memcmp(target_irdb_status_.irdb_string_tv,  target_irdb_status.irdb_string_tv, 6)
                                                            || memcmp(target_irdb_status_.irdb_string_avr, target_irdb_status.irdb_string_avr, 6)) {
      // Store the data on the target object
      target_irdb_status_ = target_irdb_status;
   }

   // ROBIN -- If ir codes are programmed, push them to voice assistants like XR19
   if(target_irdb_status_.flags != TARGET_IRDB_STATUS_DEFAULT) {
      push_ir_codes_to_voice_assistants_from_target_irdb_status();
   }

   LOG_INFO("%s: Flags 0x%02X TV %s AVR %s\n", __FUNCTION__, target_irdb_status_.flags, target_irdb_status_.irdb_string_tv, target_irdb_status_.irdb_string_avr);
   return(length);
}

void ctrlm_obj_network_rf4ce_t::push_ir_codes_to_voice_assistants_from_target_irdb_status() {
   errno_t safec_rc = -1;
   int ind =-1;

   for(auto itr = controllers_.begin(); itr != controllers_.end(); itr++) {
      if(itr->second->controller_type_get() == RF4CE_CONTROLLER_TYPE_XR19) {
         controller_irdb_status_t irdb_status = itr->second->controller_irdb_status_get();

         if(irdb_status.flags & CONTROLLER_IRDB_STATUS_FLAGS_NO_IR_PROGRAMMED) {
            itr->second->push_ir_codes();
         } else if(target_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_TV) {
           safec_rc = strcmp_s(irdb_status.irdb_string_tv, strlen(irdb_status.irdb_string_tv), target_irdb_status_.irdb_string_tv, &ind);
           ERR_CHK(safec_rc);
           if((safec_rc == EOK) && (ind)) {
             itr->second->push_ir_codes();
           }
         } else if(target_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR) {
           safec_rc = strcmp_s(irdb_status.irdb_string_avr, strlen(irdb_status.irdb_string_avr), target_irdb_status_.irdb_string_avr, &ind);
           ERR_CHK(safec_rc);
           if((safec_rc == EOK) && (ind)) {
             itr->second->push_ir_codes();
           }
         } else if(target_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_RF_DB) {
            itr->second->push_ir_codes();
         }
      }
   }
}

guchar ctrlm_obj_network_rf4ce_t::property_read_target_irdb_status(guchar *data, guchar length) {
   THREAD_ID_VALIDATE();
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   // Load the data from the target object
   data[0]  = target_irdb_status_.flags;
   data[1]  = target_irdb_status_.irdb_string_tv[0];
   data[2]  = target_irdb_status_.irdb_string_tv[1];
   data[3]  = target_irdb_status_.irdb_string_tv[2];
   data[4]  = target_irdb_status_.irdb_string_tv[3];
   data[5]  = target_irdb_status_.irdb_string_tv[4];
   data[6]  = target_irdb_status_.irdb_string_tv[5];
   data[7]  = target_irdb_status_.irdb_string_avr[0];
   data[8]  = target_irdb_status_.irdb_string_avr[1];
   data[9]  = target_irdb_status_.irdb_string_avr[2];
   data[10] = target_irdb_status_.irdb_string_avr[3];
   data[11] = target_irdb_status_.irdb_string_avr[4];
   data[12] = target_irdb_status_.irdb_string_avr[5];

   LOG_INFO("%s: Flags 0x%02X TV %s AVR %s\n", __FUNCTION__, target_irdb_status_.flags, target_irdb_status_.irdb_string_tv, target_irdb_status_.irdb_string_avr);
   return(CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS);
}

gboolean ctrlm_obj_network_rf4ce_t::target_irdb_status_read_from_db() {
   THREAD_ID_VALIDATE();
   guint32  length;
   guchar * data;

   //Read from db
   ctrlm_db_target_irdb_status_read((guchar **)&data, &length);

   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS) {
      LOG_WARN("%s: Length of DB entry invalid, either doesn't exist or is corrupt\n", __FUNCTION__);
      return(false);
   }
   //Write to the cached database
   write_target_irdb_status(data, length);
   g_free(data);
   LOG_INFO("%s: Flags 0x%02X TV %s AVR %s\n", __FUNCTION__, target_irdb_status_.flags, target_irdb_status_.irdb_string_tv, target_irdb_status_.irdb_string_avr);
   return(true);
}

void ctrlm_obj_network_rf4ce_t::target_irdb_status_set(controller_irdb_status_t controller_irdb_status) {
   THREAD_ID_VALIDATE();
   guchar data[CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS] = {0};

   if(controller_irdb_status.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_RF_DB) {
      target_irdb_status_.flags = CONTROLLER_IRDB_STATUS_FLAGS_IR_RF_DB;
   } else {
      if(controller_irdb_status.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_TV) {
         target_irdb_status_.flags             &= ~CONTROLLER_IRDB_STATUS_FLAGS_IR_RF_DB;
         target_irdb_status_.flags             |= controller_irdb_status.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_TV;
         target_irdb_status_.irdb_string_tv[0]  = controller_irdb_status.irdb_string_tv[0];
         target_irdb_status_.irdb_string_tv[1]  = controller_irdb_status.irdb_string_tv[1];
         target_irdb_status_.irdb_string_tv[2]  = controller_irdb_status.irdb_string_tv[2];
         target_irdb_status_.irdb_string_tv[3]  = controller_irdb_status.irdb_string_tv[3];
         target_irdb_status_.irdb_string_tv[4]  = controller_irdb_status.irdb_string_tv[4];
         target_irdb_status_.irdb_string_tv[5]  = controller_irdb_status.irdb_string_tv[5];
         target_irdb_status_.irdb_string_tv[6]  = controller_irdb_status.irdb_string_tv[6];
      }
      if(controller_irdb_status.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR) {
         target_irdb_status_.flags             &= ~CONTROLLER_IRDB_STATUS_FLAGS_IR_RF_DB;
         target_irdb_status_.flags             |= controller_irdb_status.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR;
         target_irdb_status_.irdb_string_avr[0] = controller_irdb_status.irdb_string_avr[0];
         target_irdb_status_.irdb_string_avr[1] = controller_irdb_status.irdb_string_avr[1];
         target_irdb_status_.irdb_string_avr[2] = controller_irdb_status.irdb_string_avr[2];
         target_irdb_status_.irdb_string_avr[3] = controller_irdb_status.irdb_string_avr[3];
         target_irdb_status_.irdb_string_avr[4] = controller_irdb_status.irdb_string_avr[4];
         target_irdb_status_.irdb_string_avr[5] = controller_irdb_status.irdb_string_avr[5];
         target_irdb_status_.irdb_string_avr[6] = controller_irdb_status.irdb_string_avr[6];
      }
   }

   // No IR Programmed can only be set if IR RF DB, TV, AVR bits are clear
   if(!(target_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_RF_DB) && !(target_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_TV) && !(target_irdb_status_.flags & CONTROLLER_IRDB_STATUS_FLAGS_IR_DB_CODE_AVR)) {
      target_irdb_status_.flags = CONTROLLER_IRDB_STATUS_FLAGS_NO_IR_PROGRAMMED;
   // No IR Programmed must be cleared if IR RF DB, TV, or AVR bits are set
   } else {
      target_irdb_status_.flags &= ~CONTROLLER_IRDB_STATUS_FLAGS_NO_IR_PROGRAMMED;
   }

   data[0]  = target_irdb_status_.flags;
   data[1]  = target_irdb_status_.irdb_string_tv[0];
   data[2]  = target_irdb_status_.irdb_string_tv[1];
   data[3]  = target_irdb_status_.irdb_string_tv[2];
   data[4]  = target_irdb_status_.irdb_string_tv[3];
   data[5]  = target_irdb_status_.irdb_string_tv[4];
   data[6]  = target_irdb_status_.irdb_string_tv[5];
   data[7]  = target_irdb_status_.irdb_string_avr[0];
   data[8]  = target_irdb_status_.irdb_string_avr[1];
   data[9]  = target_irdb_status_.irdb_string_avr[2];
   data[10] = target_irdb_status_.irdb_string_avr[3];
   data[11] = target_irdb_status_.irdb_string_avr[4];
   data[12] = target_irdb_status_.irdb_string_avr[5];

   // ROBIN -- If ir codes are programmed, push them to voice assistants like XR19
   if(target_irdb_status_.flags != TARGET_IRDB_STATUS_DEFAULT) {
      push_ir_codes_to_voice_assistants_from_target_irdb_status();
   }
   LOG_INFO("%s: Flags 0x%02X TV %s AVR %s\n", __FUNCTION__, target_irdb_status_.flags, target_irdb_status_.irdb_string_tv, target_irdb_status_.irdb_string_avr);
   ctrlm_db_target_irdb_status_write(data, CTRLM_RF4CE_RIB_ATTR_LEN_TARGET_IRDB_STATUS);
   ir_rf_database_new_ = false;
}

guchar ctrlm_obj_network_rf4ce_t::target_irdb_status_flags_get() {
   return(target_irdb_status_.flags);
}

controller_irdb_status_t ctrlm_obj_network_rf4ce_t::most_recent_controller_irdb_status_get(void) {
   THREAD_ID_VALIDATE();
   controller_irdb_status_t most_recent_controller_irdb_status;
   most_recent_controller_irdb_status.flags = CONTROLLER_IRDB_STATUS_FLAGS_NO_IR_PROGRAMMED;
   time_t time_most_recent_controller_bound, time_controller;
   time_most_recent_controller_bound = 0L;

   for(map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin(); it != controllers_.end(); it++) {
      controller_irdb_status_t controller_irdb_status = it->second->controller_irdb_status_get();
      if(!(controller_irdb_status.flags & CONTROLLER_IRDB_STATUS_FLAGS_NO_IR_PROGRAMMED)) {
         time_controller = it->second->time_binding_get();
         if(time_most_recent_controller_bound < time_controller) {
            time_most_recent_controller_bound = time_controller;
            most_recent_controller_irdb_status = controller_irdb_status;
         }
      }
   }
   return most_recent_controller_irdb_status;
}

void ctrlm_obj_network_rf4ce_t::req_process_polling_action_push(void *data, int size) {
   ctrlm_main_queue_msg_rcu_polling_action_t *dqm = (ctrlm_main_queue_msg_rcu_polling_action_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_rcu_polling_action_t));
   g_assert(dqm->cmd_result);

   THREAD_ID_VALIDATE();

   *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_SUCCESS;

   ctrlm_network_id_t network_id       = dqm->header.network_id;
   ctrlm_controller_id_t controller_id = dqm->controller_id;
   if (dqm->controller_id == CTRLM_MAIN_CONTROLLER_ID_LAST_USED) {
      controller_id = controller_id_get_last_recently_used();
   }

   guchar attr_data[CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS] = {0};
   guint32 attr_len = CTRLM_RF4CE_RIB_ATTR_LEN_CONTROLLER_IRDB_STATUS;

   if ( RF4CE_POLLING_ACTION_IRRF_STATUS == dqm->action ) {
      controller_irdb_status_t most_recent_controller_irdb_status = most_recent_controller_irdb_status_get();
      if (most_recent_controller_irdb_status.flags != CONTROLLER_IRDB_STATUS_FLAGS_NO_IR_PROGRAMMED) {
         // Load the data from the most recently programmed controller object
         attr_data[0]  = most_recent_controller_irdb_status.flags;
         attr_data[1]  = most_recent_controller_irdb_status.irdb_string_tv[0];
         attr_data[2]  = most_recent_controller_irdb_status.irdb_string_tv[1];
         attr_data[3]  = most_recent_controller_irdb_status.irdb_string_tv[2];
         attr_data[4]  = most_recent_controller_irdb_status.irdb_string_tv[3];
         attr_data[5]  = most_recent_controller_irdb_status.irdb_string_tv[4];
         attr_data[6]  = most_recent_controller_irdb_status.irdb_string_tv[5];
         attr_data[7]  = most_recent_controller_irdb_status.irdb_string_avr[0];
         attr_data[8]  = most_recent_controller_irdb_status.irdb_string_avr[1];
         attr_data[9]  = most_recent_controller_irdb_status.irdb_string_avr[2];
         attr_data[10] = most_recent_controller_irdb_status.irdb_string_avr[3];
         attr_data[11] = most_recent_controller_irdb_status.irdb_string_avr[4];
         attr_data[12] = most_recent_controller_irdb_status.irdb_string_avr[5];
         attr_data[13] = most_recent_controller_irdb_status.irdb_load_status_tv;
         attr_data[14] = most_recent_controller_irdb_status.irdb_load_status_avr;
      } else {
         LOG_INFO("%s: No controller with IR programming found. Flags <0x%02X>\n", __FUNCTION__, most_recent_controller_irdb_status.flags);
         *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
         ctrlm_obj_network_t::req_process_polling_action_push(data, size);
         return;
      }
   }

   for(auto itr = controllers_.begin(); itr != controllers_.end(); itr++) {
      if (controller_id == CTRLM_MAIN_CONTROLLER_ID_ALL || controller_id == itr->first) {

         LOG_INFO("%s: Adding polling action for controller id %u \n", __FUNCTION__, itr->first);
         switch(dqm->action) {
            case RF4CE_POLLING_ACTION_IRRF_STATUS: {
               ctrlm_timestamp_t timestamp;
               ctrlm_timestamp_get(&timestamp);
               // Push polling action
               itr->second->push_ir_codes();
               // Program controller with IR codes from most recently programmed controller
               itr->second->rf4ce_rib_set_controller(timestamp, CTRLM_RF4CE_RIB_ATTR_ID_CONTROLLER_IRDB_STATUS, (guchar)0x00, attr_len, &attr_data[0]);
               break;
            }
            default: {
               ctrlm_rf4ce_polling_action_push(network_id, itr->first, dqm->action, NULL, 0);
               break;
            }
         }
      }
   }

   ctrlm_obj_network_t::req_process_polling_action_push(data, size);
}

guchar ctrlm_obj_network_rf4ce_t::property_read_mfg_test(guchar *data, guchar length) {
   THREAD_ID_VALIDATE();
   if((length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST) && (length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_HAPTICS)) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   if(!mfg_test_enabled()) {
    LOG_ERROR("%s: Manufacturing testing is disabled\n", __FUNCTION__);
    return 0;
   }

   // Load the data from the target object
   if(CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_HAPTICS == length) {
      data[0]  =  mfg_test_.mic_delay             & 0xFF;
      data[1]  = (mfg_test_.mic_delay >> 8)       & 0xFF;
      data[2]  =  mfg_test_.mic_duration          & 0xFF;
      data[3]  = (mfg_test_.mic_duration >> 8)    & 0xFF;
      data[4]  =  mfg_test_.sweep_delay           & 0xFF;
      data[5]  = (mfg_test_.sweep_delay >> 8)     & 0xFF;
      data[6]  =  mfg_test_.haptic_delay          & 0xFF;
      data[7]  = (mfg_test_.haptic_delay >> 8)    & 0xFF;
      data[8]  =  mfg_test_.haptic_duration       & 0xFF;
      data[9]  = (mfg_test_.haptic_duration >> 8) & 0xFF;
      data[10] =  mfg_test_.reset_delay           & 0xFF;
      data[11] = (mfg_test_.reset_delay >> 8)     & 0xFF;
      LOG_INFO("%s: Mic Delay <%u>/Duration <%u>, Sweep Delay <%u>, Haptics Delay <%u>/Duration <%u>, Reset Delay <%u>\n", __FUNCTION__, mfg_test_.mic_delay, mfg_test_.mic_duration, mfg_test_.sweep_delay, mfg_test_.haptic_delay, mfg_test_.haptic_duration, mfg_test_.reset_delay);
   } else {
      data[0]  =  mfg_test_.mic_delay          & 0xFF;
      data[1]  = (mfg_test_.mic_delay >> 8)    & 0xFF;
      data[2]  =  mfg_test_.mic_duration       & 0xFF;
      data[3]  = (mfg_test_.mic_duration >> 8) & 0xFF;
      data[4]  =  mfg_test_.sweep_delay        & 0xFF;
      data[5]  = (mfg_test_.sweep_delay >> 8)  & 0xFF;
      data[6]  =  mfg_test_.reset_delay        & 0xFF;
      data[7]  = (mfg_test_.reset_delay >> 8)  & 0xFF;
      LOG_INFO("%s: Mic Delay <%u>, Mic Duration <%u>, Sweep Delay <%u>, Reset Delay <%u>\n", __FUNCTION__, mfg_test_.mic_delay, mfg_test_.mic_duration, mfg_test_.sweep_delay, mfg_test_.reset_delay);
   }

   return(length);
}

guchar ctrlm_obj_network_rf4ce_t::property_write_mfg_test(guchar *data, guchar length) {
   THREAD_ID_VALIDATE();
   if((length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST) && (length != CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_HAPTICS)) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   if(!mfg_test_enabled()) {
    LOG_ERROR("%s: Manufacturing testing is disabled\n", __FUNCTION__);
    return 0;
   }

   if(CTRLM_RF4CE_RIB_ATTR_LEN_MFG_TEST_HAPTICS == length) {
      mfg_test_.mic_delay       = data[0]  + (data[1]  << 8);
      mfg_test_.mic_duration    = data[2]  + (data[3]  << 8);
      mfg_test_.sweep_delay     = data[4]  + (data[5]  << 8);
      mfg_test_.haptic_delay    = data[6]  + (data[7]  << 8);
      mfg_test_.haptic_duration = data[8]  + (data[9]  << 8);
      mfg_test_.reset_delay     = data[10] + (data[11] << 8);
      LOG_INFO("%s: Mic Delay <%u>/Duration <%u>, Sweep Delay <%u>, Haptics Delay <%u>/Duration <%u>, Reset Delay <%u>\n", __FUNCTION__, mfg_test_.mic_delay, mfg_test_.mic_duration, mfg_test_.sweep_delay, mfg_test_.haptic_delay, mfg_test_.haptic_duration, mfg_test_.reset_delay);
   } else {
      mfg_test_.mic_delay    = data[0] + (data[1] << 8);
      mfg_test_.mic_duration = data[2] + (data[3] << 8);
      mfg_test_.sweep_delay  = data[4] + (data[5] << 8);
      mfg_test_.reset_delay  = data[6] + (data[7] << 8);
      LOG_INFO("%s: Mic Delay <%u>, Mic Duration <%u>, Sweep Delay <%u>, Reset Delay <%u>\n", __FUNCTION__, mfg_test_.mic_delay, mfg_test_.mic_duration, mfg_test_.sweep_delay, mfg_test_.reset_delay);
   }

   return(length);
}

gint *ctrlm_obj_network_rf4ce_t::controller_binding_in_progress_get_pointer() {
   return (gint *)&binding_in_progress_;
}

void ctrlm_obj_network_rf4ce_t::controller_binding_in_progress_tag_reset() {
   binding_in_progress_tag_ = 0;
}

void ctrlm_obj_network_rf4ce_t::blackout_tag_reset() {
   blackout_.blackout_tag = 0;
}

void ctrlm_obj_network_rf4ce_t::voice_command_status_set(void *data, int size){
   ctrlm_main_queue_msg_voice_command_status_t *dqm = (ctrlm_main_queue_msg_voice_command_status_t *)data;

   if(size != sizeof(ctrlm_main_queue_msg_voice_command_status_t) || NULL == data) {
      LOG_ERROR("%s: Incorrect parameters\n", __FUNCTION__);
      return;
   } else if(!controller_exists(dqm->controller_id)) {
      LOG_WARN("%s: Controller %u NOT present.\n", __FUNCTION__, dqm->controller_id);
      return;
   }
   guchar status_data[CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_STATUS] = {0};
   status_data[0] = dqm->status; // Voice Command Status
#ifdef USE_VOICE_SDK
   if(dqm->status == VOICE_COMMAND_STATUS_TV_AVR_CMD) {
      status_data[2] = dqm->data.tv_avr.cmd; // TV/AVR Command
      if(status_data[2] == CTRLM_VOICE_TV_AVR_CMD_POWER_ON || status_data[2] == CTRLM_VOICE_TV_AVR_CMD_POWER_OFF) {
         status_data[1] |= (dqm->data.tv_avr.toggle_fallback ? 0x01 : 0x00);
      } else if(status_data[2] == CTRLM_VOICE_TV_AVR_CMD_VOLUME_UP || status_data[2] == CTRLM_VOICE_TV_AVR_CMD_VOLUME_DOWN) {
         status_data[3] = dqm->data.tv_avr.ir_repeats;
      }
   }
#endif
   controllers_[dqm->controller_id]->rf4ce_rib_set_target((ctrlm_rf4ce_rib_attr_id_t)CTRLM_RF4CE_RIB_ATTR_ID_VOICE_COMMAND_STATUS, 0, CTRLM_RF4CE_RIB_ATTR_LEN_VOICE_COMMAND_STATUS, status_data);
}

gboolean ctrlm_obj_network_rf4ce_t::mfg_test_enabled() {
  return mfg_test_.enabled;
}

void ctrlm_obj_network_rf4ce_t::default_polling_configuration() {

  // Clear memory
  errno_t safec_rc = memset_s(controller_polling_methods_, sizeof(controller_polling_methods_), 0, sizeof(controller_polling_methods_));
  ERR_CHK(safec_rc);
  safec_rc = memset_s(controller_polling_configuration_heartbeat_, sizeof(controller_polling_configuration_heartbeat_), 0, sizeof(controller_polling_configuration_heartbeat_));
  ERR_CHK(safec_rc);
  safec_rc = memset_s(controller_polling_configuration_mac_, sizeof(controller_polling_configuration_mac_), 0, sizeof(controller_polling_configuration_mac_));
  ERR_CHK(safec_rc);

  // Generic polling configuration
  controller_generic_polling_configuration_.uptime_multiplier = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_HB_GENERIC_CONFIG_UPTIME_MULTIPLIER;
  controller_generic_polling_configuration_.hb_time_to_save   = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_HB_GENERIC_CONFIG_HB_TIME_TO_SAVE;

   for(int i = 0; i < RF4CE_CONTROLLER_TYPE_INVALID; i++) {
      switch(i) {
        case RF4CE_CONTROLLER_TYPE_XR11: {
          // Default XR11 Polling Methods
          controller_polling_methods_[i]                               = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR11V2_METHODS;
          // Default XR11 Heartbeat Configuration
          controller_polling_configuration_heartbeat_[i].trigger       = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR11V2_HEARTBEAT_TRIGGER;
          controller_polling_configuration_heartbeat_[i].kp_counter    = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR11V2_HEARTBEAT_KP_COUNTER;
          controller_polling_configuration_heartbeat_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR11V2_HEARTBEAT_TIME_INTERVAL;
          controller_polling_configuration_heartbeat_[i].reserved      = 0;
          // Default XR11 MAC Configuration
          controller_polling_configuration_mac_[i].trigger       = POLLING_TRIGGER_FLAG_TIME;
          controller_polling_configuration_mac_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR11V2_MAC_TIME_INTERVAL;
          break;
        }
        case RF4CE_CONTROLLER_TYPE_XR15: {
          // Default XR15 Polling Methods
          controller_polling_methods_[i]                               = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR15V1_METHODS;
          // Default XR15 Heartbeat Configuration
          controller_polling_configuration_heartbeat_[i].trigger       = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR15V1_HEARTBEAT_TRIGGER;
          controller_polling_configuration_heartbeat_[i].kp_counter    = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR15V1_HEARTBEAT_KP_COUNTER;
          controller_polling_configuration_heartbeat_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR15V1_HEARTBEAT_TIME_INTERVAL;
          controller_polling_configuration_heartbeat_[i].reserved      = 0;
          // Default XR15 MAC Configuration
          controller_polling_configuration_mac_[i].trigger       = POLLING_TRIGGER_FLAG_TIME;
          controller_polling_configuration_mac_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR15V1_MAC_TIME_INTERVAL;

          break;
        }
        case RF4CE_CONTROLLER_TYPE_XR15V2: {
          // Default XR15V2 Polling Methods
          controller_polling_methods_[i]                               = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR15V2_METHODS;
          // Default XR15V2 Heartbeat Configuration
          controller_polling_configuration_heartbeat_[i].trigger       = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR15V2_HEARTBEAT_TRIGGER;
          controller_polling_configuration_heartbeat_[i].kp_counter    = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR15V2_HEARTBEAT_KP_COUNTER;
          controller_polling_configuration_heartbeat_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR15V2_HEARTBEAT_TIME_INTERVAL;
          controller_polling_configuration_heartbeat_[i].reserved      = 0;
          // Default XR15V2 MAC Configuration
          controller_polling_configuration_mac_[i].trigger       = POLLING_TRIGGER_FLAG_TIME;
          controller_polling_configuration_mac_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR15V2_MAC_TIME_INTERVAL;

          // TODO: Implement MAC Polling
          break;
        }
        case RF4CE_CONTROLLER_TYPE_XR16: {
          // Default XR16 Polling Methods
          controller_polling_methods_[i]                               = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR16V1_METHODS;
          // Default XR16 Heartbeat Configuration
          controller_polling_configuration_heartbeat_[i].trigger       = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR16V1_HEARTBEAT_TRIGGER;
          controller_polling_configuration_heartbeat_[i].kp_counter    = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR16V1_HEARTBEAT_KP_COUNTER;
          controller_polling_configuration_heartbeat_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR16V1_HEARTBEAT_TIME_INTERVAL;
          controller_polling_configuration_heartbeat_[i].reserved      = 0;
          // Default XR16 MAC Configuration
          controller_polling_configuration_mac_[i].trigger       = POLLING_TRIGGER_FLAG_TIME;
          controller_polling_configuration_mac_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR16V1_MAC_TIME_INTERVAL;

          break;
        }
        case RF4CE_CONTROLLER_TYPE_XR19: {
          // Default XR19 Polling Methods
          controller_polling_methods_[i]                               = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR19V1_METHODS;
          // Default XR19 Heartbeat Configuration
          controller_polling_configuration_heartbeat_[i].trigger       = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR19V1_HEARTBEAT_TRIGGER;
          controller_polling_configuration_heartbeat_[i].kp_counter    = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR19V1_HEARTBEAT_KP_COUNTER;
          controller_polling_configuration_heartbeat_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XR19V1_HEARTBEAT_TIME_INTERVAL;
          controller_polling_configuration_heartbeat_[i].reserved      = 0;

          break;
        }
        case RF4CE_CONTROLLER_TYPE_XRA: {
          // Default XRA Polling Methods
          controller_polling_methods_[i]                               = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XRAV1_METHODS;
          // Default XRA Heartbeat Configuration
          controller_polling_configuration_heartbeat_[i].trigger       = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XRAV1_HEARTBEAT_TRIGGER;
          controller_polling_configuration_heartbeat_[i].kp_counter    = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XRAV1_HEARTBEAT_KP_COUNTER;
          controller_polling_configuration_heartbeat_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XRAV1_HEARTBEAT_TIME_INTERVAL;
          controller_polling_configuration_heartbeat_[i].reserved      = 0;
          // Default XRA MAC Configuration
          controller_polling_configuration_mac_[i].trigger       = POLLING_TRIGGER_FLAG_TIME;
          controller_polling_configuration_mac_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_XRAV1_MAC_TIME_INTERVAL;

          break;
        }
        default: {
          // Default Polling Methods
          controller_polling_methods_[i]                               = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_DEFAULT_METHODS;
          // Default Heartbeat Configuration
          controller_polling_configuration_heartbeat_[i].trigger       = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_DEFAULT_HEARTBEAT_TRIGGER;
          controller_polling_configuration_heartbeat_[i].kp_counter    = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_DEFAULT_HEARTBEAT_KP_COUNTER;
          controller_polling_configuration_heartbeat_[i].time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_DEFAULT_HEARTBEAT_TIME_INTERVAL;
          controller_polling_configuration_heartbeat_[i].reserved      = 0;
          // Default MAC Configuration
          // TODO: Implement MAC Polling
          break;
        }
      }
   }
}

void ctrlm_obj_network_rf4ce_t::polling_config_tr181_read() {
   guint8 default_polling_methods = 0;
   ctrlm_rf4ce_polling_configuration_t default_polling_config_hb = {0};

   ctrlm_rf4ce_polling_configuration_t default_polling_config_mac;
   errno_t safec_rc = memset_s(&default_polling_config_mac, sizeof(ctrlm_rf4ce_polling_configuration_t), 0, sizeof(ctrlm_rf4ce_polling_configuration_t));
   ERR_CHK(safec_rc);
   default_polling_config_mac.trigger = POLLING_TRIGGER_FLAG_TIME;
   default_polling_config_mac.time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_DEFAULT_MAC_TIME_INTERVAL;

   bool b_has_default_config = false;

   char tr181_buf[1024] = {0};
   if(CTRLM_TR181_RESULT_SUCCESS == ctrlm_tr181_string_get(CTRLM_RF4CE_TR181_POLLING_CONFIGURATION_DEFAULT, tr181_buf, sizeof(tr181_buf))) {
     if(4 == sscanf(tr181_buf, "%hhu:%hu:%hhu:%u:", &default_polling_methods, &default_polling_config_hb.trigger, &default_polling_config_hb.kp_counter, &default_polling_config_hb.time_interval)) {
        LOG_INFO("%s: Default HB Polling Configuration from TR181\n", __FUNCTION__);
        b_has_default_config = true;
     }
   }

   bool b_has_default_mac_config = false;

#ifdef MAC_POLLING
   bool mac_polling_enabled = false;
   if(CTRLM_TR181_RESULT_SUCCESS == ctrlm_tr181_bool_get(CTRLM_RF4CE_TR181_MAC_POLLING_CONFIGURATION_ENABLE, &mac_polling_enabled)) {
      LOG_INFO("%s: Default Mac Polling Configuration from TR181\n", __FUNCTION__);
      b_has_default_mac_config = mac_polling_enabled;
      ctrlm_tr181_int_get(CTRLM_RF4CE_TR181_MAC_POLLING_CONFIGURATION_INTERVAL, (int*)&default_polling_config_mac.time_interval, 1000, 60000);

      if(mac_polling_enabled) {
         polling_methods_ |= POLLING_METHODS_FLAG_MAC;
      } else {
         polling_methods_ &= ~POLLING_METHODS_FLAG_MAC;
      }

   }
#endif
   for(int i = 0; i < RF4CE_CONTROLLER_TYPE_INVALID; i++) {
     LOG_INFO("%s: Polling Configuration Remote Type <%s>\n", __FUNCTION__, ctrlm_rf4ce_controller_type_str((ctrlm_rf4ce_controller_type_t)i));
     const char *controller_tr181_str = 0;

     switch((ctrlm_rf4ce_controller_type_t)i) {
        case RF4CE_CONTROLLER_TYPE_XR11: {
           controller_tr181_str = CTRLM_RF4CE_TR181_POLLING_CONFIGURATION_XR11V2;
           break;
        }
        case RF4CE_CONTROLLER_TYPE_XR15: {
           controller_tr181_str = CTRLM_RF4CE_TR181_POLLING_CONFIGURATION_XR15V1;
           break;
        }
        case RF4CE_CONTROLLER_TYPE_XR15V2: {
           controller_tr181_str = CTRLM_RF4CE_TR181_POLLING_CONFIGURATION_XR15V2;
           break;
        }
        case RF4CE_CONTROLLER_TYPE_XR16: {
           controller_tr181_str = CTRLM_RF4CE_TR181_POLLING_CONFIGURATION_XR16V1;
           break;
        }
        case RF4CE_CONTROLLER_TYPE_XR19: {
           controller_tr181_str = CTRLM_RF4CE_TR181_POLLING_CONFIGURATION_XR19V1;
           break;
        }
        case RF4CE_CONTROLLER_TYPE_XRA: {
           controller_tr181_str = CTRLM_RF4CE_TR181_POLLING_CONFIGURATION_XRAV1;
           break;
        }
        default: {
           break;
        }
     }

     if (b_has_default_mac_config) {
        controller_polling_configuration_mac_[i] = default_polling_config_mac;
     }

     if(controller_tr181_str) {
        if (b_has_default_config) {
           controller_polling_methods_[i] = default_polling_methods;
           controller_polling_configuration_heartbeat_[i] = default_polling_config_hb;
        }
	safec_rc = memset_s(tr181_buf, sizeof(tr181_buf), 0, sizeof(tr181_buf));
        ERR_CHK(safec_rc);
        ctrlm_rf4ce_polling_configuration_t controller_polling_configuration;
        if(CTRLM_TR181_RESULT_SUCCESS == ctrlm_tr181_string_get(controller_tr181_str, tr181_buf, sizeof(tr181_buf))) {
           if(4 == sscanf(tr181_buf, "%hhu:%hu:%hhu:%u:", &controller_polling_methods_[i],
                 &controller_polling_configuration.trigger,
                 &controller_polling_configuration.kp_counter,
                 &controller_polling_configuration.time_interval)) {
              //If MAC polling bit is set, save the mac config
              if(controller_polling_methods_[i] & POLLING_METHODS_FLAG_MAC) {
                 controller_polling_configuration_mac_[i].trigger = controller_polling_configuration.trigger;
                 controller_polling_configuration_mac_[i].kp_counter = controller_polling_configuration.kp_counter;
                 // The MAC polling period has been set above when MAC was enabled
              }
              //If Heartbeat polling bit is set, save the heartbeat config
              if(controller_polling_methods_[i] & POLLING_METHODS_FLAG_HEARTBEAT) {
                 controller_polling_configuration_heartbeat_[i] = controller_polling_configuration;
              }
              LOG_INFO("%s: Controller Polling Configuration Read from TR181 <%s><%s>\n", __FUNCTION__, ctrlm_rf4ce_controller_type_str((ctrlm_rf4ce_controller_type_t)i),ctrlm_rf4ce_controller_polling_methods_str(controller_polling_methods_[i]));
           }
        }
     }
     controller_polling_methods_[i] &= polling_methods_; // The controller polling_methods should only contain methods currently supported by target
   }
}

void ctrlm_obj_network_rf4ce_t::polling_config_read(json_config *conf) {
   if(NULL == conf) {
      LOG_ERROR("%s: json config is NULL!\n", __FUNCTION__);
      return;
   }

   guint8 default_polling_methods = 0;
   ctrlm_rf4ce_polling_configuration_t default_polling_config_hb = {0};

   ctrlm_rf4ce_polling_configuration_t default_polling_config_mac;
   default_polling_config_mac.trigger = POLLING_TRIGGER_FLAG_TIME;
   default_polling_config_mac.time_interval = JSON_INT_VALUE_NETWORK_RF4CE_POLLING_DEFAULT_MAC_TIME_INTERVAL;

   json_config target_obj;
   if(conf->config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_POLLING_TARGET, target_obj)) { // has target object
      #ifdef MAC_POLLING
      target_obj.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_TARGET_METHODS, polling_methods_);
      target_obj.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_TARGET_FMR_CONTROLLERS_MAX, max_fmr_controllers_);
      #endif
   }

   json_config generic_polling_obj;
   if(conf->config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_POLLING_HB_GENERIC_CONFIG, generic_polling_obj)) { // has heartbeat generic config object
      generic_polling_obj.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_HB_GENERIC_CONFIG_UPTIME_MULTIPLIER, controller_generic_polling_configuration_.uptime_multiplier);
      generic_polling_obj.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_HB_GENERIC_CONFIG_HB_TIME_TO_SAVE, controller_generic_polling_configuration_.hb_time_to_save);
      LOG_INFO("%s: Generic Polling Configuration from JSON: uptime_multiplier <%u>, hb_time_to_save <%u>\n", __FUNCTION__, controller_generic_polling_configuration_.uptime_multiplier, controller_generic_polling_configuration_.hb_time_to_save);
   }

   json_config polling_obj;
   bool has_default_config = conf->config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_POLLING_DEFAULT, polling_obj);
   if(has_default_config) { // has default object
      json_config polling_obj_hb;
      polling_obj.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_DEFAULT_METHODS, default_polling_methods);
      if(polling_obj.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_POLLING_DEFAULT_HEARTBEAT, polling_obj_hb)) {
         polling_obj_hb.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_DEFAULT_HEARTBEAT_TRIGGER,       default_polling_config_hb.trigger);
         polling_obj_hb.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_DEFAULT_HEARTBEAT_KP_COUNTER,    default_polling_config_hb.kp_counter);
         polling_obj_hb.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_DEFAULT_HEARTBEAT_TIME_INTERVAL, default_polling_config_hb.time_interval);
      }
      json_config polling_obj_mac;
      if(polling_obj.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_POLLING_DEFAULT_MAC, polling_obj_mac)) {
         polling_obj_mac.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_DEFAULT_MAC_TIME_INTERVAL, default_polling_config_mac.time_interval);
      }
      LOG_INFO("%s: Default Polling Configuration from JSON\n", __FUNCTION__);
   }

   for(int i = 0; i < RF4CE_CONTROLLER_TYPE_INVALID; i++) {
      LOG_INFO("%s: Polling Configuration Remote Type <%s>\n", __FUNCTION__, ctrlm_rf4ce_controller_type_str((ctrlm_rf4ce_controller_type_t)i));
      const char *controller_json_str  = ctrlm_rf4ce_controller_polling_configuration_str((ctrlm_rf4ce_controller_type_t)i);
      if(controller_json_str) {
         if(conf->config_object_get(controller_json_str,polling_obj)) {
            if(has_default_config) {
               controller_polling_methods_[i]                 = default_polling_methods;
               controller_polling_configuration_heartbeat_[i] = default_polling_config_hb;
               controller_polling_configuration_mac_[i]       = default_polling_config_mac;
            }
            polling_obj.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_DEFAULT_METHODS, controller_polling_methods_[i]);
            json_config polling_obj_hb;
            if(polling_obj.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_POLLING_DEFAULT_HEARTBEAT, polling_obj_hb)) {
               polling_obj_hb.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_DEFAULT_HEARTBEAT_TRIGGER,       controller_polling_configuration_heartbeat_[i].trigger);
               polling_obj_hb.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_DEFAULT_HEARTBEAT_KP_COUNTER,    controller_polling_configuration_heartbeat_[i].kp_counter);
               polling_obj_hb.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_DEFAULT_HEARTBEAT_TIME_INTERVAL, controller_polling_configuration_heartbeat_[i].time_interval);
            }
            json_config polling_obj_mac;
            if(polling_obj.config_object_get(JSON_OBJ_NAME_NETWORK_RF4CE_POLLING_DEFAULT_MAC, polling_obj_mac)) {
               polling_obj_mac.config_value_get(JSON_INT_NAME_NETWORK_RF4CE_POLLING_DEFAULT_MAC_TIME_INTERVAL, controller_polling_configuration_mac_[i].time_interval);
            }
            LOG_INFO("%s: Controller Polling Configuration from JSON\n", __FUNCTION__);
         }
      }
   }
}

gboolean ctrlm_obj_network_rf4ce_t::polling_configuration_by_controller_type(ctrlm_rf4ce_controller_type_t controller_type, guint8 *polling_methods, ctrlm_rf4ce_polling_configuration_t *configurations) {
   gboolean changed = false;
   errno_t safec_rc = -1;
   int ind = -1;
   if(polling_methods == NULL || configurations == NULL || controller_type == RF4CE_CONTROLLER_TYPE_INVALID) {
      LOG_ERROR("%s: Invalid Parameters!\n", __FUNCTION__);
      return(false);
   }

   // Polling Methods
   guint8 methods = (controller_polling_methods_[controller_type]);

   LOG_DEBUG("%s: polling methods current <%s> new <%s>\n", __FUNCTION__, ctrlm_rf4ce_controller_polling_methods_str(*polling_methods), ctrlm_rf4ce_controller_polling_methods_str(methods));
   if(*polling_methods != methods) {
      *polling_methods = methods;
      changed = true;
   }

   // Heartbeat Configuration
   if(&configurations[RF4CE_POLLING_METHOD_HEARTBEAT]) {
     safec_rc = memcmp_s(&configurations[RF4CE_POLLING_METHOD_HEARTBEAT], sizeof(ctrlm_rf4ce_polling_configuration_t), &controller_polling_configuration_heartbeat_[controller_type], CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION, &ind);
     ERR_CHK(safec_rc);
     if((safec_rc == EOK) && (ind != 0)) {
        safec_rc = memcpy_s(&configurations[RF4CE_POLLING_METHOD_HEARTBEAT], sizeof(ctrlm_rf4ce_polling_configuration_t), &controller_polling_configuration_heartbeat_[controller_type], CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
        ERR_CHK(safec_rc);
        changed = true;
     }
   }

   // Mac Configuration
   if(&configurations[RF4CE_POLLING_METHOD_MAC]) {
     safec_rc = memcmp_s(&configurations[RF4CE_POLLING_METHOD_MAC], sizeof(ctrlm_rf4ce_polling_configuration_t), &controller_polling_configuration_mac_[controller_type], CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION, &ind);
     ERR_CHK(safec_rc);
     if((safec_rc == EOK) && (ind != 0)) {
       safec_rc = memcpy_s(&configurations[RF4CE_POLLING_METHOD_MAC], sizeof(ctrlm_rf4ce_polling_configuration_t), &controller_polling_configuration_mac_[controller_type], CTRLM_RF4CE_RIB_ATTR_LEN_POLLING_CONFIGURATION);
       ERR_CHK(safec_rc);
       changed = true;
     }
   }

   return(changed);
}

void ctrlm_obj_network_rf4ce_t::notify_firmware(ctrlm_rf4ce_controller_type_t controller_type, ctrlm_rf4ce_device_update_image_type_t image_type, bool force_update, version_software_t version_software, version_hardware_t version_hardware_min, version_software_t version_bootloader_min) {
   LOG_INFO("%s: Notified of new image type %s for %s: v%u.%u.%u.%u\n", __FUNCTION__, ctrlm_rf4ce_device_update_image_type_str(image_type), ctrlm_rf4ce_controller_type_str(controller_type), version_software.major, version_software.minor, version_software.revision, version_software.patch);
   char data[1] = {(char)(image_type & 0xFF)};
   for (const auto& kv : controllers_) {
      if(controller_type == kv.second->controller_type_get() && ctrlm_device_update_rf4ce_is_hardware_version_min_met(kv.second->version_hardware_get(), version_hardware_min)) {
         LOG_INFO("%s: Controller %u is compatiable, checking versions and force flag\n", __FUNCTION__, kv.first);
         version_software_t version;

         switch(image_type) {
            case RF4CE_DEVICE_UPDATE_IMAGE_TYPE_FIRMWARE: version = kv.second->version_software_get(); break;
            case RF4CE_DEVICE_UPDATE_IMAGE_TYPE_DSP: version = kv.second->version_dsp_get(); break;
            case RF4CE_DEVICE_UPDATE_IMAGE_TYPE_KEYWORD_MODEL: version = kv.second->version_keyword_model_get(); break;
            case RF4CE_DEVICE_UPDATE_IMAGE_TYPE_AUDIO_DATA_1: version = kv.second->version_audio_data_get(); break;
            case RF4CE_DEVICE_UPDATE_IMAGE_TYPE_AUDIO_DATA_2: version = kv.second->version_audio_data_get(); break;
            default: continue;
         }
         if(force_update || ctrlm_device_update_rf4ce_is_software_version_higher(version, version_software)) {
            LOG_INFO("%s: Controller %u has new firmware image available!\n", __FUNCTION__, kv.first);
            ctrlm_rf4ce_polling_action_push(network_id_get(), kv.first, RF4CE_POLLING_ACTION_OTA, data, sizeof(data));
         }
      }
   }
}

template<bool (ctrlm_obj_network_rf4ce_t::*event_func)(void*,int)>
gboolean ctrlm_obj_network_rf4ce_t::reverse_cmd_event_timer_proc(gpointer user_data) {
   LOG_INFO("%s\n", __FUNCTION__);
   ctrlm_obj_network_rf4ce_t *This = (ctrlm_obj_network_rf4ce_t *)user_data;
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)event_func, (void *)0, 0, This);
   return FALSE;
}

ctrlm_controller_status_cmd_result_t  ctrlm_obj_network_rf4ce_t::req_process_reverse_cmd(ctrlm_main_queue_msg_rcu_reverse_cmd_t *dqm) {
   if(0 == (polling_methods_ & POLLING_METHODS_FLAG_MAC)) {
      LOG_INFO("%s : Mac polling is disabled\n", __FUNCTION__);
      dqm->reverse_command.cmd_result = CTRLM_RCU_REVERSE_CMD_DISABLED;
      return CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
   }

   if (!reverse_cmd_end_event_pending_.empty()) {
      LOG_INFO("%s : Previous reverse cmd pending\n", __FUNCTION__);
      dqm->reverse_command.cmd_result = CTRLM_RCU_REVERSE_CMD_FAILURE;
      return CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
   }

   ctrlm_main_iarm_call_rcu_reverse_cmd_t& reverse_command = dqm->reverse_command;

   if (reverse_command.cmd != CTRLM_RCU_REVERSE_CMD_FIND_MY_REMOTE) {
      LOG_INFO("%s : Only Find My Remote command is supported\n", __FUNCTION__);
      reverse_command.cmd_result = CTRLM_RCU_REVERSE_CMD_FAILURE;
      return CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
   }

   if (reverse_command.num_params != 2) {
      LOG_INFO("%s : Find My Remote expected 2 parameters; %d is provided\n", __FUNCTION__, reverse_command.num_params);
      reverse_command.cmd_result = CTRLM_RCU_REVERSE_CMD_FAILURE;
      return CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
   }

   // We need to find out controllers, supporting mac polling and sort them by last used time
   //
   // Less function object, comparing two controllers based on last used key time.
   // controller1 < controller2 when controller1 was used more recently
   struct less_time_since_last_keypress_op : binary_function <ctrlm_obj_controller_rf4ce_t *,ctrlm_obj_controller_rf4ce_t *,bool> {
      bool operator() (ctrlm_obj_controller_rf4ce_t *controller1, ctrlm_obj_controller_rf4ce_t *controller2) const {
           time_t time1, time2;
           controller1->time_last_key_get(&time1);
           controller2->time_last_key_get(&time2);
           return time1 > time2;
           };
   };
   // Use STL set<> sorting feature. when we add all the elements, they will be sorted already
   // controllers with mac polling support will be ordered by last used key time.
   // recent controllers are in the beginning of the set
   typedef std::set<ctrlm_obj_controller_rf4ce_t *, less_time_since_last_keypress_op> last_used_sorted_t;
   last_used_sorted_t last_used_sorted;
   for (auto& controller : controllers_) {
       ctrlm_obj_controller_rf4ce_t* controller_obj = controller.second;
       guchar methods = controller_obj->polling_methods_get();
       LOG_DEBUG("%s : type <%s> methods <%s>\n", __FUNCTION__, ctrlm_rf4ce_controller_type_str(controller_obj->controller_type_get()), ctrlm_rf4ce_controller_polling_methods_str(methods));
       if (methods & POLLING_METHODS_FLAG_MAC) {
          // make sure if Reverse command is FMR, controller supports it
          if (dqm->reverse_command.cmd == CTRLM_RCU_REVERSE_CMD_FIND_MY_REMOTE && !controller_obj->is_fmr_supported()) {
             LOG_DEBUG("%s : FMR not supported\n", __FUNCTION__);
             continue;
          }
          last_used_sorted.insert(controller_obj);
       }
    }

   if (last_used_sorted.empty()) {
      LOG_INFO("%s : No controllers supporting MAC polling found\n", __FUNCTION__);
      dqm->reverse_command.cmd_result = CTRLM_RCU_REVERSE_CMD_CONTROLLER_NOT_CAPABLE;
      return CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
   }

   char data[POLLING_RESPONSE_DATA_LEN] = {0};
   errno_t safec_rc = -1;

   // setting up parameters for polling action. They might be in any order in IARM command
   for (int i = 0; i < reverse_command.num_params; ++i) {
      int data_index = 0;
      switch(reverse_command.params_desc[i].param_id) {
         case CTRLM_RCU_FMR_ALERT_FLAGS_ID:{
            data_index = 0;
            break;
         }
         case CTRLM_FIND_RCU_FMR_ALERT_DURATION_ID: {
            data_index = 1;
            break;
         }
      }
      safec_rc = memcpy_s(&data[data_index], sizeof(data), &reverse_command.param_data[i], reverse_command.params_desc[i].size);
      ERR_CHK(safec_rc);
   }

   reverse_cmd_end_event_pending_.clear();
   // fill up reverse_cmd_begin_event_pending_ vector with controller ID reverse command will be triggered for
   if (dqm->controller_id == CTRLM_MAIN_CONTROLLER_ID_LAST_USED) { // last used supporting MAC polling
      LOG_INFO("%s : LAST USED CONTROLLER\n", __FUNCTION__);
      ctrlm_controller_id_t controller_id = (*last_used_sorted.begin())->controller_id_get(); // last used RC is in the top of the RC supporting mac polling set container
      reverse_cmd_end_event_pending_.push_back(controller_id);
   } else if (dqm->controller_id == CTRLM_MAIN_CONTROLLER_ID_ALL) {
      int num_controllers = (last_used_sorted.size() < max_fmr_controllers_ ? last_used_sorted.size() : max_fmr_controllers_);
      LOG_INFO("%s : ALL CONTROLLERS qty <%d> max <%d>\n", __FUNCTION__, num_controllers, max_fmr_controllers_);
      reverse_cmd_end_event_pending_.resize(num_controllers);
      // last used num_RCs is in the top of the RC supporting mac polling set container
      // extract controller IDs from controllers objects, preserving the sorted order
      auto controller_id_transform_func = [](ctrlm_obj_controller_rf4ce_t * controller) { return controller->controller_id_get();};
      auto last_used_sorted_new_end = last_used_sorted.begin();
      std::advance(last_used_sorted_new_end,num_controllers);
      std::transform(last_used_sorted.begin(), last_used_sorted_new_end, reverse_cmd_end_event_pending_.begin(), controller_id_transform_func);
   } else {
      LOG_INFO("%s : SPECIFIC CONTROLLER id <%u>\n", __FUNCTION__, dqm->controller_id);
      // (C++11) [&] --> lambda capture syntax. means: capture everything by reference to lambda function
      auto find_controller_id_func = [&](const ctrlm_obj_controller_rf4ce_t * controller) { return dqm->controller_id == controller->controller_id_get();};
      if (last_used_sorted.end() == std::find_if(last_used_sorted.begin(), last_used_sorted.end(), find_controller_id_func)) {
         dqm->reverse_command.cmd_result = CTRLM_RCU_REVERSE_CMD_CONTROLLER_NOT_FOUND;
         LOG_INFO("%s : Controller <%u> not found or does not support FMR\n", __FUNCTION__, dqm->controller_id);
         return CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
      }
      reverse_cmd_end_event_pending_.push_back(dqm->controller_id);
   }
   // at this point we reverse_cmd_end_event_pending_ vector vector initialized with controller's IDs reverse command will be triggered for
   for (auto controller_id:reverse_cmd_end_event_pending_) {
      ctrlm_rf4ce_polling_action_push(network_id_get(), controller_id, RF4CE_POLLING_ACTION_ALERT, data, sizeof(data));
   }
   if (!reverse_cmd_end_event_pending_.empty()) {
      guint begin_event_timeout = (guint)(JSON_INT_VALUE_NETWORK_RF4CE_POLLING_DEFAULT_MAC_TIME_INTERVAL);
      ctrlm_timeout_create(begin_event_timeout, reverse_cmd_event_timer_proc<&ctrlm_obj_network_rf4ce_t::reverse_cmd_begin_event>, this);
   }

  return CTRLM_CONTROLLER_STATUS_REQUEST_SUCCESS;
}

bool ctrlm_obj_network_rf4ce_t::reverse_cmd_begin_event(void*,int) {
   if (chime_timeout_ == 0) {
      chime_timeout_ = 15000;
   }
   reverse_cmd_end_event_timer_id_ = ctrlm_timeout_create(chime_timeout_, reverse_cmd_event_timer_proc<&ctrlm_obj_network_rf4ce_t::reverse_cmd_end_event>, this);
   return true;
}

bool ctrlm_obj_network_rf4ce_t::reverse_cmd_end_event(void*,int) {
   ctrlm_timestamp_t current_timestamp;
   ctrlm_timestamp_get(&current_timestamp);
   guint ms_from_beginning = (guint)(JSON_INT_VALUE_NETWORK_RF4CE_POLLING_DEFAULT_MAC_TIME_INTERVAL*1.2) + 15000;
   int num_controllers = 0;
   g_mutex_lock(&reverse_cmd_event_pending_mutex_);

   ctrlm_controller_id_t controller_id = CTRLM_HAL_CONTROLLER_ID_INVALID;
   ctrlm_rcu_reverse_cmd_result_t reverse_event_result = CTRLM_RCU_REVERSE_CMD_CONTROLLER_NOT_FOUND;
   for (auto end_event_controller_id : reverse_cmd_end_event_pending_) {
      // make sure controller was not unpaired after FMR was initiated
      auto controller = controllers_.find(end_event_controller_id);
      if (controller == controllers_.end()) {
         continue;
      }
      ++num_controllers;
      controller_id = end_event_controller_id;
      // get reverse command result based on lat heartbeat time
      time_t last_heartbeat_time = 0, current_time = time(NULL);
      controller->second->time_last_heartbeat_get(&last_heartbeat_time);
      guint ms_since_last_checkin = (guint)1000*(current_time - last_heartbeat_time);
      if (ms_since_last_checkin < ms_from_beginning) {
         // if we have more than obe RC, and at least one was found, set event result as found
         reverse_event_result = CTRLM_RCU_REVERSE_CMD_CONTROLLER_FOUND;
      }
   }

   reverse_cmd_end_event_pending_.clear();
   if (num_controllers > 0) {
      if (num_controllers > 1) {
         controller_id = CTRLM_MAIN_CONTROLLER_ID_ALL;
      }
      ctrlm_rcu_iarm_event_reverse_cmd(network_id_get(), controller_id, CTRLM_RCU_IARM_EVENT_RCU_REVERSE_CMD_END,reverse_event_result,0,NULL);
   }

   g_mutex_unlock(&reverse_cmd_event_pending_mutex_);
   reverse_cmd_end_event_timer_id_ = 0;
   return true;
}

bool ctrlm_obj_network_rf4ce_t::is_fmr_supported() const {
   if(0 == (polling_methods_ & POLLING_METHODS_FLAG_MAC)) {
      return false;
   }
   // check if at least one controller supports FMR
   for (const auto& controller : controllers_) {
      const ctrlm_obj_controller_rf4ce_t& controller_obj = *controller.second;
      if ((controller.second->polling_methods_get() & POLLING_METHODS_FLAG_MAC) && controller_obj.is_fmr_supported()) {
         return true;
      }
   }
   return false;
}

void ctrlm_obj_network_rf4ce_t::polling_action_push(void *data, int size) {
   ctrlm_main_queue_msg_rf4ce_polling_action_t *dqm = (ctrlm_main_queue_msg_rf4ce_polling_action_t *)data;
   chime_timeout_ = 0;
   errno_t safec_rc = -1;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_rf4ce_polling_action_t));

   ctrlm_rf4ce_polling_action_t action        = (ctrlm_rf4ce_polling_action_t)dqm->action;
   char *                       action_data   = dqm->data;
   ctrlm_controller_id_t        controller_id = dqm->controller_id;
   for (const auto& kv : controllers_) {
      if(controller_id == CTRLM_MAIN_CONTROLLER_ID_ALL || controller_id == kv.first) {
         ctrlm_rf4ce_polling_action_msg_t *msg = (ctrlm_rf4ce_polling_action_msg_t *)malloc(sizeof(ctrlm_rf4ce_polling_action_msg_t));
         if(NULL == msg) {
            LOG_ERROR("%s: Failed to allocate memory for polling action\n", __FUNCTION__);
            return;
         }
         msg->action = action;
         safec_rc = memcpy_s(msg->data, sizeof(msg->data), action_data, sizeof(msg->data));
         ERR_CHK(safec_rc);
         kv.second->polling_action_push(msg);
         if (action == RF4CE_POLLING_ACTION_ALERT) {
            if (chime_timeout_ == 0) {
               safec_rc = memcpy_s(&chime_timeout_, sizeof(unsigned short), &msg->data[1],  2);
               ERR_CHK(safec_rc);
            }
            ctrlm_timestamp_t timestamp;
            ctrlm_timestamp_get(&timestamp);
//            ctrlm_timestamp_add_ms(&timestamp, CTRLM_RF4CE_CONST_RESPONSE_IDLE_TIME);
            // Add Data to response
            guint8 response[3 + POLLING_RESPONSE_DATA_LEN] = {0};
            response[0] = 0x55; // value is for debugging purpose only. RCU doesn't read it, it checks size only
            ctrlm_hal_result_t result = req_data(CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU, kv.first, timestamp, 1, response, NULL, NULL, true);
            if (result != CTRLM_HAL_RESULT_SUCCESS) {
               LOG_ERROR("%s: result %s\n", __FUNCTION__, ctrlm_hal_result_str(result));
            }
         }
      }
   }
}

void ctrlm_obj_network_rf4ce_t::process_xconf() {
   int result;
   int value = 0;
   bool b_value = true;
   result = ctrlm_tr181_int_get(CTRLM_RF4CE_TR181_RF4CE_AUDIO_PROFILE_TARGET, &value, 1, 7);
   if(result != CTRLM_TR181_RESULT_SUCCESS) {
      LOG_INFO("%s: audio profile target not present\n", __FUNCTION__);
   } else {
      audio_profiles_targ_ = value;
      LOG_INFO("%s: audio profile target 0x%04X\n", __FUNCTION__, audio_profiles_targ_);
   }

   result = ctrlm_tr181_int_get(CTRLM_RF4CE_TR181_RF4CE_RSP_IDLE_FF, &value, 0, 1000);
   if(result != CTRLM_TR181_RESULT_SUCCESS) {
      LOG_INFO("%s: FF Rsp Idle time not present\n", __FUNCTION__);
   } else {
      response_idle_time_ff_ = value;
      LOG_INFO("%s: FF Rsp Idle time %u\n", __FUNCTION__, response_idle_time_ff_);
   }

   if(CTRLM_TR181_RESULT_SUCCESS != ctrlm_tr181_bool_get(CTRLM_RF4CE_TR181_RF4CE_VOICE_ENCRYPTION, &b_value)) {
      LOG_INFO("%s: TR181 RF4CE Voice Encryption not present\n", __FUNCTION__);
   } else {
      LOG_INFO("%s: TR181 RF4CE Voice Encryption set to %s\n", __FUNCTION__, (b_value ? "TRUE" : "FALSE"));
      voice_command_encryption_ = (b_value ? VOICE_COMMAND_ENCRYPTION_ENABLED : VOICE_COMMAND_ENCRYPTION_DISABLED);
   }

   if(CTRLM_TR181_RESULT_SUCCESS != ctrlm_tr181_bool_get(CTRLM_RF4CE_TR181_RF4CE_HOST_PACKET_DECRYPTION, &host_decryption_)) {
      LOG_INFO("%s: TR181 RF4CE Host Packet Decryption not present\n", __FUNCTION__);
   } else {
      LOG_INFO("%s: TR181 RF4CE Host Packet Decryption set to %s\n", __FUNCTION__, (host_decryption_ ? "TRUE" : "FALSE"));
   }
}

// ASB Functions
#ifdef ASB
bool ctrlm_obj_network_rf4ce_t::rf4ce_asb_init(void *data, int size) {
   if(asb_init()) {
      LOG_ERROR("%s: Failed to init ASB\n", __FUNCTION__);
      return false;
   }
   return true;   
}

bool ctrlm_obj_network_rf4ce_t::is_asb_force_settings() {
   return(asb_force_settings_);
}

bool ctrlm_obj_network_rf4ce_t::is_asb_enabled() {
   return(asb_enabled_);
}

void ctrlm_obj_network_rf4ce_t::asb_enable_set(bool asb_enabled) {
   asb_enabled_ = asb_enabled;
}

asb_key_derivation_method_t ctrlm_obj_network_rf4ce_t::key_derivation_method_get(asb_key_derivation_bitmask_t bitmask_controller) {
   asb_key_derivation_bitmask_t supported_methods = bitmask_controller & asb_key_derivation_methods_ & asb_key_derivation_methods_get();
   asb_key_derivation_method_t  ret = ASB_KEY_DERIVATION_NONE;
   if(ASB_KEY_DERIVATION_NONE != supported_methods) {
      // Get greatest support key derivation method.. Stored in least sig bit
      size_t i;
      for(i = 0; i < (sizeof(supported_methods) * CHAR_BIT); i++) {
         if(supported_methods & 0x01) {
            ret = 1 << i;
            return(ret);
         }
         supported_methods = supported_methods >> 1;
      }
   }
   return(ret);
}

void ctrlm_obj_network_rf4ce_t::controller_set_key_derivation_method(ctrlm_controller_id_t controller_id, asb_key_derivation_method_t method) {
   if(controller_exists(controller_id)) {
      controllers_[controller_id]->asb_key_derivation_method_set(method);
   }
}

void ctrlm_obj_network_rf4ce_t::asb_link_key_validation_timeout(void *data, int size) {
   ctrlm_controller_id_t *controller_id = (ctrlm_controller_id_t *)data;
   g_assert(controller_id);
   g_assert(size == sizeof(ctrlm_controller_id_t));

   if(ctrlm_pairing_window_active_get()) {
      ctrlm_pairing_window_bind_status_set(CTRLM_BIND_STATUS_ASB_FAILURE);
   }

   if(controller_exists(*controller_id)) {
       process_pair_result(*controller_id, controllers_[*controller_id]->ieee_address_get(), CTRLM_HAL_RESULT_PAIR_FAILURE);
       asb_fallback_count_ += 1;
   }
   // Destroy ASB lib 
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::rf4ce_asb_destroy, (void *)NULL, 0, this);
}

void ctrlm_obj_network_rf4ce_t::asb_link_key_derivation_perform(void *data, int size) {
   ctrlm_controller_id_t *controller_id = (ctrlm_controller_id_t *)data;
   g_assert(controller_id);
   g_assert(size == sizeof(ctrlm_controller_id_t));

   if(controller_exists(*controller_id)) {
      controllers_[*controller_id]->asb_key_derivation_perform();
   }
}

void ctrlm_obj_network_rf4ce_t::asb_configuration(json_config *conf) {
   int  temp_i;
   bool temp_b;
   // Get JSON configuration first
   if(NULL != conf) {
      conf->config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_ASB_ENABLE, asb_enabled_);
      conf->config_value_get(JSON_INT_NAME_NETWORK_RF4CE_ASB_DERIVATION_METHODS, asb_key_derivation_methods_, 0x01, 0xFF);
      conf->config_value_get(JSON_INT_NAME_NETWORK_RF4CE_ASB_FALLBACK_THRESHOLD, asb_fallback_count_threshold_, 0x01, 0xFF);
      conf->config_value_get(JSON_BOOL_NAME_NETWORK_RF4CE_ASB_FORCE_SETTINGS, asb_force_settings_);
   }

   // Now check TR181
   if(CTRLM_TR181_RESULT_SUCCESS == ctrlm_tr181_bool_get(CTRLM_RF4CE_TR181_ASB_ENABLED, &temp_b)) {
      asb_enabled_ = temp_b;
      LOG_INFO("%s: TR181 ASB Enable set to %s\n", __FUNCTION__, (asb_enabled_ ? "TRUE" : "FALSE"));
   }
   if(asb_enabled_) {
      if(CTRLM_TR181_RESULT_SUCCESS == ctrlm_tr181_int_get(CTRLM_RF4CE_TR181_ASB_DERIVATION_METHOD, &temp_i, 0x01, 0xFF)) {
         asb_key_derivation_methods_ = temp_i;
         LOG_INFO("%s: TR181 ASB Key Derivation Method set to %d\n", __FUNCTION__, asb_key_derivation_methods_);
      }
      if(CTRLM_TR181_RESULT_SUCCESS == ctrlm_tr181_int_get(CTRLM_RF4CE_TR181_ASB_FAIL_THRESHOLD, &temp_i, 0x01, 0xFF)) {
         asb_fallback_count_threshold_ = temp_i;
         LOG_INFO("%s: TR181 ASB Fallback Threshold set to %d\n", __FUNCTION__, asb_fallback_count_threshold_);
      }
   }
   
}

void ctrlm_obj_network_rf4ce_t::rf4ce_asb_destroy(void *data, int size) {
   asb_destroy();
}
#endif
// End ASB Functions

void ctrlm_obj_network_rf4ce_t::open_chime_enable_set(bool open_chime_enabled) {
   if(open_chime_enabled) {
      ff_configuration_.flags |= FAR_FIELD_CONFIGURATION_FLAGS_OPENING_CHIME; 
   } else {
      ff_configuration_.flags &= ~(FAR_FIELD_CONFIGURATION_FLAGS_OPENING_CHIME);
   }
}

bool ctrlm_obj_network_rf4ce_t::open_chime_enable_get() {
   return (ff_configuration_.flags & FAR_FIELD_CONFIGURATION_FLAGS_OPENING_CHIME ? true : false);
}

void ctrlm_obj_network_rf4ce_t::close_chime_enable_set(bool close_chime_enabled) {
   if(close_chime_enabled) {
      ff_configuration_.flags |= FAR_FIELD_CONFIGURATION_FLAGS_CLOSING_CHIME; 
   } else {
      ff_configuration_.flags &= ~(FAR_FIELD_CONFIGURATION_FLAGS_CLOSING_CHIME);
   }
}

bool ctrlm_obj_network_rf4ce_t::close_chime_enable_get() {
   return (ff_configuration_.flags & FAR_FIELD_CONFIGURATION_FLAGS_CLOSING_CHIME ? true : false);
}

void ctrlm_obj_network_rf4ce_t::privacy_chime_enable_set(bool privacy_chime_enabled) {
   if(privacy_chime_enabled) {
      ff_configuration_.flags |= FAR_FIELD_CONFIGURATION_FLAGS_PRIVACY_CHIME;
   } else {
      ff_configuration_.flags &= ~(FAR_FIELD_CONFIGURATION_FLAGS_PRIVACY_CHIME);
   }
}

bool ctrlm_obj_network_rf4ce_t::privacy_chime_enable_get() {
   return (ff_configuration_.flags & FAR_FIELD_CONFIGURATION_FLAGS_PRIVACY_CHIME ? true : false);
}

void ctrlm_obj_network_rf4ce_t::conversational_mode_set(unsigned char conversational_mode) {
   if(conversational_mode > 0) {
      ff_configuration_.flags |= FAR_FIELD_CONFIGURATION_FLAGS_CONVERSATIONAL; 
   } else {
      ff_configuration_.flags &= ~(FAR_FIELD_CONFIGURATION_FLAGS_CONVERSATIONAL);
   }
}

unsigned char ctrlm_obj_network_rf4ce_t::conversational_mode_get() {
   return (ff_configuration_.flags & FAR_FIELD_CONFIGURATION_FLAGS_CONVERSATIONAL ? 1 : 0);
}

void ctrlm_obj_network_rf4ce_t::chime_volume_set(ctrlm_chime_volume_t chime_volume) {
   switch(chime_volume) {
      case CTRLM_CHIME_VOLUME_LOW:    ff_configuration_.chime_volume = FAR_FIELD_CONFIGURATION_CHIME_VOLUME_LOW;    break;
      case CTRLM_CHIME_VOLUME_MEDIUM: ff_configuration_.chime_volume = FAR_FIELD_CONFIGURATION_CHIME_VOLUME_MEDIUM; break;
      case CTRLM_CHIME_VOLUME_HIGH:   ff_configuration_.chime_volume = FAR_FIELD_CONFIGURATION_CHIME_VOLUME_HIGH;   break;
      default:                        ff_configuration_.chime_volume = FAR_FIELD_CONFIGURATION_CHIME_VOLUME_MEDIUM; break;
   }
}

ctrlm_chime_volume_t ctrlm_obj_network_rf4ce_t::chime_volume_get() {
      switch(ff_configuration_.chime_volume) {
      case FAR_FIELD_CONFIGURATION_CHIME_VOLUME_LOW:    return CTRLM_CHIME_VOLUME_LOW;
      case FAR_FIELD_CONFIGURATION_CHIME_VOLUME_MEDIUM: return CTRLM_CHIME_VOLUME_MEDIUM;
      case FAR_FIELD_CONFIGURATION_CHIME_VOLUME_HIGH:   return CTRLM_CHIME_VOLUME_HIGH;
      default:                                          break;
   }
   return(CTRLM_CHIME_VOLUME_MEDIUM);
}

void ctrlm_obj_network_rf4ce_t::ir_command_repeats_set(unsigned char ir_command_repeats) {
   ff_configuration_.volume_ir_repeats = ir_command_repeats;
}

unsigned char ctrlm_obj_network_rf4ce_t::ir_command_repeats_get() {
   return ff_configuration_.volume_ir_repeats;
}

void ctrlm_obj_network_rf4ce_t::controller_set_binding_security_type(ctrlm_controller_id_t controller_id, ctrlm_rcu_binding_security_type_t type) {
   if(controller_exists(controller_id)) {
      controllers_[controller_id]->binding_security_type_set(type);
   }
}

void ctrlm_obj_network_rf4ce_t::process_voice_controller_metrics(void *data, int size) {
   ctrlm_main_queue_msg_controller_voice_metrics_t *dqm = (ctrlm_main_queue_msg_controller_voice_metrics_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_controller_voice_metrics_t));

#ifdef USE_VOICE_SDK
   THREAD_ID_VALIDATE();
   ctrlm_controller_id_t controller_id = dqm->controller_id;

   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Controller object doesn't exist for controller id %u!\n", __FUNCTION__, controller_id);
      return;
   }

   ctrlm_rf4ce_voice_utterance_type_t voice_utterance_type;
   if(dqm->short_utterance) {
      voice_utterance_type = RF4CE_VOICE_SHORT_UTTERANCE;
   } else {
      voice_utterance_type = RF4CE_VOICE_NORMAL_UTTERANCE;
   }
   controllers_[controller_id]->update_voice_metrics(voice_utterance_type, dqm->packets_total, dqm->packets_lost);
#else
   LOG_INFO("%s: NOT IMPLEMENTED\n", __FUNCTION__);
#endif
}

#ifdef USE_VOICE_SDK
void ctrlm_obj_network_rf4ce_t::ind_process_voice_session_request(void *data, int size) {
   ctrlm_main_queue_msg_voice_session_request_t *dqm = (ctrlm_main_queue_msg_voice_session_request_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_request_t));

   ctrlm_voice_session_response_status_t session;
   ctrlm_voice_device_t device_type  = (dqm->type == VOICE_SESSION_TYPE_FAR_FIELD) ? CTRLM_VOICE_DEVICE_FF : CTRLM_VOICE_DEVICE_PTT;
   ctrlm_voice_format_t voice_format = (ctrlm_voice_format_t)dqm->audio_format;
   gint16 offset       = 0;
   version_software_t sw_version_obj;
   version_hardware_t hw_version_obj;
   battery_status_t   battery_status;
   char sw_version[50] = {'\0'};
   char hw_version[50] = {'\0'};

   THREAD_ID_VALIDATE();

   if(!controller_exists(dqm->controller_id)) {
      LOG_INFO("%s: invalid controller id (%u)\n", __FUNCTION__, dqm->controller_id);

      // There will be no response so re-enable frequency agility as needed
      if(voice_session_active_count_ == 0) {
         ctrlm_hal_network_property_frequency_agility_t property;
         property.state = CTRLM_HAL_FREQUENCY_AGILITY_ENABLE;
         ctrlm_network_property_set(network_id_get(), CTRLM_HAL_NETWORK_PROPERTY_FREQUENCY_AGILITY, (void *)&property, sizeof(property));
      }
      return;
   }
   voice_session_active_count_++;

   sw_version_obj = controllers_[dqm->controller_id]->version_software_get();
   hw_version_obj = controllers_[dqm->controller_id]->version_hardware_get();
   battery_status = controllers_[dqm->controller_id]->battery_status_get();

   errno_t safec_rc = sprintf_s(sw_version, sizeof(sw_version), "%u.%u.%u.%u", sw_version_obj.major, sw_version_obj.minor, sw_version_obj.revision, sw_version_obj.patch);
   if(safec_rc < EOK) {
     ERR_CHK(safec_rc);
   }

   safec_rc = sprintf_s(hw_version, sizeof(hw_version), "%u.%u.%u.%u", hw_version_obj.manufacturer, hw_version_obj.model, hw_version_obj.hw_revision, hw_version_obj.lot_code);
   if(safec_rc < EOK) {
     ERR_CHK(safec_rc);
   }

   ctrlm_rf4ce_controller_type_t controller_type = controllers_[dqm->controller_id]->controller_type_get();
   if(controller_type == RF4CE_CONTROLLER_TYPE_XR19) { // TODO Remove this after XR19 supports type in session request
      device_type = CTRLM_VOICE_DEVICE_FF;
   }

   bool command_status = (controller_type == RF4CE_CONTROLLER_TYPE_XR11 ||
                          controller_type == RF4CE_CONTROLLER_TYPE_XR19) ? true : false;

   bool use_stream_params = (dqm->type == VOICE_SESSION_TYPE_FAR_FIELD && dqm->data_len == VOICE_SESSION_REQ_DATA_LEN_MAX); // Add stream params in the response

   voice_session_req_stream_params stream_params;
   if(use_stream_params) {
      stream_params.pre_keyword_sample_qty       = (dqm->data[3] << 24) | (dqm->data[2] << 16) | (dqm->data[1] << 8) | dqm->data[0];
      stream_params.keyword_sample_qty           = (dqm->data[7] << 24) | (dqm->data[6] << 16) | (dqm->data[5] << 8) | dqm->data[4];
      stream_params.doa                          = (dqm->data[9] <<  8) | dqm->data[8];
      stream_params.standard_search_pt_triggered = ((dqm->data[10] & 0x80) > 0);
      stream_params.standard_search_pt           =  dqm->data[10] & 0x7F; // only 7 bits, top bit represents if it was triggered
      stream_params.high_search_pt               =  dqm->data[11] & 0x7F; // only 7 bits, top bit represents if it was triggered
      stream_params.high_search_pt_support       = (stream_params.high_search_pt != 0x7F);
      stream_params.high_search_pt_triggered     = ((dqm->data[11] & 0x80) > 0);
      stream_params.dynamic_gain                 = (double)((int8_t)dqm->data[12]);
      // Sanity check (If using older device)
      if(!stream_params.high_search_pt_support) {
         stream_params.standard_search_pt_triggered = true;
      }

      // Get the beam info now
      uint8_t *beams = &dqm->data[13];
      for(int i = 0; i < 4; i++) {
         stream_params.beams[i].selected  = (beams[0] & 0x08 ? true : false);
         stream_params.beams[i].triggered = (beams[0] & 0x04 ? true : false);
         switch(beams[0] & 0x03) {
            case 0:  stream_params.beams[i].angle = 0;   break;
            case 1:  stream_params.beams[i].angle = 90;  break;
            case 2:  stream_params.beams[i].angle = 180; break;
            case 3:  stream_params.beams[i].angle = 270; break;
            default: stream_params.beams[i].angle = 0;   break;
         }
         stream_params.beams[i].confidence = (beams[2] << 8) | beams[1];
         stream_params.beams[i].confidence_normalized = false;
         if(RF4CE_CONTROLLER_TYPE_XR19 == controllers_[dqm->controller_id]->controller_type_get()) { // TODO Remove this after all XR19 versions deployed support this.
            version_software_t sw_normalized = {0x02, 0x06, 0x00, 0x06};
            if(controllers_[dqm->controller_id]->version_compare(controllers_[dqm->controller_id]->version_software_get(), sw_normalized) != -1) { // Normalized
               stream_params.beams[i].confidence /= 65535;
               stream_params.beams[i].confidence_normalized = true;
            }
         }
         stream_params.beams[i].snr = ((double)((int16_t)((beams[4] << 8) | beams[3]))) / 100.0;
         beams = &beams[5];
      }

      // Check if PTT mode from FF device
      if(stream_params.pre_keyword_sample_qty == 0 && stream_params.keyword_sample_qty == 0) {
         stream_params.push_to_talk = true;
      } else {
         stream_params.push_to_talk = false;
      }
      // TODO Handle beam fields
      LOG_INFO("%s: processing session request - type <%s> voice format <%s> pksq <%u> ksq <%u> doa <%u> sp <%u> sp triggered <%s> hsp <%u> hsp triggered <%s>\n", __FUNCTION__, ctrlm_voice_device_str(device_type), ctrlm_voice_format_str(voice_format), stream_params.pre_keyword_sample_qty, stream_params.keyword_sample_qty, stream_params.doa, stream_params.standard_search_pt, (stream_params.standard_search_pt_triggered ? "YES" : "NO"), (stream_params.high_search_pt_support ? stream_params.high_search_pt : 0), (stream_params.high_search_pt_support ? (stream_params.high_search_pt_triggered ? "YES" : "NO") : "N/A"));
      switch(stream_begin_) {
         case VOICE_SESSION_RESPONSE_STREAM_KEYWORD_BEGIN: {
            if((int32_t)stream_params.pre_keyword_sample_qty + stream_offset_ < 0) {
               offset = 0 - stream_params.pre_keyword_sample_qty;
            } else {
               offset = stream_offset_;
               stream_params.pre_keyword_sample_qty = (offset > 0 ? 0 : 0 - offset);
            }
            break;
         }
         case VOICE_SESSION_RESPONSE_STREAM_KEYWORD_END: {
            stream_params.pre_keyword_sample_qty = 0;
            stream_params.keyword_sample_qty     = 0;
            break;
         }
         case VOICE_SESSION_RESPONSE_STREAM_BUF_BEGIN: {
            if(stream_offset_ < 0) {
               offset = 0;
            } else if((int16_t)stream_offset_ > (int16_t)stream_params.pre_keyword_sample_qty) {
               offset = stream_params.pre_keyword_sample_qty;
            } else {
               offset = stream_offset_;
            }
            stream_params.pre_keyword_sample_qty = stream_params.pre_keyword_sample_qty - offset;
            break;
         }
      }
      LOG_INFO("%s: processing session request - type <%s> voice format <%s> pksq <%u> ksq <%u> doa <%u> sp <%u> sp triggered <%s> hsp <%u> hsp triggered <%s>\n", __FUNCTION__, ctrlm_voice_device_str(device_type), ctrlm_voice_format_str(voice_format), stream_params.pre_keyword_sample_qty, stream_params.keyword_sample_qty, stream_params.doa, stream_params.standard_search_pt, (stream_params.standard_search_pt_triggered ? "YES" : "NO"), (stream_params.high_search_pt_support ? stream_params.high_search_pt : 0), (stream_params.high_search_pt_support ? (stream_params.high_search_pt_triggered ? "YES" : "NO") : "N/A"));
   } else {
      LOG_INFO("%s: processing session request - type <%s> voice format <%s>\n", __FUNCTION__, ctrlm_voice_device_str(device_type), ctrlm_voice_format_str(voice_format));
   }

   ctrlm_hal_rf4ce_cfm_data_t        cb_confirm_rf4ce     = NULL;
   void *                            cb_confirm_param     = NULL;
   ctrlm_voice_session_rsp_confirm_t cb_confirm_voice_obj = NULL;

   session = ctrlm_get_voice_obj()->voice_session_req(network_id_get(),         dqm->controller_id,
                                                          device_type,              voice_format,
                                                          use_stream_params ? &stream_params : NULL,
                                                          controllers_[dqm->controller_id]->product_name_get(), 
                                                          sw_version,               hw_version, 
                                                          (((double)battery_status.voltage_loaded) *  4.0 / 255), command_status,
                                                          &dqm->timestamp, &cb_confirm_voice_obj, &cb_confirm_param);
   if(session == VOICE_SESSION_RESPONSE_AVAILABLE_PAR_VOICE) {
      if(controllers_[dqm->controller_id]->is_par_voice_supported()) {
         session = VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK_PAR_VOICE;
      } else {
         session = VOICE_SESSION_RESPONSE_AVAILABLE;
      }
   }
   if(session == VOICE_SESSION_RESPONSE_AVAILABLE) {
      session = VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK;
   }

   LOG_INFO("%s: Voice Session Response Status <%#x>\n", __FUNCTION__, session);

   // Send the response back to the HAL device
   guchar response[5];
   guchar response_len = 2;

   response[0] = MSO_VOICE_CMD_ID_VOICE_SESSION_RESPONSE;
   response[1] = session;
   if(use_stream_params && session == VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK) { // Add stream params in the response
      response[1] |= 0x80;
      response[2] = (guchar) stream_begin_;
      response[3] = (offset & 0xFF);
      response[4] = (offset >> 8);
      response_len = 5;
   }

   if(session == VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK_PAR_VOICE) {
      voice_params_par_t params;
      ctrlm_get_voice_obj()->voice_params_par_get(&params);

      LOG_INFO("%s: PAR Voice EOS data bytes timeout <%d> method <%d>\n", __FUNCTION__, params.par_voice_eos_timeout, params.par_voice_eos_method);
      response[2] = params.par_voice_eos_method;
      response[3] = (params.par_voice_eos_timeout & 0xFF);
      response[4] = (params.par_voice_eos_timeout >> 8);
      response_len = 5;
   }

   ctrlm_timestamp_t hal_timestamp = dqm->timestamp;

   // Determine when to send the response (50 ms after receipt)
   if(controller_type_get(dqm->controller_id) == RF4CE_CONTROLLER_TYPE_XR19) {
      ctrlm_timestamp_add_ms(&dqm->timestamp, response_idle_time_ff_);
   } else {
      ctrlm_timestamp_add_ms(&dqm->timestamp, CTRLM_RF4CE_CONST_RESPONSE_IDLE_TIME);
   }
   

   if(cb_confirm_voice_obj != NULL && (session == VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK ||
                                       session == VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK_PAR_VOICE)) { // Only confirm response for accepted session so there is only ever one response stored
      voice_session_rsp_confirm_       = cb_confirm_voice_obj;
      voice_session_rsp_confirm_param_ = cb_confirm_param;

      cb_confirm_rf4ce = ctrlm_network_rf4ce_cfm_voice_session_rsp;
      cb_confirm_param = voice_session_rsp_params_.network_id;

      // Store controller id, packet and timestamp for retransmission in case of send error
      voice_session_rsp_params_.controller_id   = dqm->controller_id;
      voice_session_rsp_params_.response_len    = response_len;
      voice_session_rsp_params_.timestamp_hal   = hal_timestamp;
      voice_session_rsp_params_.timestamp_begin = dqm->timestamp;
      voice_session_rsp_params_.timestamp_end   = dqm->timestamp;
      voice_session_rsp_params_.retries         = 0;
      ctrlm_timestamp_add_ms(&voice_session_rsp_params_.timestamp_end, CTRLM_RF4CE_CONST_RESPONSE_WAIT_TIME);
      safec_rc = memcpy_s(&voice_session_rsp_params_.response, sizeof(voice_session_rsp_params_.response),response, response_len);
      ERR_CHK(safec_rc);
      ctrlm_timestamp_get(&voice_session_rsp_params_.timestamp_rsp_req);
   }

   req_data(CTRLM_RF4CE_PROFILE_ID_VOICE, dqm->controller_id, dqm->timestamp, response_len, response, NULL, NULL, false, true, cb_confirm_rf4ce, cb_confirm_param);

   LOG_INFO("%s: session response delivered\n", __FUNCTION__);

   if(session != VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK           && session != VOICE_SESSION_RESPONSE_AVAILABLE &&
      session != VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK_PAR_VOICE && session != VOICE_SESSION_RESPONSE_AVAILABLE_PAR_VOICE) {
      voice_session_active_count_--;
      if(voice_session_active_count_ == 0) { // Re-enable frequency agility if the no other active RF4CE voice sessions
         ctrlm_hal_network_property_frequency_agility_t property;
         property.state = CTRLM_HAL_FREQUENCY_AGILITY_ENABLE;
         ctrlm_network_property_set(network_id_get(), CTRLM_HAL_NETWORK_PROPERTY_FREQUENCY_AGILITY, (void *)&property, sizeof(property));
      }
   }

   if(dqm->status != VOICE_SESSION_RESPONSE_AVAILABLE && dqm->status != VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK) { // Session was aborted
      LOG_INFO("%s: voice session abort\n", __FUNCTION__);

      // // Broadcast the event over the iarm bus
      // ctrlm_voice_iarm_event_session_abort_t event;
      // event.api_revision  = CTRLM_VOICE_IARM_BUS_API_REVISION;
      // event.network_id    = dqm->header.network_id;
      // event.network_type  = ctrlm_network_type_get(dqm->header.network_id);
      // event.controller_id = dqm->controller_id;
      // event.session_id    = ctrlm_voice_session_id_get_next();
      // event.reason        = dqm->reason;

      // ctrlm_voice_iarm_event_session_abort(&event);
   }
}

void ctrlm_network_rf4ce_cfm_voice_session_rsp(ctrlm_hal_rf4ce_result_t result, void *user_data) {
   if(user_data == NULL) {
      return;
   }
   ctrlm_network_id_t network_id = *(ctrlm_network_id_t *)user_data;
   ctrlm_main_queue_msg_voice_session_response_confirm_t msg;
   errno_t safec_rc = memset_s(&msg, sizeof(msg), 0, sizeof(msg));
   ERR_CHK(safec_rc);
   msg.result = result;
   rdkx_timestamp_get_realtime(&msg.timestamp);

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::cfm_voice_session_rsp, &msg, sizeof(msg), NULL, network_id);
}

void ctrlm_obj_network_rf4ce_t::cfm_voice_session_rsp(void *data, int size) {
   ctrlm_main_queue_msg_voice_session_response_confirm_t *dqm = (ctrlm_main_queue_msg_voice_session_response_confirm_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_response_confirm_t));

   bool b_result = true;
   if(dqm->result != CTRLM_HAL_RF4CE_RESULT_SUCCESS) {
       if(ctrlm_timestamp_until_ms(voice_session_rsp_params_.timestamp_end) > 0) { // Still within transmission window.  Retransmit the packet.
          voice_session_rsp_params_.retries++;
          req_data(CTRLM_RF4CE_PROFILE_ID_VOICE, voice_session_rsp_params_.controller_id, voice_session_rsp_params_.timestamp_begin, voice_session_rsp_params_.response_len, voice_session_rsp_params_.response, NULL, NULL, false, true, ctrlm_network_rf4ce_cfm_voice_session_rsp, voice_session_rsp_params_.network_id);
          LOG_ERROR("%s: result <%s> session response retransmitted\n", __FUNCTION__, ctrlm_hal_rf4ce_result_str(dqm->result));
          return;
       }
       ctrlm_timestamp_t now;
       ctrlm_timestamp_get(&now);

       double loadavg[3] = { -1, -1, -1 };
       getloadavg(loadavg, 3);
       struct sysinfo s_info;
       if(sysinfo(&s_info) != 0) {
           s_info.uptime = 0;
       }
       ctrlm_controller_id_t controller_id = voice_session_rsp_params_.controller_id;
       float voltage_loaded   = 0.0;
       float voltage_unloaded = 0.0;
       if(controller_exists(controller_id)) {
           battery_status_t battery_status = controllers_[voice_session_rsp_params_.controller_id]->battery_status_get();
           voltage_loaded   = battery_status.voltage_loaded * 4.0 / 255;
           voltage_unloaded = battery_status.voltage_unloaded * 4.0 / 255;
       }
       unsigned long session_id = ctrlm_get_voice_obj()->voice_session_id_get();
       LOG_ERROR("%s: result <%s> session response transmission failure\n", __FUNCTION__, ctrlm_hal_rf4ce_result_str(dqm->result));
       LOG_ERROR("%s: packet recv to data_req <%lldms>, packet recv to now <%lldms>, retries <%u> load avg <%5.2f, %5.2f, %5.2f> type <%s> voltage <%4.2f, %4.2f> uptime <%lu> session id <%u>\n", __FUNCTION__, ctrlm_timestamp_subtract_ms(voice_session_rsp_params_.timestamp_hal, voice_session_rsp_params_.timestamp_rsp_req), ctrlm_timestamp_subtract_ms(voice_session_rsp_params_.timestamp_hal, now), voice_session_rsp_params_.retries, loadavg[0], loadavg[1], loadavg[2], ctrlm_rf4ce_controller_type_str(controller_type_get(controller_id)), voltage_loaded, voltage_unloaded, s_info.uptime, session_id);
       b_result = false;
   }

   // Session response transmission is confirmed
   if(voice_session_rsp_confirm_ != NULL) {
      (*voice_session_rsp_confirm_)(b_result, &dqm->timestamp, voice_session_rsp_confirm_param_);
      voice_session_rsp_confirm_       = NULL;
      voice_session_rsp_confirm_param_ = NULL;
   }
}

void ctrlm_obj_network_rf4ce_t::ind_process_voice_session_stop(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_voice_session_stop_t *dqm = (ctrlm_main_queue_msg_voice_session_stop_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_stop_t));

   ctrlm_get_voice_obj()->voice_session_end(network_id_get(), dqm->controller_id, dqm->session_end_reason, &dqm->timestamp);
}

void ctrlm_obj_network_rf4ce_t::ind_process_voice_session_end(void *data, int size) {
   THREAD_ID_VALIDATE();
   ctrlm_main_queue_msg_voice_session_end_t *dqm = (ctrlm_main_queue_msg_voice_session_end_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_end_t));
   
   ctrlm_controller_id_t controller_id = dqm->controller_id;
   if(!controller_exists(controller_id)) {
      LOG_ERROR("%s: Controller object doesn't exist for controller id %u!\n", __FUNCTION__, controller_id);
   } else {
      //Voice command will update the last key info
      ctrlm_update_last_key_info(controller_id, IARM_BUS_IRMGR_KEYSRC_RF, KED_PUSH_TO_TALK, controllers_[controller_id]->product_name_get(), false, true);

      // Update the time last key
      controllers_[controller_id]->time_last_key_update();
   }

   voice_session_active_count_--;
   
   if(voice_session_active_count_ == 0) {
      ctrlm_hal_network_property_frequency_agility_t property;
      property.state = CTRLM_HAL_FREQUENCY_AGILITY_ENABLE;
      ctrlm_network_property_set(network_id_get(), CTRLM_HAL_NETWORK_PROPERTY_FREQUENCY_AGILITY, (void *)&property, sizeof(property));
   }
}
#endif

void ctrlm_obj_network_rf4ce_t::set_timers() {
   for(auto it = controllers_.begin(); it != controllers_.end(); it++) {
      it->second->handle_voice_configuration();
      it->second->handle_controller_metrics();
      it->second->handle_controller_battery_status();
   }
}

void ctrlm_obj_network_rf4ce_t::xconf_configuration() {
   if(FALSE == force_dsp_configuration_) {
      dsp_configuration_xconf();
   } else {
      LOG_WARN("%s: force dsp configuration is true, tell device(s) to read it\n", __FUNCTION__);
      update_far_field_configuration(RF4CE_POLLING_ACTION_DSP_CONFIGURATION);
   }
}

guchar ctrlm_obj_network_rf4ce_t::property_read_far_field_configuration(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_CONFIGURATION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = memset_s(data, length, 0, length);
   ERR_CHK(safec_rc);

   data[0] = ff_configuration_.flags;
   data[1] = ff_configuration_.chime_volume;
   data[2] = ff_configuration_.volume_ir_repeats;

   LOG_INFO("%s: Flags <%02X>, Chime Volume <%ddB>, IR Repeats <%u>\n", __FUNCTION__, ff_configuration_.flags, ff_configuration_.chime_volume, ff_configuration_.volume_ir_repeats);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_CONFIGURATION);
}

guchar ctrlm_obj_network_rf4ce_t::property_write_far_field_configuration(guchar *data, guchar length) {
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_CONFIGURATION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = memset_s(&ff_configuration_, sizeof(ff_configuration_), 0, sizeof(ff_configuration_));
   ERR_CHK(safec_rc);

   ff_configuration_.flags             = data[0];
   ff_configuration_.chime_volume      = data[1];
   ff_configuration_.volume_ir_repeats = data[2];

   LOG_INFO("%s: Flags <%02X>, Chime Volume <%ddB>, IR Repeats <%u>\n", __FUNCTION__, ff_configuration_.flags, ff_configuration_.chime_volume, ff_configuration_.volume_ir_repeats);

   return(CTRLM_RF4CE_RIB_ATTR_LEN_FAR_FIELD_CONFIGURATION);
}

guchar ctrlm_obj_network_rf4ce_t::property_read_dsp_configuration(guchar *data, guchar length) {
   stringstream ss;
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DSP_CONFIGURATION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = memset_s(data, length, 0, length);
   ERR_CHK(safec_rc);

   data[0] = dsp_configuration_.flags;
   data[1] = dsp_configuration_.vad_threshold;
   data[2] = dsp_configuration_.no_vad_threshold;
   data[3] = dsp_configuration_.vad_hang_time;
   data[4] = dsp_configuration_.initial_eos_timeout;
   data[5] = dsp_configuration_.eos_timeout;
   data[6] = dsp_configuration_.initial_speech_delay;
   data[7] = dsp_configuration_.primary_keyword_sensitivity;
   data[8] = dsp_configuration_.secondary_keyword_sensitivity;
   data[9] = dsp_configuration_.beamformer_type;
   data[10] = dsp_configuration_.noise_reduction_aggressiveness;
   data[11] = dsp_configuration_.dynamic_gain_target_level;
   data[12] = dsp_configuration_.ic_config_atten_update;
   data[13] = dsp_configuration_.ic_config_detect;

   ss << "Interference Canceller <" << (dsp_configuration_.flags & RF4CE_DSP_CONFIGURATION_FLAG_IC ? "ENABLED" : "DISABLED") << ">, ";
   ss << "IC Max Attenuation <";           if(RF4CE_DSP_CONFIGURATION_MAX_ATTENUATION_IS_DEFAULT(dsp_configuration_.ic_config_atten_update))  { ss << "DEFAULT"; } else { ss << RF4CE_DSP_CONFIGURATION_MAX_ATTENUATION_PRINT(dsp_configuration_.ic_config_atten_update);  } ss << ">, ";
   ss << "IC Update Threshold <";          if(RF4CE_DSP_CONFIGURATION_UPDATE_THRESHOLD_IS_DEFAULT(dsp_configuration_.ic_config_atten_update)) { ss << "DEFAULT"; } else { ss << RF4CE_DSP_CONFIGURATION_UPDATE_THRESHOLD_PRINT(dsp_configuration_.ic_config_atten_update); } ss << ">, ";
   ss << "IC TV Beam Detect Threshold <";  if(RF4CE_DSP_CONFIGURATION_DETECT_THRESHOLD_IS_DEFAULT(dsp_configuration_.ic_config_detect))       { ss << "DEFAULT"; } else { ss << RF4CE_DSP_CONFIGURATION_DETECT_THRESHOLD_PRINT(dsp_configuration_.ic_config_detect);       } ss << ">";

   LOG_INFO("%s: Flags <%02X>, VAD Threshold <%.04f>, No VAD Threshold <%.04f>, VAD Hang Time <%u>, Initial EOS Timeout <%u>, EOS Timeout <%u>, Initial Speech Delay <%u>, Primary KW Sensitvity <%u>, Secondary KW Sensitivity <%u>, Beamformer Type <%u>, Noise Reduction Aggressiveness <%u>, Dynamic Gain Target <%d>, %s\n", 
           __FUNCTION__, dsp_configuration_.flags, Q_NOTATION_TO_DOUBLE(dsp_configuration_.vad_threshold, 8), Q_NOTATION_TO_DOUBLE(dsp_configuration_.no_vad_threshold, 8), dsp_configuration_.vad_hang_time,
           dsp_configuration_.initial_eos_timeout, dsp_configuration_.eos_timeout, dsp_configuration_.initial_speech_delay, dsp_configuration_.primary_keyword_sensitivity, dsp_configuration_.secondary_keyword_sensitivity,
           dsp_configuration_.beamformer_type, dsp_configuration_.noise_reduction_aggressiveness, dsp_configuration_.dynamic_gain_target_level,
           ss.str().c_str());
   return(CTRLM_RF4CE_RIB_ATTR_LEN_DSP_CONFIGURATION);
}

guchar ctrlm_obj_network_rf4ce_t::property_write_dsp_configuration(guchar *data, guchar length) {
   stringstream ss;
   if(length != CTRLM_RF4CE_RIB_ATTR_LEN_DSP_CONFIGURATION) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }

   errno_t safec_rc = memset_s(&dsp_configuration_, sizeof(dsp_configuration_), 0, sizeof(dsp_configuration_));
   ERR_CHK(safec_rc);

   dsp_configuration_.flags                         = data[0];
   dsp_configuration_.vad_threshold                 = data[1];
   dsp_configuration_.no_vad_threshold              = data[2];
   dsp_configuration_.vad_hang_time                 = data[3];
   dsp_configuration_.initial_eos_timeout           = data[4];
   dsp_configuration_.eos_timeout                   = data[5];
   dsp_configuration_.initial_speech_delay          = data[6];
   dsp_configuration_.primary_keyword_sensitivity    = data[7];
   dsp_configuration_.secondary_keyword_sensitivity = data[8];
   dsp_configuration_.beamformer_type               = data[9];
   dsp_configuration_.noise_reduction_aggressiveness = data[10];
   dsp_configuration_.dynamic_gain_target_level     = data[11];
   dsp_configuration_.ic_config_atten_update        = data[12];
   dsp_configuration_.ic_config_detect              = data[13];

   ctrlm_db_rf4ce_write_dsp_configuration_xr19(network_id_get(), data, length);
   update_far_field_configuration(RF4CE_POLLING_ACTION_DSP_CONFIGURATION);

   ss << "Interference Canceller <" << (dsp_configuration_.flags & RF4CE_DSP_CONFIGURATION_FLAG_IC ? "ENABLED" : "DISABLED") << ">, ";
   ss << "IC Max Attenuation <";           if(RF4CE_DSP_CONFIGURATION_MAX_ATTENUATION_IS_DEFAULT(dsp_configuration_.ic_config_atten_update))  { ss << "DEFAULT"; } else { ss << RF4CE_DSP_CONFIGURATION_MAX_ATTENUATION_PRINT(dsp_configuration_.ic_config_atten_update);  } ss << ">, ";
   ss << "IC Update Threshold <";          if(RF4CE_DSP_CONFIGURATION_UPDATE_THRESHOLD_IS_DEFAULT(dsp_configuration_.ic_config_atten_update)) { ss << "DEFAULT"; } else { ss << RF4CE_DSP_CONFIGURATION_UPDATE_THRESHOLD_PRINT(dsp_configuration_.ic_config_atten_update); } ss << ">, ";
   ss << "IC TV Beam Detect Threshold <";  if(RF4CE_DSP_CONFIGURATION_DETECT_THRESHOLD_IS_DEFAULT(dsp_configuration_.ic_config_detect))       { ss << "DEFAULT"; } else { ss << RF4CE_DSP_CONFIGURATION_DETECT_THRESHOLD_PRINT(dsp_configuration_.ic_config_detect);       } ss << ">";

   LOG_INFO("%s: Flags <%02X>, VAD Threshold <%.04f>, No VAD Threshold <%.04f>, VAD Hang Time <%u>, Initial EOS Timeout <%u>, EOS Timeout <%u>, Initial Speech Delay <%u>, Primary KW Sensitvity <%u>, Secondary KW Sensitivity <%u>, Beamformer Type <%u>, Noise Reduction Aggressiveness <%u>, Dynamic Gain Target <%d>, %s\n", 
           __FUNCTION__, dsp_configuration_.flags, Q_NOTATION_TO_DOUBLE(dsp_configuration_.vad_threshold, 8), Q_NOTATION_TO_DOUBLE(dsp_configuration_.no_vad_threshold, 8), dsp_configuration_.vad_hang_time,
           dsp_configuration_.initial_eos_timeout, dsp_configuration_.eos_timeout, dsp_configuration_.initial_speech_delay, dsp_configuration_.primary_keyword_sensitivity, dsp_configuration_.secondary_keyword_sensitivity,
           dsp_configuration_.beamformer_type, dsp_configuration_.noise_reduction_aggressiveness, dsp_configuration_.dynamic_gain_target_level,
           ss.str().c_str());
   return(CTRLM_RF4CE_RIB_ATTR_LEN_DSP_CONFIGURATION);
}

void ctrlm_obj_network_rf4ce_t::update_far_field_configuration(ctrlm_rf4ce_polling_action_t action) {
   for(auto itr = controllers_.begin(); itr != controllers_.end(); itr++) {
      switch(itr->second->controller_type_get()) {
         case RF4CE_CONTROLLER_TYPE_XR19: {
            ctrlm_rf4ce_polling_action_push(network_id_get(), itr->first, action, NULL, 0);
            break;
         }
         default: {
            break;
         }
      }
   }
}

void ctrlm_obj_network_rf4ce_t::dsp_configuration_xconf() {
   char   rfc_val[100]    = {'\0'};
   unsigned char *decoded_buf     = NULL;
   size_t decoded_buf_len = 0;
   if(force_dsp_configuration_) {
      LOG_WARN("%s: not going to xconf for DSP configuration\n", __FUNCTION__);
      return;
   }
   if(CTRLM_TR181_RESULT_SUCCESS == ctrlm_tr181_string_get(CTRLM_RF4CE_TR181_XR19_DSP_CONFIGURATION, rfc_val, sizeof(rfc_val))) {
      decoded_buf = g_base64_decode(rfc_val, &decoded_buf_len);
      if(decoded_buf) {
         if(decoded_buf_len == CTRLM_RF4CE_RIB_ATTR_LEN_DSP_CONFIGURATION) {
            LOG_INFO("%s: DSP configuration taken from XCONF\n", __FUNCTION__);
            property_write_dsp_configuration(decoded_buf, (uint8_t)decoded_buf_len);
         } else {
            LOG_WARN("%s: incorrect length\n", __FUNCTION__);
         }
         free(decoded_buf);
      } else {
         LOG_WARN("%s: failed to decode base64\n", __FUNCTION__);
      }
   } else {
      LOG_INFO("%s: no rfc value\n", __FUNCTION__);
   }
}

void ctrlm_obj_network_rf4ce_t::attributes_from_db() {
   // So far just getting the DSP configuration
   if(!force_dsp_configuration_) {
      unsigned char *data = NULL;
      unsigned int length = 0;

      ctrlm_db_rf4ce_read_dsp_configuration_xr19(network_id_get(), &data, &length);
      if(data) {
         property_write_dsp_configuration(data, length);
         ctrlm_db_free(data);
      }
   } else {
      LOG_WARN("%s: not reading dsp configuration from DB\n", __FUNCTION__);
   }
}

vector<rf4ce_device_update_session_resume_info_t> *ctrlm_obj_network_rf4ce_t::device_update_session_resume_list_get() {
   vector<rf4ce_device_update_session_resume_info_t> *sessions = NULL;
   map<ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it_map;
   for(it_map = controllers_.begin(); it_map != controllers_.end(); it_map++) {
      rf4ce_device_update_session_resume_info_t info;
      if(!it_map->second->device_update_session_resume_load(&info)) {
         continue;
      }

      if(sessions == NULL) {
         sessions = new vector<rf4ce_device_update_session_resume_info_t>;
         if(sessions == NULL) {
            LOG_ERROR("%s: out of memory\n", __FUNCTION__);
            break;
         }
      }
      // Add session to list
      sessions->push_back(info);
   }
   return(sessions);
}

guint32 ctrlm_obj_network_rf4ce_t::device_update_session_timeout_get() {
   return(device_update_session_timeout_);
}

ctrlm_hal_result_t ctrlm_obj_network_rf4ce_t::network_init(GThread *ctrlm_main_thread) {
   ctrlm_hal_result_t result = CTRLM_HAL_RESULT_SUCCESS;

   // Read the ir_rf_database
   ir_rf_database_read_from_db();
   attributes_from_db();
   // Load controllers for this network
   controllers_load();
   // Read the target_irdb_status from the db.  If not there, use the last programmed controller irdb status
   if(!(target_irdb_status_read_from_db())) {
      target_irdb_status_set(most_recent_controller_irdb_status_get());
   }

   // Call the base class function
   result = ctrlm_obj_network_t::network_init(ctrlm_main_thread);

   if(CTRLM_HAL_RESULT_SUCCESS == result) {      
      vector<rf4ce_device_update_session_resume_info_t> *sessions = device_update_session_resume_list_get();
      if(sessions != NULL) {
         ctrlm_device_update_rf4ce_session_resume(sessions);
      }
   }
   return(result);
}
 
void ctrlm_obj_network_rf4ce_t::network_destroy() {
   // Load controllers for this network
   for(map <ctrlm_controller_id_t, ctrlm_obj_controller_rf4ce_t *>::iterator it = controllers_.begin(); it != controllers_.end(); it++) {
      if(it->second != NULL) {
         it->second->controller_destroy();
      }
   }

   // Call the base class function
   ctrlm_obj_network_t::network_destroy();
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_cfm_init(ctrlm_network_id_t network_id, ctrlm_hal_rf4ce_cfm_init_params_t params) {
   if(!ctrlm_network_id_is_valid(network_id)) {
      LOG_ERROR("%s: Invalid network id %u\n", __FUNCTION__, network_id);
      g_assert(0);
      return(CTRLM_HAL_RESULT_ERROR_NETWORK_ID);
   }
   LOG_INFO("%s: result %s\n", __FUNCTION__, ctrlm_hal_result_str(params.result));
   // Signal completion of the operation
   sem_t semaphore;
   sem_init(&semaphore, 0, 0);

   ctrlm_main_queue_msg_hal_cfm_init_t msg;
   errno_t safec_rc = memset_s(&msg, sizeof(msg), 0, sizeof(msg));
   ERR_CHK(safec_rc);

   msg.params.rf4ce = params;
   msg.semaphore    = &semaphore;
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::hal_init_cfm, &msg, sizeof(msg), NULL, network_id);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);

   return(params.result);
}

void ctrlm_obj_network_rf4ce_t::hal_init_cfm(void *data, int size) {
   ctrlm_main_queue_msg_hal_cfm_init_t *dqm = (ctrlm_main_queue_msg_hal_cfm_init_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_hal_cfm_init_t));

   if(dqm->params.rf4ce.result == CTRLM_HAL_RESULT_SUCCESS) {
      hal_api_set(dqm->params.rf4ce.property_get, dqm->params.rf4ce.property_set, dqm->params.rf4ce.term);
      hal_rf4ce_api_set(dqm->params.rf4ce.pair, dqm->params.rf4ce.unpair, dqm->params.rf4ce.data, dqm->params.rf4ce.rib_data_import, dqm->params.rf4ce.rib_data_export);
   }

   hal_init_confirm(dqm->params.rf4ce);

   ctrlm_obj_network_t::hal_init_cfm(data, size);
}

void ctrlm_obj_network_rf4ce_t::req_process_network_status(void *data, int size) {
   ctrlm_main_queue_msg_main_network_status_t *dqm = (ctrlm_main_queue_msg_main_network_status_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_main_network_status_t));
   g_assert(dqm->cmd_result);

   ctrlm_network_status_rf4ce_t *status_rf4ce  = &dqm->status->status.rf4ce;
   errno_t safec_rc = strncpy_s(status_rf4ce->version_hal, sizeof(status_rf4ce->version_hal), version_get(), CTRLM_MAIN_VERSION_LENGTH-1);
   ERR_CHK(safec_rc);
   status_rf4ce->version_hal[CTRLM_MAIN_VERSION_LENGTH - 1] = '\0';
   safec_rc = strncpy_s(status_rf4ce->chipset, sizeof(status_rf4ce->chipset), chipset_get(), CTRLM_MAIN_MAX_CHIPSET_LENGTH-1);
   ERR_CHK(safec_rc);
   status_rf4ce->chipset[CTRLM_MAIN_MAX_CHIPSET_LENGTH - 1] = '\0';

   int index = 0;
   for(auto const &itr : controllers_) {
      //If the validation result is not success, then this remote has not finished pairing.  Do not send it's status.
      if(itr.second->validation_result_get() == CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) {
         status_rf4ce->controllers[index] = itr.first;
         index++;
         if(index >= CTRLM_MAIN_MAX_BOUND_CONTROLLERS) {
            break;
         }
      } else {
         LOG_WARN("%s: Controller <%u> is pending.  Ignoring.\n", __FUNCTION__, itr.first);
      }
   }
   status_rf4ce->controller_qty = index;
   LOG_INFO("%s: HAL Version <%s> Controller Qty %u\n", __FUNCTION__, status_rf4ce->version_hal, status_rf4ce->controller_qty);
   pan_id_get(&status_rf4ce->pan_id);
   ieee_address_get(&status_rf4ce->ieee_address);
   short_address_get(&status_rf4ce->short_address);
   ctrlm_rf4ce_rf_channel_info_t rf_channel_info;
   rf_channel_info_get(&rf_channel_info);
   status_rf4ce->rf_channel_active.number  = rf_channel_info.rf_channel_number;
   status_rf4ce->rf_channel_active.quality = rf_channel_info.rf_channel_quality;
   dqm->status->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
   *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;

   ctrlm_obj_network_t::req_process_network_status(data, size);
}

void ctrlm_obj_network_rf4ce_t::req_process_chip_status(void *data, int size) {
   ctrlm_main_queue_msg_main_chip_status_t *dqm = (ctrlm_main_queue_msg_main_chip_status_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_main_chip_status_t));
   g_assert(dqm->status != NULL);

#if (CTRLM_HAL_RF4CE_API_VERSION >= 15)
#ifndef  CTRLM_RF4CE_CHIP_CONNECTIVITY_CHECK_NOT_SUPPORTED
      ctrlm_hal_network_property_network_stats_t network_stats;
      network_stats.ieee_address = 0;  
      ctrlm_hal_result_t result = property_get(CTRLM_HAL_NETWORK_PROPERTY_NETWORK_STATS, (void **)&network_stats);
      if(result == CTRLM_HAL_RESULT_SUCCESS) { // Update cache on successful HAL call
      // validate MAC address
      // Qorvo MAC address range is 00;15;5F:xx;xx;xx;xx;xx. A valid MAC address should match the OUI i.e. MSB 3 bytes with Qorvo/Greenpeak.
      // 0xA5 is default value read by SPI FIFO so that will indicate an invalid address if the serial communication is broken with the chip.
         if (network_stats.ieee_address == CTRLM_RF4CE_QORVO_BAD_MAC_ADDRESS) {
            dqm->status->chip_connected = 0;
            LOG_ERROR("%s: RF4CE Chip is not connected!\n", __FUNCTION__);
         } else if ((network_stats.ieee_address & CTRLM_RF4CE_QORVO_MAC_ADDRESS_PATTERN) != CTRLM_RF4CE_QORVO_MAC_ADDRESS_PATTERN) {
            dqm->status->chip_connected = 0;
            LOG_ERROR("%s: Bad Chip MAC address %llu!\n", __FUNCTION__, network_stats.ieee_address);
         } else {
            LOG_INFO("%s: RF4CE Chip is connected!\n", __FUNCTION__);
            dqm->status->chip_connected = 1;
         }
      dqm->status->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
      }
#else
      dqm->status->chip_connected = 1;
      LOG_ERROR("%s: RF4CE Chip Connectivity is not supported on this platform!\n", __FUNCTION__);
      dqm->status->result = CTRLM_IARM_CALL_RESULT_ERROR_NOT_SUPPORTED;
#endif
      if(dqm->semaphore) {
         sem_post(dqm->semaphore);
      }
#else
      ctrlm_obj_network_t::req_process_chip_status(data, size);
#endif
}


void ctrlm_obj_network_rf4ce_t::cs_values_set(const ctrlm_cs_values_t *values, bool db_load) {
   if(values == NULL) {
      LOG_ERROR("%s: values are NULL\n", __FUNCTION__);
      return;
   }

   // ASB
#ifdef ASB
   if(!is_asb_force_settings()) {
      asb_enabled_ = values->asb_enable;
   } else {
      LOG_WARN("%s: %s network not using ASB cs_value due to force settings set to TRUE\n", __FUNCTION__, name_get());
   }
#endif

   // Far Field Configuration
   far_field_configuration_t temp = ff_configuration_;
   
   if(values->chime_open_enable) {
      ff_configuration_.flags |= FAR_FIELD_CONFIGURATION_FLAGS_OPENING_CHIME; 
   } else {
      ff_configuration_.flags &= ~(FAR_FIELD_CONFIGURATION_FLAGS_OPENING_CHIME);
   }

   if(values->chime_close_enable) {
      ff_configuration_.flags |= FAR_FIELD_CONFIGURATION_FLAGS_CLOSING_CHIME; 
   } else {
      ff_configuration_.flags &= ~(FAR_FIELD_CONFIGURATION_FLAGS_CLOSING_CHIME);
   }

   if(values->chime_privacy_enable) {
      ff_configuration_.flags |= FAR_FIELD_CONFIGURATION_FLAGS_PRIVACY_CHIME;
   } else {
      ff_configuration_.flags &= ~(FAR_FIELD_CONFIGURATION_FLAGS_PRIVACY_CHIME);
   }

   if(values->conversational_mode > 0) {
      ff_configuration_.flags |= FAR_FIELD_CONFIGURATION_FLAGS_CONVERSATIONAL; 
   } else {
      ff_configuration_.flags &= ~(FAR_FIELD_CONFIGURATION_FLAGS_CONVERSATIONAL);
   }

   switch(values->chime_volume) {
      case CTRLM_CHIME_VOLUME_LOW:    ff_configuration_.chime_volume = FAR_FIELD_CONFIGURATION_CHIME_VOLUME_LOW;    break;
      case CTRLM_CHIME_VOLUME_MEDIUM: ff_configuration_.chime_volume = FAR_FIELD_CONFIGURATION_CHIME_VOLUME_MEDIUM; break;
      case CTRLM_CHIME_VOLUME_HIGH:   ff_configuration_.chime_volume = FAR_FIELD_CONFIGURATION_CHIME_VOLUME_HIGH;   break;
      default:                        ff_configuration_.chime_volume = FAR_FIELD_CONFIGURATION_CHIME_VOLUME_MEDIUM; break;
   }

   ff_configuration_.volume_ir_repeats = values->ir_repeats;

   if(!db_load && memcmp(&temp, &ff_configuration_, sizeof(temp))) {
      // Configuration changed
      update_far_field_configuration();
   }

}
 
ctrlm_rf4ce_polling_configuration_t ctrlm_obj_network_rf4ce_t::controller_polling_configuration_heartbeat_get(ctrlm_rf4ce_controller_type_t controller_type) {
   return (controller_polling_configuration_heartbeat_[controller_type]);
}
 
ctrlm_rf4ce_polling_generic_config_t ctrlm_obj_network_rf4ce_t::controller_generic_polling_configuration_get() {
   return (controller_generic_polling_configuration_);
}

bool ctrlm_obj_network_rf4ce_t::is_ir_rf_database_new() {
   return ir_rf_database_new_;
}

void ctrlm_obj_network_rf4ce_t::req_process_ir_set_code(void *data, int size) {
   ctrlm_main_queue_msg_ir_set_code_t *dqm = (ctrlm_main_queue_msg_ir_set_code_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_ir_set_code_t));

   if(controller_exists(dqm->controller_id)) {
      ctrlm_rf4ce_ir_rf_db_dev_type_t dev_type = CTRLM_RF4CE_IR_RF_DB_DEV_TV;
      switch(dqm->ir_codes->get_type()) {
         case CTRLM_IRDB_DEV_TYPE_TV: {
            dev_type = CTRLM_RF4CE_IR_RF_DB_DEV_TV;
            this->ir_rf_database_.clear_tv_ir_codes();
            break;
         }
         case CTRLM_IRDB_DEV_TYPE_AVR: {
            dev_type = CTRLM_RF4CE_IR_RF_DB_DEV_AVR;
            this->ir_rf_database_.clear_avr_ir_codes();
            break;
         }
         default: {
            LOG_ERROR("%s: Unexpected dev_type %d\n", __FUNCTION__, dqm->ir_codes->get_type());
            return;
         }
      }
      if(dqm->ir_codes->get_key_map()) {
         LOG_INFO("%s: Setting IR Codes on Controller %u\n", __FUNCTION__, dqm->controller_id);
         unsigned char status[1] = {IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES};
         for(const auto &itr : *dqm->ir_codes->get_key_map()) {
            ctrlm_rf4ce_ir_rf_db_entry_t *entry = ctrlm_rf4ce_ir_rf_db_entry_t::from_raw_ir_code(dev_type, itr.first, (uint8_t *)itr.second.data(), (uint16_t)itr.second.size());
            if(entry) {
               this->ir_rf_database_.add_ir_code_entry(entry);
            } else {
               LOG_WARN("%s: failed to create entry\n", __FUNCTION__);
            }
         }
         LOG_INFO("%s:\n%s\n", __FUNCTION__, this->ir_rf_database_.to_string(true).c_str());
         controllers_[dqm->controller_id]->rf4ce_rib_set_target(CTRLM_RF4CE_RIB_ATTR_ID_IR_RF_DATABASE_STATUS, CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL, CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS, status);
         controllers_[dqm->controller_id]->irdb_entry_id_name_set(dqm->ir_codes->get_type(), dqm->ir_codes->get_id());
         this->ir_rf_database_.store_db();
         if(dqm->success) *dqm->success = true;
      } else {
         LOG_ERROR("%s: Invalid IR Codes\n", __FUNCTION__);
         if(dqm->success) *dqm->success = false;
      }
   } else {
      LOG_ERROR("%s: Controller %u doesn't exist\n", __FUNCTION__, dqm->controller_id);
      if(dqm->success) *dqm->success = false;
   }

   // post the semaphore
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_rf4ce_t::req_process_ir_clear_codes(void *data, int size) {
   ctrlm_main_queue_msg_ir_clear_t *dqm = (ctrlm_main_queue_msg_ir_clear_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_ir_clear_t));

   if(controller_exists(dqm->controller_id)) {
      LOG_INFO("%s: Clearing IR Codes on Controller %u\n", __FUNCTION__, dqm->controller_id);
      unsigned char status[1] = {IR_RF_DATABASE_STATUS_DB_DOWNLOAD_YES};
      this->ir_rf_database_.clear_ir_codes();
      this->ir_rf_database_.store_db();
      LOG_INFO("%s:\n%s\n", __FUNCTION__, this->ir_rf_database_.to_string(true).c_str());
      controllers_[dqm->controller_id]->irdb_entry_id_name_set(CTRLM_IRDB_DEV_TYPE_TV, "0");
      controllers_[dqm->controller_id]->irdb_entry_id_name_set(CTRLM_IRDB_DEV_TYPE_AVR, "0");
      controllers_[dqm->controller_id]->rf4ce_rib_set_target(CTRLM_RF4CE_RIB_ATTR_ID_IR_RF_DATABASE_STATUS, CTRLM_RF4CE_RIB_ATTR_INDEX_GENERAL, CTRLM_RF4CE_RIB_ATTR_LEN_IR_RF_DATABASE_STATUS, status);
      if(dqm->success) *dqm->success = true;
   } else {
      LOG_ERROR("%s: Controller %u doesn't exist\n", __FUNCTION__, dqm->controller_id);
      if(dqm->success) *dqm->success = false;
   }

   // post the semaphore
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_rf4ce_polling_action_push(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, guint8 action, char* data, size_t len) {
   ctrlm_main_queue_msg_rf4ce_polling_action_t msg = {0};
   msg.controller_id     = controller_id;
   msg.action            = action;
   if(data) {
      errno_t safec_rc = memcpy_s(msg.data, sizeof(msg.data), data, (len > sizeof(msg.data) ? sizeof(msg.data) : len));
      ERR_CHK(safec_rc);
   }
   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_rf4ce_t::polling_action_push, &msg, sizeof(msg), NULL, network_id);
}

bool ctrlm_obj_network_rf4ce_t::analyze_assert_reason(const char *assert_info ) {
   if (assert_info == 0) {
      return (false);
   }

   // special case: major Qorvo chip failure assert
   std::regex e ("^07 Assert [0-9]*!gpHal_Reset.");
   std::string s = assert_info;
   std::smatch m;
   if (std::regex_search (s,m,e)) {
      failed_state_ = true;
      LOG_ERROR("%s: RF4CE chip malfunction. RF4CE is in failed state\n", __FUNCTION__);
      return (true);
   }
   return false;
}
#if CTRLM_HAL_RF4CE_API_VERSION >= 15 && !defined(CTRLM_HOST_DECRYPTION_NOT_SUPPORTED)
bool ctrlm_obj_network_rf4ce_t::sec_init()
{
   if (ctx_ != NULL) {
      LOG_ERROR("%s: open ssl was already initialized\n", __FUNCTION__);
      return false;
   }
    // Initialise the library
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG | OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);
#else
    OPENSSL_config(NULL);
#endif

    if((ctx_ = EVP_CIPHER_CTX_new()) == NULL) {
        LOG_ERROR("%s: EVP_CIPHER_CTX_new()) failed\n", __FUNCTION__);
        return false;
    }

    // First initialize with correct algorithm
    if( 1 != EVP_DecryptInit_ex(ctx_, EVP_aes_128_ccm(), 0, 0, 0)) {
        LOG_ERROR("%s: EVP_DecryptInit_ex() failed\n", __FUNCTION__);
        sec_deinit();
        return false;
    }

    // Set nonce size
    if( 1 != EVP_CIPHER_CTX_ctrl(ctx_, EVP_CTRL_CCM_SET_IVLEN, 13, 0)) {
        LOG_ERROR("%s: EVP_CIPHER_CTX_ctrl() : EVP_CTRL_CCM_SET_IVLEN failed\n", __FUNCTION__);
        sec_deinit();
        return false;
    }
   return true;
}

void ctrlm_obj_network_rf4ce_t::sec_deinit() {
   // Clean up
   if (ctx_ == NULL) {
      LOG_ERROR("%s: open ssl was not initialized\n", __FUNCTION__);
      return;
   }
   EVP_CIPHER_CTX_free(ctx_);
   ctx_ = NULL;
   EVP_cleanup();
   ERR_free_strings();
}

ctrlm_hal_result_t ctrlm_obj_network_rf4ce_t::hal_rf4ce_decrypt_callback(ctrlm_hal_rf4ce_decrypt_params_t* param) {
   if (instance != NULL) {
      return instance->hal_rf4ce_decrypt(param);
   }
   return CTRLM_HAL_RESULT_ERROR;
}

ctrlm_hal_result_t ctrlm_obj_network_rf4ce_t::hal_rf4ce_decrypt(ctrlm_hal_rf4ce_decrypt_params_t* param) {
   if (ctx_ == NULL) {
      LOG_ERROR("%s: open ssl was not initialized\n", __FUNCTION__);
      return CTRLM_HAL_RESULT_ERROR;
   }

   // Set MIC length
   if(1 != EVP_CIPHER_CTX_ctrl(ctx_, EVP_CTRL_CCM_SET_TAG, param->tag_length, param->tag)) {
      LOG_ERROR("%s: EVP_CIPHER_CTX_ctrl() : EVP_CTRL_CCM_SET_TAG failed\n", __FUNCTION__);
      return CTRLM_HAL_RESULT_ERROR;
   }

   // Set key and nonce
   if(1 != EVP_DecryptInit_ex(ctx_, 0, 0, param->key, param->nonce)) {
      LOG_ERROR("%s: EVP_DecryptInit_ex() : set key and nonce failed\n", __FUNCTION__);
      return CTRLM_HAL_RESULT_ERROR;
   }

   // We will encrypt data_len bytes
   int outl = 0;
   if(1 != EVP_DecryptUpdate(ctx_, 0, &outl, 0, param->data_length)) {
      LOG_ERROR("%s: EVP_DecryptUpdate() : set encrypt data_len bytes failed\n", __FUNCTION__);
      return CTRLM_HAL_RESULT_ERROR;
   }

   // Add aux data
   if(1 != EVP_DecryptUpdate(ctx_, 0, &outl, param->auth, param->auth_length)) {
      LOG_ERROR("%s: EVP_DecryptUpdate() : add aux data failed\n", __FUNCTION__);
      return CTRLM_HAL_RESULT_ERROR;
   }

   // Decrypt
   if(1 != EVP_DecryptUpdate(ctx_, param->cipher_text, &outl, param->plain_text, param->data_length)) {
      LOG_ERROR("%s: EVP_DecryptUpdate() : decrypt failed\n", __FUNCTION__);
      return CTRLM_HAL_RESULT_ERROR;
   }

   if(!EVP_DecryptFinal_ex(ctx_, &param->cipher_text[outl], &outl)) {
       LOG_DEBUG("%s: EVP_DecryptFinal_ex() : decrypt finalization is not successful\n", __FUNCTION__);
   }  //cID:160255 - checked return

   return CTRLM_HAL_RESULT_SUCCESS;
}
#endif

void ctrlm_obj_network_rf4ce_t::power_state_change(ctrlm_main_queue_power_state_change_t *dqm) {
   g_assert(dqm);
#if (CTRLM_HAL_RF4CE_API_VERSION >= 11)
   if((dqm->old_state != CTRLM_POWER_STATE_DEEP_SLEEP && dqm->new_state == CTRLM_POWER_STATE_DEEP_SLEEP) ||
      (dqm->old_state == CTRLM_POWER_STATE_DEEP_SLEEP && dqm->new_state != CTRLM_POWER_STATE_DEEP_SLEEP)) {
      ctrlm_main_queue_msg_network_property_set_t msg;
      ctrlm_hal_network_property_dpi_control_t dpi = {0};
      errno_t safec_rc = memset_s(&msg, sizeof(msg), 0, sizeof(msg));
      ERR_CHK(safec_rc);

      dpi.dpi_enable = (dqm->new_state == CTRLM_POWER_STATE_DEEP_SLEEP ? 1 : 0);
#ifndef CTRLM_HAL_DPI_DEFAULT
      // Use custom list
      dpi.defaults = 0;
      dpi.params   = dpi_args_;
#else
      dpi.defaults = 1;
      dpi.params   = NULL;
#endif

      msg.property = CTRLM_HAL_NETWORK_PROPERTY_DPI_CONTROL;
      msg.value = &dpi;

      property_set(msg.property, msg.value);
   }
#endif
}
