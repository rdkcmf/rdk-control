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
#include "ctrlm_attr_general.h"
#include <sstream>

// ctrlm_ieee_addr_t
ctrlm_ieee_addr_t::ctrlm_ieee_addr_t() : ctrlm_attr_t("IEEE Address") {
    this->from_uint64(0);
}

ctrlm_ieee_addr_t::ctrlm_ieee_addr_t(uint64_t ieee) : ctrlm_attr_t("IEEE Address") {
    this->from_uint64(ieee);
}

ctrlm_ieee_addr_t::~ctrlm_ieee_addr_t() {

}

std::string ctrlm_ieee_addr_t::to_string(bool colons) const {
    std::stringstream ret;
    int max = sizeof(uint64_t)-1;
    for(int i = max; i >= 0; i--) {
        unsigned char octet = (this->ieee >> (8 * i)) & 0xFF;
        if(i != max && colons) {
            ret << ":";
        }
        ret << COUT_HEX_MODIFIER << (int)octet;
    }
    return(ret.str());
}

uint64_t ctrlm_ieee_addr_t::to_uint64() const {
    return(this->ieee);
}

void ctrlm_ieee_addr_t::from_uint64(uint64_t ieee) {
    this->ieee = ieee;
}

std::string ctrlm_ieee_addr_t::get_value() const {
    return(this->to_string());
}

// end ctrlm_ieee_addr_t

// ctrlm_product_name_t
ctrlm_product_name_t::ctrlm_product_name_t(std::string product_name) : ctrlm_attr_t("Product Name") {
    this->product_name = product_name;
}

ctrlm_product_name_t::~ctrlm_product_name_t() {

}

void ctrlm_product_name_t::set_product_name(std::string product_name) {
    this->product_name = product_name;
}

std::string ctrlm_product_name_t::get_value() const {
    return(this->product_name);
}
// end ctrlm_product_name_t

// ctrlm_audio_profiles_t
ctrlm_audio_profiles_t::ctrlm_audio_profiles_t(int profiles) : ctrlm_attr_t("Audio Profiles") {
    this->supported_profiles = profiles;
}

ctrlm_audio_profiles_t::~ctrlm_audio_profiles_t() {

}

int ctrlm_audio_profiles_t::get_profiles() const {
    return(this->supported_profiles);
}

void ctrlm_audio_profiles_t::set_profiles(int profiles) {
    this->supported_profiles = profiles;
}

bool ctrlm_audio_profiles_t::supports(ctrlm_audio_profiles_t::profile p) const {
    return(this->supported_profiles & p);
}

std::string ctrlm_audio_profiles_t::profile_str(ctrlm_audio_profiles_t::profile p) {
    std::stringstream ss; ss << "INVALID";
    switch(p) {
        case ctrlm_audio_profiles_t::profile::NONE:               {ss.str("NONE"); break;}
        case ctrlm_audio_profiles_t::profile::ADPCM_16BIT_16KHZ:  {ss.str("ADPCM_16BIT_16KHZ"); break;}
        case ctrlm_audio_profiles_t::profile::PCM_16BIT_16KHZ:    {ss.str("PCM_16BIT_16KHZ"); break;}
        case ctrlm_audio_profiles_t::profile::OPUS_16BIT_16KHZ:   {ss.str("OPUS_16BIT_16KHZ"); break;}
        default:  {ss << " (" << (int)p << ")"; break;}
    }
    return(ss.str());
}

std::string ctrlm_audio_profiles_t::get_value() const {
    std::stringstream ss;
    if(this->supported_profiles == ctrlm_audio_profiles_t::profile::NONE) {
        ss << "NONE";
    } else {
        bool comma = false;
        if(this->supported_profiles & ctrlm_audio_profiles_t::profile::ADPCM_16BIT_16KHZ) {
            ss << this->profile_str(ctrlm_audio_profiles_t::profile::ADPCM_16BIT_16KHZ);
            comma = true;
        }
        if(this->supported_profiles & ctrlm_audio_profiles_t::profile::PCM_16BIT_16KHZ) {
            ss << (comma ? "," : "") << this->profile_str(ctrlm_audio_profiles_t::profile::PCM_16BIT_16KHZ);
            comma = true;
        }
        if(this->supported_profiles & ctrlm_audio_profiles_t::profile::OPUS_16BIT_16KHZ) {
            ss << (comma ? "," : "") << this->profile_str(ctrlm_audio_profiles_t::profile::OPUS_16BIT_16KHZ);
            comma = true;
        }
    }
    return(ss.str());
}
// end ctrlm_audio_profiles_t

// ctrlm_controller_capabilities_t
ctrlm_controller_capabilities_t::ctrlm_controller_capabilities_t() : ctrlm_attr_t("Controller Capabilities") {
    for(int i = 0; i < ctrlm_controller_capabilities_t::capability::INVALID; i++) {
        this->capabilities[i] = false;
    }
}

ctrlm_controller_capabilities_t::ctrlm_controller_capabilities_t(const ctrlm_controller_capabilities_t& cap) {
    for(int i = 0; i < ctrlm_controller_capabilities_t::capability::INVALID; i++) {
        this->capabilities[i] = cap.has_capability((ctrlm_controller_capabilities_t::capability)i);
    }
}

ctrlm_controller_capabilities_t::~ctrlm_controller_capabilities_t() {

}

bool ctrlm_controller_capabilities_t::operator==(const ctrlm_controller_capabilities_t& cap) {
    bool ret = true;
    for(int i = 0; i < ctrlm_controller_capabilities_t::capability::INVALID; i++) {
        if(this->has_capability((ctrlm_controller_capabilities_t::capability)i) != cap.has_capability((ctrlm_controller_capabilities_t::capability)i)) {
            ret = false;
            break;
        }
    }
    return(ret);
}

bool ctrlm_controller_capabilities_t::operator!=(const ctrlm_controller_capabilities_t& cap) {
    return(!(*this == cap));
}

void ctrlm_controller_capabilities_t::add_capability(ctrlm_controller_capabilities_t::capability c) {
    this->capabilities[c] = true;
}

void ctrlm_controller_capabilities_t::remove_capability(ctrlm_controller_capabilities_t::capability c) {
    this->capabilities[c] = false;
}

bool ctrlm_controller_capabilities_t::has_capability(ctrlm_controller_capabilities_t::capability c) const {
    return(this->capabilities[c]);
}

std::string ctrlm_controller_capabilities_t::capability_str(ctrlm_controller_capabilities_t::capability c) {
    std::stringstream ss; ss << "INVALID";
    switch(c) {
        case ctrlm_controller_capabilities_t::capability::FMR:     {ss.str("FindMyRemote"); break;}
        case ctrlm_controller_capabilities_t::capability::PAR:     {ss.str("PressAndReleaseVoice"); break;}
        case ctrlm_controller_capabilities_t::capability::HAPTICS: {ss.str("Haptics"); break;}
        default: {ss << " <" << (int)c << ">"; break;}
    }
    return(ss.str());
}

std::string ctrlm_controller_capabilities_t::get_value() const {
    std::stringstream ret;
    bool comma = false;
    for(int i = 0; i < ctrlm_controller_capabilities_t::capability::INVALID; i++) {
        if(this->capabilities[i] == true) {
            if(comma) {
                ret << ",";
            }
            ret << this->capability_str((ctrlm_controller_capabilities_t::capability)i);
            comma = true;
        }
    }
    return(ret.str());
}
// end ctrlm_controller_capabilities_t