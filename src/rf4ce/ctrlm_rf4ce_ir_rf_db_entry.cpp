#include "ctrlm_rf4ce_ir_rf_db_entry.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include "ctrlm_utils.h"
#include "ctrlm_database.h"

#define IR_RF_DB_ENTRY_HEADER_DEV_TV (0x8C)
#define IR_RF_DB_ENTRY_HEADER_DEV_AVR (0xAC)
#define IR_RF_DB_ENTRY_HEADER_TX_OPTIONS (0x4C)
#define IR_RF_DB_ENTRY_HEADER_RF_DESCRIPTOR_LEN (0x02)

#define IR_RF_DB_ENTRY_HEADER_LEN (8)

ctrlm_rf4ce_ir_rf_db_entry_t::ctrlm_rf4ce_ir_rf_db_entry_t() {
    this->rf_descriptor[0] = 0x00;
    this->rf_descriptor[1] = 0x00;
    this->ir_flags         = 0x00; 
    this->rf_config        = 0x00;
}

ctrlm_rf4ce_ir_rf_db_entry_t::ctrlm_rf4ce_ir_rf_db_entry_t(ctrlm_rf4ce_ir_rf_db_entry_t *entry) {
    this->type             = entry->type;
    this->key              = entry->key;
    this->rf_descriptor[0] = entry->rf_descriptor[0];
    this->rf_descriptor[1] = entry->rf_descriptor[1];
    this->ir_flags         = entry->ir_flags; 
    this->rf_config        = entry->rf_config;
    this->ir_code          = entry->ir_code;
}

ctrlm_rf4ce_ir_rf_db_entry_t::~ctrlm_rf4ce_ir_rf_db_entry_t() {

}

ctrlm_rf4ce_ir_rf_db_entry_t* ctrlm_rf4ce_ir_rf_db_entry_t::from_rib_binary(uint8_t *data, uint16_t length) {
    ctrlm_rf4ce_ir_rf_db_entry_t *ret = NULL;
    try {
        if(length > 0) {
            if(data[0] != 0xC0) {
                if(length >= IR_RF_DB_ENTRY_HEADER_LEN) {
                    ret = new ctrlm_rf4ce_ir_rf_db_entry_t();
                    switch(data[0]) {
                        case IR_RF_DB_ENTRY_HEADER_DEV_TV: {
                            ret->type = CTRLM_RF4CE_IR_RF_DB_DEV_TV;
                            break;
                        }
                        case IR_RF_DB_ENTRY_HEADER_DEV_AVR: {
                            ret->type = CTRLM_RF4CE_IR_RF_DB_DEV_AVR;
                            break;
                        }
                        default: {
                            throw std::string("invalid devtype");
                        }
                    }
                    ret->rf_config = data[1]; ret->rf_descriptor[0] = data[4]; ret->rf_descriptor[1] = data[5];
                    ret->ir_flags = data[6];
                    ret->set_key_via_descriptor();
                    uint16_t ir_code_length = data[7];
                    uint8_t *ir_code = &data[8];
                    if(length-8 == ir_code_length) {
                        for(uint16_t i = 0; i < ir_code_length; i++) {
                            ret->ir_code.push_back(ir_code[i]);
                        }
                    } else {
                        throw std::string("IR code length mismatch");
                    }
                } else {
                    throw std::string("Length is not long enough to contain all flags/descriptors/length fields");
                }
            } else {
                throw std::string("Default flag is set, meaning this isn't a valid IR Code");
            }
        } else {
            throw std::string("Length is 0");
        }
    } catch(std::string err) {
        if(ret) {
            delete ret;
            ret = NULL;
        }
        LOG_WARN("%s: Unable to create ir_rf_db_entry from rib binary: %s\n", __FUNCTION__, err.c_str());
    }
    return(ret);
}

ctrlm_rf4ce_ir_rf_db_entry_t* ctrlm_rf4ce_ir_rf_db_entry_t::from_raw_ir_code(ctrlm_rf4ce_ir_rf_db_dev_type_t type, ctrlm_key_code_t key, uint8_t *data, uint16_t length) {
    ctrlm_rf4ce_ir_rf_db_entry_t *ret = new ctrlm_rf4ce_ir_rf_db_entry_t();
    ret->type = type;
    ret->key  = key;
    try {
        if(NULL == data) {
            throw std::string("data buffer NULL");
        }
        for(uint16_t i = 0; i < length; i++) {
            ret->ir_code.push_back(data[i]);
        }
        ret->set_descriptor_via_key();
    } catch(std::string err) {
        if(ret) {
            delete ret;
            ret = NULL;
        }
        LOG_WARN("%s: Unable to create ir_rf_db_entry from raw ir code: %s\n", __FUNCTION__, err.c_str());
    }
    return(ret);
}

ctrlm_rf4ce_ir_rf_db_entry_t* ctrlm_rf4ce_ir_rf_db_entry_t::from_ir_rf_db_entry(ctrlm_rf4ce_ir_rf_db_entry_t *entry) {
    ctrlm_rf4ce_ir_rf_db_entry_t *ret = new ctrlm_rf4ce_ir_rf_db_entry_t(entry);
    return(ret);
}

