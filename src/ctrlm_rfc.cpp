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
// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <jansson.h>
#include "ctrlm_log.h"
#include "ctrlm_rfc.h"
#include "ctrlm_utils.h"
// End Includes

// Defines
// Global Defines
#define CTRLM_RFC_MAX_ATTEMPTS (2)
#define CTRLM_RFC_FILE_PATH    "/opt/RFC/"
// End Global Defines

// RF4CE Blackout RFC Defines
#define CTRLM_RF4CE_BLACKOUT_RFC_FILE                CTRLM_RFC_FILE_PATH ".RFC_rf4ce_pair_blackout.ini"
#define CTRLM_RF4CE_REGEX_BLACKOUT_ENABLE            "(?<=rf4ce_pair_blackout=)(false|true)"
#define CTRLM_RF4CE_REGEX_BLACKOUT_FAIL_THRESHOLD    "(?<=pairing_fail_threshold=)[0-9]*"
#define CTRLM_RF4CE_REGEX_BLACKOUT_REBOOT_THRESHOLD  "(?<=blackout_reboot_threshold=)[0-9]*"
#define CTRLM_RF4CE_REGEX_BLACKOUT_TIME              "(?<=blackout_time=)[0-9]*"
#define CTRLM_RF4CE_REGEX_BLACKOUT_TIME_INCREMENT    "(?<=blackout_time_increment=)[0-9]*"
// End RF4CE Blackout RFC Defines

// IP RFC Defines
#define CTRLM_IP_RFC_FILE                CTRLM_RFC_FILE_PATH ".RFC_IPREMOTE.ini"
#define CTRLM_IP_REGEX_ENABLE            "(?<=RFC_ENABLE_IPREMOTE=)(false|true)"
#define CTRLM_IP_REGEX_REVOKE_LIST       "(?<=RFC_DATA_IPREMOTE_REVOKE_LIST=)\\S*"
// End IP RFC Defines
// End Defines

// Globals
ctrlm_rfc_t *g_ctrlm_rfc_rf4ce_blackout;
ctrlm_rfc_t *g_ctrlm_rfc_ip;
static gboolean init = false;


// Private Function Implementations
void rfc_struct_default(ctrlm_rfc_t *rfc) {
   errno_t safec_rc = memset_s(rfc,  sizeof(ctrlm_rfc_t), 0, sizeof(ctrlm_rfc_t));
   ERR_CHK(safec_rc);
   rfc->stored   = false; // Just incase glib doesn't use 0 as false
}

