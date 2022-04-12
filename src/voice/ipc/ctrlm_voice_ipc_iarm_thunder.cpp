/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
#include "ctrlm_voice_ipc_iarm_thunder.h"
#include "ctrlm_ipc.h"
#include "ctrlm_ipc_voice.h"
#include <functional>
#include "jansson.h"
#include "ctrlm_voice_obj.h"
#include <string>
#include <algorithm>

#define JSON_ENCODE_FLAGS                             (JSON_COMPACT)
#define JSON_DECODE_FLAGS                             (JSON_DECODE_ANY)

#define JSON_DEREFERENCE(x)                         if(x) {             \
                                                        json_decref(x); \
                                                        x = NULL;       \
                                                    }

#define JSON_REMOTE_ID                                "remoteId"
#define JSON_SESSION_ID                               "sessionId"
#define JSON_DEVICE_TYPE                              "deviceType"
#define JSON_KEYWORD_VERIFICATION                     "keywordVerification"
#define JSON_KEYWORD_VERIFIED                         "verified"
#define JSON_THUNDER_RESULT                           "success" // Thunder uses success key value to determine whether the command was succcessful
#define JSON_STATUS                                   "status"
#define JSON_URL_PTT                                  "urlPtt"
#define JSON_URL_HF                                   "urlHf"
#define JSON_WW_FEEDBACK                              "wwFeedback"
#define JSON_PRV                                      "prv"
#define JSON_CAPABILITIES                             "capabilities"
#define JSON_MESSAGE                                  "message"
#define JSON_STREAM_END_REASON                        "reason"
#define JSON_SESSION_END_RESULT                       "result"
#define JSON_SESSION_END_RESULT_SUCCESS               "success"
#define JSON_SESSION_END_RESULT_ERROR                 "error"
#define JSON_SESSION_END_RESULT_ABORT                 "abort"
#define JSON_SESSION_END_RESULT_SHORT                 "shortUtterance"
#define JSON_SESSION_END_TRANSCRIPTION                "transcription"
#define JSON_SESSION_END_PROTOCOL_ERROR               "protocolErrorCode"
#define JSON_SESSION_END_PROTOCOL_LIBRARY_ERROR       "protocolLibraryErrorCode"
#define JSON_SESSION_END_SERVER_ERROR                 "serverErrorCode"
#define JSON_SESSION_END_SERVER_STR                   "serverErrorString"
#define JSON_SESSION_END_INTERNAL_ERROR               "internalErrorCode"
#define JSON_SESSION_END_ERROR_REASON                 "reason"
#define JSON_SESSION_END_ABORT_REASON                 "reason"
#define JSON_SESSION_END_SHORT_REASON                 "reason"
#define JSON_SESSION_END_STB_STATS                    "stbStats"
#define JSON_SESSION_END_STB_STATS_TYPE               "type"
#define JSON_SESSION_END_STB_STATS_FIRMWARE           "firmware"
#define JSON_SESSION_END_STB_STATS_DEVICE_ID          "deviceId"
#define JSON_SESSION_END_STB_STATS_CTRLM_VERSION      "ctrlmVersion"
#define JSON_SESSION_END_STB_STATS_CONTROLLER_VERSION "controllerVersion"
#define JSON_SESSION_END_STB_STATS_CONTROLLER_TYPE    "controllerType"
#define JSON_SESSION_END_SERVER_STATS                 "serverStats"
#define JSON_SESSION_END_SERVER_STATS_IP              "serverIp"
#define JSON_SESSION_END_SERVER_STATS_DNS_TIME        "dnsTime"
#define JSON_SESSION_END_SERVER_STATS_CONNECT_TIME    "connectTime"

static bool broadcast_event(const char *bus_name, int event, const char *str);
static const char *voice_device_str(ctrlm_voice_device_t device);
static const char *voice_device_status_str(ctrlm_voice_device_status_t status);

ctrlm_voice_ipc_iarm_thunder_t::ctrlm_voice_ipc_iarm_thunder_t(ctrlm_voice_t *obj_voice): ctrlm_voice_ipc_t(obj_voice) {

}

