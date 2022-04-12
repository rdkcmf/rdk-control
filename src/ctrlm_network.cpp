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
#include <glib.h>
#include <string.h>
#include "ctrlm.h"
#include "ctrlm_network.h"
#include "ctrlm_recovery.h"
#include "ctrlm_rcu.h"

using namespace std;

void ctrlm_network_property_set(ctrlm_network_id_t network_id, ctrlm_hal_network_property_t property, void *value, guint32 length) {
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_network_property_set_t *msg = (ctrlm_main_queue_msg_network_property_set_t *)g_malloc(sizeof(ctrlm_main_queue_msg_network_property_set_t) + length);

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return;
   }
   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_NETWORK_PROPERTY_SET;
   msg->header.network_id = network_id;
   msg->property          = property;
   msg->value             = (void *)&msg[1];

   errno_t safec_rc = memcpy_s(msg->value, length, value, length);
   ERR_CHK(safec_rc);

   ctrlm_main_queue_msg_push(msg);
}

ctrlm_obj_network_t::ctrlm_obj_network_t(ctrlm_network_type_t type, ctrlm_network_id_t id, const char *name, gboolean mask_key_codes, GThread *original_thread) :
   type_(type),
   id_(id),
   name_(name),
   mask_key_codes_(mask_key_codes),
   receiver_id_(""),
   device_id_(""),
   service_account_id_(""),
   partner_id_(""),
   experience_(""),
   stb_name_(""),
   hal_api_main_(NULL),
   hal_api_property_get_(NULL),
   hal_api_property_set_(NULL),
   hal_api_term_(NULL),
   mutex_cond_init_(FALSE)
{
   LOG_INFO("ctrlm_obj_network_t constructor - Type (%u) Id (%u) Name (%s)\n", type_, id_, name_.c_str());
   version_         = "unknown";
   init_result_     = CTRLM_HAL_RESULT_ERROR;
   ready_           = false;
   hal_thread_      = NULL;
   failed_state_    = false;
   original_thread_ = original_thread;
   ctrlm_main_thread_ = NULL;
   g_cond_clear(&condition_);  //CID:82418 - Uninit_ctor
}

ctrlm_obj_network_t::ctrlm_obj_network_t() {
   LOG_INFO("ctrlm_obj_network_t constructor - default\n");
   init_result_     = CTRLM_HAL_RESULT_ERROR;
   ready_           = false;
   hal_thread_      = NULL;
   failed_state_    = false;
   ctrlm_main_thread_ = NULL;
   g_cond_clear(&condition_);  //CID:89792 - Uninit_ctor
   hal_api_main_ = NULL;
   hal_api_property_get_ = NULL;
   hal_api_property_set_ = NULL;
   hal_api_term_ = NULL;
   mutex_cond_init_ = FALSE;
}

ctrlm_obj_network_t::~ctrlm_obj_network_t() {
   LOG_INFO("ctrlm_obj_network_t deconstructor - Type (%u) Id (%u) Name (%s)\n", type_, id_, name_.c_str());
   if(NULL != hal_api_term_) {
      gint64                    timeout;

      if(FALSE == mutex_cond_init_) {
         g_cond_init(&condition_);
         g_mutex_init(&mutex_);
      }

      if(FALSE == g_mutex_trylock(&mutex_)) {
         LOG_WARN("%s: Could not obtain mutex\n", __FUNCTION__);
      } else {
         ctrlm_network_term_hal_t *term_data;

         if(NULL == (term_data = (ctrlm_network_term_hal_t *)g_malloc(sizeof(ctrlm_network_term_hal_t)))) {
            LOG_ERROR("%s: Failed to allocate ctrlm_network_term_hal_t\n", __FUNCTION__);
            return;
         }

         term_data->cond       = &condition_;
         term_data->term       = hal_api_term_;
         term_data->hal_thread = hal_thread_;

         g_thread_new("hal_term_thread", terminate_hal, term_data);
         timeout = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
         if(FALSE == g_cond_wait_until(&condition_, &mutex_, timeout)) {
            LOG_WARN("%s: HAL termination timed out\n", __FUNCTION__);
         }
      }
   }
}

ctrlm_network_id_t ctrlm_obj_network_t::network_id_get() const {
   THREAD_ID_VALIDATE();
   return(id_);
}
ctrlm_network_type_t ctrlm_obj_network_t::type_get() const {
   THREAD_ID_VALIDATE();
   return(type_);
}

