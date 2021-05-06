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
#ifndef __CTRLM_VOICE_H__
#define __CTRLM_VOICE_H__
#include <string>
#include <iostream>
#include <vector>
#include <uuid/uuid.h>
#include "include/ctrlm_ipc.h"
#include "include/ctrlm_ipc_voice.h"
#include "ctrlm.h"
#include "ctrlm_voice.h"
#include "ctrlm_xraudio_hal.h"
#include "ctrlm_config.h"
#include "jansson.h"
#include "xr_timestamp.h"
#include "ctrlm_voice_types.h"
#include "ctrlm_voice_ipc.h"
#include "xrsr.h"

#define VOICE_SESSION_REQ_DATA_LEN_MAX (33)

#define CTRLM_VOICE_AUDIO_SETTINGS_INITIALIZER    {(ctrlm_voice_audio_mode_t)JSON_INT_VALUE_VOICE_AUDIO_MODE, (ctrlm_voice_audio_timing_t)JSON_INT_VALUE_VOICE_AUDIO_TIMING, JSON_FLOAT_VALUE_VOICE_AUDIO_CONFIDENCE_THRESHOLD, (ctrlm_voice_audio_ducking_type_t)JSON_INT_VALUE_VOICE_AUDIO_DUCKING_TYPE, JSON_FLOAT_VALUE_VOICE_AUDIO_DUCKING_LEVEL}
typedef enum {
   CTRLM_VOICE_AUDIO_MODE_OFF             = 0,
   CTRLM_VOICE_AUDIO_MODE_MUTING          = 1,
   CTRLM_VOICE_AUDIO_MODE_DUCKING         = 2
} ctrlm_voice_audio_mode_t; // explicitly defined each value as they are passed through RFC

typedef enum {
   CTRLM_VOICE_AUDIO_TIMING_VSR              = 0,
   CTRLM_VOICE_AUDIO_TIMING_CLOUD            = 1,
   CTRLM_VOICE_AUDIO_TIMING_CONFIDENCE       = 2,
   CTRLM_VOICE_AUDIO_TIMING_DUAL_SENSITIVITY = 3,
} ctrlm_voice_audio_timing_t; // explicitly defined each value as they are passed through RFC

typedef double ctrlm_voice_audio_confidence_threshold_t;
typedef double ctrlm_voice_audio_ducking_level_t;

typedef enum {
   CTRLM_VOICE_AUDIO_DUCKING_TYPE_ABSOLUTE = 0,
   CTRLM_VOICE_AUDIO_DUCKING_TYPE_RELATIVE = 1,
} ctrlm_voice_audio_ducking_type_t;

typedef struct {
   ctrlm_voice_audio_mode_t                 mode;
   ctrlm_voice_audio_timing_t               timing;
   ctrlm_voice_audio_confidence_threshold_t confidence_threshold;
   ctrlm_voice_audio_ducking_type_t         ducking_type;
   ctrlm_voice_audio_ducking_level_t        ducking_level;
} ctrlm_voice_audio_settings_t;

typedef enum {
    CTRLM_VOICE_FORMAT_ADPCM     = VOICE_SESSION_AUDIO_FORMAT_ADPCM_16K,
    CTRLM_VOICE_FORMAT_ADPCM_SKY = VOICE_SESSION_AUDIO_FORMAT_ADPCM_SKY_16K,
    CTRLM_VOICE_FORMAT_PCM       = VOICE_SESSION_AUDIO_FORMAT_PCM_16K,
    CTRLM_VOICE_FORMAT_OPUS      = VOICE_SESSION_AUDIO_FORMAT_OPUS_16K,
    CTRLM_VOICE_FORMAT_INVALID   = VOICE_SESSION_AUDIO_FORMAT_INVALID
} ctrlm_voice_format_t;

typedef enum {
   VOICE_COMMAND_STATUS_PENDING    = 0,
   VOICE_COMMAND_STATUS_TIMEOUT    = 1,
   VOICE_COMMAND_STATUS_OFFLINE    = 2,
   VOICE_COMMAND_STATUS_SUCCESS    = 3,
   VOICE_COMMAND_STATUS_FAILURE    = 4,
   VOICE_COMMAND_STATUS_NO_CMDS    = 5,
   VOICE_COMMAND_STATUS_TV_AVR_CMD = 6,
   VOICE_COMMAND_STATUS_MIC_CMD    = 7,
   VOICE_COMMAND_STATUS_AUDIO_CMD  = 8
} ctrlm_voice_command_status_t;

