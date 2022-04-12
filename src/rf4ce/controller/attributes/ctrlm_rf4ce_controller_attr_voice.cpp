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
#include "ctrlm_rf4ce_controller_attr_voice.h"
#include "rf4ce/ctrlm_rf4ce_controller.h"
#include "ctrlm_db_types.h"
#include "ctrlm.h"
#include "ctrlm_utils.h"
#include <algorithm>
#include <sstream>
#include "ctrlm_database.h"
#include "ctrlm_voice.h"
#include "ctrlm_config_types.h"
#include "ctrlm_voice_obj.h"

// ctrlm_rf4ce_controller_audio_profiles_t
#define AUDIO_PROFILES_ID    (0x17)
#define AUDIO_PROFILES_INDEX (0x00)
#define AUDIO_PROFILES_LEN   (0x02)
ctrlm_rf4ce_controller_audio_profiles_t::ctrlm_rf4ce_controller_audio_profiles_t(ctrlm_obj_network_t *net, ctrlm_controller_id_t id) :
ctrlm_audio_profiles_t(ctrlm_audio_profiles_t::profile::NONE),
ctrlm_db_attr_t(net, id),
ctrlm_rf4ce_rib_attr_t(AUDIO_PROFILES_ID, AUDIO_PROFILES_INDEX, ctrlm_rf4ce_rib_attr_t::permission::PERMISSION_BOTH, ctrlm_rf4ce_rib_attr_t::permission::PERMISSION_CONTROLLER)
{
    this->set_name_prefix("Controller ");
}

ctrlm_rf4ce_controller_audio_profiles_t::~ctrlm_rf4ce_controller_audio_profiles_t() {

}

bool ctrlm_rf4ce_controller_audio_profiles_t::to_buffer(char *data, size_t len) {
    bool ret = false;
    if(len >= AUDIO_PROFILES_LEN) {
        data[0] =  this->supported_profiles       & 0xFF;
        data[1] = (this->supported_profiles >> 8) & 0xFF;
        ret = true;
    }
    return(ret);
}