const char * ctrlm_obj_network_t::name_get() const {
   THREAD_ID_VALIDATE();
   return(name_.c_str());
}

std::string ctrlm_obj_network_t::db_name_get() const {
   THREAD_ID_VALIDATE();
   return("network");
}

void ctrlm_obj_network_t::mask_key_codes_set(gboolean mask_key_codes) {
   THREAD_ID_VALIDATE();
   mask_key_codes_ = mask_key_codes;
}

gboolean ctrlm_obj_network_t::mask_key_codes_get() const {
   THREAD_ID_VALIDATE();
   return(mask_key_codes_);
}

const char * ctrlm_obj_network_t::version_get() const {
   THREAD_ID_VALIDATE();
   return(version_.c_str());
}

void ctrlm_obj_network_t::receiver_id_set(const string& receiver_id) {
   THREAD_ID_VALIDATE();
   receiver_id_ = receiver_id;
}

string ctrlm_obj_network_t::receiver_id_get() const {
   THREAD_ID_VALIDATE();
   return(receiver_id_);
}

void ctrlm_obj_network_t::device_id_set(const string& device_id) {
   THREAD_ID_VALIDATE();
    device_id_ = device_id;
}

string ctrlm_obj_network_t::device_id_get() const {
   THREAD_ID_VALIDATE();
    return(device_id_);
}

void ctrlm_obj_network_t::service_account_id_set(const string& service_account_id) {
   THREAD_ID_VALIDATE();
   service_account_id_ = service_account_id;
}

string ctrlm_obj_network_t::service_account_id_get() const {
   THREAD_ID_VALIDATE();
   return(service_account_id_);
}

void ctrlm_obj_network_t::partner_id_set(const string& partner_id) {
   THREAD_ID_VALIDATE();
   partner_id_ = partner_id;
}

string ctrlm_obj_network_t::partner_id_get() const {
   THREAD_ID_VALIDATE();
   return(partner_id_);
}
 
void ctrlm_obj_network_t::experience_set(const string& experience) {
   THREAD_ID_VALIDATE();
   experience_ = experience;
}

string ctrlm_obj_network_t::experience_get() const {
   THREAD_ID_VALIDATE();
   return(experience_);
}

void ctrlm_obj_network_t::stb_name_set(const string& stb_name) {
   THREAD_ID_VALIDATE();
   LOG_INFO("%s: STB Name <%s>\n", __FUNCTION__, stb_name.c_str());
   stb_name_ = stb_name;
}

string ctrlm_obj_network_t::stb_name_get() const {
   THREAD_ID_VALIDATE();
   return(stb_name_);
}

bool ctrlm_obj_network_t::is_ready() const {
   THREAD_ID_VALIDATE();
   return(ready_);
}

void ctrlm_obj_network_t::hal_api_main_set(ctrlm_hal_network_main_t main) {
   THREAD_ID_VALIDATE();
   hal_api_main_ = main;
}

void ctrlm_obj_network_t::hal_api_set(ctrlm_hal_req_property_get_t property_get, ctrlm_hal_req_property_set_t property_set, ctrlm_hal_req_term_t term) {
   THREAD_ID_VALIDATE();
   LOG_INFO("%s: %s property get %p set %p term %p.\n", __FUNCTION__, name_.c_str(), property_get, property_set, term);
   hal_api_property_get_ = property_get;
   hal_api_property_set_ = property_set;
   hal_api_term_         = term;
}

ctrlm_hal_result_t ctrlm_obj_network_t::hal_init_request(GThread *ctrlm_main_thread) {
   ctrlm_hal_main_init_t main_init;
   ctrlm_main_thread_ = ctrlm_main_thread;
   THREAD_ID_VALIDATE();
   
   // Initialization parameters
   main_init.network_id = id_;

   // Initialize condition and mutex
   g_cond_init(&condition_);
   g_mutex_init(&mutex_);
   mutex_cond_init_ = TRUE;

   g_mutex_lock(&mutex_);
   g_thread_new(name_.c_str(), (void* (*)(void*))hal_api_main_, &main_init);

   // Block until initialization is complete or a timeout occurs
   LOG_INFO("%s: Waiting for %s initialization...\n", __FUNCTION__, name_.c_str());
   g_cond_wait(&condition_, &mutex_);
   g_mutex_unlock(&mutex_);
   
   if(CTRLM_HAL_RESULT_SUCCESS == init_result_) {
      ready_ = true;
   }
   
   return(init_result_);
}