typedef enum {
   VOICE_SESSION_RESPONSE_AVAILABLE                 = 0x00,
   VOICE_SESSION_RESPONSE_BUSY                      = 0x01,
   VOICE_SESSION_RESPONSE_SERVER_NOT_READY          = 0x02,
   VOICE_SESSION_RESPONSE_UNSUPPORTED_AUDIO_FORMAT  = 0x03,
   VOICE_SESSION_RESPONSE_FAILURE                   = 0x04,
   VOICE_SESSION_RESPONSE_AVAILABLE_SKIP_CHAN_CHECK = 0x05
} ctrlm_voice_session_response_status_t;

typedef enum {
    CTRLM_VOICE_STATE_SRC_READY     = 0x00,
    CTRLM_VOICE_STATE_SRC_STREAMING = 0x01,
    CTRLM_VOICE_STATE_SRC_WAITING   = 0x02,
    CTRLM_VOICE_STATE_SRC_INVALID   = 0x03
} ctrlm_voice_state_src_t;

typedef enum {
    CTRLM_VOICE_STATE_DST_READY      = 0x00,
    CTRLM_VOICE_STATE_DST_REQUESTED  = 0x01,
    CTRLM_VOICE_STATE_DST_OPENED     = 0x02,
    CTRLM_VOICE_STATE_DST_STREAMING  = 0x03,
    CTRLM_VOICE_STATE_DST_INVALID    = 0x04
} ctrlm_voice_state_dst_t;

typedef enum {
   CTRLM_VOICE_DEVICE_STATUS_READY,
   CTRLM_VOICE_DEVICE_STATUS_OPENED,
   CTRLM_VOICE_DEVICE_STATUS_DISABLED,
   CTRLM_VOICE_DEVICE_STATUS_PRIVACY,
   CTRLM_VOICE_DEVICE_STATUS_NOT_SUPPORTED,
   CTRLM_VOICE_DEVICE_STATUS_INVALID,
} ctrlm_voice_device_status_t;

typedef enum {
   CTRLM_VOICE_REMOTE_VOICE_END_MIC_KEY_RELEASE     =  1,
   CTRLM_VOICE_REMOTE_VOICE_END_EOS_DETECTION       =  2,
   CTRLM_VOICE_REMOTE_VOICE_END_SECONDARY_KEY_PRESS =  3,
   CTRLM_VOICE_REMOTE_VOICE_END_TIMEOUT_MAXIMUM     =  4,
   CTRLM_VOICE_REMOTE_VOICE_END_TIMEOUT_TARGET      =  5,
   CTRLM_VOICE_REMOTE_VOICE_END_OTHER_ERROR         =  6
} ctrlm_voice_remote_voice_end_reasons_t;

typedef enum {
   CTRLM_VOICE_TV_AVR_CMD_POWER_OFF   = 0,
   CTRLM_VOICE_TV_AVR_CMD_POWER_ON    = 1,
   CTRLM_VOICE_TV_AVR_CMD_VOLUME_UP   = 2,
   CTRLM_VOICE_TV_AVR_CMD_VOLUME_DOWN = 3,
   CTRLM_VOICE_TV_AVR_CMD_VOLUME_MUTE = 4,
} ctrlm_voice_tv_avr_cmd_t;

#define CTRLM_FAR_FIELD_BEAMS_MAX (4)

typedef struct {
   guint32 pre_keyword_sample_qty;
   guint32 keyword_sample_qty;
   guint16 doa;
   bool    standard_search_pt_triggered;
   uint8_t standard_search_pt;
   bool    high_search_pt_support;
   bool    high_search_pt_triggered;
   uint8_t high_search_pt;
   double  dynamic_gain;
   struct {
      bool     selected;
      bool     triggered;
      uint16_t angle; // 0-360
      double   confidence;
      bool     confidence_normalized;
      double   snr;
   } beams[CTRLM_FAR_FIELD_BEAMS_MAX];
   bool    push_to_talk;
} voice_session_req_stream_params;

typedef struct {
   ctrlm_controller_id_t                 controller_id;
   ctrlm_timestamp_t                     timestamp;
   voice_session_type_t                  type;
   voice_session_audio_format_t          audio_format;
   unsigned char                         data_len;
   unsigned char                         data[VOICE_SESSION_REQ_DATA_LEN_MAX];
   ctrlm_voice_session_response_status_t status;
   ctrlm_voice_session_abort_reason_t    reason;
} ctrlm_main_queue_msg_voice_session_request_t;

