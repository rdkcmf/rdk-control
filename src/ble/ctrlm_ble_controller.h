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
#ifndef _CTRLM_BLE_CONTROLLER_H_
#define _CTRLM_BLE_CONTROLLER_H_

//////////////////////////////////////////
// Includes
//////////////////////////////////////////

#include <string>
#include "ctrlm_hal_ble.h"
#include "../ctrlm_controller.h"
#include "../ctrlm_ipc_rcu.h"

//////////////////////////////////////////
// defines
//////////////////////////////////////////

#define CTRLM_BLE_LEN_VOICE_METRICS                      (44)

#define BROADCAST_PRODUCT_NAME_IR_DEVICE                 ("IR Device")

//////////////////////////////////////////
// Enumerations
//////////////////////////////////////////

typedef enum {
   BLE_CONTROLLER_TYPE_PR1,
   BLE_CONTROLLER_TYPE_LC103,
   BLE_CONTROLLER_TYPE_EC302,
   BLE_CONTROLLER_TYPE_IR,    // add new remote types before this
   BLE_CONTROLLER_TYPE_UNKNOWN,
   BLE_CONTROLLER_TYPE_INVALID
} ctrlm_ble_controller_type_t;

typedef enum {
    CTRLM_BLE_RESULT_VALIDATION_SUCCESS          = 0x00,
    CTRLM_BLE_RESULT_VALIDATION_PENDING          = 0x01,
    CTRLM_BLE_RESULT_VALIDATION_FAILURE          = 0x02
} ctrlm_ble_result_validation_t;


//////////////////////////////////////////
// Class ctrlm_obj_controller_ble_t
//////////////////////////////////////////

class ctrlm_obj_network_ble_t;

class ctrlm_obj_controller_ble_t : public ctrlm_obj_controller_t {

public:
   ctrlm_obj_controller_ble_t(ctrlm_controller_id_t controller_id, ctrlm_obj_network_ble_t &network, ctrlm_ble_result_validation_t validation_result);
   ctrlm_obj_controller_ble_t();

   void db_create();
   void db_destroy();
   void db_load();
   void db_store();

   void                          getStatus(ctrlm_controller_status_t *status);

   void                          validation_result_set(ctrlm_ble_result_validation_t validation_result);
   ctrlm_ble_result_validation_t validation_result_get() const;

   ctrlm_ble_controller_type_t   getControllerType(void);
   void                          setControllerType(std::string productName);
   bool                          getOTAProductName(std::string &name);
   void                          setName(std::string controller_name);
   std::string                   getName( void );
   void                          setBatteryPercent(int percent);
   int                           getBatteryPercent();
   void                          setConnected(bool connected);
   bool                          getConnected( void );
   void                          setMacAddress(unsigned long long ieee_address);
   unsigned long long            getMacAddress(void);
   void                          setDeviceID(int device_id);
   int                           getDeviceID(void);
   void                          setSerialNumber(std::string sn);
   std::string                   getSerialNumber();
   void                          setManufacturer(std::string controller_manufacturer);
   std::string                   getManufacturer();
   void                          setModel(std::string controller_model);
   std::string                   getModel();
   void                          setFwRevision(std::string rev);
   std::string                   getFwRevision();
   void                          setHwRevision(std::string rev);
   ctrlm_version_t               getHwRevision();
   void                          setSwRevision(std::string rev);
   ctrlm_version_t               getSwRevision();

   guchar                        property_write_irdb_entry_id_name_tv(guchar *data, guchar length);
   guchar                        property_write_irdb_entry_id_name_avr(guchar *data, guchar length);
   guchar                        property_read_irdb_entry_id_name_tv(guchar *data, guchar length);
   guchar                        property_read_irdb_entry_id_name_avr(guchar *data, guchar length);

   bool                          swUpgradeRequired(ctrlm_version_t newVersion, bool force);

   void                          setUpgradeInProgress(bool upgrading);
   bool                          getUpgradeInProgress(void);
   void                          setUpgradeAttempted(bool upgrade_attempted);
   bool                          getUpgradeAttempted(void);

   virtual void                  ota_failure_cnt_incr();
   virtual void                  ota_clear_all_failure_counters();
   virtual void                  ota_failure_type_z_cnt_set(uint8_t ota_failures);
   virtual uint8_t               ota_failure_type_z_cnt_get(void) const;
   virtual bool                  is_controller_type_z(void) const;

   void                          setIrCode(int code);
   void                          setAudioGainLevel(guint8 gain);
   void                          setAudioCodecs(guint32 value);
   void                          setAudioStreaming(bool streaming);
   void                          setTouchMode(unsigned int param);
   void                          setTouchModeSettable(bool param);

   time_t                        getLastKeyTime();
   guint16                       getLastKeyCode();
   void                          process_event_key(ctrlm_key_status_t key_status, guint16 key_code);

   void                          setLastWakeupKey(guint16 code);
   guint16                       getLastWakeupKey();

   void                          setWakeupConfig(uint8_t config);
   ctrlm_rcu_wakeup_config_t     getWakeupConfig();
   void                          setWakeupCustomList(int *list, int size);
   std::vector<uint16_t>         getWakeupCustomList();
   std::string                   wakeupCustomListToString();

   void                          setUpgradePaused(bool paused);
   bool                          getUpgradePaused();

   void                          update_voice_metrics(bool is_short_utterance, guint32 voice_packets_sent, guint32 voice_packets_lost);
   void                          property_write_voice_metrics(void);
   guchar                        property_parse_voice_metrics(guchar *data, guchar length);
   bool                          handle_day_change();

   void print_status();

private:
   ctrlm_obj_network_ble_t          *obj_network_ble_;
   ctrlm_ble_controller_type_t      controller_type_;
   std::string                      product_name_;
   std::string                      serial_number_;
   std::string                      mac_address_str_;
   unsigned long long               ieee_address_;
   int                              device_id_;
   std::string                      manufacturer_;
   std::string                      model_;
   std::string                      fw_revision_;
   ctrlm_version_t                  hw_revision_;
   ctrlm_version_t                  sw_revision_;

   bool                             upgrade_in_progress_;
   bool                             upgrade_attempted_;
   bool                             upgrade_paused_;

   int                              ir_code_;

   guint8                           audio_gain_level_;
   guint32                          audio_codecs_;
   bool                             audio_streaming_;

   unsigned int                     touch_mode_;
   bool                             touch_mode_settable_;

   bool                             stored_in_db_;
   bool                             connected_;
   ctrlm_ble_result_validation_t    validation_result_;

   time_t                           time_binding_;
   time_t                           last_key_time_;
   time_t                           last_key_time_flush_;
   ctrlm_key_status_t               last_key_status_;
   guint16                          last_key_code_;
   guint16                          last_wakeup_key_code_;

   ctrlm_rcu_wakeup_config_t        wakeup_config_;
   std::vector<uint16_t>            wakeup_custom_list_;

   int                              battery_percent_;

   guint32                          voice_cmd_count_today_;
   guint32                          voice_cmd_count_yesterday_;
   guint32                          voice_cmd_short_today_;
   guint32                          voice_cmd_short_yesterday_;
   guint32                          today_;
   guint32                          voice_packets_sent_today_;
   guint32                          voice_packets_sent_yesterday_;
   guint32                          voice_packets_lost_today_;
   guint32                          voice_packets_lost_yesterday_;
   guint32                          utterances_exceeding_packet_loss_threshold_today_;
   guint32                          utterances_exceeding_packet_loss_threshold_yesterday_;


   void last_key_time_update();
};

// End Class ctrlm_obj_controller_ble_t

#endif
