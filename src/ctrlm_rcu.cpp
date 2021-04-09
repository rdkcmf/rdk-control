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
#include <semaphore.h>
#include "ctrlm.h"
#include "ctrlm_utils.h"
#include "ctrlm_rcu.h"
#include "ctrlm_validation.h"
#include "rf4ce/ctrlm_rf4ce_network.h"

void ctrlm_rcu_init(void) {
   LOG_INFO("%s\n", __FUNCTION__);
   if(!ctrlm_rcu_init_iarm()) {
      LOG_ERROR("%s: IARM init failed.\n", __FUNCTION__);
   }
}

void ctrlm_rcu_terminate(void) {
   LOG_INFO("%s: clean up\n", __FUNCTION__);
   ctrlm_rcu_terminate_iarm();
}

gboolean ctrlm_rcu_validation_finish(ctrlm_rcu_iarm_call_validation_finish_t *params) {

#if 1 // ASYNCHRONOUS
   if(!ctrlm_controller_id_is_valid(params->network_id, params->controller_id)) {
      LOG_ERROR("%s: invalid controller id (%u, %u)\n", __FUNCTION__, params->network_id, params->controller_id);
      return(false);
   }
   if(params->validation_result >= CTRLM_RCU_VALIDATION_RESULT_MAX) {
      LOG_ERROR("%s: invalid validation result (%d)\n", __FUNCTION__, params->validation_result);
      return(false);
   }

   ctrlm_inform_validation_end(params->network_id, params->controller_id, CTRLM_RCU_BINDING_TYPE_INTERACTIVE, CTRLM_RCU_VALIDATION_TYPE_APPLICATION, params->validation_result, NULL, NULL);
   return(true);
#else // synchronous
   GCond cond;
   GMutex mutex;
   ctrlm_validation_end_cmd_result_t cmd_result = CTRLM_VALIDATION_END_CMD_RESULT_PENDING;

   g_cond_init(&cond);
   g_mutex_init(&mutex);
   g_mutex_lock(&mutex);

   if(!ctrlm_inform_validation_end(params->network_id, params->controller_id, params->validation_result, &cond, &cmd_result)) {
      g_mutex_unlock(&mutex);
      return(false);
   }

   // Wait for the result condition to be signaled
   while(CTRLM_VALIDATION_END_CMD_RESULT_PENDING == cmd_result) {
      g_cond_wait(&cond, &mutex);
   }
   g_mutex_unlock(&mutex);

   if(cmd_result == CTRLM_VALIDATION_END_CMD_RESULT_SUCCESS) {
      return(true);
   }
   return(false);
#endif
}