typedef struct {
   ctrlm_controller_id_t            controller_id;
   ctrlm_voice_session_end_reason_t reason;
   unsigned long                    stop_data;
   unsigned char                    utterance_too_short;
   int                              voice_data_pipe_write;
   unsigned long                    buffer_high_watermark;
   unsigned long                    session_packets_total;
   unsigned long                    session_packets_lost;
   unsigned long                    session_rf_channel;
   unsigned char                    session_link_quality;
   unsigned long                    session_id;
} ctrlm_main_queue_msg_voice_session_end_t;

typedef struct {
   ctrlm_controller_id_t            controller_id;
   char                             transcription[CTRLM_VOICE_SESSION_TEXT_MAX_LENGTH];
   unsigned char                    success;
} ctrlm_main_queue_msg_voice_session_result_t;

typedef struct {
   ctrlm_controller_id_t         controller_id;
   ctrlm_voice_command_status_t  status;
   union {
      struct {
         ctrlm_voice_tv_avr_cmd_t cmd;
         bool                     toggle_fallback;
         unsigned char            ir_repeats;
      } tv_avr;
   } data;
} ctrlm_main_queue_msg_voice_command_status_t;

// Event Callback Structures
typedef struct {
   uuid_t                       uuid;
   rdkx_timestamp_t             timestamp;
} ctrlm_voice_cb_header_t;

typedef struct {
    ctrlm_voice_cb_header_t      header;
    xrsr_src_t                   src;
    xrsr_session_configuration_t *configuration;
    ctrlm_voice_endpoint_t       *endpoint;
    bool                         keyword_verification;
} ctrlm_voice_session_begin_cb_t;

typedef struct {
    ctrlm_voice_cb_header_t  header;
    xrsr_src_t               src;
} ctrlm_voice_stream_begin_cb_t;

typedef struct {
    ctrlm_voice_cb_header_t  header;
    xrsr_stream_stats_t      stats;
} ctrlm_voice_stream_end_cb_t;

typedef struct {
    ctrlm_voice_cb_header_t  header;
    bool                     retry;
} ctrlm_voice_disconnected_cb_t;

typedef struct {
    ctrlm_voice_cb_header_t     header;
    bool                        success;
    xrsr_session_stats_t        stats;
} ctrlm_voice_session_end_cb_t;

// End Event Callback Structures

typedef struct {
   std::string                 server_url_vrex_src_ptt;
   std::string                 server_url_vrex_src_ff;
   bool                        sat_enabled;
   std::string                 aspect_ratio;
   std::string                 guide_language;
   std::string                 app_id_http;
   std::string                 app_id_ws;
   unsigned long               timeout_vrex_connect;
   unsigned long               timeout_vrex_session;
   unsigned long               timeout_stats;
   unsigned long               timeout_packet_initial;
   unsigned long               timeout_packet_subsequent;
   guchar                      bitrate_minimum;
   guint16                     time_threshold;
   bool                        utterance_save;
   unsigned long               utterance_file_qty_max;
   unsigned long               utterance_file_size_max;
   std::string                 utterance_path;
   unsigned long               utterance_duration_min;
   unsigned long               ffv_leading_samples;
   bool                        force_voice_settings;
   unsigned long               keyword_sensitivity;
   bool                        vrex_test_flag;
   std::string                 opus_encoder_params_str;
   guchar                      opus_encoder_params[CTRLM_RCU_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS];
   bool                        force_toggle_fallback;
} voice_session_prefss_t;

typedef struct {
   guint16 timeout_packet_initial;
   guint16 timeout_packet_subsequent;
   guchar  bitrate_minimum;
   guint16 time_threshold;
} voice_params_qos_t;

typedef struct {
   guchar data[CTRLM_RCU_RIB_ATTR_LEN_OPUS_ENCODING_PARAMS];
} voice_params_opus_encoder_t;

