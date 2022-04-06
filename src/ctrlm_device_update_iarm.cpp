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
#include "libIBus.h"
#include "ctrlm.h"
#include "ctrlm_utils.h"
#include "ctrlm_rcu.h"
#include "rf4ce/ctrlm_rf4ce_network.h"
#include "ctrlm_device_update.h"

static IARM_Result_t ctrlm_device_update_status_get(void *arg);
static IARM_Result_t ctrlm_device_update_session_get(void *arg);
static IARM_Result_t ctrlm_device_update_image_get(void *arg);
static IARM_Result_t ctrlm_device_update_download_initiate(void *arg);
static IARM_Result_t ctrlm_device_update_load_initiate(void *arg);
static IARM_Result_t ctrlm_device_update_update_available(void *arg);

gboolean ctrlm_device_update_init_iarm() {
   IARM_Result_t rc;
   LOG_INFO("%s\n", __FUNCTION__);

   rc = IARM_Bus_RegisterCall(CTRLM_DEVICE_UPDATE_IARM_CALL_STATUS_GET, ctrlm_device_update_status_get);
   if(rc != IARM_RESULT_SUCCESS) {
      LOG_ERROR("%s: CTRLM_DEVICE_UPDATE_IARM_CALL_STATUS_GET %d\n", __FUNCTION__, rc);
      return(false);
   }

   rc = IARM_Bus_RegisterCall(CTRLM_DEVICE_UPDATE_IARM_CALL_SESSION_GET, ctrlm_device_update_session_get);
   if(rc != IARM_RESULT_SUCCESS) {
      LOG_ERROR("%s: CTRLM_DEVICE_UPDATE_IARM_CALL_SESSION_GET %d\n", __FUNCTION__, rc);
      return(false);
   }

   rc = IARM_Bus_RegisterCall(CTRLM_DEVICE_UPDATE_IARM_CALL_IMAGE_GET, ctrlm_device_update_image_get);
   if(rc != IARM_RESULT_SUCCESS) {
      LOG_ERROR("%s: CTRLM_DEVICE_UPDATE_IARM_CALL_IMAGE_GET %d\n", __FUNCTION__, rc);
      return(false);
   }

   rc = IARM_Bus_RegisterCall(CTRLM_DEVICE_UPDATE_IARM_CALL_DOWNLOAD_INITIATE, ctrlm_device_update_download_initiate);
   if(rc != IARM_RESULT_SUCCESS) {
      LOG_ERROR("%s: CTRLM_DEVICE_UPDATE_IARM_CALL_DOWNLOAD_INITIATE %d\n", __FUNCTION__, rc);
      return(false);
   }

   rc = IARM_Bus_RegisterCall(CTRLM_DEVICE_UPDATE_IARM_CALL_LOAD_INITIATE, ctrlm_device_update_load_initiate);
   if(rc != IARM_RESULT_SUCCESS) {
      LOG_ERROR("%s: CTRLM_DEVICE_UPDATE_IARM_CALL_LOAD_INITIATE %d\n", __FUNCTION__, rc);
      return(false);
   }
   rc = IARM_Bus_RegisterCall(CTRLM_DEVICE_UPDATE_IARM_CALL_UPDATE_AVAILABLE, ctrlm_device_update_update_available);
   if(rc != IARM_RESULT_SUCCESS) {
      LOG_ERROR("%s: CTRLM_DEVICE_UPDATE_IARM_CALL_UPDATE_AVAILABLE %d\n", __FUNCTION__, rc);
      return(false);
   }

   return(true);
}

