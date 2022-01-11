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
#ifndef __CTRLM_RFC_ATTR_H__
#define __CTRLM_RFC_ATTR_H__
#include <string>

class ctrlm_rfc_attr_t;
/**
 * This is the callback type that will be called when an RFC attribute value has changed
 */
typedef void(*ctrlm_rfc_attr_changed_t)(ctrlm_rfc_attr_t *attr, void *data);

/**
 * @brief ControlMgr RFC Attribute
 * 
 * This class is the interface for RFC Attributes
 */
class ctrlm_rfc_attr_t {
public:
    /**
     * ControlMgr RFC Attribute Constructor
     * @param tr181_string The TR181 string associated with the attribute
     * @param enabled An optional parameter to enable/disable RFC for this attribute (mostly for Development)
     */
    ctrlm_rfc_attr_t(std::string tr181_string, bool enabled = true);
    /**
     * ControlMgr RFC Attribute Destructor
     */
    virtual ~ctrlm_rfc_attr_t();

public:
    /**
     * Getter for the TR181 string associated with the attribute
     * @return The TR181 string associated with the attribute
     */
    std::string get_tr181_string() const;
    /**
     * Getter for the enabled value for this attribute
     * @return True if enabled, otherwise False
     */
    bool        is_enabled() const;
    /**
     * Setter for the enabled value for this attribute
     * @param enabled True if enabled, otherwise False
     */
    void        set_enabled(bool enabled);
    /**
     * Set the callback that is called when the attributes value has changed
     * @param listener The callback
     * @param user_data Application data that the developer wants to pass to the callback
     */
    void        set_changed_listener(ctrlm_rfc_attr_changed_t listener, void *user_data);

public:
    /**
     * Interface function that the attribute will implement to convert the RFC
     * string value to it's internal data structure
     * @param value The string received from RFC
     */
    virtual void rfc_value(std::string value) = 0;

protected:
    /**
     * This function is called when the attribute has detected that a change in value
     * has occured, which will trigger the user callback
     */
    void notify_changed();

private:
    std::string tr181_string;
    bool enabled;
    ctrlm_rfc_attr_changed_t listener;
    void *user_data;
};

#endif