typedef struct {
   bool             available;
   bool             connect_attempt;
   bool             connect_success;
   bool             has_keyword;
   rdkx_timestamp_t ctrl_request;
   rdkx_timestamp_t ctrl_response;
   rdkx_timestamp_t ctrl_audio_rxd_first;
   rdkx_timestamp_t ctrl_audio_rxd_keyword;
   rdkx_timestamp_t ctrl_audio_rxd_final;
   rdkx_timestamp_t ctrl_stop;
   rdkx_timestamp_t ctrl_cmd_status_wr;
   rdkx_timestamp_t ctrl_cmd_status_rd;
   rdkx_timestamp_t srvr_request;
   rdkx_timestamp_t srvr_connect;
   rdkx_timestamp_t srvr_init_txd;
   rdkx_timestamp_t srvr_audio_txd_first;
   rdkx_timestamp_t srvr_audio_txd_keyword;
   rdkx_timestamp_t srvr_audio_txd_final;
   rdkx_timestamp_t srvr_rsp_keyword;
   rdkx_timestamp_t srvr_disconnect;
} ctrlm_voice_session_timing_t;

typedef struct {
   std::string                     controller_name;
   std::string                     controller_version_sw;
   std::string                     controller_version_hw;
   double                          controller_voltage;
   std::string                     stb_name;
   uint32_t                        ffv_leading_samples; //This was a long. We can't have that many samples and "if(stream_params->keyword_sample_begin > info.ffv_leading_samples)" evaluates to true if keyword_sample_begin is < 0 and ffv_leading_samples is long
   bool                            has_stream_params;
   voice_session_req_stream_params stream_params;
} ctrlm_voice_session_info_t;

typedef struct {
   std::string urlPtt;
   std::string urlHf;
   ctrlm_voice_device_status_t status[CTRLM_VOICE_DEVICE_INVALID];
} ctrlm_voice_status_t;

typedef void (*ctrlm_voice_session_rsp_confirm_t)(bool result, ctrlm_timestamp_t *timestamp, void *user_data);

class ctrlm_voice_t {
    public:

    // Application Interface
    ctrlm_voice_t();
    virtual ~ctrlm_voice_t();

    ctrlm_voice_session_response_status_t voice_session_req(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_voice_device_t device_type, ctrlm_voice_format_t format, voice_session_req_stream_params *stream_params, const char *controller_name, const char *sw_version, const char *hw_version, double voltage, bool command_status=false, ctrlm_timestamp_t *timestamp=NULL, ctrlm_voice_session_rsp_confirm_t *cb_confirm=NULL, void **cb_confirm_param=NULL, bool use_external_data_pipe=false);
    void                                  voice_session_rsp_confirm(bool result, ctrlm_timestamp_t *timestamp);
    bool                                  voice_session_data(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, const char *buffer, long unsigned int length, ctrlm_timestamp_t *timestamp=NULL);
    bool                                  voice_session_data(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, int fd);
    void                                  voice_session_data_post_processing(int bytes_sent, const char *action, ctrlm_timestamp_t *timestamp);
    void                                  voice_session_end(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, ctrlm_voice_session_end_reason_t reason, ctrlm_timestamp_t *timestamp=NULL);
    void                                  voice_session_term();
    void                                  voice_session_info(ctrlm_voice_session_info_t *data);
    bool                                  voice_session_id_is_current(uuid_t uuid);

    virtual void                          voice_stb_data_stb_sw_version_set(std::string &sw_version);
    std::string                           voice_stb_data_stb_sw_version_get() const;
    virtual void                          voice_stb_data_stb_name_set(std::string &stb_name);
    std::string                           voice_stb_data_stb_name_get() const;
    virtual void                          voice_stb_data_account_number_set(std::string &account_number);
    std::string                           voice_stb_data_account_number_get() const;
    virtual void                          voice_stb_data_receiver_id_set(std::string &receiver_id);
    std::string                           voice_stb_data_receiver_id_get() const;
    virtual void                          voice_stb_data_device_id_set(std::string &device_id);
    std::string                           voice_stb_data_device_id_get() const;
    virtual void                          voice_stb_data_partner_id_set(std::string &partner_id);
    std::string                           voice_stb_data_partner_id_get() const;
    virtual void                          voice_stb_data_experience_set(std::string &experience);
    std::string                           voice_stb_data_experience_get() const;
    std::string                           voice_stb_data_app_id_http_get() const;
    std::string                           voice_stb_data_app_id_ws_get() const;
    virtual void                          voice_stb_data_guide_language_set(const char *language);
    std::string                           voice_stb_data_guide_language_get() const;
    virtual void                          voice_stb_data_sat_set(std::string &sat_token);
    std::string                           voice_stb_data_sat_get() const;
    bool                                  voice_stb_data_test_get() const;