bool ctrlm_voice_ipc_iarm_thunder_t::register_ipc() const {
    bool ret = true;
    IARM_Result_t rc;
    LOG_INFO("%s: Thunder\n", __FUNCTION__);
    // NOTE: The IARM events are registered in ctrlm_main.cpp

    LOG_INFO("%s: Registering for %s IARM call\n", __FUNCTION__, CTRLM_VOICE_IARM_CALL_STATUS);
    rc = IARM_Bus_RegisterCall(CTRLM_VOICE_IARM_CALL_STATUS, &ctrlm_voice_ipc_iarm_thunder_t::status);
    if(rc != IARM_RESULT_SUCCESS) {
        LOG_ERROR("%s: Failed to register %d\n", __FUNCTION__, rc);
        ret = false;
    }

    LOG_INFO("%s: Registering for %s IARM call\n", __FUNCTION__, CTRLM_VOICE_IARM_CALL_CONFIGURE_VOICE);
    rc = IARM_Bus_RegisterCall(CTRLM_VOICE_IARM_CALL_CONFIGURE_VOICE, &ctrlm_voice_ipc_iarm_thunder_t::configure_voice);
    if(rc != IARM_RESULT_SUCCESS) {
        LOG_ERROR("%s: Failed to register %d\n", __FUNCTION__, rc);
        ret = false;
    }

    LOG_INFO("%s: Registering for %s IARM call\n", __FUNCTION__, CTRLM_VOICE_IARM_CALL_SET_VOICE_INIT);
    rc = IARM_Bus_RegisterCall(CTRLM_VOICE_IARM_CALL_SET_VOICE_INIT, &ctrlm_voice_ipc_iarm_thunder_t::set_voice_init);
    if(rc != IARM_RESULT_SUCCESS) {
        LOG_ERROR("%s: Failed to register %d\n", __FUNCTION__, rc);
        ret = false;
    }

    LOG_INFO("%s: Registering for %s IARM call\n", __FUNCTION__, CTRLM_VOICE_IARM_CALL_SEND_VOICE_MESSAGE);
    rc = IARM_Bus_RegisterCall(CTRLM_VOICE_IARM_CALL_SEND_VOICE_MESSAGE, &ctrlm_voice_ipc_iarm_thunder_t::send_voice_message);
    if(rc != IARM_RESULT_SUCCESS) {
        LOG_ERROR("%s: Failed to register %d\n", __FUNCTION__, rc);
        ret = false;
    }

    LOG_INFO("%s: Registering for %s IARM call\n", __FUNCTION__, CTRLM_VOICE_IARM_CALL_SESSION_BY_TEXT);
    rc = IARM_Bus_RegisterCall(CTRLM_VOICE_IARM_CALL_SESSION_BY_TEXT, &ctrlm_voice_ipc_iarm_thunder_t::start_session_with_transcription);
    if(rc != IARM_RESULT_SUCCESS) {
        LOG_ERROR("%s: Failed to register %d\n", __FUNCTION__, rc);
        ret = false;
    }

    return(ret);
}

bool ctrlm_voice_ipc_iarm_thunder_t::session_begin(const ctrlm_voice_ipc_event_session_begin_t &session_begin) {
    bool    ret   = false;
    json_t *event_data = json_object();
    int rc;

    // Assemble event data
    rc  = json_object_set_new_nocheck(event_data, JSON_REMOTE_ID, json_integer(session_begin.common.controller_id));
    rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_ID, json_string(session_begin.common.session_id_server.c_str()));
    rc |= json_object_set_new_nocheck(event_data, JSON_DEVICE_TYPE, json_string(voice_device_str(session_begin.common.device_type)));
    rc |= json_object_set_new_nocheck(event_data, JSON_KEYWORD_VERIFICATION, (session_begin.keyword_verification ? json_true() : json_false()));

    if(0 != rc) {
        LOG_ERROR("%s: Error creating JSON payload\n", __FUNCTION__);
        JSON_DEREFERENCE(event_data);
    } else {
        char *json_str = json_dumps(event_data, JSON_ENCODE_FLAGS);
        if(json_str) {
            //TODO: surface the event through IARM
            LOG_INFO("%s: %s\n", __FUNCTION__, json_str);
            ret = broadcast_event(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_SESSION_BEGIN, json_str);
            free(json_str);
        } else {
            LOG_ERROR("%s: Failed to encode JSON string\n", __FUNCTION__);
        }
    }

    JSON_DEREFERENCE(event_data);

    return(ret);
}

