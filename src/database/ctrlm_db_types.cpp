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
#include <iostream>
#include "ctrlm_db_types.h"
#include <sqlite3.h>
#include <cstring>
#include "ctrlm_log.h"

// ctrlm_db_obj_t
ctrlm_db_obj_t::ctrlm_db_obj_t(std::string key, std::string table) {
    this->key   = key;
    this->table = table;
}

ctrlm_db_obj_t::~ctrlm_db_obj_t() {

}

bool ctrlm_db_obj_t::read_db(ctrlm_db_ctx_t ctx) {
    bool ret = false;
    sqlite3 *handle = (sqlite3 *)ctx;
    LOG_INFO("%s: reading blob %s from table %s\n", __FUNCTION__, this->key.c_str(), this->table.c_str());
    if(handle) {
        sqlite3_stmt *stmt = NULL;
        std::string query = "SELECT value FROM " + this->table + " WHERE key='" + this->key + "';";
        int rc = sqlite3_prepare(handle, query.c_str(), query.length(), &stmt, NULL);
        if(rc == SQLITE_OK && stmt) {
            rc = sqlite3_step(stmt);
            if(rc == SQLITE_ROW) {
                ret = this->extract_data(stmt);
            } else {
                LOG_WARN("%s: no row found for <%s, %s>\n", __FUNCTION__, this->table.c_str(), this->key.c_str());
            }
        } else {
            LOG_ERROR("%s: failed to prepare SQL statement <%d, %s>\n", __FUNCTION__, rc, sqlite3_errmsg(handle));
        }
        rc = sqlite3_finalize(stmt);
        if(rc != SQLITE_OK) {
            LOG_ERROR("%s: failed to finalize SQL statement <%d, %s>\n", __FUNCTION__, rc, sqlite3_errmsg(handle));
        }
    } else {
        LOG_ERROR("%s: database handle is NULL\n", __FUNCTION__);
    }
    return(ret);
}

bool ctrlm_db_obj_t::write_db(ctrlm_db_ctx_t ctx) {
    bool ret = false;
    sqlite3 *handle = (sqlite3 *)ctx;
    LOG_INFO("%s: writing blob %s to table %s\n", __FUNCTION__, this->key.c_str(), this->table.c_str());
    if(handle) {
        sqlite3_stmt *stmt = NULL;
        std::string query = "INSERT OR REPLACE INTO " + this->table + "(key,value) VALUES (?,?);";
        int rc = sqlite3_prepare(handle, query.c_str(), -1, &stmt, NULL);
        if(rc == SQLITE_OK && stmt) {
            rc  = sqlite3_bind_text(stmt, 1, this->key.c_str(), -1, SQLITE_STATIC);
            rc |= this->bind_data(stmt, 2);
            if(rc == SQLITE_OK) {
                rc = sqlite3_step(stmt);
                if(rc == SQLITE_DONE) {
                    LOG_DEBUG("%s: %s written to database successfully\n", __FUNCTION__, this->key.c_str());
                    ret = true;
                } else {
                    LOG_ERROR("%s: failed to SQL step <%d, %s>\n", __FUNCTION__, rc, sqlite3_errmsg(handle));
                    LOG_DEBUG("%s: statement <%s>\n", __FUNCTION__, sqlite3_expanded_sql(stmt));
                }
            } else {
                LOG_ERROR("%s: failed to SQL bind <%d, %s>\n", __FUNCTION__, rc, sqlite3_errmsg(handle));
            }
        } else {
            LOG_ERROR("%s: failed to prepare SQL statement <%d, %s>\n", __FUNCTION__, rc, sqlite3_errmsg(handle));
        }
        rc = sqlite3_finalize(stmt);
        if(rc != SQLITE_OK) {
            LOG_ERROR("%s: failed to finalize SQL statement <%d, %s>\n", __FUNCTION__, rc, sqlite3_errmsg(handle));
        }
    } else {
        LOG_ERROR("%s: database handle is NULL\n", __FUNCTION__);
    }
    return(ret);
}
// end ctrlm_db_obj_t

