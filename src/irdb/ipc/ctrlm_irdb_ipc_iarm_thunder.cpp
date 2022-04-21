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

#include "ctrlm_irdb_ipc_iarm_thunder.h"
#include "libIBus.h"
#include <iostream>
#include "ctrlm_ipc.h"
#include "ctrlm.h"
#include "jansson.h"

template<typename T>
static void json_to_iarm_response(const char *function, json_t **obj, T iarm);

ctrlm_irdb_ipc_iarm_thunder_t::ctrlm_irdb_ipc_iarm_thunder_t() : ctrlm_irdb_ipc_t() {
    IRDB_LOG_INFO("%s\n", __FUNCTION__);
}

ctrlm_irdb_ipc_iarm_thunder_t::~ctrlm_irdb_ipc_iarm_thunder_t() {
    IRDB_LOG_INFO("%s\n", __FUNCTION__);
}

bool ctrlm_irdb_ipc_iarm_thunder_t::register_ipc() const {
    bool ret = true;

    if(!this->register_iarm_call(CTRLM_MAIN_IARM_CALL_IR_MANUFACTURERS, get_manufacturers)) ret = false;
    if(!this->register_iarm_call(CTRLM_MAIN_IARM_CALL_IR_MODELS, get_models)) ret = false;
    if(!this->register_iarm_call(CTRLM_MAIN_IARM_CALL_IR_CODES, get_ir_codes_by_names)) ret = false;
    if(!this->register_iarm_call(CTRLM_MAIN_IARM_CALL_IR_AUTO_LOOKUP, get_ir_codes_by_auto_lookup)) ret = false;
    if(!this->register_iarm_call(CTRLM_MAIN_IARM_CALL_IR_SET_CODE, set_ir_codes_by_name)) ret = false;
    if(!this->register_iarm_call(CTRLM_MAIN_IARM_CALL_IR_CLEAR_CODE, clear_ir_codes)) ret = false;
    if(!this->register_iarm_call(CTRLM_MAIN_IARM_CALL_IR_INITIALIZE, initialize_irdb)) ret = false;

    return(ret);
}

void ctrlm_irdb_ipc_iarm_thunder_t::deregister_ipc() const {

}

bool ctrlm_irdb_ipc_iarm_thunder_t::register_iarm_call(const char *call, IARM_BusCall_t handler) const {
    bool ret = true;
    IARM_Result_t rc;

    IRDB_LOG_INFO("%s: Registering for %s\n", __FUNCTION__, call);
    rc = IARM_Bus_RegisterCall(call, handler);
    if(rc != IARM_RESULT_SUCCESS) {
        IRDB_LOG_ERROR("%s: Failed to register for %s\n", __FUNCTION__, call);
        ret = false;
    }
    return(ret);
}

ctrlm_irdb_dev_type_t ctrlm_irdb_ipc_iarm_thunder_t::ctrlm_irdb_dev_type_from_iarm(ctrlm_ir_device_type_t dev_type) {
    switch(dev_type) {
        case CTRLM_IR_DEVICE_TV:  return(CTRLM_IRDB_DEV_TYPE_TV);
        case CTRLM_IR_DEVICE_AMP: return(CTRLM_IRDB_DEV_TYPE_AVR);
        default: break;
    }
    return(CTRLM_IRDB_DEV_TYPE_INVALID);
}

const char *ctrlm_irdb_ipc_iarm_thunder_t::ctrlm_irdb_dev_type_str(ctrlm_irdb_dev_type_t dev_type) {
    switch(dev_type) {
        case CTRLM_IRDB_DEV_TYPE_TV:  return("TV");
        case CTRLM_IRDB_DEV_TYPE_AVR: return("AMP");
        default: break;
    }
    return("INVALID");
}

