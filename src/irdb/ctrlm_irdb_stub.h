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

#ifndef __CTRLM_IRDB_STUB_H__
#define __CTRLM_IRDB_STUB_H__

#include "ctrlm_irdb.h"

class ctrlm_irdb_stub_t : public ctrlm_irdb_t {
public:
    ctrlm_irdb_stub_t(ctrlm_irdb_mode_t mode);
    ~ctrlm_irdb_stub_t();

    std::vector<ctrlm_irdb_manufacturer_t> get_manufacturers(ctrlm_irdb_dev_type_t type, std::string prefix = "");
    std::vector<ctrlm_irdb_model_t>        get_models(ctrlm_irdb_dev_type_t type, ctrlm_irdb_manufacturer_t manufacturer, std::string prefix = "");
    std::vector<ctrlm_irdb_ir_entry_id_t>  get_ir_codes_by_infoframe(ctrlm_irdb_dev_type_t &type, unsigned char *infoframe, size_t infoframe_len);
    std::vector<ctrlm_irdb_ir_entry_id_t>  get_ir_codes_by_names(ctrlm_irdb_dev_type_t type, ctrlm_irdb_manufacturer_t manufacturer, ctrlm_irdb_model_t model = "");
    bool                                   set_ir_codes_by_name(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_irdb_dev_type_t type, ctrlm_irdb_ir_entry_id_t name);
    bool                                   clear_ir_codes(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id);
};

#endif