gboolean ctrlm_rcu_controller_status(ctrlm_rcu_iarm_call_controller_status_t *params) {
   LOG_INFO("%s: (%u, %u)\n", __FUNCTION__, params->network_id, params->controller_id);

   if(params->network_id == CTRLM_MAIN_NETWORK_ID_ALL || params->controller_id == CTRLM_MAIN_CONTROLLER_ID_ALL) {
      LOG_ERROR("%s: Cannot get status for multiple controllers\n", __FUNCTION__);
      return(false);
   }
   sem_t semaphore;
   ctrlm_controller_status_cmd_result_t cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_controller_status_t msg;
   memset(&msg, 0, sizeof(msg));

   sem_init(&semaphore, 0, 0);

   msg.controller_id     = params->controller_id;
   msg.status            = &params->status;
   msg.semaphore         = &semaphore;
   msg.cmd_result        = &cmd_result;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_controller_status, &msg, sizeof(msg), NULL, params->network_id);

   // Wait for the result condition to be signaled
   while(CTRLM_CONTROLLER_STATUS_REQUEST_PENDING == cmd_result) {
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_CONTROLLER_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

gboolean ctrlm_rcu_rib_request_get(ctrlm_rcu_iarm_call_rib_request_t *params) {
   if(params->attribute_id == CTRLM_RCU_RIB_ATTR_ID_IR_RF_DATABASE) {
      LOG_INFO("%s: (%u, %u) Attribute <%s> Index <%s> Length %u\n", __FUNCTION__, params->network_id, params->controller_id, ctrlm_rcu_rib_attr_id_str(params->attribute_id), ctrlm_key_code_str((ctrlm_key_code_t)params->attribute_index), params->length);
   } else {
      LOG_INFO("%s: (%u, %u) Attribute <%s> Index %u Length %u\n", __FUNCTION__, params->network_id, params->controller_id, ctrlm_rcu_rib_attr_id_str(params->attribute_id), params->attribute_index, params->length);
   }

   if(params->network_id == CTRLM_MAIN_NETWORK_ID_ALL || params->controller_id == CTRLM_MAIN_CONTROLLER_ID_ALL) {
      LOG_ERROR("%s: Cannot get multiple RIB entries\n", __FUNCTION__);
      return(false);
   }
   sem_t  semaphore;
   ctrlm_rib_request_cmd_result_t cmd_result = CTRLM_RIB_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_rib_t msg;
   memset(&msg, 0, sizeof(msg));

   sem_init(&semaphore, 0, 0);

   msg.controller_id     = params->controller_id;
   msg.attribute_id      = params->attribute_id;
   msg.attribute_index   = params->attribute_index;
   msg.length            = params->length;
   msg.length_out        = &params->length;
   msg.data              = (guchar *)params->data;
   msg.semaphore         = &semaphore;
   msg.cmd_result        = &cmd_result;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_rib_get, &msg, sizeof(msg), NULL, params->network_id);

   // Wait for the result condition to be signaled
   while(CTRLM_RIB_REQUEST_PENDING == cmd_result) {
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_RIB_REQUEST_SUCCESS) {
      return(true);
   }

   params->length = 0;
   LOG_ERROR("%s: Failed to get RIB entry\n", __FUNCTION__);

   return(false);
}