    virtual bool                          voice_configure_config_file_json(json_t *obj_voice, json_t *json_obj_vsdk);
    virtual bool                          voice_configure(ctrlm_voice_iarm_call_settings_t *settings, bool db_write);
    virtual bool                          voice_configure(json_t *settings, bool db_write);
    virtual bool                          voice_status(ctrlm_voice_status_t *status);
    virtual bool                          voice_init_set(const char *init, bool db_write = true);
    virtual bool                          voice_message(const char *msg);
    virtual bool                          server_message(const char *message, unsigned long length);
    void                                  voice_params_qos_get(voice_params_qos_t *params);
    void                                  voice_params_opus_encoder_get(voice_params_opus_encoder_t *params);
    virtual void                          process_xconf(json_t **json_obj_vsdk);
    virtual void                          query_strings_updated();


    bool                                  voice_device_streaming(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id);
    void                                  voice_controller_command_status_read(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id);
    void                                  voice_status_set();

    // Helper semaphores for synchronization
    sem_t                    vsr_semaphore;

protected:
    void                                  voice_session_timeout();
    void                                  voice_session_controller_command_status_read_timeout();
    void                                  voice_session_stats_clear();
    void                                  voice_session_stats_print();
    
    ctrlm_voice_state_src_t               voice_state_src_get() const;
    ctrlm_voice_state_dst_t               voice_state_dst_get() const;

    void                                  voice_session_info_reset();

    void                                  voice_set_ipc(ctrlm_voice_ipc_t *ipc);
    // End Application Interface

    // Static Callbacks
    static int ctrlm_voice_packet_timeout(void *data);
    static int ctrlm_voice_controller_command_status_read_timeout(void *data);
    // End Static Callbacks

    // Event Interface
public:
    virtual void                  voice_session_begin_callback(ctrlm_voice_session_begin_cb_t *session_begin);
    virtual void                  voice_stream_begin_callback(ctrlm_voice_stream_begin_cb_t *stream_begin);
    virtual void                  voice_stream_end_callback(ctrlm_voice_stream_end_cb_t *stream_end);
    virtual void                  voice_session_end_callback(ctrlm_voice_session_end_cb_t *sesison_end);
    virtual void                  voice_server_message_callback(const char *msg, unsigned long length);
    virtual void                  voice_server_connected_callback(ctrlm_voice_cb_header_t *connected);
    virtual void                  voice_server_disconnected_callback(ctrlm_voice_disconnected_cb_t *disconnected);
    virtual void                  voice_server_sent_init_callback(ctrlm_voice_cb_header_t *init);
    virtual void                  voice_stream_kwd_callback(ctrlm_voice_cb_header_t *kwd);
    virtual void                  voice_action_tv_mute_callback(bool mute);
    virtual void                  voice_action_tv_power_callback(bool power, bool toggle);
    virtual void                  voice_action_tv_volume_callback(bool up, uint32_t repeat_count);
    virtual void                  voice_action_keyword_verification_callback(bool success, rdkx_timestamp_t timestamp);
    virtual void                  voice_server_return_code_callback(long ret_code);
    virtual void                  voice_session_transcription_callback(const char *transcription);
    virtual void                  voice_power_state_change(ctrlm_power_state_t power_state);
    // End Event Interface

    protected:
    virtual void          voice_sdk_open(json_t *json_obj_vsdk);
    virtual void          voice_sdk_update_routes() = 0;
    virtual bool          voice_session_has_stb_data();
    unsigned long         voice_session_id_next();
    void                  voice_session_notify_abort(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id, unsigned long session_id, ctrlm_voice_session_abort_reason_t reason);

    void                  voice_params_opus_encoder_default(void);
    void                  voice_params_opus_encoder_validate(void);
    void                  voice_params_opus_samples_per_packet_set(void);
    bool                  voice_params_hex_str_to_bytes(std::string hex_string, guchar *data, guint32 length);

    // This function is called to change voice device statuses. Must acquire the device_status_semaphore before calling.
    void                  voice_device_status_change(ctrlm_voice_device_t device, ctrlm_voice_device_status_t status, bool *update_routes = NULL);

    protected:
    // STB Data
    std::string              software_version;
    std::string              stb_name;
    std::string              account_number;
    std::string              device_id;
    std::string              receiver_id;
    std::string              partner_id;
    std::string              experience;
    std::string              sat_token;
    bool                     sat_token_required;
    voice_session_prefss_t   prefs;
    // End STB Data

