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

#include "ctrlm_irdb_stub.h"
#include "ctrlm_irdb_ipc_iarm_thunder.h"

ctrlm_irdb_stub_t::ctrlm_irdb_stub_t(ctrlm_irdb_mode_t mode) : ctrlm_irdb_t(mode) {
    IRDB_LOG_INFO("%s: registering for IARM Thunder calls\n", __FUNCTION__);
    this->ipc = new ctrlm_irdb_ipc_iarm_thunder_t();
    this->ipc->register_ipc();
}

ctrlm_irdb_stub_t::~ctrlm_irdb_stub_t() {
    IRDB_LOG_ERROR("%s not implemented\n", __FUNCTION__);
}

ctrlm_irdb_manufacturer_list_t ctrlm_irdb_stub_t::get_manufacturers(ctrlm_irdb_dev_type_t type, std::string prefix) {
    ctrlm_irdb_manufacturer_list_t ret;
    IRDB_LOG_ERROR("%s not implemented\n", __FUNCTION__);
    return(ret);
}

ctrlm_irdb_model_list_t ctrlm_irdb_stub_t::get_models(ctrlm_irdb_dev_type_t type, ctrlm_irdb_manufacturer_t manufacturer, std::string prefix) {
    ctrlm_irdb_model_list_t ret;
    IRDB_LOG_ERROR("%s not implemented\n", __FUNCTION__);
    return(ret);
}

ctrlm_irdb_ir_entry_id_ranked_list_t ctrlm_irdb_stub_t::get_ir_codes_by_infoframe(ctrlm_irdb_dev_type_t &type, unsigned char *infoframe, size_t infoframe_len) {
    ctrlm_irdb_ir_entry_id_ranked_list_t ret;
    IRDB_LOG_ERROR("%s not implemented\n", __FUNCTION__);
    return(ret);
}

ctrlm_irdb_ir_entry_id_ranked_list_t ctrlm_irdb_stub_t::get_ir_codes_by_edid(ctrlm_irdb_dev_type_t &type, unsigned char *edid, size_t edid_len) {
    ctrlm_irdb_ir_entry_id_ranked_list_t ret;
    IRDB_LOG_ERROR("%s not implemented\n", __FUNCTION__);
    return(ret);
}

ctrlm_irdb_ir_entry_id_ranked_list_t ctrlm_irdb_stub_t::get_ir_codes_by_cec(ctrlm_irdb_dev_type_t &type, std::string osd, unsigned int vendor_id, unsigned int logical_address) {
    ctrlm_irdb_ir_entry_id_ranked_list_t ret;
    IRDB_LOG_ERROR("%s not implemented\n", __FUNCTION__);
    return(ret);
}

ctrlm_irdb_ir_entry_id_by_type_t ctrlm_irdb_stub_t::get_ir_codes_by_autolookup() {
    ctrlm_irdb_ir_entry_id_by_type_t ret;
    IRDB_LOG_ERROR("%s not implemented\n", __FUNCTION__);
    return(ret);
}

ctrlm_irdb_ir_entry_id_list_t ctrlm_irdb_stub_t::get_ir_codes_by_names(ctrlm_irdb_dev_type_t type, ctrlm_irdb_manufacturer_t manufacturer, ctrlm_irdb_model_t model) {
    ctrlm_irdb_ir_entry_id_list_t ret;
    IRDB_LOG_ERROR("%s not implemented\n", __FUNCTION__);
    return(ret);
}

bool ctrlm_irdb_stub_t::set_ir_codes_by_name(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_irdb_dev_type_t type, ctrlm_irdb_ir_entry_id_t name) {
    IRDB_LOG_ERROR("%s not implemented\n", __FUNCTION__);
    return(false);
}

bool ctrlm_irdb_stub_t::clear_ir_codes(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) {
    IRDB_LOG_ERROR("%s not implemented\n", __FUNCTION__);
    return(false);
}