gboolean rf4ce_blackout_rfc_get() {
   char                       *result   = NULL;
   char                       *contents = NULL;
   ctrlm_rfc_rf4ce_blackout_t *rf4ce_blackout = NULL;
   errno_t                     safec_rc = -1;
   int                         ind      = -1;

   if(NULL == g_ctrlm_rfc_rf4ce_blackout) {
      return false;
   }

   if(g_ctrlm_rfc_rf4ce_blackout->stored) {
      LOG_INFO("%s: Already retrieved RF4CE Blackout RFC settings\n", __FUNCTION__);
      return false;
   }

   if(g_ctrlm_rfc_rf4ce_blackout->attempts >= CTRLM_RFC_MAX_ATTEMPTS) {
      LOG_ERROR("%s: Already tried max attempts to get RFC data..\n", __FUNCTION__);
      return false;
   }

   g_ctrlm_rfc_rf4ce_blackout->attempts++;
   rf4ce_blackout = &g_ctrlm_rfc_rf4ce_blackout->rfc.rf4ce_blackout;

   safec_rc = memset_s(rf4ce_blackout, sizeof(ctrlm_rfc_rf4ce_blackout_t), 0, sizeof(ctrlm_rfc_rf4ce_blackout_t));
   ERR_CHK(safec_rc);

   contents = ctrlm_get_file_contents(CTRLM_RF4CE_BLACKOUT_RFC_FILE);
   if(NULL == contents) {
      LOG_ERROR("%s: Could not get the contents RFC file\n", __FUNCTION__);
      return false;
   }

   result = ctrlm_do_regex((char *)CTRLM_RF4CE_REGEX_BLACKOUT_ENABLE, contents);
   if(NULL != result) {
      safec_rc = strcmp_s("false", strlen("false"), result, &ind);
      ERR_CHK(safec_rc);
      if((safec_rc == EOK) && (!ind)) {
         rf4ce_blackout->enabled = false;
      } else {
         rf4ce_blackout->enabled = true;
      }
      LOG_INFO("%s: Blackout Enabled <%s> from RFC\n", __FUNCTION__, rf4ce_blackout->enabled ? "YES" : "NO");
      g_free(result);
      result = NULL;
   }

   result = ctrlm_do_regex((char *)CTRLM_RF4CE_REGEX_BLACKOUT_FAIL_THRESHOLD , contents);
   if(NULL != result) {
      guint temp = atoi(result);
      if(temp > 1) {
         rf4ce_blackout->pairing_fail_threshold = temp;
         LOG_INFO("%s: Blackout Fail Threshold <%u> from RFC\n", __FUNCTION__, rf4ce_blackout->pairing_fail_threshold);
      }
      g_free(result);
      result = NULL;
   }

   result = ctrlm_do_regex((char *)CTRLM_RF4CE_REGEX_BLACKOUT_REBOOT_THRESHOLD , contents);
   if(NULL != result) {
      guint temp = atoi(result);
      if(temp > 1) {
         rf4ce_blackout->blackout_reboot_threshold = temp;
         LOG_INFO("%s: Blackout Reboot Threshold <%u> from RFC\n", __FUNCTION__, rf4ce_blackout->blackout_reboot_threshold);
      }
      g_free(result);
      result = NULL;
   }

   result = ctrlm_do_regex((char *)CTRLM_RF4CE_REGEX_BLACKOUT_TIME , contents);
   if(NULL != result) {
      guint temp = atoi(result);
      if(temp > 1) {
         rf4ce_blackout->blackout_time = temp;
         LOG_INFO("%s: Blackout Time <%u> from RFC\n", __FUNCTION__, rf4ce_blackout->blackout_time);
      }
      g_free(result);
      result = NULL;
   }

   if(NULL != contents) {
      g_free(contents);
   }

   g_ctrlm_rfc_rf4ce_blackout->stored = true;
   return true;
}

gboolean ip_rfc_get() {
   char           *result   = NULL;
   char           *contents = NULL;
   guchar         *decoded_buf     = NULL;
   gsize           decoded_buf_len = 0;
   json_error_t    json_error;
   ctrlm_rfc_ip_t *ip = NULL;
   errno_t         safec_rc = -1;
   int             ind      = -1;

   if(NULL == g_ctrlm_rfc_ip) {
      return false;
   }

   if(g_ctrlm_rfc_ip->stored) {
      LOG_INFO("%s: Already retrieved IP RFC settings\n", __FUNCTION__);
      return false;
   }

   if(g_ctrlm_rfc_ip->attempts >= CTRLM_RFC_MAX_ATTEMPTS) {
      LOG_ERROR("%s: Already tried max attempts to get RFC data..\n", __FUNCTION__);
      return false;
   }

   g_ctrlm_rfc_ip->attempts++;
   ip = &g_ctrlm_rfc_ip->rfc.ip;

   contents = ctrlm_get_file_contents(CTRLM_IP_RFC_FILE);
   if(NULL == contents) {
      LOG_ERROR("%s: Could not get the contents RFC file\n", __FUNCTION__);
      return false;
   }

   result = ctrlm_do_regex((char *)CTRLM_IP_REGEX_ENABLE, contents);
   if(NULL != result) {
      safec_rc = strcmp_s("false", strlen("false"), result, &ind);
      ERR_CHK(safec_rc);
      if((safec_rc == EOK) && (!ind)) {
         ip->enabled = false;
      } else {
         ip->enabled = true;
      }
      LOG_INFO("%s: IP Network Enabled <%s> from RFC\n", __FUNCTION__, ip->enabled ? "YES" : "NO");
      g_free(result);
      result = NULL;
   }

   result = ctrlm_do_regex((char *)CTRLM_IP_REGEX_REVOKE_LIST, contents);
   if(NULL != result) {
      decoded_buf = g_base64_decode(result, &decoded_buf_len);
      g_free(result);
      if(NULL == decoded_buf || 0 == decoded_buf_len) {
         LOG_ERROR("%s: Failed to decode buffer\n", __FUNCTION__);
         if(decoded_buf) {
            g_free(decoded_buf);
         }
      } else {
         ip->revoke_list = json_loads((char *)decoded_buf, JSON_REJECT_DUPLICATES, &json_error);
         g_free(decoded_buf);
         if(ip->revoke_list == NULL) {
            LOG_ERROR("%s: JSON decoding error at line %u column %u <%s>\n", __FUNCTION__, json_error.line, json_error.column, json_error.text);
         } else {
            LOG_INFO("%s: Revoke List received from RFC\n", __FUNCTION__);
         }
      }
   }

   if(NULL != contents) {
      g_free(contents);
   }

   g_ctrlm_rfc_ip->stored = true;
   return true;
}
// End Private Function Implementations