bool ctrlm_voice_ipc_iarm_thunder_t::stream_begin(const ctrlm_voice_ipc_event_stream_begin_t &stream_begin) {
    bool    ret   = false;
    json_t *event_data = json_object();
    int rc;

    // Assemble event data
    rc  = json_object_set_new_nocheck(event_data, JSON_REMOTE_ID, json_integer(stream_begin.common.controller_id));
    rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_ID, json_string(stream_begin.common.session_id_server.c_str()));

    if(0 != rc) {
        LOG_ERROR("%s: Error creating JSON payload\n", __FUNCTION__);
        JSON_DEREFERENCE(event_data);
    } else {
        char *json_str = json_dumps(event_data, JSON_ENCODE_FLAGS);
        if(json_str) {
            //TODO: surface the event through IARM
            LOG_INFO("%s: %s\n", __FUNCTION__, json_str);
            ret = broadcast_event(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_STREAM_BEGIN, json_str);
            free(json_str);
        } else {
            LOG_ERROR("%s: Failed to encode JSON string\n", __FUNCTION__);
        }
    }

    JSON_DEREFERENCE(event_data);

    return(ret);
}

bool ctrlm_voice_ipc_iarm_thunder_t::stream_end(const ctrlm_voice_ipc_event_stream_end_t &stream_end) {
    bool    ret   = false;
    json_t *event_data = json_object();
    int rc;

    // Assemble event data
    rc  = json_object_set_new_nocheck(event_data, JSON_REMOTE_ID, json_integer(stream_end.common.controller_id));
    rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_ID, json_string(stream_end.common.session_id_server.c_str()));
    rc |= json_object_set_new_nocheck(event_data, JSON_STREAM_END_REASON, json_integer(stream_end.reason));

    if(0 != rc) {
        LOG_ERROR("%s: Error creating JSON payload\n", __FUNCTION__);
        JSON_DEREFERENCE(event_data);
    } else {
        char *json_str = json_dumps(event_data, JSON_ENCODE_FLAGS);
        if(json_str) {
            //TODO: surface the event through IARM
            LOG_INFO("%s: %s\n", __FUNCTION__, json_str);
            ret = broadcast_event(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_STREAM_END, json_str);
            free(json_str);
        } else {
            LOG_ERROR("%s: Failed to encode JSON string\n", __FUNCTION__);
        }
    }

    JSON_DEREFERENCE(event_data);

    return(ret);
}

