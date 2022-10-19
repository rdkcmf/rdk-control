/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2014 RDK Management
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
// Defines
#define CTRLM_BLE_TIME_STR_LEN (20)
// End Defines

// Includes

#include <string>
#include <string.h>
#include "../ctrlm.h"
#include "../ctrlm_utils.h"
#include "../ctrlm_log.h"
#include "ctrlm_database.h"
#include "ctrlm_ble_network.h"
#include "ctrlm_ble_controller.h"
#include "ctrlm_ble_utils.h"
#include "../ctrlm_controller.h"
#include "ctrlm_hal_ip.h"

#include <sstream>
#include <iterator>
#include <iostream>

using namespace std;

// End Includes

#define BROADCAST_PRODUCT_NAME_PR1                       ("Platco PR1")
#define XCONF_PRODUCT_NAME_PR1                           ("PR1-10")

#define BROADCAST_PRODUCT_NAME_LC103                     ("SkyQ LC103")
#define BROADCAST_PRODUCT_NAME_LC203                     ("SkyQ LC203")
#define XCONF_PRODUCT_NAME_LC103                         ("LC103-10")

#define BROADCAST_PRODUCT_NAME_EC302                     ("SkyQ EC302")
#define XCONF_PRODUCT_NAME_EC302                         ("EC302-10")

// map to convert a key code to its identifiable name on the remote.
// map<key code, tuple<name, masked_name>>
static const map<uint16_t, tuple<const char*, const char*>> ctrlm_ble_key_names {
   {KEY_1,             {"1",                 "Numeric"}},
   {KEY_2,             {"2",                 "Numeric"}},
   {KEY_3,             {"3",                 "Numeric"}},
   {KEY_4,             {"4",                 "Numeric"}},
   {KEY_5,             {"5",                 "Numeric"}},
   {KEY_6,             {"6",                 "Numeric"}},
   {KEY_7,             {"7",                 "Numeric"}},
   {KEY_8,             {"8",                 "Numeric"}},
   {KEY_9,             {"9",                 "Numeric"}},
   {KEY_0,             {"0",                 "Numeric"}},
   {KEY_F1,            {"Standby",           "Standby"}},
   {KEY_F15,           {"Input",             "Input"}},
   {KEY_UP,            {"Up",                "Directional"}},
   {KEY_LEFT,          {"Left",              "Directional"}},
   {KEY_RIGHT,         {"Right",             "Directional"}},
   {KEY_DOWN,          {"Down",              "Directional"}},
   {KEY_ENTER,         {"Select",            "Select"}},
   {KEY_KPPLUS,        {"Volume+",           "Volume+"}},
   {KEY_KPMINUS,       {"Volume-",           "Volume-"}},
   {KEY_KPASTERISK,    {"Mute",              "Mute"}},
   {KEY_HOME,          {"Home",              "Home"}},
   {KEY_F17,           {"Proximity Sensor",  "Proximity Sensor"}},
   {KEY_F8,            {"Voice",             "Voice"}},
   {KEY_ESC,           {"Dismiss",           "Dismiss"}},
   {KEY_F9,            {"Info",              "Info"}},
   {KEY_BATTERY,       {"Battery Low",       "Battery Low"}},
   {KEY_F16,           {"Plus",              "Plus"}},
   {KEY_F13,           {"Option",            "Option"}},
   {KEY_F4,            {"Red",               "Red"}},
   {KEY_DELETE,        {"Green",             "Green"}},
   {KEY_INSERT,        {"Yellow",            "Yellow"}},
   {KEY_END,           {"Blue",              "Blue"}},
   {KEY_PAGEUP,        {"Channel+",          "Channel+"}},
   {KEY_PAGEDOWN,      {"Channel-",          "Channel-"}},
   {KEY_F2,            {"Help/Cog",          "Help/Cog"}},
   {KEY_F5,            {"Apps",              "Apps"}},
   {KEY_F10,           {"Rewind",            "Rewind"}},
   {KEY_F11,           {"Play",              "Play"}},
   {KEY_F12,           {"FFwd",              "FFwd"}},
   {KEY_F7,            {"Record",            "Record"}},
   {KEY_F3,            {"Search",            "Search"}},
   {KEY_F6,            {"Sky",               "Sky"}},
   {KEY_F14,           {"Quick Access",      "Quick Access"}},
   {KEY_KPRIGHTPAREN,  {"App 1",             "App 1"}},
   {KEY_KPLEFTPAREN,   {"App 2",             "App 2"}},
   {KEY_KPCOMMA,       {"App 3",             "App 3"}}
};


// Function Implementations

ctrlm_obj_controller_ble_t::ctrlm_obj_controller_ble_t(ctrlm_controller_id_t controller_id, ctrlm_obj_network_ble_t &network, ctrlm_ble_result_validation_t validation_result) : 
   ctrlm_obj_controller_t(controller_id, network),
   obj_network_ble_(&network),
   controller_type_(BLE_CONTROLLER_TYPE_UNKNOWN),
   product_name_("UNKNOWN"),
   ieee_address_(0),
   device_id_(0),
   manufacturer_(),
   model_(),
   upgrade_in_progress_(false),
   upgrade_attempted_(false),
   upgrade_paused_(false),
   stored_in_db_(false),
   connected_(false),
   validation_result_(validation_result),
   time_binding_(0),
   last_key_time_(0),
   last_key_time_flush_(0), // First key will always get flushed to DB
   last_key_status_(CTRLM_KEY_STATUS_INVALID),
   last_key_code_(CTRLM_KEY_CODE_INVALID),
   last_wakeup_key_code_(CTRLM_KEY_CODE_INVALID),
   wakeup_config_(CTRLM_RCU_WAKEUP_CONFIG_INVALID),
   wakeup_custom_list_(),
   battery_percent_(0),
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
   utterances_exceeding_packet_loss_threshold_yesterday_(0)
{
   LOG_INFO("%s: constructor - controller id <%u>\n", __FUNCTION__, controller_id);
}

