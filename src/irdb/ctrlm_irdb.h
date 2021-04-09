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

#ifndef __CTRLM_IRDB_H__
#define __CTRLM_IRDB_H__

#include <iostream>
#include <vector>
#include <map>
#include "ctrlm.h"
#include "ctrlm_ipc_key_codes.h"
#include "ctrlm_irdb_ipc.h"
#include "ctrlm_irdb_log.h"

typedef enum {
    CTRLM_IRDB_MODE_OFFLINE,
    CTRLM_IRDB_MODE_ONLINE,
    CTRLM_IRDB_MODE_HYBRID
} ctrlm_irdb_mode_t;

typedef enum {
    CTRLM_IRDB_DEV_TYPE_TV,
    CTRLM_IRDB_DEV_TYPE_AVR,
    CTRLM_IRDB_DEV_TYPE_INVALID
} ctrlm_irdb_dev_type_t;

typedef std::map<ctrlm_key_code_t, std::vector<char> > ctrlm_irdb_ir_codes_t;

typedef std::string    ctrlm_irdb_manufacturer_t;
typedef std::string    ctrlm_irdb_model_t;
typedef std::string    ctrlm_irdb_ir_entry_id_t;

typedef std::vector<char> ctrlm_irdb_ir_code_t;

class ctrlm_irdb_ir_code_set_t {
public:
  ctrlm_irdb_ir_code_set_t(ctrlm_irdb_dev_type_t type = CTRLM_IRDB_DEV_TYPE_INVALID, ctrlm_irdb_ir_entry_id_t id = "");

  void add_key(ctrlm_key_code_t key, ctrlm_irdb_ir_code_t ir_code);
  ctrlm_irdb_ir_codes_t   *get_key_map();
  ctrlm_irdb_dev_type_t    get_type();
  ctrlm_irdb_ir_entry_id_t get_id();

private:
  ctrlm_irdb_dev_type_t type_;
  ctrlm_irdb_ir_entry_id_t id_;
  ctrlm_irdb_ir_codes_t ir_codes_;
};

typedef struct {
   ctrlm_network_id_t         network_id;
   ctrlm_controller_id_t      controller_id;
   ctrlm_irdb_ir_code_set_t * ir_codes;
   bool *                     success;
   sem_t *                    semaphore;
} ctrlm_main_queue_msg_ir_set_code_t;

typedef struct {
   ctrlm_network_id_t    network_id;
   ctrlm_controller_id_t controller_id;
   bool *                success;
   sem_t *               semaphore;
} ctrlm_main_queue_msg_ir_clear_t;

class ctrlm_irdb_t {
public:
    ctrlm_irdb_t(ctrlm_irdb_mode_t mode);
    virtual ~ctrlm_irdb_t();

    virtual std::vector<ctrlm_irdb_manufacturer_t> get_manufacturers(ctrlm_irdb_dev_type_t type, std::string prefix = "")                                                                                    = 0;
    virtual std::vector<ctrlm_irdb_model_t>        get_models(ctrlm_irdb_dev_type_t type, ctrlm_irdb_manufacturer_t manufacturer, std::string prefix = "")                                                   = 0;
    virtual std::vector<ctrlm_irdb_ir_entry_id_t>  get_ir_codes_by_infoframe(ctrlm_irdb_dev_type_t &type, unsigned char *infoframe, size_t infoframe_len)                                                    = 0;
    virtual std::vector<ctrlm_irdb_ir_entry_id_t>  get_ir_codes_by_names(ctrlm_irdb_dev_type_t type, ctrlm_irdb_manufacturer_t manufacturer, ctrlm_irdb_model_t model = "")                                  = 0;
    virtual bool                                   set_ir_codes_by_name(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_irdb_dev_type_t type, ctrlm_irdb_ir_entry_id_t name)       = 0;
    virtual bool                                   clear_ir_codes(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id)                                                                        = 0;

protected:
    bool                                           _set_ir_codes(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_irdb_ir_code_set_t &ir_codes);
    bool                                           _clear_ir_codes(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id);
    void                                           normalize_string(std::string &str);

protected:
    ctrlm_irdb_mode_t mode;
    ctrlm_irdb_ipc_t *ipc;

};

#endif