bool ctrlm_voice_ipc_iarm_thunder_t::session_end(const ctrlm_voice_ipc_event_session_end_t &session_end) {
    bool    ret   = false;
    json_t *event_data = json_object();
    json_t *event_result = json_object();
    int rc;

    // Assemble event data
    rc  = json_object_set_new_nocheck(event_data, JSON_REMOTE_ID, json_integer(session_end.common.controller_id));
    rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_ID, json_string(session_end.common.session_id_server.c_str()));
    switch(session_end.result) {
        case SESSION_END_SUCCESS: {
            int rc_success;
            rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_END_RESULT, json_string(JSON_SESSION_END_RESULT_SUCCESS));

            // Add Success Data to result object
            rc_success  = json_object_set_new_nocheck(event_result, JSON_SESSION_END_TRANSCRIPTION, json_string(session_end.transcription.c_str()));
            if(0 != rc_success) {
                LOG_ERROR("%s: Error creating success JSON subobject\n", __FUNCTION__);
                JSON_DEREFERENCE(event_result);
            } else {
                rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_END_RESULT_SUCCESS, event_result);
            }
            break;
        }
        case SESSION_END_FAILURE: {
            int rc_failure;
            rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_END_RESULT, json_string(JSON_SESSION_END_RESULT_ERROR));

            // Add Failure Data to result object
            rc_failure  = json_object_set_new_nocheck(event_result, JSON_SESSION_END_ERROR_REASON, json_integer(session_end.reason));
            rc_failure |= json_object_set_new_nocheck(event_result, JSON_SESSION_END_PROTOCOL_ERROR, json_integer(session_end.return_code_protocol));
            rc_failure |= json_object_set_new_nocheck(event_result, JSON_SESSION_END_PROTOCOL_LIBRARY_ERROR, json_integer(session_end.return_code_protocol_library));
            rc_failure |= json_object_set_new_nocheck(event_result, JSON_SESSION_END_SERVER_ERROR, json_integer(session_end.return_code_server));
            rc_failure |= json_object_set_new_nocheck(event_result, JSON_SESSION_END_SERVER_STR, json_string(session_end.return_code_server_str.c_str()));
            rc_failure |= json_object_set_new_nocheck(event_result, JSON_SESSION_END_INTERNAL_ERROR, json_integer(session_end.return_code_internal));
            if(0 != rc_failure) {
                LOG_ERROR("%s: Error creating failure JSON subobject\n", __FUNCTION__);
                JSON_DEREFERENCE(event_result);
            } else {
                rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_END_RESULT_ERROR, event_result);
            }
            break;
        }
        case SESSION_END_ABORT: {
            int rc_abort;
            rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_END_RESULT, json_string(JSON_SESSION_END_RESULT_ABORT));

            // Add Abort Data to result object
            rc_abort  = json_object_set_new_nocheck(event_result, JSON_SESSION_END_ABORT_REASON, json_integer(session_end.reason));
            if(0 != rc_abort) {
                LOG_ERROR("%s: Error creating abort JSON subobject\n", __FUNCTION__);
                JSON_DEREFERENCE(event_result);
            } else {
                rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_END_RESULT_ABORT, event_result);
            }
            break;
        }
        case SESSION_END_SHORT_UTTERANCE: {
            int rc_short;
            rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_END_RESULT, json_string(JSON_SESSION_END_RESULT_SHORT));

            // Add Short Utterance Data to result object
            rc_short  = json_object_set_new_nocheck(event_result, JSON_SESSION_END_SHORT_REASON, json_integer(session_end.reason));
            if(0 != rc_short) {
                LOG_ERROR("%s: Error creating short utterance JSON subobject\n", __FUNCTION__);
                JSON_DEREFERENCE(event_result);
            } else {
                rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_END_RESULT_SHORT, event_result);
            }
            break;
        }
    }
    if(session_end.stb_stats) {
        int stats_rc;
        json_t *event_stb_stats = json_object();

        stats_rc  = json_object_set_new_nocheck(event_stb_stats, JSON_SESSION_END_STB_STATS_TYPE, json_string(session_end.stb_stats->type.c_str()));
        stats_rc |= json_object_set_new_nocheck(event_stb_stats, JSON_SESSION_END_STB_STATS_FIRMWARE, json_string(session_end.stb_stats->firmware.c_str()));
        stats_rc |= json_object_set_new_nocheck(event_stb_stats, JSON_SESSION_END_STB_STATS_DEVICE_ID, json_string(session_end.stb_stats->device_id.c_str()));
        stats_rc |= json_object_set_new_nocheck(event_stb_stats, JSON_SESSION_END_STB_STATS_CTRLM_VERSION, json_string(session_end.stb_stats->ctrlm_version.c_str()));
        stats_rc |= json_object_set_new_nocheck(event_stb_stats, JSON_SESSION_END_STB_STATS_CONTROLLER_VERSION, json_string(session_end.stb_stats->controller_version.c_str()));
        stats_rc |= json_object_set_new_nocheck(event_stb_stats, JSON_SESSION_END_STB_STATS_CONTROLLER_TYPE, json_string(session_end.stb_stats->controller_type.c_str()));
        if(0 != stats_rc) {
            LOG_WARN("%s: STB Stats corrupted.. Removing..\n", __FUNCTION__);
            JSON_DEREFERENCE(event_stb_stats);
        } else {
            rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_END_STB_STATS, event_stb_stats);
        }
    }
    if(session_end.server_stats) {
        int stats_rc;
        json_t *event_server_stats = json_object();

        stats_rc  = json_object_set_new_nocheck(event_server_stats, JSON_SESSION_END_SERVER_STATS_IP, json_string(session_end.server_stats->server_ip.c_str()));
        stats_rc |= json_object_set_new_nocheck(event_server_stats, JSON_SESSION_END_SERVER_STATS_DNS_TIME, json_real(session_end.server_stats->dns_time));
        stats_rc |= json_object_set_new_nocheck(event_server_stats, JSON_SESSION_END_SERVER_STATS_CONNECT_TIME, json_real(session_end.server_stats->connect_time));
        if(0 != stats_rc) {
            LOG_WARN("%s: Server Stats corrupted.. Removing..\n", __FUNCTION__);
            JSON_DEREFERENCE(event_server_stats);
        } else {
            rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_END_SERVER_STATS, event_server_stats);
        }
    }

    if(0 != rc) {
        LOG_ERROR("%s: Error creating JSON payload\n", __FUNCTION__);
        JSON_DEREFERENCE(event_data);
    } else {
        char *json_str = json_dumps(event_data, JSON_ENCODE_FLAGS);
        if(json_str) {
            //TODO: surface the event through IARM
            LOG_INFO("%s: %s\n", __FUNCTION__, json_str);
            ret = broadcast_event(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_SESSION_END, json_str);
            free(json_str);
        } else {
            LOG_ERROR("%s: Failed to encode JSON string\n", __FUNCTION__);
        }
    }

    JSON_DEREFERENCE(event_data);

    return(ret);
}