bool ctrlm_rf4ce_controller_audio_profiles_t::read_db(ctrlm_db_ctx_t ctx) {
    bool ret = false;
    ctrlm_db_blob_t blob("audio_profiles", this->get_table());
    char buf[AUDIO_PROFILES_LEN] = {0,0};

    if(blob.read_db(ctx)) {
        size_t len = blob.to_buffer(buf, sizeof(buf));
        if(buf >= 0) {
            if(len == AUDIO_PROFILES_LEN) {
                this->supported_profiles = (buf[1] << 8) | buf[0];
                ret = true;
                LOG_INFO("%s: %s read from database: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
            } else {
                LOG_ERROR("%s: data from db too small <%s>\n", __FUNCTION__, this->get_name().c_str());
            }
        } else {
            LOG_ERROR("%s: failed to convert blob to buffer <%s>\n", __FUNCTION__, this->get_name().c_str());
        }
    } else {
        LOG_ERROR("%s: failed to read from db <%s>\n", __FUNCTION__, this->get_name().c_str());
    }
    return(ret);
}

bool ctrlm_rf4ce_controller_audio_profiles_t::write_db(ctrlm_db_ctx_t ctx) {
    bool ret = false;
    ctrlm_db_blob_t blob("audio_profiles", this->get_table());
    char buf[AUDIO_PROFILES_LEN] = {0,0};

    this->to_buffer(buf, sizeof(buf));
    if(blob.from_buffer(buf, sizeof(buf))) {
        if(blob.write_db(ctx)) {
            ret = true;
            LOG_INFO("%s: %s written to database: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
        } else {
            LOG_ERROR("%s: failed to write to db <%s>\n", __FUNCTION__, this->get_name().c_str());
        }
    } else {
        LOG_ERROR("%s: failed to convert buffer to blob <%s>\n", __FUNCTION__, this->get_name().c_str());
    }
    return(ret);
}

ctrlm_rf4ce_rib_attr_t::status ctrlm_rf4ce_controller_audio_profiles_t::read_rib(ctrlm_rf4ce_rib_attr_t::access accessor, rf4ce_rib_attr_index_t index, char *data, size_t *len) {
    ctrlm_rf4ce_rib_attr_t::status ret = ctrlm_rf4ce_rib_attr_t::status::FAILURE;
    if(data && len) {
        if(this->to_buffer(data, *len)) {
            *len = AUDIO_PROFILES_LEN;
            ret = ctrlm_rf4ce_rib_attr_t::status::SUCCESS;
            LOG_INFO("%s: %s read from RIB: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
        } else {
            LOG_ERROR("%s: buffer is not large enough <%d>\n", __FUNCTION__, *len);
            ret = ctrlm_rf4ce_rib_attr_t::status::WRONG_SIZE;
        }
    } else {
        LOG_ERROR("%s: data and/or length is NULL\n", __FUNCTION__);
        ret = ctrlm_rf4ce_rib_attr_t::status::INVALID_PARAM;
    }
    return(ret);
}

ctrlm_rf4ce_rib_attr_t::status ctrlm_rf4ce_controller_audio_profiles_t::write_rib(ctrlm_rf4ce_rib_attr_t::access accessor, rf4ce_rib_attr_index_t index, char *data, size_t len, bool importing) {
    ctrlm_rf4ce_rib_attr_t::status ret = ctrlm_rf4ce_rib_attr_t::status::FAILURE;
    if(data) {
        if(len == AUDIO_PROFILES_LEN) {
            int temp = this->supported_profiles;
            this->supported_profiles = data[0] + (data[1] << 8);
            ret = ctrlm_rf4ce_rib_attr_t::status::SUCCESS;
            LOG_INFO("%s: %s written to RIB: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
            if(temp != this->supported_profiles && false == importing) {
                ctrlm_db_attr_write(this);
            }
        } else {
            LOG_ERROR("%s: buffer is wrong size <%d>\n", __FUNCTION__, len);
            ret = ctrlm_rf4ce_rib_attr_t::status::WRONG_SIZE;
        }
    } else {
        LOG_ERROR("%s: data is NULL\n", __FUNCTION__);
        ret = ctrlm_rf4ce_rib_attr_t::status::INVALID_PARAM;
    }
    return(ret);
}

void ctrlm_rf4ce_controller_audio_profiles_t::export_rib(rf4ce_rib_export_api_t export_api) {
    char buf[AUDIO_PROFILES_LEN];
    if(ctrlm_rf4ce_rib_attr_t::status::SUCCESS == this->to_buffer(buf, sizeof(buf))) {
        export_api(this->get_identifier(), (uint8_t)this->get_index(), (uint8_t *)buf, (uint8_t)sizeof(buf));
    }
}
// end ctrlm_rf4ce_controller_audio_profiles_t

// ctrlm_rf4ce_voice_statistics_t
#define VOICE_STATISTICS_ID    (0x19)
#define VOICE_STATISTICS_INDEX (0x00)
#define VOICE_STATISTICS_LEN   (0x08)
ctrlm_rf4ce_voice_statistics_t::ctrlm_rf4ce_voice_statistics_t(ctrlm_obj_network_t *net, ctrlm_controller_id_t id) :
ctrlm_attr_t("Controller Voice Statistics"),
ctrlm_db_attr_t(net, id),
ctrlm_rf4ce_rib_attr_t(VOICE_STATISTICS_ID, VOICE_STATISTICS_INDEX, ctrlm_rf4ce_rib_attr_t::permission::PERMISSION_BOTH, ctrlm_rf4ce_rib_attr_t::permission::PERMISSION_CONTROLLER)
{
    this->voice_sessions = 0;
    this->tx_time        = 0;
}

ctrlm_rf4ce_voice_statistics_t::~ctrlm_rf4ce_voice_statistics_t() {

}

bool ctrlm_rf4ce_voice_statistics_t::to_buffer(char *data, size_t len) {
    bool ret = false;
    if(len >= VOICE_STATISTICS_LEN) {
        data[0] = (uint8_t)(this->voice_sessions);
        data[1] = (uint8_t)(this->voice_sessions >> 8);
        data[2] = (uint8_t)(this->voice_sessions >> 16);
        data[3] = (uint8_t)(this->voice_sessions >> 24);
        data[4] = (uint8_t)(this->tx_time);
        data[5] = (uint8_t)(this->tx_time >> 8);
        data[6] = (uint8_t)(this->tx_time >> 16);
        data[7] = (uint8_t)(this->tx_time >> 24);
        ret = true;
    }
    return(ret);
}

std::string ctrlm_rf4ce_voice_statistics_t::get_value() const {
    std::stringstream ss;
    ss << "Voice Sessions Activated <" << this->voice_sessions << "> ";
    ss << "Audio Data TX Time <" << this->tx_time << ">";
    return(ss.str());
}

bool ctrlm_rf4ce_voice_statistics_t::read_db(ctrlm_db_ctx_t ctx) {
    bool ret = false;
    ctrlm_db_blob_t blob("voice_statistics", this->get_table());
    char buf[VOICE_STATISTICS_LEN] = {0};

    if(blob.read_db(ctx)) {
        size_t len = blob.to_buffer(buf, sizeof(buf));
        if(buf >= 0) {
            if(len == VOICE_STATISTICS_LEN) {
                this->voice_sessions = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
                this->tx_time        = (buf[7] << 24) | (buf[6] << 16) | (buf[5] << 8) | buf[4];
                ret = true;
                LOG_INFO("%s: %s read from database: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
            } else {
                LOG_ERROR("%s: data from db too small <%s>\n", __FUNCTION__, this->get_name().c_str());
            }
        } else {
            LOG_ERROR("%s: failed to convert blob to buffer <%s>\n", __FUNCTION__, this->get_name().c_str());
        }
    } else {
        LOG_ERROR("%s: failed to read from db <%s>\n", __FUNCTION__, this->get_name().c_str());
    }
    return(ret);
}

bool ctrlm_rf4ce_voice_statistics_t::write_db(ctrlm_db_ctx_t ctx) {
    bool ret = false;
    ctrlm_db_blob_t blob("audio_profiles", this->get_table());
    char buf[VOICE_STATISTICS_LEN] = {0};

    this->to_buffer(buf, sizeof(buf));
    if(blob.from_buffer(buf, sizeof(buf))) {
        if(blob.write_db(ctx)) {
            ret = true;
            LOG_INFO("%s: %s written to database: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
        } else {
            LOG_ERROR("%s: failed to write to db <%s>\n", __FUNCTION__, this->get_name().c_str());
        }
    } else {
        LOG_ERROR("%s: failed to convert buffer to blob <%s>\n", __FUNCTION__, this->get_name().c_str());
    }
    return(ret);
}

ctrlm_rf4ce_rib_attr_t::status ctrlm_rf4ce_voice_statistics_t::read_rib(ctrlm_rf4ce_rib_attr_t::access accessor, rf4ce_rib_attr_index_t index, char *data, size_t *len) {
    ctrlm_rf4ce_rib_attr_t::status ret = ctrlm_rf4ce_rib_attr_t::status::FAILURE;
    if(data && len) {
        if(this->to_buffer(data, *len)) {
            *len = VOICE_STATISTICS_LEN;
            ret = ctrlm_rf4ce_rib_attr_t::status::SUCCESS;
            LOG_INFO("%s: %s read from RIB: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
        } else {
            LOG_ERROR("%s: buffer is not large enough <%d>\n", __FUNCTION__, *len);
            ret = ctrlm_rf4ce_rib_attr_t::status::WRONG_SIZE;
        }
    } else {
        LOG_ERROR("%s: data and/or length is NULL\n", __FUNCTION__);
        ret = ctrlm_rf4ce_rib_attr_t::status::INVALID_PARAM;
    }
    return(ret);
}

ctrlm_rf4ce_rib_attr_t::status ctrlm_rf4ce_voice_statistics_t::write_rib(ctrlm_rf4ce_rib_attr_t::access accessor, rf4ce_rib_attr_index_t index, char *data, size_t len, bool importing) {
    ctrlm_rf4ce_rib_attr_t::status ret = ctrlm_rf4ce_rib_attr_t::status::FAILURE;
    if(data) {
        if(len == VOICE_STATISTICS_LEN) {
            uint32_t temp_sessions = this->voice_sessions;
            uint32_t temp_tx_time  = this->tx_time;

            this->voice_sessions = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
            this->tx_time        = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];
            ret = ctrlm_rf4ce_rib_attr_t::status::SUCCESS;
            LOG_INFO("%s: %s written to RIB: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
            if((temp_sessions != this->voice_sessions ||
                temp_tx_time  != this->tx_time) &&
                false == importing) {
                ctrlm_db_attr_write(this);
            }
        } else {
            LOG_ERROR("%s: buffer is wrong size <%d>\n", __FUNCTION__, len);
            ret = ctrlm_rf4ce_rib_attr_t::status::WRONG_SIZE;
        }
    } else {
        LOG_ERROR("%s: data is NULL\n", __FUNCTION__);
        ret = ctrlm_rf4ce_rib_attr_t::status::INVALID_PARAM;
    }
    return(ret);
}

void ctrlm_rf4ce_voice_statistics_t::export_rib(rf4ce_rib_export_api_t export_api) {
    char buf[VOICE_STATISTICS_LEN];
    if(ctrlm_rf4ce_rib_attr_t::status::SUCCESS == this->to_buffer(buf, sizeof(buf))) {
        export_api(this->get_identifier(), (uint8_t)this->get_index(), (uint8_t *)buf, (uint8_t)sizeof(buf));
    }
}
// end ctrlm_rf4ce_controller_audio_profiles_t

// ctrlm_rf4ce_voice_session_statistics_t
#define VOICE_SESSION_STATISTICS_ID    (0x1C)
#define VOICE_SESSION_STATISTICS_INDEX (0x00)
#define VOICE_SESSION_STATISTICS_LEN   (16)
ctrlm_rf4ce_voice_session_statistics_t::ctrlm_rf4ce_voice_session_statistics_t(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) :
ctrlm_rf4ce_rib_attr_t(VOICE_SESSION_STATISTICS_ID, VOICE_SESSION_STATISTICS_INDEX, ctrlm_rf4ce_rib_attr_t::permission::PERMISSION_BOTH, ctrlm_rf4ce_rib_attr_t::permission::PERMISSION_CONTROLLER)
{
    this->network_id    = network_id;
    this->controller_id = controller_id;
}

ctrlm_rf4ce_voice_session_statistics_t::~ctrlm_rf4ce_voice_session_statistics_t() {

}

ctrlm_rf4ce_rib_attr_t::status ctrlm_rf4ce_voice_session_statistics_t::write_rib(ctrlm_rf4ce_rib_attr_t::access accessor, rf4ce_rib_attr_index_t index, char *data, size_t len, bool importing) {
    ctrlm_rf4ce_rib_attr_t::status ret = ctrlm_rf4ce_rib_attr_t::status::FAILURE;
    if(data) {
        if(len == VOICE_SESSION_STATISTICS_LEN) {
            uint16_t total_packets, total_dropped, drop_retry, drop_buffer, mac_retries, network_retries, cca_sense;
            float drop_percent, retry_percent, buffer_percent;
            total_packets   = (data[1]  << 8) | data[0];
            drop_retry      = (data[3]  << 8) | data[2];
            drop_buffer     = (data[5]  << 8) | data[4];
            total_dropped   = drop_retry + drop_buffer;
            drop_percent    = (100.0 * total_dropped / total_packets);
            retry_percent   = (100.0 * drop_retry    / total_packets);
            buffer_percent  = (100.0 * drop_buffer   / total_packets);
            // Interference indicators
            mac_retries     = (data[7]  << 8) | data[6];
            network_retries = (data[9]  << 8) | data[8];
            cca_sense       = (data[11] << 8) | data[10];
   
            // Write this data to the database??

            LOG_INFO("%s: Voice Session Stats written to RIB: Total Packets %u Dropped %u (%5.2f%%) due to Retry %u (%5.2f%%) Buffer %u (%5.2f%%)\n", __FUNCTION__, total_packets, total_dropped, drop_percent, drop_retry, retry_percent, drop_buffer, buffer_percent);

            if(mac_retries != 0xFFFF || network_retries != 0xFFFF || cca_sense != 0xFFFF) {
                LOG_INFO("%s: Total MAC Retries %u Network Retries %u CCA Sense Failures %u\n", __FUNCTION__, mac_retries, network_retries, cca_sense);
            }

            // Send data to voice object
            ctrlm_voice_t *obj = ctrlm_get_voice_obj();
            if(NULL != obj) {
               ctrlm_voice_stats_session_t stats_session;

               stats_session.available        = 1;
               stats_session.packets_total    = total_packets;
               stats_session.dropped_retry    = drop_retry;
               stats_session.dropped_buffer   = drop_buffer;
               stats_session.retry_mac        = mac_retries;
               stats_session.retry_network    = network_retries;
               stats_session.cca_sense        = cca_sense;

               // The following are not set here and will be ignored
               //stats_session.rf_channel       = 0;
               //stats_session.buffer_watermark = 0;
               //stats_session.packets_lost     = 0;
               //stats_session.link_quality     = 0;

               obj->voice_session_stats(stats_session);
            }

            ret = ctrlm_rf4ce_rib_attr_t::status::SUCCESS;

        } else {
            LOG_ERROR("%s: buffer is wrong size <%d>\n", __FUNCTION__, len);
            ret = ctrlm_rf4ce_rib_attr_t::status::WRONG_SIZE;
        }
    } else {
        LOG_ERROR("%s: data is NULL\n", __FUNCTION__);
        ret = ctrlm_rf4ce_rib_attr_t::status::INVALID_PARAM;
    }
    return(ret);
}
// end ctrlm_rf4ce_voice_session_statistics_t

// ctrlm_rf4ce_voice_command_status_t
#define VOICE_COMMAND_STATUS_ID    (0x10)
#define VOICE_COMMAND_STATUS_INDEX (0x00)
#define VOICE_COMMAND_STATUS_LEN_OLD      (1)
#define VOICE_COMMAND_STATUS_LEN_NEW      (5)
ctrlm_rf4ce_voice_command_status_t::ctrlm_rf4ce_voice_command_status_t(ctrlm_obj_controller_rf4ce_t *controller) :
ctrlm_attr_t("Voice Command Status"),
ctrlm_rf4ce_rib_attr_t(VOICE_COMMAND_STATUS_ID, VOICE_COMMAND_STATUS_INDEX, ctrlm_rf4ce_rib_attr_t::permission::PERMISSION_BOTH, ctrlm_rf4ce_rib_attr_t::permission::PERMISSION_TARGET) 
{
    this->controller = controller;
    this->reset();
}

ctrlm_rf4ce_voice_command_status_t::~ctrlm_rf4ce_voice_command_status_t() {

}

void ctrlm_rf4ce_voice_command_status_t::reset() {
    this->vcs = ctrlm_rf4ce_voice_command_status_t::status::NO_COMMAND_SENT;
    this->flags  = 0x00;
}

std::string ctrlm_rf4ce_voice_command_status_t::status_str(ctrlm_rf4ce_voice_command_status_t::status s) {
    std::stringstream ss; ss.str("INVALID");
    switch(s) {
        case ctrlm_rf4ce_voice_command_status_t::status::PENDING:            {ss.str("PENDING");            break;}
        case ctrlm_rf4ce_voice_command_status_t::status::TIMEOUT:            {ss.str("TIMEOUT");            break;}
        case ctrlm_rf4ce_voice_command_status_t::status::OFFLINE:            {ss.str("OFFLINE");            break;}
        case ctrlm_rf4ce_voice_command_status_t::status::SUCCESS:            {ss.str("SUCCESS");            break;}
        case ctrlm_rf4ce_voice_command_status_t::status::FAILURE:            {ss.str("FAILURE");            break;}
        case ctrlm_rf4ce_voice_command_status_t::status::NO_COMMAND_SENT:    {ss.str("NO_COMMAND_SENT");    break;}
        case ctrlm_rf4ce_voice_command_status_t::status::TV_AVR_COMMAND:     {ss.str("TV_AVR_COMMAND");     break;}
        case ctrlm_rf4ce_voice_command_status_t::status::MICROPHONE_COMMAND: {ss.str("MICROPHONE_COMMAND"); break;}
        case ctrlm_rf4ce_voice_command_status_t::status::AUDIO_COMMAND:      {ss.str("AUDIO_COMMAND");      break;}
        default: {ss << " <" << s << ">"; break;}
    }
    return(ss.str());
}

std::string ctrlm_rf4ce_voice_command_status_t::tv_avr_command_flag_str(ctrlm_rf4ce_voice_command_status_t::tv_avr_command_flag f) {
    std::stringstream ss; ss.str("INVALID");
    switch(f) {
        case ctrlm_rf4ce_voice_command_status_t::tv_avr_command_flag::TOGGLE_FALLBACK: {ss.str("TOGGLE_FALLBACK"); break;}
        default: {ss << " <" << f << ">"; break;}
    }
    return(ss.str());
}

std::string ctrlm_rf4ce_voice_command_status_t::tv_avr_command_str(ctrlm_rf4ce_voice_command_status_t::tv_avr_command c) {
    std::stringstream ss; ss.str("INVALID");
    switch(c) {
        case ctrlm_rf4ce_voice_command_status_t::tv_avr_command::POWER_OFF:    {ss.str("POWER_OFF");    break;}
        case ctrlm_rf4ce_voice_command_status_t::tv_avr_command::POWER_ON:     {ss.str("POWER_ON");     break;}
        case ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_UP:    {ss.str("VOLUME_UP");    break;}
        case ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_DOWN:  {ss.str("VOLUME_DOWN");  break;}
        case ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_MUTE:  {ss.str("VOLUME_MUTE");  break;}
        case ctrlm_rf4ce_voice_command_status_t::tv_avr_command::POWER_TOGGLE: {ss.str("POWER_TOGGLE"); break;}
        default: {ss << " <" << c << ">"; break;}
    }
    return(ss.str());
}

std::string ctrlm_rf4ce_voice_command_status_t::get_value() const {
    std::stringstream ss;
    ss << "Status <" << this->status_str(this->vcs) << "> ";
    if(this->vcs == ctrlm_rf4ce_voice_command_status_t::status::TV_AVR_COMMAND) {
        ss << "Flags <";
        if(this->flags & ctrlm_rf4ce_voice_command_status_t::tv_avr_command_flag::TOGGLE_FALLBACK) {
            ss << this->tv_avr_command_flag_str(ctrlm_rf4ce_voice_command_status_t::tv_avr_command_flag::TOGGLE_FALLBACK);
        }
        ss << "> ";
        ss << "Command <" << this->tv_avr_command_str(this->tac) << "> ";
        if(this->tac == ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_UP   ||
           this->tac == ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_DOWN ||
           this->tac == ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_MUTE) {
            ss << "IR Repeats <" << this->ir_repeats << "> ";
        }
    }
    return(ss.str());
}

ctrlm_rf4ce_rib_attr_t::status ctrlm_rf4ce_voice_command_status_t::read_rib(ctrlm_rf4ce_rib_attr_t::access accessor, rf4ce_rib_attr_index_t index, char *data, size_t *len) {
    ctrlm_rf4ce_rib_attr_t::status ret = ctrlm_rf4ce_rib_attr_t::status::FAILURE;
    if(data && len) {
        if(*len >= VOICE_COMMAND_STATUS_LEN_OLD) {
            data[0] = this->vcs & 0xFF;
            if(*len >= VOICE_COMMAND_STATUS_LEN_NEW) {
                memset(&data[1], 0, *len-1);
                data[1] = this->flags & 0xFF;
                if(this->vcs == ctrlm_rf4ce_voice_command_status_t::status::TV_AVR_COMMAND) {
                    data[2] = this->tac & 0xFF;
                    if(this->tac == ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_UP   ||
                       this->tac == ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_DOWN ||
                       this->tac == ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_MUTE) {
                        data[3] = this->ir_repeats & 0xFF;
                    }
                }
                *len = VOICE_COMMAND_STATUS_LEN_NEW;
            } else {
                *len = VOICE_COMMAND_STATUS_LEN_OLD;
            }
            ret = ctrlm_rf4ce_rib_attr_t::status::SUCCESS;
            LOG_INFO("%s: %s read from RIB: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
            if(this->vcs != ctrlm_rf4ce_voice_command_status_t::status::PENDING && accessor == ctrlm_rf4ce_rib_attr_t::CONTROLLER) {
                ctrlm_voice_t *obj = ctrlm_get_voice_obj();
                if(obj != NULL) {
                    obj->voice_controller_command_status_read(this->controller->network_id_get(), this->controller->controller_id_get());
                }
                this->reset();
            }
        } else {
            LOG_ERROR("%s: buffer is not large enough <%d>\n", __FUNCTION__, *len);
            ret = ctrlm_rf4ce_rib_attr_t::status::WRONG_SIZE;
        }
    } else {
        LOG_ERROR("%s: data and/or length is NULL\n", __FUNCTION__);
        ret = ctrlm_rf4ce_rib_attr_t::status::INVALID_PARAM;
    }
    return(ret);
}

ctrlm_rf4ce_rib_attr_t::status ctrlm_rf4ce_voice_command_status_t::write_rib(ctrlm_rf4ce_rib_attr_t::access accessor, rf4ce_rib_attr_index_t index, char *data, size_t len, bool importing) {
    ctrlm_rf4ce_rib_attr_t::status ret = ctrlm_rf4ce_rib_attr_t::status::FAILURE;
    if(data) {
        if(len == VOICE_COMMAND_STATUS_LEN_OLD || len == VOICE_COMMAND_STATUS_LEN_NEW) {
            this->vcs = (ctrlm_rf4ce_voice_command_status_t::status)data[0];
            if(len == VOICE_COMMAND_STATUS_LEN_NEW) {
                this->flags = data[1];
                if(this->vcs == ctrlm_rf4ce_voice_command_status_t::status::TV_AVR_COMMAND) {
                    this->tac = (ctrlm_rf4ce_voice_command_status_t::tv_avr_command)data[2];
                    if(this->tac == ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_UP   ||
                       this->tac == ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_DOWN ||
                       this->tac == ctrlm_rf4ce_voice_command_status_t::tv_avr_command::VOLUME_MUTE) {
                        this->ir_repeats = data[3] & 0xFF;
                    }
                }
            }
            if(!(this->controller && this->controller->controller_type_get() == RF4CE_CONTROLLER_TYPE_XR19)) { // IF NOT XR19
                if(this->vcs >= ctrlm_rf4ce_voice_command_status_t::status::TV_AVR_COMMAND) {
                    this->vcs = ctrlm_rf4ce_voice_command_status_t::status::SUCCESS;
                }
            }
            ret = ctrlm_rf4ce_rib_attr_t::status::SUCCESS;
            LOG_INFO("%s: %s written to RIB: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
        } else {
            LOG_ERROR("%s: buffer is wrong size <%d>\n", __FUNCTION__, len);
            ret = ctrlm_rf4ce_rib_attr_t::status::WRONG_SIZE;
        }
    } else {
        LOG_ERROR("%s: data is NULL\n", __FUNCTION__);
        ret = ctrlm_rf4ce_rib_attr_t::status::INVALID_PARAM;
    }
    return(ret);
}
// end ctrlm_rf4ce_voice_command_status_t

// ctrlm_rf4ce_voice_command_length_t
#define VOICE_COMMAND_LENGTH_ID    (0x11)
#define VOICE_COMMAND_LENGTH_INDEX (0x00)
#define VOICE_COMMAND_LENGTH_LEN   (1)
ctrlm_rf4ce_voice_command_length_t::ctrlm_rf4ce_voice_command_length_t() :
ctrlm_attr_t("Voice Session Length"),
ctrlm_rf4ce_rib_attr_t(VOICE_COMMAND_LENGTH_ID, VOICE_COMMAND_LENGTH_INDEX, ctrlm_rf4ce_rib_attr_t::permission::PERMISSION_BOTH, ctrlm_rf4ce_rib_attr_t::permission::PERMISSION_TARGET)
{
    this->vcl = ctrlm_rf4ce_voice_command_length_t::length::VALUE;
}

ctrlm_rf4ce_voice_command_length_t::~ctrlm_rf4ce_voice_command_length_t() {

}

void ctrlm_rf4ce_voice_command_length_t::set_updated_listener(ctrlm_rf4ce_voice_command_length_listener_t listener) {
    this->updated_listener = listener;
}

std::string ctrlm_rf4ce_voice_command_length_t::length_str(length l) {
    std::stringstream ss; ss << "INVALID";
    switch(l) {
        case ctrlm_rf4ce_voice_command_length_t::length::CONTROLLER_DEFAULT: {ss.str("CONTROLLER_DEFAULT"); break;}
        case ctrlm_rf4ce_voice_command_length_t::length::PROFILE_NEGOTIATION: {ss.str("PROFILE_NEGOTIATION"); break;}
        case ctrlm_rf4ce_voice_command_length_t::length::VALUE: {ss.str(""); ss << "VALUE <" << (int)ctrlm_rf4ce_voice_command_length_t::length::VALUE << " samples>"; break;}
        default: {ss << " <" << (int)l << ">"; break;}
    }
    return(ss.str());
}

std::string ctrlm_rf4ce_voice_command_length_t::get_value() const {
    return(this->length_str(this->vcl));
}

ctrlm_rf4ce_rib_attr_t::status ctrlm_rf4ce_voice_command_length_t::read_rib(ctrlm_rf4ce_rib_attr_t::access accessor, rf4ce_rib_attr_index_t index, char *data, size_t *len) {
    ctrlm_rf4ce_rib_attr_t::status ret = ctrlm_rf4ce_rib_attr_t::status::FAILURE;
    if(data && len) {
        if(*len >= VOICE_COMMAND_LENGTH_LEN) {
            data[0] = this->vcl & 0xFF;
            *len = VOICE_COMMAND_LENGTH_LEN;
            ret = ctrlm_rf4ce_rib_attr_t::status::SUCCESS;
            LOG_INFO("%s: %s read from RIB: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
        } else {
            LOG_ERROR("%s: buffer is not large enough <%d>\n", __FUNCTION__, *len);
            ret = ctrlm_rf4ce_rib_attr_t::status::WRONG_SIZE;
        }
    } else {
        LOG_ERROR("%s: data and/or length is NULL\n", __FUNCTION__);
        ret = ctrlm_rf4ce_rib_attr_t::status::INVALID_PARAM;
    }
    return(ret);
}

ctrlm_rf4ce_rib_attr_t::status ctrlm_rf4ce_voice_command_length_t::write_rib(ctrlm_rf4ce_rib_attr_t::access accessor, rf4ce_rib_attr_index_t index, char *data, size_t len, bool importing) {
    ctrlm_rf4ce_rib_attr_t::status ret = ctrlm_rf4ce_rib_attr_t::status::FAILURE;
    if(data) {
        if(len == VOICE_COMMAND_LENGTH_LEN) {
            ctrlm_rf4ce_voice_command_length_t::length old_length = this->vcl;
            this->vcl = (ctrlm_rf4ce_voice_command_length_t::length)data[0];
            ret = ctrlm_rf4ce_rib_attr_t::status::SUCCESS;
            LOG_INFO("%s: %s written to RIB: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
            if(this->vcl != old_length) {
                if(this->updated_listener) {
                    this->updated_listener(*this);
                }
            }
        } else {
            LOG_ERROR("%s: buffer is wrong size <%d>\n", __FUNCTION__, len);
            ret = ctrlm_rf4ce_rib_attr_t::status::WRONG_SIZE;
        }
    } else {
        LOG_ERROR("%s: data is NULL\n", __FUNCTION__);
        ret = ctrlm_rf4ce_rib_attr_t::status::INVALID_PARAM;
    }
    return(ret);
}
// end ctrlm_rf4ce_voice_command_length_t

// ctrlm_rf4ce_voice_metrics_t
#define VOICE_METRICS_LEN   (44)
ctrlm_rf4ce_voice_metrics_t::ctrlm_rf4ce_voice_metrics_t(ctrlm_obj_network_t *net, ctrlm_controller_id_t id) :
ctrlm_attr_t("Voice Metrics"),
ctrlm_config_attr_t("voice"),
ctrlm_db_attr_t(net, id)
{
    this->voice_cmd_count_today                                = 0;
    this->voice_cmd_count_yesterday                            = 0;
    this->voice_cmd_short_today                                = 0;
    this->voice_cmd_short_yesterday                            = 0;
    this->today                                                = time(NULL) / (60 * 60 * 24);
    this->voice_packets_sent_today                             = 0;
    this->voice_packets_sent_yesterday                         = 0;
    this->voice_packets_lost_today                             = 0;
    this->voice_packets_lost_yesterday                         = 0;
    this->utterances_exceeding_packet_loss_threshold_today     = 0;
    this->utterances_exceeding_packet_loss_threshold_yesterday = 0;
    this->packet_loss_threshold                                = JSON_INT_VALUE_VOICE_PACKET_LOSS_THRESHOLD;//TODO
}

ctrlm_rf4ce_voice_metrics_t::~ctrlm_rf4ce_voice_metrics_t() {

}

void ctrlm_rf4ce_voice_metrics_t::process_time(bool write_db) {
    time_t time_in_seconds = time(NULL);
    time_t shutdown_time   = ctrlm_shutdown_time_get();
    if(time_in_seconds < shutdown_time) {
        LOG_WARN("%s: Current Time <%ld> is less than the last shutdown time <%ld>.  Wait until time updates.\n", __FUNCTION__, time_in_seconds, shutdown_time);
        return;
    }
    uint32_t now = time_in_seconds / (60 * 60 * 24);
    uint32_t day_change = now - this->today;

   //If this is a different day...
   if(day_change > 0) {
      //If this is the next day...
      if(day_change == 1) {
         this->voice_cmd_count_yesterday                            = this->voice_cmd_count_today;
         this->voice_cmd_short_yesterday                            = this->voice_cmd_short_today;
         this->voice_packets_sent_yesterday                         = this->voice_packets_sent_today;
         this->voice_packets_lost_yesterday                         = this->voice_packets_lost_today;
         this->utterances_exceeding_packet_loss_threshold_yesterday = this->utterances_exceeding_packet_loss_threshold_today;
      } else {
         this->voice_cmd_count_yesterday                            = 0;
         this->voice_cmd_short_yesterday                            = 0;
         this->voice_packets_sent_yesterday                         = 0;
         this->voice_packets_lost_yesterday                         = 0;
         this->utterances_exceeding_packet_loss_threshold_yesterday = 0;
      }

      this->voice_cmd_count_today                                   = 0;
      this->voice_cmd_short_today                                   = 0;
      this->voice_packets_sent_today                                = 0;
      this->voice_packets_lost_today                                = 0;
      this->utterances_exceeding_packet_loss_threshold_today        = 0;
      this->today                                                   = now;
      LOG_INFO("%s: %s - day has changed by %u\n", __FUNCTION__, this->get_name().c_str(), day_change);

      if(write_db) {
          ctrlm_db_attr_write(this);
      }
   }
}

void ctrlm_rf4ce_voice_metrics_t::add_packets(uint32_t sent, uint32_t lost) {
    this->voice_packets_sent_today += sent;
    this->voice_packets_lost_today += lost;

    if((((float)lost/(float)sent)*100.0) > (float)(this->packet_loss_threshold)) {
        this->utterances_exceeding_packet_loss_threshold_today++;
    }
}

void ctrlm_rf4ce_voice_metrics_t::increment_voice_count(uint32_t sent, uint32_t lost) {
    this->voice_cmd_count_today++;
    this->add_packets(sent, lost);
    ctrlm_db_attr_write(this);
}

void ctrlm_rf4ce_voice_metrics_t::increment_short_voice_count(uint32_t sent, uint32_t lost) {
    this->voice_cmd_short_today++;
    this->add_packets(sent, lost);
    ctrlm_db_attr_write(this);
}

uint32_t ctrlm_rf4ce_voice_metrics_t::get_commands_today() const {
    return(this->voice_cmd_count_today);
}

uint32_t ctrlm_rf4ce_voice_metrics_t::get_commands_yesterday() const {
    return(this->voice_cmd_count_yesterday);
}

uint32_t ctrlm_rf4ce_voice_metrics_t::get_short_commands_today() const {
    return(this->voice_cmd_short_today);
}

uint32_t ctrlm_rf4ce_voice_metrics_t::get_short_commands_yesterday() const {
    return(this->voice_cmd_short_yesterday);
}

uint32_t ctrlm_rf4ce_voice_metrics_t::get_packets_sent_today() const {
    return(this->voice_packets_sent_today);
}

uint32_t ctrlm_rf4ce_voice_metrics_t::get_packets_sent_yesterday() const {
    return(this->voice_packets_sent_yesterday);
}

uint32_t ctrlm_rf4ce_voice_metrics_t::get_packets_lost_today() const {
    return(this->voice_packets_lost_today);
}

uint32_t ctrlm_rf4ce_voice_metrics_t::get_packets_lost_yesterday() const {
    return(this->voice_packets_lost_yesterday);
}

uint32_t ctrlm_rf4ce_voice_metrics_t::get_packet_loss_exceeding_threshold_today() const {
    return(this->utterances_exceeding_packet_loss_threshold_today);
}

uint32_t ctrlm_rf4ce_voice_metrics_t::get_packet_loss_exceeding_threshold_yesterday() const {
    return(this->utterances_exceeding_packet_loss_threshold_yesterday);
}

float ctrlm_rf4ce_voice_metrics_t::get_average_packet_loss_today() const {
    float ret = 0.0;
    if(this->voice_cmd_count_today > 0) {
        ret = (float)((float)this->voice_packets_lost_today/(float)this->voice_packets_sent_today) * 100.0;
    }
    return(ret);
}

float ctrlm_rf4ce_voice_metrics_t::get_average_packet_loss_yesterday() const {
    float ret = 0.0;
    if(this->voice_cmd_count_yesterday > 0) {
        ret = (float)((float)this->voice_packets_lost_yesterday/(float)this->voice_packets_sent_yesterday) * 100.0;
    }
    return(ret);
}

std::string ctrlm_rf4ce_voice_metrics_t::get_value() const {
    std::stringstream ss;
    ss << "Voice Cmd Count Today <" << this->voice_cmd_count_today << "> ";
    ss << "Voice Cmd Count Yesterday <" << this->voice_cmd_count_yesterday << "> ";
    ss << "Voice Cmd Short Today <" << this->voice_cmd_short_today << "> ";
    ss << "Voice Cmd Short Yesterday <" << this->voice_cmd_short_yesterday << "> "; 
    ss << "Voice Packets Sent Today <" << this->voice_packets_sent_today << "> ";
    ss << "Voice Packets Sent Yesterday <" << this->voice_packets_sent_yesterday << "> ";	
    ss << "Packets Lost Today <" << this->voice_packets_lost_today << "> "; 
    ss << "Packets Lost Yesterday <" << this->voice_packets_lost_yesterday << "> ";
    ss << "Utterances Exceeding Pkt Loss Threshold Today <" << this->utterances_exceeding_packet_loss_threshold_today << "> ";
    ss << "Utterances Exceeding Pkt Loss Threshold Yesterday <" << this->utterances_exceeding_packet_loss_threshold_yesterday << "> ";
    ss << " Today=<" << this->today << "> ";
    return(ss.str());
}

bool ctrlm_rf4ce_voice_metrics_t::read_config() {
    bool ret = false;
    ctrlm_config_int_t threshold("voice.packet_loss_threshold");
    if(threshold.get_config_value(this->packet_loss_threshold, 0, 100)) {
        LOG_INFO("%s: Packet Loss Threshold from config file: %d%%\n", __FUNCTION__, this->packet_loss_threshold);
        ret = true;
    } else {
        LOG_INFO("%s: Packet Loss Threshold default: %d%%\n", __FUNCTION__, this->packet_loss_threshold);
    }
    return(ret);
}

bool ctrlm_rf4ce_voice_metrics_t::read_db(ctrlm_db_ctx_t ctx) {
    bool ret = false;
    ctrlm_db_blob_t blob("voice_cmd_counts", this->get_table());
    char buf[VOICE_METRICS_LEN];

    memset(buf, 0, sizeof(buf));
    if(blob.read_db(ctx)) {
        size_t len = blob.to_buffer(buf, sizeof(buf));
        if(len >= 0) {
            if(len >= VOICE_METRICS_LEN) {
                this->voice_cmd_count_today                                 = ((buf[3]  << 24) | (buf[2]  << 16) | (buf[1]  << 8) | buf[0]);
                this->voice_cmd_count_yesterday                             = ((buf[7]  << 24) | (buf[6]  << 16) | (buf[5]  << 8) | buf[4]);
                this->voice_cmd_short_today                                 = ((buf[11] << 24) | (buf[10] << 16) | (buf[9]  << 8) | buf[8]);
                this->voice_cmd_short_yesterday                             = ((buf[15] << 24) | (buf[14] << 16) | (buf[13] << 8) | buf[12]);
                this->today                                                 = ((buf[19] << 24) | (buf[18] << 16) | (buf[17] << 8) | buf[16]);
                this->voice_packets_sent_today                              = ((buf[23] << 24) | (buf[22] << 16) | (buf[21] << 8) | buf[20]);
                this->voice_packets_sent_yesterday                          = ((buf[27] << 24) | (buf[26] << 16) | (buf[25] << 8) | buf[24]);
                this->voice_packets_lost_today                              = ((buf[31] << 24) | (buf[30] << 16) | (buf[29] << 8) | buf[28]);
                this->voice_packets_lost_yesterday                          = ((buf[35] << 24) | (buf[34] << 16) | (buf[33] << 8) | buf[32]);
                this->utterances_exceeding_packet_loss_threshold_today      = ((buf[39] << 24) | (buf[38] << 16) | (buf[37] << 8) | buf[36]);
                this->utterances_exceeding_packet_loss_threshold_yesterday  = ((buf[43] << 24) | (buf[42] << 16) | (buf[41] << 8) | buf[40]);
                ret = true;
                LOG_INFO("%s: %s read from database: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
            } else {
                LOG_ERROR("%s: data from db is too small <%s>\n", __FUNCTION__, this->get_name().c_str());
            }
        } else {
            LOG_ERROR("%s: failed to convert blob to buffer <%s>\n", __FUNCTION__, this->get_name().c_str());
        }
    } else {
        LOG_ERROR("%s: failed to read from db <%s>\n", __FUNCTION__, this->get_name().c_str());
    }
    return(ret);
}

bool ctrlm_rf4ce_voice_metrics_t::write_db(ctrlm_db_ctx_t ctx) {
    bool ret = false;
    ctrlm_db_blob_t blob("voice_cmd_counts", this->get_table());
    char buf[VOICE_METRICS_LEN];

    memset(buf, 0, sizeof(buf));
    buf[0]  = (uint8_t)(this->voice_cmd_count_today);
    buf[1]  = (uint8_t)(this->voice_cmd_count_today >> 8);
    buf[2]  = (uint8_t)(this->voice_cmd_count_today >> 16);
    buf[3]  = (uint8_t)(this->voice_cmd_count_today >> 24);
    buf[4]  = (uint8_t)(this->voice_cmd_count_yesterday);
    buf[5]  = (uint8_t)(this->voice_cmd_count_yesterday >> 8);
    buf[6]  = (uint8_t)(this->voice_cmd_count_yesterday >> 16);
    buf[7]  = (uint8_t)(this->voice_cmd_count_yesterday >> 24);
    buf[8]  = (uint8_t)(this->voice_cmd_short_today);
    buf[9]  = (uint8_t)(this->voice_cmd_short_today >> 8);
    buf[10] = (uint8_t)(this->voice_cmd_short_today >> 16);
    buf[11] = (uint8_t)(this->voice_cmd_short_today >> 24);
    buf[12] = (uint8_t)(this->voice_cmd_short_yesterday);
    buf[13] = (uint8_t)(this->voice_cmd_short_yesterday >> 8);
    buf[14] = (uint8_t)(this->voice_cmd_short_yesterday >> 16);
    buf[15] = (uint8_t)(this->voice_cmd_short_yesterday >> 24);
    buf[16] = (uint8_t)(this->today);
    buf[17] = (uint8_t)(this->today >> 8);
    buf[18] = (uint8_t)(this->today >> 16);
    buf[19] = (uint8_t)(this->today >> 24);
    buf[20] = (uint8_t)(this->voice_packets_sent_today);
    buf[21] = (uint8_t)(this->voice_packets_sent_today >> 8);
    buf[22] = (uint8_t)(this->voice_packets_sent_today >> 16);
    buf[23] = (uint8_t)(this->voice_packets_sent_today >> 24);
    buf[24] = (uint8_t)(this->voice_packets_sent_yesterday);
    buf[25] = (uint8_t)(this->voice_packets_sent_yesterday >> 8);
    buf[26] = (uint8_t)(this->voice_packets_sent_yesterday >> 16);
    buf[27] = (uint8_t)(this->voice_packets_sent_yesterday >> 24);
    buf[28] = (uint8_t)(this->voice_packets_lost_today);
    buf[29] = (uint8_t)(this->voice_packets_lost_today >> 8);
    buf[30] = (uint8_t)(this->voice_packets_lost_today >> 16);
    buf[31] = (uint8_t)(this->voice_packets_lost_today >> 24);
    buf[32] = (uint8_t)(this->voice_packets_lost_yesterday);
    buf[33] = (uint8_t)(this->voice_packets_lost_yesterday >> 8);
    buf[34] = (uint8_t)(this->voice_packets_lost_yesterday >> 16);
    buf[35] = (uint8_t)(this->voice_packets_lost_yesterday >> 24);
    buf[36] = (uint8_t)(this->utterances_exceeding_packet_loss_threshold_today);
    buf[37] = (uint8_t)(this->utterances_exceeding_packet_loss_threshold_today >> 8);
    buf[38] = (uint8_t)(this->utterances_exceeding_packet_loss_threshold_today >> 16);
    buf[39] = (uint8_t)(this->utterances_exceeding_packet_loss_threshold_today >> 24);
    buf[40] = (uint8_t)(this->utterances_exceeding_packet_loss_threshold_yesterday);
    buf[41] = (uint8_t)(this->utterances_exceeding_packet_loss_threshold_yesterday >> 8);
    buf[42] = (uint8_t)(this->utterances_exceeding_packet_loss_threshold_yesterday >> 16);
    buf[43] = (uint8_t)(this->utterances_exceeding_packet_loss_threshold_yesterday >> 24);
    if(blob.from_buffer(buf, sizeof(buf))) {
        if(blob.write_db(ctx)) {
            ret = true;
            LOG_INFO("%s: %s written to database: %s\n", __FUNCTION__, this->get_name().c_str(), this->get_value().c_str());
        } else {
            LOG_ERROR("%s: failed to write to db <%s>\n", __FUNCTION__, this->get_name().c_str());
        }
    } else {
        LOG_ERROR("%s: failed to convert buffer to blob <%s>\n", __FUNCTION__, this->get_name().c_str());
    }
    return(ret);
}
// end ctrlm_rf4ce_voice_metrics_t