ctrlm_obj_controller_ble_t::ctrlm_obj_controller_ble_t() {
   LOG_INFO("%s: default constructor\n", __FUNCTION__);
}

const char* ctrlm_obj_controller_ble_t::getKeyName(uint16_t code, bool mask) {
   if (ctrlm_ble_key_names.end() != ctrlm_ble_key_names.find(code)) {
      return mask ? get<1>(ctrlm_ble_key_names.at(code)) : get<0>(ctrlm_ble_key_names.at(code));
   } else {
      return "Unknown";
   }
}

void ctrlm_obj_controller_ble_t::db_create() {
   LOG_INFO("%s: Creating database for controller %u\n", __FUNCTION__, controller_id_get());
   ctrlm_db_ble_controller_create(network_id_get(), controller_id_get());
   stored_in_db_ = true;
}

void ctrlm_obj_controller_ble_t::db_destroy() {
   LOG_INFO("%s: Destroying database for controller %u\n", __FUNCTION__, controller_id_get());
   ctrlm_db_ble_controller_destroy(network_id_get(), controller_id_get());
   stored_in_db_ = false;
}

void ctrlm_obj_controller_ble_t::db_load() {
   ctrlm_network_id_t network_id = network_id_get();
   ctrlm_controller_id_t controller_id = controller_id_get();

   string name;
   ctrlm_db_ble_read_controller_name(network_id, controller_id, name);
   setName(name);
   ctrlm_db_ble_read_controller_manufacturer(network_id, controller_id, manufacturer_);
   ctrlm_db_ble_read_controller_model(network_id, controller_id, model_);
   ctrlm_db_ble_read_ieee_address(network_id, controller_id, ieee_address_);
   ctrlm_db_ble_read_time_binding(network_id, controller_id, time_binding_);
   ctrlm_db_ble_read_last_key_press(network_id, controller_id, last_key_time_, last_key_code_);
   ctrlm_db_ble_read_battery_percent(network_id, controller_id, battery_percent_);
   ctrlm_db_ble_read_fw_revision(network_id, controller_id, fw_revision_);
   string rev;
   ctrlm_db_ble_read_hw_revision(network_id, controller_id, rev);
   hw_revision_.loadFromString(rev);
   ctrlm_db_ble_read_sw_revision(network_id, controller_id, rev);
   sw_revision_.loadFromString(rev);
   ctrlm_db_ble_read_serial_number(network_id, controller_id, serial_number_);
   ctrlm_db_ble_read_device_id(network_id, controller_id, device_id_);

   guchar *data = NULL;
   guint32 length = 0;
   ctrlm_db_ble_read_voice_metrics(network_id, controller_id, &data, &length);
   if(NULL != data) {
      property_parse_voice_metrics(data, length);
   }
   ctrlm_db_free(data);

   data = NULL;
   length = 0;
   ctrlm_db_ble_read_irdb_entry_id_name_tv(network_id, controller_id, &data, &length);
   if(NULL != data) {
      property_read_irdb_entry_id_name_tv(data, length);
   }
   ctrlm_db_free(data);

   data = NULL;
   length = 0;
   ctrlm_db_ble_read_irdb_entry_id_name_avr(network_id, controller_id, &data, &length);
   if(NULL != data) {
      property_read_irdb_entry_id_name_avr(data, length);
   }
   ctrlm_db_free(data);

   ctrlm_db_ble_read_ota_failure_cnt_last_success(network_id, controller_id, ota_failure_cnt_from_last_success_);
   LOG_INFO("%s: Controller %s <%s> OTA total failures since last successful upgrade = %d\n", __FUNCTION__, ctrlm_ble_controller_type_str(controller_type_), ctrlm_convert_mac_long_to_string(ieee_address_).c_str(), ota_failure_cnt_from_last_success_);

   ctrlm_db_ble_read_ota_failure_type_z_count(network_id, controller_id, ota_failure_type_z_cnt_);
   LOG_INFO("%s: Controller %s <%s> OTA type Z failure count = %d.... is %s\n", __FUNCTION__, ctrlm_ble_controller_type_str(controller_type_), ctrlm_convert_mac_long_to_string(ieee_address_).c_str(), ota_failure_type_z_cnt_get(), is_controller_type_z() ? "TYPE Z" : "NOT TYPE Z");

   stored_in_db_ = true;
}

void ctrlm_obj_controller_ble_t::db_store() {
   ctrlm_network_id_t network_id = network_id_get();
   ctrlm_controller_id_t controller_id = controller_id_get();

   ctrlm_db_ble_write_controller_name(network_id, controller_id, product_name_);
   ctrlm_db_ble_write_controller_manufacturer(network_id, controller_id, manufacturer_);
   ctrlm_db_ble_write_controller_model(network_id, controller_id, model_);
   ctrlm_db_ble_write_ieee_address(network_id, controller_id, ieee_address_);
   ctrlm_db_ble_write_time_binding(network_id, controller_id, time_binding_);
   ctrlm_db_ble_write_last_key_press(network_id, controller_id, last_key_time_, last_key_code_);
   ctrlm_db_ble_write_battery_percent(network_id, controller_id, battery_percent_);
   ctrlm_db_ble_write_fw_revision(network_id, controller_id, fw_revision_);
   ctrlm_db_ble_write_hw_revision(network_id, controller_id, hw_revision_.toString());
   ctrlm_db_ble_write_sw_revision(network_id, controller_id, sw_revision_.toString());
   ctrlm_db_ble_write_serial_number(network_id, controller_id, serial_number_);
   ctrlm_db_ble_write_device_id(network_id, controller_id, device_id_);
}

