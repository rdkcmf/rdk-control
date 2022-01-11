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
#ifndef __CTRLM_DB_ATTR_H__
#define __CTRLM_DB_ATTR_H__
#include <string>
#include <vector>
#include "ctrlm.h"

typedef void* ctrlm_db_ctx_t;

/**
 * @brief ControlMgr Database Attribute
 * 
 * This class is the interface for Database Attributes
 */
class ctrlm_db_attr_t {
public:
    /**
     * ControlMgr DB Attribute Constructor (TABLE)
     * @param table The table in which this attribute belongs to in the DB
     */
    ctrlm_db_attr_t(std::string table = "NULL");
    /**
     * ControlMgr DB Attribute Constructor (NETWORK)
     * @param net The network in which this attribute belongs
     */
    ctrlm_db_attr_t(ctrlm_obj_network_t *net);
    /**
     * ControlMgr DB Attribute Constructor (CONTROLLER)
     * @param net The controller's network in which this attribute belongs
     * @param id The controller's id in which this attribute belongs
     */
    ctrlm_db_attr_t(ctrlm_obj_network_t *net, ctrlm_controller_id_t id);
    /**
     * ControlMgr DB Attribute Destructor
     */
    virtual ~ctrlm_db_attr_t();

public:
    /**
     * Getter function for the attribute's DB table
     * @return The table for the attribute
     */
    std::string get_table() const;
    /**
     * Setter function for the attribute's DB table
     * @param table The table for the attribute
     */
    void set_table(std::string table);

public:
    /**
     * Interface for class extensions to implement reading their data from DB
     * @return True if the value was read from the DB, otherwise False
     */
    virtual bool read_db(ctrlm_db_ctx_t ctx) = 0;
    /**
     * Interface for class extensions to implement writing their data to DB
     * @return True if the value was queued to be written to the DB, otherwise False
     */
    virtual bool write_db(ctrlm_db_ctx_t ctx) = 0;

protected:
    std::string table;
};

#endif