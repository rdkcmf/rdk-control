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
#include "ctrlm_version.h"
#include <sstream>
#include "ctrlm_log.h"

// ctrlm_version_t
ctrlm_version_tt::ctrlm_version_tt() : ctrlm_attr_t("Version") {

}

ctrlm_version_tt::~ctrlm_version_tt() {
    
}
// end ctrlm_version_t

// ctrlm_sw_version_t
ctrlm_sw_version_t::ctrlm_sw_version_t(version_num major, version_num minor, version_num revision, version_num patch) : ctrlm_version_tt() {
    this->major    = major;
    this->minor    = minor;
    this->revision = revision;
    this->patch    = patch;
    this->set_name_prefix("SW ");
}

ctrlm_sw_version_t::ctrlm_sw_version_t(const ctrlm_sw_version_t &version) : ctrlm_version_tt() {
    this->major    = version.major;
    this->minor    = version.minor;
    this->revision = version.revision;
    this->patch    = version.patch;
    this->set_name_prefix("SW ");
}

ctrlm_sw_version_t::~ctrlm_sw_version_t() {
    
}

version_num ctrlm_sw_version_t::get_major() const {
    return(this->major);
}

version_num ctrlm_sw_version_t::get_minor() const {
    return(this->minor);
}

version_num ctrlm_sw_version_t::get_revision() const {
    return(this->revision);
}

version_num ctrlm_sw_version_t::get_patch() const {
    return(this->patch);
}

void ctrlm_sw_version_t::set_major(version_num major) {
    this->major = major;
}

void ctrlm_sw_version_t::set_minor(version_num minor) {
    this->minor = minor;
}

void ctrlm_sw_version_t::set_revision(version_num revision) {
    this->revision = revision;
}

void ctrlm_sw_version_t::set_patch(version_num patch) {
    this->patch = patch;
}

version_software_t ctrlm_sw_version_t::to_versiont() const {
    version_software_t version;
    version.major    = this->major    & 0xFF;
    version.minor    = this->minor    & 0xFF;
    version.revision = this->revision & 0xFF;
    version.patch    = this->patch    & 0xFF;
    return(version);
}

std::string ctrlm_sw_version_t::get_value() const {
    std::stringstream ss;
    ss << this->major << "." << this->minor << "." << this->revision << "." << this->patch;
    return(ss.str());
}
// end ctrlm_sw_version_t

// ctrlm_hw_version_t
ctrlm_hw_version_t::ctrlm_hw_version_t(version_num manufacturer, version_num model, version_num revision, version_num lot) : ctrlm_version_tt() {
    this->manufacturer    = manufacturer;
    this->model    = model;
    this->revision = revision;
    this->lot    = lot;
    this->set_name_prefix("HW ");
}

ctrlm_hw_version_t::ctrlm_hw_version_t(const ctrlm_hw_version_t &version) : ctrlm_version_tt() {
    this->manufacturer    = version.manufacturer;
    this->model    = version.model;
    this->revision = version.revision;
    this->lot    = version.lot;
    this->set_name_prefix("HW ");
}

ctrlm_hw_version_t::~ctrlm_hw_version_t() {
    
}

version_num ctrlm_hw_version_t::get_manufacturer() const {
    return(this->manufacturer);
}

version_num ctrlm_hw_version_t::get_model() const {
    return(this->model);
}

version_num ctrlm_hw_version_t::get_revision() const {
    return(this->revision);
}

version_num ctrlm_hw_version_t::get_lot() const {
    return(this->lot);
}

void ctrlm_hw_version_t::set_manufacturer(version_num manufacturer) {
    this->manufacturer = manufacturer;
}

void ctrlm_hw_version_t::set_model(version_num model) {
    this->model = model;
}

void ctrlm_hw_version_t::set_revision(version_num revision) {
    this->revision = revision;
}

void ctrlm_hw_version_t::set_lot(version_num lot) {
    this->lot = lot;
}

version_hardware_t ctrlm_hw_version_t::to_versiont() const {
    version_hardware_t version;
    version.manufacturer    = this->manufacturer    & 0xFF;
    version.model           = this->model           & 0xFF;
    version.hw_revision     = this->revision        & 0xFF;
    version.lot_code        = this->lot             & 0xFF;
    return(version);
}

void ctrlm_hw_version_t::from(ctrlm_hw_version_t *v) {
    if(v) {
        this->manufacturer = v->get_manufacturer();
        this->model        = v->get_model();
        this->revision     = v->get_revision();
        this->lot          = v->get_lot();
    }
}

std::string ctrlm_hw_version_t::get_value() const {
    std::stringstream ss;
    ss << this->manufacturer << "." << this->model << "." << this->revision << "." << this->lot;
    return(ss.str());
}
// end ctrlm_hw_version_t

// ctrlm_sw_build_id_t
ctrlm_sw_build_id_t::ctrlm_sw_build_id_t(std::string id) : ctrlm_version_tt() {
    this->set_name("Build ID");
    this->id = id;
}

ctrlm_sw_build_id_t::~ctrlm_sw_build_id_t() {

}

std::string ctrlm_sw_build_id_t::get_id() const {
    return(this->id);
}

void ctrlm_sw_build_id_t::set_id(std::string id) {
    this->id = id;
}

std::string ctrlm_sw_build_id_t::get_value() const {
    return(this->id);
}
// end ctrlm_sw_build_id_t