void ctrlm_obj_controller_ble_t::validation_result_set(ctrlm_ble_result_validation_t validation_result) {
   if(CTRLM_BLE_RESULT_VALIDATION_SUCCESS == validation_result) {
      validation_result_ = validation_result;
      time_binding_  = time(NULL);
      last_key_time_ = time(NULL);
      db_store();
      return;
   }
   validation_result_ = validation_result;
}

ctrlm_ble_result_validation_t ctrlm_obj_controller_ble_t::validation_result_get() const {
   return validation_result_;
}

ctrlm_ble_controller_type_t ctrlm_obj_controller_ble_t::getControllerType(void) {
   return controller_type_;
}

void ctrlm_obj_controller_ble_t::setControllerType(std::string productName) {
   if (productName.find(BROADCAST_PRODUCT_NAME_IR_DEVICE) != std::string::npos) {
      controller_type_ = BLE_CONTROLLER_TYPE_IR;
   } else if (productName.find(BROADCAST_PRODUCT_NAME_PR1) != std::string::npos) {
      controller_type_ = BLE_CONTROLLER_TYPE_PR1;
   } else if (productName.find(BROADCAST_PRODUCT_NAME_EC302) != std::string::npos) {
      controller_type_ = BLE_CONTROLLER_TYPE_EC302;
   } else if (productName.find(BROADCAST_PRODUCT_NAME_LC103) != std::string::npos) {
      controller_type_ = BLE_CONTROLLER_TYPE_LC103;
   } else if (productName.find(BROADCAST_PRODUCT_NAME_LC203) != std::string::npos) {
      // LC203 for all controlMgr purposes is the same as LC103, it even loads the same f/w
      controller_type_ = BLE_CONTROLLER_TYPE_LC103;
   } else {
      controller_type_ = BLE_CONTROLLER_TYPE_UNKNOWN;
   }
   LOG_DEBUG("%s: controller_type_ set to <%s>\n", __FUNCTION__, ctrlm_ble_controller_type_str(controller_type_));
}

bool ctrlm_obj_controller_ble_t::getOTAProductName(string &name) {
   switch(controller_type_) {
      case BLE_CONTROLLER_TYPE_PR1:
         name = XCONF_PRODUCT_NAME_PR1;
         break;
      case BLE_CONTROLLER_TYPE_LC103:
         name = XCONF_PRODUCT_NAME_LC103;
         if (is_controller_type_z()) {
            name.append("Z");
         }
         break;
      case BLE_CONTROLLER_TYPE_EC302:
         name = XCONF_PRODUCT_NAME_EC302;
         break;
      case BLE_CONTROLLER_TYPE_IR:
         return false;
      default:
         LOG_ERROR("%s: controller of type %s not mapped to OTA product name\n", __FUNCTION__, ctrlm_ble_controller_type_str(controller_type_));
         return false;
   }
   return true;
}

void ctrlm_obj_controller_ble_t::setDeviceID(int device_id) {
   LOG_DEBUG("%s: Controller %u set device ID to %d\n", __FUNCTION__, controller_id_get(), device_id);
   device_id_ = device_id;
}

int ctrlm_obj_controller_ble_t::getDeviceID() {
   return device_id_;
}

void ctrlm_obj_controller_ble_t::setMacAddress(unsigned long long ieee_address) {
   LOG_DEBUG("%s: Controller %u set MAC to 0x%llX\n", __FUNCTION__, controller_id_get(), ieee_address);
   ieee_address_ = ieee_address;
}

unsigned long long ctrlm_obj_controller_ble_t::getMacAddress() {
   return ieee_address_;
}

void ctrlm_obj_controller_ble_t::setConnected(bool connected) {
   LOG_DEBUG("%s: Controller %u set connected to <%d>\n", __FUNCTION__, controller_id_get(), connected);
   connected_ = connected;
}
bool ctrlm_obj_controller_ble_t::getConnected() {
   return connected_;
}

void ctrlm_obj_controller_ble_t::setSerialNumber(string sn) {
   LOG_DEBUG("%s: Controller %u set serial number to %s\n", __FUNCTION__, controller_id_get(), sn.c_str());
   serial_number_ = sn;
}
string ctrlm_obj_controller_ble_t::getSerialNumber() {
   return serial_number_;
}

void ctrlm_obj_controller_ble_t::setName(string controller_name) {
   LOG_DEBUG("%s: Controller %u set name to %s\n", __FUNCTION__, controller_id_get(), controller_name.c_str());
   product_name_ = controller_name;
   setControllerType(product_name_);
}

string ctrlm_obj_controller_ble_t::getName() {
   return product_name_;
}

void ctrlm_obj_controller_ble_t::setManufacturer(string controller_manufacturer) {
   LOG_DEBUG("%s: Controller %u set manufacturer to %s\n", __FUNCTION__, controller_id_get(), controller_manufacturer.c_str());
   manufacturer_ = controller_manufacturer;
}
string ctrlm_obj_controller_ble_t::getManufacturer() {
   return manufacturer_;
}

