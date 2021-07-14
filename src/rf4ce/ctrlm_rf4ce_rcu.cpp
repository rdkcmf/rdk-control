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
#include <unistd.h>
#include <map>
#include <algorithm>
#include "libIBus.h"
#include "../ctrlm.h"
#include "../ctrlm_rcu.h"
#include "ctrlm_rf4ce_network.h"
#include "../ctrlm_utils.h"
#include "../ctrlm_validation.h"
#include "../ctrlm_device_update.h"
#include "../ctrlm_voice.h"
#include "ctrlm_rf4ce_utils.h"

using namespace std;

#define KEY_COMBO_BACKLIGHT                   ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_2 << 8) | (0))
#define KEY_COMBO_AUDIO_THEME                 ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_3 << 8) | (0))
#define KEY_COMBO_MODE_IR_CLIP_ALT            ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_6 << 8) | (CTRLM_KEY_CODE_DIGIT_0))
#define KEY_COMBO_MODE_IR_MOTOROLA_ALT        ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_6 << 8) | (CTRLM_KEY_CODE_DIGIT_1))
#define KEY_COMBO_MODE_IR_CISCO_ALT           ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_6 << 8) | (CTRLM_KEY_CODE_DIGIT_2))
#define KEY_COMBO_POLL_FIRMWARE               ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_6 << 8) | (CTRLM_KEY_CODE_DIGIT_4))
#define KEY_COMBO_POLL_AUDIO_DATA             ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_6 << 8) | (CTRLM_KEY_CODE_DIGIT_5))
#define KEY_COMBO_RESET_SOFT                  ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_8 << 8) | (CTRLM_KEY_CODE_DIGIT_0))
#define KEY_COMBO_RESET_FACTORY               ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_8 << 8) | (CTRLM_KEY_CODE_DIGIT_1))
#define KEY_COMBO_BLINK_SOFTWARE_VERSION      ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_8 << 8) | (CTRLM_KEY_CODE_DIGIT_3))
#define KEY_COMBO_BLINK_AVR_CODE              ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_8 << 8) | (CTRLM_KEY_CODE_DIGIT_5))
#define KEY_COMBO_RESET_IR                    ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_8 << 8) | (CTRLM_KEY_CODE_DIGIT_6))
#define KEY_COMBO_RESET_RF                    ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_8 << 8) | (CTRLM_KEY_CODE_DIGIT_7))
#define KEY_COMBO_BLINK_TV_CODE               ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_9 << 8) | (CTRLM_KEY_CODE_DIGIT_0))
#define KEY_COMBO_IR_DB_TV_SEARCH             ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_9 << 8) | (CTRLM_KEY_CODE_DIGIT_1))
#define KEY_COMBO_IR_DB_AVR_SEARCH            ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_9 << 8) | (CTRLM_KEY_CODE_DIGIT_2))
#define KEY_COMBO_KEY_REMAPPING               ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_9 << 8) | (CTRLM_KEY_CODE_DIGIT_4))
#define KEY_COMBO_BLINK_IR_DB_VERSION         ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_9 << 8) | (CTRLM_KEY_CODE_DIGIT_5))
#define KEY_COMBO_BLINK_BATTERY_LEVEL         ((CTRLM_KEY_CODE_DIGIT_9 << 16) | (CTRLM_KEY_CODE_DIGIT_9 << 8) | (CTRLM_KEY_CODE_DIGIT_9))
#define KEY_COMBO_XR16_IR_DB_TV_SEARCH        ((CTRLM_KEY_CODE_LAST << 16)    | (CTRLM_KEY_CODE_INPUT_SELECT << 8) | (CTRLM_KEY_CODE_MUTE))
#define KEY_COMBO_XR16_IR_DB_AVR_SEARCH       ((CTRLM_KEY_CODE_MENU << 16)    | (CTRLM_KEY_CODE_INPUT_SELECT << 8) | (CTRLM_KEY_CODE_MUTE))
#define KEY_COMBO_XR16_RESET_RF_ON            ((CTRLM_KEY_CODE_POWER_ON  << 16) | (CTRLM_KEY_CODE_LAST << 8)       | (CTRLM_KEY_CODE_VOL_UP))
#define KEY_COMBO_XR16_RESET_RF_OFF           ((CTRLM_KEY_CODE_POWER_OFF << 16) | (CTRLM_KEY_CODE_LAST << 8)       | (CTRLM_KEY_CODE_VOL_UP))
#define KEY_COMBO_XR16_RESET_FACTORY_ON       ((CTRLM_KEY_CODE_POWER_ON  << 16) | (CTRLM_KEY_CODE_LAST << 8)       | (CTRLM_KEY_CODE_VOL_DOWN))
#define KEY_COMBO_XR16_RESET_FACTORY_OFF      ((CTRLM_KEY_CODE_POWER_OFF << 16) | (CTRLM_KEY_CODE_LAST << 8)       | (CTRLM_KEY_CODE_VOL_DOWN))
#define KEY_COMBO_XR16_POLL_FIRMWARE          (CTRLM_KEY_CODE_INPUT_SELECT)
#define KEY_COMBO_XR16_BLINK_SOFTWARE_VERSION (CTRLM_KEY_CODE_MUTE)
#define KEY_COMBO_DISCOVERY                   (CTRLM_KEY_CODE_MENU)
#define KEY_COMBO_MODE_IR_CLIP                (CTRLM_KEY_CODE_OCAP_A)
#define KEY_COMBO_MODE_IR_MOTOROLA            (CTRLM_KEY_CODE_OCAP_B)
#define KEY_COMBO_MODE_IR_CISCO               (CTRLM_KEY_CODE_OCAP_C)
#define KEY_COMBO_MODE_CLIP_DISCOVERY         (CTRLM_KEY_CODE_OCAP_D)