IARM_Result_t ctrlm_device_update_update_available(void *arg) {
   ctrlm_device_update_iarm_call_update_available_t *params = (ctrlm_device_update_iarm_call_update_available_t *) arg;
   if(params == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(params->api_revision != CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, params->api_revision, CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION);
      params->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s:got location '%s' and filenames '%s'\n", __FUNCTION__,params->firmwareLocation, params->firmwareNames);

   //format from script utilty will be:
   // firmwareLocation - "/opt/CDL"
   // firmwareNames - "test.bin,test1.bin,test2.bin"

   gchar *tok = NULL;
   gchar *saveptr = NULL;
   size_t  len = 0;
   len = strlen(params->firmwareNames);
   if(len != 0){
      tok  = strtok_s(params->firmwareNames, &len, ",", &saveptr);
   }
   while(tok!=NULL){
      std::string filename=params->firmwareLocation;
      filename+="/";
      filename+=tok;
      LOG_INFO("%s: filename %s\n", __FUNCTION__, filename.c_str());

      //start processing new update file via message to background task
      if(ctrlm_device_update_process_xconf_update(filename.c_str())==false) {
         params->result = CTRLM_IARM_CALL_RESULT_ERROR;
      }
      else {
         // something went wrong so send error back
         params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
      }
      tok = strtok_s(NULL, &len, ",", &saveptr);

   }
   return IARM_RESULT_SUCCESS;

}

IARM_Result_t ctrlm_device_update_status_get(void *arg) {
   ctrlm_device_update_iarm_call_status_t *status = (ctrlm_device_update_iarm_call_status_t *) arg;
   if(status == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(status->api_revision != CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, status->api_revision, CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION);
      status->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   if(!ctrlm_device_update_status_info_get(status)) {
      LOG_ERROR("%s: Error getting status\n", __FUNCTION__);
      status->result = CTRLM_IARM_CALL_RESULT_ERROR;
   } else {
      status->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
   }
   return IARM_RESULT_SUCCESS;
}

IARM_Result_t ctrlm_device_update_session_get(void *arg) {
   ctrlm_device_update_iarm_call_session_t *session = (ctrlm_device_update_iarm_call_session_t *) arg;
   if(session == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(session->api_revision != CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, session->api_revision, CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION);
      session->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   if(session->session_id == 0) {
      LOG_ERROR("%s: Invalid session id %u\n", __FUNCTION__, session->session_id);
      session->result = CTRLM_IARM_CALL_RESULT_ERROR;
      return(IARM_RESULT_SUCCESS);
   }
   if(!ctrlm_device_update_session_get_by_id(session->session_id, session)) {
      LOG_ERROR("%s: Session id not found %u\n", __FUNCTION__, session->session_id);
      session->result = CTRLM_IARM_CALL_RESULT_ERROR;
      return(IARM_RESULT_SUCCESS);
   }

   LOG_INFO("%s: Found session id %u\n", __FUNCTION__, session->session_id);
   session->result = CTRLM_IARM_CALL_RESULT_SUCCESS;

   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_device_update_image_get(void *arg) {
   ctrlm_device_update_iarm_call_image_t *image = (ctrlm_device_update_iarm_call_image_t *) arg;
   if(image == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(image->api_revision != CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, image->api_revision, CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION);
      image->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   if(!ctrlm_device_update_is_image_id_valid(image->image_id)) {
      LOG_ERROR("%s: Invalid image id %u\n", __FUNCTION__, image->image_id);
      image->result = CTRLM_IARM_CALL_RESULT_ERROR;
      return(IARM_RESULT_SUCCESS);
   }

   if(!ctrlm_device_update_image_get_by_id(image->image_id, &image->image)) {
      LOG_ERROR("%s: image not found with image id %u\n", __FUNCTION__, image->image_id);
      image->result = CTRLM_IARM_CALL_RESULT_ERROR;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: Found image with image id %u\n", __FUNCTION__, image->image_id);
   image->result = CTRLM_IARM_CALL_RESULT_SUCCESS;

   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_device_update_download_initiate(void *arg) {
   ctrlm_device_update_iarm_call_download_params_t *params = (ctrlm_device_update_iarm_call_download_params_t *) arg;
   if(params == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(params->api_revision != CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, params->api_revision, CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION);
      params->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   if(params->session_id == 0) {
      LOG_ERROR("%s: Invalid session id %u\n", __FUNCTION__, params->session_id);
      params->result = CTRLM_IARM_CALL_RESULT_ERROR;
      return(IARM_RESULT_SUCCESS);
   }

   if(!ctrlm_device_update_interactive_download_start(params->session_id, params->background_download, params->percent_increment, params->load_image_immediately)) {
      LOG_ERROR("%s: unable to start download %u\n", __FUNCTION__, params->session_id);
      params->result = CTRLM_IARM_CALL_RESULT_ERROR;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: Session id %u\n", __FUNCTION__, params->session_id);
   params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_device_update_load_initiate(void *arg) {
   ctrlm_device_update_iarm_call_load_params_t *params = (ctrlm_device_update_iarm_call_load_params_t *) arg;
   if(params == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(IARM_RESULT_INVALID_PARAM);
   }
   if(params->api_revision != CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION) {
      LOG_INFO("%s: Unsupported API Revision (%u, %u)\n", __FUNCTION__, params->api_revision, CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION);
      params->result = CTRLM_IARM_CALL_RESULT_ERROR_API_REVISION;
      return(IARM_RESULT_SUCCESS);
   }
   if(params->session_id == 0) {
      LOG_ERROR("%s: Invalid session id %u\n", __FUNCTION__, params->session_id);
      params->result = CTRLM_IARM_CALL_RESULT_ERROR;
      return(IARM_RESULT_SUCCESS);
   }

   switch(params->load_type) {
      case CTRLM_DEVICE_UPDATE_IARM_LOAD_TYPE_DEFAULT:
      case CTRLM_DEVICE_UPDATE_IARM_LOAD_TYPE_NORMAL:
      case CTRLM_DEVICE_UPDATE_IARM_LOAD_TYPE_POLL:
      case CTRLM_DEVICE_UPDATE_IARM_LOAD_TYPE_ABORT:
         break;
      default:
         LOG_ERROR("%s: Invalid load type %d\n", __FUNCTION__, params->load_type);
         params->result = CTRLM_IARM_CALL_RESULT_ERROR;
         return(IARM_RESULT_SUCCESS);
   }

   if(!ctrlm_device_update_interactive_load_start(params->session_id, params->load_type, params->time_to_load, params->time_after_inactive)) {
      LOG_ERROR("%s: unable to start download %u\n", __FUNCTION__, params->session_id);
      params->result = CTRLM_IARM_CALL_RESULT_ERROR;
      return(IARM_RESULT_SUCCESS);
   }
   LOG_INFO("%s: Session id %u\n", __FUNCTION__, params->session_id);
   params->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
   return(IARM_RESULT_SUCCESS);
}

void ctrlm_device_update_iarm_event_ready_to_download(ctrlm_device_update_session_id_t session_id) {
   ctrlm_device_update_iarm_event_ready_to_download_t event = {0};
   event.api_revision = CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION;
   event.session_id   = session_id;

   if(!ctrlm_device_update_image_device_get_by_id(session_id, &event.image_id, &event.device)) {
      LOG_ERROR("%s: Session details not found %u\n", __FUNCTION__, session_id);
      return;
   }

   LOG_INFO("%s: Found session id %u\n", __FUNCTION__, session_id);

   IARM_Result_t retval = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, (IARM_EventId_t) CTRLM_DEVICE_UPDATE_IARM_EVENT_READY_TO_DOWNLOAD, (void *) &event, sizeof(event));
   if(retval != IARM_RESULT_SUCCESS) {
      LOG_ERROR("%s: Event send error (%i)\n", __FUNCTION__, retval);
   }
}

void ctrlm_device_update_iarm_event_download_status(ctrlm_device_update_session_id_t session_id, guchar percent_complete) {
   ctrlm_device_update_iarm_event_download_status_t event = {0};
   event.api_revision     = CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION;
   event.session_id       = session_id;
   event.percent_complete = percent_complete;
   LOG_INFO("%s: Session Id %u Percent complete %u\n", __FUNCTION__, session_id, percent_complete);

   IARM_Result_t retval = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, (IARM_EventId_t) CTRLM_DEVICE_UPDATE_IARM_EVENT_DOWNLOAD_STATUS, (void *) &event, sizeof(event));
   if(retval != IARM_RESULT_SUCCESS) {
      LOG_ERROR("%s: Event send error (%i)\n", __FUNCTION__, retval);
   }
}

void ctrlm_device_update_iarm_event_load_begin(ctrlm_device_update_session_id_t session_id) {
   ctrlm_device_update_iarm_event_load_begin_t event = {0};
   event.api_revision = CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION;
   event.session_id   = session_id;
   LOG_INFO("%s: Session Id %u\n", __FUNCTION__, session_id);

   IARM_Result_t retval = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, (IARM_EventId_t) CTRLM_DEVICE_UPDATE_IARM_EVENT_LOAD_BEGIN, (void *) &event, sizeof(event));
   if(retval != IARM_RESULT_SUCCESS) {
      LOG_ERROR("%s: Event send error (%i)\n", __FUNCTION__, retval);
   }
}

void ctrlm_device_update_iarm_event_load_end(ctrlm_device_update_session_id_t session_id, ctrlm_device_update_iarm_load_result_t result) {
   ctrlm_device_update_iarm_event_load_end_t event = {0};
   event.api_revision = CTRLM_DEVICE_UPDATE_IARM_BUS_API_REVISION;
   event.session_id   = session_id;
   event.result       = result;
   LOG_INFO("%s: Session Id %u Result <%s>\n", __FUNCTION__, session_id, ctrlm_device_update_iarm_load_result_str(result));

   IARM_Result_t retval = IARM_Bus_BroadcastEvent(CTRLM_MAIN_IARM_BUS_NAME, (IARM_EventId_t) CTRLM_DEVICE_UPDATE_IARM_EVENT_LOAD_END, (void *) &event, sizeof(event));
   if(retval != IARM_RESULT_SUCCESS) {
      LOG_ERROR("%s: Event send error (%i)\n", __FUNCTION__, retval);
   }
}