// ctrlm_db_blob_t
ctrlm_db_blob_t::ctrlm_db_blob_t(std::string key, std::string table, std::string data) : ctrlm_db_obj_t(key, table) {
    this->from_string(data);
}

ctrlm_db_blob_t::ctrlm_db_blob_t(std::string key, std::string table, char *data, size_t length) : ctrlm_db_obj_t(key, table) {
    this->from_buffer(data, length);
}

ctrlm_db_blob_t::~ctrlm_db_blob_t() {
    
}

std::string ctrlm_db_blob_t::to_string() const {
    std::string ret = std::string(this->blob.begin(), this->blob.end());
    // possible the string contains null bytes, so make a new string from the c_str
    ret = std::string(ret.c_str());
    return(ret);
}

size_t ctrlm_db_blob_t::to_buffer(char *data, size_t length) {
    size_t ret = -1;
    if(data) {
        if(this->blob.size() <= length) {
            memcpy(data, this->blob.data(), this->blob.size());
            ret = this->blob.size();
        } else {
            LOG_WARN("%s: blob too large for buffer (blob <%d>, buf <%d>)\n", __FUNCTION__, this->blob.size(), length);
        }
    } else {
        LOG_ERROR("%s: buffer is NULL\n", __FUNCTION__);
    }
    return(ret);
}

bool ctrlm_db_blob_t::from_string(std::string &str) {
    bool ret = false;
    // Clear current contents
    this->blob.clear();
    // Copy new contents
    if(str.length() > 0) {
        std::copy(str.begin(), str.end(), std::back_inserter(this->blob));
        ret = true;
    } else {
        LOG_DEBUG("%s: empty string\n", __FUNCTION__);
    }
    return(ret);
}

bool ctrlm_db_blob_t::from_buffer(char *data, size_t length) {
    bool ret = false;
    // Clear current contents
    this->blob.clear();
    if(data) {
        // Copy new contents
        if(length > 0) {
            std::copy(data, data + length, std::back_inserter(this->blob));
            ret = true;
        } else {
            LOG_WARN("%s: empty buffer");
        }
    } else {
        LOG_ERROR("%s: buffer is NULL\n", __FUNCTION__);
    }
    return(ret);
}

int ctrlm_db_blob_t::bind_data(ctrlm_db_stmt_t stmt, int index) {
    int ret = sqlite3_bind_blob((sqlite3_stmt*)stmt, index, (const unsigned char *)this->blob.data(), this->blob.size(), SQLITE_STATIC);
    return(ret);
}

bool ctrlm_db_blob_t::extract_data(ctrlm_db_stmt_t stmt) {
    size_t col_len = sqlite3_column_bytes((sqlite3_stmt*)stmt, 0);
    const char *sql_data = (char *)sqlite3_column_blob((sqlite3_stmt*)stmt, 0);
    return(this->from_buffer((char *)sql_data, col_len));
}
// end ctrlm_db_blob_t

// ctrlm_db_uint64_t
ctrlm_db_uint64_t::ctrlm_db_uint64_t(std::string key, std::string table) : ctrlm_db_obj_t(key, table) {
    this->set_uint64(0);
}

ctrlm_db_uint64_t::ctrlm_db_uint64_t(std::string key, std::string table, uint64_t data) : ctrlm_db_obj_t(key, table) {
    this->set_uint64(data);
}

ctrlm_db_uint64_t::~ctrlm_db_uint64_t() {

}

uint64_t ctrlm_db_uint64_t::get_uint64() const {
    return(this->data);
}

void ctrlm_db_uint64_t::set_uint64(uint64_t data) {
    this->data = data;
}

int ctrlm_db_uint64_t::bind_data(ctrlm_db_stmt_t stmt, int index) {
    int ret = sqlite3_bind_int64((sqlite3_stmt*)stmt, index, this->get_uint64());
    return(ret);
}

bool ctrlm_db_uint64_t::extract_data(ctrlm_db_stmt_t stmt) {
    this->set_uint64(sqlite3_column_int64((sqlite3_stmt*)stmt, 0));
    return(true);
}
// end ctrlm_db_uint64_t