bool ctrlm_voice_ipc_iarm_thunder_t::server_message(const char *message, unsigned long size) {
    bool    ret   = false;
    if(message) {
        LOG_INFO("%s: %ul : %s\n", __FUNCTION__, size, message);  //CID -160950 - Printargs
        ret = broadcast_event(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_SERVER_MESSAGE, message);
    }
    return(ret);
}

bool ctrlm_voice_ipc_iarm_thunder_t::keyword_verification(const ctrlm_voice_ipc_event_keyword_verification_t &keyword_verification) {
    bool    ret   = false;
    json_t *event_data = json_object();
    int rc;

    // Assemble event data
    rc  = json_object_set_new_nocheck(event_data, JSON_REMOTE_ID, json_integer(keyword_verification.common.controller_id));
    rc |= json_object_set_new_nocheck(event_data, JSON_SESSION_ID, json_string(keyword_verification.common.session_id_server.c_str()));
    rc |= json_object_set_new_nocheck(event_data, JSON_KEYWORD_VERIFIED, (keyword_verification.verified ? json_true() : json_false()));

    if(0 != rc) {
        LOG_ERROR("%s: Error creating JSON payload\n", __FUNCTION__);
        JSON_DEREFERENCE(event_data);
    } else {
        char *json_str = json_dumps(event_data, JSON_ENCODE_FLAGS);
        if(json_str) {
            //TODO: surface the event through IARM
            LOG_INFO("%s: %s\n", __FUNCTION__, json_str);
            ret = broadcast_event(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_KEYWORD_VERIFICATION, json_str);
            free(json_str);
        } else {
            LOG_ERROR("%s: Failed to encode JSON string\n", __FUNCTION__);
        }
    }

    JSON_DEREFERENCE(event_data);

    return(ret);
}

bool ctrlm_voice_ipc_iarm_thunder_t::session_statistics(const ctrlm_voice_ipc_event_session_statistics_t &session_stats) {
    // Not supported
    return(true);
}

void ctrlm_voice_ipc_iarm_thunder_t::deregister_ipc() const {
    LOG_INFO("%s: Thunder\n", __FUNCTION__);
}