void ctrlm_obj_controller_ble_t::setModel(string controller_model) {
   LOG_DEBUG("%s: Controller %u set model to %s\n", __FUNCTION__, controller_id_get(), controller_model.c_str());
   model_ = controller_model;
}
string ctrlm_obj_controller_ble_t::getModel() {
   return model_;
}

void ctrlm_obj_controller_ble_t::setFwRevision(string rev) {
   LOG_DEBUG("%s: Controller %u set FW Revision to %s\n", __FUNCTION__, controller_id_get(), rev.c_str());
   fw_revision_ = rev;
}
string ctrlm_obj_controller_ble_t::getFwRevision() {
   return fw_revision_;
}

void ctrlm_obj_controller_ble_t::setHwRevision(string rev) {
   LOG_DEBUG("%s: Controller %u set HW Revision to %s\n", __FUNCTION__, controller_id_get(), rev.c_str());
   hw_revision_.loadFromString(rev);
}
ctrlm_version_t ctrlm_obj_controller_ble_t::getHwRevision() {
   return hw_revision_;
}

void ctrlm_obj_controller_ble_t::setSwRevision(string rev) {
   LOG_DEBUG("%s: Controller %u set SW Revision to %s\n", __FUNCTION__, controller_id_get(), rev.c_str());
   sw_revision_.loadFromString(rev);
}
ctrlm_version_t ctrlm_obj_controller_ble_t::getSwRevision() {
   return sw_revision_;
}

guchar ctrlm_obj_controller_ble_t::property_write_irdb_entry_id_name_tv(guchar *data, guchar length) {
   gchar irdb_entry_id_name_tv[CTRLM_MAX_PARAM_STR_LEN];
   if(length != CTRLM_MAX_PARAM_STR_LEN) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   errno_t safec_rc = -1;

   // Clear the array
   safec_rc = memset_s(irdb_entry_id_name_tv, sizeof(irdb_entry_id_name_tv), 0, length);
   ERR_CHK(safec_rc);
   // Copy IRDB Code to data buf
   safec_rc = strncpy_s(irdb_entry_id_name_tv, sizeof(irdb_entry_id_name_tv), (gchar *)data, length);
   ERR_CHK(safec_rc);

   irdb_entry_id_name_tv[length-1] = 0;

   if(irdb_entry_id_name_tv_ != irdb_entry_id_name_tv) {
      // Store the data on the controller object
      irdb_entry_id_name_tv_ = irdb_entry_id_name_tv;
      if(validation_result_ == CTRLM_BLE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_ble_write_irdb_entry_id_name_tv(network_id_get(), controller_id_get(), (guchar *)data, CTRLM_MAX_PARAM_STR_LEN);
      }
   }

   LOG_INFO("%s: TV IRDB Code <%s>\n", __FUNCTION__, irdb_entry_id_name_tv_.c_str());
   return(CTRLM_MAX_PARAM_STR_LEN);
}

guchar ctrlm_obj_controller_ble_t::property_read_irdb_entry_id_name_tv(guchar *data, guchar length) {
   guchar irdb_entry_id_name_tv[CTRLM_MAX_PARAM_STR_LEN];
   if(data == NULL || length != CTRLM_MAX_PARAM_STR_LEN) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   errno_t safec_rc = -1;

   // Clear the array
   safec_rc = memset_s(irdb_entry_id_name_tv, sizeof(irdb_entry_id_name_tv), 0, length);
   ERR_CHK(safec_rc);
   // Copy IRDB Code to data buf
   safec_rc = strncpy_s((char *)irdb_entry_id_name_tv, sizeof(irdb_entry_id_name_tv), (char *)data, length);
   ERR_CHK(safec_rc);
   irdb_entry_id_name_tv[length-1] = 0;

   if(irdb_entry_id_name_tv_ != (char *)irdb_entry_id_name_tv) {
      // Store the data on the controller object
      irdb_entry_id_name_tv_ = (char *)irdb_entry_id_name_tv;
   }

   LOG_INFO("%s: TV IRDB Code <%s>\n", __FUNCTION__, irdb_entry_id_name_tv_.c_str());
   return(length);
}

guchar ctrlm_obj_controller_ble_t::property_write_irdb_entry_id_name_avr(guchar *data, guchar length) {
   gchar irdb_entry_id_name_avr[CTRLM_MAX_PARAM_STR_LEN];
   if(length != CTRLM_MAX_PARAM_STR_LEN) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   errno_t safec_rc = -1;

   // Clear the array
   safec_rc = memset_s(irdb_entry_id_name_avr, sizeof(irdb_entry_id_name_avr), 0, length);
   ERR_CHK(safec_rc);
   // Copy IRDB Code to data buf
   safec_rc = strncpy_s(irdb_entry_id_name_avr, sizeof(irdb_entry_id_name_avr), (gchar *)data, length);
   ERR_CHK(safec_rc);
   irdb_entry_id_name_avr[length-1] = 0;

   if(irdb_entry_id_name_avr_ != irdb_entry_id_name_avr) {
      // Store the data on the controller object
      irdb_entry_id_name_avr_ = irdb_entry_id_name_avr;
      if(validation_result_ == CTRLM_BLE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
         ctrlm_db_ble_write_irdb_entry_id_name_avr(network_id_get(), controller_id_get(), (guchar *)data, CTRLM_MAX_PARAM_STR_LEN);
      }
   }

   LOG_INFO("%s: AVR IRDB Code <%s>\n", __FUNCTION__, irdb_entry_id_name_avr_.c_str());
   return(CTRLM_MAX_PARAM_STR_LEN);
}