void ctrlm_obj_network_t::hal_init_confirm(ctrlm_hal_result_t result, const char *version) {
   THREAD_ID_VALIDATE();
   
   init_result_ = result;
   
   if(NULL != version)   {
      version_ = version;
   }
   LOG_INFO("%s: %s initialization complete.\n", __FUNCTION__, name_.c_str());
   // Unblock the caller of hal_init
   g_cond_broadcast(&condition_);
}

void ctrlm_obj_network_t::thread_monitor_poll(ctrlm_thread_monitor_response_t *response) {
   THREAD_ID_VALIDATE();
   ctrlm_hal_network_property_thread_monitor_t thread_monitor;
   LOG_DEBUG("%s: %s enter\n", __FUNCTION__, name_.c_str());
   thread_monitor.response = (ctrlm_hal_thread_monitor_response_t *)response;
   property_set(CTRLM_HAL_NETWORK_PROPERTY_THREAD_MONITOR, (void *)&thread_monitor);
   LOG_DEBUG("%s: %s exit\n", __FUNCTION__, name_.c_str());
}

ctrlm_hal_result_t ctrlm_obj_network_t::property_get(ctrlm_hal_network_property_t property, void **value) {
   THREAD_ID_VALIDATE();
   if(hal_api_property_get_ == NULL) {
      LOG_ERROR("%s: NULL HAL API\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR);
   }
   return(hal_api_property_get_(property, value));
}

ctrlm_hal_result_t ctrlm_obj_network_t::property_set(ctrlm_hal_network_property_t property, void *value) {
   THREAD_ID_VALIDATE();
   if(hal_api_property_set_ == NULL) {
      LOG_ERROR("%s: NULL HAL API\n", __FUNCTION__);
      return(CTRLM_HAL_RESULT_ERROR);
   }
   return(hal_api_property_set_(property, value));
}

void ctrlm_obj_network_t::disable_hal_calls() {
   LOG_INFO("%s: from ctrlm_obj_network_t\n", __FUNCTION__);
   hal_api_property_get_ = NULL;
   hal_api_property_set_ = NULL;
   hal_api_term_ = NULL;
}

gpointer ctrlm_obj_network_t::terminate_hal(gpointer data) {
   ctrlm_network_term_hal_t *term_data = (ctrlm_network_term_hal_t *)data;

   if(NULL == term_data) {
      LOG_ERROR("%s: Termination data is null\n", __FUNCTION__);
      return NULL;
   }

   if(NULL == term_data->term) {
      LOG_INFO("%s: hal_api_term_ is NULL\n", __FUNCTION__);
      return NULL;
   }

   ctrlm_hal_result_t res = term_data->term();
   if(CTRLM_HAL_RESULT_SUCCESS != res) {
      LOG_ERROR("%s: HAL req term failed with code <%d>\n", __FUNCTION__, (int)res);
   } else {
      LOG_INFO("%s: HAL terminated successfully\n", __FUNCTION__);
   }

   if(CTRLM_HAL_RESULT_SUCCESS == res && term_data->hal_thread) {
      g_thread_join(term_data->hal_thread);
   }

   if(term_data->cond) {
      g_cond_signal(term_data->cond);
   }
   return NULL;
}

const char *ctrlm_obj_network_t::get_thread_name(const GThread *thread_id) const {
   if(thread_id == ctrlm_main_thread_) {
      return "ctrlm_main_thread";
   } else if (thread_id == original_thread_) {
      return "original_thread";
   } else if (thread_id == hal_thread_) {
      return "hal_thread";
   } else {
      return "unknown_thread";
   }
}
void ctrlm_obj_network_t::thread_id_validate(const char *pCallingFunction) const {
   GThread *thread_self = g_thread_self();
   //Check for ctrlm_main_thread
   if(thread_self != ctrlm_main_thread_) {
      //Check for original thread until ctrlm_main_thread is created
      if(!ctrlm_main_successful_init_get()) {
         if(thread_self != original_thread_) {
            LOG_INFO("%s: Method <%s>: called from <%s> <%p>, not original_thread<%p> or ctrlm_main_thread<%p>!!!!!\n", __FUNCTION__, pCallingFunction, get_thread_name(thread_self), thread_self, original_thread_, ctrlm_main_thread_);
            if(!ctrlm_is_production_build()) {
               g_assert(0);
            }
         }
      } else {
         LOG_INFO("%s: Method <%s>: called from <%s> <%p>, not ctrlm_main_thread<%p>!!!!!\n", __FUNCTION__, pCallingFunction, get_thread_name(thread_self), thread_self, ctrlm_main_thread_);
         if(!ctrlm_is_production_build()) {
            g_assert(0);
         }
      }
   }
}

bool ctrlm_obj_network_t::message_dispatch(gpointer msg){
   ctrlm_main_queue_msg_header_t *hdr = (ctrlm_main_queue_msg_header_t *)msg;
   LOG_INFO("ctrlm_obj_network_t::message_dispatch:  message type %u\n",  hdr->type);
   return false;
}

void ctrlm_obj_network_t::hal_init_complete(){
}

void ctrlm_obj_network_t::controller_list_get(std::vector<ctrlm_controller_id_t>& list) const {
}


ctrlm_rcu_controller_type_t ctrlm_obj_network_t::ctrlm_controller_type_get(ctrlm_controller_id_t controller_id){
   return(CTRLM_RCU_CONTROLLER_TYPE_INVALID);
}

ctrlm_rcu_binding_type_t ctrlm_obj_network_t::ctrlm_binding_type_get(ctrlm_controller_id_t controller_id){
   return(CTRLM_RCU_BINDING_TYPE_INVALID);
}

void ctrlm_obj_network_t::ctrlm_controller_status_get(ctrlm_controller_id_t controller_id, void *status) {
  LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::ind_process_voice_session_request(void *data, int size){
  LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::ind_process_voice_session_stop(void *data, int size){
  LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::ind_process_voice_session_begin(void *data, int size){
  LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::ind_process_voice_session_end(void *data, int size){
  LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::ind_process_voice_session_result(void *data, int size) {
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::process_voice_controller_metrics(void *data, int size) {
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::voice_command_status_set(void *data, int size){
  LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
}

gboolean ctrlm_obj_network_t::key_event_hook(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_key_status_t key_status, ctrlm_key_code_t key_code) {
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   return true;
}

gboolean ctrlm_obj_network_t::terminate_voice_session(ctrlm_voice_session_termination_reason_t reason) {
   return true;
}


void ctrlm_obj_network_t::set_timers() {
   LOG_INFO("%s\n", __FUNCTION__);
}

void ctrlm_obj_network_t::xconf_configuration() {
   LOG_INFO("%s\n", __FUNCTION__);
}
 
bool ctrlm_obj_network_t::is_fmr_supported() const {
   LOG_ERROR("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   return false;
}

ctrlm_controller_status_cmd_result_t ctrlm_obj_network_t::req_process_reverse_cmd(ctrlm_main_queue_msg_rcu_reverse_cmd_t *dqm) {
   LOG_ERROR("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   return CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
}

bool ctrlm_obj_network_t::controller_exists(ctrlm_controller_id_t controller_id) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
   return false;
}

void ctrlm_obj_network_t::controller_exists(void *data, int size) {
   ctrlm_main_queue_msg_controller_exists_t *dqm = (ctrlm_main_queue_msg_controller_exists_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_controller_exists_t));

   if(dqm->semaphore != NULL && dqm->controller_exists != NULL) {
      *dqm->controller_exists = controller_exists(dqm->controller_id);
      sem_post(dqm->semaphore);
   }
}

json_t *ctrlm_obj_network_t::xconf_export_controllers() {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
   return NULL;
}

ctrlm_hal_result_t ctrlm_obj_network_t::network_init(GThread *ctrlm_main_thread) {
   ctrlm_hal_result_t result = CTRLM_HAL_RESULT_SUCCESS;

   LOG_INFO("%s: Initializing %s network\n", __FUNCTION__, name_get());

   result = hal_init_request(ctrlm_main_thread);
#if CTRLM_HAL_RF4CE_API_VERSION >= 9
   // Create file flag to show that failed init was due to NVM
   if(CTRLM_HAL_RESULT_ERROR_INVALID_NVM == result) {
      guint invalid_nvm = 1;
      ctrlm_recovery_property_set(CTRLM_RECOVERY_INVALID_HAL_NVM, &invalid_nvm);
   }
#endif

   if(CTRLM_HAL_RESULT_SUCCESS != result) { // Error occurred
      LOG_FATAL("%s: Failed to initialize %s network\n", __FUNCTION__, name_get());
   } else {
      LOG_INFO("%s: Initialized %s network version %s\n", __FUNCTION__, name_get(), version_get());
      // Post HAL initialization
      hal_init_complete();
      // Xconf
      xconf_configuration();
   }

   return(result);
}
 
void ctrlm_obj_network_t::network_destroy() {
   LOG_INFO("%s: Destroying %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::req_process_controller_link_key(void *data, int size) {
   ctrlm_main_queue_msg_controller_link_key_t *dqm = (ctrlm_main_queue_msg_controller_link_key_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_controller_link_key_t));

   if(dqm->cmd_result && *dqm->cmd_result == CTRLM_CONTROLLER_STATUS_REQUEST_PENDING) {
      LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
      *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_controller_status(void *data, int size) {
   ctrlm_main_queue_msg_controller_status_t *dqm = (ctrlm_main_queue_msg_controller_status_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_controller_status_t));

   if(dqm->cmd_result && *dqm->cmd_result == CTRLM_CONTROLLER_STATUS_REQUEST_PENDING) {
      LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
      *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
   }
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_controller_product_name(void *data, int size) {
   ctrlm_main_queue_msg_product_name_t *dqm = (ctrlm_main_queue_msg_product_name_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_product_name_t));

   if(dqm->cmd_result && *dqm->cmd_result == CTRLM_MAIN_STATUS_REQUEST_PENDING) {
      LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
      *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_ERROR;
   }

   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::hal_init_cfm(void *data, int size) {
   ctrlm_main_queue_msg_hal_cfm_init_t *dqm = (ctrlm_main_queue_msg_hal_cfm_init_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_hal_cfm_init_t));

   // The logic here is pretty network specific
   // Do nothing

   if(dqm->semaphore != NULL) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_rib_set(void *data, int size) {
   ctrlm_main_queue_msg_rib_t *dqm = (ctrlm_main_queue_msg_rib_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_rib_t));

   if(dqm->cmd_result && *dqm->cmd_result == CTRLM_RIB_REQUEST_PENDING) {
      LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
      *dqm->cmd_result = CTRLM_RIB_REQUEST_ERROR;
   }

   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }   
}

void ctrlm_obj_network_t::req_process_rib_get(void *data, int size) {
   ctrlm_main_queue_msg_rib_t *dqm = (ctrlm_main_queue_msg_rib_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_rib_t));

   if(dqm->cmd_result && *dqm->cmd_result == CTRLM_RIB_REQUEST_PENDING) {
      LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
      *dqm->cmd_result = CTRLM_RIB_REQUEST_ERROR;
   }

   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }   
}

void ctrlm_obj_network_t::req_process_network_status(void *data, int size) {
   ctrlm_main_queue_msg_main_network_status_t *dqm = (ctrlm_main_queue_msg_main_network_status_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_main_network_status_t));

   if(dqm->cmd_result && *dqm->cmd_result == CTRLM_MAIN_STATUS_REQUEST_PENDING) {
      LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
      dqm->status->result = CTRLM_IARM_CALL_RESULT_ERROR_NOT_SUPPORTED;
      *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_ERROR;
   }

   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_voice_session_begin(void *data, int size){
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   ctrlm_main_queue_msg_voice_session_t *dqm = (ctrlm_main_queue_msg_voice_session_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_t));

   // post the semaphore just to ensure nothing blocks
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_voice_session_end(void *data, int size){
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   ctrlm_main_queue_msg_voice_session_t *dqm = (ctrlm_main_queue_msg_voice_session_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_voice_session_t));

   // post the semaphore just to ensure nothing blocks
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_start_pairing(void *data, int size){
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   ctrlm_main_queue_msg_start_pairing_t *dqm = (ctrlm_main_queue_msg_start_pairing_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_start_pairing_t));

   // post the semaphore just to ensure nothing blocks
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_pair_with_code(void *data, int size){
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   ctrlm_main_queue_msg_pair_with_code_t *dqm = (ctrlm_main_queue_msg_pair_with_code_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_pair_with_code_t));

   // post the semaphore just to ensure nothing blocks
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_ir_set_code(void *data, int size){
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   ctrlm_main_queue_msg_ir_set_code_t *dqm = (ctrlm_main_queue_msg_ir_set_code_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_ir_set_code_t));

   // post the semaphore just to ensure nothing blocks
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_ir_clear_codes(void *data, int size){
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   ctrlm_main_queue_msg_ir_clear_t *dqm = (ctrlm_main_queue_msg_ir_clear_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_ir_clear_t));

   // post the semaphore just to ensure nothing blocks
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_find_my_remote(void *data, int size){
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   ctrlm_main_queue_msg_find_my_remote_t *dqm = (ctrlm_main_queue_msg_find_my_remote_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_find_my_remote_t));

   // post the semaphore just to ensure nothing blocks
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_get_rcu_status(void *data, int size){
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   ctrlm_main_queue_msg_get_rcu_status_t *dqm = (ctrlm_main_queue_msg_get_rcu_status_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_get_rcu_status_t));

   // post the semaphore just to ensure nothing blocks
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_get_last_keypress(void *data, int size){
   LOG_WARN("%s: request is not valid for %s network\n", __FUNCTION__, name_get());
   ctrlm_main_queue_msg_get_last_keypress_t *dqm = (ctrlm_main_queue_msg_get_last_keypress_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_get_last_keypress_t));

   // post the semaphore just to ensure nothing blocks
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_polling_action_push(void *data, int size) {
   ctrlm_main_queue_msg_rcu_polling_action_t *dqm = (ctrlm_main_queue_msg_rcu_polling_action_t *)data;
   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_rcu_polling_action_t));

   if(dqm->cmd_result && *dqm->cmd_result == CTRLM_CONTROLLER_STATUS_REQUEST_PENDING) {
      LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
      *dqm->cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
   }

   // post the semaphore just to ensure nothing blocks
   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_chip_status(void *data, int size) {
   ctrlm_main_queue_msg_main_chip_status_t *dqm = (ctrlm_main_queue_msg_main_chip_status_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_main_chip_status_t));
   g_assert(dqm->status != NULL);

   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
   dqm->status->result = CTRLM_IARM_CALL_RESULT_ERROR_NOT_SUPPORTED;

   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_network_managed_upgrade(void *data, int size) {
   ctrlm_main_queue_msg_network_fw_upgrade_t *dqm = (ctrlm_main_queue_msg_network_fw_upgrade_t *)data;

   g_assert(dqm);
   g_assert(size == sizeof(ctrlm_main_queue_msg_network_fw_upgrade_t));
   g_assert(dqm->network_managing_upgrade != NULL);

   LOG_INFO("%s: not implemented for %s network\n", __FUNCTION__, name_get());
   *dqm->network_managing_upgrade = false;

   if(dqm->semaphore) {
      sem_post(dqm->semaphore);
   }
}

void ctrlm_obj_network_t::req_process_network_continue_upgrade(void *data, int size) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::factory_reset() {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::controller_unbind(ctrlm_controller_id_t controller_id, ctrlm_unbind_reason_t reason) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
}

bool ctrlm_obj_network_t::binding_config_set(ctrlm_controller_bind_config_t conf) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
   return false;
}

bool ctrlm_obj_network_t::discovery_config_set(ctrlm_controller_discovery_config_t conf) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
   return false;
}

void ctrlm_obj_network_t::cs_values_set(const ctrlm_cs_values_t *values, bool db_load) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::recovery_set(ctrlm_recovery_type_t recovery) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
}

bool ctrlm_obj_network_t::backup_hal_nvm() {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
   return false;
}

void ctrlm_obj_network_t::bind_validation_begin(ctrlm_main_queue_msg_bind_validation_begin_t *dqm) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
}

void ctrlm_obj_network_t::bind_validation_end(ctrlm_main_queue_msg_bind_validation_end_t *dqm) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
}

bool ctrlm_obj_network_t::bind_validation_timeout(ctrlm_controller_id_t controller_id) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
   return false;
}

bool ctrlm_obj_network_t::is_failed_state() const {
   return (failed_state_);
}

bool ctrlm_obj_network_t::analyze_assert_reason(const char *assert_info ) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
   return (false);
}

void ctrlm_obj_network_t::power_state_change(ctrlm_main_queue_power_state_change_t *dqm) {
   LOG_WARN("%s: not implemented for %s network\n", __FUNCTION__, name_get());
}