IARM_Result_t ctrlm_voice_ipc_iarm_thunder_t::status(void *data) {
    IARM_Result_t ret        = IARM_RESULT_SUCCESS;
    bool result              = false;
    json_t *obj              = NULL;
    json_t *obj_capabilities = NULL;
    ctrlm_voice_iarm_call_json_t *call_data = (ctrlm_voice_iarm_call_json_t *)data;

    if(call_data == NULL || CTRLM_VOICE_IARM_BUS_API_REVISION != call_data->api_revision) {
        result = false;
        ret = IARM_RESULT_INVALID_PARAM;
    } else {
        ctrlm_voice_status_t status;
        result = ctrlm_get_voice_obj()->voice_status(&status);
        if(result) {
            obj = json_object();
            obj_capabilities = json_array();
            int rc = 0;

            for(int i = CTRLM_VOICE_DEVICE_PTT; i < CTRLM_VOICE_DEVICE_INVALID; i++) {
                json_t *temp = json_object();
                rc |= json_object_set_new_nocheck(temp, JSON_STATUS, json_string(voice_device_status_str(status.status[i])));
                rc |= json_object_set_new_nocheck(obj, voice_device_str((ctrlm_voice_device_t)i), temp);
            }

            rc |= json_object_set_new_nocheck(obj, JSON_URL_PTT, json_string(status.urlPtt.c_str()));
            rc |= json_object_set_new_nocheck(obj, JSON_URL_HF, json_string(status.urlHf.c_str()));
            rc |= json_object_set_new_nocheck(obj, JSON_WW_FEEDBACK, status.wwFeedback ? json_true() : json_false());
            rc |= json_object_set_new_nocheck(obj, JSON_PRV, status.prv_enabled ? json_true() : json_false());
            rc |= json_object_set_new_nocheck(obj, JSON_THUNDER_RESULT, json_true());

            if (status.capabilities.prv) {
               rc |= json_array_append_new(obj_capabilities, json_string("PRV"));
            }
            if (status.capabilities.wwFeedback) {
               rc |= json_array_append_new(obj_capabilities, json_string("WWFEEDBACK"));
            }
            rc |= json_object_set_new_nocheck(obj, JSON_CAPABILITIES, obj_capabilities);

            if(rc) {
                LOG_WARN("%s: JSON error..\n", __FUNCTION__);
            }
        }
    }

    if(result) {
        json_result(obj, call_data->result, sizeof(call_data->result));
    } else {
        json_result_bool(result, call_data->result, sizeof(call_data->result));
    }

    if(obj) {
        json_decref(obj);
        obj = NULL;
    }
    return(ret);
}

IARM_Result_t ctrlm_voice_ipc_iarm_thunder_t::configure_voice(void *data) {
    IARM_Result_t ret = IARM_RESULT_SUCCESS;
    bool result       = false;
    ctrlm_voice_iarm_call_json_t *call_data = (ctrlm_voice_iarm_call_json_t *)data;

    if(call_data == NULL || CTRLM_VOICE_IARM_BUS_API_REVISION != call_data->api_revision) {
        result = false;
        ret = IARM_RESULT_INVALID_PARAM;
    } else {
        json_t *obj = json_loads(call_data->payload, JSON_DECODE_FLAGS, NULL);
        if(obj) {
            ctrlm_voice_t *voice_obj = ctrlm_get_voice_obj();
            if(voice_obj) {
                result = voice_obj->voice_configure(obj, true);
            } else {
                LOG_ERROR("%s: Voice Object is NULL!\n", __FUNCTION__);
            }
            json_decref(obj);
        } else {
            LOG_ERROR("%s: Invalid JSON\n", __FUNCTION__);
        }
        json_result_bool(result, call_data->result, sizeof(call_data->result));
    }
    return(ret);
}

IARM_Result_t ctrlm_voice_ipc_iarm_thunder_t::set_voice_init(void *data) {
    IARM_Result_t ret = IARM_RESULT_SUCCESS;
    bool result       = false;
    ctrlm_voice_iarm_call_json_t *call_data = (ctrlm_voice_iarm_call_json_t *)data;

    if(call_data == NULL || CTRLM_VOICE_IARM_BUS_API_REVISION != call_data->api_revision) {
        result = false;
        ret = IARM_RESULT_INVALID_PARAM;
    } else {
        ctrlm_voice_t *voice_obj = ctrlm_get_voice_obj();
        if(voice_obj) {
            result = voice_obj->voice_init_set(call_data->payload);
        } else {
            LOG_ERROR("%s: Voice Object is NULL!\n", __FUNCTION__);
        }
        json_result_bool(result, call_data->result, sizeof(call_data->result));
    }

    return(ret);
}