guchar ctrlm_obj_controller_ble_t::property_read_irdb_entry_id_name_avr(guchar *data, guchar length) {
   guchar irdb_entry_id_name_avr[CTRLM_MAX_PARAM_STR_LEN];
   if(data == NULL || length != CTRLM_MAX_PARAM_STR_LEN) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   errno_t safec_rc = -1;

   // Clear the array
   safec_rc = memset_s(irdb_entry_id_name_avr, sizeof(irdb_entry_id_name_avr), 0, length);
   ERR_CHK(safec_rc);
   // Copy IRDB Code to data buf
   safec_rc = strncpy_s((char *)irdb_entry_id_name_avr, sizeof(irdb_entry_id_name_avr),  (char *)data, length);
   ERR_CHK(safec_rc);
   irdb_entry_id_name_avr[length-1] = 0;

   if(irdb_entry_id_name_avr_ != (char *)irdb_entry_id_name_avr) {
      // Store the data on the controller object
      irdb_entry_id_name_avr_ = (char *)irdb_entry_id_name_avr;
   }

   LOG_INFO("%s: AVR IRDB Code <%s>\n", __FUNCTION__, irdb_entry_id_name_avr_.c_str());
   return(length);
}

bool ctrlm_obj_controller_ble_t::swUpgradeRequired(ctrlm_version_t newVersion, bool force) {
   if ((unsigned)controller_type_ >= BLE_CONTROLLER_TYPE_IR) {
      return false;
   } else {
      if (sw_revision_.empty()) {
         LOG_WARN("%s: Controller <%s> does not have a valid software revision so not upgrading\n", __FUNCTION__, ctrlm_convert_mac_long_to_string(ieee_address_).c_str());
         return false;
      } else {
         int rev_compare = sw_revision_.compare(newVersion);
         bool required = false;
         if (rev_compare == 0) {
            LOG_WARN("%s: Controller <%s> already has requested software installed\n", __FUNCTION__, ctrlm_convert_mac_long_to_string(ieee_address_).c_str());
            ota_clear_all_failure_counters();
         } else if (upgrade_attempted_) {
            // A type Z OTA failure is recognized by the OTA downloading 100% and completing successfully,
            // but the remote never reboots and updates its software revision field to the new firmware rev.
            // So if we've already attempted an upgrade during this upgrade procedure window but the revision is
            // still not updated then its a type Z failure.
            ota_failure_type_z_cnt_set(ota_failure_type_z_cnt_get() + 1);
         } else if (force) {
            // force flag is true and versions aren't equal, need upgrade
            required = true;
         } else if (rev_compare < 0) {
            // Current rev is older and we haven't attempted an upgrade on this controller this session, so need upgrade
            required = true;
         }
         LOG_DEBUG("%s: Controller %u: current rev = <%s>, new rev = <%s>, force = <%s>, upgrade_attempted_ = <%s>, rev_compare = <%d>, required = <%s>\n", __FUNCTION__, 
               controller_id_get(), sw_revision_.toString().c_str(), newVersion.toString().c_str(), force ? "TRUE" : "FALSE", upgrade_attempted_ ? "TRUE" : "FALSE", rev_compare, required ? "TRUE" : "FALSE");
         return required;
      }
   }
}

void ctrlm_obj_controller_ble_t::setUpgradeAttempted(bool upgrade_attempted) {
   LOG_DEBUG("%s: Controller %u set Upgrade Attempted to %d\n", __FUNCTION__, controller_id_get(), upgrade_attempted);
   upgrade_attempted_ = upgrade_attempted;
}

bool ctrlm_obj_controller_ble_t::getUpgradeAttempted(void) {
   return upgrade_attempted_;
}

void ctrlm_obj_controller_ble_t::setUpgradeInProgress(bool upgrading) {
   LOG_DEBUG("%s: Controller %u set Upgrade in Progress to %d\n", __FUNCTION__, controller_id_get(), upgrading);
   upgrade_in_progress_ = upgrading;
}

bool ctrlm_obj_controller_ble_t::getUpgradeInProgress(void) {
   return upgrade_in_progress_;
}

void ctrlm_obj_controller_ble_t::setUpgradePaused(bool paused) {
   upgrade_paused_ = paused;
}

bool ctrlm_obj_controller_ble_t::getUpgradePaused() {
   return upgrade_paused_;
}

void ctrlm_obj_controller_ble_t::ota_failure_cnt_incr() {
   ctrlm_obj_controller_t::ota_failure_cnt_incr();
   ctrlm_db_ble_write_ota_failure_cnt_last_success(network_id_get(), controller_id_get(), ota_failure_cnt_from_last_success_);
   LOG_DEBUG("%s: Controller %s <%s> OTA failure count since last successful upgrade = %d\n", __FUNCTION__, ctrlm_ble_controller_type_str(controller_type_), ctrlm_convert_mac_long_to_string(ieee_address_).c_str(), ota_failure_cnt_from_last_success_);
}

void ctrlm_obj_controller_ble_t::ota_clear_all_failure_counters() {
   ctrlm_obj_controller_t::ota_clear_all_failure_counters();
   ctrlm_db_ble_write_ota_failure_cnt_last_success(network_id_get(), controller_id_get(), ota_failure_cnt_from_last_success_);
   LOG_DEBUG("%s: Controller %s <%s> OTA failure count since last successful upgrade reset to 0.\n", __FUNCTION__, ctrlm_ble_controller_type_str(controller_type_), ctrlm_convert_mac_long_to_string(ieee_address_).c_str());
}