bool ctrlm_rf4ce_ir_rf_db_entry_t::to_binary(uint8_t **data, uint16_t *length) {
    bool ret = false;
    if(data && length) {
        *data = NULL;
        try {
            errno_t safec_rc = -1;
            uint16_t buffer_size = IR_RF_DB_ENTRY_HEADER_LEN + this->ir_code.size();
            if((*data = (uint8_t*)malloc(buffer_size)) == NULL) {
                throw std::string("Failed to allocate buffer");
            }
            uint8_t *data_ptr = *data;
            safec_rc = memset_s(&data_ptr, buffer_size, 0, buffer_size);
            if(safec_rc < 0) ERR_CHK(safec_rc);
            switch(this->type) {
                case CTRLM_RF4CE_IR_RF_DB_DEV_TV: {
                    data_ptr[0] = IR_RF_DB_ENTRY_HEADER_DEV_TV;
                    break;
                }
                case CTRLM_RF4CE_IR_RF_DB_DEV_AVR: {
                    data_ptr[0] = IR_RF_DB_ENTRY_HEADER_DEV_AVR;
                    break;
                }
                default: {
                    throw std::string("Invalid device type");
                    break;
                }
            }
            data_ptr[1] = this->rf_config;
            data_ptr[2] = IR_RF_DB_ENTRY_HEADER_TX_OPTIONS;
            data_ptr[3] = IR_RF_DB_ENTRY_HEADER_RF_DESCRIPTOR_LEN;
            data_ptr[4] = this->rf_descriptor[0];
            data_ptr[5] = this->rf_descriptor[1];
            data_ptr[6] = this->ir_flags;
            data_ptr[7] = this->ir_code.size();
            safec_rc = memcpy_s(&data_ptr[8], buffer_size - 8, this->ir_code.data(), this->ir_code.size());
            ERR_CHK(safec_rc);
            *length = buffer_size;
            ret = true;
        } catch(std::string err) {
            if(*data) {
                free(*data);
                *data = NULL;
            }
            LOG_ERROR("%s: ir_rf_db_entry to binary failed: %s\n", __FUNCTION__, err.c_str());
            ret = false;
        }
    }
    return(ret);
}

ctrlm_rf4ce_ir_rf_db_entry_t* ctrlm_rf4ce_ir_rf_db_entry_t::from_db(ctrlm_key_code_t key) {
    ctrlm_rf4ce_ir_rf_db_entry_t *ret = NULL;
    uint32_t length = 0;
    uint8_t *data   = NULL;
    //Read from db
    ctrlm_db_ir_rf_database_read(key, (guchar **)&data, &length);

    try {
        if(data == NULL) {
            throw std::string("No DB entry for this key");
        } else if(length > CTRLM_HAL_RF4CE_CONST_MAX_RIB_ATTRIBUTE_SIZE) {
            throw std::string("Length of DB entry is invalid, possible corruption");
        }
        ret = ctrlm_rf4ce_ir_rf_db_entry_t::from_rib_binary(data, length);
        if(ret == NULL) {
            throw std::string("from_rib_binary failed");
        }
    } catch (std::string err) {
        LOG_WARN("%s: Failed to create IR RF DB Entry from DB: %s\n", __FUNCTION__, err.c_str()); // WARN as this can happen when entries don't exist.
    }

    if(data) {
        g_free(data);
        data = NULL;
    }

    return(ret);
}

/**
 * Macro to simplify HEX numbers when working with string streams.
 */
#define HEX_FORMATTER() std::uppercase << std::setw(2) << std::setfill('0') << std::hex

std::string ctrlm_rf4ce_ir_rf_db_entry_t::to_string(bool debug) const {
    std::stringstream ss;
    ss << "Device Type <" << this->type << ">";
    ss << ", Key Code <" << ctrlm_key_code_str(this->key) << ">";
    ss << ", Length <" << this->ir_code.size() << ">";
    ss << ", RF Config <0x" << HEX_FORMATTER() << +this->rf_config << ">";
    ss << ", RF Descriptor <0x" << HEX_FORMATTER() << +this->rf_descriptor[0] << HEX_FORMATTER() << +this->rf_descriptor[1] << ">";
    ss << ", IR Flags <0x" << HEX_FORMATTER() << +this->ir_flags << ">";
    if(debug) {
        ss << ", Data <0x";
        for(unsigned int i = 0; i < this->ir_code.size(); i++) {
            ss << HEX_FORMATTER() << +this->ir_code[i];
        }
        ss << ">";
    }
    return(ss.str());
}

ctrlm_rf4ce_ir_rf_db_dev_type_t ctrlm_rf4ce_ir_rf_db_entry_t::get_type() const {
    return(this->type);
}

ctrlm_key_code_t ctrlm_rf4ce_ir_rf_db_entry_t::get_key() const {
    return(this->key);
}

