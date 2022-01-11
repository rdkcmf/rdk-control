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
#ifndef __CTRLM_ATTR_GENERAL_H__
#define __CTRLM_ATTR_GENERAL_H__
#include "ctrlm_attr.h"
#include <stdint.h>

/**
 * @brief ControlMgr IEEE Address Class
 * 
 * This class is used to handle the IEEE address attribute for ControlMgr.
 */
class ctrlm_ieee_addr_t : public ctrlm_attr_t {
public:
    /**
     * ControlMgr IEEE Address Contructor
     * @param ieee A string containing an ieee address
     */
    ctrlm_ieee_addr_t();
    /**
     * ControlMgr IEEE Address Contructor (uint64_t)
     * @param ieee A uint64_t containing an IEEE address
     */
    ctrlm_ieee_addr_t(uint64_t ieee);
    /**
     * ControlMgr IEEE Address Destructor
     */
    virtual ~ctrlm_ieee_addr_t();

public:
    /**
     * Function to get the IEEE address string
     * @param colons Set to true if the ieee address string should include colons, otherwise False
     * @return The IEEE address string
     */
    std::string to_string(bool colons = true) const;
    /**
     * Function to get the IEEE address uint64_t
     * @return The IEEE address packed into an uin64_t
     */
    uint64_t to_uint64() const;
    /**
     * Function to set the IEEE address from a uint64_t
     * @param ieee uint64_t containing the ieee address
     */
    void from_uint64(uint64_t ieee);

public:
    /**
     * Implementation of the ctrlm_attr_t get_value interface
     * @see ctrlm_attr_t::get_value()
     */
    std::string get_value() const;

protected:
    uint64_t ieee;
};

/**
 * Overloaded Operators
 */
inline bool operator== (const ctrlm_ieee_addr_t& ieee, const ctrlm_ieee_addr_t& ieee2) {
    return(ieee.to_uint64() == ieee2.to_uint64());
}
inline bool operator== (const ctrlm_ieee_addr_t& ieee, const uint64_t& ieee2) {
    return(ieee == ctrlm_ieee_addr_t(ieee2));
}
inline bool operator== (const uint64_t& ieee2, const ctrlm_ieee_addr_t& ieee) {
    return(ieee == ieee2);
}
inline bool operator!= (const ctrlm_ieee_addr_t& ieee, const uint64_t& ieee2) {
    return(!(ieee == ieee2));
}
inline bool operator!= (const uint64_t& ieee, const ctrlm_ieee_addr_t& ieee2) {
    return(!(ieee == ieee2));
}

/**
 * @brief ControlMgr Product Name Class
 * 
 * This is the class that contains the name of a product.
 */
class ctrlm_product_name_t : public ctrlm_attr_t {
public:
    /**
     * ControlMgr Product Name Contructor
     * @param product_name The name of the product
     */
    ctrlm_product_name_t(std::string product_name = "");
    /**
     * ControlMgr Product Name Destructor
     */
    virtual ~ctrlm_product_name_t();

public:
    /**
     * Function for setting the product name
     * @param product_name The name of the product
     */
    virtual void set_product_name(std::string product_name);

public:
    /**
     * Implementation of the ctrlm_attr_t get_value interface
     * @see ctrlm_attr_t::get_value()
     */
    std::string get_value() const;

protected:
    std::string product_name;
};

/**
 * @brief ControlMgr Audio Profiles Class
 * 
 * This class contains the audio profiles supported by a device.
 */
class ctrlm_audio_profiles_t : public ctrlm_attr_t {
public:
    /**
     * Enum containing the available profiles 
     */
    enum profile{
        NONE                = 0x00,
        ADPCM_16BIT_16KHZ   = 0x01,
        PCM_16BIT_16KHZ     = 0x02,
        OPUS_16BIT_16KHZ    = 0x04
    };

public:
    /**
     * ControlMgr Audio Profiles Constructor
     * @param profiles The profiles supported by the device (bitfield)
     */
    ctrlm_audio_profiles_t(int profiles = NONE);
    /**
     * ControlMgr Audio Profiles Destructor
     */
    ~ctrlm_audio_profiles_t();

public:
    /**
     * Function to get the profiles
     * @return An integer with each profile bit set for the supported profiles
     */
    int get_profiles() const;
    /**
     * Function to set the profiles
     * @param profiles The profile bitfield to set the supported profiles
     */
    void set_profiles(int profiles);
    /**
     * Function to check if a specific profile is supported
     * @param p The profile in question
     * @return True if the profile is supported, otherwise False
     */
    bool supports(profile p) const;
    /**
     * Static function to get a string for a specific profile
     */
    static std::string profile_str(profile p);

public:
    /**
     * Implementation of the ctrlm_attr_t get_value interface
     * @see ctrlm_attr_t::get_value()
     */
    std::string get_value() const;

protected:
    int supported_profiles;
};

/**
 * @brief ControlMgr Controller Capabilities Class
 * 
 * This class maintains the capabilties for a controller.
 */
class ctrlm_controller_capabilities_t : public ctrlm_attr_t {
public:
    /**
     * Enum of the different capabilities
     */
    enum capability{
        FMR      = 0,
        PAR,
        HAPTICS,
        INVALID
    };

public:
    /**
     * ControlMgr Controller Capabilities Constructor
     */
    ctrlm_controller_capabilities_t();
    /**
     * ControlMgr Controller Capabilities Copy Constructor
     */
    ctrlm_controller_capabilities_t(const ctrlm_controller_capabilities_t& cap);
    /**
     * ControlMgr Controller Capabilities Destructor
     */
    virtual ~ctrlm_controller_capabilities_t();
    // operators
    bool operator==(const ctrlm_controller_capabilities_t& cap);
    bool operator!=(const ctrlm_controller_capabilities_t& cap);
    // end operators

public:
    /**
     * Function to add a capability to the controller
     * @param c The capability
     */
    void add_capability(capability c);
    /**
     * Function to remove a capability to the controller
     * @param c The capability
     */
    void remove_capability(capability c);
    /**
     * Function to check if a controller support a capability
     * @param c The capability
     */
    bool has_capability(capability c) const;
    /**
     * Static function to get the string for a capability
     * @param c The capability
     */
    static std::string capability_str(capability c);
public:
    /**
     * Implementation of the ctrlm_attr_t get_value interface
     * @see ctrlm_attr_t::get_value()
     */
    std::string get_value() const;

private:
    bool capabilities[capability::INVALID];
};

#endif