void ctrlm_obj_controller_ble_t::ota_failure_type_z_cnt_set(uint8_t ota_failures) {
   if (controller_type_ != BLE_CONTROLLER_TYPE_LC103) {
      return ;
   }
   if (ota_failure_type_z_cnt_get() < ota_failures) {
      LOG_WARN("%s: Controller %s <%s> type Z OTA failure suspected, incrementing counter.\n", __FUNCTION__, ctrlm_ble_controller_type_str(controller_type_), ctrlm_convert_mac_long_to_string(ieee_address_).c_str());
   }
   bool is_type_z_before = is_controller_type_z();
   ctrlm_obj_controller_t::ota_failure_type_z_cnt_set(ota_failures);
   bool is_type_z_after = is_controller_type_z();
   if (is_type_z_before != is_type_z_after) {
      LOG_WARN("%s: Controller %s <%s> switched from %s to %s\n",__FUNCTION__, ctrlm_ble_controller_type_str(controller_type_), ctrlm_convert_mac_long_to_string(ieee_address_).c_str(), is_type_z_before ? "TYPE Z" : "NOT TYPE Z", is_type_z_after ? "TYPE Z" : "NOT TYPE Z");
   }
   ctrlm_db_ble_write_ota_failure_type_z_count(network_id_get(), controller_id_get(), ota_failure_type_z_cnt_get());
   LOG_INFO("%s: Controller %s <%s> OTA type Z failure count updated to %d.... is %s\n", __FUNCTION__, ctrlm_ble_controller_type_str(controller_type_), ctrlm_convert_mac_long_to_string(ieee_address_).c_str(), ota_failure_type_z_cnt_get(), is_controller_type_z() ? "TYPE Z" : "NOT TYPE Z");
}

uint8_t ctrlm_obj_controller_ble_t::ota_failure_type_z_cnt_get(void) const {
   if (controller_type_ != BLE_CONTROLLER_TYPE_LC103) {
      return 0;
   }
   return ctrlm_obj_controller_t::ota_failure_type_z_cnt_get();
}

bool ctrlm_obj_controller_ble_t::is_controller_type_z(void) const {
   if (controller_type_ != BLE_CONTROLLER_TYPE_LC103) {
      return false;
   }
   return ctrlm_obj_controller_t::is_controller_type_z();
}

void ctrlm_obj_controller_ble_t::setIrCode(int ircode) {
   LOG_DEBUG("%s: Controller %u set IR 5 digit code to %d\n", __FUNCTION__, controller_id_get(), ircode);
   ir_code_ = ircode;
}

void ctrlm_obj_controller_ble_t::setAudioStreaming(bool streaming) {
   LOG_DEBUG("%s: Controller %u set Audio Streaming to %s\n", __FUNCTION__, controller_id_get(), streaming ? "TRUE" : "FALSE");
   audio_streaming_ = streaming;
}

void ctrlm_obj_controller_ble_t::setAudioGainLevel(guint8 gain) {
   LOG_DEBUG("%s: Controller %u set Audio Gain level to %u\n", __FUNCTION__, controller_id_get(), gain);
   audio_gain_level_ = gain;
}

void ctrlm_obj_controller_ble_t::setAudioCodecs(guint32 value) {
   LOG_DEBUG("%s: Controller %u set Audio Codecs to 0x%X\n", __FUNCTION__, controller_id_get(), value);
   audio_codecs_ = value;
}

void ctrlm_obj_controller_ble_t::setTouchMode(unsigned int param) {
   LOG_DEBUG("%s: Controller %u set Touch Mode to %u\n", __FUNCTION__, controller_id_get(), param);
   touch_mode_ = param;
}
void ctrlm_obj_controller_ble_t::setTouchModeSettable(bool param) {
   LOG_DEBUG("%s: Controller %u set Touch Mode Settable to %s\n", __FUNCTION__, controller_id_get(), param ? "TRUE" : "FALSE");
   touch_mode_settable_ = param;
}

void ctrlm_obj_controller_ble_t::setBatteryPercent(int percent) {
   LOG_DEBUG("%s: Controller %u set battery level percentage to %d\%\n", __FUNCTION__, controller_id_get(), percent);
   battery_percent_ = percent;
}
int ctrlm_obj_controller_ble_t::getBatteryPercent() {
   return battery_percent_;
}

time_t ctrlm_obj_controller_ble_t::getLastKeyTime() {
   return last_key_time_;
}

guint16 ctrlm_obj_controller_ble_t::getLastKeyCode() {
   return last_key_code_;
}

void ctrlm_obj_controller_ble_t::setLastWakeupKey(guint16 code) {
   last_wakeup_key_code_ = code;
}
guint16 ctrlm_obj_controller_ble_t::getLastWakeupKey() {
   return last_wakeup_key_code_;
}

void ctrlm_obj_controller_ble_t::setWakeupConfig(uint8_t config) {
   if (config > CTRLM_RCU_WAKEUP_CONFIG_INVALID) {
      wakeup_config_ = CTRLM_RCU_WAKEUP_CONFIG_INVALID;
   } else {
      wakeup_config_ = (ctrlm_rcu_wakeup_config_t)config;
   }
}
ctrlm_rcu_wakeup_config_t ctrlm_obj_controller_ble_t::getWakeupConfig() {
   return wakeup_config_;
}

void ctrlm_obj_controller_ble_t::setWakeupCustomList(int *list, int size) {
   if (NULL == list) {
      LOG_ERROR("%s: list is NULL\n", __FUNCTION__);
      return;
   }
   wakeup_custom_list_.clear();
   for (int i = 0; i < size; i++) {
      wakeup_custom_list_.push_back(list[i]);
   }
}
vector<uint16_t> ctrlm_obj_controller_ble_t::getWakeupCustomList() {
   return wakeup_custom_list_;
}

