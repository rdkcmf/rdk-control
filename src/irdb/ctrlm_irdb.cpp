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

#include "ctrlm_irdb.h"
#include "ctrlm_network.h"
#include <algorithm>
#include "ctrlm_utils.h"

class ctrlm_irdb_code_rank_t {
public:
    std::string tv_code;
    int         tv_rank;
    std::string avr_code;
    int         avr_rank;

    ctrlm_irdb_code_rank_t() {
        this->tv_code  = "";
        this->tv_rank  = -1;
        this->avr_code = "";
        this->avr_rank = -1;
    }

    void update(ctrlm_irdb_dev_type_t type, ctrlm_irdb_ir_entry_id_t code, int rank) {
        switch(type) {
            case CTRLM_IRDB_DEV_TYPE_TV: {
                if(rank > this->tv_rank) {
                    this->tv_rank = rank;
                    this->tv_code = code;
                }
                // Sanity log
                if(code != this->tv_code) {
                    IRDB_LOG_WARN("%s: Multiple TV codes! Keeping existing code.\n", __FUNCTION__);
                }
                break;
            }
            case CTRLM_IRDB_DEV_TYPE_AVR: {
                if(rank > this->avr_rank) {
                    this->avr_rank = rank;
                    this->avr_code = code;
                }
                // Sanity log
                if(code != this->avr_code) {
                    IRDB_LOG_WARN("%s: Multiple AVR codes! Keeping existing code.\n", __FUNCTION__);
                }
                break;
            }
            default: {
                break;
            }
        }
    }
};

ctrlm_irdb_ir_code_set_t::ctrlm_irdb_ir_code_set_t(ctrlm_irdb_dev_type_t type, ctrlm_irdb_ir_entry_id_t id) {
   type_ = type;
   id_   = id;
}

void ctrlm_irdb_ir_code_set_t::add_key(ctrlm_key_code_t key, ctrlm_irdb_ir_code_t ir_code) {
   ir_codes_[key] = ir_code;
}

std::map<ctrlm_key_code_t, ctrlm_irdb_ir_code_t> *ctrlm_irdb_ir_code_set_t::get_key_map() {
   return &ir_codes_;
}

ctrlm_irdb_dev_type_t ctrlm_irdb_ir_code_set_t::get_type() {
   return type_;
}

ctrlm_irdb_ir_entry_id_t ctrlm_irdb_ir_code_set_t::get_id() {
   return id_;
}

ctrlm_irdb_t::ctrlm_irdb_t(ctrlm_irdb_mode_t mode) {
    this->mode = mode;
    this->ipc  = NULL;
}

ctrlm_irdb_t::~ctrlm_irdb_t() {
    if(this->ipc) {
        this->ipc->deregister_ipc();
        delete this->ipc;
        this->ipc = NULL;
    }
}

ctrlm_irdb_ir_entry_id_by_type_t ctrlm_irdb_t::get_ir_codes_by_autolookup() {
    ctrlm_irdb_ir_entry_id_by_type_t ret; // std::map initialization handled by default constructor
    ctrlm_irdb_code_rank_t final_codes;
    LOG_INFO("%s\n", __FUNCTION__);

#if   defined(PLATFORM_STB) && defined(CTRLM_THUNDER)
    // Check EDID data
    std::vector<uint8_t> edid;
    this->display_settings.get_edid(edid);
    if(edid.size() > 0) {
        ctrlm_irdb_dev_type_t type = CTRLM_IRDB_DEV_TYPE_INVALID;
        auto ir_codes = this->get_ir_codes_by_edid(type, edid.data(), edid.size());
        if(type != CTRLM_IRDB_DEV_TYPE_INVALID) {
            if(ir_codes.size() > 0) {
                for(const auto &itr : ir_codes) {
                    final_codes.update(type, itr.first, itr.second);
                }
            } else {
                IRDB_LOG_WARN("%s: 0 codes for edid data\n", __FUNCTION__);
            }
        } else {
            IRDB_LOG_ERROR("%s: edid dev type invalid\n", __FUNCTION__);
        }
    } else {
        IRDB_LOG_INFO("%s: No EDID data\n", __FUNCTION__);
    }
    // Check CEC data
    // TODO when CEC Plugin is available


#elif defined(PLATFORM_TV) && defined(CTRLM_THUNDER)
    // Check Infoframe data
    std::map<int, std::vector<uint8_t> > infoframes;
    this->hdmi_input.get_infoframes(infoframes);
    for(auto &itr : infoframes) {
        if(itr.second.size() > 0) {
            ctrlm_irdb_dev_type_t type = CTRLM_IRDB_DEV_TYPE_INVALID;
            auto ir_codes = this->get_ir_codes_by_infoframe(type, itr.second.data(), itr.second.size());
            if(type != CTRLM_IRDB_DEV_TYPE_INVALID) {
                if(ir_codes.size() > 0) {
                    for(const auto &itr : ir_codes) {
                        final_codes.update(type, itr.first, itr.second);
                    }
                } else {
                    IRDB_LOG_WARN("%s: no code for port %d infoframe\n", __FUNCTION__, itr.first);
                }
            } else {
                IRDB_LOG_WARN("%s: port %d infoframe dev type invalid\n", __FUNCTION__, itr.first);
            }
        } else {
            IRDB_LOG_INFO("%s: no infoframe for port %d\n", __FUNCTION__, itr.first);
        }
    }
    // Check CEC data
    std::vector<Thunder::CECSink::cec_device_t> cec_devices;
    this->cec_sink.get_cec_devices(cec_devices);
    if(cec_devices.size() > 0) {
        for(auto &itr : cec_devices) {
            ctrlm_irdb_dev_type_t type = CTRLM_IRDB_DEV_TYPE_INVALID;
            auto ir_codes = this->get_ir_codes_by_cec(type, itr.osd, (unsigned int)itr.vendor_id, itr.logical_address);
            if(type != CTRLM_IRDB_DEV_TYPE_INVALID) {
                if(ir_codes.size() > 0) {
                    for(const auto &itr : ir_codes) {
                        final_codes.update(type, itr.first, itr.second);
                    }
                } else {
                    IRDB_LOG_WARN("%s: no code for cec device\n", __FUNCTION__);
                }
            } else {
                IRDB_LOG_WARN("%s: cec dev type invalid\n", __FUNCTION__);
            }
        }
    } else {
        IRDB_LOG_INFO("%s: No CEC device data\n", __FUNCTION__);
    }


#endif

    if(!final_codes.tv_code.empty()) {
        ret[CTRLM_IRDB_DEV_TYPE_TV]  = final_codes.tv_code;
    }
    if(!final_codes.avr_code.empty()) {
        ret[CTRLM_IRDB_DEV_TYPE_AVR] = final_codes.avr_code;
    }
    return(ret);
}

