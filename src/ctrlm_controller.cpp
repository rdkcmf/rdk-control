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
#include "ctrlm.h"
#include "ctrlm_network.h"

using namespace std;

#define OTA_MAX_RETRIES                                  (2)

ctrlm_obj_controller_t::ctrlm_obj_controller_t(ctrlm_controller_id_t controller_id, ctrlm_obj_network_t &network) :
   controller_id_(controller_id),
   obj_network_(&network),
   irdb_entry_id_name_tv_("0"),
   irdb_entry_id_name_avr_("0"),
   ota_failure_cnt_from_last_success_(0),
   ota_failure_cnt_session_(0),
   ota_failure_type_z_cnt_(0)
{
   LOG_INFO("ctrlm_obj_controller_t constructor - %u\n", controller_id_);
   
}

ctrlm_obj_controller_t::ctrlm_obj_controller_t() {
   controller_id_ = CTRLM_MAIN_CONTROLLER_ID_INVALID;
   obj_network_ = NULL;  //CID:82352 - Uninit_ctor
   LOG_INFO("ctrlm_obj_controller_t constructor - default\n");
}

ctrlm_obj_controller_t::~ctrlm_obj_controller_t() {
   LOG_INFO("ctrlm_obj_controller_t deconstructor - %u\n", controller_id_);
   
}

ctrlm_controller_id_t ctrlm_obj_controller_t::controller_id_get() const {
   return(controller_id_);
}

ctrlm_network_id_t ctrlm_obj_controller_t::network_id_get() const {
   return(obj_network_->network_id_get());
}

string ctrlm_obj_controller_t::receiver_id_get() const {
   return(obj_network_->receiver_id_get());
}

string ctrlm_obj_controller_t::device_id_get() const {
   return(obj_network_->device_id_get());
}

string ctrlm_obj_controller_t::service_account_id_get() const {
   return(obj_network_->service_account_id_get());
}

string ctrlm_obj_controller_t::partner_id_get() const {
   return(obj_network_->partner_id_get());
}

string ctrlm_obj_controller_t::experience_get() const {
   return(obj_network_->experience_get());
}

string ctrlm_obj_controller_t::stb_name_get() const {
   return(obj_network_->stb_name_get());
}

guchar ctrlm_obj_controller_t::property_write_irdb_entry_id_name_tv(guchar *data, guchar length) {
   LOG_WARN("%s: request is not valid\n", __FUNCTION__);
   return 0;
}

guchar ctrlm_obj_controller_t::property_write_irdb_entry_id_name_avr(guchar *data, guchar length) {
   LOG_WARN("%s: request is not valid\n", __FUNCTION__);
   return 0;
}
guchar ctrlm_obj_controller_t::property_read_irdb_entry_id_name_tv(guchar *data, guchar length) {
   LOG_WARN("%s: request is not valid\n", __FUNCTION__);
   return 0;
}

guchar ctrlm_obj_controller_t::property_read_irdb_entry_id_name_avr(guchar *data, guchar length) {
   LOG_WARN("%s: request is not valid\n", __FUNCTION__);
   return 0;
}

ctrlm_controller_capabilities_t ctrlm_obj_controller_t::get_capabilities() const {
   return(ctrlm_controller_capabilities_t()); // return empty capabilities object
}

void ctrlm_obj_controller_t::irdb_entry_id_name_set(ctrlm_irdb_dev_type_t type, ctrlm_irdb_ir_entry_id_t irdb_entry_id_name) {
   switch(type) {
      case CTRLM_IRDB_DEV_TYPE_TV:
         property_write_irdb_entry_id_name_tv((guchar *)irdb_entry_id_name.c_str(), CTRLM_MAX_PARAM_STR_LEN);
         break;
      case CTRLM_IRDB_DEV_TYPE_AVR:
         property_write_irdb_entry_id_name_avr((guchar *)irdb_entry_id_name.c_str(), CTRLM_MAX_PARAM_STR_LEN);
         break;
      default:
         LOG_WARN("%s: Invalid type <%d>\n", __FUNCTION__, type);
         break;
   }
}

ctrlm_irdb_ir_entry_id_t ctrlm_obj_controller_t::get_irdb_entry_id_name_tv() {
   return irdb_entry_id_name_tv_;
}

ctrlm_irdb_ir_entry_id_t ctrlm_obj_controller_t::get_irdb_entry_id_name_avr() {
   return irdb_entry_id_name_avr_;
}


void ctrlm_obj_controller_t::ota_failure_cnt_incr() {
   ota_failure_cnt_session_++;
   ota_failure_cnt_from_last_success_++;
   LOG_DEBUG("%s ota_failure_cnt_session_ = %d, ota_failure_cnt_from_last_success_ = %d\n", __FUNCTION__, ota_failure_cnt_session_, ota_failure_cnt_from_last_success_);
}

void ctrlm_obj_controller_t::ota_clear_all_failure_counters() {
   ota_failure_cnt_session_ = 0;
   ota_failure_cnt_from_last_success_ = 0;
   ota_failure_type_z_cnt_set(0);
   LOG_DEBUG("%s all OTA failure counters reset to 0\n", __FUNCTION__);
}

void ctrlm_obj_controller_t::ota_failure_cnt_session_clear() {
   ota_failure_cnt_session_ = 0;
   LOG_DEBUG("%s ota_failure_cnt_session_ reset to 0\n", __FUNCTION__);
}

bool ctrlm_obj_controller_t::retry_ota() const {
   bool retry = ota_failure_cnt_session_ < OTA_MAX_RETRIES;
   LOG_WARN("%s ota_failure_cnt_session_ = <%d>, retry OTA = <%s>\n", __FUNCTION__,ota_failure_cnt_session_,retry ? "TRUE":"FALSE");
   return retry;
}

void ctrlm_obj_controller_t::ota_failure_type_z_cnt_set(uint8_t ota_failures) {
   ota_failure_type_z_cnt_  = (ota_failures >= 4) ? 0 : ota_failures;
}

uint8_t ctrlm_obj_controller_t::ota_failure_type_z_cnt_get(void) const {
   return ota_failure_type_z_cnt_;
}

bool ctrlm_obj_controller_t::is_controller_type_z(void) const {
   return (ota_failure_type_z_cnt_ >= 2) ? true : false;
}