IARM_Result_t ctrlm_irdb_ipc_iarm_thunder_t::get_manufacturers(void *arg) {
    IRDB_LOG_INFO("%s\n", __FUNCTION__);
    ctrlm_iarm_call_IRManufacturers_params_t *params = (ctrlm_iarm_call_IRManufacturers_params_t *)arg;
    json_t *ret = json_object();
    ctrlm_irdb_t *irdb = ctrlm_main_irdb_get();

    bool success                   = false;
    ctrlm_irdb_dev_type_t dev_type = CTRLM_IRDB_DEV_TYPE_INVALID;
    json_t *manufacturers          = NULL;

    if(params == NULL || params->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
        IRDB_LOG_ERROR("%s: Invalid parameters\n", __FUNCTION__);
        return(IARM_RESULT_INVALID_PARAM);
    }

    dev_type = ctrlm_irdb_ipc_iarm_thunder_t::ctrlm_irdb_dev_type_from_iarm(params->type);
    if(dev_type != CTRLM_IRDB_DEV_TYPE_INVALID) {
        success = true;
    }

    if(irdb && success) {
        manufacturers = json_array();
        auto mans = irdb->get_manufacturers(dev_type, params->manufacturer);
        for(const auto &itr : mans) {
            json_array_append_new(manufacturers, json_string(itr.c_str()));
        }
    }

    // Assemble the return
    json_object_set_new(ret, "success", json_boolean(success));
    if(success) {
        json_object_set_new(ret, "avDevType", json_string(ctrlm_irdb_ipc_iarm_thunder_t::ctrlm_irdb_dev_type_str(dev_type)));
        json_object_set_new(ret, "manufacturers", (manufacturers != NULL ? manufacturers : json_null()));
    }
    json_to_iarm_response(__FUNCTION__, &ret, params);
    return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_irdb_ipc_iarm_thunder_t::get_models(void *arg) {
    IRDB_LOG_INFO("%s\n", __FUNCTION__);
    ctrlm_iarm_call_IRModels_params_t *params = (ctrlm_iarm_call_IRModels_params_t *)arg;
    json_t *ret = json_object();
    ctrlm_irdb_t *irdb = ctrlm_main_irdb_get();

    bool success                   = false;
    ctrlm_irdb_dev_type_t dev_type = CTRLM_IRDB_DEV_TYPE_INVALID;
    json_t *models                 = NULL;

    if(params == NULL || params->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
        IRDB_LOG_ERROR("%s: Invalid parameters\n", __FUNCTION__);
        return(IARM_RESULT_INVALID_PARAM);
    }

    dev_type = ctrlm_irdb_ipc_iarm_thunder_t::ctrlm_irdb_dev_type_from_iarm(params->type);
    if(dev_type != CTRLM_IRDB_DEV_TYPE_INVALID) {
        success = true;
    }

    if(irdb && success) {
        models = json_array();
        auto mods = irdb->get_models(dev_type, params->manufacturer, params->model);
        for(const auto &itr : mods) {
            json_array_append_new(models, json_string(itr.c_str()));
        }
    }

    // Assemble the return
    json_object_set_new(ret, "success", json_boolean(success));
    if(success) {
        json_object_set_new(ret, "avDevType", json_string(ctrlm_irdb_ipc_iarm_thunder_t::ctrlm_irdb_dev_type_str(dev_type)));
        json_object_set_new(ret, "manufacturer", json_string(params->manufacturer));
        json_object_set_new(ret, "models", (models != NULL ? models : json_null()));

    }
    json_to_iarm_response(__FUNCTION__, &ret, params);
    return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_irdb_ipc_iarm_thunder_t::get_ir_codes_by_auto_lookup(void *arg) {
    IRDB_LOG_INFO("%s\n", __FUNCTION__);
    ctrlm_iarm_call_IRAutoLookup_params_t *params = (ctrlm_iarm_call_IRAutoLookup_params_t *)arg;
    json_t *ret = json_object();
    ctrlm_irdb_t *irdb = ctrlm_main_irdb_get();

    bool success                   = false;
    json_t *tv_codes               = NULL;
    json_t *avr_codes              = NULL;

    if(params == NULL || params->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
        IRDB_LOG_ERROR("%s: Invalid parameters\n", __FUNCTION__);
        return(IARM_RESULT_INVALID_PARAM);
    }

    if(irdb) {
        if(irdb->can_get_ir_codes_by_autolookup()) {
            auto cd_map = irdb->get_ir_codes_by_autolookup();
            if(cd_map.count(CTRLM_IRDB_DEV_TYPE_TV) > 0) {
                tv_codes = json_array();
                json_array_append_new(tv_codes, json_string(cd_map[CTRLM_IRDB_DEV_TYPE_TV].c_str()));
                json_object_set_new(ret, "tvCodes", tv_codes);
            }
            if(cd_map.count(CTRLM_IRDB_DEV_TYPE_AVR) > 0) {
                avr_codes = json_array();
                json_array_append_new(avr_codes, json_string(cd_map[CTRLM_IRDB_DEV_TYPE_AVR].c_str()));
                json_object_set_new(ret, "avrCodes", avr_codes);
            }
            success = true;
        } else {
            IRDB_LOG_WARN("%s: IRDB doesn't support get_ir_codes_by_autolookup\n", __FUNCTION__);
        }
    }

    // Assemble the return
    json_object_set_new(ret, "success", json_boolean(success));
    json_to_iarm_response(__FUNCTION__, &ret, params);
    return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_irdb_ipc_iarm_thunder_t::get_ir_codes_by_names(void *arg) {
    IRDB_LOG_INFO("%s\n", __FUNCTION__);
    ctrlm_iarm_call_IRCodes_params_t *params = (ctrlm_iarm_call_IRCodes_params_t *)arg;
    json_t *ret = json_object();
    ctrlm_irdb_t *irdb = ctrlm_main_irdb_get();

    bool success                   = false;
    ctrlm_irdb_dev_type_t dev_type = CTRLM_IRDB_DEV_TYPE_INVALID;
    json_t *codes                  = NULL;

    if(params == NULL || params->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
        IRDB_LOG_ERROR("%s: Invalid parameters\n", __FUNCTION__);
        return(IARM_RESULT_INVALID_PARAM);
    }

    dev_type = ctrlm_irdb_ipc_iarm_thunder_t::ctrlm_irdb_dev_type_from_iarm(params->type);
    if(dev_type != CTRLM_IRDB_DEV_TYPE_INVALID) {
        success = true;
    }

    if(irdb && success) {
        codes = json_array();
        auto cds = irdb->get_ir_codes_by_names(dev_type, params->manufacturer, params->model);
        for(const auto &itr : cds) {
            json_array_append_new(codes, json_string(itr.c_str()));
        }
    }

    // Assemble the return
    json_object_set_new(ret, "success", json_boolean(success));
    if(success) {
        json_object_set_new(ret, "avDevType", json_string(ctrlm_irdb_ipc_iarm_thunder_t::ctrlm_irdb_dev_type_str(dev_type)));
        json_object_set_new(ret, "manufacturer", json_string(params->manufacturer));
        json_object_set_new(ret, "model", json_string(params->model));
        json_object_set_new(ret, "codes", (codes != NULL ? codes : json_null()));
    }
    json_to_iarm_response(__FUNCTION__, &ret, params);
    return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_irdb_ipc_iarm_thunder_t::set_ir_codes_by_name(void *arg) {
    IRDB_LOG_INFO("%s\n", __FUNCTION__);
    ctrlm_iarm_call_IRSetCode_params_t *params = (ctrlm_iarm_call_IRSetCode_params_t *)arg;
    json_t *ret = json_object();
    ctrlm_irdb_t *irdb = ctrlm_main_irdb_get();

    bool success                   = false;
    ctrlm_irdb_dev_type_t dev_type = CTRLM_IRDB_DEV_TYPE_INVALID;

    if(params == NULL || params->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
        IRDB_LOG_ERROR("%s: Invalid parameters\n", __FUNCTION__);
        return(IARM_RESULT_INVALID_PARAM);
    }

    dev_type = ctrlm_irdb_ipc_iarm_thunder_t::ctrlm_irdb_dev_type_from_iarm(params->type);
    if(dev_type != CTRLM_IRDB_DEV_TYPE_INVALID) {
        success = true;
    }

    if(irdb && success) {
        success = irdb->set_ir_codes_by_name(params->network_id, params->controller_id, dev_type, params->code);
    }

    // Assemble the return
    json_object_set_new(ret, "success", json_boolean(success));
    json_to_iarm_response(__FUNCTION__, &ret, params);
    return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_irdb_ipc_iarm_thunder_t::clear_ir_codes(void *arg) {
    IRDB_LOG_INFO("%s\n", __FUNCTION__);
    ctrlm_iarm_call_IRClear_params_t *params = (ctrlm_iarm_call_IRClear_params_t *)arg;
    json_t *ret = json_object();
    ctrlm_irdb_t *irdb = ctrlm_main_irdb_get();

    bool success                   = false;

    if(params == NULL || params->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
        IRDB_LOG_ERROR("%s: Invalid parameters\n", __FUNCTION__);
        return(IARM_RESULT_INVALID_PARAM);
    }

    if(irdb) {
        success = irdb->clear_ir_codes(params->network_id, params->controller_id);
    }

    // Assemble the return
    json_object_set_new(ret, "success", json_boolean(success));
    json_to_iarm_response(__FUNCTION__, &ret, params);
    return(IARM_RESULT_SUCCESS);
}

IARM_Result_t ctrlm_irdb_ipc_iarm_thunder_t::initialize_irdb(void *arg) {
    IRDB_LOG_INFO("%s\n", __FUNCTION__);
    ctrlm_iarm_call_initialize_irdb_params_t *params = (ctrlm_iarm_call_initialize_irdb_params_t *)arg;
    json_t *ret        = json_object();
    ctrlm_irdb_t *irdb = ctrlm_main_irdb_get();
    bool success       = false;

    if(params == NULL || params->api_revision != CTRLM_MAIN_IARM_BUS_API_REVISION) {
        IRDB_LOG_ERROR("%s: Invalid parameters\n", __FUNCTION__);
        return(IARM_RESULT_INVALID_PARAM);
    }

    if(irdb) {
        success = irdb->initialize_irdb();
    }

    // Assemble the return
    json_object_set_new(ret, "success", json_boolean(success));
    json_to_iarm_response(__FUNCTION__, &ret, params);
    return(IARM_RESULT_SUCCESS);
}

template<typename T>
void json_to_iarm_response(const char *function, json_t **obj, T iarm) {
    if(function != NULL && obj != NULL && *obj != NULL && iarm != NULL) {
        char *ret_str = json_dumps(*obj, JSON_COMPACT);
        if(ret_str) {
            if(strlen(ret_str) >= sizeof(iarm->response)) {
                IRDB_LOG_WARN("%s: JSON String is longer than the result!\n", function);
            }

            snprintf(iarm->response, sizeof(iarm->response), "%s", ret_str);
            iarm->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            
            if(ret_str) {
                free(ret_str);
                ret_str = NULL;
            }
            if(*obj) {
                json_decref(*obj);
                *obj = NULL;
            }
        } else {
            LOG_ERROR("%s: failed to dump JSON to string\n", function);
        }
    } else {
        LOG_ERROR("%s: %s passed NULL parameters\n", __FUNCTION__, function);
    }
}