IARM_Result_t ctrlm_voice_ipc_iarm_thunder_t::send_voice_message(void *data) {
    IARM_Result_t ret = IARM_RESULT_SUCCESS;
    bool result       = false;
    ctrlm_voice_iarm_call_json_t *call_data = (ctrlm_voice_iarm_call_json_t *)data;

    if(call_data == NULL || CTRLM_VOICE_IARM_BUS_API_REVISION != call_data->api_revision) {
        result = false;
        ret = IARM_RESULT_INVALID_PARAM;
    } else {
        ctrlm_voice_t *voice_obj = ctrlm_get_voice_obj();
        if(voice_obj) {
            result = voice_obj->voice_message(call_data->payload);
        } else {
            LOG_ERROR("%s: Voice Object is NULL!\n", __FUNCTION__);
        }
        json_result_bool(result, call_data->result, sizeof(call_data->result));
    }

    return(ret);
}

IARM_Result_t ctrlm_voice_ipc_iarm_thunder_t::start_session_with_transcription(void *data) {
    IARM_Result_t ret = IARM_RESULT_SUCCESS;
    bool result       = true;
    ctrlm_voice_iarm_call_json_t *call_data = (ctrlm_voice_iarm_call_json_t *)data;

    if(call_data == NULL || CTRLM_VOICE_IARM_BUS_API_REVISION != call_data->api_revision) {
        result = false;
        ret = IARM_RESULT_INVALID_PARAM;
    } else {
        ctrlm_voice_t *voice_obj = ctrlm_get_voice_obj();
        if(voice_obj) {
            LOG_INFO("%s: payload = <%s>\n", __FUNCTION__, call_data->payload);

            json_error_t error;
            json_t *obj = json_loads((const char *)call_data->payload, JSON_REJECT_DUPLICATES, &error);
            if(obj == NULL) {
                LOG_ERROR("%s: invalid json", __FUNCTION__);
                result = false;
            } else if(!json_is_object(obj)) {
                LOG_ERROR("%s: json object not found", __FUNCTION__);
                result = false;
            } else {
                json_t *obj_transcription = json_object_get(obj, "transcription");
                std::string str_transcription = "";
                if (obj_transcription != NULL) {
                    str_transcription = std::string(json_string_value(obj_transcription));
                }
                if (str_transcription.empty()) {
                    LOG_ERROR("%s: Empty transcription.\n", __FUNCTION__);
                    result = false;
                }
                
                ctrlm_voice_format_t format = CTRLM_VOICE_FORMAT_ADPCM;
                ctrlm_voice_device_t device = CTRLM_VOICE_DEVICE_PTT;
                json_t *obj_type = json_object_get(obj, "type");
                std::string str_type = "";
                if (obj_type != NULL) {
                    str_type = std::string(json_string_value(obj_type));
                    transform(str_type.begin(), str_type.end(), str_type.begin(), ::tolower);

                    if (str_type.compare("ptt") == 0) {
                        device = CTRLM_VOICE_DEVICE_PTT;
                    } else if (str_type.compare("ff") == 0) {
                        device = CTRLM_VOICE_DEVICE_FF;
                    } else if (str_type.compare("mic") == 0) {
                        device = CTRLM_VOICE_DEVICE_MICROPHONE;
                    } else {
                        LOG_ERROR("%s: Invalid device type parameter.\n", __FUNCTION__);
                        result = false;
                    }
                } else {
                    LOG_INFO("%s: Optional device type parameter not present, setting to PTT\n", __FUNCTION__);
                }
                if (true == result) {
                    ctrlm_voice_session_response_status_t voice_status = voice_obj->voice_session_req(
                            CTRLM_MAIN_NETWORK_ID_INVALID, CTRLM_MAIN_CONTROLLER_ID_INVALID, 
                            device, format, NULL, "APPLICATION", "0.0.0.0", "0.0.0.0", 0.0, 
                            false, NULL, NULL, NULL, false, (const char*)str_transcription.c_str() );
                    if (voice_status != VOICE_SESSION_RESPONSE_AVAILABLE && 
                        voice_status != VOICE_SESSION_RESPONSE_AVAILABLE_PAR_VOICE) {
                        LOG_ERROR("%s: Failed opening voice session in ctrlm_voice_t, error = <%d>\n", __FUNCTION__, voice_status);
                        result = false;
                    }
                }
            }
        } else {
            LOG_ERROR("%s: Voice Object is NULL!\n", __FUNCTION__);
            result = false;
        }
        json_result_bool(result, call_data->result, sizeof(call_data->result));
    }

    return(ret);
}