bool ctrlm_irdb_t::can_get_ir_codes_by_autolookup() { 
    return(false);
}

bool ctrlm_irdb_t::_set_ir_codes(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_irdb_ir_code_set_t &ir_codes) {
    bool ret = false;
    sem_t semaphore;

    IRDB_LOG_INFO("%s: Setting IR codes for (%u, %u)\n", __FUNCTION__, network_id, controller_id);

    ctrlm_main_queue_msg_ir_set_code_t msg = {0};

    sem_init(&semaphore, 0, 0);

    msg.network_id         = network_id;
    msg.controller_id      = controller_id;
    msg.ir_codes           = &ir_codes;
    msg.success            = &ret;
    msg.semaphore          = &semaphore;

    ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_ir_set_code, &msg, sizeof(msg), NULL, network_id);

    sem_wait(&semaphore);
    sem_destroy(&semaphore);

    return(ret);
}

bool ctrlm_irdb_t::_clear_ir_codes(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) {
    bool ret = false;
    sem_t semaphore;

    IRDB_LOG_INFO("%s: Clearing IR codes for (%u, %u)\n", __FUNCTION__, network_id, controller_id);

    ctrlm_main_queue_msg_ir_clear_t msg = {0};

    sem_init(&semaphore, 0, 0);

    msg.network_id    = network_id;
    msg.controller_id = controller_id;
    msg.success       = &ret;
    msg.semaphore     = &semaphore;

    ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_ir_clear_codes, &msg, sizeof(msg), NULL, network_id);

    sem_wait(&semaphore);
    sem_destroy(&semaphore);
    
    return(ret);
}

void ctrlm_irdb_t::normalize_string(std::string &str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        switch (c) {
            case u'à': case u'á': case u'ä': case u'â': case u'ã': case u'å':
            case u'À': case u'Á': case u'Ä': case u'Â': case u'Ã': case u'Å': {
                c = 'a';
                break;
            }
            case u'é': case u'è': case u'ê': case u'ë':
            case u'È': case u'É': case u'Ê': case u'Ë': {
                c = 'e';
                break;
            }
            case u'ì': case u'í': case u'î': case u'ï':
            case u'Ì': case u'Í': case u'Î': case u'Ï': {
                c = 'i';
                break;
            }
            case u'ò': case u'ó': case u'ô': case u'õ': case u'ö': case u'ø':
            case u'Ò': case u'Ó': case u'Ô': case u'Õ': case u'Ö': case u'Ø': {
                c = 'o';
                break;
            }
            case u'ù': case u'ú': case u'û': case u'ü':
            case u'Ù': case u'Ú': case u'Û': case u'Ü': {
                c = 'u';
                break;
            }
            case u'ß': {
                c = 'b';
                break;
            }
            case u'Ñ': case u'ñ': {
                c = 'n';
                break;
            }
            default: {
                c = std::tolower(c);
                break;
            }
		}
        return(c);
    });
}