void ctrlm_rf4ce_ir_rf_db_entry_t::set_ir_flags(uint8_t flags) {
    this->ir_flags = flags;
}

#define DESCRIPTOR_EXCEPTION_STRING "Invalid Keycode / Type Combo"
void ctrlm_rf4ce_ir_rf_db_entry_t::set_descriptor_via_key() {
    switch(this->key) {
        case CTRLM_KEY_CODE_TV_POWER_ON:
        case CTRLM_KEY_CODE_AVR_POWER_ON:
        case CTRLM_KEY_CODE_POWER_ON: {
            this->rf_descriptor[0] = 0x01; this->rf_descriptor[1] = 0x6D; this->rf_config = 0x01; this->ir_flags = 0x4F;
            // Sanity checks
            if((this->key == CTRLM_KEY_CODE_TV_POWER_ON  && this->type == CTRLM_RF4CE_IR_RF_DB_DEV_AVR) ||
               (this->key == CTRLM_KEY_CODE_AVR_POWER_ON && this->type == CTRLM_RF4CE_IR_RF_DB_DEV_TV)) throw std::string(DESCRIPTOR_EXCEPTION_STRING);
            break;
        }
        case CTRLM_KEY_CODE_TV_POWER_OFF:
        case CTRLM_KEY_CODE_AVR_POWER_OFF:
        case CTRLM_KEY_CODE_POWER_OFF: {
            this->rf_descriptor[0] = 0x01; this->rf_descriptor[1] = 0x6C; this->rf_config = 0x01; this->ir_flags = 0x4F;
            // Sanity checks
            if((this->key == CTRLM_KEY_CODE_TV_POWER_OFF  && this->type == CTRLM_RF4CE_IR_RF_DB_DEV_AVR) ||
               (this->key == CTRLM_KEY_CODE_AVR_POWER_OFF && this->type == CTRLM_RF4CE_IR_RF_DB_DEV_TV)) throw std::string(DESCRIPTOR_EXCEPTION_STRING);
            break;
        }
        case CTRLM_KEY_CODE_TV_POWER:
        case CTRLM_KEY_CODE_AVR_POWER_TOGGLE:
        case CTRLM_KEY_CODE_POWER_TOGGLE: {
            this->rf_descriptor[0] = 0x31; this->rf_descriptor[1] = 0x03; this->rf_config = 0x21; this->ir_flags = 0x00;
            // Sanity checks
            if((this->key == CTRLM_KEY_CODE_TV_POWER  && this->type == CTRLM_RF4CE_IR_RF_DB_DEV_AVR) ||
               (this->key == CTRLM_KEY_CODE_AVR_POWER_TOGGLE && this->type == CTRLM_RF4CE_IR_RF_DB_DEV_TV)) throw std::string(DESCRIPTOR_EXCEPTION_STRING);
            break;
        }
        case CTRLM_KEY_CODE_VOL_UP: {
            this->rf_descriptor[0] = 0x31; this->rf_descriptor[1] = 0x06; this->rf_config = 0x01; this->ir_flags = 0x00;
            break;
        }
        case CTRLM_KEY_CODE_VOL_DOWN: {
            this->rf_descriptor[0] = 0x31; this->rf_descriptor[1] = 0x07; this->rf_config = 0x01; this->ir_flags = 0x00;
            break;
        }
        case CTRLM_KEY_CODE_MUTE: {
            this->rf_descriptor[0] = 0x31; this->rf_descriptor[1] = 0x08; this->rf_config = 0x01; this->ir_flags = 0x00;
            break;
        }
        case CTRLM_KEY_CODE_INPUT_SELECT: {
            this->rf_descriptor[0] = 0x31; this->rf_descriptor[1] = 0x09; this->rf_config = 0x01; this->ir_flags = 0x00;
            break;
        }
        default: {
            throw std::string("Unknown IR keycode");
            break;
        }
    }
}

void ctrlm_rf4ce_ir_rf_db_entry_t::set_key_via_descriptor() {
    switch(this->rf_descriptor[0]) {
        case 0x01: {
            switch(this->rf_descriptor[1]) {
                case 0x6D: {this->key = CTRLM_KEY_CODE_POWER_ON; break;}
                case 0x6C: {this->key = CTRLM_KEY_CODE_POWER_OFF; break;}
                default: throw std::string("Invalid discrete descriptor");
            }
            break;
        }
        case 0x31: {
            switch(this->rf_descriptor[1]) {
                case 0x03: {this->key = CTRLM_KEY_CODE_POWER_TOGGLE; break;}
                case 0x06: {this->key = CTRLM_KEY_CODE_VOL_UP; break;}
                case 0x07: {this->key = CTRLM_KEY_CODE_VOL_DOWN; break;}
                case 0x08: {this->key = CTRLM_KEY_CODE_MUTE; break;}
                case 0x09: {this->key = CTRLM_KEY_CODE_INPUT_SELECT; break;}
                default: throw std::string("Invalid ghost code descriptor");
            }
            break;
        }
        default: throw std::string("Invalid descriptor identifier");
    }
}