void ctrlm_voice_ipc_iarm_thunder_t::json_result_bool(bool result, char *result_str, size_t result_str_len) {
    if(result_str != NULL) {
        int rc      = 0;
        json_t *obj = json_object();

        rc = json_object_set_new_nocheck(obj, JSON_THUNDER_RESULT, (result ? json_true() : json_false()));
        if(rc) {
            LOG_ERROR("%s: failed to add success var to JSON\n", __FUNCTION__);
        } else {
            json_result(obj, result_str, result_str_len);
        }

        if(obj) {
            json_decref(obj);
            obj = NULL;
        }
    }
}

void ctrlm_voice_ipc_iarm_thunder_t::json_result(json_t *obj, char *result_str, size_t result_str_len) {
    if(obj != NULL && result_str != NULL) {
        char   *obj_str = NULL;
        errno_t safec_rc = memset_s(result_str, result_str_len * sizeof(char), 0, result_str_len * sizeof(char));
        ERR_CHK(safec_rc);

        obj_str = json_dumps(obj, JSON_COMPACT);
        if(obj_str) {
            if(strlen(obj_str) >= result_str_len) {
                LOG_ERROR("%s: result string buffer not big enough!\n", __FUNCTION__);
            }
            safec_rc = strcpy_s(result_str, result_str_len, obj_str);
            ERR_CHK(safec_rc);
            free(obj_str);
            obj_str = NULL;
        }
    }
}

const char *voice_device_str(ctrlm_voice_device_t device) {
    switch(device) {
        case CTRLM_VOICE_DEVICE_PTT: return("ptt");
        case CTRLM_VOICE_DEVICE_FF:  return("ff");
        case CTRLM_VOICE_DEVICE_MICROPHONE: return("mic");
        default: break;
    }
    return("invalid");
}

const char *voice_device_status_str(ctrlm_voice_device_status_t status) {
    switch(status) {
        case CTRLM_VOICE_DEVICE_STATUS_READY:         return("ready");
        case CTRLM_VOICE_DEVICE_STATUS_OPENED:        return("opened");
        case CTRLM_VOICE_DEVICE_STATUS_DISABLED:      return("disabled");
        case CTRLM_VOICE_DEVICE_STATUS_PRIVACY:       return("privacy");
        case CTRLM_VOICE_DEVICE_STATUS_NOT_SUPPORTED: return("not supported");
        default: break;
    }
    return("invalid");
}

bool broadcast_event(const char *bus_name, int event, const char *str) {
    bool ret = false;
    size_t size = sizeof(ctrlm_voice_iarm_event_json_t) + strlen(str) + 1;
    ctrlm_voice_iarm_event_json_t *data = (ctrlm_voice_iarm_event_json_t *)malloc(size);
    if(data) {
        IARM_Result_t result;

        //Can't be replaced with safeC version of this
        memset(data, 0, size);

        data->api_revision = CTRLM_VOICE_IARM_BUS_API_REVISION;
        //Can't be replaced with safeC version of this, as safeC string functions doesn't allow string size more than 4K
        sprintf(data->payload, "%s", str);
        result = IARM_Bus_BroadcastEvent(bus_name, event, data, size);
        if(IARM_RESULT_SUCCESS != result) {
            LOG_ERROR("%s: IARM Bus Error!\n", __FUNCTION__);
        } else {
            ret = true;
        }
        if(data) {
            free(data);
        }
    } else {
        LOG_ERROR("%s: Failed to allocate data for IARM event\n", __FUNCTION__);
    }
    return(ret);
}
