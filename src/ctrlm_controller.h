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

#define LAST_KEY_DATABASE_FLUSH_INTERVAL (2 * 60 * 60) // in seconds

// Specification: Comcast-SP-RF4CE-XRC-Profile-D11-160505
// 6.5.8.12 Memory Dump
#define   CTRLM_CRASH_DUMP_OFFSET_MEMORY_SIZE         0
#define   CTRLM_CRASH_DUMP_OFFSET_MEMORY_IDENTIFIER   2
#define   CTRLM_CRASH_DUMP_OFFSET_RESERVED_0xFF       4
#define   CTRLM_CRASH_DUMP_MAX_SIZE                   (255*CTRLM_RF4CE_RIB_ATTR_LEN_MEMORY_DUMP)

// Remote Crash Dump File header                          // Dump File Format
#define CTRLM_CRASH_DUMP_FILE_LEN_SIGNATURE           4   // 4 bytes signature 'XDMP'
#define CTRLM_CRASH_DUMP_FILE_OFFSET_HDR_SIZE         4   // 2 bytes header size
#define CTRLM_CRASH_DUMP_FILE_OFFSET_DEVICE_ID        6   
#define CTRLM_CRASH_DUMP_FILE_LEN_DEVICE_ID           4   // 4 bytes device id
#define CTRLM_CRASH_DUMP_FILE_OFFSET_HW_VER          10   // 4 bytes hw version
#define CTRLM_CRASH_DUMP_FILE_OFFSET_SW_VER          14   // 4 bytes sw version
#define CTRLM_CRASH_DUMP_FILE_OFFSET_NUM_DUMPS       18   // 2 bytes number of dumps in the file
#define CTRLM_CRASH_DUMP_FILE_HDR_SIZE               20    
// Remote Crash FILE Data Header (offsets are relative to beginning of theheader)
#define CTRLM_CRASH_DUMP_DATA_HDR_OFFSET_MEM_ID       0   // 2 bytes memory Id
#define CTRLM_CRASH_DUMP_DATA_HDR_OFFSET_DATA         2   // 4 bytes offset to dump data
#define CTRLM_CRASH_DUMP_DATA_HDR_OFFSET_DATA_LEN     6   // 2 bytes data length
#define CTRLM_CRASH_DUMP_DATA_HDR_SIZE                8

typedef struct {
   guchar  manufacturer;
   guchar  model;
   guchar  hw_revision;
   guint16 lot_code;
} version_hardware_t;

typedef struct {
   guchar major;
   guchar minor;
   guchar revision;
   guchar patch;
} version_software_t;

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