typedef struct {
   ctrlm_network_id_t    network_id;
   ctrlm_controller_id_t controller_id;
   ctrlm_key_code_t      key_code;
   guint                 timeout_tag;
} timeout_key_release_t;

// Mapping from controller id to last rf4ce keypress
map <guint64, timeout_key_release_t> g_ctrlm_rcu_keypress_last_rf4ce;

static gboolean ctrlm_rcu_timeout_key_release(gpointer user_data);

void ctrlm_obj_network_rf4ce_t::ind_process_data(void *data, int size) { // ctrlm_main_queue_msg_rf4ce_ind_data_t *dqm
   ctrlm_main_queue_msg_rf4ce_ind_data_t *dqm = (ctrlm_main_queue_msg_rf4ce_ind_data_t *)data;

   g_assert(dqm);
   g_assert((unsigned int)size >= sizeof(ctrlm_main_queue_msg_rf4ce_ind_data_t));

   if(dqm->profile_id == CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU) {
      this->ind_process_data_rcu(dqm);
   } else if(dqm->profile_id == CTRLM_RF4CE_PROFILE_ID_VOICE) {
      LOG_ERROR("%s: voice profile indications are not handled!\n", __FUNCTION__);
      // Voice profile data indications are handled in the context of the HAL thread
   } else if(dqm->profile_id == CTRLM_RF4CE_PROFILE_ID_DEVICE_UPDATE) {
      this->ind_process_data_device_update(dqm);
      // TODO: May be able to reuse this buffer in the response
   } else {
      LOG_ERROR("%s: unsupported profile id 0x%X\n", __FUNCTION__, dqm->profile_id);
   }
}

