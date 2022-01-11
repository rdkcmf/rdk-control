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
#ifndef _CTRLM_CONTROLLER_H_
#define _CTRLM_CONTROLLER_H_

#include <sstream>
#include <vector>
#include "irdb/ctrlm_irdb.h"
#include "ctrlm_version.h"
#include "ctrlm_attr_general.h"

#define LAST_KEY_DATABASE_FLUSH_INTERVAL (2 * 60 * 60) // in seconds

class ctrlm_version_t {
public:
   ctrlm_version_t() {};
   ctrlm_version_t(std::string revStr) {
      loadFromString(revStr);
   };

   void operator = (const ctrlm_version_t &v ) { 
      revs = v.revs;
   };

   void loadFromString(std::string revStr) {
      revs.clear();
      std::istringstream iss(revStr);
      std::string token;
      int i = 0;
      unsigned long ul;
      while (std::getline(iss, token, '.')) {
         if (i >= 4) {
            // Can only support 4 revisions of size 16 bits.  This is because when revisions are compared,
            // the values are shifted into a 64 bit int.
            break;
         }
         errno = 0;
         ul = strtoul (token.c_str(), NULL, 0);
         if (errno) {
            revs.clear();
            return;
         }
         revs.push_back((uint16_t)ul);
         i++;
      }
   }

   std::string toString() {
      std::ostringstream sstr;
      for (uint16_t i = 0; i < revs.size(); i++) {
         sstr << std::dec << revs[i];
         if (i < revs.size()-1) {
            sstr << ".";
         }
      }
      return sstr.str();
   };

   int compare(ctrlm_version_t param) {
      unsigned long long this_ver=0, param_ver=0;
      uint16_t i = 0;
      for (uint16_t rev : revs) {
         this_ver = (this_ver << 16) | rev;
         if (i < param.revs.size()) {
            param_ver = (param_ver << 16) | param.revs[i];
         } else {
            param_ver = (param_ver << 16);
         }
         i++;
      }
      if(this_ver > param_ver) {
         return 1;
      } else if(this_ver < param_ver) {
         return -1;
      }
      return 0;
   };

   std::vector<uint16_t> revs;
};

class ctrlm_obj_network_t;

class ctrlm_obj_controller_t
{
public:
   ctrlm_obj_controller_t(ctrlm_controller_id_t controller_id, ctrlm_obj_network_t &network);
   ctrlm_obj_controller_t();
   virtual ~ctrlm_obj_controller_t();

   // External methods
   
   void send_to(unsigned long delay, unsigned long length, char *data);
   void irdb_entry_id_name_set(ctrlm_irdb_dev_type_t type, ctrlm_irdb_ir_entry_id_t irdb_ir_entry_id);
   ctrlm_irdb_ir_entry_id_t get_irdb_entry_id_name_tv();
   ctrlm_irdb_ir_entry_id_t get_irdb_entry_id_name_avr();
   virtual ctrlm_controller_capabilities_t get_capabilities() const;

   // Internal methods
   ctrlm_controller_id_t controller_id_get() const;
   ctrlm_network_id_t    network_id_get() const;
   std::string           receiver_id_get() const;
   std::string           device_id_get() const;
   std::string           service_account_id_get() const;
   std::string           partner_id_get() const;
   std::string           experience_get() const;
   std::string           stb_name_get() const;

private:
   ctrlm_controller_id_t controller_id_;
   ctrlm_obj_network_t * obj_network_;

   virtual guchar property_write_irdb_entry_id_name_tv(guchar *data, guchar length);
   virtual guchar property_write_irdb_entry_id_name_avr(guchar *data, guchar length);
   virtual guchar property_read_irdb_entry_id_name_tv(guchar *data, guchar length);
   virtual guchar property_read_irdb_entry_id_name_avr(guchar *data, guchar length);

protected:
   ctrlm_irdb_ir_entry_id_t irdb_entry_id_name_tv_;
   ctrlm_irdb_ir_entry_id_t irdb_entry_id_name_avr_;
};

#endif