std::string ctrlm_obj_controller_ble_t::wakeupCustomListToString() {
  std::ostringstream oss;
  if (!wakeup_custom_list_.empty()) {
    std::copy(wakeup_custom_list_.begin(), wakeup_custom_list_.end()-1, std::ostream_iterator<int>(oss, ","));
    // add the last element now to avoid trailing comma
    oss << wakeup_custom_list_.back();
  }
  return oss.str();
}

void ctrlm_obj_controller_ble_t::process_event_key(ctrlm_key_status_t key_status, guint16 key_code) {
   last_key_status_ = key_status;
   last_key_code_   = key_code;
   last_key_time_update();
   return;
}

void ctrlm_obj_controller_ble_t::last_key_time_update() {
   last_key_time_ = time(NULL);

   if(last_key_time_ > last_key_time_flush_) {
      last_key_time_flush_ = last_key_time_ + LAST_KEY_DATABASE_FLUSH_INTERVAL;
      ctrlm_db_ble_write_last_key_press(network_id_get(), controller_id_get(), last_key_time_, last_key_code_);
   }
}

// EGTODO: move to base controller class
void ctrlm_obj_controller_ble_t::update_voice_metrics(bool is_short_utterance, guint32 voice_packets_sent, guint32 voice_packets_lost) {
   ctrlm_voice_t *voice_obj = ctrlm_get_voice_obj();
   int32_t packet_loss_threshold = (voice_obj ? voice_obj->packet_loss_threshold_get() : JSON_INT_VALUE_VOICE_PACKET_LOSS_THRESHOLD); //TODO Allow overrides from config file
   ctrlm_voice_util_stats_t voice_util_metrics;
   handle_day_change();
   voice_packets_sent_today_       += voice_packets_sent;
   voice_packets_lost_today_       += voice_packets_lost;
   if((((float)voice_packets_lost/(float)voice_packets_sent)*100.0) > (float)(packet_loss_threshold)) {
      utterances_exceeding_packet_loss_threshold_today_++;
   }

   if(false == is_short_utterance) {
      voice_cmd_count_today_++;
   } else {
      voice_cmd_short_today_++;
   }

   voice_util_metrics.voice_cmd_count_today                                = voice_cmd_count_today_;
   voice_util_metrics.voice_cmd_count_yesterday                            = voice_cmd_count_yesterday_;
   voice_util_metrics.voice_cmd_short_today                                = voice_cmd_short_today_;
   voice_util_metrics.voice_cmd_short_yesterday                            = voice_cmd_short_yesterday_;
   voice_util_metrics.voice_packets_sent_today                             = voice_packets_sent_today_;
   voice_util_metrics.voice_packets_sent_yesterday                         = voice_packets_sent_yesterday_;
   voice_util_metrics.voice_packets_lost_today                             = voice_packets_lost_today_;
   voice_util_metrics.voice_packets_lost_yesterday                         = voice_packets_lost_yesterday_;
   voice_util_metrics.utterances_exceeding_packet_loss_threshold_today     = utterances_exceeding_packet_loss_threshold_today_;
   voice_util_metrics.utterances_exceeding_packet_loss_threshold_yesterday = utterances_exceeding_packet_loss_threshold_yesterday_;

   ctrlm_print_voice_stats(__FUNCTION__, &voice_util_metrics);

   property_write_voice_metrics();
}

// EGTODO: move to base controller class
bool ctrlm_obj_controller_ble_t::handle_day_change() {
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

void ctrlm_obj_controller_ble_t::property_write_voice_metrics(void) {
   guchar data[CTRLM_BLE_LEN_VOICE_METRICS];
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

   // if(!loading_db_ && validation_result_ == CTRLM_BLE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
   if(validation_result_ == CTRLM_BLE_RESULT_VALIDATION_SUCCESS) { // write this data to the database
      ctrlm_db_ble_write_voice_metrics(network_id_get(), controller_id_get(), data, CTRLM_BLE_LEN_VOICE_METRICS);
   }
}

guchar ctrlm_obj_controller_ble_t::property_parse_voice_metrics(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_BLE_LEN_VOICE_METRICS) {
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
   }
   return(length);
}


void ctrlm_obj_controller_ble_t::getStatus(ctrlm_controller_status_t *status) {
   errno_t safec_rc = -1;
   if(status == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return;
   }

   //If the day has changed, store the values related to today and yesterday
   if(handle_day_change()) {
      property_write_voice_metrics();
   }

   status->ieee_address                                         = ieee_address_;
   status->time_binding                                         = time_binding_;
   status->time_last_key                                        = last_key_time_;
   status->last_key_status                                      = last_key_status_;
   status->last_key_code                                        = (ctrlm_key_code_t)last_key_code_;
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
      status->voice_packet_loss_average_today                   = 0;
   }
   if(voice_packets_sent_yesterday_>0) {
      status->voice_packet_loss_average_yesterday               = (float)((float)voice_packets_lost_yesterday_/(float)voice_packets_sent_yesterday_) * 100.0;
   } else {
      status->voice_packet_loss_average_yesterday               = 0;
   }
   status->utterances_exceeding_packet_loss_threshold_today     = utterances_exceeding_packet_loss_threshold_today_;
   status->utterances_exceeding_packet_loss_threshold_yesterday = utterances_exceeding_packet_loss_threshold_yesterday_;

   safec_rc = strncpy_s(status->manufacturer, sizeof(status->manufacturer), manufacturer_.c_str(),CTRLM_RCU_MAX_MANUFACTURER_LENGTH-1);
   ERR_CHK(safec_rc);
   status->manufacturer[CTRLM_RCU_MAX_MANUFACTURER_LENGTH - 1] = '\0';
   safec_rc = strncpy_s(status->type, sizeof(status->type), product_name_.c_str(), CTRLM_RCU_MAX_USER_STRING_LENGTH - 1);
   ERR_CHK(safec_rc);
   status->type[CTRLM_RCU_MAX_USER_STRING_LENGTH - 1] = '\0';

   status->battery_level_percent = battery_percent_;

   // ctrlm_print_controller_status(__FUNCTION__, status);
}