void ctrlm_obj_network_rf4ce_t::ind_process_data_rcu(ctrlm_main_queue_msg_rf4ce_ind_data_t *dqm) {
   THREAD_ID_VALIDATE();
   LOG_DEBUG("%s: enter\n", __FUNCTION__);
   if(dqm == NULL) {
      LOG_ERROR("%s: Invalid parameters\n", __FUNCTION__);
      return;
   }

   if(!controller_exists(dqm->controller_id)) {
      LOG_ERROR("%s: Invalid controller id %u\n", __FUNCTION__, dqm->controller_id);
      return;
   }

   unsigned long cmd_length = dqm->length;
   guchar *      cmd_data   = dqm->data;
   ctrlm_rf4ce_frame_control_t frame_control;
   
   if(cmd_length < 2) {
      LOG_ERROR("%s: Invalid length %lu\n", __FUNCTION__, cmd_length);
      return;
   }
   if(g_ctrlm_rcu_keypress_last_rf4ce.count(dqm->controller_id) == 0) {
      LOG_INFO("%s: New controller id %u\n", __FUNCTION__, dqm->controller_id);
      g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].network_id    = network_id_get();
      g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].controller_id = dqm->controller_id;
   }

   frame_control = (ctrlm_rf4ce_frame_control_t)((guchar)cmd_data[0]);

   switch(frame_control) {
      case RF4CE_FRAME_CONTROL_USER_CONTROL_PRESSED: {
         ctrlm_key_code_t key_code = (ctrlm_key_code_t)((guchar)cmd_data[1]);
         LOG_DEBUG("%s: User control pressed <%s>. Payload (%ld)\n", __FUNCTION__, mask_key_codes_get() ? "*" : ctrlm_key_code_str(key_code), cmd_length - 2);
         
         // If new key, then send key release and delete key release timer
         if(g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].timeout_tag) {
            ctrlm_timeout_destroy(&g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].timeout_tag);
            ctrlm_rcu_iarm_event_key_press(g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].network_id, dqm->controller_id, CTRLM_KEY_STATUS_UP, g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].key_code);
         }

         if(key_event_hook(network_id_get(), dqm->controller_id, CTRLM_KEY_STATUS_DOWN, key_code)) {         
            controllers_[dqm->controller_id]->print_remote_firmware_debug_info(RF4CE_PRINT_FIRMWARE_LOG_BUTTON_PRESS);

            g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].key_code = key_code;
            ctrlm_rcu_iarm_event_key_press(network_id_get(), dqm->controller_id, CTRLM_KEY_STATUS_DOWN, g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].key_code);

            process_event_key(dqm->controller_id, CTRLM_KEY_STATUS_DOWN, key_code);

            // Set a timer to release the key if no repeats are received for a while
            g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].timeout_tag = ctrlm_timeout_create(timeout_key_release_, ctrlm_rcu_timeout_key_release, (gpointer)&g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id]);
         }
         break;
      }
      case RF4CE_FRAME_CONTROL_USER_CONTROL_REPEATED: {
         LOG_DEBUG("%s: User control repeated.\n", __FUNCTION__);
         
         if(key_event_hook(network_id_get(), dqm->controller_id, CTRLM_KEY_STATUS_REPEAT, (ctrlm_key_code_t) 0)) {
            // Need to store last key and repeat it here
            ctrlm_rcu_iarm_event_key_press(network_id_get(), dqm->controller_id, CTRLM_KEY_STATUS_REPEAT, g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].key_code);

            // Kick key release timer
            ctrlm_timeout_destroy(&g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].timeout_tag);
            g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].timeout_tag = ctrlm_timeout_create(timeout_key_release_, ctrlm_rcu_timeout_key_release, (gpointer)&g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id]);
         }
         break;
      }
      case RF4CE_FRAME_CONTROL_USER_CONTROL_RELEASED: {
         LOG_DEBUG("%s: User control released.\n", __FUNCTION__);
         if(key_event_hook(network_id_get(), dqm->controller_id, CTRLM_KEY_STATUS_UP, (ctrlm_key_code_t) 0)) {
            // Need to store last key and release it here
            ctrlm_rcu_iarm_event_key_press(network_id_get(), dqm->controller_id, CTRLM_KEY_STATUS_UP, g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].key_code);

            process_event_key(dqm->controller_id, CTRLM_KEY_STATUS_UP, g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].key_code);

            // Cancel key release timer, delete last key
            ctrlm_timeout_destroy(&g_ctrlm_rcu_keypress_last_rf4ce[dqm->controller_id].timeout_tag);
         }
         break;
      }
      case RF4CE_FRAME_CONTROL_CHECK_VALIDATION_REQUEST: {
         guchar check_validation_control = cmd_data[1];
         LOG_INFO("%s: Check validation request (%s).\n", __FUNCTION__, (check_validation_control & 0x1) ? "Automatic" : "Normal");
         
         ctrlm_rf4ce_result_validation_t result = controllers_[dqm->controller_id]->validation_result_get();
         // Send check validation response
         guchar response[2];
         response[0] = RF4CE_FRAME_CONTROL_CHECK_VALIDATION_RESPONSE;
         response[1] = result;

         // Determine when to send the response (50 ms after receipt)
         ctrlm_timestamp_add_ms(&dqm->timestamp, CTRLM_RF4CE_CONST_RESPONSE_IDLE_TIME);

         req_data(CTRLM_RF4CE_PROFILE_ID_COMCAST_RCU, dqm->controller_id, dqm->timestamp, 2, response, NULL, NULL);

         LOG_INFO("%s: Check validation result <%s>.\n", __FUNCTION__, ctrlm_rf4ce_result_validation_str(result));

         if(result != CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS && result != CTRLM_RF4CE_RESULT_VALIDATION_PENDING) {
            bool is_bound = controller_is_bound(dqm->controller_id);

            LOG_INFO("%s: is bound? <%s>.\n", __FUNCTION__, is_bound ? "YES" : "NO");
            if((result == CTRLM_RF4CE_RESULT_VALIDATION_TIMEOUT || result == CTRLM_RF4CE_RESULT_VALIDATION_COLLISION  ||
                result == CTRLM_RF4CE_RESULT_VALIDATION_ABORT   || result == CTRLM_RF4CE_RESULT_VALIDATION_FULL_ABORT ||
                result == CTRLM_RF4CE_RESULT_VALIDATION_FAILED  || result == CTRLM_RF4CE_RESULT_VALIDATION_IN_PROGRESS)
                && is_bound) {
               // Restore previous pairing entry if available
               LOG_INFO("%s: Failed validation (restore controller).\n", __FUNCTION__);
               usleep(200000); // Spec is 100ms, so 200ms is fine. Callback needs to be implemented.
               LOG_INFO("%s: Check validation result <%s> CONTINUE.\n", __FUNCTION__, ctrlm_rf4ce_result_validation_str(result));
               controller_restore(dqm->controller_id);
            } else { // Remove the controller now that the remote has read the validation result
               LOG_INFO("%s: Failed validation (remove controller).\n", __FUNCTION__);
               //Give the remote some time to get the FULL ABORT before removing the controller
               usleep(200000);
               bind_validation_timeout(dqm->controller_id);
            }
         }
         break;
      }
      case RF4CE_FRAME_CONTROL_CHECK_VALIDATION_RESPONSE: {
         LOG_INFO("%s: Check validation response.\n", __FUNCTION__);
         break;
      }
      case RF4CE_FRAME_CONTROL_SET_ATTRIBUTE_REQUEST: {
         if(cmd_length != ((unsigned long)(cmd_data[3])) + 4) {
            LOG_ERROR("%s: Set attribute request. Invalid Length %lu, %u\n", __FUNCTION__, cmd_length, cmd_data[3] + 4);
            // TODO need to send error response back in this case?
         } else if(!controller_exists(dqm->controller_id)) {
            LOG_ERROR("%s: Set attribute request. Invalid controller %u\n", __FUNCTION__, dqm->controller_id);
         } else {
            LOG_INFO("%s: Set attribute request\n", __FUNCTION__);
            controllers_[dqm->controller_id]->rf4ce_rib_set_controller(dqm->timestamp, (ctrlm_rf4ce_rib_attr_id_t)((guchar)cmd_data[1]), cmd_data[2], cmd_data[3], &cmd_data[4]);
         }
         break;
      }
      case RF4CE_FRAME_CONTROL_SET_ATTRIBUTE_RESPONSE: {
         LOG_INFO("%s: Set attribute response.\n", __FUNCTION__);
         break;
      }
      case RF4CE_FRAME_CONTROL_GET_ATTRIBUTE_REQUEST: {
         if(cmd_length != 4) {
            LOG_ERROR("%s: Get attribute request. Invalid Length %lu, %u\n", __FUNCTION__, cmd_length, 4);
            // TODO need to send error response back in this case?
         } else if(!controller_exists(dqm->controller_id)) {
            LOG_ERROR("%s: Get attribute request. Invalid controller %u\n", __FUNCTION__, dqm->controller_id);
         } else {
            LOG_INFO("%s: Get attribute request.\n", __FUNCTION__);
            controllers_[dqm->controller_id]->rf4ce_rib_get_controller(dqm->timestamp, (ctrlm_rf4ce_rib_attr_id_t)((guchar)cmd_data[1]), cmd_data[2], cmd_data[3]);
         }
         break;
      }
      case RF4CE_FRAME_CONTROL_GET_ATTRIBUTE_RESPONSE: {
         LOG_INFO("%s: Get attribute response.\n", __FUNCTION__);
         break;
      }
      case RF4CE_FRAME_CONTROL_KEY_COMBO: {
         guchar length = cmd_data[1];
         unsigned long code;
         if(cmd_length != ((unsigned long)(length)) + 2) {
            LOG_ERROR("%s: Key Combo. Invalid Length %lu, %u\n", __FUNCTION__, cmd_length, length + 2);
         } else {
            ctrlm_rcu_function_t  function   = CTRLM_RCU_FUNCTION_INVALID;
            unsigned long         value      = 0;
            gboolean              all_digits = true;
            LOG_INFO("%s: Key Combo - Length %u\n", __FUNCTION__, length);

            // Determine if all the keys are numbers
            for(unsigned long index = 0; index < length; index++) {
               if((cmd_data[index + 2] < CTRLM_KEY_CODE_DIGIT_0) || (cmd_data[index + 2] > CTRLM_KEY_CODE_DIGIT_9)) {
                  all_digits = false;
               }
            }

            if(length == 0) {
               all_digits = false;
               function   = CTRLM_RCU_FUNCTION_SETUP;
            } else if(length == 5 && all_digits) { // 5 digit code

               value = (cmd_data[2] - CTRLM_KEY_CODE_DIGIT_0);

               // Write the 5 digit TV code into an unsigned long
               for(unsigned long index = 1; index < length; index++) {
                  value *= 10;
                  value +=  (cmd_data[index + 2] - CTRLM_KEY_CODE_DIGIT_0);
               }

               if(value >= 10000 && value < 20000) {
                  function = CTRLM_RCU_FUNCTION_IR_DB_TV_SELECT;
               } else if(value >= 30000 && value < 40000) {
                  function = CTRLM_RCU_FUNCTION_IR_DB_AVR_SELECT;
               } else if(value >= 80000 && value < 90000) {
                  value -= 80000;
                  LOG_INFO("%s: Key Combo - Remote Theme %lu\n", __FUNCTION__, value);
                  controllers_[dqm->controller_id]->audio_theme_set((ctrlm_rf4ce_device_update_audio_theme_t)value);
                  controllers_[dqm->controller_id]->manual_poll_audio_data();
               }
            } else if(length <= 4) {
               unsigned char last_digit = 10;
               code                     = cmd_data[2];
               // Write the combo keys into an unsigned long for comparison
               for(unsigned long index = 1; index < length; index++) {
                  code <<= 8;
                  code  |= cmd_data[index + 2];
               }
               // Store the last digit if it is a number
               if(((code & 0xFF) >= CTRLM_KEY_CODE_DIGIT_0) && ((code & 0xFF) <= CTRLM_KEY_CODE_DIGIT_9)) {
                  last_digit = (code & 0xFF) - CTRLM_KEY_CODE_DIGIT_0;
               }
               // Decode the combo keys into remote functions
               if((length == 3) && ((code & 0xFFFF00) == KEY_COMBO_BACKLIGHT) && (last_digit < 10)) { // Backlight function
                  function = CTRLM_RCU_FUNCTION_BACKLIGHT;
                  value    = last_digit;
               } else if((length == 3) && ((code & 0xFFFF00) == KEY_COMBO_AUDIO_THEME) && (last_digit < 10)) { // Remote Theme function
                  LOG_INFO("%s: Key Combo - Remote Theme %u\n", __FUNCTION__, last_digit);
                  controllers_[dqm->controller_id]->audio_theme_set((ctrlm_rf4ce_device_update_audio_theme_t)last_digit);
               } else {
                  ctrlm_rf4ce_controller_type_t type = controllers_[dqm->controller_id]->controller_type_get();
                  switch(code) {
                     case KEY_COMBO_POLL_FIRMWARE:
                     {
                        function = CTRLM_RCU_FUNCTION_POLL_FIRMWARE;
                        if(FALSE == ctrlm_device_update_is_controller_updating(network_id_get(), dqm->controller_id, false)) {
                           controllers_[dqm->controller_id]->manual_poll_firmware();
                        }
                        break;
                     }
                     case KEY_COMBO_POLL_AUDIO_DATA: {
                        function = CTRLM_RCU_FUNCTION_POLL_AUDIO_DATA;
                        if(FALSE == ctrlm_device_update_is_controller_updating(network_id_get(), dqm->controller_id, false)) {
                           controllers_[dqm->controller_id]->manual_poll_audio_data();
                        }
                        break;
                     }
                     case KEY_COMBO_RESET_SOFT:             function = CTRLM_RCU_FUNCTION_RESET_SOFT;             break;
                     case KEY_COMBO_RESET_FACTORY:          function = CTRLM_RCU_FUNCTION_RESET_FACTORY;          break;
                     case KEY_COMBO_BLINK_SOFTWARE_VERSION: function = CTRLM_RCU_FUNCTION_BLINK_SOFTWARE_VERSION; break;
                     case KEY_COMBO_BLINK_AVR_CODE:         function = CTRLM_RCU_FUNCTION_BLINK_AVR_CODE;         break;
                     case KEY_COMBO_RESET_IR:               function = CTRLM_RCU_FUNCTION_RESET_IR;               break;
                     case KEY_COMBO_RESET_RF:               function = CTRLM_RCU_FUNCTION_RESET_RF;               break;
                     case KEY_COMBO_BLINK_TV_CODE:          function = CTRLM_RCU_FUNCTION_BLINK_TV_CODE;          break;
                     case KEY_COMBO_IR_DB_TV_SEARCH:        function = CTRLM_RCU_FUNCTION_IR_DB_TV_SEARCH;        break;
                     case KEY_COMBO_IR_DB_AVR_SEARCH:       function = CTRLM_RCU_FUNCTION_IR_DB_AVR_SEARCH;       break;
                     case KEY_COMBO_KEY_REMAPPING:          function = CTRLM_RCU_FUNCTION_KEY_REMAPPING;          break;
                     case KEY_COMBO_BLINK_IR_DB_VERSION:    function = CTRLM_RCU_FUNCTION_BLINK_IR_DB_VERSION;    break;
                     case KEY_COMBO_BLINK_BATTERY_LEVEL:    function = CTRLM_RCU_FUNCTION_BLINK_BATTERY_LEVEL;    break;
                     case KEY_COMBO_MODE_IR_CLIP_ALT:
                     case KEY_COMBO_MODE_IR_CLIP:           function = CTRLM_RCU_FUNCTION_MODE_IR_CLIP;           break;
                     case KEY_COMBO_MODE_IR_MOTOROLA_ALT:
                     case KEY_COMBO_MODE_IR_MOTOROLA:       function = CTRLM_RCU_FUNCTION_MODE_IR_MOTOROLA;       break;
                     case KEY_COMBO_MODE_IR_CISCO_ALT:
                     case KEY_COMBO_MODE_IR_CISCO:          function = CTRLM_RCU_FUNCTION_MODE_IR_CISCO;          break;
                     case KEY_COMBO_MODE_CLIP_DISCOVERY:    function = CTRLM_RCU_FUNCTION_MODE_CLIP_DISCOVERY;    break;
                     case KEY_COMBO_DISCOVERY: {
                        function = CTRLM_RCU_FUNCTION_DISCOVERY;

                        if((length == 1) && (((guchar)cmd_data[2]) == CTRLM_KEY_CODE_MENU)) {
                           if(type != RF4CE_CONTROLLER_TYPE_XR2 && type != RF4CE_CONTROLLER_TYPE_XR5 && type != RF4CE_CONTROLLER_TYPE_XR11){
                              function = CTRLM_RCU_FUNCTION_INVALID_KEY_COMBO; //Setup + Menu key combo is invalid on newer remotes
                           }
                        }

                        break;
                     }
                     /////////////////////
                     // XR16 key combos //
                     /////////////////////
                     case KEY_COMBO_XR16_POLL_FIRMWARE: {
                        if(type == RF4CE_CONTROLLER_TYPE_XR16 ) {
                           function = CTRLM_RCU_FUNCTION_POLL_FIRMWARE;
                           if(FALSE == ctrlm_device_update_is_controller_updating(network_id_get(), dqm->controller_id, false)) {
                              controllers_[dqm->controller_id]->manual_poll_firmware();
                           }
                        } else {
                           function = CTRLM_RCU_FUNCTION_INVALID_KEY_COMBO;
                        }
                        break;
                     }
                     case KEY_COMBO_XR16_RESET_FACTORY_ON:
                     case KEY_COMBO_XR16_RESET_FACTORY_OFF: {
                        if(type == RF4CE_CONTROLLER_TYPE_XR16 ) {
                           function = CTRLM_RCU_FUNCTION_RESET_FACTORY;
                        } else {
                           function = CTRLM_RCU_FUNCTION_INVALID_KEY_COMBO;
                        }
                        break;
                     }
                     case KEY_COMBO_XR16_BLINK_SOFTWARE_VERSION: {
                        if(type == RF4CE_CONTROLLER_TYPE_XR16 ) {
                           function = CTRLM_RCU_FUNCTION_BLINK_SOFTWARE_VERSION;
                        } else {
                           function = CTRLM_RCU_FUNCTION_INVALID_KEY_COMBO;
                        }
                        break;
                     }
                     case KEY_COMBO_XR16_RESET_RF_ON:
                     case KEY_COMBO_XR16_RESET_RF_OFF: {
                        if(type == RF4CE_CONTROLLER_TYPE_XR16 ) {
                           function = CTRLM_RCU_FUNCTION_RESET_RF;
                        } else {
                           function = CTRLM_RCU_FUNCTION_INVALID_KEY_COMBO;
                        }
                        break;
                     }
                     case KEY_COMBO_XR16_IR_DB_TV_SEARCH: {
                        if(type == RF4CE_CONTROLLER_TYPE_XR16 ) {
                           function = CTRLM_RCU_FUNCTION_IR_DB_TV_SEARCH;
                        } else {
                           function = CTRLM_RCU_FUNCTION_INVALID_KEY_COMBO;
                        }
                        break;
                     }
                     case KEY_COMBO_XR16_IR_DB_AVR_SEARCH: {
                        if(type == RF4CE_CONTROLLER_TYPE_XR16 ) {
                           function = CTRLM_RCU_FUNCTION_IR_DB_AVR_SEARCH;
                        } else {
                           function = CTRLM_RCU_FUNCTION_INVALID_KEY_COMBO;
                        }
                        break;
                     }
                     default: {
                        function = CTRLM_RCU_FUNCTION_INVALID_KEY_COMBO;
                     }
                  }
               }
            } else {
               LOG_WARN("%s: Key Combo - Unsupported Length %u\n", __FUNCTION__, length);
            }

            if(strncmp(controllers_[dqm->controller_id]->product_name_get(), "XR15", 4) == 0) {
               if(function == CTRLM_RCU_FUNCTION_MODE_IR_MOTOROLA ||
                  function == CTRLM_RCU_FUNCTION_MODE_IR_CISCO ||
                  function == CTRLM_RCU_FUNCTION_BLINK_IR_DB_VERSION) {
                  LOG_WARN("%s: Key Combo - Unsupported Function for XR15 <%s>\n", __FUNCTION__, ctrlm_rcu_function_str(function));
                  function = CTRLM_RCU_FUNCTION_INVALID;
               }
            }
            if(strncmp(controllers_[dqm->controller_id]->product_name_get(), "XR11", 4) == 0) {
               if(function == CTRLM_RCU_FUNCTION_BLINK_AVR_CODE) {
                  LOG_WARN("%s: Key Combo - Unsupported Function for XR11 <%s>\n", __FUNCTION__, ctrlm_rcu_function_str(function));
                  function = CTRLM_RCU_FUNCTION_INVALID;
               }
            }

            if(function == CTRLM_RCU_FUNCTION_INVALID_KEY_COMBO) {
               #define COMBO_KEYS_BUF_SIZE (50)
               char combo_keys[COMBO_KEYS_BUF_SIZE];
               guint32 combo_keys_index = 0;

               for(guint32 index = 0; index < length; index++) {
                  if(combo_keys_index > 0) {
                     combo_keys[combo_keys_index++] = ',';
                  }
                  if(combo_keys_index < COMBO_KEYS_BUF_SIZE) {
                     int size = sprintf_s(&combo_keys[combo_keys_index], COMBO_KEYS_BUF_SIZE - combo_keys_index, "%s", ctrlm_key_code_str((ctrlm_key_code_t)((guchar)cmd_data[index + 2])));
                     if(size > 0) {
                        combo_keys_index += size;
                     }  else {
                        ERR_CHK(size);
                     }
                  }
               }

               LOG_INFO("%s: Key Combo - Unsupported <%s>\n", __FUNCTION__, combo_keys);
            }

            if(function == CTRLM_RCU_FUNCTION_RESET_FACTORY    ||
               function == CTRLM_RCU_FUNCTION_RESET_RF         ||
               function == CTRLM_RCU_FUNCTION_MODE_IR_CLIP     ||
               function == CTRLM_RCU_FUNCTION_MODE_IR_MOTOROLA ||
               function == CTRLM_RCU_FUNCTION_MODE_IR_CISCO    ||
               function == CTRLM_RCU_FUNCTION_MODE_CLIP_DISCOVERY) {
               // Unbind the controller since it has effectively unpaired
               controller_unbind(dqm->controller_id, CTRLM_UNBIND_REASON_CONTROLLER_RESET);
            }

            if((function != CTRLM_RCU_FUNCTION_INVALID) && (function != CTRLM_RCU_FUNCTION_INVALID_KEY_COMBO)) {
               ctrlm_rcu_iarm_event_control(dqm->controller_id, "RF", "sfm", ctrlm_rcu_function_str(function), value, function);
            }
         }
         break;
      }
      case RF4CE_FRAME_CONTROL_GHOST_CODE: {
         if(cmd_length < 2 || cmd_length > 3) {
            LOG_ERROR("%s: Ghost Code. Invalid Length %lu, (should be 2 or 3)\n", __FUNCTION__, cmd_length);
         } else {
            ctrlm_remote_keypad_config remote_keypad_config = ctrlm_rf4ce_get_remote_keypad_config(controllers_[dqm->controller_id]->product_name_get());
            switch(cmd_data[1]) {
               case CTRLM_RCU_GHOST_CODE_VOLUME_UNITY_GAIN: {
                  LOG_INFO("%s: Ghost Code - Volume Unity Gain\n", __FUNCTION__);
                  ctrlm_rcu_iarm_event_key_ghost(network_id_get(), dqm->controller_id, remote_keypad_config, CTRLM_RCU_GHOST_CODE_VOLUME_UNITY_GAIN);
                  break;
               }
               case CTRLM_RCU_GHOST_CODE_POWER_OFF: {
                  LOG_INFO("%s: Ghost Code - Power Off\n", __FUNCTION__);
                  ctrlm_rcu_iarm_event_key_ghost(network_id_get(), dqm->controller_id, remote_keypad_config, CTRLM_RCU_GHOST_CODE_POWER_OFF);
                  break;
               }
               case CTRLM_RCU_GHOST_CODE_POWER_ON: {
                  LOG_INFO("%s: Ghost Code - Power On\n", __FUNCTION__);
                  ctrlm_rcu_iarm_event_key_ghost(network_id_get(), dqm->controller_id, remote_keypad_config, CTRLM_RCU_GHOST_CODE_POWER_ON);
                  break;
               }
               case CTRLM_RCU_GHOST_CODE_IR_POWER_TOGGLE: {
                  LOG_INFO("%s: Ghost Code - IR Power Toggle\n", __FUNCTION__);
                  ctrlm_rcu_iarm_event_key_ghost(network_id_get(), dqm->controller_id, remote_keypad_config, CTRLM_RCU_GHOST_CODE_IR_POWER_TOGGLE);
                  break;
               }
               case CTRLM_RCU_GHOST_CODE_IR_POWER_OFF: {
                  LOG_INFO("%s: Ghost Code - IR Power Off\n", __FUNCTION__);
                  ctrlm_rcu_iarm_event_key_ghost(network_id_get(), dqm->controller_id, remote_keypad_config, CTRLM_RCU_GHOST_CODE_IR_POWER_OFF);
                  break;
               }
               case CTRLM_RCU_GHOST_CODE_IR_POWER_ON: {
                  LOG_INFO("%s: Ghost Code - IR Power On\n", __FUNCTION__);
                  ctrlm_rcu_iarm_event_key_ghost(network_id_get(), dqm->controller_id, remote_keypad_config, CTRLM_RCU_GHOST_CODE_IR_POWER_ON);
                  break;
               }
               case CTRLM_RCU_GHOST_CODE_VOLUME_UP: {
                  LOG_INFO("%s: Ghost Code - Volume Up\n", __FUNCTION__);
                  ctrlm_rcu_iarm_event_key_ghost(network_id_get(), dqm->controller_id, remote_keypad_config, CTRLM_RCU_GHOST_CODE_VOLUME_UP);
                  break;
               }
               case CTRLM_RCU_GHOST_CODE_VOLUME_DOWN: {
                  LOG_INFO("%s: Ghost Code - Volume Down\n", __FUNCTION__);
                  ctrlm_rcu_iarm_event_key_ghost(network_id_get(), dqm->controller_id, remote_keypad_config, CTRLM_RCU_GHOST_CODE_VOLUME_DOWN);
                  break;
               }
               case CTRLM_RCU_GHOST_CODE_MUTE: {
                  LOG_INFO("%s: Ghost Code - Mute\n", __FUNCTION__);
                  ctrlm_rcu_iarm_event_key_ghost(network_id_get(), dqm->controller_id, remote_keypad_config, CTRLM_RCU_GHOST_CODE_MUTE);
                  break;
               }
               case CTRLM_RCU_GHOST_CODE_INPUT: {
                  LOG_INFO("%s: Ghost Code - Input\n", __FUNCTION__);
                  ctrlm_rcu_iarm_event_key_ghost(network_id_get(), dqm->controller_id, remote_keypad_config, CTRLM_RCU_GHOST_CODE_INPUT);
                  break;
               }
               case CTRLM_RCU_GHOST_CODE_FIND_MY_REMOTE: {
                  g_mutex_lock(&reverse_cmd_event_pending_mutex_);
                  // destroy end event timer if no more events pending
                  if (!reverse_cmd_end_event_pending_.empty()) {
                     ctrlm_rcu_iarm_event_reverse_cmd(network_id_get(), dqm->controller_id, CTRLM_RCU_IARM_EVENT_RCU_REVERSE_CMD_END,CTRLM_RCU_REVERSE_CMD_USER_INTERACTION,1,&cmd_data[2]);
                     reverse_cmd_end_event_pending_.clear();
                  }
                  g_mutex_unlock(&reverse_cmd_event_pending_mutex_);
                  if (reverse_cmd_end_event_timer_id_ != 0) {
                     ctrlm_timeout_destroy(&reverse_cmd_end_event_timer_id_);
                     reverse_cmd_end_event_timer_id_ = 0;
                  }
                  LOG_INFO("%s: Find My Remote - User pressed %s\n", __FUNCTION__, ctrlm_key_code_str((ctrlm_key_code_t) cmd_data[2]));
                  break;
               }
               default: {
                  LOG_WARN("%s: Ghost Code - unrecognized code 0x%02X\n", __FUNCTION__, cmd_data[1]);
               }
            }
         }
         break;
      }
      case RF4CE_FRAME_CONTROL_HEARTBEAT: {
         LOG_DEBUG("%s: Heartbeat command\n", __FUNCTION__);
         controllers_[dqm->controller_id]->rf4ce_heartbeat(dqm->timestamp, (guint16)((cmd_data[1] << 8) + cmd_data[2]));
         break;
      }
      case RF4CE_FRAME_CONTROL_HEARTBEAT_RESPONSE: {
         LOG_DEBUG("%s: Heartbeat Response command\n", __FUNCTION__);
         break;
      }
      case RF4CE_FRAME_CONTROL_CONFIGURATION_COMPLETE: {
         LOG_INFO("%s: Configuration Complete command\n", __FUNCTION__);
         controllers_[dqm->controller_id]->rib_configuration_complete(dqm->timestamp, (ctrlm_rf4ce_rib_configuration_complete_status_t)cmd_data[1]);
         break;
      }
      default: {
         LOG_ERROR("%s: Unhandled frame control (0x%02X) Length %lu.\n", __FUNCTION__, frame_control, cmd_length);
         ctrlm_print_data_hex(__FUNCTION__, cmd_data, cmd_length, 16);
         break;
      }
   }

   // Update device update timer so that download sessions do not timeout
   switch(frame_control) {
      case RF4CE_FRAME_CONTROL_USER_CONTROL_PRESSED:
      case RF4CE_FRAME_CONTROL_USER_CONTROL_REPEATED:
      case RF4CE_FRAME_CONTROL_USER_CONTROL_RELEASED: {
         // Update timer
         ctrlm_device_update_timeout_update_activity(network_id_get(), dqm->controller_id);
         break;
      }
      case RF4CE_FRAME_CONTROL_KEY_COMBO: {
         // Update timer with a longer value to prevent timeout from holding the button too long
         ctrlm_device_update_timeout_update_activity(network_id_get(), dqm->controller_id, CTRLM_DEVICE_UPDATE_EXTENDED_TIMEOUT_VALUE);
         break;
      }
      default: {
         break;
      }
   }
}

