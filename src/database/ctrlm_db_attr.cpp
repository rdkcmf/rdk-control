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
#include "ctrlm_db_attr.h"
#include "ctrlm_network.h"
#include "ctrlm_attr.h"
#include <sstream>

// ctrlm_db_attr_t
ctrlm_db_attr_t::ctrlm_db_attr_t(std::string table) {
    this->table = table;
}

ctrlm_db_attr_t::ctrlm_db_attr_t(ctrlm_obj_network_t *net) {
    if(net) {
        std::stringstream db_table;
        db_table << net->db_name_get() << "_" << COUT_HEX_MODIFIER << (int)net->network_id_get() << "_global";
        this->table = db_table.str();
    } else {
        LOG_ERROR("%s: network is NULL\n", __FUNCTION__);
    }
}

ctrlm_db_attr_t::ctrlm_db_attr_t(ctrlm_obj_network_t *net, ctrlm_controller_id_t id) {
    if(net) {
        std::stringstream db_table;
        db_table << net->db_name_get() << "_" << COUT_HEX_MODIFIER << (int)net->network_id_get() << "_controller_" << COUT_HEX_MODIFIER << (int)id;
        this->table = db_table.str();
    } else {
        LOG_ERROR("%s: network is NULL\n", __FUNCTION__);
    }
}

ctrlm_db_attr_t::~ctrlm_db_attr_t() {

}

std::string ctrlm_db_attr_t::get_table() const {
    return(this->table);
}

void ctrlm_db_attr_t::set_table(std::string table) {
    this->table = table;
}
// end ctrlm_db_attr_t