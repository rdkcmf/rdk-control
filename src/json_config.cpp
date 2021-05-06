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
#include "json_config.h"
#include "ctrlm_log.h"


json_config::json_config() : json_root_obj(0),
                             json_section_obj(0) {
}

json_config::~json_config() {
   json_decref(json_root_obj);
}

bool json_config::open_for_read(const char* json_file_name, const char* json_section_name){

   json_error_t json_error;
   json_root_obj = json_load_file(json_file_name, JSON_REJECT_DUPLICATES,  &json_error);
   if (json_root_obj == 0) {
      LOG_INFO("%s: Cannot open %-25s for read\n", __FUNCTION__, json_file_name);
      return false;
   }
   json_section_obj = json_object_get(json_root_obj, json_section_name);
   if(json_section_obj == NULL || !json_is_object(json_section_obj)) {
      LOG_WARN("%s: lson object %s not found\n", __FUNCTION__, json_section_name);
      return false;
   }
   return true;
}

bool json_config::config_object_set(json_t *json_obj){
   if(json_obj == 0 || !json_is_object(json_obj)) {
      LOG_INFO("%s: use default configuration\n", __FUNCTION__);
      return false;
   }
   json_section_obj = json_obj;
   return true;
}

bool json_config::config_value_get(const char* key, bool& val) const {

   json_t *json_obj = json_object_get(json_section_obj, key);
   if(json_obj == 0 || !json_is_boolean(json_obj)) {
      LOG_INFO("%s: %-25s - ABSENT\n", __FUNCTION__, key);
      return false;
   }
   val = json_is_true(json_obj);
   LOG_INFO("%s: %-25s - PRESENT <%s>\n", __FUNCTION__, key, val ? "true" : "false");
   return true;
}

bool json_config::config_value_get(const char* key, int& val, int min_val, int max_val) const {

   json_t *json_obj = json_object_get(json_section_obj, key);
   if(json_obj == 0 || (!json_is_integer(json_obj) && !json_is_boolean(json_obj)) ) {
      LOG_INFO("%s: %-25s - ABSENT\n", __FUNCTION__, key);
      return false;
   }
   // handle gboolean or integer boolean types
   if (json_is_boolean(json_obj)){
      bool value=false;
      if (config_value_get(key, value)){
         val=value;
         return true;
      }
      return false;
   }
   json_int_t value = json_integer_value(json_obj);
   LOG_INFO("%s: %-25s - PRESENT <%" JSON_INTEGER_FORMAT ">\n", __FUNCTION__, key, value);
   if((int)value < min_val || (int)value > max_val) {
      LOG_INFO("%s: %-25s - OUT OF RANGE %" JSON_INTEGER_FORMAT "\n", __FUNCTION__, key, value);
      return false;
   }
   val = (int)value;
   return true;
}

bool json_config::config_value_get(const char* key, double& val, double min_val, double max_val) const {

   json_t *json_obj = json_object_get(json_section_obj, key);
   if(json_obj == 0 || !json_is_real(json_obj) ) {
      LOG_INFO("%s: %-25s - ABSENT\n", __FUNCTION__, key);
      return false;
   }
   // handle real number types
   double value = json_real_value(json_obj);
   LOG_INFO("%s: %-25s - PRESENT <%f>\n", __FUNCTION__, key, value);
   if(value < min_val || value > max_val) {
      LOG_INFO("%s: %-25s - OUT OF RANGE %f\n", __FUNCTION__, key, value);
      return false;
   }
   val = value;
   return true;
}

bool json_config::config_value_get(const char* key, std::string& val) const{

   json_t *json_obj = json_object_get(json_section_obj, key);
   if(json_obj == 0 || !json_is_string(json_obj)) {
      LOG_INFO("%s: %-25s - ABSENT\n", __FUNCTION__, key);
      return false;
   }
   val = json_string_value(json_obj);
   LOG_INFO("%s: %-25s - PRESENT <%s>\n", __FUNCTION__, key, val.c_str());
   return true;
}

bool json_config::config_object_get(const char* key, json_config& config_object) const {

   json_t *json_obj = json_object_get(json_section_obj, key);
   if(json_obj == 0 || !json_is_object(json_obj)) {
      LOG_INFO("%s: %-25s - ABSENT\n", __FUNCTION__, key);
      return false;
   }
   LOG_INFO("%s: %-25s - PRESENT\n", __FUNCTION__, key);
   config_object.config_object_set(json_obj);
   return true;
}

json_t* json_config::current_object_get() const {
   return json_section_obj;
}