//#define DEMO_ONLY
gboolean  ctrlm_obj_network_rf4ce_t::key_event_hook(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_key_status_t key_status, ctrlm_key_code_t key_code) {

#ifdef DEMO_ONLY
   if (key_code == CTRLM_KEY_CODE_DIGIT_0 && key_status == CTRLM_KEY_STATUS_DOWN) {
      LOG_INFO("%s: Key 0x%02x DOWN\n", __FUNCTION__, key_code);
      if (is_voice_session_in_progress()) {
         ctrlm_main_queue_msg_terminate_voice_session_t *msg = (ctrlm_main_queue_msg_terminate_voice_session_t *)g_malloc(sizeof(ctrlm_main_queue_msg_terminate_voice_session_t));
         if(NULL == msg) {
            LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
            g_assert(0);
            return(CTRLM_HAL_RESULT_ERROR_OUT_OF_MEMORY);
         }
         msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_TERMINATE_VOICE_SESSION;
         msg->header.network_id = network_id;
         msg->controller_id     = controller_id;
         msg->reason            = VOICE_COMMAND_VOICE_SESSION_TERMINATE_SPECIAL_KEY_PRESS;
         ctrlm_main_queue_msg_push(msg);
         return false;
      } else {
         LOG_INFO("%s: Skip key 0x%02x DOWN\n", __FUNCTION__, key_code);
         return false;
      }
   }
#endif

   if(controllers_[controller_id]->validation_result_get() != CTRLM_RF4CE_RESULT_VALIDATION_SUCCESS) {
      gboolean auto_bind_in_progress = controllers_[controller_id]->autobind_in_progress_get();
      gboolean screen_bind_in_progress = controllers_[controller_id]->screen_bind_in_progress_get();
      return ctrlm_validation_key_sniff(network_id, controller_id, ctrlm_controller_type_get(controller_id), key_status, key_code, (screen_bind_in_progress == true ? screen_bind_in_progress : auto_bind_in_progress));
   } else {
      LOG_DEBUG("%s: Controller <%u> is not in validation.  Ignoring.\n", __FUNCTION__, controller_id);
   }

   return true;
}

gboolean ctrlm_rcu_timeout_key_release(gpointer user_data) {
   timeout_key_release_t *key_release = (timeout_key_release_t *) user_data;
   LOG_INFO("%s: Timeout - Key Release - Id %u.\n", __FUNCTION__, key_release->controller_id);
   
   ctrlm_rcu_iarm_event_key_press(key_release->network_id, key_release->controller_id, CTRLM_KEY_STATUS_UP, (ctrlm_key_code_t) key_release->key_code);
   key_release->timeout_tag = 0;
   return(FALSE);
};