// Public Function Implementations
gboolean ctrlm_rfc_init() {
   g_ctrlm_rfc_rf4ce_blackout = NULL;
   g_ctrlm_rfc_ip             = NULL;

   // Allocate rfc structs
   if(NULL == (g_ctrlm_rfc_rf4ce_blackout = (ctrlm_rfc_t *)malloc(sizeof(ctrlm_rfc_t)))) {
      LOG_ERROR("%s: Failed to allocate memory for rf4ce blackout rfc object\n", __FUNCTION__);
      return false;
   }

   if(NULL == (g_ctrlm_rfc_ip = (ctrlm_rfc_t *)malloc(sizeof(ctrlm_rfc_t)))) {
      LOG_ERROR("%s: Failed to allocate memory for ip rfc object\n", __FUNCTION__);
      if(g_ctrlm_rfc_rf4ce_blackout) {
         free(g_ctrlm_rfc_rf4ce_blackout);
      }
      return false;
   }

   // Default structs
   rfc_struct_default(g_ctrlm_rfc_rf4ce_blackout);
   rfc_struct_default(g_ctrlm_rfc_ip);

   // Retrieve the RFC information
   if(false == rf4ce_blackout_rfc_get()) {
      LOG_WARN("%s: Failed to get RF4CE Blackout RFC settings..\n", __FUNCTION__);
   }

   if(false == ip_rfc_get()) {
      LOG_WARN("%s: Failed to get IP RFC settings\n", __FUNCTION__);
   }

   init = true;

   return true;
}

gboolean ctrlm_rfc_rf4ce_blackout_get(ctrlm_rfc_rf4ce_blackout_t **rf4ce_blackout) {
   // Check to see if init was performed
   if(false == init) {
      LOG_ERROR("%s: No init was performed before get call\n", __FUNCTION__);
      return false;
   }
   // Check if we already have the RFC data and if not try to get it.
   if(false == g_ctrlm_rfc_rf4ce_blackout->stored) {
      if(false == rf4ce_blackout_rfc_get()) {
         LOG_ERROR("%s: Failed to get RFC data\n", __FUNCTION__);
         return false;
      }
   }
   *rf4ce_blackout = &g_ctrlm_rfc_rf4ce_blackout->rfc.rf4ce_blackout;
   return true;
}

gboolean ctrlm_rfc_ip_get(ctrlm_rfc_ip_t **ip) {
   // Check to see if init was performed
   if(false == init) {
      LOG_ERROR("%s: No init was performed before get call\n", __FUNCTION__);
      return false;
   }
   // Check if we already have the RFC data and if not try to get it.
   if(false == g_ctrlm_rfc_ip->stored) {
      if(false == ip_rfc_get()) {
         LOG_ERROR("%s: Failed to get RFC data\n", __FUNCTION__);
         return false;
      }
   }
   *ip = &g_ctrlm_rfc_ip->rfc.ip;
   return true;
}

void ctrlm_rfc_destroy() {
   if(g_ctrlm_rfc_rf4ce_blackout) {
      free(g_ctrlm_rfc_rf4ce_blackout);
   }

   if(g_ctrlm_rfc_ip) {
      if(g_ctrlm_rfc_ip->rfc.ip.revoke_list) {
         json_decref(g_ctrlm_rfc_ip->rfc.ip.revoke_list);
      }
      free(g_ctrlm_rfc_ip);
   }
}
// End Public Function Implementations
