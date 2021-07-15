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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "ctrlm_tr181.h"
#include "libIBus.h" 

#define RFC_CALLER_ID "controlMgr"

// static void ctrlm_event_handler_tr181(const char *owner, IARM_EventId_t event_id, void *data, size_t len);
static ctrlm_tr181_result_t ctrlm_tr181_call_get(const char *parameter, RFC_ParamData_t *param);

ctrlm_tr181_result_t ctrlm_tr181_string_get(const char *parameter, char *s, size_t len) {
   if(s == NULL || len < 1) {
      LOG_ERROR("%s: Parameters cannot be null!\n", __FUNCTION__);
      return CTRLM_TR181_RESULT_FAILURE;
   }

   RFC_ParamData_t param;
   ctrlm_tr181_result_t result = ctrlm_tr181_call_get(parameter, &param);

   --len;
   if(CTRLM_TR181_RESULT_SUCCESS == result) {
      errno_t safec_rc = strncpy_s(s, len + 1, param.value, (len < MAX_PARAM_LEN ? len : MAX_PARAM_LEN));
      ERR_CHK(safec_rc);
      s[len] = '\0';
   }
   return result;
}

ctrlm_tr181_result_t ctrlm_tr181_bool_get(const char *parameter, bool *b) {
   if(NULL == b) {
      LOG_ERROR("%s: Parameters cannot be null!\n", __FUNCTION__);
      return CTRLM_TR181_RESULT_FAILURE;
   }

   RFC_ParamData_t param;
   ctrlm_tr181_result_t result = ctrlm_tr181_call_get(parameter, &param);

   if(CTRLM_TR181_RESULT_SUCCESS == result) {
      if(!strcasecmp(param.value, "true")) {
         *b = true;
      } else if(!strcasecmp(param.value, "false")) {
         *b = false;
      } else {
         LOG_ERROR("%s: %s Invalid bool str <%s>\n", __FUNCTION__, parameter, param.value);
         return CTRLM_TR181_RESULT_FAILURE;
      }
   }

   return result;
}

ctrlm_tr181_result_t ctrlm_tr181_int_get(const char *parameter, int *i, int min, int max) {
   if(NULL == i) {
      LOG_ERROR("%s: Parameters cannot be null!\n", __FUNCTION__);
      return CTRLM_TR181_RESULT_FAILURE;
   }

   RFC_ParamData_t param;
   ctrlm_tr181_result_t result = ctrlm_tr181_call_get(parameter, &param);

   if(CTRLM_TR181_RESULT_SUCCESS == result) {
      int temp = atoi(param.value);
      if(temp >= min && temp <= max) {
         *i = temp;
      } else {
         LOG_ERROR("%s: %s Integer returned is out of range <%d>\n", __FUNCTION__, parameter, temp);
         return CTRLM_TR181_RESULT_FAILURE;
      }
   }

   return result;
}

ctrlm_tr181_result_t ctrlm_tr181_real_get(const char *parameter, double *d, double min, double max) {
   if(NULL == d) {
      LOG_ERROR("%s: Parameters cannot be null!\n", __FUNCTION__);
      return CTRLM_TR181_RESULT_FAILURE;
   }

   RFC_ParamData_t param;
   ctrlm_tr181_result_t result = ctrlm_tr181_call_get(parameter, &param);

   if(CTRLM_TR181_RESULT_SUCCESS == result) {
      double temp = atof(param.value);
      if(temp >= min && temp <= max) {
         *d = temp;
      } else {
         LOG_ERROR("%s: %s Double returned is out of range <%f>\n", __FUNCTION__, parameter, temp);
         return CTRLM_TR181_RESULT_FAILURE;
      }
   }

   return result;
}

// Calls the TR181 get API
ctrlm_tr181_result_t ctrlm_tr181_call_get(const char *parameter, RFC_ParamData_t *param) {
   if(NULL == param || NULL == parameter) {
      LOG_ERROR("%s: parameter is null!\n", __FUNCTION__);
      return CTRLM_TR181_RESULT_FAILURE;
   }

   // TODO It doesn't handle reading of default values when defined
   WDMP_STATUS rc = getRFCParameter((char *)RFC_CALLER_ID, parameter, param);
   if(WDMP_SUCCESS != rc) {
      LOG_ERROR("%s: RFC get %s failed <%s>\n", __FUNCTION__, parameter, getRFCErrorString(rc));
      return CTRLM_TR181_RESULT_FAILURE;
   }

   return CTRLM_TR181_RESULT_SUCCESS;
}