void ctrlm_obj_controller_ble_t::print_status() {
   char time_binding_str[CTRLM_BLE_TIME_STR_LEN];
   char time_last_key_str[CTRLM_BLE_TIME_STR_LEN];
   errno_t safec_rc = -1;

   // Format time strings
   if(time_binding_ == 0) {
      safec_rc = strcpy_s(time_binding_str, sizeof(time_binding_str), "NEVER");
      ERR_CHK(safec_rc);
   } else {
      time_binding_str[0]        = '\0';
      strftime(time_binding_str,        CTRLM_BLE_TIME_STR_LEN, "%F %T", localtime((time_t *)&time_binding_));
   }
   if(last_key_time_ == 0) {
      safec_rc = strcpy_s(time_last_key_str, sizeof(time_last_key_str), "NEVER");
      ERR_CHK(safec_rc);
   } else {
      time_last_key_str[0]        = '\0';
      strftime(time_last_key_str,        CTRLM_BLE_TIME_STR_LEN, "%F %T", localtime((time_t *)&last_key_time_));
   }

   string ota_product_name;
   getOTAProductName(ota_product_name);

   LOG_WARN("------------------------------------------------------------\n");
   LOG_INFO("%s: Controller ID                : %u\n", __FUNCTION__, controller_id_get());
   LOG_INFO("%s: Friendly Name                : %s\n", __FUNCTION__, product_name_.c_str());
   LOG_INFO("%s: OTA Product Name             : %s\n", __FUNCTION__, ota_product_name.c_str());
   LOG_INFO("%s: Manufacturer                 : %s\n", __FUNCTION__, manufacturer_.c_str());
   LOG_INFO("%s: Model                        : %s\n", __FUNCTION__, model_.c_str());
   LOG_INFO("%s: MAC Address                  : %s\n", __FUNCTION__, ctrlm_convert_mac_long_to_string(ieee_address_).c_str());
   LOG_INFO("%s: Device ID                    : %d\n", __FUNCTION__, device_id_);
   LOG_INFO("%s: Connected                    : %s\n", __FUNCTION__, (connected_==true) ? "true" : "false");
   LOG_INFO("%s: Battery Level                : %d%%\n", __FUNCTION__, battery_percent_);
   LOG_INFO("%s: HW Revision                  : %s\n", __FUNCTION__, hw_revision_.toString().c_str());
   LOG_INFO("%s: FW Revision                  : %s\n", __FUNCTION__, fw_revision_.c_str());
   LOG_INFO("%s: SW Revision                  : %s\n", __FUNCTION__, sw_revision_.toString().c_str());
   LOG_INFO("%s: Serial Number                : %s\n", __FUNCTION__, serial_number_.c_str());
   LOG_INFO("%s:\n", __FUNCTION__);
   LOG_INFO("%s: Bound Time                   : %s\n", __FUNCTION__, time_binding_str);
   LOG_INFO("%s: Last Key Code                : 0x%X\n", __FUNCTION__, last_key_code_);
   LOG_INFO("%s: Last Key Time                : %s\n", __FUNCTION__, time_last_key_str);
   LOG_INFO("%s: Last Wakeup Key              : 0x%X\n", __FUNCTION__, last_wakeup_key_code_);
   LOG_INFO("%s: Wakeup Config                : %s\n", __FUNCTION__, ctrlm_rcu_wakeup_config_str(wakeup_config_));
   if (wakeup_config_ == CTRLM_RCU_WAKEUP_CONFIG_CUSTOM) {
   LOG_INFO("%s: Wakeup Config Custom List    : %s\n", __FUNCTION__, wakeupCustomListToString().c_str());
   }
   LOG_INFO("%s:\n", __FUNCTION__);
   LOG_INFO("%s: Voice Cmd Count Today        : %lu\n", __FUNCTION__, voice_cmd_count_today_);
   LOG_INFO("%s: Voice Packets Sent Today     : %lu\n", __FUNCTION__, voice_packets_sent_today_);
   LOG_INFO("%s: Voice Packets Lost Today     : %lu\n", __FUNCTION__, voice_packets_lost_today_);
   LOG_INFO("%s: Voice Cmd Short Today        : %lu\n", __FUNCTION__, voice_cmd_short_today_);
   LOG_INFO("%s: Voice Cmd Count Yesterday    : %lu\n", __FUNCTION__, voice_cmd_count_yesterday_);
   LOG_INFO("%s: Voice Packets Sent Yesterday : %lu\n", __FUNCTION__, voice_packets_sent_yesterday_);
   LOG_INFO("%s: Voice Packets Lost Yesterday : %lu\n", __FUNCTION__, voice_packets_lost_yesterday_);
   LOG_INFO("%s: Voice Cmd Short Yesterday    : %lu\n", __FUNCTION__, voice_cmd_short_yesterday_);
   LOG_WARN("------------------------------------------------------------\n");
}

// End Function Implementations