gboolean ctrlm_rcu_rib_request_set(ctrlm_rcu_iarm_call_rib_request_t *params) {

   if(params->attribute_id == CTRLM_RCU_RIB_ATTR_ID_IR_RF_DATABASE) {
      LOG_INFO("%s: (%u, %u) Attribute <%s> Index <%s> Length %u\n", __FUNCTION__, params->network_id, params->controller_id, ctrlm_rcu_rib_attr_id_str(params->attribute_id), ctrlm_key_code_str((ctrlm_key_code_t)params->attribute_index), params->length);
   } else {
      LOG_INFO("%s: (%u, %u) Attribute <%s> Index %u Length %u\n", __FUNCTION__, params->network_id, params->controller_id, ctrlm_rcu_rib_attr_id_str(params->attribute_id), params->attribute_index, params->length);
   }

   if(params->length > CTRLM_RCU_MAX_RIB_ATTRIBUTE_SIZE) {
      LOG_ERROR("%s: Invalid length %u\n", __FUNCTION__, params->length);
      return(false);
   }
   sem_t semaphore;
   ctrlm_rib_request_cmd_result_t cmd_result = CTRLM_RIB_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_rib_t msg;
   memset(&msg, 0, sizeof(msg));

   sem_init(&semaphore, 0, 0);

   msg.controller_id     = params->controller_id;
   msg.attribute_id      = params->attribute_id;
   msg.attribute_index   = params->attribute_index;
   msg.length            = params->length;
   msg.length_out        = 0;
   msg.data              = (guchar *)params->data;
   msg.semaphore         = &semaphore;
   msg.cmd_result        = &cmd_result;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_rib_set, &msg, sizeof(msg), NULL, params->network_id);

   // Wait for the result condition to be signaled
   while(CTRLM_RIB_REQUEST_PENDING == cmd_result) {
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_RIB_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

gboolean ctrlm_rcu_controller_link_key(ctrlm_rcu_iarm_call_controller_link_key_t *params) {
   LOG_INFO("%s: (%u, %u)\n", __FUNCTION__, params->network_id, params->controller_id);

   if(params->network_id == CTRLM_MAIN_NETWORK_ID_ALL || params->controller_id == CTRLM_MAIN_CONTROLLER_ID_ALL) {
      LOG_ERROR("%s: Cannot get status for multiple controllers\n", __FUNCTION__);
      return(false);
   }
   sem_t semaphore;
   ctrlm_controller_status_cmd_result_t cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_controller_link_key_t msg;
   memset(&msg, 0, sizeof(msg));

   sem_init(&semaphore, 0, 0);

   msg.controller_id     = params->controller_id;
   msg.link_key          = params->link_key;
   msg.semaphore         = &semaphore;
   msg.cmd_result        = &cmd_result;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_controller_link_key, &msg, sizeof(msg), NULL, params->network_id);

   // Wait for the result condition to be signaled
   while(CTRLM_CONTROLLER_STATUS_REQUEST_PENDING == cmd_result) {
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_CONTROLLER_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

gboolean ctrlm_rcu_reverse_cmd(ctrlm_main_iarm_call_rcu_reverse_cmd_t *params) {
   LOG_INFO("%s: (%s, %s)\n", __FUNCTION__, ctrlm_network_type_str(params->network_type).c_str(), ctrlm_controller_name_str(params->controller_id).c_str());
#if (CTRLM_HAL_RF4CE_API_VERSION < 14)
   LOG_ERROR("%s: Reverse command is supported with Ctrlm HAL API >= 14\n", __FUNCTION__);
   return(false);
#endif

   sem_t semaphore;
   ctrlm_controller_status_cmd_result_t cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   unsigned int msg_size = sizeof(ctrlm_main_queue_msg_rcu_reverse_cmd_t) + params->total_size - sizeof(ctrlm_main_iarm_call_rcu_reverse_cmd_t);
   ctrlm_main_queue_msg_rcu_reverse_cmd_t *msg = (ctrlm_main_queue_msg_rcu_reverse_cmd_t *)g_malloc(msg_size);

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_CONTROLLER_REVERSE_CMD;
   msg->header.network_id = ctrlm_network_id_get(params->network_type);
   msg->controller_id     = params->controller_id;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   memcpy(&msg->reverse_command, params, params->total_size);

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);

   // Return reverse command result
   params->cmd_result = msg->reverse_command.cmd_result;

   if(cmd_result == CTRLM_CONTROLLER_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}


gboolean ctrlm_rcu_controller_type_get(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_rcu_controller_type_t *type) {
   LOG_INFO("%s: (%u, %u)\n", __FUNCTION__, network_id, controller_id);

   if(network_id == CTRLM_MAIN_NETWORK_ID_ALL || controller_id == CTRLM_MAIN_CONTROLLER_ID_ALL) {
      LOG_ERROR("%s: Cannot get type for multiple controllers\n", __FUNCTION__);
      return(false);
   }

   sem_t semaphore;
   ctrlm_controller_status_cmd_result_t cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_controller_type_get_t *msg = (ctrlm_main_queue_msg_controller_type_get_t *)g_malloc(sizeof(ctrlm_main_queue_msg_controller_type_get_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_CONTROLLER_TYPE_GET;
   msg->header.network_id = network_id;
   msg->controller_id     = controller_id;
   msg->controller_type   = type;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_CONTROLLER_STATUS_REQUEST_PENDING == cmd_result) {
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_CONTROLLER_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

gboolean ctrlm_rcu_rf4ce_polling_action(ctrlm_rcu_iarm_call_rf4ce_polling_action_t *params) {
   LOG_INFO("%s: (%u, %u)\n", __FUNCTION__, params->network_id, params->controller_id);

   sem_t semaphore;
   ctrlm_controller_status_cmd_result_t cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_rcu_polling_action_t msg;
   memset(&msg, 0, sizeof(msg));

   sem_init(&semaphore, 0, 0);

   msg.header.type        = CTRLM_MAIN_QUEUE_MSG_TYPE_RCU_POLLING_ACTION;
   msg.header.network_id  = params->network_id;
   msg.controller_id      = params->controller_id;
   msg.action             = params->action;
   msg.semaphore          = &semaphore;
   msg.cmd_result         = &cmd_result;
   memcpy(msg.data, params->data, sizeof(msg.data));

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_polling_action_push, &msg, sizeof(msg), NULL, params->network_id);

   // Wait for the result condition to be signaled
   while(CTRLM_CONTROLLER_STATUS_REQUEST_PENDING == cmd_result) {
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_CONTROLLER_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}