    // Session Data
    ctrlm_network_id_t       network_id;
    ctrlm_network_type_t     network_type;
    ctrlm_controller_id_t    controller_id;
    std::string              controller_name;
    std::string              controller_version_sw;
    std::string              controller_version_hw;
    double                   controller_voltage;
    bool                     controller_command_status;
    ctrlm_voice_device_t     voice_device;
    ctrlm_voice_format_t     format;
    long                     server_ret_code;
    bool                     has_stream_params;
    voice_session_req_stream_params stream_params;
    // End Session Data

    protected:
    sem_t                                             device_status_semaphore;
    ctrlm_voice_device_status_t                       device_status[CTRLM_VOICE_DEVICE_INVALID];
    std::vector<ctrlm_voice_endpoint_t *>             endpoints;
    ctrlm_voice_endpoint_t *                          endpoint_current;
    std::vector<std::pair<std::string, std::string> > query_strs_ptt;

    private:
    ctrlm_voice_state_src_t  state_src;
    ctrlm_voice_state_dst_t  state_dst;
    bool                     xrsr_opened;
    ctrlm_voice_ipc_t       *voice_ipc;

    // Current Session Data
    ctrlm_hal_input_object_t hal_input_object;
    int                      audio_pipe[2];
    unsigned long            audio_sent_bytes;
    unsigned long            audio_sent_samples;
    unsigned long            opus_samples_per_packet;
    unsigned long            session_id;
    std::string              trx;
    uuid_t                   uuid;
    std::string              message;
    std::string              transcription;
    ctrlm_main_queue_msg_voice_command_status_t status;
    uint8_t                  last_cmd_id; // Needed for ADPCM over RF4CE, since duplicate packets are possible
    uint32_t                 packets_processed;
    uint32_t                 packets_lost;
    double                   confidence;
    bool                     dual_sensitivity_immediate;
    bool                     keyword_verified;
    ctrlm_power_state_t      power_state;

    bool                     session_active_server;
    bool                     session_active_controller;

    ctrlm_voice_session_timing_t session_timing;

    ctrlm_voice_audio_mode_t                 audio_mode;
    ctrlm_voice_audio_timing_t               audio_timing;
    ctrlm_voice_audio_ducking_type_t         audio_ducking_type;
    ctrlm_voice_audio_ducking_level_t        audio_ducking_level;
    ctrlm_voice_audio_confidence_threshold_t audio_confidence_threshold;

    ctrlm_voice_ipc_event_common_t ipc_common_data;

    // End Current Session Data
    // Timeout tags
    unsigned int             timeout_packet_tag;
    unsigned int             timeout_ctrl_cmd_status_read;
    // End Timeout tags

    ctrlm_voice_session_end_reason_t end_reason;

   bool                 is_voice_assistant(ctrlm_voice_device_t device);
   bool                 controller_supports_qos(void);
   void                 set_audio_mode(ctrlm_voice_audio_settings_t *settings);
   void                 audio_state_set(bool session);
   bool                 privacy_mode(void);
   void                 keyword_power_state_change(bool success);

};

// Helper Functions
const char *ctrlm_voice_format_str(ctrlm_voice_format_t format);
const char *ctrlm_voice_state_src_str(ctrlm_voice_state_src_t state);
const char *ctrlm_voice_state_dst_str(ctrlm_voice_state_dst_t state);
const char *ctrlm_voice_device_str(ctrlm_voice_device_t device);
const char *ctrlm_voice_device_status_str(ctrlm_voice_device_status_t status);
const char *ctrlm_voice_command_status_str(ctrlm_voice_command_status_t status);
const char *ctrlm_voice_command_status_tv_avr_str(ctrlm_voice_tv_avr_cmd_t cmd);
const char *ctrlm_voice_audio_mode_str(ctrlm_voice_audio_mode_t mode);
const char *ctrlm_voice_audio_timing_str(ctrlm_voice_audio_timing_t timing);
ctrlm_hal_input_device_t voice_device_to_hal(ctrlm_voice_device_t device);
xraudio_encoding_t voice_format_to_xraudio(ctrlm_voice_format_t format);
ctrlm_voice_device_t xrsr_to_voice_device(xrsr_src_t device);
void ctrlm_voice_xrsr_session_capture_start(ctrlm_main_queue_msg_audio_capture_start_t *capture_start);
void ctrlm_voice_xrsr_session_capture_stop(void);

#endif
