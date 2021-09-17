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
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/sysinfo.h>
#include <poll.h>
#include <glib.h>
#include <string.h>
#include <semaphore.h>
#include <memory>
#include <algorithm>
#include <fstream>
#include <secure_wrapper.h>
#include <rdkversion.h>
#include "jansson.h"
#include "libIBus.h"
#include "irMgr.h"
#include "plat_ir.h"
#include "sysMgr.h"
#ifdef BREAKPAD_SUPPORT
#include "client/linux/handler/exception_handler.h"
#endif
#include "comcastIrKeyCodes.h"
#include "ctrlm_version_build.h"
#include "ctrlm_hal_rf4ce.h"
#include "ctrlm_hal_ble.h"
#include "ctrlm_hal_ip.h"
#include "ctrlm.h"
#include "ctrlm_utils.h"
#include "ctrlm_database.h"
#include "ctrlm_rcu.h"
#include "ctrlm_validation.h"
#include "ctrlm_recovery.h"
#ifdef AUTH_ENABLED
#include "ctrlm_auth.h"
#ifdef AUTH_THUNDER
#include "ctrlm_auth_thunder.h"
#else
#include "ctrlm_auth_legacy.h"
#endif
#endif
#include "ctrlm_rfc.h"
#include "ctrlm_config.h"
#include "ctrlm_tr181.h"
#include "rf4ce/ctrlm_rf4ce_network.h"
#include "ctrlm_device_update.h"
#include "ctrlm_vendor_network_factory.h"
#include "dsMgr.h"
#include "dsRpc.h"
#include "dsDisplay.h"
#ifdef SYSTEMD_NOTIFY
#include <systemd/sd-daemon.h>
#endif
#include "xr_voice_sdk.h"
#include "ctrlm_voice_obj.h"
#include "ctrlm_voice_obj_generic.h"
#include "ctrlm_voice_endpoint.h"
#include "ctrlm_irdb_factory.h"
#ifdef FDC_ENABLED
#include "xr_fdc.h"
#endif
#include<features.h>
#ifdef MEMORY_LOCK
#include "clnl.h"
#endif

using namespace std;

#ifndef CTRLM_VERSION
#define CTRLM_VERSION "1.0"
#endif

#define CTRLM_THREAD_NAME_MAIN          "Ctrlm Main"
#define CTRLM_THREAD_NAME_DATABASE      "Ctrlm Database"
#define CTRLM_THREAD_NAME_DEVICE_UPDATE "Ctrlm Device Update"
#define CTRLM_THREAD_NAME_VOICE_SDK     "Voice SDK"
#define CTRLM_THREAD_NAME_RF4CE         "RF4CE"
#define CTRLM_THREAD_NAME_BLE           "BLE"

#define CTRLM_DEFAULT_DEVICE_MAC_INTERFACE "eth0"

#define CTRLM_RESTART_DELAY_SHORT    "0"
#define CTRLM_RESTART_UPDATE_TIMEOUT (5000)

#define CTRLM_RF4CE_LEN_IR_REMOTE_USAGE 14
#define CTRLM_RF4CE_LEN_LAST_KEY_INFO   sizeof(ctrlm_last_key_info)
#define CTRLM_RF4CE_LEN_SHUTDOWN_TIME   4
#define CTRLM_RF4CE_LEN_PAIRING_METRICS sizeof(ctrlm_pairing_metrics_t)

#define CTRLM_MAIN_QUEUE_REPEAT_DELAY   (5000)

#define NETWORK_ID_BASE_RF4CE   1
#define NETWORK_ID_BASE_IP      11
#define NETWORK_ID_BASE_BLE     21
#define NETWORK_ID_BASE_CUSTOM  41

#define CTRLM_MAIN_FIRST_BOOT_TIME_MAX (180) // maximum amount of uptime allowed (in seconds) for declaring "first boot"

typedef void (*ctrlm_queue_push_t)(gpointer);
typedef void (*ctrlm_monitor_poll)(void *data);

typedef struct {
   const char *                    name;
   ctrlm_queue_push_t              queue_push;
   ctrlm_obj_network_t *           obj_network;
   ctrlm_monitor_poll              function;
   ctrlm_thread_monitor_response_t response;
} ctrlm_thread_monitor_t;

typedef struct
{
   bool has_ir_xr2;
   bool has_ir_xr5;
   bool has_ir_xr11;
   bool has_ir_xr15;
   bool has_ir_remote;
} ctrlm_ir_remote_usage;

typedef struct {
   int                         controller_id;
   guchar                      source_type;
   guint32                     source_key_code;
   long long                   timestamp;
   ctrlm_ir_remote_type        last_ir_remote_type;
   bool                        is_screen_bind_mode;
   ctrlm_remote_keypad_config  remote_keypad_config;
   char                        source_name[CTRLM_RCU_RIB_ATTR_LEN_PRODUCT_NAME];
} ctrlm_last_key_info;

typedef struct {
   gboolean                            active;
   ctrlm_pairing_modes_t               pairing_mode;
   ctrlm_pairing_restrict_by_remote_t  restrict_by_remote;
   ctrlm_bind_status_t                 bind_status;
} ctrlm_pairing_window;

typedef struct {
   unsigned long            num_screenbind_failures;
   unsigned long            last_screenbind_error_timestamp;
   ctrlm_bind_status_t      last_screenbind_error_code;
   char                     last_screenbind_remote_type[CTRLM_MAIN_SOURCE_NAME_MAX_LENGTH];
   unsigned long            num_non_screenbind_failures;
   unsigned long            last_non_screenbind_error_timestamp;
   ctrlm_bind_status_t      last_non_screenbind_error_code;
   unsigned char            last_non_screenbind_error_binding_type;
   char                     last_non_screenbind_remote_type[CTRLM_MAIN_SOURCE_NAME_MAX_LENGTH];
} ctrlm_pairing_metrics_t;

static const char *key_source_names[sizeof(IARM_Bus_IRMgr_KeySrc_t)] = 
{
   "FP",
   "IR",
   "RF"
};

typedef struct {
   GThread *                          main_thread;
   GMainLoop *                        main_loop;
   sem_t                              semaphore;
   sem_t                              ctrlm_utils_sem;
   GAsyncQueue *                      queue;
   string                             stb_name;
   string                             receiver_id;
   string                             device_id;
   string                             service_account_id;
   string                             partner_id;
   string                             device_mac;
   string                             experience;
   string                             service_access_token;
   time_t                             service_access_token_expiration;
   guint                              service_access_token_expiration_tag;
   sem_t                              service_access_token_semaphore;
   string                             image_name;
   string                             image_branch;
   string                             image_version;
   string                             image_build_time;
   string                             db_path;
   string                             minidump_path;
   gboolean                           has_receiver_id;
   gboolean                           has_device_id;
   gboolean                           has_service_account_id;
   gboolean                           has_partner_id;
   gboolean                           has_experience;
   gboolean                           has_service_access_token;
   gboolean                           sat_enabled;
   gboolean                           production_build;
   guint                              thread_monitor_timeout_val;
   guint                              thread_monitor_timeout_tag;
   guint                              thread_monitor_index;
   bool                               thread_monitor_active;
   bool                               thread_monitor_minidump;
   string                             server_url_authservice;
   guint                              authservice_poll_val;
   guint                              authservice_poll_tag;
   guint                              authservice_fast_poll_val;
   guint                              authservice_fast_retries;
   guint                              authservice_fast_retries_max;
   guint                              keycode_logging_poll_val;
   guint                              keycode_logging_poll_tag;
   guint                              keycode_logging_retries;
   guint                              keycode_logging_max_retries;
   guint                              bound_controller_qty;
   gboolean                           recently_booted;
   guint                              recently_booted_timeout_val;
   guint                              recently_booted_timeout_tag;
   gboolean                           line_of_sight;
   guint                              line_of_sight_timeout_val;
   guint                              line_of_sight_timeout_tag;
   gboolean                           autobind;
   guint                              autobind_timeout_val;
   guint                              autobind_timeout_tag;
   gboolean                           binding_button;
   guint                              binding_button_timeout_val;
   guint                              binding_button_timeout_tag;
   gboolean                           binding_screen_active;
   guint                              screen_bind_timeout_val;
   guint                              screen_bind_timeout_tag;
   gboolean                           one_touch_autobind_active;
   guint                              one_touch_autobind_timeout_val;
   guint                              one_touch_autobind_timeout_tag;
   ctrlm_pairing_window               pairing_window;
   bool                               mask_key_codes_json;
   bool                               mask_key_codes_iarm;
   bool                               mask_pii;
   guint                              crash_recovery_threshold;
   gboolean                           successful_init;
   ctrlm_ir_remote_usage              ir_remote_usage_today;
   ctrlm_ir_remote_usage              ir_remote_usage_yesterday;
   guint32                            today;
   ctrlm_pairing_metrics_t            pairing_metrics;
   char                               discovery_remote_type[CTRLM_HAL_RF4CE_USER_STRING_SIZE];
   time_t                             shutdown_time;
   ctrlm_last_key_info                last_key_info;
   gboolean                           loading_db;
   map<ctrlm_controller_id_t, bool>   precomission_table;
   map<ctrlm_network_id_t, ctrlm_obj_network_t *> networks;     // Map to hold the Networks that will be used by Control Manager
   map<ctrlm_network_id_t, ctrlm_network_type_t>  network_type; // Map to hold the Network types that will be used by Control Manager
   vector<ctrlm_thread_monitor_t>     monitor_threads;
   int                                return_code;
   ctrlm_voice_t                     *voice_session;
   ctrlm_irdb_t                      *irdb;
   ctrlm_cs_values_t                  cs_values;
#ifdef AUTH_ENABLED
   ctrlm_auth_t                      *authservice;
#endif
   ctrlm_power_state_t                power_state;
   gboolean                           auto_ack;
   gboolean                           local_conf;
} ctrlm_global_t;

static ctrlm_global_t g_ctrlm;

// Prototypes
#ifdef AUTH_ENABLED
static gboolean ctrlm_has_authservice_data(void);
static gboolean ctrlm_load_authservice_data(void);
#ifdef AUTH_RECEIVER_ID
static gboolean ctrlm_load_receiver_id(void);
static void     ctrlm_main_has_receiver_id_set(gboolean has_id);
#endif
#ifdef AUTH_DEVICE_ID
static gboolean ctrlm_load_device_id(void);
static void     ctrlm_main_has_device_id_set(gboolean has_id);
#endif
#ifdef AUTH_ACCOUNT_ID
static gboolean ctrlm_load_service_account_id(const char *account_id);
static void     ctrlm_main_has_service_account_id_set(gboolean has_id);
#endif
#ifdef AUTH_PARTNER_ID
static gboolean ctrlm_load_partner_id(void);
static void     ctrlm_main_has_partner_id_set(gboolean has_id);
#endif
#ifdef AUTH_EXPERIENCE
static gboolean ctrlm_load_experience(void);
static void     ctrlm_main_has_experience_set(gboolean has_experience);
#endif
#ifdef AUTH_SAT_TOKEN
static gboolean ctrlm_load_service_access_token(void);
static void     ctrlm_main_has_service_access_token_set(gboolean has_token);
#endif
#endif
static gboolean ctrlm_load_version(void);
static gboolean ctrlm_load_device_mac(void);
static gboolean ctrlm_load_config(json_t **json_obj_root, json_t **json_obj_net_rf4ce, json_t **json_obj_voice, json_t **json_obj_device_update, json_t **json_obj_validation, json_t **json_obj_vsdk);
static gboolean ctrlm_iarm_init(void);
static void     ctrlm_iarm_terminate(void);
static gboolean ctrlm_networks_pre_init(json_t *json_obj_net_rf4ce, json_t *json_obj_root);
static gboolean ctrlm_networks_init(void);
static void     ctrlm_networks_terminate(void);
static void     ctrlm_thread_monitor_init(void);
static void     ctrlm_terminate(void);
static gboolean ctrlm_message_queue_delay(gpointer data);
static void     ctrlm_trigger_startup_actions(void);

static gpointer ctrlm_main_thread(gpointer param);
static void     ctrlm_queue_msg_destroy(gpointer msg);
static gboolean ctrlm_timeout_recently_booted(gpointer user_data);
static gboolean ctrlm_timeout_systemd_restart_delay(gpointer user_data);
static gboolean ctrlm_thread_monitor(gpointer user_data);
static gboolean ctrlm_was_cpu_halted(void);
static gboolean ctrlm_start_iarm(gpointer user_data);
#ifdef AUTH_ENABLED
static gboolean ctrlm_authservice_poll(gpointer user_data);
static gboolean ctrlm_authservice_expired(gpointer user_data);
#endif
static gboolean ctrlm_keycode_logging_poll(gpointer user_data);
static gboolean ctrlm_ntp_check(gpointer user_data);
static void     ctrlm_signals_register(void);
static void     ctrlm_signal_handler(int signal);

static void     ctrlm_main_iarm_call_status_get_(ctrlm_main_iarm_call_status_t *status);
static void     ctrlm_main_iarm_call_property_set_(ctrlm_main_iarm_call_property_t *property);
static void     ctrlm_main_iarm_call_property_get_(ctrlm_main_iarm_call_property_t *property);
static void     ctrlm_main_iarm_call_discovery_config_set_(ctrlm_main_iarm_call_discovery_config_t *config);
static void     ctrlm_main_iarm_call_autobind_config_set_(ctrlm_main_iarm_call_autobind_config_t *config);
static void     ctrlm_main_iarm_call_precommission_config_set_(ctrlm_main_iarm_call_precommision_config_t *config);
static void     ctrlm_main_iarm_call_factory_reset_(ctrlm_main_iarm_call_factory_reset_t *reset);
static void     ctrlm_main_iarm_call_controller_unbind_(ctrlm_main_iarm_call_controller_unbind_t *unbind);
static void     ctrlm_main_update_export_controller_list(void);
static void     ctrlm_main_iarm_call_ir_remote_usage_get_(ctrlm_main_iarm_call_ir_remote_usage_t *ir_remote_usage);
static void     ctrlm_main_iarm_call_pairing_metrics_get_(ctrlm_main_iarm_call_pairing_metrics_t *pairing_metrics);
static void     ctrlm_main_iarm_call_last_key_info_get_(ctrlm_main_iarm_call_last_key_info_t *last_key_info);
static void     ctrlm_main_iarm_call_control_service_start_pairing_mode_(ctrlm_main_iarm_call_control_service_pairing_mode_t *pairing);
static void     ctrlm_main_iarm_call_control_service_end_pairing_mode_(ctrlm_main_iarm_call_control_service_pairing_mode_t *pairing);
static void     ctrlm_stop_one_touch_autobind_(ctrlm_network_id_t network_id);
static void     ctrlm_close_pairing_window_(ctrlm_network_id_t network_id, ctrlm_close_pairing_window_reason reason);
static void     ctrlm_pairing_window_bind_status_set_(ctrlm_bind_status_t bind_status);
static void     ctrlm_discovery_remote_type_set_(const char* remote_type_str);
static void     ctrlm_controller_product_name_get(ctrlm_controller_id_t controller_id, char *source_name);
static void     ctrlm_global_rfc_values_retrieved(const ctrlm_rfc_attr_t &attr);

static gboolean ctrlm_timeout_line_of_sight(gpointer user_data);
static gboolean ctrlm_timeout_autobind(gpointer user_data);
static gboolean ctrlm_timeout_binding_button(gpointer user_data);
static gboolean ctrlm_timeout_screen_bind(gpointer user_data);
static gboolean ctrlm_timeout_one_touch_autobind(gpointer user_data);

static gboolean ctrlm_main_handle_day_change_ir_remote_usage();
static void     ctrlm_property_write_ir_remote_usage(void);
static guchar   ctrlm_property_write_ir_remote_usage(guchar *data, guchar length);
static void     ctrlm_property_read_ir_remote_usage(void);
static void     ctrlm_property_write_pairing_metrics(void);
static guchar   ctrlm_property_write_pairing_metrics(guchar *data, guchar length);
static void     ctrlm_property_read_pairing_metrics(void);
static void     ctrlm_property_write_last_key_info(void);
static guchar   ctrlm_property_write_last_key_info(guchar *data, guchar length);
static void     ctrlm_property_read_last_key_info(void);
static void     ctrlm_property_write_shutdown_time(void);
static guchar   ctrlm_property_write_shutdown_time(guchar *data, guchar length);
static void     ctrlm_property_read_shutdown_time(void);
static void     control_service_values_read_from_db();
static void     ctrlm_check_for_key_tag(int key_tag);

#ifdef MEMORY_LOCK
const char *memory_lock_progs[] = {
"/usr/bin/controlMgr",
"/usr/lib/libctrlm_hal_rf4ce.so.0.0.0",
"/usr/lib/libxraudio.so.0.0.0",
"/usr/lib/libxraudio-hal.so.0.0.0",
"/usr/lib/libxr_mq.so.0.0.0",
"/usr/lib/libxr-timer.so.0.0.0",
"/usr/lib/libxr-timestamp.so.0.0.0"
};
#endif

#if CTRLM_HAL_RF4CE_API_VERSION >= 9
static void ctrlm_crash_recovery_check();
static void ctrlm_backup_data();
#endif

static void ctrlm_vsdk_thread_poll(void *data);
static void ctrlm_vsdk_thread_response(void *data);

#ifdef MEM_DEBUG
static gboolean ctrlm_memory_profile(gpointer user_data) {
   g_mem_profile();
   return(TRUE);
}
#endif

#ifdef BREAKPAD_SUPPORT
static bool ctrlm_minidump_callback(const google_breakpad::MinidumpDescriptor& descriptor, void* context, bool succeeded) {
  LOG_FATAL("Minidump location: %s Status: %s\n", descriptor.path(), succeeded ? "SUCCEEDED" : "FAILED");
  return succeeded;
}
#endif


gboolean ctrlm_is_production_build(void) {
   return(g_ctrlm.production_build);
}

int main(int argc, char *argv[]) {
   // Set stdout to be line buffered
   setvbuf(stdout, NULL, _IOLBF, 0);

   LOG_INFO("ctrlm_main: name <%-24s> version <%-7s> branch <%-20s> commit <%s>\n", "ctrlm-main", CTRLM_MAIN_VERSION, CTRLM_MAIN_BRANCH, CTRLM_MAIN_COMMIT_ID);
   #ifdef CPC_ENABLED
   LOG_INFO("ctrlm_main: name <%-24s> version <%-7s> branch <%-20s> commit <%s>\n", "ctrlm-cpc", CTRLM_CPC_VERSION, CTRLM_CPC_BRANCH, CTRLM_CPC_COMMIT_ID);
   #endif

#ifdef MEMORY_LOCK
   clnl_init();
   for(unsigned int i = 0; i < (sizeof(memory_lock_progs) / sizeof(memory_lock_progs[0])); i++) {
      if(ctrlm_file_exists(memory_lock_progs[i])) {
         if(clnl_lock(memory_lock_progs[i], SECTION_TEXT)) {
            LOG_ERROR("%s: failed to lock instructions to memory <%s>\n", __FUNCTION__, memory_lock_progs[i]);
         } else {
            LOG_INFO("%s: successfully locked to memory <%s>\n", __FUNCTION__, memory_lock_progs[i]);
         }
      } else {
         LOG_DEBUG("%s: file doesn't exist, cannot lock to memory <%s>\n", memory_lock_progs[i]);
      }
   }
#endif

   ctrlm_signals_register();

#ifdef BREAKPAD_SUPPORT
   std::string minidump_path = "/opt/minidumps";
   #ifdef BREAKPAD_MINIDUMP_PATH_OVERRIDE
   minidump_path = BREAKPAD_MINIDUMP_PATH_OVERRIDE;
   #else
   FILE *fp= NULL;;
   if(( fp = fopen("/tmp/.SecureDumpEnable", "r")) != NULL) {
        minidump_path = "/opt/secure/minidumps";
        fclose(fp);
   }
   #endif

   google_breakpad::MinidumpDescriptor descriptor(minidump_path.c_str());
   google_breakpad::ExceptionHandler eh(descriptor, NULL, ctrlm_minidump_callback, NULL, true, -1);

   //ctrlm_crash();
#endif

   LOG_INFO("ctrlm_main: glib     run-time version... %d.%d.%d\n", glib_major_version, glib_minor_version, glib_micro_version);
   LOG_INFO("ctrlm_main: glib compile-time version... %d.%d.%d\n", GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
   const char *error = glib_check_version(GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);

   if(NULL != error) {
      LOG_FATAL("ctrlm_main: Glib not compatible: %s\n", error);
      return(-1);
   } else {
      LOG_INFO("ctrlm_main: Glib run-time library is compatible\n");
   }

#ifdef MEM_DEBUG
   LOG_WARN("ctrlm_main: Memory debug is ENABLED.\n");
   g_mem_set_vtable(glib_mem_profiler_table);
#endif

   sem_init(&g_ctrlm.ctrlm_utils_sem, 0, 1);

   if(!ctrlm_iarm_init()) {
      LOG_FATAL("ctrlm_main: Unable to initialize IARM bus.\n");
      // TODO handle this failure such that it retries
      return(-1);
   }

   // init device manager
   ctrlm_dsmgr_init();

   vsdk_version_info_t version_info[VSDK_VERSION_QTY_MAX] = {0};

   uint32_t qty_vsdk = VSDK_VERSION_QTY_MAX;
   vsdk_version(version_info, &qty_vsdk);

   for(uint32_t index = 0; index < qty_vsdk; index++) {
      vsdk_version_info_t *entry = &version_info[index];
      if(entry->name != NULL) {
         LOG_INFO("ctrlm_main: name <%-24s> version <%-7s> branch <%-20s> commit <%s>\n", entry->name ? entry->name : "NULL", entry->version ? entry->version : "NULL", entry->branch ? entry->branch : "NULL", entry->commit_id ? entry->commit_id : "NULL");
      }
   }
   vsdk_init();

   //struct sched_param param;
   //param.sched_priority = 10;
   //if(0 != sched_setscheduler(0, SCHED_FIFO, &param)) {
   //   printf("Error: Unable to set scheduler priority!\n");
   //}

   // Initialize control manager global structure
   g_ctrlm.main_loop                      = g_main_loop_new(NULL, true);
   g_ctrlm.main_thread                    = NULL;
   g_ctrlm.queue                          = NULL;
   g_ctrlm.production_build               = true;
   g_ctrlm.has_service_access_token       = false;
   g_ctrlm.sat_enabled                    = true;
   g_ctrlm.service_access_token_expiration_tag = 0;
   g_ctrlm.thread_monitor_timeout_val     = JSON_INT_VALUE_CTRLM_GLOBAL_THREAD_MONITOR_PERIOD * 1000;
   g_ctrlm.thread_monitor_index           = 0;
   g_ctrlm.thread_monitor_active          = true;
   g_ctrlm.thread_monitor_minidump        = JSON_BOOL_VALUE_CTRLM_GLOBAL_THREAD_MONITOR_MINIDUMP;
   g_ctrlm.server_url_authservice         = JSON_STR_VALUE_CTRLM_GLOBAL_URL_AUTH_SERVICE;
   g_ctrlm.authservice_poll_val           = JSON_INT_VALUE_CTRLM_GLOBAL_AUTHSERVICE_POLL_PERIOD * 1000;
   g_ctrlm.authservice_fast_poll_val      = JSON_INT_VALUE_CTRLM_GLOBAL_AUTHSERVICE_FAST_POLL_PERIOD;
   g_ctrlm.authservice_fast_retries       = 0;
   g_ctrlm.authservice_fast_retries_max   = JSON_INT_VALUE_CTRLM_GLOBAL_AUTHSERVICE_FAST_MAX_RETRIES;
   g_ctrlm.bound_controller_qty           = 0;
   g_ctrlm.recently_booted                = FALSE;
   g_ctrlm.recently_booted_timeout_val    = JSON_INT_VALUE_CTRLM_GLOBAL_TIMEOUT_RECENTLY_BOOTED;
   g_ctrlm.recently_booted_timeout_tag    = 0;
   g_ctrlm.line_of_sight                  = FALSE;
   g_ctrlm.line_of_sight_timeout_val      = JSON_INT_VALUE_CTRLM_GLOBAL_TIMEOUT_LINE_OF_SIGHT;
   g_ctrlm.line_of_sight_timeout_tag      = 0;
   g_ctrlm.autobind                       = FALSE;
   g_ctrlm.autobind_timeout_val           = JSON_INT_VALUE_CTRLM_GLOBAL_TIMEOUT_AUTOBIND;
   g_ctrlm.autobind_timeout_tag           = 0;
   g_ctrlm.binding_button                 = FALSE;
   g_ctrlm.binding_button_timeout_val     = JSON_INT_VALUE_CTRLM_GLOBAL_TIMEOUT_BUTTON_BINDING;
   g_ctrlm.binding_button_timeout_tag     = 0;
   g_ctrlm.binding_screen_active          = FALSE;
   g_ctrlm.screen_bind_timeout_val        = JSON_INT_VALUE_CTRLM_GLOBAL_TIMEOUT_SCREEN_BIND;
   g_ctrlm.screen_bind_timeout_tag        = 0;
   g_ctrlm.one_touch_autobind_timeout_val = JSON_INT_VALUE_CTRLM_GLOBAL_TIMEOUT_ONE_TOUCH_AUTOBIND;
   g_ctrlm.one_touch_autobind_timeout_tag = 0;
   g_ctrlm.one_touch_autobind_active      = FALSE;
   g_ctrlm.pairing_window.active          = FALSE;
   g_ctrlm.pairing_window.bind_status     = CTRLM_BIND_STATUS_NO_DISCOVERY_REQUEST;
   g_ctrlm.pairing_window.restrict_by_remote = CTRLM_PAIRING_RESTRICT_NONE;
   g_ctrlm.mask_key_codes_json            = JSON_BOOL_VALUE_CTRLM_GLOBAL_MASK_KEY_CODES;
   g_ctrlm.mask_key_codes_iarm            = false;
   g_ctrlm.mask_pii                       = true;
   g_ctrlm.keycode_logging_poll_val       = JSON_INT_VALUE_CTRLM_GLOBAL_KEYCODE_LOGGING_POLL_PERIOD * 1000;
   g_ctrlm.keycode_logging_retries        = 0;
   g_ctrlm.keycode_logging_max_retries    = JSON_INT_VALUE_CTRLM_GLOBAL_KEYCODE_LOGGING_MAX_RETRIES;
   g_ctrlm.db_path                        = JSON_STR_VALUE_CTRLM_GLOBAL_DB_PATH;
   g_ctrlm.minidump_path                  = JSON_STR_VALUE_CTRLM_GLOBAL_MINIDUMP_PATH;
   g_ctrlm.crash_recovery_threshold       = JSON_INT_VALUE_CTRLM_GLOBAL_CRASH_RECOVERY_THRESHOLD;
   g_ctrlm.successful_init                = FALSE;
   //g_ctrlm.precomission_table             = g_hash_table_new(g_str_hash, g_str_equal);
   g_ctrlm.loading_db                     = false;
   g_ctrlm.return_code                    = 0;
   g_ctrlm.power_state                    = ctrlm_main_iarm_call_get_power_state();
   g_ctrlm.auto_ack                       = true;
   g_ctrlm.local_conf                     = false;

   g_ctrlm.service_access_token.clear();
   g_ctrlm.has_receiver_id                = false;
   g_ctrlm.has_device_id                  = false;
   g_ctrlm.has_service_account_id         = false;
   g_ctrlm.has_partner_id                 = false;
   g_ctrlm.has_experience                 = false;
   g_ctrlm.has_service_access_token       = false;
   sem_init(&g_ctrlm.service_access_token_semaphore, 0, 1);

   g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_UNKNOWN;
   g_ctrlm.last_key_info.is_screen_bind_mode  = false;
   g_ctrlm.last_key_info.remote_keypad_config = CTRLM_REMOTE_KEYPAD_CONFIG_INVALID;
   errno_t safec_rc = strcpy_s(g_ctrlm.last_key_info.source_name, sizeof(g_ctrlm.last_key_info.source_name), ctrlm_rcu_ir_remote_types_str(g_ctrlm.last_key_info.last_ir_remote_type));
   ERR_CHK(safec_rc);

   g_ctrlm.discovery_remote_type[0] = '\0';

#ifdef MEM_DEBUG
   g_mem_profile();
   g_timeout_add_seconds(10, ctrlm_memory_profile, NULL);
#endif

   LOG_INFO("ctrlm_main: load version\n");
   if(!ctrlm_load_version()) {
      LOG_FATAL("ctrlm_main: failed to load version\n");
      return(-1);
   }

   // Launch a thread to handle DB writes asynchronously
   // Create an asynchronous queue to receive incoming messages from the networks
   g_ctrlm.queue = g_async_queue_new_full(ctrlm_queue_msg_destroy);

   g_ctrlm.mask_pii = ctrlm_is_production_build() ? JSON_ARRAY_VAL_BOOL_CTRLM_GLOBAL_MASK_PII_0 : JSON_ARRAY_VAL_BOOL_CTRLM_GLOBAL_MASK_PII_1;

   LOG_INFO("ctrlm_main: load device mac\n");
   if(!ctrlm_load_device_mac()) {
      LOG_ERROR("ctrlm_main: failed to load device mac\n");
      // Do not crash the program here
   }

   LOG_INFO("ctrlm_main: create Voice object\n");
   g_ctrlm.voice_session = new ctrlm_voice_generic_t();

   LOG_INFO("ctrlm_main: create IRDB object\n");
   g_ctrlm.irdb = ctrlm_irdb_create();

   LOG_INFO("ctrlm_main: ctrlm_rfc init\n");
   // This tells the RFC component to go fetch all the enabled attributes once
   // we are fully initialized. This brings us to parity with the current RFC/TR181
   // implementation.
   ctrlm_rfc_t *rfc = ctrlm_rfc_t::get_instance();
   if(rfc) {
      rfc->add_changed_listener(ctrlm_rfc_t::attrs::GLOBAL, &ctrlm_global_rfc_values_retrieved);
   }
   // TODO: We could possibly schedule this to run once every few hours or whatever
   //       the team decides is best.

   // Check if recently booted
   struct sysinfo s_info;
   if(sysinfo(&s_info) != 0) {
      LOG_ERROR("ctrlm_main: Unable to get system uptime\n");
   } else {
      LOG_INFO("ctrlm_main: System up for %lu seconds\n", s_info.uptime);
      if(s_info.uptime < CTRLM_MAIN_FIRST_BOOT_TIME_MAX) { // System first boot
         LOG_INFO("ctrlm_main: System first boot\n");
      }
      if(s_info.uptime < (long)(g_ctrlm.recently_booted_timeout_val / 1000)) { // System just booted
         LOG_INFO("ctrlm_main: Setting recently booted to true\n");
         g_ctrlm.recently_booted = TRUE;
      }
   }

   LOG_INFO("ctrlm_main: load config\n");
   // Load configuration from the filesystem
   json_t *json_obj_root, *json_obj_net_rf4ce, *json_obj_voice, *json_obj_device_update, *json_obj_validation, *json_obj_vsdk;
   json_obj_root          = NULL;
   json_obj_net_rf4ce     = NULL;
   json_obj_voice         = NULL;
   json_obj_device_update = NULL;
   json_obj_validation    = NULL;
   json_obj_vsdk          = NULL;
   ctrlm_load_config(&json_obj_root, &json_obj_net_rf4ce, &json_obj_voice, &json_obj_device_update, &json_obj_validation, &json_obj_vsdk);

#ifdef AUTH_ENABLED
   LOG_INFO("ctrlm_main: ctrlm_auth init\n");
#ifdef AUTH_THUNDER
   g_ctrlm.authservice = new ctrlm_auth_thunder_t();
#else
   g_ctrlm.authservice = new ctrlm_auth_legacy_t(g_ctrlm.server_url_authservice);
#endif
   if (!ctrlm_has_authservice_data()) {
      if (!g_ctrlm.authservice->is_ready()) {
         LOG_WARN("%s: Authservice not ready, no reason to poll\n", __FUNCTION__);
      } else {
         if (!ctrlm_load_authservice_data()) {
            LOG_INFO("%s: Starting polling authservice for device data\n", __FUNCTION__);
            g_ctrlm.authservice_poll_tag = ctrlm_timeout_create(g_ctrlm.recently_booted ? g_ctrlm.authservice_fast_poll_val : g_ctrlm.authservice_poll_val,
                                                                ctrlm_authservice_poll,
                                                                NULL);
            if (!g_ctrlm.recently_booted) {
               g_ctrlm.authservice_fast_retries = g_ctrlm.authservice_fast_retries_max;
            }
         }
      }
   }
#endif // AUTH_ENABLED

   g_ctrlm.voice_session->voice_stb_data_stb_name_set(g_ctrlm.stb_name);
   g_ctrlm.voice_session->voice_stb_data_pii_mask_set(g_ctrlm.mask_pii);

#if defined(BREAKPAD_SUPPORT) && !defined(BREAKPAD_MINIDUMP_PATH_OVERRIDE)
   if(g_ctrlm.minidump_path != JSON_STR_VALUE_CTRLM_GLOBAL_MINIDUMP_PATH) {
      google_breakpad::MinidumpDescriptor descriptor_json(g_ctrlm.minidump_path.c_str());
      eh.set_minidump_descriptor(descriptor_json);
   }
#endif

   // Database init must occur after network qty and type are known
   LOG_INFO("ctrlm_main: pre-init networks\n");
   ctrlm_networks_pre_init(json_obj_net_rf4ce, json_obj_root);

   LOG_INFO("ctrlm_main: init recovery\n");
   ctrlm_recovery_init();

#if CTRLM_HAL_RF4CE_API_VERSION >= 9
   // Check to see if we are recovering from a crash before init
   ctrlm_crash_recovery_check();
#endif

   LOG_INFO("ctrlm_main: init database\n");
   if(!ctrlm_db_init(g_ctrlm.db_path.c_str())) {
#if CTRLM_HAL_RF4CE_API_VERSION >= 9
      guint invalid_db = 1;
      ctrlm_recovery_property_set(CTRLM_RECOVERY_INVALID_CTRLM_DB, &invalid_db);
      return(-1);
#endif
   }

   g_ctrlm.loading_db = true;
   //Read shutdown time data from the db
   ctrlm_property_read_shutdown_time();
   //Read IR remote usage data from the db
   ctrlm_property_read_ir_remote_usage();
   //Read pairing metrics data from the db
   ctrlm_property_read_pairing_metrics();
   //Read last key info from the db
   ctrlm_property_read_last_key_info();
   g_ctrlm.loading_db = false;

   // This needs to happen after the DB is init, but before voice
   if(TRUE == g_ctrlm.recently_booted) {
      g_ctrlm.recently_booted_timeout_tag   = ctrlm_timeout_create(g_ctrlm.recently_booted_timeout_val - (s_info.uptime * 1000), ctrlm_timeout_recently_booted, NULL);
   }

   ctrlm_timeout_create(CTRLM_RESTART_UPDATE_TIMEOUT, ctrlm_timeout_systemd_restart_delay, NULL);

   LOG_INFO("ctrlm_main: init validation\n");
   ctrlm_validation_init(json_obj_validation);

   LOG_INFO("ctrlm_main: init rcu\n");
   ctrlm_rcu_init();

   // Device update components needs to be initialized after controllers are loaded
   LOG_INFO("ctrlm_main: init device update\n");
   ctrlm_device_update_init(json_obj_device_update);

   // Initialize semaphore
   sem_init(&g_ctrlm.semaphore, 0, 0);

   g_ctrlm.main_thread = g_thread_new("ctrlm_main", ctrlm_main_thread, NULL);

   // Block until initialization is complete or a timeout occurs
   LOG_INFO("ctrlm_main: Waiting for ctrlm main thread initialization...\n");
   sem_wait(&g_ctrlm.semaphore);


   LOG_INFO("ctrlm_main: init networks\n");
   if(!ctrlm_networks_init()) {
      LOG_FATAL("ctrlm_main: Unable to initialize networks\n");
      return(-1);
   }

   LOG_INFO("ctrlm_main: init voice\n");
   g_ctrlm.voice_session->voice_configure_config_file_json(json_obj_voice, json_obj_vsdk, g_ctrlm.local_conf );
   LOG_INFO("ctrlm_main: networks init complete\n");

#if CTRLM_HAL_RF4CE_API_VERSION >= 9
   // Init was successful, create backups of all NVM files
   guint crash_count = 0;
   ctrlm_recovery_property_set(CTRLM_RECOVERY_CRASH_COUNT, &crash_count);
   ctrlm_backup_data();
   // Terminate recovery component and unlink shared memory
   ctrlm_recovery_terminate(TRUE);
#endif

   if(json_obj_root) {
      json_decref(json_obj_root);
   }

   //export device list for all devices on all networks for xconf update checks
   LOG_INFO("ctrlm_main: init xconf device list\n");
   ctrlm_main_update_export_controller_list();

   // Thread monitor
   ctrlm_thread_monitor_init();

   g_ctrlm.successful_init = TRUE;

   //Get the keycode logging preference
   IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t param;
   IARM_Result_t iarm_result = IARM_Bus_Call(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_API_GetKeyCodeLoggingPref, &param, sizeof(IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t));
   if(IARM_RESULT_SUCCESS == iarm_result) {
      g_ctrlm.mask_key_codes_iarm = (param.logStatus == 0);
      LOG_INFO("%s: Keycode mask logging iarm <%s>.\n", __FUNCTION__, g_ctrlm.mask_key_codes_iarm ? "ON" : "OFF");
   } else {
      LOG_WARN("%s: IARM Bus Call Failed <%s>. Starting polling SYSMgr for keycode logging status\n", __FUNCTION__, ctrlm_iarm_result_str(iarm_result));
      g_ctrlm.keycode_logging_poll_tag = ctrlm_timeout_create(g_ctrlm.keycode_logging_poll_val, ctrlm_keycode_logging_poll, NULL);
   }

   ctrlm_trigger_startup_actions();

   LOG_INFO("ctrlm_main: Enter main loop\n");
   g_main_loop_run(g_ctrlm.main_loop);

   //Save the shutdown time if it is valid
   time_t current_time = time(NULL);
   if(g_ctrlm.shutdown_time < current_time) {
      g_ctrlm.shutdown_time = current_time;
      ctrlm_property_write_shutdown_time();
   } else {
      LOG_WARN("%s: Current Time <%ld> is less than the last shutdown time <%ld>.  Ignoring.\n", __FUNCTION__, current_time, g_ctrlm.shutdown_time);
   }

   LOG_INFO("ctrlm_main: main loop exited\n");
   ctrlm_terminate();

   ctrlm_dsmgr_deinit();

   if(g_ctrlm.voice_session != NULL) {
      delete g_ctrlm.voice_session;
      g_ctrlm.voice_session = NULL;
   }

   vsdk_term();

   if(g_ctrlm.irdb != NULL) {
      delete g_ctrlm.irdb;
      g_ctrlm.irdb = NULL;
   }

   sem_destroy(&g_ctrlm.service_access_token_semaphore);

#if AUTH_ENABLED
   if(g_ctrlm.authservice != NULL) {
      delete g_ctrlm.authservice;
      g_ctrlm.authservice = NULL;
   }
#endif

   sem_destroy(&g_ctrlm.ctrlm_utils_sem);

#ifdef MEMORY_LOCK
   for(unsigned int i = 0; i < (sizeof(memory_lock_progs) / sizeof(memory_lock_progs[0])); i++) {
      if(ctrlm_file_exists(memory_lock_progs[i])) {
         clnl_unlock(memory_lock_progs[i], SECTION_TEXT);
      }
   }
   clnl_destroy();
#endif

   ctrlm_config_t::destroy_instance();
   ctrlm_rfc_t::destroy_instance();

   LOG_INFO("ctrlm_main: exit program\n");
   return (g_ctrlm.return_code);
}

void ctrlm_utils_sem_wait(){
   sem_wait(&g_ctrlm.ctrlm_utils_sem);
}

void ctrlm_utils_sem_post(){
   sem_post(&g_ctrlm.ctrlm_utils_sem);
}

void ctrlm_thread_monitor_init(void) {
   ctrlm_thread_monitor_t thread_monitor;

   g_ctrlm.thread_monitor_timeout_tag = ctrlm_timeout_create(g_ctrlm.thread_monitor_timeout_val, ctrlm_thread_monitor, NULL);

   thread_monitor.name        = CTRLM_THREAD_NAME_MAIN;
   thread_monitor.queue_push  = ctrlm_main_queue_msg_push;
   thread_monitor.obj_network = NULL;
   thread_monitor.function    = NULL;
   thread_monitor.response    = CTRLM_THREAD_MONITOR_RESPONSE_ALIVE;
   g_ctrlm.monitor_threads.push_back(thread_monitor);

   thread_monitor.name        = CTRLM_THREAD_NAME_DATABASE;
   thread_monitor.queue_push  = ctrlm_db_queue_msg_push_front;
   thread_monitor.obj_network = NULL;
   thread_monitor.function    = NULL;
   thread_monitor.response    = CTRLM_THREAD_MONITOR_RESPONSE_ALIVE;
   g_ctrlm.monitor_threads.push_back(thread_monitor);

   thread_monitor.name        = CTRLM_THREAD_NAME_DEVICE_UPDATE;
   thread_monitor.queue_push  = ctrlm_device_update_queue_msg_push;
   thread_monitor.obj_network = NULL;
   thread_monitor.function    = NULL;
   thread_monitor.response    = CTRLM_THREAD_MONITOR_RESPONSE_ALIVE;
   g_ctrlm.monitor_threads.push_back(thread_monitor);

   thread_monitor.name        = CTRLM_THREAD_NAME_VOICE_SDK;
   thread_monitor.queue_push  = NULL;
   thread_monitor.obj_network = NULL;
   thread_monitor.function    = ctrlm_vsdk_thread_poll;
   thread_monitor.response    = CTRLM_THREAD_MONITOR_RESPONSE_ALIVE;
   g_ctrlm.monitor_threads.push_back(thread_monitor);

   for(auto const &itr : g_ctrlm.networks) {
      if(CTRLM_HAL_RESULT_SUCCESS == itr.second->init_result_) {
         thread_monitor.name        = itr.second->name_get();
         thread_monitor.queue_push  = NULL;
         thread_monitor.obj_network = itr.second;
         thread_monitor.function    = NULL;
         thread_monitor.response    = CTRLM_THREAD_MONITOR_RESPONSE_ALIVE;
         g_ctrlm.monitor_threads.push_back(thread_monitor);
      }
   }

   g_ctrlm.monitor_threads.shrink_to_fit();

   if(CTRLM_TR181_RESULT_SUCCESS != ctrlm_tr181_bool_get(CTRLM_RF4CE_TR181_THREAD_MONITOR_MINIDUMP_ENABLE, &g_ctrlm.thread_monitor_minidump)) {
      LOG_INFO("%s: Thread Monitor Minidump is <%s> (TR181 not present)\n", __FUNCTION__, (g_ctrlm.thread_monitor_minidump ? "ENABLED" : "DISABLED"));
   } else {
      LOG_INFO("%s: Thread Monitor Minidump is <%s>\n", __FUNCTION__, (g_ctrlm.thread_monitor_minidump ? "ENABLED" : "DISABLED"));
   }

   // Run once to kick off the first poll
   ctrlm_thread_monitor(NULL);
}

gboolean ctrlm_thread_monitor(gpointer user_data) {
   if(g_ctrlm.thread_monitor_index < (60 * 60 * 1000)) { // One hour in milliseconds
      printf("."); fflush(stdout);
      g_ctrlm.thread_monitor_index += g_ctrlm.thread_monitor_timeout_val;
   } else {
      printf("\n");
      LOG_INFO("%s: .", __FUNCTION__); fflush(stdout);
      g_ctrlm.thread_monitor_index = 0;
   }

   #ifdef FDC_ENABLED
   uint32_t limit_soft = 40;
   uint32_t limit_hard = FD_SETSIZE - 20;
   int32_t rc = xr_fdc_check(limit_soft, limit_hard, true);
   if(rc < 0) {
      LOG_ERROR("%s: xr_fdc_check\n", __FUNCTION__);
   } else if(rc > 0) {
      LOG_FATAL("%s: xr_fdc_check hard limit\n", __FUNCTION__);
      ctrlm_quit_main_loop();
      return(FALSE);
   }
   #endif

   if(ctrlm_was_cpu_halted()) {
      LOG_INFO("%s: skipping response check due to power state <%s>\n", __FUNCTION__,ctrlm_power_state_str(g_ctrlm.power_state));
      g_ctrlm.thread_monitor_active = false; // Deactivate thread monitoring
   } else if(!g_ctrlm.thread_monitor_active) {
      LOG_INFO("%s: activate due to power state <%s>\n", __FUNCTION__,ctrlm_power_state_str(g_ctrlm.power_state));
      g_ctrlm.thread_monitor_active = true;  // Activate thread monitoring again
   } else {
      // Check the response from each thread on the previous attempt
      for(vector<ctrlm_thread_monitor_t>::iterator it = g_ctrlm.monitor_threads.begin(); it != g_ctrlm.monitor_threads.end(); it++) {
         LOG_DEBUG("%s: Checking %s\n", __FUNCTION__, it->name);

         if(it->response != CTRLM_THREAD_MONITOR_RESPONSE_ALIVE) {
            LOG_FATAL("%s: Thread %s is unresponsive\n", __FUNCTION__, it->name);
            #ifdef BREAKPAD_SUPPORT
            if(g_ctrlm.thread_monitor_minidump) {
               LOG_FATAL("%s: Thread Monitor Minidump is enabled\n", __FUNCTION__);

               if(       0 == strncmp(it->name, CTRLM_THREAD_NAME_MAIN,          sizeof(CTRLM_THREAD_NAME_MAIN))) {
                  ctrlm_crash_ctrlm_main();
               } else if(0 == strncmp(it->name, CTRLM_THREAD_NAME_VOICE_SDK,     sizeof(CTRLM_THREAD_NAME_VOICE_SDK))) {
                  ctrlm_crash_vsdk();
               } else if(0 == strncmp(it->name, CTRLM_THREAD_NAME_RF4CE,         sizeof(CTRLM_THREAD_NAME_RF4CE))) {
                  #ifdef RF4CE_HAL_QORVO
                  ctrlm_crash_rf4ce_qorvo();
                  #else
                  ctrlm_crash_rf4ce_ti();
                  #endif
               } else if(0 == strncmp(it->name, CTRLM_THREAD_NAME_BLE,           sizeof(CTRLM_THREAD_NAME_BLE))) {
                  ctrlm_crash_ble();
               } else if(0 == strncmp(it->name, CTRLM_THREAD_NAME_DATABASE,      sizeof(CTRLM_THREAD_NAME_DATABASE))) {
                  ctrlm_crash_ctrlm_database();
               } else if(0 == strncmp(it->name, CTRLM_THREAD_NAME_DEVICE_UPDATE, sizeof(CTRLM_THREAD_NAME_DEVICE_UPDATE))) {
                  ctrlm_crash_ctrlm_device_update();
               } else {
                  ctrlm_crash();
               }
            }
            #endif
            ctrlm_quit_main_loop();
            return (FALSE);
         }
      }
   }

   if(g_ctrlm.thread_monitor_active) { // Thread monitoring is active
      // Send a message to each thread to respond
      for(vector<ctrlm_thread_monitor_t>::iterator it = g_ctrlm.monitor_threads.begin(); it != g_ctrlm.monitor_threads.end(); it++) {
         LOG_DEBUG("%s: Sending %s\n", __FUNCTION__, it->name);
         it->response = CTRLM_THREAD_MONITOR_RESPONSE_DEAD;

         if(it->queue_push != NULL) { // Allocate a message and send it to the thread's queue
            ctrlm_thread_monitor_msg_t *msg = (ctrlm_thread_monitor_msg_t *)g_malloc(sizeof(ctrlm_thread_monitor_msg_t));

            if(NULL == msg) {
               LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
               g_assert(0);
               ctrlm_quit_main_loop();
               return(FALSE);
            }
            msg->type     = CTRLM_MAIN_QUEUE_MSG_TYPE_TICKLE;
            msg->response = &it->response;

            (*(it->queue_push))((gpointer)msg);
         } else if(it->obj_network != NULL){
            ctrlm_thread_monitor_msg_t *msg = (ctrlm_thread_monitor_msg_t *)g_malloc(sizeof(ctrlm_thread_monitor_msg_t));

            if(NULL == msg) {
               LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
               g_assert(0);
               ctrlm_quit_main_loop();
               return(FALSE);
            }
            msg->type        = CTRLM_MAIN_QUEUE_MSG_TYPE_THREAD_MONITOR_POLL;
            msg->obj_network = it->obj_network;
            msg->response    = &it->response;
            ctrlm_main_queue_msg_push(msg);
         } else if(it->function != NULL) {
            (*it->function)(&it->response);
         }
      }
   }

   return(TRUE);
}

// Returns true if the CPU was halted based on the current power state
gboolean ctrlm_was_cpu_halted(void) {
   #ifdef ENABLE_DEEP_SLEEP
   if(g_ctrlm.power_state == CTRLM_POWER_STATE_STANDBY) {
      return(TRUE);
   }
   #endif
   if(g_ctrlm.power_state == CTRLM_POWER_STATE_DEEP_SLEEP || g_ctrlm.power_state == CTRLM_POWER_STATE_STANDBY_VOICE_SESSION) {
      return(TRUE);
   }
   return(FALSE);
}

void ctrlm_vsdk_thread_response(void *data) {
   ctrlm_thread_monitor_response_t *response = (ctrlm_thread_monitor_response_t *)data;
   if(response != NULL) {
      *response = CTRLM_THREAD_MONITOR_RESPONSE_ALIVE;
   }
}

void ctrlm_vsdk_thread_poll(void *data) {
   vsdk_thread_poll(ctrlm_vsdk_thread_response, data);
}

#ifdef AUTH_ENABLED
static gboolean ctrlm_authservice_expired(gpointer user_data) {
   LOG_WARN("%s: SAT Token is expired...\n", __FUNCTION__);
   ctrlm_main_invalidate_service_access_token();
   return(FALSE);
}

gboolean ctrlm_authservice_poll(gpointer user_data) {
   ctrlm_main_queue_msg_authservice_poll_t *msg = (ctrlm_main_queue_msg_authservice_poll_t *)g_malloc(sizeof(ctrlm_main_queue_msg_authservice_poll_t));
   gboolean ret = FALSE;
   gboolean switch_poll_interval = FALSE;

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      ctrlm_quit_main_loop();
      ctrlm_timeout_destroy(&g_ctrlm.authservice_poll_tag);
      return(false);
   }

   sem_t semaphore;
   sem_init(&semaphore, 0, 0);

   msg->type                 = CTRLM_MAIN_QUEUE_MSG_TYPE_AUTHSERVICE_POLL;
   msg->ret                  = &ret;
   msg->switch_poll_interval = &switch_poll_interval;
   msg->semaphore            = &semaphore;
   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);
   sem_destroy(&semaphore);

   if(ret == FALSE) {
      ctrlm_timeout_destroy(&g_ctrlm.authservice_poll_tag);
   }

   if(switch_poll_interval) {
      ctrlm_timeout_destroy(&g_ctrlm.authservice_poll_tag);
      g_ctrlm.authservice_poll_tag = ctrlm_timeout_create(g_ctrlm.authservice_poll_val, ctrlm_authservice_poll, NULL);
   }

   return ret;
}
#endif

gboolean ctrlm_keycode_logging_poll(gpointer user_data) {
   IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t param;
   IARM_Result_t iarm_result = IARM_Bus_Call(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_API_GetKeyCodeLoggingPref, &param, sizeof(IARM_BUS_SYSMGR_KEYCodeLoggingInfo_Param_t));
   if(IARM_RESULT_SUCCESS == iarm_result) {
      g_ctrlm.mask_key_codes_iarm = (param.logStatus == 0);
      LOG_INFO("%s: Keycode mask logging iarm <%s>.\n", __FUNCTION__, g_ctrlm.mask_key_codes_iarm ? "ON" : "OFF");
      g_ctrlm.keycode_logging_poll_tag = 0;
      return false;
   }

   g_ctrlm.keycode_logging_retries += 1;
   if(g_ctrlm.keycode_logging_max_retries > g_ctrlm.keycode_logging_retries) {
      LOG_WARN("%s: IARM Bus Call Failed <%s>. Keep the keycode logging timer going.  Retries <%d>.\n", __FUNCTION__, ctrlm_iarm_result_str(iarm_result), g_ctrlm.keycode_logging_retries);
      return true;
   } else {
      LOG_WARN("%s: IARM Bus Call Failed <%s>. Stop the keycode logging timer.  Retries <%d>.\n", __FUNCTION__, ctrlm_iarm_result_str(iarm_result), g_ctrlm.keycode_logging_retries);
      g_ctrlm.keycode_logging_poll_tag = 0;
      return false;
   }
}

void ctrlm_signals_register(void) {
   // Handle these signals
   LOG_INFO("%s: Registering SIGINT...\n", __FUNCTION__);
   if(signal(SIGINT, ctrlm_signal_handler) == SIG_ERR) {
      LOG_ERROR("%s: Unable to register for SIGINT.\n", __FUNCTION__);
   }
   LOG_INFO("%s: Registering SIGTERM...\n", __FUNCTION__);
   if(signal(SIGTERM, ctrlm_signal_handler) == SIG_ERR) {
      LOG_ERROR("%s: Unable to register for SIGTERM.\n", __FUNCTION__);
   }
   LOG_INFO("%s: Registering SIGQUIT...\n", __FUNCTION__);
   if(signal(SIGQUIT, ctrlm_signal_handler) == SIG_ERR) {
      LOG_ERROR("%s: Unable to register for SIGQUIT.\n", __FUNCTION__);
   }
   LOG_INFO("%s: Registering SIGPIPE...\n", __FUNCTION__);
   if(signal(SIGPIPE, ctrlm_signal_handler) == SIG_ERR) {
      LOG_ERROR("%s: Unable to register for SIGPIPE.\n", __FUNCTION__);
   }
}

void ctrlm_signal_handler(int signal) {
   switch(signal) {
      case SIGTERM:
      case SIGINT: {
         LOG_INFO("%s: Received %s\n", __FUNCTION__, signal == SIGINT ? "SIGINT" : "SIGTERM");
         ctrlm_quit_main_loop();
         break;
      }
      case SIGQUIT: {
         LOG_INFO("%s: Received SIGQUIT\n", __FUNCTION__);
#ifdef BREAKPAD_SUPPORT
         ctrlm_crash();
#endif
         break;
      }
      case SIGPIPE: {
         LOG_ERROR("%s: Received SIGPIPE. Pipe is broken\n", __FUNCTION__);
         break;
      }
      default:
         LOG_ERROR("%s: Received unhandled signal %d\n", __FUNCTION__, signal);
         break;
   }
}

void ctrlm_quit_main_loop() {
   if (g_main_loop_is_running(g_ctrlm.main_loop)) {
      g_main_loop_quit(g_ctrlm.main_loop);
   }
}


void ctrlm_terminate(void) {
   LOG_INFO("%s: IARM\n", __FUNCTION__);
   ctrlm_main_iarm_terminate();

   LOG_INFO("%s: Main\n", __FUNCTION__);
   // Now clean up control manager main
   if(g_ctrlm.main_thread != NULL) {
      ctrlm_main_queue_msg_header_t *msg = (ctrlm_main_queue_msg_header_t *)g_malloc(sizeof(ctrlm_main_queue_msg_header_t));
      if(msg == NULL) {
         LOG_ERROR("%s: Out of memory\n", __FUNCTION__);
      } else {
         struct timespec end_time;
         clock_gettime(CLOCK_REALTIME, &end_time);
         end_time.tv_sec += 5;
         msg->type = CTRLM_MAIN_QUEUE_MSG_TYPE_TERMINATE;

         ctrlm_main_queue_msg_push((gpointer)msg);

         // Block until termination is acknowledged or a timeout occurs
         LOG_INFO("%s: Waiting for main thread termination...\n", __FUNCTION__);
         int acknowledged = sem_timedwait(&g_ctrlm.semaphore, &end_time); 

         if(acknowledged==-1) { // no response received
            LOG_INFO("%s: Do NOT wait for thread to exit\n", __FUNCTION__);
         } else {
            // Wait for thread to exit
            LOG_INFO("%s: Waiting for thread to exit\n", __FUNCTION__);
            g_thread_join(g_ctrlm.main_thread);
            LOG_INFO("%s: thread exited.\n", __FUNCTION__);
         }
      }
   }

   LOG_INFO("%s: Recovery\n", __FUNCTION__);
   ctrlm_recovery_terminate(FALSE);

   LOG_INFO("%s: Validation\n", __FUNCTION__);
   ctrlm_validation_terminate();

   LOG_INFO("%s: Rcu\n", __FUNCTION__);
   ctrlm_rcu_terminate();

   LOG_INFO("%s: Device Update\n", __FUNCTION__);
   ctrlm_device_update_terminate();

   LOG_INFO("%s: Networks\n", __FUNCTION__);
   ctrlm_networks_terminate();

   LOG_INFO("%s: Database\n", __FUNCTION__);
   ctrlm_db_terminate();

   LOG_INFO("%s: IARM\n", __FUNCTION__);
   ctrlm_iarm_terminate();
}

#if CTRLM_HAL_RF4CE_API_VERSION >= 15
void ctrlm_on_network_assert(ctrlm_network_id_t network_id, const char* assert_info) {
   LOG_INFO("%s: Assert \'%s\' on %s(%u) network.\n", __FUNCTION__, assert_info, ctrlm_network_type_str(ctrlm_network_type_get(network_id)).c_str(), network_id);
   if (CTRLM_NETWORK_TYPE_RF4CE == ctrlm_network_type_get(network_id)) {
      ctrlm_obj_network_t* net = NULL;
      if(g_ctrlm.networks.count(network_id) > 0) {
         net = g_ctrlm.networks[network_id];
         net->analyze_assert_reason(assert_info);
      }

      // RDK HW test requests ctrlm via IARM to check rf4ce state
      // keep ctrlm on if rf4ce chip is in failed state.
      if (net == NULL || !net->is_failed_state()) {
         ctrlm_on_network_assert(network_id);
      }
   }
}
#endif

void ctrlm_on_network_assert(ctrlm_network_id_t network_id) {
   LOG_ERROR("%s: Assert on network %u. Terminating...\n", __FUNCTION__, network_id);
   if(g_ctrlm.networks.count(network_id) > 0) {
      g_ctrlm.networks[network_id]->disable_hal_calls();
   }
   if (g_ctrlm.main_thread == g_thread_self ()) {
       // Invalidate main thread so terminate does not attempt to terminate it
       g_ctrlm.main_thread = NULL;
   }
   // g_main_loop_quit() will be called in ctrlm_signal_handler(SIGTERM)
   g_ctrlm.return_code = -1;
   ctrlm_signal_handler(SIGTERM);
   // give main() time to clean up
   sleep(5);
   // Exit here in case main fails to exit
   exit(-1);
}

gboolean ctrlm_load_version(void) {
   rdk_version_info_t info;
   int ret_val = rdk_version_parse_version(&info);

   if(ret_val != 0) {
      LOG_ERROR("%s: parse error <%s>\n", __FUNCTION__, info.parse_error == NULL ? "" : info.parse_error);
   } else {
      g_ctrlm.image_name       = info.image_name;
      g_ctrlm.stb_name         = info.stb_name;
      g_ctrlm.image_branch     = info.branch_name;
      g_ctrlm.image_version    = info.version_name;
      g_ctrlm.image_build_time = info.image_build_time;
      g_ctrlm.production_build = info.production_build;

      LOG_INFO("%s: STB Name <%s> Image Type <%s> Version <%s> Branch <%s> Build Time <%s>\n", __FUNCTION__, g_ctrlm.stb_name.c_str(), g_ctrlm.production_build ? "PROD" : "DEV", g_ctrlm.image_version.c_str(), g_ctrlm.image_branch.c_str(), g_ctrlm.image_build_time.c_str());
   }

   rdk_version_object_free(&info);

   return(ret_val == 0);
}

gboolean ctrlm_load_device_mac(void) {
   gboolean ret = false;
   std::string mac;
   std::string file;
   std::ifstream ifs;
   const char *interface = NULL;

   // First check environment variable to find ESTB interface, or fall back to default
   interface = getenv("ESTB_INTERFACE");
   file = "/sys/class/net/" + std::string((interface != NULL ? interface : CTRLM_DEFAULT_DEVICE_MAC_INTERFACE)) + "/address";

   // Open file and read mac address
   ifs.open(file.c_str(), std::ifstream::in);
   if(ifs.is_open()) {
      ifs >> mac;
      std::transform(mac.begin(), mac.end(), mac.begin(), ::toupper);
      g_ctrlm.device_mac = mac;
      ret = true;
      LOG_INFO("%s: Device Mac set to <%s>\n", __FUNCTION__, ctrlm_is_pii_mask_enabled() ? "***" : mac.c_str());
   } else {
      LOG_ERROR("%s: Failed to get MAC address for device mac\n", __FUNCTION__);
      g_ctrlm.device_mac = "UNKNOWN";
   }

   return(ret);
}

#ifdef AUTH_ENABLED
void ctrlm_main_auth_start_poll() {
   ctrlm_timeout_destroy(&g_ctrlm.authservice_poll_tag);
   g_ctrlm.authservice_poll_tag = ctrlm_timeout_create(g_ctrlm.recently_booted? g_ctrlm.authservice_fast_poll_val : g_ctrlm.authservice_poll_val,
                                                         ctrlm_authservice_poll,
                                                         NULL);
}

#ifdef AUTH_RECEIVER_ID
gboolean ctrlm_main_has_receiver_id_get(void) {
   return(g_ctrlm.has_receiver_id);
}

void ctrlm_main_has_receiver_id_set(gboolean has_id) {
   g_ctrlm.has_receiver_id = has_id;
}

gboolean ctrlm_load_receiver_id(void) {
   if(!g_ctrlm.authservice->get_receiver_id(g_ctrlm.receiver_id)) {
      ctrlm_main_has_receiver_id_set(false);
      return(false);
   }

   g_ctrlm.voice_session->voice_stb_data_receiver_id_set(g_ctrlm.receiver_id);

   for(auto const &itr : g_ctrlm.networks) {
      itr.second->receiver_id_set(g_ctrlm.receiver_id);
   }
   ctrlm_main_has_receiver_id_set(true);
   return(true);
}
#endif

#ifdef AUTH_DEVICE_ID
void ctrlm_main_has_device_id_set(gboolean has_id) {
   g_ctrlm.has_device_id   = has_id;
}

gboolean ctrlm_main_has_device_id_get(void) {
   return(g_ctrlm.has_device_id);
}

gboolean ctrlm_load_device_id(void) {
   if(!g_ctrlm.authservice->get_device_id(g_ctrlm.device_id)) {
      ctrlm_main_has_device_id_set(false);
      return(false);
   }
   g_ctrlm.voice_session->voice_stb_data_device_id_set(g_ctrlm.device_id);

   for(auto const &itr : g_ctrlm.networks) {
      itr.second->device_id_set(g_ctrlm.device_id);
   }
   ctrlm_main_has_device_id_set(true);
   return(true);
}
#endif

#ifdef AUTH_ACCOUNT_ID
gboolean ctrlm_main_has_service_account_id_get(void) {
   return(g_ctrlm.has_service_account_id);
}

void ctrlm_main_has_service_account_id_set(gboolean has_id) {
   g_ctrlm.has_service_account_id = has_id;
}

gboolean ctrlm_load_service_account_id(const char *account_id) {
   if(account_id == NULL) {
      if(!g_ctrlm.authservice->get_account_id(g_ctrlm.service_account_id)) {
         ctrlm_main_has_service_account_id_set(false);
         return(false);
      }
   } else {
      g_ctrlm.service_account_id = account_id;
   }
   g_ctrlm.voice_session->voice_stb_data_account_number_set(g_ctrlm.service_account_id);

   for(auto const &itr : g_ctrlm.networks) {
      itr.second->service_account_id_set(g_ctrlm.service_account_id);
   }
   ctrlm_main_has_service_account_id_set(true);
   return(true);
}
#endif

#ifdef AUTH_PARTNER_ID
gboolean ctrlm_main_has_partner_id_get(void) {
   return(g_ctrlm.has_partner_id);
}

void ctrlm_main_has_partner_id_set(gboolean has_id) {
   g_ctrlm.has_partner_id = has_id;
}

gboolean ctrlm_load_partner_id(void) {
   if(!g_ctrlm.authservice->get_partner_id(g_ctrlm.partner_id)) {
      ctrlm_main_has_partner_id_set(false);
      return(false);
   }
   g_ctrlm.voice_session->voice_stb_data_partner_id_set(g_ctrlm.partner_id);

   for(auto const &itr : g_ctrlm.networks) {
      itr.second->partner_id_set(g_ctrlm.partner_id);
   }
   ctrlm_main_has_partner_id_set(true);
   return(true);
}
#endif

#ifdef AUTH_EXPERIENCE
gboolean ctrlm_main_has_experience_get(void) {
   return(g_ctrlm.has_experience);
}

void ctrlm_main_has_experience_set(gboolean has_experience) {
   g_ctrlm.has_experience = has_experience;
}

gboolean ctrlm_load_experience(void) {
   if(!g_ctrlm.authservice->get_experience(g_ctrlm.experience)) {
      ctrlm_main_has_experience_set(false);
      return(false);
   }
   g_ctrlm.voice_session->voice_stb_data_experience_set(g_ctrlm.experience);

   for(auto const &itr : g_ctrlm.networks) {
      itr.second->experience_set(g_ctrlm.experience);
   }
   ctrlm_main_has_experience_set(true);
   return(true);
}
#endif

#ifdef AUTH_SAT_TOKEN
gboolean ctrlm_main_needs_service_access_token_get(void) {
   gboolean ret = false;
   sem_wait(&g_ctrlm.service_access_token_semaphore);
   if(g_ctrlm.sat_enabled) {
      ret = !g_ctrlm.has_service_access_token;
   }
   sem_post(&g_ctrlm.service_access_token_semaphore);
   return ret;
}

void ctrlm_main_has_service_access_token_set(gboolean has_token) {
   sem_wait(&g_ctrlm.service_access_token_semaphore);
   g_ctrlm.has_service_access_token = has_token;
   sem_post(&g_ctrlm.service_access_token_semaphore);
}

gboolean ctrlm_load_service_access_token(void) {
   if(!g_ctrlm.authservice->get_sat(g_ctrlm.service_access_token, g_ctrlm.service_access_token_expiration)) {
      ctrlm_main_has_service_access_token_set(false);
      return(false);
   }
   time_t current = time(NULL);
   if(g_ctrlm.service_access_token_expiration - current <= 0) {
      LOG_WARN("%s: SAT Token retrieved is already expired...\n", __FUNCTION__);
      ctrlm_main_has_service_access_token_set(false);
      return(false);
   }

   ctrlm_main_has_service_access_token_set(true);
   LOG_INFO("%s: SAT Token retrieved and expires in %ld seconds\n", __FUNCTION__, g_ctrlm.service_access_token_expiration - current);
   LOG_DEBUG("%s: <%s>\n", __FUNCTION__, g_ctrlm.service_access_token.c_str());
   g_ctrlm.voice_session->voice_stb_data_sat_set(g_ctrlm.service_access_token);

   if(!g_ctrlm.authservice->supports_sat_expiration()) {
      time_t timeout = g_ctrlm.service_access_token_expiration - current;
      LOG_INFO("%s: Setting SAT Timer for %ld seconds\n", __FUNCTION__, timeout);
      g_ctrlm.service_access_token_expiration_tag = ctrlm_timeout_create(timeout * 1000, ctrlm_authservice_expired, NULL);
   }
   return(true);
}
#endif
#endif

gboolean ctrlm_has_authservice_data(void) {
   gboolean ret = TRUE;
#ifdef AUTH_ENABLED
#ifdef AUTH_RECEIVER_ID
   if(!ctrlm_main_has_receiver_id_get()) {
      ret = FALSE;
   }
#endif

#ifdef AUTH_DEVICE_ID
   if(!ctrlm_main_has_device_id_get()) {
      ret = FALSE;
   }
#endif

#ifdef AUTH_ACCOUNT_ID
   if(!ctrlm_main_has_service_account_id_get()) {
      ret = FALSE;
   }
#endif

#ifdef AUTH_PARTNER_ID
   if(!ctrlm_main_has_partner_id_get()) {
      ret = FALSE;
   }
#endif

#ifdef AUTH_EXPERIENCE
   if(!ctrlm_main_has_experience_get()) {
      ret = FALSE;
   }
#endif

#ifdef AUTH_SAT_TOKEN
   if(ctrlm_main_needs_service_access_token_get()) {
      ret = FALSE;
   }
#endif
#endif

   return(ret);
}

gboolean ctrlm_load_authservice_data(void) {
   gboolean ret = TRUE;
#ifdef AUTH_ENABLED
   if(g_ctrlm.authservice->is_ready()) {
#ifdef AUTH_RECEIVER_ID
   if(!ctrlm_main_has_receiver_id_get()) {
      LOG_INFO("%s: load receiver id\n", __FUNCTION__);
      if(!ctrlm_load_receiver_id()) {
         LOG_WARN("%s: failed to load receiver id\n", __FUNCTION__);
         ret = FALSE;
      } else {
         LOG_INFO("%s: load receiver id successfully <%s>\n", __FUNCTION__, ctrlm_is_pii_mask_enabled() ? "***" : g_ctrlm.receiver_id.c_str());
      }
   }
#endif

#ifdef AUTH_DEVICE_ID
   if(!ctrlm_main_has_device_id_get()) {
      LOG_INFO("%s: load device id\n", __FUNCTION__);
      if(!ctrlm_load_device_id()) {
         LOG_WARN("%s: failed to load device id\n", __FUNCTION__);
         ret = FALSE;
      } else {
         LOG_INFO("%s: load device id successfully <%s>\n", __FUNCTION__, ctrlm_is_pii_mask_enabled() ? "***" : g_ctrlm.device_id.c_str());
      }
   }
#endif

#ifdef AUTH_ACCOUNT_ID
   if(!ctrlm_main_has_service_account_id_get()) {
      LOG_INFO("%s: load account id\n", __FUNCTION__);
      if(!ctrlm_load_service_account_id(NULL)) {
         LOG_WARN("%s: failed to load account id\n", __FUNCTION__);
         ret = FALSE;
      } else {
         LOG_INFO("%s: load account id successfully <%s>\n", __FUNCTION__, ctrlm_is_pii_mask_enabled() ? "***" : g_ctrlm.service_account_id.c_str());
      }
   }
#endif

#ifdef AUTH_PARTNER_ID
   if(!ctrlm_main_has_partner_id_get()) {
      LOG_INFO("%s: load partner id\n", __FUNCTION__);
      if(!ctrlm_load_partner_id()) {
         LOG_WARN("%s: failed to load partner id\n", __FUNCTION__);
         ret = FALSE;
      } else {
         LOG_INFO("%s: load partner id successfully <%s>\n", __FUNCTION__, ctrlm_is_pii_mask_enabled() ? "***" : g_ctrlm.partner_id.c_str());
      }
   }
#endif

#ifdef AUTH_EXPERIENCE
   if(!ctrlm_main_has_experience_get()) {
      LOG_INFO("%s: load experience\n", __FUNCTION__);
      if(!ctrlm_load_experience()) {
         LOG_WARN("%s: failed to load experience\n", __FUNCTION__);
         ret = FALSE;
      } else {
         LOG_INFO("%s: load experience successfully <%s>\n", __FUNCTION__, ctrlm_is_pii_mask_enabled() ? "***" : g_ctrlm.experience.c_str());
      }
   }
#endif

#ifdef AUTH_SAT_TOKEN
   if(ctrlm_main_needs_service_access_token_get()) {
      LOG_INFO("%s: load sat token\n", __FUNCTION__);
      if(!ctrlm_load_service_access_token()) {
         LOG_WARN("%s: failed to load sat token\n", __FUNCTION__);
         if(!g_ctrlm.authservice->supports_sat_expiration()) { // Do not continue to poll just for SAT if authservice supports onChange event
            ret = FALSE;
         }
      } else {
         LOG_INFO("%s: load sat token successfully\n", __FUNCTION__);
      }
   }
#endif
   } else {
      LOG_WARN("%s: Authservice is not ready...\n", __FUNCTION__);
   }
#endif

   return(ret);
}

gboolean ctrlm_load_config(json_t **json_obj_root, json_t **json_obj_net_rf4ce, json_t **json_obj_voice, json_t **json_obj_device_update, json_t **json_obj_validation, json_t **json_obj_vsdk) {
   std::string config_fn_opt = "/opt/ctrlm_config.json";
   std::string config_fn_etc = "/etc/ctrlm_config.json";
   json_t *json_obj_ctrlm;
   ctrlm_config_t *ctrlm_config = ctrlm_config_t::get_instance();
   gboolean local_conf = false;
   
   LOG_INFO("%s\n", __FUNCTION__);

   if(ctrlm_config == NULL) {
      LOG_ERROR("%s: Failed to get config manager instance\n", __FUNCTION__);
      return(false);
   } else if(!ctrlm_is_production_build() && g_file_test(config_fn_opt.c_str(), G_FILE_TEST_EXISTS) && ctrlm_config->load_config(config_fn_opt)) {
      LOG_INFO("%s: Read configuration from <%s>\n", __FUNCTION__, config_fn_opt.c_str());
      local_conf = true;
   } else if(g_file_test(config_fn_etc.c_str(), G_FILE_TEST_EXISTS) && ctrlm_config->load_config(config_fn_etc)) {
      LOG_INFO("%s: Read configuration from <%s>\n", __FUNCTION__, config_fn_etc.c_str());
   } else {
      LOG_WARN("%s: Configuration error. Configuration file(s) missing, using defaults\n", __FUNCTION__);
      return(false);
   }

   // Parse the JSON data
   *json_obj_root = ctrlm_config->json_from_path("", true); // Get root AND add ref to it, since this code derefs it
   if(*json_obj_root == NULL) {
      LOG_ERROR("%s: JSON object from config manager is NULL\n", __FUNCTION__);
      return(false);
   } else if(!json_is_object(*json_obj_root)) {
      // invalid response data
      LOG_INFO("%s: received invalid json response data - not a json object\n", __FUNCTION__);
      json_decref(*json_obj_root);
      *json_obj_root = NULL;
      return(false);
   }

   // Extract the RF4CE network configuration object
   *json_obj_net_rf4ce = json_object_get(*json_obj_root, JSON_OBJ_NAME_NETWORK_RF4CE);
   if(*json_obj_net_rf4ce == NULL || !json_is_object(*json_obj_net_rf4ce)) {
      LOG_WARN("%s: RF4CE network object not found\n", __FUNCTION__);
   }

   // Extract the voice configuration object
   *json_obj_voice = json_object_get(*json_obj_root, JSON_OBJ_NAME_VOICE);
   if(*json_obj_voice == NULL || !json_is_object(*json_obj_voice)) {
      LOG_WARN("%s: voice object not found\n", __FUNCTION__);
   }

   // Extract the device update configuration object
   *json_obj_device_update = json_object_get(*json_obj_root, JSON_OBJ_NAME_DEVICE_UPDATE);
   if(*json_obj_device_update == NULL || !json_is_object(*json_obj_device_update)) {
      LOG_WARN("%s: device update object not found\n", __FUNCTION__);
   }

  //Extract the vsdk configuration object
   *json_obj_vsdk = json_object_get( *json_obj_root, JSON_OBJ_NAME_VSDK);
   if(*json_obj_vsdk == NULL || !json_is_object(*json_obj_vsdk)) {
      LOG_WARN("%s: vsdk object not found\n", __FUNCTION__);
      json_obj_vsdk = NULL;
   }

   // Extract the ctrlm global configuration object
   json_obj_ctrlm = json_object_get(*json_obj_root, JSON_OBJ_NAME_CTRLM_GLOBAL);
   if(json_obj_ctrlm == NULL || !json_is_object(json_obj_ctrlm)) {
      LOG_WARN("%s: control manger object not found\n", __FUNCTION__);
   } else {
      json_config conf_global;
      if(conf_global.config_object_set(json_obj_ctrlm)) {
         LOG_ERROR("%s: unable to set config object\n", __FUNCTION__);
      }

      // Extract the validation configuration object
      *json_obj_validation = json_object_get(json_obj_ctrlm, JSON_OBJ_NAME_CTRLM_GLOBAL_VALIDATION_CONFIG);
      if(*json_obj_validation == NULL || !json_is_object(*json_obj_validation)) {
         LOG_WARN("%s: validation object not found\n", __FUNCTION__);
      }

      // Now parse the control manager object
      json_t *json_obj = json_object_get(json_obj_ctrlm, JSON_STR_NAME_CTRLM_GLOBAL_DB_PATH);
      const char *text = "Database Path";
      if(json_obj != NULL && json_is_string(json_obj)) {
         LOG_INFO("%s: %-24s - PRESENT <%s>\n", __FUNCTION__, text, json_string_value(json_obj));
         g_ctrlm.db_path = json_string_value(json_obj);
      } else {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_STR_NAME_CTRLM_GLOBAL_MINIDUMP_PATH);
      text = "Minidump Path";
      if(json_obj != NULL && json_is_string(json_obj)) {
         LOG_INFO("%s: %-24s - PRESENT <%s>\n", __FUNCTION__, text, json_string_value(json_obj));
         g_ctrlm.minidump_path = json_string_value(json_obj);
      } else {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_THREAD_MONITOR_PERIOD);
      text = "Thread Monitor Period";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 1) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.thread_monitor_timeout_val = value * 1000;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_BOOL_NAME_CTRLM_GLOBAL_THREAD_MONITOR_MINIDUMP);
      text     = "Thread Monitor Minidump";
      if(json_obj != NULL && json_is_boolean(json_obj)) {
         LOG_INFO("%s: %-28s - PRESENT <%s>\n", __FUNCTION__, text, json_is_true(json_obj) ? "true" : "false");
         if(json_is_true(json_obj)) {
            g_ctrlm.thread_monitor_minidump = true;
         } else {
            g_ctrlm.thread_monitor_minidump = false;
         }
      } else {
         LOG_INFO("%s: %-28s - ABSENT\n", __FUNCTION__, text);
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_STR_NAME_CTRLM_GLOBAL_URL_AUTH_SERVICE);
      text     = "Auth Service URL";
      if(json_obj == NULL || !json_is_string(json_obj)) {
         LOG_INFO("%s: %-25s - ABSENT\n", __FUNCTION__, text);
      } else {
         LOG_INFO("%s: %-25s - PRESENT <%s>\n", __FUNCTION__, text, json_string_value(json_obj));
         g_ctrlm.server_url_authservice = json_string_value(json_obj);
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_AUTHSERVICE_POLL_PERIOD);
      text = "Authservice Poll Period";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 1) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.authservice_poll_val = value * 1000;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_AUTHSERVICE_FAST_POLL_PERIOD);
      text = "Authservice Fast Poll Period";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 1) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.authservice_fast_poll_val = value;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_AUTHSERVICE_FAST_MAX_RETRIES);
      text = "Authservice Fast Max Retries";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 1) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.authservice_fast_retries_max = value;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_KEYCODE_LOGGING_POLL_PERIOD);
      text = "Keycode Logging Poll Period";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 1) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.keycode_logging_poll_val = value * 1000;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_KEYCODE_LOGGING_MAX_RETRIES);
      text = "Keycode Logging Max Retries";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 1) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.keycode_logging_max_retries = value;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_RECENTLY_BOOTED);
      text = "Timeout Recently Booted";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 0) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.recently_booted_timeout_val = value;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_LINE_OF_SIGHT);
      text     = "Timeout Line of Sight";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 0) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.line_of_sight_timeout_val = value;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_AUTOBIND);
      text     = "Timeout Autobind";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 0) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.autobind_timeout_val = value;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_BUTTON_BINDING);
      text     = "Timeout Button Binding";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 0) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.binding_button_timeout_val = value;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_SCREEN_BIND);
      text     = "Timeout Screen Bind";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 0) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.screen_bind_timeout_val = value;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_ONE_TOUCH_AUTOBIND);
      text     = "Timeout One Touch Autobind";
      if(json_obj == NULL || !json_is_integer(json_obj)) {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      } else {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 0) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.one_touch_autobind_timeout_val = value;
         }
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_BOOL_NAME_CTRLM_GLOBAL_MASK_KEY_CODES);
      text     = "Mask Key Codes";
      if(json_obj != NULL && json_is_boolean(json_obj)) {
         LOG_INFO("%s: %-28s - PRESENT <%s>\n", __FUNCTION__, text, json_is_true(json_obj) ? "true" : "false");
         if(json_is_true(json_obj)) {
            g_ctrlm.mask_key_codes_json = true;
         } else {
            g_ctrlm.mask_key_codes_json = false;
         }
      } else {
         LOG_INFO("%s: %-28s - ABSENT\n", __FUNCTION__, text);
      }

      text     = "Mask PII";
      if(conf_global.config_value_get(JSON_ARRAY_NAME_CTRLM_GLOBAL_MASK_PII, g_ctrlm.mask_pii, ctrlm_is_production_build() ? CTRLM_JSON_ARRAY_INDEX_PRD : CTRLM_JSON_ARRAY_INDEX_DEV)) {
         LOG_INFO("%s: %-28s - PRESENT <%s>\n", __FUNCTION__, text, g_ctrlm.mask_pii ? "true" : "false");
      } else {
         LOG_INFO("%s: %-28s - ABSENT\n", __FUNCTION__, text);
      }
      json_obj = json_object_get(json_obj_ctrlm, JSON_INT_NAME_CTRLM_GLOBAL_CRASH_RECOVERY_THRESHOLD);
      text     = "Crash Recovery Threshold";
      if(json_obj != NULL && json_is_integer(json_obj)) {
         json_int_t value = json_integer_value(json_obj);
         LOG_INFO("%s: %-24s - PRESENT <%lld>\n", __FUNCTION__, text, value);
         if(value < 0) {
            LOG_INFO("%s: %-24s - OUT OF RANGE %lld\n", __FUNCTION__, text, value);
         } else {
            g_ctrlm.crash_recovery_threshold = value;
         }
      } else {
         LOG_INFO("%s: %-28s - ABSENT\n", __FUNCTION__, text);
      }

#if defined(AUTH_ENABLED) && defined(AUTH_DEVICE_ID)
      json_obj = json_object_get(json_obj_ctrlm, JSON_STR_NAME_CTRLM_GLOBAL_DEVICE_ID);
      text = "Device ID";
      if(json_obj != NULL && json_is_string(json_obj) && (strlen(json_string_value(json_obj)) != 0)) {
         LOG_INFO("%s: %-24s - PRESENT <%s>\n", __FUNCTION__, text, json_string_value(json_obj));
         g_ctrlm.device_id = json_string_value(json_obj);
         ctrlm_main_has_device_id_set(true);
      } else {
         LOG_INFO("%s: %-24s - ABSENT\n", __FUNCTION__, text);
      }
#endif
   }

   LOG_INFO("%s: Database Path                <%s>\n",  __FUNCTION__, g_ctrlm.db_path.c_str());
   LOG_INFO("%s: Minidump Path                <%s>\n",  __FUNCTION__, g_ctrlm.minidump_path.c_str());
   LOG_INFO("%s: Thread Monitor Period        %u ms\n", __FUNCTION__, g_ctrlm.thread_monitor_timeout_val);
   LOG_INFO("%s: Authservice Poll Period      %u ms\n", __FUNCTION__, g_ctrlm.authservice_poll_val);
   LOG_INFO("%s: Keycode Logging Poll Period  %u ms\n", __FUNCTION__, g_ctrlm.keycode_logging_poll_val);
   LOG_INFO("%s: Keycode Logging Max Retries  <%u>\n",  __FUNCTION__, g_ctrlm.keycode_logging_max_retries);
   LOG_INFO("%s: Timeout Recently Booted      %u ms\n", __FUNCTION__, g_ctrlm.recently_booted_timeout_val);
   LOG_INFO("%s: Timeout Line of Sight        %u ms\n", __FUNCTION__, g_ctrlm.line_of_sight_timeout_val);
   LOG_INFO("%s: Timeout Autobind             %u ms\n", __FUNCTION__, g_ctrlm.autobind_timeout_val);
   LOG_INFO("%s: Timeout Screen Bind          %u ms\n", __FUNCTION__, g_ctrlm.screen_bind_timeout_val);
   LOG_INFO("%s: Timeout One Touch Autobind   %u ms\n", __FUNCTION__, g_ctrlm.one_touch_autobind_timeout_val);
   LOG_INFO("%s: Mask Key Codes               <%s>\n",  __FUNCTION__, g_ctrlm.mask_key_codes_json ? "YES" : "NO");
   LOG_INFO("%s: Mask PII                     <%s>\n",  __FUNCTION__, g_ctrlm.mask_pii ? "YES" : "NO");
   LOG_INFO("%s: Crash Recovery Threshold     <%u>\n",  __FUNCTION__, g_ctrlm.crash_recovery_threshold);
   LOG_INFO("%s: Auth Service URL             <%s>\n",  __FUNCTION__, g_ctrlm.server_url_authservice.c_str());

   g_ctrlm.local_conf = local_conf;

   return true;
}

void ctrlm_global_rfc_values_retrieved(const ctrlm_rfc_attr_t &attr) {
   // DB Path / Authservice URL / Device ID updates not supported via RFC
   // attr.get_rfc_value(JSON_STR_NAME_CTRLM_GLOBAL_DB_PATH, g_ctrlm.db_path);
   // attr.get_rfc_value(JSON_STR_NAME_CTRLM_GLOBAL_URL_AUTH_SERVICE, g_ctrlm.server_url_authservice);
   // attr.get_rfc_value(JSON_STR_NAME_CTRLM_GLOBAL_DEVICE_ID, g_ctrlm.device_id);
   if(attr.get_rfc_value(JSON_STR_NAME_CTRLM_GLOBAL_MINIDUMP_PATH, g_ctrlm.minidump_path)) {
      google_breakpad::MinidumpDescriptor descriptor(g_ctrlm.minidump_path.c_str());
      google_breakpad::ExceptionHandler eh(descriptor, NULL, ctrlm_minidump_callback, NULL, true, -1);
   }
   if(attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_THREAD_MONITOR_PERIOD, g_ctrlm.thread_monitor_timeout_val, 1)) {
      g_ctrlm.thread_monitor_timeout_val *= 1000;
   }
   attr.get_rfc_value(JSON_BOOL_NAME_CTRLM_GLOBAL_THREAD_MONITOR_MINIDUMP, g_ctrlm.thread_monitor_minidump);
   if(attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_AUTHSERVICE_POLL_PERIOD, g_ctrlm.authservice_poll_val, 1)) {
      g_ctrlm.authservice_poll_val *= 1000;
   }
   if(attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_KEYCODE_LOGGING_POLL_PERIOD, g_ctrlm.keycode_logging_poll_val, 1)) {
      g_ctrlm.keycode_logging_poll_val *= 1000;
   }
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_KEYCODE_LOGGING_MAX_RETRIES, g_ctrlm.keycode_logging_max_retries, 1);
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_RECENTLY_BOOTED, g_ctrlm.recently_booted_timeout_val, 1);
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_LINE_OF_SIGHT, g_ctrlm.line_of_sight_timeout_val, 0);
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_AUTOBIND, g_ctrlm.autobind_timeout_val, 0);
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_BUTTON_BINDING, g_ctrlm.binding_button_timeout_val, 0);
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_SCREEN_BIND, g_ctrlm.screen_bind_timeout_val, 0);
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_TIMEOUT_ONE_TOUCH_AUTOBIND, g_ctrlm.one_touch_autobind_timeout_val, 0);
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_CRASH_RECOVERY_THRESHOLD, g_ctrlm.crash_recovery_threshold, 0);
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_KEYCODE_LOGGING_MAX_RETRIES, g_ctrlm.keycode_logging_max_retries, 1);
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_KEYCODE_LOGGING_MAX_RETRIES, g_ctrlm.keycode_logging_max_retries, 1);
   attr.get_rfc_value(JSON_BOOL_NAME_CTRLM_GLOBAL_MASK_KEY_CODES, g_ctrlm.mask_key_codes_json);
   if(attr.get_rfc_value(JSON_ARRAY_NAME_CTRLM_GLOBAL_MASK_PII, g_ctrlm.mask_pii, ctrlm_is_production_build() ? CTRLM_JSON_ARRAY_INDEX_PRD : CTRLM_JSON_ARRAY_INDEX_DEV)) {
      g_ctrlm.voice_session->voice_stb_data_pii_mask_set(g_ctrlm.mask_pii);
   }
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_AUTHSERVICE_POLL_PERIOD, g_ctrlm.authservice_poll_val);   
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_AUTHSERVICE_FAST_POLL_PERIOD, g_ctrlm.authservice_fast_poll_val);
   attr.get_rfc_value(JSON_INT_NAME_CTRLM_GLOBAL_AUTHSERVICE_FAST_MAX_RETRIES, g_ctrlm.authservice_fast_retries_max);   
}

gboolean ctrlm_iarm_init(void) {
   IARM_Result_t result;

   // Initialize the IARM Bus
   result = IARM_Bus_Init(CTRLM_MAIN_IARM_BUS_NAME);
   if(IARM_RESULT_SUCCESS != result) {
      LOG_FATAL("%s: Unable to initialize IARM bus!\n", __FUNCTION__);
      return(false);
   }

   // Connect to IARM Bus
   result = IARM_Bus_Connect();
   if(IARM_RESULT_SUCCESS != result) {
      LOG_FATAL("%s: Unable to connect IARM bus!\n", __FUNCTION__);
      return(false);
   }

   // Register all events that can be generated by Control Manager
   result = IARM_Bus_RegisterEvent(CTRLM_MAIN_IARM_EVENT_MAX);
   if(IARM_RESULT_SUCCESS != result) {
      LOG_FATAL("%s: Unable to register events!\n", __FUNCTION__);
      return(false);
   }

   return(true);
}


void ctrlm_iarm_terminate(void) {
   IARM_Result_t result;

   // Disconnect from IARM Bus
   result = IARM_Bus_Disconnect();
   if(IARM_RESULT_SUCCESS != result) {
      LOG_FATAL("%s: Unable to disconnect IARM bus!\n", __FUNCTION__);
   }

   // Terminate the IARM Bus
   #if 0 // TODO may need to enable this when running in the RDK
   result = IARM_Bus_Term();
   if(IARM_RESULT_SUCCESS != result) {
      LOG_FATAL("%s: Unable to terminate IARM bus!\n", __FUNCTION__);
   }
   #endif
}

ctrlm_network_id_t network_id_get_next(ctrlm_network_type_t network_type) {
   ctrlm_network_id_t network_id = CTRLM_MAIN_NETWORK_ID_ALL;

   static ctrlm_network_id_t network_id_rf4ce  = NETWORK_ID_BASE_RF4CE;
   static ctrlm_network_id_t network_id_ip     = NETWORK_ID_BASE_IP;
   static ctrlm_network_id_t network_id_ble    = NETWORK_ID_BASE_BLE;
   static ctrlm_network_id_t network_id_custom = NETWORK_ID_BASE_CUSTOM;
   switch(network_type) {
      case CTRLM_NETWORK_TYPE_RF4CE:
         network_id = network_id_rf4ce++;
         if(NETWORK_ID_BASE_IP == network_id)
         {
             LOG_WARN("%s: RF4CE network ID wraparound\n", __FUNCTION__ );
             network_id = NETWORK_ID_BASE_RF4CE;
         }
         break;
      case CTRLM_NETWORK_TYPE_IP:
         network_id = network_id_ip++;
         if(NETWORK_ID_BASE_BLE == network_id)
         {
             LOG_WARN("%s: IP network ID wraparound\n", __FUNCTION__ );
             network_id = NETWORK_ID_BASE_IP;
         }
         break;
      case CTRLM_NETWORK_TYPE_BLUETOOTH_LE:
         network_id = network_id_ble++;
         if(NETWORK_ID_BASE_CUSTOM == network_id)
         {
             LOG_WARN("%s: BLE network ID wraparound\n", __FUNCTION__ );
             network_id = NETWORK_ID_BASE_BLE;
         }
         break;
      case CTRLM_NETWORK_TYPE_DSP:
          network_id = CTRLM_MAIN_NETWORK_ID_DSP;
      default:
         network_id = network_id_custom++;
         if(CTRLM_MAIN_NETWORK_ID_DSP == network_id_custom)
         {
             LOG_WARN("%s: Custom network ID wraparound\n", __FUNCTION__ );
             network_id_custom = NETWORK_ID_BASE_CUSTOM;
         }
         break;
   }
   return(network_id);
}

extern ctrlm_obj_network_t* create_ctrlm_obj_network_t(ctrlm_network_type_t type, ctrlm_network_id_t id, const char *name, gboolean mask_key_codes, json_t *json_obj_net_ip, GThread *original_thread);

gboolean ctrlm_networks_pre_init(json_t *json_obj_net_rf4ce, json_t *json_config_root) {
   #ifdef CTRLM_NETWORK_RF4CE
   ctrlm_network_id_t network_id;
   network_id = network_id_get_next(CTRLM_NETWORK_TYPE_RF4CE);

   ctrlm_obj_network_rf4ce_t *obj_net_rf4ce = new ctrlm_obj_network_rf4ce_t(CTRLM_NETWORK_TYPE_RF4CE, network_id, "RF4CE", g_ctrlm.mask_key_codes_json, json_obj_net_rf4ce, g_thread_self());
   // Set main function for the RF4CE Network object
   obj_net_rf4ce->hal_api_main_set(ctrlm_hal_rf4ce_main);
   g_ctrlm.networks[network_id]      = obj_net_rf4ce;
   //g_ctrlm.networks[network_id].net.rf4ce = obj_net_rf4ce;
   g_ctrlm.network_type[network_id]       = g_ctrlm.networks[network_id]->type_get();
   #endif

   unsigned long ignore_mask = 0;
//   if (!g_ctrlm.ip_network_enabled) {
//      ignore_mask |= 1 << CTRLM_NETWORK_TYPE_IP;
//   }
   networks_map_t networks_map;
   ctrlm_vendor_network_factory(ignore_mask, json_config_root, networks_map);


   auto networks_map_end = networks_map.end();
   for (auto net = networks_map.begin(); net != networks_map_end; ++net) {
      g_ctrlm.networks[net->first]   = net->second;
      g_ctrlm.network_type[net->first]    = g_ctrlm.networks[net->first]->type_get();
   }

   for(auto const &itr : g_ctrlm.networks) {
      itr.second->stb_name_set(g_ctrlm.stb_name);
      itr.second->mask_key_codes_set(ctrlm_is_production_build() ? true : g_ctrlm.mask_key_codes_json);
   }

   return(true);
}

gboolean ctrlm_networks_init(void) {
   //ctrlm_crash();

   // Initialize all the network interfaces
   for(auto const &itr : g_ctrlm.networks) {
      // Initialize the network
      ctrlm_hal_result_t result = itr.second->network_init(g_ctrlm.main_thread);

      if(CTRLM_HAL_RESULT_SUCCESS != result) { // Error occurred
         // For now, we only care about rf4ce
         if((itr.second->type_get() == CTRLM_NETWORK_TYPE_RF4CE || !itr.second->is_failed_state()) || itr.second->type_get() == CTRLM_NETWORK_TYPE_BLUETOOTH_LE) {
             return(false);
         }
      }
   }

   // Read Control Service values from DB
   control_service_values_read_from_db();

   return(true);
}

void ctrlm_networks_terminate(void) {
   g_ctrlm.successful_init = false;
   for(auto const &itr : g_ctrlm.networks) {
      LOG_INFO("%s: Terminating %s network\n", __FUNCTION__, itr.second->name_get());

      itr.second->network_destroy();

      // Call destructor
      delete itr.second;
   }
}

void ctrlm_network_list_get(vector<ctrlm_network_id_t> *list) {
   if(list == NULL || list->size() != 0) {
      LOG_WARN("%s: Invalid list.\n", __FUNCTION__);
      return;
   }
   vector<ctrlm_network_id_t>::iterator it_vector;
   it_vector = list->begin();

   for(auto const &itr : g_ctrlm.networks) {
      LOG_INFO("%s: Network id %u\n", __FUNCTION__, itr.first);
      it_vector = list->insert(it_vector, itr.first);
   }
}

// Determine if the network id is valid
gboolean ctrlm_network_id_is_valid(ctrlm_network_id_t network_id) {
   if(g_ctrlm.networks.count(network_id) == 0) {
      if(network_id != CTRLM_MAIN_NETWORK_ID_DSP) {
         LOG_ERROR("%s: Invalid Network Id %u\n", __FUNCTION__, network_id);
      }
      return(false);
   }
   return(true);
}

gboolean ctrlm_controller_id_is_valid(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) {
   gboolean ret = false;

   if(ctrlm_network_id_is_valid(network_id)) {
      if(g_ctrlm.main_thread == g_thread_self()) {
         LOG_DEBUG("%s: main thread\n", __FUNCTION__);
         ret = g_ctrlm.networks[network_id]->controller_exists(controller_id);
      } else {
         ctrlm_main_queue_msg_controller_exists_t dqm = {0};
         sem_t semaphore;
         sem_init(&semaphore, 0, 0);

         dqm.controller_id     = controller_id;
         dqm.semaphore         = &semaphore;
         dqm.controller_exists = &ret;

         ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::controller_exists, &dqm, sizeof(dqm), NULL, network_id);

         sem_wait(&semaphore);
         sem_destroy(&semaphore);
      }
   }
   return ret;
}

// Return the network type
ctrlm_network_type_t ctrlm_network_type_get(ctrlm_network_id_t network_id) {
   if(!ctrlm_network_id_is_valid(network_id)) {
      if(network_id != CTRLM_MAIN_NETWORK_ID_DSP) {
         LOG_ERROR("%s: Invalid Network Id\n", __FUNCTION__);
      }
      return(CTRLM_NETWORK_TYPE_INVALID);
   }
   return(g_ctrlm.network_type[network_id]);
}

ctrlm_network_id_t ctrlm_network_id_get(ctrlm_network_type_t network_type) {
   auto pred = [network_type](const std::pair<ctrlm_network_id_t, ctrlm_network_type_t>& entry) {return network_type == entry.second;};
   auto networks_type_end = g_ctrlm.network_type.cend();
   auto network_type_entry = std::find_if(g_ctrlm.network_type.cbegin(), networks_type_end, pred);
   if (network_type_entry != networks_type_end) {
      return network_type_entry->first;
   }
   return(CTRLM_NETWORK_TYPE_INVALID);
}

gboolean ctrlm_precomission_lookup(ctrlm_network_id_t network_id, ctrlm_controller_id_t controller_id) {
   return(g_ctrlm.precomission_table.count(controller_id));
}
gboolean ctrlm_is_recently_booted(void) {
   return(g_ctrlm.recently_booted);
}

gboolean ctrlm_is_line_of_sight(void) {
   return(g_ctrlm.line_of_sight);
}

gboolean ctrlm_is_autobind_active(void) {
   return(g_ctrlm.autobind);
}

gboolean ctrlm_is_binding_button_pressed(void) {
   return(g_ctrlm.binding_button);
}

gboolean ctrlm_is_binding_screen_active(void) {
   return(g_ctrlm.binding_screen_active);
}

gboolean ctrlm_is_one_touch_autobind_active(void) {
   return(g_ctrlm.one_touch_autobind_active);
}

gboolean ctrlm_is_binding_table_empty(void) {
   return((g_ctrlm.bound_controller_qty > 0) ? FALSE : TRUE);
}

// Add a message to the control manager's processing queue
void ctrlm_main_queue_msg_push(gpointer msg) {
   g_async_queue_push(g_ctrlm.queue, msg);
}

void ctrlm_queue_msg_destroy(gpointer msg) {
   if(msg) {
      LOG_DEBUG("%s: Free %p\n", __FUNCTION__, msg);
      g_free(msg);
   }
}

void ctrlm_main_update_export_controller_list() {

   LOG_DEBUG("%s: entering\n", __FUNCTION__);
   device_update_check_locations_t update_locations_valid = ctrlm_device_update_check_locations_get();
   string xconf_path = ctrlm_device_update_get_xconf_path();

   // we create the list if the check location is set to xconf or both
   if (update_locations_valid == DEVICE_UPDATE_CHECK_FILESYSTEM) {
      LOG_WARN("%s: set for file system updates.  Do not process controller list\n", __FUNCTION__);
      return;
   }
   LOG_DEBUG("%s: doing xconf json create\n", __FUNCTION__);

   xconf_path += "/rc-proxy-params.json";

   json_t *controller_list = json_array();
   for(auto const &itr : g_ctrlm.networks) {
      json_t *temp = itr.second->xconf_export_controllers();
      if(temp && json_is_array(temp)) {
         json_array_extend(controller_list, temp);
      } else {
         LOG_WARN("%s: Network %d did not supply a valid controller list\n", __FUNCTION__, itr.first);
      }
   }
   // CUSTOM FORMATTING CODE
   if(json_array_size(controller_list) > 0) {
      std::string output = "";

      // Work with string now
      output += "[\n";
      for(unsigned int i = 0; i < json_array_size(controller_list); i++) {
         char *buf = json_dumps(json_array_get(controller_list, i), JSON_PRESERVE_ORDER | JSON_INDENT(0) | JSON_COMPACT);
         if(buf) {
            output += buf;
            if(i != json_array_size(controller_list)-1) {
               output += ",";
            }
            output += "\n";
         }
      }
      output += "]";
      if(false == g_file_set_contents(xconf_path.c_str(), output.c_str(), output.length(), NULL)) {
         LOG_ERROR("%s: Failed to dump xconf controller list\n", __FUNCTION__);
      }
   } else {
      LOG_INFO("%s: No controller information for XCONF\n", __FUNCTION__);
   }

   // END CUSTOM FORMATTING CODE
   json_decref(controller_list);

   LOG_DEBUG("%s: exiting\n", __FUNCTION__);

}

void ctrlm_main_update_check_update_complete_all(ctrlm_main_queue_msg_update_file_check_t *msg) {

   LOG_DEBUG("%s: entering\n", __FUNCTION__);
#ifdef CTRLM_NETWORK_RF4CE
   try {
      for(auto const &itr : g_ctrlm.networks) {
         ctrlm_obj_network_t *obj_net = itr.second;
         ctrlm_network_type_t network_type = obj_net->type_get();
         LOG_INFO("%s: network %s\n", __FUNCTION__, obj_net->name_get());
         if(network_type == CTRLM_NETWORK_TYPE_RF4CE) {
            ((ctrlm_obj_network_rf4ce_t *)obj_net)->check_if_update_file_still_needed(msg);
         }
      }

   } catch (exception& e) {
      LOG_ERROR("%s: exception %s\n", __FUNCTION__, e.what());
   }
#endif
   LOG_DEBUG("%s: exiting\n", __FUNCTION__);

}

gpointer ctrlm_main_thread(gpointer param) {
   bool running = true;
   LOG_INFO("%s: Started\n", __FUNCTION__);
   // Unblock the caller that launched this thread
   sem_post(&g_ctrlm.semaphore);

   LOG_INFO("%s: Enter main loop\n", __FUNCTION__);
   do {
      gpointer msg = g_async_queue_pop(g_ctrlm.queue);

      if(msg == NULL) {
         LOG_ERROR("%s: NULL message received\n", __FUNCTION__);
         continue;
      }

      ctrlm_main_queue_msg_header_t *hdr = (ctrlm_main_queue_msg_header_t *)msg;
      ctrlm_obj_network_t           *obj_net       = NULL;

      LOG_DEBUG("%s: Type <%s> Network Id %u\n", __FUNCTION__, ctrlm_main_queue_msg_type_str(hdr->type), hdr->network_id);
      if(0 == (hdr->type & CTRLM_MAIN_QUEUE_MSG_TYPE_GLOBAL)) { // Network specific message
         // This check has to occur before assigning the network objects,
         // since the object will be created if it doesn't exist already.
         if(!ctrlm_network_id_is_valid(hdr->network_id)) {
            LOG_ERROR("%s: INVALID Network Id! %u\n", __FUNCTION__, hdr->network_id);
            ctrlm_queue_msg_destroy(msg);
            continue;
         }

         obj_net       = g_ctrlm.networks[hdr->network_id];

         if (!obj_net->is_ready()) {
            LOG_ERROR("%s: Network %s not ready\n", __FUNCTION__, obj_net->name_get());
            ctrlm_queue_msg_destroy(msg);
            continue;
         }
      }

      switch(hdr->type) {
         case CTRLM_MAIN_QUEUE_MSG_TYPE_TICKLE: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_TICKLE\n", __FUNCTION__);
            ctrlm_thread_monitor_msg_t *thread_monitor_msg = (ctrlm_thread_monitor_msg_t *) msg;
            *thread_monitor_msg->response = CTRLM_THREAD_MONITOR_RESPONSE_ALIVE;
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_TERMINATE: {
            LOG_INFO("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_TERMINATE\n", __FUNCTION__);
            sem_post(&g_ctrlm.semaphore);
            running = false;
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_EXPORT_CONTROLLER_LIST: {
            LOG_DEBUG("%s: message type %s\n", __FUNCTION__, ctrlm_main_queue_msg_type_str(hdr->type));
            ctrlm_main_update_export_controller_list();
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_BIND_VALIDATION_BEGIN: {
            ctrlm_main_queue_msg_bind_validation_begin_t *dqm = (ctrlm_main_queue_msg_bind_validation_begin_t *)msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_BIND_VALIDATION_BEGIN\n", __FUNCTION__);

            ctrlm_validation_begin(hdr->network_id, dqm->controller_id, obj_net->ctrlm_controller_type_get(dqm->controller_id));
            obj_net->bind_validation_begin(dqm);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_BIND_VALIDATION_END: {
            ctrlm_main_queue_msg_bind_validation_end_t *dqm = (ctrlm_main_queue_msg_bind_validation_end_t *)msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_BIND_VALIDATION_END\n", __FUNCTION__);

            ctrlm_validation_end(hdr->network_id, dqm->controller_id, obj_net->ctrlm_controller_type_get(dqm->controller_id), dqm->binding_type, dqm->validation_type, dqm->result, dqm->cond, dqm->cmd_result);
            obj_net->bind_validation_end(dqm);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_BIND_VALIDATION_FAILED_TIMEOUT: {
              ctrlm_main_queue_msg_bind_validation_failed_timeout_t *dqm = (ctrlm_main_queue_msg_bind_validation_failed_timeout_t *)msg;
              LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_BIND_VALIDATION_FAILED_TIMEOUT\n", __FUNCTION__);
              obj_net->bind_validation_timeout(dqm->controller_id);
              if(g_ctrlm.pairing_window.active) {
                 ctrlm_pairing_window_bind_status_set_(CTRLM_BIND_STATUS_VALILDATION_FAILURE);
              }
              break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_BIND_CONFIGURATION_COMPLETE: {
            ctrlm_main_queue_msg_bind_configuration_complete_t *dqm = (ctrlm_main_queue_msg_bind_configuration_complete_t *)msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_BIND_CONFIGURATION_COMPLETE\n", __FUNCTION__);
            ctrlm_controller_status_t status;
            if(dqm->result == CTRLM_RCU_CONFIGURATION_RESULT_SUCCESS) {
               obj_net->ctrlm_controller_status_get(dqm->controller_id, &status);
            } else {
               errno_t safec_rc = memset_s(&status, sizeof(ctrlm_controller_status_t), 0, sizeof(ctrlm_controller_status_t));
               ERR_CHK(safec_rc);
            }

            ctrlm_configuration_complete(hdr->network_id, dqm->controller_id, obj_net->ctrlm_controller_type_get(dqm->controller_id), obj_net->ctrlm_binding_type_get(dqm->controller_id), &status, dqm->result);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_NETWORK_PROPERTY_SET: {
            ctrlm_main_queue_msg_network_property_set_t *dqm = (ctrlm_main_queue_msg_network_property_set_t *)msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_NETWORK_PROPERTY_SET\n", __FUNCTION__);
            LOG_INFO("%s: Setting network property <%s>\n", __FUNCTION__, ctrlm_hal_network_property_str(dqm->property));
            ctrlm_hal_result_t result = obj_net->property_set(dqm->property, dqm->value);
            if(result != CTRLM_HAL_RESULT_SUCCESS) {
                  LOG_ERROR("%s: Unable to set network property <%s>\n", __FUNCTION__, ctrlm_hal_network_property_str(dqm->property));
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STATUS: {
            ctrlm_main_queue_msg_main_status_t *dqm = (ctrlm_main_queue_msg_main_status_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STATUS\n", __FUNCTION__);
            ctrlm_main_iarm_call_status_get_(dqm->status);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_PROPERTY_SET: {
            ctrlm_main_queue_msg_main_property_t *dqm = (ctrlm_main_queue_msg_main_property_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_PROPERTY_SET\n", __FUNCTION__);
            ctrlm_main_iarm_call_property_set_(dqm->property);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_PROPERTY_GET: {
            ctrlm_main_queue_msg_main_property_t *dqm = (ctrlm_main_queue_msg_main_property_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_PROPERTY_GET\n", __FUNCTION__);
            ctrlm_main_iarm_call_property_get_(dqm->property);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_DISCOVERY_CONFIG_SET: {
            ctrlm_main_queue_msg_main_discovery_config_t *dqm = (ctrlm_main_queue_msg_main_discovery_config_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_DISCOVERY_CONFIG_SET\n", __FUNCTION__);
            ctrlm_main_iarm_call_discovery_config_set_(dqm->config);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_AUTOBIND_CONFIG_SET: {
            ctrlm_main_queue_msg_main_autobind_config_t *dqm = (ctrlm_main_queue_msg_main_autobind_config_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_AUTOBIND_CONFIG_SET\n", __FUNCTION__);
            ctrlm_main_iarm_call_autobind_config_set_(dqm->config);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_PRECOMMISSION_CONFIG_SET: {
            ctrlm_main_queue_msg_main_precommision_config_t *dqm = (ctrlm_main_queue_msg_main_precommision_config_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_PRECOMMISSION_CONFIG_SET\n", __FUNCTION__);
            ctrlm_main_iarm_call_precommission_config_set_(dqm->config);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_FACTORY_RESET: {
            ctrlm_main_queue_msg_main_factory_reset_t *dqm = (ctrlm_main_queue_msg_main_factory_reset_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_FACTORY_RESET\n", __FUNCTION__);
            ctrlm_main_iarm_call_factory_reset_(dqm->reset);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROLLER_UNBIND: {
            ctrlm_main_queue_msg_main_controller_unbind_t *dqm = (ctrlm_main_queue_msg_main_controller_unbind_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROLLER_UNBIND\n", __FUNCTION__);
            ctrlm_main_iarm_call_controller_unbind_(dqm->unbind);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_TIMEOUT_LINE_OF_SIGHT: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_TIMEOUT_LINE_OF_SIGHT\n", __FUNCTION__);
            g_ctrlm.line_of_sight             = FALSE;
            g_ctrlm.line_of_sight_timeout_tag = 0;
            ctrlm_main_iarm_event_binding_line_of_sight(FALSE);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_TIMEOUT_AUTOBIND: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_TIMEOUT_AUTOBIND\n", __FUNCTION__);
            g_ctrlm.autobind             = FALSE;
            g_ctrlm.autobind_timeout_tag = 0;
            ctrlm_main_iarm_event_autobind_line_of_sight(FALSE);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STOP_BINDING_BUTTON: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STOP_BINDING_BUTTON\n", __FUNCTION__);
            if (g_ctrlm.binding_button) {
               ctrlm_timeout_destroy(&g_ctrlm.binding_button_timeout_tag);
               g_ctrlm.binding_button = FALSE;
               ctrlm_main_iarm_event_binding_button(FALSE);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STOP_BINDING_SCREEN: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STOP_BINDING_SCREEN\n", __FUNCTION__);
            if (g_ctrlm.binding_screen_active) {
               ctrlm_timeout_destroy(&g_ctrlm.screen_bind_timeout_tag);
               g_ctrlm.binding_screen_active = FALSE;
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STOP_ONE_TOUCH_AUTOBIND: {
            ctrlm_stop_one_touch_autobind_(hdr->network_id);
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STOP_ONE_TOUCH_AUTOBIND\n", __FUNCTION__);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CLOSE_PAIRING_WINDOW: {
            ctrlm_main_queue_msg_close_pairing_window_t *dqm = (ctrlm_main_queue_msg_close_pairing_window_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CLOSE_PAIRING_WINDOW\n", __FUNCTION__);
            ctrlm_close_pairing_window_(dqm->network_id, dqm->reason);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_BIND_STATUS_SET: {
            ctrlm_main_queue_msg_pairing_window_bind_status_t *dqm = (ctrlm_main_queue_msg_pairing_window_bind_status_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_BIND_STATUS_SET\n", __FUNCTION__);
            ctrlm_pairing_window_bind_status_set_(dqm->bind_status);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_DISCOVERY_REMOTE_TYPE_SET: {
            ctrlm_main_queue_msg_discovery_remote_type_t *dqm = (ctrlm_main_queue_msg_discovery_remote_type_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_DISCOVERY_REMOTE_TYPE_SET\n", __FUNCTION__);
            ctrlm_discovery_remote_type_set_(dqm->remote_type_str);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_CHECK_UPDATE_FILE_NEEDED:
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_CHECK_UPDATE_FILE_NEEDED\n", __FUNCTION__);
            ctrlm_main_update_check_update_complete_all((ctrlm_main_queue_msg_update_file_check_t *)msg);
            break;
         case CTRLM_MAIN_QUEUE_MSG_TYPE_THREAD_MONITOR_POLL: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_THREAD_MONITOR_POLL\n", __FUNCTION__);
            ctrlm_thread_monitor_msg_t *thread_monitor_msg = (ctrlm_thread_monitor_msg_t *) msg;
            ctrlm_obj_network_t        *obj_net            = thread_monitor_msg->obj_network;
            obj_net->thread_monitor_poll(thread_monitor_msg->response);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_POWER_STATE_CHANGE: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_POWER_STATE_CHANGE\n", __FUNCTION__);
            ctrlm_main_queue_power_state_change_t *dqm = (ctrlm_main_queue_power_state_change_t *)msg;
            if(NULL == dqm) {
               LOG_ERROR("%s: Power State Change msg NULL\n", __FUNCTION__);
               break;
            }
            #ifdef ENABLE_DEEP_SLEEP
            //Deep Sleep may set Networked Standby, may need other power state modifications later
            if(dqm->system) {
                //If Power Manager has sent STANDBY, then ctrlm is either in ON or STANDBY and should remain so until next Power Manager message
               if(dqm->new_state == CTRLM_POWER_STATE_STANDBY) {
                    LOG_INFO("%s: system STANDBY, skip\n", __FUNCTION__);
                    dqm->new_state = g_ctrlm.power_state;
                }

                if(dqm->new_state == CTRLM_POWER_STATE_DEEP_SLEEP && ctrlm_main_iarm_networked_standby())  {
                    LOG_INFO("%s: deep sleep message set Network Standby flag\n", __FUNCTION__);
                    dqm->new_state = CTRLM_POWER_STATE_STANDBY;
                }
 
                //Sky EPG sends excess ON messages, filter them. Will not adversely affect other EPGs
                if(dqm->new_state == CTRLM_POWER_STATE_ON && g_ctrlm.power_state == CTRLM_POWER_STATE_STANDBY_VOICE_SESSION) {
                    LOG_INFO("%s: handling standby voice, ignore ON\n", __FUNCTION__);
                    dqm->new_state = g_ctrlm.power_state;
                }
            }
            #endif

            if(g_ctrlm.power_state == dqm->new_state) {
               LOG_INFO("%s: already in power state %s\n", __FUNCTION__, ctrlm_power_state_str(g_ctrlm.power_state));
               break;
            }

            dqm->old_state = g_ctrlm.power_state;
            g_ctrlm.power_state = dqm->new_state;

            LOG_INFO("%s: enter power state %s\n", __FUNCTION__, ctrlm_power_state_str(g_ctrlm.power_state));

            #ifdef ENABLE_DEEP_SLEEP
            /* We have a problem in that if a voice command fails and the EPG does not come up, then
             * the user will use the remote to bring up the EPG in which case we get a wake up message
             * again saying the wakeup reason was voice, because that was actually the last wake
             * from deep sleep reason. Using a unique power state to allow for correct transitions
             */
            if((dqm->old_state == CTRLM_POWER_STATE_STANDBY && dqm->new_state == CTRLM_POWER_STATE_ON && ctrlm_main_iarm_wakeup_reason_voice())) {
               g_ctrlm.power_state = CTRLM_POWER_STATE_STANDBY_VOICE_SESSION;
               g_ctrlm.voice_session->voice_standby_session_request();
               //voice will transition power state to ON after voice transaction
               break;
            }
            #endif

            // Set Power change in Networks
            for(auto const &itr : g_ctrlm.networks) {
               itr.second->power_state_change(dqm);
            }

            // Set Power change in DB
            ctrlm_db_power_state_change(dqm);

            g_ctrlm.voice_session->voice_power_state_change(g_ctrlm.power_state);

            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_AUTHSERVICE_POLL: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_AUTHSERVICE_POLL\n", __FUNCTION__);
            ctrlm_main_queue_msg_authservice_poll_t *dqm = (ctrlm_main_queue_msg_authservice_poll_t *) msg;

            if(!ctrlm_load_authservice_data()) {
               if(dqm->ret) {
                  *dqm->ret = TRUE;

                  if (g_ctrlm.authservice_fast_retries < g_ctrlm.authservice_fast_retries_max) {
                     g_ctrlm.authservice_fast_retries++;
                  }
               }

               if(g_ctrlm.authservice_fast_retries == g_ctrlm.authservice_fast_retries_max) {
                  *dqm->ret = FALSE;
                  *dqm->switch_poll_interval = TRUE;
                  g_ctrlm.authservice_fast_retries = 0;
                  LOG_INFO("%s: Switching to normal authservice poll interval\n", __FUNCTION__);
               }
            }

            if(dqm->semaphore != NULL) {
               // Signal the condition to indicate that the result is present
               sem_post(dqm->semaphore);
            }
            break;
         }
#ifdef CTRLM_NETWORK_RF4CE
         case CTRLM_MAIN_QUEUE_MSG_TYPE_NOTIFY_FIRMWARE: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_NOTIFY_FIRMWARE\n", __FUNCTION__);
            if(ctrlm_main_successful_init_get()) {
               ctrlm_main_queue_msg_notify_firmware_t *dqm = (ctrlm_main_queue_msg_notify_firmware_t *)msg;
               for(auto const &itr : g_ctrlm.networks) {
                  if(ctrlm_network_type_get(itr.first) == CTRLM_NETWORK_TYPE_RF4CE) {
                     ctrlm_obj_network_rf4ce_t *net_rf4ce = (ctrlm_obj_network_rf4ce_t *)itr.second;
                     net_rf4ce->notify_firmware(dqm->controller_type, dqm->image_type, dqm->force_update, dqm->version_software, dqm->version_hardware_min, dqm->version_bootloader_min);
                  }
               }
            } else {
               // Networks are not ready, push back to the queue, then continue so it's not freed
               ctrlm_timeout_create(CTRLM_MAIN_QUEUE_REPEAT_DELAY, ctrlm_message_queue_delay, msg);
               continue;
            }
            break;
         }
#endif
         case CTRLM_MAIN_QUEUE_MSG_TYPE_IR_REMOTE_USAGE: {
            gboolean day_changed = false;
            ctrlm_main_queue_msg_ir_remote_usage_t *dqm = (ctrlm_main_queue_msg_ir_remote_usage_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_IR_REMOTE_USAGE\n", __FUNCTION__);
            //Check if the day changed
            day_changed = ctrlm_main_handle_day_change_ir_remote_usage();
            if(day_changed) {
               ctrlm_property_write_ir_remote_usage();
            }
            ctrlm_main_iarm_call_ir_remote_usage_get_(dqm->ir_remote_usage);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_PAIRING_METRICS: {
            ctrlm_main_queue_msg_pairing_metrics_t *dqm = (ctrlm_main_queue_msg_pairing_metrics_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_PAIRING_METRICS\n", __FUNCTION__);
            ctrlm_main_iarm_call_pairing_metrics_get_(dqm->pairing_metrics);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_LAST_KEY_INFO: {
            ctrlm_main_queue_msg_last_key_info_t *dqm = (ctrlm_main_queue_msg_last_key_info_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_LAST_KEY_INFO\n", __FUNCTION__);
            ctrlm_main_iarm_call_last_key_info_get_(dqm->last_key_info);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_CONTROLLER_TYPE_GET: {
            bool result = false;
            ctrlm_main_queue_msg_controller_type_get_t *dqm = (ctrlm_main_queue_msg_controller_type_get_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_CONTROLLER_TYPE_GET\n", __FUNCTION__);
            if(dqm->controller_type) {
               *dqm->controller_type = obj_net->ctrlm_controller_type_get(dqm->controller_id);
               result = (*dqm->controller_type != CTRLM_RCU_CONTROLLER_TYPE_INVALID ? true : false);
            }
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = (result ? CTRLM_CONTROLLER_STATUS_REQUEST_SUCCESS : CTRLM_CONTROLLER_STATUS_REQUEST_ERROR);
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_SET_VALUES: {
            ctrlm_main_queue_msg_main_control_service_settings_t *dqm = (ctrlm_main_queue_msg_main_control_service_settings_t *) msg;
            ctrlm_main_iarm_call_control_service_settings_t *settings = dqm->settings;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_SET_VALUES\n", __FUNCTION__);

            if(settings->available & CTRLM_MAIN_CONTROL_SERVICE_SETTINGS_ASB_ENABLED) {
#ifdef ASB
               // Write new asb_enabled flag to NVM
               ctrlm_db_asb_enabled_write(&settings->asb_enabled, CTRLM_ASB_ENABLED_LEN); 
               g_ctrlm.cs_values.asb_enable = settings->asb_enabled;
               LOG_INFO("%s: ASB Enabled Set Values <%s>\n", __FUNCTION__, g_ctrlm.cs_values.asb_enable ? "true" : "false");
#else
               LOG_INFO("%s: ASB Enabled Set Values <false>, ASB Not Supported\n", __FUNCTION__);
#endif
            }
            if(settings->available & CTRLM_MAIN_CONTROL_SERVICE_SETTINGS_OPEN_CHIME_ENABLED) {
               // Write new open_chime_enabled flag to NVM
               ctrlm_db_open_chime_enabled_write(&settings->open_chime_enabled, CTRLM_OPEN_CHIME_ENABLED_LEN);
               g_ctrlm.cs_values.chime_open_enable = settings->open_chime_enabled;
               LOG_INFO("%s: Open Chime Enabled <%s>\n", __FUNCTION__, settings->open_chime_enabled ? "true" : "false");
            }
            if(settings->available & CTRLM_MAIN_CONTROL_SERVICE_SETTINGS_CLOSE_CHIME_ENABLED) {
               // Write new close_chime_enabled flag to NVM
               ctrlm_db_close_chime_enabled_write(&settings->close_chime_enabled, CTRLM_CLOSE_CHIME_ENABLED_LEN); 
               g_ctrlm.cs_values.chime_close_enable = settings->close_chime_enabled;
               LOG_INFO("%s: Close Chime Enabled <%s>\n", __FUNCTION__, settings->close_chime_enabled ? "true" : "false");
            }
            if(settings->available & CTRLM_MAIN_CONTROL_SERVICE_SETTINGS_PRIVACY_CHIME_ENABLED) {
               // Write new privacy_chime_enabled flag to NVM
               ctrlm_db_privacy_chime_enabled_write(&settings->privacy_chime_enabled, CTRLM_PRIVACY_CHIME_ENABLED_LEN); 
               g_ctrlm.cs_values.chime_privacy_enable = settings->privacy_chime_enabled;
               LOG_INFO("%s: Privacy Chime Enabled <%s>\n", __FUNCTION__, settings->privacy_chime_enabled ? "true" : "false");
            }
            if(settings->available & CTRLM_MAIN_CONTROL_SERVICE_SETTINGS_CONVERSATIONAL_MODE) {
               if((settings->conversational_mode < CTRLM_MIN_CONVERSATIONAL_MODE) || (settings->conversational_mode > CTRLM_MAX_CONVERSATIONAL_MODE)) {
                  LOG_WARN("%s: Conversational Mode Invalid <%d>.  Ignoring.\n", __FUNCTION__, settings->conversational_mode);
               } else {
                  // Write new conversational mode to NVM
                  ctrlm_db_conversational_mode_write((guchar *)&settings->conversational_mode, CTRLM_CONVERSATIONAL_MODE_LEN); 
                  g_ctrlm.cs_values.conversational_mode = settings->conversational_mode;
                  LOG_INFO("%s: Conversational Mode Set <%d>\n", __FUNCTION__, settings->conversational_mode);
               }
            }
            if(settings->available & CTRLM_MAIN_CONTROL_SERVICE_SETTINGS_SET_CHIME_VOLUME) {
               if(settings->chime_volume >= CTRLM_CHIME_VOLUME_INVALID) {
                  LOG_WARN("%s: Chime Volume Invalid <%d>.  Ignoring.\n", __FUNCTION__, settings->chime_volume);
               } else {
                  // Write new chime_volume to NVM
                  ctrlm_db_chime_volume_write((guchar *)&settings->chime_volume, CTRLM_CHIME_VOLUME_LEN); 
                  g_ctrlm.cs_values.chime_volume = settings->chime_volume;
                  LOG_INFO("%s: Chime Volume Set <%d>\n", __FUNCTION__, settings->chime_volume);
               }
            }
            if(settings->available & CTRLM_MAIN_CONTROL_SERVICE_SETTINGS_SET_IR_COMMAND_REPEATS) {
               if((settings->ir_command_repeats < CTRLM_MIN_IR_COMMAND_REPEATS) || (settings->ir_command_repeats > CTRLM_MAX_IR_COMMAND_REPEATS)) {
                  LOG_WARN("%s: IR command repeats Invalid <%d>.  Ignoring.\n", __FUNCTION__, settings->ir_command_repeats);
               } else {
                  // Write new ir_command_repeats to NVM
                  ctrlm_db_ir_command_repeats_write(&settings->ir_command_repeats, CTRLM_IR_COMMAND_REPEATS_LEN); 
                  g_ctrlm.cs_values.ir_repeats = settings->ir_command_repeats;
                  LOG_INFO("%s: IR Command Repeats Set <%d>\n", __FUNCTION__, settings->ir_command_repeats);
               }
            }

            // Set these values in the networks
            for(auto const &itr : g_ctrlm.networks) {
               itr.second->cs_values_set(&g_ctrlm.cs_values, false);
            }

            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_GET_VALUES: {
            ctrlm_main_queue_msg_main_control_service_settings_t *dqm = (ctrlm_main_queue_msg_main_control_service_settings_t *) msg;
            ctrlm_main_iarm_call_control_service_settings_t *settings = dqm->settings;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_GET_VALUES\n", __FUNCTION__);
#ifdef ASB
            settings->asb_supported = true;
#else
            settings->asb_supported = false;
#endif
            settings->asb_enabled   = g_ctrlm.cs_values.asb_enable;
            settings->open_chime_enabled          = g_ctrlm.cs_values.chime_open_enable;
            settings->close_chime_enabled         = g_ctrlm.cs_values.chime_close_enable;
            settings->privacy_chime_enabled       = g_ctrlm.cs_values.chime_privacy_enable;
            settings->conversational_mode         = g_ctrlm.cs_values.conversational_mode;
            settings->chime_volume                = g_ctrlm.cs_values.chime_volume;
            settings->ir_command_repeats          = g_ctrlm.cs_values.ir_repeats;
            LOG_INFO("%s: ASB Get Values: Supported <%s>  ASB Enabled <%s>  Open Chime Enabled <%s>  Close Chime Enabled <%s>  Privacy Chime Enabled <%s>  Conversational Mode <%u>  Chime Volume <%d>  IR Command Repeats <%d>\n", 
               __FUNCTION__, settings->asb_supported ? "true" : "false", settings->asb_enabled ? "true" : "false", settings->open_chime_enabled ? "true" : "false",
               settings->close_chime_enabled ? "true" : "false", settings->privacy_chime_enabled ? "true" : "false", settings->conversational_mode, settings->chime_volume, settings->ir_command_repeats);
            
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_CAN_FIND_MY_REMOTE: {
            ctrlm_main_queue_msg_main_control_service_can_find_my_remote_t *dqm = (ctrlm_main_queue_msg_main_control_service_can_find_my_remote_t *) msg;
            ctrlm_main_iarm_call_control_service_can_find_my_remote_t *can_find_my_remote = dqm->can_find_my_remote;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_CAN_FIND_MY_REMOTE\n", __FUNCTION__);
            obj_net = g_ctrlm.networks[hdr->network_id];
            can_find_my_remote->is_supported = obj_net->is_fmr_supported();
            LOG_INFO("%s: Can find My Remote: Supported <%s>\n", __FUNCTION__, can_find_my_remote->is_supported ? "true" : "false");
            
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_START_PAIRING_MODE: {
            ctrlm_main_queue_msg_main_control_service_pairing_mode_t *dqm = (ctrlm_main_queue_msg_main_control_service_pairing_mode_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_START_PAIRING_MODE\n", __FUNCTION__);
            ctrlm_main_iarm_call_control_service_start_pairing_mode_(dqm->pairing);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_END_PAIRING_MODE: {
            ctrlm_main_queue_msg_main_control_service_pairing_mode_t *dqm = (ctrlm_main_queue_msg_main_control_service_pairing_mode_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_END_PAIRING_MODE\n", __FUNCTION__);
            ctrlm_main_iarm_call_control_service_end_pairing_mode_(dqm->pairing);
            if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
               // Signal the condition to indicate that the result is present
               *dqm->cmd_result = CTRLM_MAIN_STATUS_REQUEST_SUCCESS;
               sem_post(dqm->semaphore);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_BATTERY_MILESTONE_EVENT: {
            ctrlm_main_queue_msg_rf4ce_battery_milestone_t *dqm = (ctrlm_main_queue_msg_rf4ce_battery_milestone_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_BATTERY_MILESTONE_EVENT\n", __FUNCTION__);
            ctrlm_rcu_iarm_event_battery_milestone(dqm->network_id, dqm->controller_id, dqm->battery_event, dqm->percent);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_REMOTE_REBOOT_EVENT: {
            ctrlm_main_queue_msg_remote_reboot_t *dqm = (ctrlm_main_queue_msg_remote_reboot_t *) msg;
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_REMOTE_REBOOT_EVENT\n", __FUNCTION__);
            ctrlm_rcu_iarm_event_remote_reboot(dqm->network_id, dqm->controller_id, dqm->voltage, dqm->reason, dqm->assert_number);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_NTP_CHECK: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_NTP_CHECK\n", __FUNCTION__);
            if(time(NULL) < ctrlm_shutdown_time_get()) {
               ctrlm_timeout_create(5000, ctrlm_ntp_check, NULL);
            } else {
               LOG_INFO("%s: Time is correct, calling functions that depend on time at boot\n", __FUNCTION__);
               for(auto const &itr : g_ctrlm.networks) {
                  itr.second->set_timers();
               }
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_CONTROLLER_REVERSE_CMD: {
            if(ctrlm_main_successful_init_get()) {
               ctrlm_main_queue_msg_rcu_reverse_cmd_t *dqm = (ctrlm_main_queue_msg_rcu_reverse_cmd_t *) msg;
               ctrlm_controller_status_cmd_result_t      cmd_result = CTRLM_CONTROLLER_STATUS_REQUEST_ERROR;
               LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_CONTROLLER_REVERSE_CMD\n", __FUNCTION__);
               cmd_result = obj_net->req_process_reverse_cmd(dqm);
               if(dqm->semaphore != NULL && dqm->cmd_result != NULL) {
                  // Signal the condition to indicate that the result is present
                  *dqm->cmd_result = cmd_result;
                  sem_post(dqm->semaphore);
               }
            } else {
               // Networks are not ready, push back to the queue, then continue so it's not freed
               ctrlm_timeout_create(CTRLM_MAIN_QUEUE_REPEAT_DELAY, ctrlm_message_queue_delay, msg);
            }
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_AUDIO_CAPTURE_START: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_AUDIO_CAPTURE_START\n", __FUNCTION__);
            ctrlm_main_queue_msg_audio_capture_start_t *start_msg = (ctrlm_main_queue_msg_audio_capture_start_t *)msg;
            ctrlm_voice_xrsr_session_capture_start(start_msg);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_AUDIO_CAPTURE_STOP: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_AUDIO_CAPTURE_STOP\n", __FUNCTION__);
            ctrlm_voice_xrsr_session_capture_stop();
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_ACCOUNT_ID_UPDATE: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_ACCOUNT_ID_UPDATE\n", __FUNCTION__);
            #ifdef AUTH_ENABLED
            #ifdef AUTH_ACCOUNT_ID
            ctrlm_main_queue_msg_account_id_update_t *dqm = (ctrlm_main_queue_msg_account_id_update_t *)msg;
            if(dqm) {
               ctrlm_load_service_account_id(dqm->account_id);
            }
            #endif
            #endif
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_STARTUP: {
            LOG_DEBUG("%s: message type CTRLM_MAIN_QUEUE_MSG_TYPE_STARTUP\n", __FUNCTION__);
            // All main thread startup activities can be placed here.
            // RFC Retrieval, NTP check, register for IARM calls
            ctrlm_start_iarm(NULL);
            ctrlm_rfc_t *rfc = ctrlm_rfc_t::get_instance();
            if(rfc) {
               rfc->fetch_attributes();
            }
            ctrlm_timeout_create(1000, ctrlm_ntp_check, NULL);
            break;
         }
         case CTRLM_MAIN_QUEUE_MSG_TYPE_HANDLER: {
            ctrlm_main_queue_msg_handler_t *dqm = (ctrlm_main_queue_msg_handler_t *) msg;

            switch(dqm->type) {
               case CTRLM_HANDLER_NETWORK: {
                  ctrlm_obj_network_t *net = (ctrlm_obj_network_t *)dqm->obj;
                  if(net != NULL) { // This occurs when the network object is passed in
                     (net->*dqm->msg_handler.n)(dqm->data, dqm->data_len);
                  } else {               // This occurs when the network_id is "supposed" to be passed in, let's check
                     vector<ctrlm_obj_network_t *> nets;
                     if(ctrlm_network_id_is_valid(dqm->header.network_id)) { // Valid network id, this is only for one network
                        nets.push_back(g_ctrlm.networks[dqm->header.network_id]);
                     } else if(dqm->header.network_id == CTRLM_MAIN_NETWORK_ID_ALL) { // This is for ALL networks, this is still valid
                        for(const auto &itr : g_ctrlm.networks) {
                           nets.push_back(itr.second);
                        }
                     } else {
                        if(dqm->header.network_id != CTRLM_MAIN_NETWORK_ID_DSP) {
                           LOG_ERROR("%s: Invalid Network ID! %u\n", __FUNCTION__, dqm->header.network_id);
                        }
                        break;
                     }
                     for(const auto &itr : nets) {
                        (itr->*dqm->msg_handler.n)(dqm->data, dqm->data_len);
                     }
                  }
                  break;
               }
               case CTRLM_HANDLER_VOICE: {
                  ctrlm_voice_endpoint_t *voice = (ctrlm_voice_endpoint_t *)dqm->obj;
                  (voice->*dqm->msg_handler.v)(dqm->data, dqm->data_len);
                  break;
               }
               case CTRLM_HANDLER_CONTROLLER: {
                 ctrlm_obj_controller_t *controller = (ctrlm_obj_controller_t *)dqm->obj;
                 if(controller) {
                    (controller->*dqm->msg_handler.c)(dqm->data, dqm->data_len);
                 } else {
                    LOG_ERROR("%s: Controller object NULL!\n", __FUNCTION__);
                 }
                 break;
               }
               default: {
                  LOG_ERROR("%s: unnkown handler type\n", __FUNCTION__);
                  break;
               }
            }
            break;
         }
         default: {
            if (obj_net == 0 || !obj_net->message_dispatch(msg)) {
               LOG_ERROR("%s: Network: %s. Unknown message type %u\n", __FUNCTION__, (obj_net !=0?obj_net->name_get():"N/A"), hdr->type);
            }
            break;
         }
      }
      ctrlm_queue_msg_destroy(msg);
   } while(running);
   return(NULL);
}


gboolean ctrlm_is_binding_table_full(void) {
   return(g_ctrlm.bound_controller_qty >= CTRLM_MAIN_MAX_BOUND_CONTROLLERS);
}

gboolean ctrlm_is_key_code_mask_enabled(void) {
   return(g_ctrlm.mask_key_codes_json || g_ctrlm.mask_key_codes_iarm);
}

bool ctrlm_is_pii_mask_enabled(void) {
   return(g_ctrlm.mask_pii);
}

gboolean ctrlm_timeout_recently_booted(gpointer user_data) {
   LOG_INFO("%s: Timeout - Recently booted.\n", __FUNCTION__);
   g_ctrlm.recently_booted             = FALSE;
   g_ctrlm.recently_booted_timeout_tag = 0;
   return(FALSE);
}

gboolean ctrlm_timeout_systemd_restart_delay(gpointer user_data) {
   LOG_INFO("%s: Timeout - Update systemd restart delay to " CTRLM_RESTART_DELAY_SHORT "\n", __FUNCTION__);
   v_secure_system("systemctl set-environment CTRLM_RESTART_DELAY=" CTRLM_RESTART_DELAY_SHORT);
   return(FALSE);
}

gboolean ctrlm_main_iarm_call_status_get(ctrlm_main_iarm_call_status_t *status) {
   if(status == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_status_t *msg = (ctrlm_main_queue_msg_main_status_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_status_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STATUS;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->status            = status;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process STATUS_GET request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_status_get_(ctrlm_main_iarm_call_status_t *status) {
   unsigned long index = 0;
   errno_t safec_rc = -1;

   if(g_ctrlm.networks.size() > CTRLM_MAIN_MAX_NETWORKS) {
      status->network_qty = CTRLM_MAIN_MAX_NETWORKS;
   } else {
      status->network_qty = g_ctrlm.networks.size();
   }
   for(auto const &itr : g_ctrlm.networks) {
      status->networks[index].id   = itr.first;
      status->networks[index].type = itr.second->type_get();
      index++;
   }

   status->result = CTRLM_IARM_CALL_RESULT_SUCCESS;

   safec_rc = strcpy_s(status->ctrlm_version, sizeof(status->ctrlm_version), CTRLM_VERSION);
   ERR_CHK(safec_rc);

   safec_rc = strcpy_s(status->ctrlm_commit_id, sizeof(status->ctrlm_commit_id), CTRLM_MAIN_COMMIT_ID);
   ERR_CHK(safec_rc);

   safec_rc = strncpy_s(status->stb_device_id, sizeof(status->stb_device_id), g_ctrlm.device_id.c_str(), CTRLM_MAIN_DEVICE_ID_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   status->stb_device_id[CTRLM_MAIN_DEVICE_ID_MAX_LENGTH - 1] = '\0';

   safec_rc = strncpy_s(status->stb_receiver_id, sizeof(status->stb_receiver_id), g_ctrlm.receiver_id.c_str(), CTRLM_MAIN_RECEIVER_ID_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   status->stb_receiver_id[CTRLM_MAIN_RECEIVER_ID_MAX_LENGTH - 1] = '\0';
}

gboolean ctrlm_main_iarm_call_ir_remote_usage_get(ctrlm_main_iarm_call_ir_remote_usage_t *ir_remote_usage) {
   if(ir_remote_usage == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_ir_remote_usage_t *msg = (ctrlm_main_queue_msg_ir_remote_usage_t *)g_malloc(sizeof(ctrlm_main_queue_msg_ir_remote_usage_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->type              = CTRLM_MAIN_QUEUE_MSG_TYPE_IR_REMOTE_USAGE;
   msg->ir_remote_usage   = ir_remote_usage;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process IR_REMOTE_USAGE_GET request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_ir_remote_usage_get_(ctrlm_main_iarm_call_ir_remote_usage_t *ir_remote_usage) {
   ir_remote_usage->result                  = CTRLM_IARM_CALL_RESULT_SUCCESS;
   ir_remote_usage->today                   = g_ctrlm.today;
   ir_remote_usage->has_ir_xr2_yesterday    = g_ctrlm.ir_remote_usage_yesterday.has_ir_xr2;
   ir_remote_usage->has_ir_xr2_today        = g_ctrlm.ir_remote_usage_today.has_ir_xr2;
   ir_remote_usage->has_ir_xr5_yesterday    = g_ctrlm.ir_remote_usage_yesterday.has_ir_xr5;
   ir_remote_usage->has_ir_xr5_today        = g_ctrlm.ir_remote_usage_today.has_ir_xr5;
   ir_remote_usage->has_ir_xr11_yesterday   = g_ctrlm.ir_remote_usage_yesterday.has_ir_xr11;
   ir_remote_usage->has_ir_xr11_today       = g_ctrlm.ir_remote_usage_today.has_ir_xr11;
   ir_remote_usage->has_ir_xr15_yesterday   = g_ctrlm.ir_remote_usage_yesterday.has_ir_xr15;
   ir_remote_usage->has_ir_xr15_today       = g_ctrlm.ir_remote_usage_today.has_ir_xr15;
   ir_remote_usage->has_ir_remote_yesterday = g_ctrlm.ir_remote_usage_yesterday.has_ir_remote;
   ir_remote_usage->has_ir_remote_today     = g_ctrlm.ir_remote_usage_today.has_ir_remote;
}

gboolean ctrlm_main_iarm_call_pairing_metrics_get(ctrlm_main_iarm_call_pairing_metrics_t *pairing_metrics) {
   if(pairing_metrics == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_pairing_metrics_t *msg = (ctrlm_main_queue_msg_pairing_metrics_t *)g_malloc(sizeof(ctrlm_main_queue_msg_pairing_metrics_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->type              = CTRLM_MAIN_QUEUE_MSG_TYPE_PAIRING_METRICS;
   msg->pairing_metrics   = pairing_metrics;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process IR_REMOTE_USAGE_GET request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_pairing_metrics_get_(ctrlm_main_iarm_call_pairing_metrics_t *pairing_metrics) {
   errno_t safec_rc                                        = -1;
   pairing_metrics->result                                 = CTRLM_IARM_CALL_RESULT_SUCCESS;
   pairing_metrics->num_screenbind_failures                = g_ctrlm.pairing_metrics.num_screenbind_failures;
   pairing_metrics->last_screenbind_error_timestamp        = g_ctrlm.pairing_metrics.last_screenbind_error_timestamp;
   pairing_metrics->last_screenbind_error_code             = g_ctrlm.pairing_metrics.last_screenbind_error_code;
   pairing_metrics->num_non_screenbind_failures            = g_ctrlm.pairing_metrics.num_non_screenbind_failures;
   pairing_metrics->last_non_screenbind_error_timestamp    = g_ctrlm.pairing_metrics.last_non_screenbind_error_timestamp;
   pairing_metrics->last_non_screenbind_error_code         = g_ctrlm.pairing_metrics.last_non_screenbind_error_code;
   pairing_metrics->last_non_screenbind_error_binding_type = g_ctrlm.pairing_metrics.last_non_screenbind_error_binding_type;

   safec_rc = strncpy_s(pairing_metrics->last_screenbind_remote_type, sizeof(pairing_metrics->last_screenbind_remote_type), g_ctrlm.pairing_metrics.last_screenbind_remote_type, CTRLM_MAIN_SOURCE_NAME_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   pairing_metrics->last_screenbind_remote_type[CTRLM_MAIN_SOURCE_NAME_MAX_LENGTH - 1] = '\0';

   safec_rc = strncpy_s(pairing_metrics->last_non_screenbind_remote_type, sizeof(pairing_metrics->last_non_screenbind_remote_type), g_ctrlm.pairing_metrics.last_non_screenbind_remote_type, CTRLM_MAIN_SOURCE_NAME_MAX_LENGTH - 1);
   ERR_CHK(safec_rc);
   pairing_metrics->last_non_screenbind_remote_type[CTRLM_MAIN_SOURCE_NAME_MAX_LENGTH - 1] = '\0';
   LOG_INFO("%s: num_screenbind_failures <%lu>  last_screenbind_error_timestamp <%lums> last_screenbind_error_code <%s> last_screenbind_remote_type <%s>\n", 
      __FUNCTION__, pairing_metrics->num_screenbind_failures, pairing_metrics->last_screenbind_error_timestamp, 
      ctrlm_bind_status_str(pairing_metrics->last_screenbind_error_code), pairing_metrics->last_non_screenbind_remote_type);
   LOG_INFO("%s: num_non_screenbind_failures <%lu>  last_non_screenbind_error_timestamp <%lums> last_non_screenbind_error_code <%s> last_non_screenbind_remote_type <%s> last_non_screenbind_error_binding_type <%s>\n", 
      __FUNCTION__, pairing_metrics->num_non_screenbind_failures, pairing_metrics->last_non_screenbind_error_timestamp, 
      ctrlm_bind_status_str(pairing_metrics->last_non_screenbind_error_code), pairing_metrics->last_non_screenbind_remote_type,
      ctrlm_rcu_binding_type_str((ctrlm_rcu_binding_type_t)pairing_metrics->last_non_screenbind_error_binding_type));
}

gboolean ctrlm_main_iarm_call_last_key_info_get(ctrlm_main_iarm_call_last_key_info_t *last_key_info) {
   if(last_key_info == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_last_key_info_t *msg = (ctrlm_main_queue_msg_last_key_info_t *)g_malloc(sizeof(ctrlm_main_queue_msg_last_key_info_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->type              = CTRLM_MAIN_QUEUE_MSG_TYPE_LAST_KEY_INFO;
   msg->last_key_info     = last_key_info;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process LAST_KEY_INFO_GET request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_last_key_info_get_(ctrlm_main_iarm_call_last_key_info_t *last_key_info) {
   last_key_info->result                 = CTRLM_IARM_CALL_RESULT_SUCCESS;
   last_key_info->controller_id          = g_ctrlm.last_key_info.controller_id;
   last_key_info->source_type            = g_ctrlm.last_key_info.source_type;
   last_key_info->source_key_code        = g_ctrlm.last_key_info.source_key_code;
   last_key_info->timestamp              = g_ctrlm.last_key_info.timestamp;
   if(g_ctrlm.last_key_info.source_type == IARM_BUS_IRMGR_KEYSRC_RF) {
      last_key_info->is_screen_bind_mode  = false;
      last_key_info->remote_keypad_config = ctrlm_get_remote_keypad_config(g_ctrlm.last_key_info.source_name);
   } else {
      last_key_info->is_screen_bind_mode  = g_ctrlm.last_key_info.is_screen_bind_mode;
      last_key_info->remote_keypad_config = g_ctrlm.last_key_info.remote_keypad_config;
   }

   errno_t safec_rc = strcpy_s(last_key_info->source_name, sizeof(last_key_info->source_name), g_ctrlm.last_key_info.source_name);
   ERR_CHK(safec_rc);

   if(ctrlm_is_key_code_mask_enabled()) {
      LOG_INFO("%s: controller_id <%d>, key_code <*>, key_src <%d>, timestamp <%lldms>, is_screen_bind_mode <%d>, remote_keypad_config <%d>, sourceName <%s>\n",
       __FUNCTION__, last_key_info->controller_id, last_key_info->source_type, last_key_info->timestamp, last_key_info->is_screen_bind_mode, last_key_info->remote_keypad_config, last_key_info->source_name);
   } else {
      LOG_INFO("%s: controller_id <%d>, key_code <%ld>, key_src <%d>, timestamp <%lldms>, is_screen_bind_mode <%d>, remote_keypad_config <%d>, sourceName <%s>\n",
       __FUNCTION__, last_key_info->controller_id, last_key_info->source_key_code, last_key_info->source_type, last_key_info->timestamp, last_key_info->is_screen_bind_mode, last_key_info->remote_keypad_config, last_key_info->source_name);
   }
}

gboolean ctrlm_main_iarm_call_network_status_get(ctrlm_main_iarm_call_network_status_t *status) {
   if(status == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_network_status_t msg = {0};

   sem_init(&semaphore, 0, 0);

   msg.status            = status;
   msg.status->result    = CTRLM_IARM_CALL_RESULT_ERROR;
   msg.semaphore         = &semaphore;
   msg.cmd_result        = &cmd_result;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_network_status, &msg, sizeof(msg), NULL, status->network_id);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process NETWORK_STATUS_GET request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

gboolean ctrlm_main_iarm_call_property_set(ctrlm_main_iarm_call_property_t *property) {
   if(property == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_property_t *msg = (ctrlm_main_queue_msg_main_property_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_property_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_PROPERTY_SET;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->property          = property;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process PROPERTY_SET request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_property_set_(ctrlm_main_iarm_call_property_t *property) {
   if(property->network_id != CTRLM_MAIN_NETWORK_ID_ALL && !ctrlm_network_id_is_valid(property->network_id)) {
      property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: network id - Out of range %u\n", __FUNCTION__, property->network_id);
      return;
   }

   switch(property->name) {
      case CTRLM_PROPERTY_BINDING_BUTTON_ACTIVE: {
         if(property->value == 0) {
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            ctrlm_timeout_destroy(&g_ctrlm.binding_button_timeout_tag);
            // If this was a change, fire the IARM event
            if (g_ctrlm.binding_button) {
               ctrlm_main_iarm_event_binding_button(false);
            }
            g_ctrlm.binding_button = false;
            LOG_INFO("%s: BINDING BUTTON <INACTIVE>\n", __FUNCTION__);
         } else if(property->value == 1) {
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            ctrlm_timeout_destroy(&g_ctrlm.binding_button_timeout_tag);
            // If this was a change, fire the IARM event
            if (!g_ctrlm.binding_button) {
               ctrlm_main_iarm_event_binding_button(true);
            }
            g_ctrlm.binding_button = true;
            // If screenbind is enabled, disable it
            if(g_ctrlm.binding_screen_active == true) {
               g_ctrlm.binding_screen_active = false;
               LOG_INFO("%s: BINDING SCREEN <INACTIVE> -- Due to entering button button mode\n", __FUNCTION__);
            }
            // Set a timer to stop binding button mode
            g_ctrlm.binding_button_timeout_tag = ctrlm_timeout_create(g_ctrlm.binding_button_timeout_val, ctrlm_timeout_binding_button, NULL);
            LOG_INFO("%s: BINDING BUTTON <ACTIVE>\n", __FUNCTION__);
         } else {
            property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
            LOG_ERROR("%s: BINDING BUTTON ACTIVE - Invalid parameter 0x%08lX\n", __FUNCTION__, property->value);
         }
         break;
      }
      case CTRLM_PROPERTY_BINDING_SCREEN_ACTIVE: {
         if(property->value == 0) {
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            ctrlm_timeout_destroy(&g_ctrlm.screen_bind_timeout_tag);
            g_ctrlm.binding_screen_active = false;
            LOG_INFO("%s: SCREEN BIND STATE <INACTIVE>\n", __FUNCTION__);
         } else if(property->value == 1) {
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            ctrlm_timeout_destroy(&g_ctrlm.screen_bind_timeout_tag);
            g_ctrlm.binding_screen_active = true;
            // If binding button is enabled, disable it
            if(g_ctrlm.binding_button == true) {
               g_ctrlm.binding_button = false;
               LOG_INFO("%s: BINDING SCREEN <INACTIVE> -- Due to entering button button mode\n", __FUNCTION__);
            }
            // Set a timer to stop screen bind mode
            g_ctrlm.screen_bind_timeout_tag = ctrlm_timeout_create(g_ctrlm.screen_bind_timeout_val, ctrlm_timeout_screen_bind, NULL);
            LOG_INFO("%s: SCREEN BIND STATE <ACTIVE>\n", __FUNCTION__);
         } else {
            property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
            LOG_ERROR("%s: SCREEN BIND STATE - Invalid parameter 0x%08lX\n", __FUNCTION__, property->value);
         }
         break;
      }
      case CTRLM_PROPERTY_BINDING_LINE_OF_SIGHT_ACTIVE: {
         property->result = CTRLM_IARM_CALL_RESULT_ERROR_READ_ONLY;
         LOG_ERROR("%s: BINDING LINE OF SIGHT ACTIVE - Read only\n", __FUNCTION__);
         break;
      }
      case CTRLM_PROPERTY_AUTOBIND_LINE_OF_SIGHT_ACTIVE:{
         property->result = CTRLM_IARM_CALL_RESULT_ERROR_READ_ONLY;
         LOG_ERROR("%s: AUTOBIND LINE OF SIGHT ACTIVE - Read only\n", __FUNCTION__);
         break;
      }
      case CTRLM_PROPERTY_ACTIVE_PERIOD_BUTTON: {
         if(property->value < CTRLM_PROPERTY_ACTIVE_PERIOD_BUTTON_VALUE_MIN || property->value > CTRLM_PROPERTY_ACTIVE_PERIOD_BUTTON_VALUE_MAX) {
            property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
            LOG_ERROR("%s: ACTIVE PERIOD BUTTON - Out of range %lu\n", __FUNCTION__, property->value);
         } else {
            g_ctrlm.binding_button_timeout_val = property->value;
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            LOG_INFO("%s: ACTIVE PERIOD BUTTON %lu ms\n", __FUNCTION__, property->value);
         }
         break;
      }
      case CTRLM_PROPERTY_ACTIVE_PERIOD_SCREENBIND: {
         if(property->value < CTRLM_PROPERTY_ACTIVE_PERIOD_SCREENBIND_VALUE_MIN || property->value > CTRLM_PROPERTY_ACTIVE_PERIOD_SCREENBIND_VALUE_MAX) {
            property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
            LOG_ERROR("%s: ACTIVE PERIOD SCREENBIND - Out of range %lu\n", __FUNCTION__, property->value);
         } else {
            g_ctrlm.screen_bind_timeout_val = property->value;
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            LOG_INFO("%s: ACTIVE PERIOD SCREENBIND %lu ms\n", __FUNCTION__, property->value);
         }
         break;
      }
      case CTRLM_PROPERTY_ACTIVE_PERIOD_ONE_TOUCH_AUTOBIND: {
         if(property->value < CTRLM_PROPERTY_ACTIVE_PERIOD_ONE_TOUCH_AUTOBIND_VALUE_MIN || property->value > CTRLM_PROPERTY_ACTIVE_PERIOD_ONE_TOUCH_AUTOBIND_VALUE_MAX) {
            property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
            LOG_ERROR("%s: ACTIVE PERIOD ONE-TOUCH AUTOBIND - Out of range %lu\n", __FUNCTION__, property->value);
         } else {
            g_ctrlm.one_touch_autobind_timeout_val = property->value;
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            LOG_INFO("%s: ACTIVE PERIOD ONE-TOUCH AUTOBIND %lu ms\n", __FUNCTION__, property->value);
         }
         break;
      }
      case CTRLM_PROPERTY_ACTIVE_PERIOD_LINE_OF_SIGHT: {
         if(property->value < CTRLM_PROPERTY_ACTIVE_PERIOD_LINE_OF_SIGHT_VALUE_MIN || property->value > CTRLM_PROPERTY_ACTIVE_PERIOD_LINE_OF_SIGHT_VALUE_MAX) {
            property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
            LOG_ERROR("%s: ACTIVE PERIOD LINE OF SIGHT - Out of range %lu\n", __FUNCTION__, property->value);
         } else {
            g_ctrlm.line_of_sight_timeout_val = property->value;
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            LOG_INFO("%s: ACTIVE PERIOD LINE OF SIGHT %lu ms\n", __FUNCTION__, property->value);
         }
         break;
      }
      case CTRLM_PROPERTY_VALIDATION_TIMEOUT_INITIAL: {
         if(property->value < CTRLM_PROPERTY_VALIDATION_TIMEOUT_MIN || property->value > CTRLM_PROPERTY_VALIDATION_TIMEOUT_MAX) {
            property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
            LOG_ERROR("%s: VALIDATION TIMEOUT INITIAL - Out of range %lu\n", __FUNCTION__, property->value);
         } else {
            ctrlm_validation_timeout_initial_set(property->value);
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            LOG_INFO("%s: VALIDATION TIMEOUT INITIAL %lu ms\n", __FUNCTION__, property->value);
         }
         break;
      }
      case CTRLM_PROPERTY_VALIDATION_TIMEOUT_DURING: {
         if(property->value < CTRLM_PROPERTY_VALIDATION_TIMEOUT_MIN || property->value > CTRLM_PROPERTY_VALIDATION_TIMEOUT_MAX) {
            property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
            LOG_ERROR("%s: VALIDATION TIMEOUT DURING - Out of range %lu\n", __FUNCTION__, property->value);
         } else {
            ctrlm_validation_timeout_subsequent_set(property->value);
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            LOG_INFO("%s: VALIDATION TIMEOUT DURING %lu ms\n", __FUNCTION__, property->value);
         }
         break;
      }
      case CTRLM_PROPERTY_VALIDATION_MAX_ATTEMPTS: {
         if(property->value > CTRLM_PROPERTY_VALIDATION_MAX_ATTEMPTS_MAX) {
            property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
            LOG_ERROR("%s: VALIDATION MAX ATTEMPTS - Out of range %lu\n", __FUNCTION__, property->value);
         } else {
            ctrlm_validation_max_attempts_set(property->value);
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            LOG_INFO("%s: VALIDATION MAX ATTEMPTS %lu times\n", __FUNCTION__, property->value);
         }
         break;
      }
      case CTRLM_PROPERTY_CONFIGURATION_TIMEOUT: {
         if(property->value < CTRLM_PROPERTY_CONFIGURATION_TIMEOUT_MIN || property->value > CTRLM_PROPERTY_CONFIGURATION_TIMEOUT_MAX) {
            property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
            LOG_ERROR("%s: CONFIGURATION TIMEOUT - Out of range %lu\n", __FUNCTION__, property->value);
         } else {
            ctrlm_validation_timeout_configuration_set(property->value);
            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            LOG_INFO("%s: CONFIGURATION TIMEOUT %lu ms\n", __FUNCTION__, property->value);
         }
         break;
      }
      case CTRLM_PROPERTY_AUTO_ACK: {
         if(ctrlm_is_production_build()) {
            LOG_ERROR("%s: AUTO ACK - unable to set in prod build\n", __FUNCTION__);
         } else {
            g_ctrlm.auto_ack = property->value ? true : false;

            #ifdef CTRLM_NETWORK_RF4CE
            #if CTRLM_HAL_RF4CE_API_VERSION >= 16
            for(auto const &itr : g_ctrlm.networks) {
               if(itr.second->type_get() == CTRLM_NETWORK_TYPE_RF4CE) {
                  itr.second->property_set(CTRLM_HAL_NETWORK_PROPERTY_AUTO_ACK, &g_ctrlm.auto_ack);
               }
            }
            #endif
            #endif

            property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            LOG_INFO("%s: AUTO ACK <%s>\n", __FUNCTION__, property->value ? "enabled" : "disabled");
         }
         break;
      }
      default: {
         LOG_ERROR("%s: Invalid Property %d\n", __FUNCTION__, property->name);
         property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      }
   }
}

gboolean ctrlm_main_iarm_call_property_get(ctrlm_main_iarm_call_property_t *property) {
   if(property == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_property_t *msg = (ctrlm_main_queue_msg_main_property_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_property_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_PROPERTY_GET;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->property          = property;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);


   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process PROPERTY_GET request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_property_get_(ctrlm_main_iarm_call_property_t *property) {
   if(property->network_id != CTRLM_MAIN_NETWORK_ID_ALL && !ctrlm_network_id_is_valid(property->network_id)) {
      property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: network id - Out of range %u\n", __FUNCTION__, property->network_id);
      return;
   }

   switch(property->name) {
      case CTRLM_PROPERTY_BINDING_BUTTON_ACTIVE: {
         property->value  = g_ctrlm.binding_button;
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: BINDING BUTTON <%s>\n", __FUNCTION__, g_ctrlm.binding_button ? "ACTIVE" : "INACTIVE");
         break;
      }
      case CTRLM_PROPERTY_BINDING_SCREEN_ACTIVE: {
         property->value  = g_ctrlm.binding_screen_active;
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: BINDING SCREEN <%s>\n", __FUNCTION__, g_ctrlm.binding_screen_active ? "ACTIVE" : "INACTIVE");
         break;
      }
      case CTRLM_PROPERTY_BINDING_LINE_OF_SIGHT_ACTIVE: {
         property->value  = g_ctrlm.line_of_sight;
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: BINDING LINE OF SIGHT <%s>\n", __FUNCTION__, g_ctrlm.line_of_sight ? "ACTIVE" : "INACTIVE");
         break;
      }
      case CTRLM_PROPERTY_AUTOBIND_LINE_OF_SIGHT_ACTIVE: {
         property->value  = g_ctrlm.autobind;
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: AUTOBIND LINE OF SIGHT <%s>\n", __FUNCTION__, g_ctrlm.autobind ? "ACTIVE" : "INACTIVE");
         break;
      }
      case CTRLM_PROPERTY_ACTIVE_PERIOD_BUTTON: {
         property->value  = g_ctrlm.binding_button_timeout_val;
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: ACTIVE PERIOD BUTTON %lu ms\n", __FUNCTION__, property->value);
         break;
      }
      case CTRLM_PROPERTY_ACTIVE_PERIOD_SCREENBIND: {
         property->value  = g_ctrlm.screen_bind_timeout_val;
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: ACTIVE PERIOD SCREENBIND %lu ms\n", __FUNCTION__, property->value);
         break;
      }
      case CTRLM_PROPERTY_ACTIVE_PERIOD_ONE_TOUCH_AUTOBIND: {
         property->value  = g_ctrlm.one_touch_autobind_timeout_val;
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: ACTIVE PERIOD ONE-TOUCH AUTOBIND %lu ms\n", __FUNCTION__, property->value);
         break;
      }
      case CTRLM_PROPERTY_ACTIVE_PERIOD_LINE_OF_SIGHT: {
         property->value  = g_ctrlm.line_of_sight_timeout_val;
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: ACTIVE PERIOD LINE OF SIGHT %lu ms\n", __FUNCTION__, property->value);
         break;
      }
      case CTRLM_PROPERTY_VALIDATION_TIMEOUT_INITIAL: {
         property->value  = ctrlm_validation_timeout_initial_get();
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: VALIDATION TIMEOUT INITIAL %lu ms\n", __FUNCTION__, property->value);
         break;
      }
      case CTRLM_PROPERTY_VALIDATION_TIMEOUT_DURING: {
         property->value  = ctrlm_validation_timeout_subsequent_get();
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: VALIDATION TIMEOUT DURING %lu ms\n", __FUNCTION__, property->value);
         break;
      }
      case CTRLM_PROPERTY_VALIDATION_MAX_ATTEMPTS: {
         property->value  = ctrlm_validation_max_attempts_get();
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: VALIDATION MAX ATTEMPTS %lu times\n", __FUNCTION__, property->value);
         break;
      }
      case CTRLM_PROPERTY_CONFIGURATION_TIMEOUT: {
         property->value  = ctrlm_validation_timeout_configuration_get();
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: CONFIGURATION TIMEOUT %lu ms\n", __FUNCTION__, property->value);
         break;
      }
      case CTRLM_PROPERTY_AUTO_ACK: {
         property->value  = g_ctrlm.auto_ack;
         property->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         LOG_INFO("%s: AUTO ACK <%s>\n", __FUNCTION__, property->value ? "enabled" : "disabled");
         break;
      }
      default: {
         LOG_ERROR("%s: Invalid Property %d\n", __FUNCTION__, property->name);
         property->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      }
   }
}


gboolean ctrlm_main_iarm_call_discovery_config_set(ctrlm_main_iarm_call_discovery_config_t *config) {
   if(config == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_discovery_config_t *msg = (ctrlm_main_queue_msg_main_discovery_config_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_discovery_config_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_DISCOVERY_CONFIG_SET;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->config            = config;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process DISCOVERY_CONFIG_SET request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }
   
   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_discovery_config_set_(ctrlm_main_iarm_call_discovery_config_t *config) {
   if(config->network_id != CTRLM_MAIN_NETWORK_ID_ALL && !ctrlm_network_id_is_valid(config->network_id)) {
      config->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: network id - Out of range %u\n", __FUNCTION__, config->network_id);
      return;
   }
   config->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
   ctrlm_controller_discovery_config_t discovery_config;
   discovery_config.enabled               = config->enable ? true : false;
   discovery_config.require_line_of_sight = config->require_line_of_sight ? true : false;

   for(auto const &itr : g_ctrlm.networks) {
      if(config->network_id == itr.first || config->network_id == CTRLM_MAIN_NETWORK_ID_ALL) {
         itr.second->discovery_config_set(discovery_config);
         config->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
      }
   }
}

gboolean ctrlm_main_iarm_call_autobind_config_set(ctrlm_main_iarm_call_autobind_config_t *config) {
   if(config == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_autobind_config_t *msg = (ctrlm_main_queue_msg_main_autobind_config_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_autobind_config_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_AUTOBIND_CONFIG_SET;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->config            = config;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process AUTOBIND_CONFIG_SET request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_autobind_config_set_(ctrlm_main_iarm_call_autobind_config_t *config) {
   if(config->network_id != CTRLM_MAIN_NETWORK_ID_ALL && !ctrlm_network_id_is_valid(config->network_id)) {
      config->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: network id - Out of range %u\n", __FUNCTION__, config->network_id);
      return;
   }

   ctrlm_timeout_destroy(&g_ctrlm.one_touch_autobind_timeout_tag);

   if(config->threshold_pass < CTRLM_AUTOBIND_THRESHOLD_MIN || config->threshold_pass > CTRLM_AUTOBIND_THRESHOLD_MAX) {
      config->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: threshold pass - Out of range %u\n", __FUNCTION__, config->threshold_pass);
   } else if(config->threshold_fail < CTRLM_AUTOBIND_THRESHOLD_MIN || config->threshold_fail > CTRLM_AUTOBIND_THRESHOLD_MAX) {
      config->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: threshold fail - Out of range %u\n", __FUNCTION__, config->threshold_fail);
   } else {
      // If screenbind is enabled, disable it
      if(g_ctrlm.binding_screen_active == true) {
         g_ctrlm.binding_screen_active = false;
         LOG_INFO("%s: BINDING SCREEN <INACTIVE> -- Due to autobind config being set\n", __FUNCTION__);
      }

      config->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;

      ctrlm_controller_bind_config_t bind_config;
      bind_config.mode = CTRLM_PAIRING_MODE_ONE_TOUCH_AUTO_BIND;
      bind_config.data.autobind.enable         = (config->enable ? true : false);
      bind_config.data.autobind.pass_threshold = config->threshold_pass;
      bind_config.data.autobind.fail_threshold = config->threshold_fail;

      for(auto const &itr : g_ctrlm.networks) {
         if(config->network_id == itr.first || config->network_id == CTRLM_MAIN_NETWORK_ID_ALL) {
            itr.second->binding_config_set(bind_config);
            config->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         }
      }
      // Set a timer to stop one touch autobind mode
      g_ctrlm.one_touch_autobind_timeout_tag = ctrlm_timeout_create(g_ctrlm.one_touch_autobind_timeout_val, ctrlm_timeout_one_touch_autobind, NULL);
   }
}

gboolean ctrlm_main_iarm_call_precommission_config_set(ctrlm_main_iarm_call_precommision_config_t *config) {
   if(config == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_precommision_config_t *msg = (ctrlm_main_queue_msg_main_precommision_config_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_precommision_config_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_PRECOMMISSION_CONFIG_SET;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->config            = config;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process PRECOMISSION_CONFIG_SET request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_precommission_config_set_(ctrlm_main_iarm_call_precommision_config_t *config) {
   if(config->network_id != CTRLM_MAIN_NETWORK_ID_ALL && !ctrlm_network_id_is_valid(config->network_id)) {
      config->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: network id - Out of range %u\n", __FUNCTION__, config->network_id);
      return;
   }
   if(config->controller_qty > CTRLM_MAIN_MAX_BOUND_CONTROLLERS) {
      config->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: controller qty - Out of range %lu\n", __FUNCTION__, config->controller_qty);
      return;
   }
   unsigned long index;

   for(index = 0; index < config->controller_qty; index++) {
      g_ctrlm.precomission_table[config->controllers[index]] = true;
      LOG_INFO("%s: Adding 0x%016llX\n", __FUNCTION__, config->controllers[index]);
   }
   config->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
}

gboolean ctrlm_main_iarm_call_factory_reset(ctrlm_main_iarm_call_factory_reset_t *reset) {
   if(reset == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_factory_reset_t *msg = (ctrlm_main_queue_msg_main_factory_reset_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_factory_reset_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_FACTORY_RESET;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->reset             = reset;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process FACTORY_RESET request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_factory_reset_(ctrlm_main_iarm_call_factory_reset_t *reset) {
   if(reset->network_id != CTRLM_MAIN_NETWORK_ID_ALL && !ctrlm_network_id_is_valid(reset->network_id)) {
      reset->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: network id - Out of range %u\n", __FUNCTION__, reset->network_id);
      return;
   }

   for(auto const &itr : g_ctrlm.networks) {
      if(reset->network_id == itr.first || reset->network_id == CTRLM_MAIN_NETWORK_ID_ALL) {
         itr.second->factory_reset();
      }
   }

   ctrlm_recovery_factory_reset();

   reset->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
}

gboolean ctrlm_main_iarm_call_controller_unbind(ctrlm_main_iarm_call_controller_unbind_t *unbind) {
   if(unbind == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_controller_unbind_t *msg = (ctrlm_main_queue_msg_main_controller_unbind_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_controller_unbind_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

  sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROLLER_UNBIND;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->unbind            = unbind;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process CONTROLLER_UNBIND request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_controller_unbind_(ctrlm_main_iarm_call_controller_unbind_t *unbind) {
   if(!ctrlm_network_id_is_valid(unbind->network_id)) {
      unbind->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: network id - Out of range %u\n", __FUNCTION__, unbind->network_id);
      return;
   }

   ctrlm_obj_network_t *obj_net = g_ctrlm.networks[unbind->network_id];

   obj_net->controller_unbind(unbind->controller_id, CTRLM_UNBIND_REASON_TARGET_USER);
   unbind->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
}

gboolean ctrlm_main_iarm_call_chip_status_get(ctrlm_main_iarm_call_chip_status_t *status) {
   if(status == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_chip_status_t msg = {0};

   sem_init(&semaphore, 0, 0);

   msg.status            = status;
   msg.status->result    = CTRLM_IARM_CALL_RESULT_ERROR;
   msg.semaphore         = &semaphore;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_chip_status, &msg, sizeof(msg), NULL, status->network_id);

   // Wait for the result condition to be signaled
   sem_wait(&semaphore);

   sem_destroy(&semaphore);

   return(true);
}

void ctrlm_event_handler_ir(const char *owner, IARM_EventId_t event_id, void *data, size_t len)
{
   IARM_Bus_IRMgr_EventData_t *ir_event = (IARM_Bus_IRMgr_EventData_t *)data;

   switch(ir_event->data.irkey.keySrc) {
      case IARM_BUS_IRMGR_KEYSRC_IR: {
         int key_code = ir_event->data.irkey.keyCode;
         int key_type = ir_event->data.irkey.keyType;
         if(key_code == KED_SETUP && key_type == KET_KEYDOWN) { // Received IR Line of sight code
            LOG_INFO("%s: Setup IR code received.\n", __FUNCTION__);
            // Cancel active line of sight timer (if active)
            ctrlm_timeout_destroy(&g_ctrlm.line_of_sight_timeout_tag);

            if(!g_ctrlm.line_of_sight) {
               ctrlm_main_iarm_event_binding_line_of_sight(TRUE);
            }
            // Set line of sight as active
            g_ctrlm.line_of_sight = TRUE;

            // Set a timer to clear the line of sight after a period of time
            g_ctrlm.line_of_sight_timeout_tag = ctrlm_timeout_create(g_ctrlm.line_of_sight_timeout_val, ctrlm_timeout_line_of_sight, NULL);
            return;
         } else if(key_code == KED_RF_PAIR_GHOST && key_type == KET_KEYDOWN) { // Received IR autobinding ghost code
            LOG_INFO("%s: Autobind ghost code received.\n", __FUNCTION__);
            // Cancel active autobind timer (if active)
            ctrlm_timeout_destroy(&g_ctrlm.autobind_timeout_tag);

            if(!g_ctrlm.autobind) {
               ctrlm_main_iarm_event_autobind_line_of_sight(TRUE);
            }
            // Set autobind as active
            g_ctrlm.autobind = TRUE;

            // Set a timer to clear the autobind after a period of time
            g_ctrlm.autobind_timeout_tag = ctrlm_timeout_create(g_ctrlm.autobind_timeout_val, ctrlm_timeout_autobind, NULL);
            return;
         }
      break;
      }
      case IARM_BUS_IRMGR_KEYSRC_FP: {
         if(0) { // TODO Pairing button was pressed
            // Cancel active line of sight timer (if active)
            ctrlm_timeout_destroy(&g_ctrlm.binding_button_timeout_tag);

            if(!g_ctrlm.binding_button) {
               ctrlm_main_iarm_event_binding_button(TRUE);
            }
            // Set button binding as active
            g_ctrlm.binding_button = TRUE;

            // Set a timer to clear the line of sight after a period of time
            g_ctrlm.binding_button_timeout_tag = ctrlm_timeout_create(g_ctrlm.binding_button_timeout_val, ctrlm_timeout_binding_button, NULL);
         }
         return;
      }
      case IARM_BUS_IRMGR_KEYSRC_RF: {
         LOG_DEBUG("%s: RF key source received.\n", __FUNCTION__);
         break;
      }
      default: {
         LOG_WARN("%s: Other key source received. 0x%08x\n", __FUNCTION__, ir_event->data.irkey.keySrc);
         break;
      }
   }

   //Listen for control events
   if(event_id == IARM_BUS_IRMGR_EVENT_CONTROL)
   {
      IARM_Bus_IRMgr_EventData_t *ir_event_data           = (IARM_Bus_IRMgr_EventData_t*)data;
      int key_code                                        = ir_event_data->data.irkey.keyCode;
      int key_src                                         = ir_event_data->data.irkey.keySrc;
      int key_tag                                         = ir_event_data->data.irkey.keyTag;
      int controller_id                                   = ir_event_data->data.irkey.keySourceId;
      bool need_to_update_db                              = false;
      bool need_to_update_last_key_info                   = false;
      bool send_on_control_event                          = false;
      ctrlm_ir_remote_type old_remote_type                = g_ctrlm.last_key_info.last_ir_remote_type;
      ctrlm_remote_keypad_config old_remote_keypad_config = g_ctrlm.last_key_info.remote_keypad_config;
      guchar last_source_type                             = g_ctrlm.last_key_info.source_type;
      gboolean  write_last_key_info                       = false;
      char last_source_name[CTRLM_RCU_RIB_ATTR_LEN_PRODUCT_NAME];
      char source_name[CTRLM_RCU_RIB_ATTR_LEN_PRODUCT_NAME];
      ctrlm_remote_keypad_config remote_keypad_config;
      string type;
      string data;
      errno_t safec_rc = -1;
      int ind = -1;

      safec_rc = strcpy_s(last_source_name, sizeof(last_source_name), g_ctrlm.last_key_info.source_name);
      ERR_CHK(safec_rc);

      if ((key_src != IARM_BUS_IRMGR_KEYSRC_IR) && (key_src != IARM_BUS_IRMGR_KEYSRC_RF)) {
         // NOTE: For now, we are explicitly ignoring keypresses from IP or FP sources!
         return;
      }

      //Check for day change
      need_to_update_db = ctrlm_main_handle_day_change_ir_remote_usage();

      if (len != sizeof(IARM_Bus_IRMgr_EventData_t)) {
         LOG_ERROR("%s: ERROR - Got IARM_BUS_IRMGR_EVENT_CONTROL event with bad data length <%u>, should be <%u>!!\n",
                       __FUNCTION__, len, sizeof(IARM_Bus_IRMgr_EventData_t));
         return;
      }

      switch(key_code) {
         case KED_XR2V3:
         case KED_XR5V2:
         case KED_XR11V1:
         case KED_XR11V2: {
            g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_XR11V2;
            g_ctrlm.last_key_info.remote_keypad_config = CTRLM_REMOTE_KEYPAD_CONFIG_HAS_SETUP_KEY_WITH_NUMBER_KEYS;
            g_ctrlm.last_key_info.source_type          = IARM_BUS_IRMGR_KEYSRC_IR;
            g_ctrlm.last_key_info.is_screen_bind_mode  = false;
            type                                       = "irRemote";
            data                                       = std::to_string(g_ctrlm.last_key_info.remote_keypad_config);
            send_on_control_event                      = true;
            need_to_update_last_key_info               = true;
            safec_rc = strncpy_s(source_name, sizeof(source_name), ctrlm_rcu_ir_remote_types_str(g_ctrlm.last_key_info.last_ir_remote_type), CTRLM_RCU_RIB_ATTR_LEN_PRODUCT_NAME - 1);
            ERR_CHK(safec_rc);
            source_name[CTRLM_RCU_RIB_ATTR_LEN_PRODUCT_NAME - 1] = '\0';
            switch(key_code) {
               case KED_XR2V3:
                  if(!g_ctrlm.ir_remote_usage_today.has_ir_xr2) {
                     g_ctrlm.ir_remote_usage_today.has_ir_xr2 = true;
                     need_to_update_db = true;
                  }
                  break;
               case KED_XR5V2:
                  if(!g_ctrlm.ir_remote_usage_today.has_ir_xr5) {
                     g_ctrlm.ir_remote_usage_today.has_ir_xr5 = true;
                     need_to_update_db = true;
                  }
                  break;
               case KED_XR11V1:
               case KED_XR11V2:
                  if(!g_ctrlm.ir_remote_usage_today.has_ir_xr11) {
                     g_ctrlm.ir_remote_usage_today.has_ir_xr11 = true;
                     need_to_update_db = true;
                  }
                  break;
            }
            break;
         }

         case KED_XR11_NOTIFY:
            g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_XR11V2;
            g_ctrlm.last_key_info.remote_keypad_config = CTRLM_REMOTE_KEYPAD_CONFIG_HAS_SETUP_KEY_WITH_NUMBER_KEYS;
            g_ctrlm.last_key_info.source_type          = IARM_BUS_IRMGR_KEYSRC_IR;
            g_ctrlm.last_key_info.is_screen_bind_mode  = false;
            LOG_INFO("%s: Received KED_XR11_NOTIFY\n", __FUNCTION__);
            if((!g_ctrlm.ir_remote_usage_today.has_ir_remote) || (old_remote_type != g_ctrlm.last_key_info.last_ir_remote_type) || (old_remote_keypad_config != g_ctrlm.last_key_info.remote_keypad_config)) {
               //Don't use has_ir_xr11 because this could be XR2 or XR5.  Use has_ir_remote.
               g_ctrlm.ir_remote_usage_today.has_ir_remote = true;
               need_to_update_db = true;
            }
            break;
         case KED_XR15V1_NOTIFY:
            g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_XR15V1;
            g_ctrlm.last_key_info.remote_keypad_config = CTRLM_REMOTE_KEYPAD_CONFIG_HAS_NO_SETUP_KEY_WITH_NUMBER_KEYS;
            g_ctrlm.last_key_info.source_type          = IARM_BUS_IRMGR_KEYSRC_IR;
            LOG_INFO("%s: Received KED_XR15V1_NOTIFY\n", __FUNCTION__);
            if((!g_ctrlm.ir_remote_usage_today.has_ir_xr15) || (old_remote_type != g_ctrlm.last_key_info.last_ir_remote_type) || (old_remote_keypad_config != g_ctrlm.last_key_info.remote_keypad_config)) {
               g_ctrlm.ir_remote_usage_today.has_ir_xr15 = true;
               need_to_update_db = true;
            }
            break;
         case KED_XR16V1_NOTIFY:
            g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_NA;
            g_ctrlm.last_key_info.remote_keypad_config = CTRLM_REMOTE_KEYPAD_CONFIG_HAS_NO_SETUP_KEY_WITH_NO_NUMBER_KEYS;
            g_ctrlm.last_key_info.source_type          = IARM_BUS_IRMGR_KEYSRC_IR;
            LOG_INFO("%s: Received KED_XR16V1_NOTIFY\n", __FUNCTION__);
            if((!g_ctrlm.ir_remote_usage_today.has_ir_remote) || (old_remote_type != g_ctrlm.last_key_info.last_ir_remote_type) || (old_remote_keypad_config != g_ctrlm.last_key_info.remote_keypad_config)) {
               g_ctrlm.ir_remote_usage_today.has_ir_remote = true;
               need_to_update_db = true;
            }
            break;
         case KED_SCREEN_BIND_NOTIFY:
            g_ctrlm.last_key_info.is_screen_bind_mode = true;
            LOG_INFO("%s: Received KED_SCREEN_BIND_NOTIFY\n", __FUNCTION__);
            break;
         case KED_VOLUMEUP:
         case KED_VOLUMEDOWN:
         case KED_MUTE:
         case KED_INPUTKEY:
         case KED_POWER:
         case KED_TVPOWER:
         case KED_RF_POWER:
         case KED_DISCRETE_POWER_ON:
         case KED_DISCRETE_POWER_STANDBY: {
            send_on_control_event        = true;
            need_to_update_last_key_info = true;
            type                         = "TV";
            break;
         }
         case KED_PUSH_TO_TALK:
            send_on_control_event        = true;
            need_to_update_last_key_info = true;
            type                         = "mic";
            break;
         default:
            break;
      }

      if(key_src == IARM_BUS_IRMGR_KEYSRC_RF) {
         ctrlm_controller_product_name_get(controller_id, source_name);
         remote_keypad_config = ctrlm_get_remote_keypad_config(source_name);
      } else {
         remote_keypad_config = g_ctrlm.last_key_info.remote_keypad_config;
         controller_id = -1;
         //Check to see if the tag was included.  If so, use it.
         ctrlm_check_for_key_tag(key_tag);
         safec_rc = strcpy_s(source_name, sizeof(source_name), ctrlm_rcu_ir_remote_types_str(g_ctrlm.last_key_info.last_ir_remote_type));
         ERR_CHK(safec_rc);
      }

      LOG_INFO("%s: Got IARM_BUS_IRMGR_EVENT_CONTROL event, controller_id <%d>, controlCode <0x%02X>, src <%d>.\n",
                  __FUNCTION__, controller_id, (unsigned int)key_code, key_src);

      //Only save the last key info to the db if the source type (IR or RF) or the source name (XR11, XR15) have changed
      //It's not worth the writes to the db for every key.  It is important to know if we are IR/RF and XR11/XR15
      bool isValid = false;
      (last_source_type != key_src) ? isValid = true : (safec_rc = strcmp_s(last_source_name, sizeof(last_source_name), source_name, &ind));
      if((isValid) || ((safec_rc == EOK) && (ind != 0))) {
          write_last_key_info = true;
      } else if(safec_rc != EOK) {
        ERR_CHK(safec_rc);
      }

      if(send_on_control_event) {
         data = std::to_string(remote_keypad_config);
         ctrlm_rcu_iarm_event_control(controller_id, key_source_names[key_src], type.c_str(), data.c_str(), key_code, 0);
      }

      if(need_to_update_last_key_info) {
         ctrlm_update_last_key_info(controller_id, key_src, key_code, source_name, g_ctrlm.last_key_info.is_screen_bind_mode, write_last_key_info);
      }

      if(need_to_update_db) {
         ctrlm_property_write_ir_remote_usage();
      }
   }
   //Listen for key events to keep track of last key info 
   else if (event_id == IARM_BUS_IRMGR_EVENT_IRKEY)
   {
      IARM_Bus_IRMgr_EventData_t *ir_event_data = (IARM_Bus_IRMgr_EventData_t*)data;
      int      key_code                              = ir_event_data->data.irkey.keyCode;
      int      key_src                               = ir_event_data->data.irkey.keySrc;
      int      key_tag                               = ir_event_data->data.irkey.keyTag;
      int      controller_id                         = ir_event_data->data.irkey.keySourceId;
      guchar   last_source_type                      = g_ctrlm.last_key_info.source_type;
      gboolean  write_last_key_info                  = false;
      char last_source_name[CTRLM_RCU_RIB_ATTR_LEN_PRODUCT_NAME];
      char source_name[CTRLM_RCU_RIB_ATTR_LEN_PRODUCT_NAME];

      errno_t safec_rc = -1;
      int ind = -1;

      safec_rc = strcpy_s(last_source_name, sizeof(last_source_name), g_ctrlm.last_key_info.source_name);
      ERR_CHK(safec_rc);

      if ((ir_event_data->data.irkey.keyType == KET_KEYUP) || (ir_event_data->data.irkey.keyType == KET_KEYREPEAT)) {
         // Don't remember keyup or repeat events - only use keydown.
         return;
      }

      if ((key_src != IARM_BUS_IRMGR_KEYSRC_IR) && (key_src != IARM_BUS_IRMGR_KEYSRC_RF)) {
         // NOTE: For now, we are explicitly ignoring keypresses from IP or FP sources!
         return;
      }

      if (key_src != IARM_BUS_IRMGR_KEYSRC_RF) {
         controller_id = -1;
      }

      if(key_src == IARM_BUS_IRMGR_KEYSRC_RF) {
         ctrlm_controller_product_name_get(controller_id, source_name);
      } else {
         controller_id = -1;
         //Check to see if the tag was included.  If so, use it.
         ctrlm_check_for_key_tag(key_tag);
         safec_rc = strcpy_s(source_name, sizeof(source_name), ctrlm_rcu_ir_remote_types_str(g_ctrlm.last_key_info.last_ir_remote_type));
         ERR_CHK(safec_rc);
      }

      if (len != sizeof(IARM_Bus_IRMgr_EventData_t)) {
         LOG_ERROR("%s: ERROR - Got IARM_BUS_IRMGR_EVENT_IRKEY event with bad data length <%u>, should be <%u>!!\n",
                         __FUNCTION__, len, sizeof(IARM_Bus_IRMgr_EventData_t));
         return;
      }

      //Only save the last key info to the db if the source type (IR or RF) or the source name (XR11, XR15) have changed
      //It's not worth the writes to the db for every key.  It is important to know if we are IR/RF and XR11/XR15
      bool isValid = false;
      (last_source_type != key_src) ? isValid = true : (safec_rc = strcmp_s(last_source_name, sizeof(last_source_name), source_name, &ind));
      if((isValid) || ((safec_rc == EOK) && (ind != 0))) {
          write_last_key_info = true;
      } else if(safec_rc != EOK) {
        ERR_CHK(safec_rc);
      }

      ctrlm_update_last_key_info(controller_id, key_src, key_code, source_name, g_ctrlm.last_key_info.is_screen_bind_mode, write_last_key_info);
   }
}

void ctrlm_check_for_key_tag(int key_tag) {
   switch(key_tag) {
      case XMP_TAG_PLATCO: {
         g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_PLATCO;
         break;
      }
      case XMP_TAG_XR11V2: {
         g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_XR11V2;
         break;
      }
      case XMP_TAG_XR15V1: {
         g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_XR15V1;
         break;
      }
      case XMP_TAG_XR15V2: {
         g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_XR15V2;
         break;
      }
      case XMP_TAG_XR16V1: {
         g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_XR16V1;
         break;
      }
      case XMP_TAG_XRAV1: {
         g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_XRAV1;
         break;
      }
      case XMP_TAG_XR20V1: {
         g_ctrlm.last_key_info.last_ir_remote_type  = CTRLM_IR_REMOTE_TYPE_XR20V1;
         break;
      }
      case XMP_TAG_COMCAST:
      case XMP_TAG_UNDEFINED:
      default: {
         break;
      }
   }
   if(key_tag != XMP_TAG_COMCAST) {
      LOG_INFO("%s: key_tag <%s>\n", __FUNCTION__, ctrlm_rcu_ir_remote_types_str(g_ctrlm.last_key_info.last_ir_remote_type));
   }
}

void ctrlm_event_handler_system(const char *owner, IARM_EventId_t event_id, void *data, size_t len)
{
   // Handle SYSMGR Events
   if(!strncmp(owner, IARM_BUS_SYSMGR_NAME, strlen(IARM_BUS_SYSMGR_NAME))) {
      switch(event_id) {
         case IARM_BUS_SYSMGR_EVENT_KEYCODE_LOGGING_CHANGED: {
               IARM_Bus_SYSMgr_EventData_t *system_event = (IARM_Bus_SYSMgr_EventData_t *)data;

               g_ctrlm.mask_key_codes_iarm = (system_event->data.keyCodeLogData.logStatus == 0);
               LOG_INFO("%s: Keycode mask logging change event <%s>.\n", __FUNCTION__, g_ctrlm.mask_key_codes_iarm ? "ON" : "OFF");
               break;
         }
         default: {
            LOG_INFO("%s: SYSMGR Event ID %u is not implemented\n", __FUNCTION__, event_id);
            break;
         }
      }
   } 
   else {
      LOG_INFO("%s: Event handler for component %s are not implemented\n", __FUNCTION__, owner);
   }
}

gboolean ctrlm_timeout_line_of_sight(gpointer user_data) {
   LOG_INFO("%s: Timeout - Line of sight.\n", __FUNCTION__);
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_header_t *msg = (ctrlm_main_queue_msg_header_t *)g_malloc(sizeof(ctrlm_main_queue_msg_header_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      g_ctrlm.line_of_sight_timeout_tag = 0;
      return(FALSE);
   }

   msg->type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_TIMEOUT_LINE_OF_SIGHT;
   msg->network_id = CTRLM_MAIN_NETWORK_ID_ALL;

   ctrlm_main_queue_msg_push(msg);
   g_ctrlm.line_of_sight_timeout_tag = 0;
   return(FALSE);
}

gboolean ctrlm_timeout_autobind(gpointer user_data) {
   LOG_INFO("%s: Timeout - Autobind.\n", __FUNCTION__);
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_header_t *msg = (ctrlm_main_queue_msg_header_t *)g_malloc(sizeof(ctrlm_main_queue_msg_header_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      g_ctrlm.autobind_timeout_tag = 0;
      return(FALSE);
   }

   msg->type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_TIMEOUT_AUTOBIND;
   msg->network_id = CTRLM_MAIN_NETWORK_ID_ALL;

   ctrlm_main_queue_msg_push(msg);
   g_ctrlm.autobind_timeout_tag = 0;
   return(FALSE);
}

gboolean ctrlm_timeout_binding_button(gpointer user_data) {
   LOG_INFO("%s: Timeout - Binding button.\n", __FUNCTION__);
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_close_pairing_window_t *msg = (ctrlm_main_queue_msg_close_pairing_window_t *)g_malloc(sizeof(ctrlm_main_queue_msg_close_pairing_window_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      g_ctrlm.binding_button_timeout_tag = 0;
      return(FALSE);
   }

   msg->type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CLOSE_PAIRING_WINDOW;
   msg->network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->reason     = CTRLM_CLOSE_PAIRING_WINDOW_REASON_TIMEOUT;

   ctrlm_main_queue_msg_push(msg);
   g_ctrlm.binding_button_timeout_tag = 0;
   return(FALSE);
}

gboolean ctrlm_timeout_screen_bind(gpointer user_data) {
   LOG_INFO("%s: Timeout - Screen Bind.\n", __FUNCTION__);
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_close_pairing_window_t *msg = (ctrlm_main_queue_msg_close_pairing_window_t *)g_malloc(sizeof(ctrlm_main_queue_msg_close_pairing_window_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      g_ctrlm.screen_bind_timeout_tag = 0;
      return(FALSE);
   }

   msg->type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CLOSE_PAIRING_WINDOW;
   msg->network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->reason     = CTRLM_CLOSE_PAIRING_WINDOW_REASON_TIMEOUT;

   ctrlm_main_queue_msg_push(msg);
   g_ctrlm.screen_bind_timeout_tag = 0;
   return(FALSE);
}

gboolean ctrlm_timeout_one_touch_autobind(gpointer user_data) {
   LOG_INFO("%s: Timeout - One touch Autobind.\n", __FUNCTION__);
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_close_pairing_window_t *msg = (ctrlm_main_queue_msg_close_pairing_window_t *)g_malloc(sizeof(ctrlm_main_queue_msg_close_pairing_window_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      g_ctrlm.one_touch_autobind_timeout_tag = 0;
      return(FALSE);
   }

   msg->type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CLOSE_PAIRING_WINDOW;
   msg->network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->reason     = CTRLM_CLOSE_PAIRING_WINDOW_REASON_TIMEOUT;

   ctrlm_main_queue_msg_push(msg);
   g_ctrlm.one_touch_autobind_timeout_tag = 0;
   return(FALSE);
}

void ctrlm_stop_binding_button(void) {
   LOG_INFO("%s: Binding button stopped.\n", __FUNCTION__);
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_header_t *msg = (ctrlm_main_queue_msg_header_t *)g_malloc(sizeof(ctrlm_main_queue_msg_header_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return;
   }

   msg->type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STOP_BINDING_BUTTON;
   msg->network_id = CTRLM_MAIN_NETWORK_ID_ALL;

   ctrlm_main_queue_msg_push(msg);
}

void ctrlm_stop_binding_screen(void) {
   LOG_INFO("%s: Screen bind mode stopped.\n", __FUNCTION__);
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_header_t *msg = (ctrlm_main_queue_msg_header_t *)g_malloc(sizeof(ctrlm_main_queue_msg_header_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return;
   }

   msg->type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STOP_BINDING_SCREEN;
   msg->network_id = CTRLM_MAIN_NETWORK_ID_ALL;

   ctrlm_main_queue_msg_push(msg);
}

void ctrlm_stop_one_touch_autobind(void) {
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_header_t *msg = (ctrlm_main_queue_msg_header_t *)g_malloc(sizeof(ctrlm_main_queue_msg_header_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return;
   }

   msg->type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_STOP_ONE_TOUCH_AUTOBIND;
   msg->network_id = CTRLM_MAIN_NETWORK_ID_ALL;

   ctrlm_main_queue_msg_push(msg);
}

void ctrlm_close_pairing_window(ctrlm_close_pairing_window_reason reason) {
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_close_pairing_window_t *msg = (ctrlm_main_queue_msg_close_pairing_window_t *)g_malloc(sizeof(ctrlm_main_queue_msg_close_pairing_window_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return;
   }

   msg->type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CLOSE_PAIRING_WINDOW;
   msg->network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->reason     = reason;

   ctrlm_main_queue_msg_push(msg);
}

void ctrlm_pairing_window_bind_status_set(ctrlm_bind_status_t bind_status) {
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_pairing_window_bind_status_t *msg = (ctrlm_main_queue_msg_pairing_window_bind_status_t *)g_malloc(sizeof(ctrlm_main_queue_msg_pairing_window_bind_status_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      g_ctrlm.binding_button_timeout_tag = 0;
      return;
   }

   msg->type        = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_BIND_STATUS_SET;
   msg->bind_status = bind_status;

   ctrlm_main_queue_msg_push(msg);
}

void ctrlm_discovery_remote_type_set(const char *remote_type_str) {
   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_discovery_remote_type_t *msg = (ctrlm_main_queue_msg_discovery_remote_type_t *)g_malloc(sizeof(ctrlm_main_queue_msg_discovery_remote_type_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return;
   }

   msg->type            = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_DISCOVERY_REMOTE_TYPE_SET;
   errno_t safec_rc = strcpy_s(msg->remote_type_str, sizeof(msg->remote_type_str), remote_type_str);
   ERR_CHK(safec_rc);

   ctrlm_main_queue_msg_push(msg);
}

const char* ctrlm_minidump_path_get() {
    return g_ctrlm.minidump_path.c_str();
}

void ctrlm_main_sat_enabled_set(gboolean sat_enabled) {
   g_ctrlm.sat_enabled = sat_enabled;
}

void ctrlm_main_invalidate_service_access_token(void) {
   if(g_ctrlm.sat_enabled) {
      sem_wait(&g_ctrlm.service_access_token_semaphore);
      // We want to poll for SAT token whether we have it or not
      LOG_INFO("%s: Invalidating SAT Token...\n", __FUNCTION__);
      g_ctrlm.has_service_access_token = false;
      #ifdef AUTH_ENABLED
      ctrlm_timeout_destroy(&g_ctrlm.service_access_token_expiration_tag);
      ctrlm_timeout_destroy(&g_ctrlm.authservice_poll_tag);
      g_ctrlm.authservice_poll_tag = ctrlm_timeout_create(g_ctrlm.recently_booted ? g_ctrlm.authservice_fast_poll_val : g_ctrlm.authservice_poll_val,
                                                               ctrlm_authservice_poll,
                                                               NULL);
      #endif
      sem_post(&g_ctrlm.service_access_token_semaphore);
   }
}

gboolean ctrlm_main_successful_init_get(void) {
   return(g_ctrlm.successful_init);
}

ctrlm_irdb_t* ctrlm_main_irdb_get() {
   return(g_ctrlm.irdb);
}

ctrlm_auth_t* ctrlm_main_auth_get() {
#ifdef AUTH_ENABLED
   return(g_ctrlm.authservice);
#else
   return(NULL);
#endif
}

string ctrlm_receiver_id_get(){
   return g_ctrlm.receiver_id;
}

string ctrlm_device_id_get(){
   return g_ctrlm.device_id;
}

string ctrlm_stb_name_get(){
   return g_ctrlm.stb_name;
}

string ctrlm_device_mac_get() {
   return g_ctrlm.device_mac;
}

gboolean ctrlm_main_handle_day_change_ir_remote_usage() {
   time_t time_in_seconds = time(NULL);
   if(time_in_seconds < g_ctrlm.shutdown_time) {
      LOG_WARN("%s: Current Time <%ld> is less than the last shutdown time <%ld>.  Wait until time updates.\n", __FUNCTION__, time_in_seconds, g_ctrlm.shutdown_time);
      return(false);
   }
   guint32 today = time_in_seconds / (60 * 60 * 24);
   guint32 day_change = today - g_ctrlm.today;
   LOG_INFO("%s: today <%u> g_ctrlm.today <%u>.\n", __FUNCTION__, today, g_ctrlm.today);

   //If this is a different day...
   if(day_change != 0) {

      //If this is the next day...
      if(day_change == 1) {
         g_ctrlm.ir_remote_usage_yesterday.has_ir_xr2    = g_ctrlm.ir_remote_usage_today.has_ir_xr2;
         g_ctrlm.ir_remote_usage_yesterday.has_ir_xr5    = g_ctrlm.ir_remote_usage_today.has_ir_xr5;
         g_ctrlm.ir_remote_usage_yesterday.has_ir_xr11   = g_ctrlm.ir_remote_usage_today.has_ir_xr11;
         g_ctrlm.ir_remote_usage_yesterday.has_ir_xr15   = g_ctrlm.ir_remote_usage_today.has_ir_xr15;
         g_ctrlm.ir_remote_usage_yesterday.has_ir_remote = g_ctrlm.ir_remote_usage_today.has_ir_remote;
      } else {
         g_ctrlm.ir_remote_usage_yesterday.has_ir_xr2    = false;
         g_ctrlm.ir_remote_usage_yesterday.has_ir_xr5    = false;
         g_ctrlm.ir_remote_usage_yesterday.has_ir_xr11   = false;
         g_ctrlm.ir_remote_usage_yesterday.has_ir_xr15   = false;
         g_ctrlm.ir_remote_usage_yesterday.has_ir_remote = false;
      }

      g_ctrlm.ir_remote_usage_today.has_ir_xr2    = false;
      g_ctrlm.ir_remote_usage_today.has_ir_xr5    = false;
      g_ctrlm.ir_remote_usage_today.has_ir_xr11   = false;
      g_ctrlm.ir_remote_usage_today.has_ir_xr15   = false;
      g_ctrlm.ir_remote_usage_today.has_ir_remote = false;
      g_ctrlm.today = today;

      return(true);
   }

   return(false);
}

void ctrlm_property_write_ir_remote_usage(void) {
   guchar data[CTRLM_RF4CE_LEN_IR_REMOTE_USAGE];
   data[0]  = (guchar)(g_ctrlm.ir_remote_usage_today.has_ir_xr2);
   data[1]  = (guchar)(g_ctrlm.ir_remote_usage_today.has_ir_xr5);
   data[2]  = (guchar)(g_ctrlm.ir_remote_usage_today.has_ir_xr11);
   data[3]  = (guchar)(g_ctrlm.ir_remote_usage_today.has_ir_xr15);
   data[4]  = (guchar)(g_ctrlm.ir_remote_usage_today.has_ir_remote);
   data[5]  = (guchar)(g_ctrlm.ir_remote_usage_yesterday.has_ir_xr2);
   data[6]  = (guchar)(g_ctrlm.ir_remote_usage_yesterday.has_ir_xr5);
   data[7]  = (guchar)(g_ctrlm.ir_remote_usage_yesterday.has_ir_xr11);
   data[8]  = (guchar)(g_ctrlm.ir_remote_usage_yesterday.has_ir_xr15);
   data[9]  = (guchar)(g_ctrlm.ir_remote_usage_yesterday.has_ir_remote);
   data[10] = (guchar)(g_ctrlm.today);
   data[11] = (guchar)(g_ctrlm.today >> 8);
   data[12] = (guchar)(g_ctrlm.today >> 16);
   data[13] = (guchar)(g_ctrlm.today >> 24);

   LOG_INFO("%s: Has XR2  IR today <%d> yesterday <%d>\n", __FUNCTION__, g_ctrlm.ir_remote_usage_today.has_ir_xr2, g_ctrlm.ir_remote_usage_yesterday.has_ir_xr2);
   LOG_INFO("%s: Has XR5  IR today <%d> yesterday <%d>\n", __FUNCTION__, g_ctrlm.ir_remote_usage_today.has_ir_xr5, g_ctrlm.ir_remote_usage_yesterday.has_ir_xr5);
   LOG_INFO("%s: Has XR11 IR today <%d> yesterday <%d>\n", __FUNCTION__, g_ctrlm.ir_remote_usage_today.has_ir_xr11, g_ctrlm.ir_remote_usage_yesterday.has_ir_xr11);
   LOG_INFO("%s: Has XR15 IR today <%d> yesterday <%d>\n", __FUNCTION__, g_ctrlm.ir_remote_usage_today.has_ir_xr15, g_ctrlm.ir_remote_usage_yesterday.has_ir_xr15);
   LOG_INFO("%s: Has IR Remote: today <%d> yesterday <%d>\n", __FUNCTION__, g_ctrlm.ir_remote_usage_today.has_ir_remote, g_ctrlm.ir_remote_usage_yesterday.has_ir_remote);

   ctrlm_db_ir_remote_usage_write(data, CTRLM_RF4CE_LEN_IR_REMOTE_USAGE);
}

guchar ctrlm_property_write_ir_remote_usage(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_LEN_IR_REMOTE_USAGE) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   ctrlm_ir_remote_usage ir_remote_usage_today;
   ctrlm_ir_remote_usage ir_remote_usage_yesterday;
   guint32               today;
   gboolean              ir_remote_usage_changed = false;
   gboolean              day_changed             = false;

   ir_remote_usage_today.has_ir_xr2        = data[0];
   ir_remote_usage_today.has_ir_xr5        = data[1];
   ir_remote_usage_today.has_ir_xr11       = data[2];
   ir_remote_usage_today.has_ir_xr15       = data[3];
   ir_remote_usage_today.has_ir_remote     = data[4];
   ir_remote_usage_yesterday.has_ir_xr2    = data[5];
   ir_remote_usage_yesterday.has_ir_xr5    = data[6];
   ir_remote_usage_yesterday.has_ir_xr11   = data[7];
   ir_remote_usage_yesterday.has_ir_xr15   = data[8];
   ir_remote_usage_yesterday.has_ir_remote = data[9];
   today                                   = ((data[13] << 24) | (data[12] << 16) | (data[11] << 8) | data[10]);
   
   if(g_ctrlm.ir_remote_usage_today.has_ir_xr2        != ir_remote_usage_today.has_ir_xr2       ||
      g_ctrlm.ir_remote_usage_yesterday.has_ir_xr2    != ir_remote_usage_yesterday.has_ir_xr2   ||
      g_ctrlm.ir_remote_usage_today.has_ir_xr5        != ir_remote_usage_today.has_ir_xr5       ||
      g_ctrlm.ir_remote_usage_yesterday.has_ir_xr5    != ir_remote_usage_yesterday.has_ir_xr5   ||
      g_ctrlm.ir_remote_usage_today.has_ir_xr11       != ir_remote_usage_today.has_ir_xr11      ||
      g_ctrlm.ir_remote_usage_yesterday.has_ir_xr11   != ir_remote_usage_yesterday.has_ir_xr11  ||
      g_ctrlm.ir_remote_usage_today.has_ir_xr15       != ir_remote_usage_today.has_ir_xr15      ||
      g_ctrlm.ir_remote_usage_yesterday.has_ir_xr15   != ir_remote_usage_yesterday.has_ir_xr15  ||
      g_ctrlm.ir_remote_usage_today.has_ir_remote     != ir_remote_usage_today.has_ir_remote    ||
      g_ctrlm.ir_remote_usage_yesterday.has_ir_remote != ir_remote_usage_yesterday.has_ir_remote ||
      g_ctrlm.today                                   != today) {
      // Store the data
      g_ctrlm.ir_remote_usage_today.has_ir_xr2         = ir_remote_usage_today.has_ir_xr2;
      g_ctrlm.ir_remote_usage_yesterday.has_ir_xr2     = ir_remote_usage_yesterday.has_ir_xr2;
      g_ctrlm.ir_remote_usage_today.has_ir_xr5         = ir_remote_usage_today.has_ir_xr5;
      g_ctrlm.ir_remote_usage_yesterday.has_ir_xr5     = ir_remote_usage_yesterday.has_ir_xr5;
      g_ctrlm.ir_remote_usage_today.has_ir_xr11        = ir_remote_usage_today.has_ir_xr11;
      g_ctrlm.ir_remote_usage_yesterday.has_ir_xr11    = ir_remote_usage_yesterday.has_ir_xr11;
      g_ctrlm.ir_remote_usage_today.has_ir_xr15        = ir_remote_usage_today.has_ir_xr15;
      g_ctrlm.ir_remote_usage_yesterday.has_ir_xr15    = ir_remote_usage_yesterday.has_ir_xr15;
      g_ctrlm.ir_remote_usage_today.has_ir_remote      = ir_remote_usage_today.has_ir_remote;
      g_ctrlm.ir_remote_usage_yesterday.has_ir_remote  = ir_remote_usage_yesterday.has_ir_remote;
      g_ctrlm.today                                    = today;
      ir_remote_usage_changed                          = true;
   }

   day_changed = ctrlm_main_handle_day_change_ir_remote_usage();

   if(day_changed || (!g_ctrlm.loading_db && ir_remote_usage_changed)) {
      ctrlm_property_write_ir_remote_usage();
   }

   return(length);
}

void ctrlm_property_read_ir_remote_usage(void) {
   guchar *data = NULL;
   guint32 length;
   ctrlm_db_ir_remote_usage_read(&data, &length);

   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - IR Remote Usage\n", __FUNCTION__);
   } else {
      ctrlm_property_write_ir_remote_usage(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   LOG_INFO("%s: Has XR2  IR today <%d> yesterday <%d>\n", __FUNCTION__, g_ctrlm.ir_remote_usage_today.has_ir_xr2, g_ctrlm.ir_remote_usage_yesterday.has_ir_xr2);
   LOG_INFO("%s: Has XR5  IR today <%d> yesterday <%d>\n", __FUNCTION__, g_ctrlm.ir_remote_usage_today.has_ir_xr5, g_ctrlm.ir_remote_usage_yesterday.has_ir_xr5);
   LOG_INFO("%s: Has XR11 IR today <%d> yesterday <%d>\n", __FUNCTION__, g_ctrlm.ir_remote_usage_today.has_ir_xr11, g_ctrlm.ir_remote_usage_yesterday.has_ir_xr11);
   LOG_INFO("%s: Has XR15 IR today <%d> yesterday <%d>\n", __FUNCTION__, g_ctrlm.ir_remote_usage_today.has_ir_xr15, g_ctrlm.ir_remote_usage_yesterday.has_ir_xr15);
   LOG_INFO("%s: Has IR Remote:  today <%d> yesterday <%d>\n", __FUNCTION__, g_ctrlm.ir_remote_usage_today.has_ir_remote, g_ctrlm.ir_remote_usage_yesterday.has_ir_remote);
}

void ctrlm_property_write_pairing_metrics(void) {
   guchar data[CTRLM_RF4CE_LEN_PAIRING_METRICS];
   errno_t safec_rc = memcpy_s(data, sizeof(data), &g_ctrlm.pairing_metrics, CTRLM_RF4CE_LEN_PAIRING_METRICS);
   ERR_CHK(safec_rc);

   LOG_INFO("%s: num_screenbind_failures <%lu>, last_screenbind_error_timestamp <%lums>, last_screenbind_error_code <%d>, last_screenbind_remote_type <%s>\n",
      __FUNCTION__, g_ctrlm.pairing_metrics.num_screenbind_failures, g_ctrlm.pairing_metrics.last_screenbind_error_timestamp, g_ctrlm.pairing_metrics.last_screenbind_error_code, g_ctrlm.pairing_metrics.last_screenbind_remote_type);

   LOG_INFO("%s: num_non_screenbind_failures <%lu>, last_non_screenbind_error_timestamp <%lums>, last_non_screenbind_error_code <%d>, last_non_screenbind_error_binding_type <%d> last_screenbind_remote_type <%s>\n",
      __FUNCTION__, g_ctrlm.pairing_metrics.num_non_screenbind_failures, g_ctrlm.pairing_metrics.last_non_screenbind_error_timestamp, g_ctrlm.pairing_metrics.last_non_screenbind_error_code, g_ctrlm.pairing_metrics.last_non_screenbind_error_binding_type, g_ctrlm.pairing_metrics.last_non_screenbind_remote_type);

   ctrlm_db_pairing_metrics_write(data, CTRLM_RF4CE_LEN_PAIRING_METRICS);
}

guchar ctrlm_property_write_pairing_metrics(guchar *data, guchar length) {
   errno_t safec_rc = -1;
   int ind = -1;
   if(data == NULL || length != CTRLM_RF4CE_LEN_PAIRING_METRICS) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   ctrlm_pairing_metrics_t pairing_metrics;
   safec_rc = memcpy_s(&pairing_metrics, sizeof(ctrlm_pairing_metrics_t), data, CTRLM_RF4CE_LEN_PAIRING_METRICS);
   ERR_CHK(safec_rc);

   safec_rc = memcmp_s(&g_ctrlm.pairing_metrics, sizeof(ctrlm_pairing_metrics_t), &pairing_metrics, CTRLM_RF4CE_LEN_PAIRING_METRICS, &ind);
   ERR_CHK(safec_rc);
   if((safec_rc == EOK) && (ind != 0)) {
      safec_rc = memcpy_s(&g_ctrlm.pairing_metrics, sizeof(ctrlm_pairing_metrics_t), &pairing_metrics, CTRLM_RF4CE_LEN_PAIRING_METRICS);
      ERR_CHK(safec_rc);
   }

   if(!g_ctrlm.loading_db) {
      ctrlm_property_write_pairing_metrics();
   }
   return(length);
}

void ctrlm_property_read_pairing_metrics(void) {
   guchar *data = NULL;
   guint32 length;
   ctrlm_db_pairing_metrics_read(&data, &length);

   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Pairing Metrics\n", __FUNCTION__);
   } else {
      ctrlm_property_write_pairing_metrics(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   LOG_INFO("%s: num_screenbind_failures <%lu>, last_screenbind_error_timestamp <%lums>, last_screenbind_error_code <%d>, last_screenbind_remote_type <%s>\n",
      __FUNCTION__, g_ctrlm.pairing_metrics.num_screenbind_failures, g_ctrlm.pairing_metrics.last_screenbind_error_timestamp, g_ctrlm.pairing_metrics.last_screenbind_error_code, g_ctrlm.pairing_metrics.last_screenbind_remote_type);

   LOG_INFO("%s: num_non_screenbind_failures <%lu>, last_non_screenbind_error_timestamp <%lums>, last_non_screenbind_error_code <%d>, last_non_screenbind_error_binding_type <%d> last_screenbind_remote_type <%s>\n",
      __FUNCTION__, g_ctrlm.pairing_metrics.num_non_screenbind_failures, g_ctrlm.pairing_metrics.last_non_screenbind_error_timestamp, g_ctrlm.pairing_metrics.last_non_screenbind_error_code, g_ctrlm.pairing_metrics.last_non_screenbind_error_binding_type, g_ctrlm.pairing_metrics.last_non_screenbind_remote_type);
}
void ctrlm_property_write_last_key_info(void) {
   guchar data[CTRLM_RF4CE_LEN_LAST_KEY_INFO];
   errno_t safec_rc = memcpy_s(data, sizeof(data), &g_ctrlm.last_key_info, CTRLM_RF4CE_LEN_LAST_KEY_INFO);
   ERR_CHK(safec_rc);

   if(ctrlm_is_key_code_mask_enabled()) {
      LOG_INFO("%s: controller_id <%d>, key_code <*>, key_src <%d>, timestamp <%lldms>, is_screen_bind_mode <%d>, remote_keypad_config <%d>, sourceName <%s>\n",
       __FUNCTION__, g_ctrlm.last_key_info.controller_id, g_ctrlm.last_key_info.source_type, g_ctrlm.last_key_info.timestamp, g_ctrlm.last_key_info.is_screen_bind_mode, g_ctrlm.last_key_info.remote_keypad_config, g_ctrlm.last_key_info.source_name);
   } else {
      LOG_INFO("%s: controller_id <%d>, key_code <%d>, key_src <%d>, timestamp <%lldms>, is_screen_bind_mode <%d>, remote_keypad_config <%d>, sourceName <%s>\n",
       __FUNCTION__, g_ctrlm.last_key_info.controller_id, g_ctrlm.last_key_info.source_key_code, g_ctrlm.last_key_info.source_type, g_ctrlm.last_key_info.timestamp, g_ctrlm.last_key_info.is_screen_bind_mode, g_ctrlm.last_key_info.remote_keypad_config, g_ctrlm.last_key_info.source_name);
   }

   ctrlm_db_last_key_info_write(data, CTRLM_RF4CE_LEN_LAST_KEY_INFO);
}

guchar ctrlm_property_write_last_key_info(guchar *data, guchar length) {
   errno_t safec_rc = -1;
   int ind = -1;
   if(data == NULL || length != CTRLM_RF4CE_LEN_LAST_KEY_INFO) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   ctrlm_last_key_info last_key_info;
   safec_rc = memcpy_s(&last_key_info, sizeof(ctrlm_last_key_info), data, CTRLM_RF4CE_LEN_LAST_KEY_INFO);
   ERR_CHK(safec_rc);

   safec_rc = memcmp_s(&g_ctrlm.last_key_info, sizeof(ctrlm_last_key_info), &last_key_info, CTRLM_RF4CE_LEN_LAST_KEY_INFO, &ind);
   ERR_CHK(safec_rc);
   if((safec_rc == EOK) && (ind != 0)) {
      safec_rc = memcpy_s(&g_ctrlm.last_key_info, sizeof(ctrlm_last_key_info), &last_key_info, CTRLM_RF4CE_LEN_LAST_KEY_INFO);
      ERR_CHK(safec_rc);
   }

   if(!g_ctrlm.loading_db) {
      ctrlm_property_write_last_key_info();
   }
   return(length);
}

void ctrlm_property_read_last_key_info(void) {
   guchar *data = NULL;
   guint32 length;
   ctrlm_db_last_key_info_read(&data, &length);

   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Last Key Info\n", __FUNCTION__);
   } else {
      ctrlm_property_write_last_key_info(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }

   //Reset is_screen_bind_mode
   g_ctrlm.last_key_info.is_screen_bind_mode = false;

   if(ctrlm_is_key_code_mask_enabled()) {
      LOG_INFO("%s: controller_id <%d>, key_code <*>, key_src <%d>, timestamp <%lldms>, is_screen_bind_mode <%d>, remote_keypad_config <%d>, sourceName <%s>\n",
       __FUNCTION__, g_ctrlm.last_key_info.controller_id, g_ctrlm.last_key_info.source_type, g_ctrlm.last_key_info.timestamp, g_ctrlm.last_key_info.is_screen_bind_mode, g_ctrlm.last_key_info.remote_keypad_config, g_ctrlm.last_key_info.source_name);
   } else {
      LOG_INFO("%s: controller_id <%d>, key_code <%d>, key_src <%d>, timestamp <%lldms>, is_screen_bind_mode <%d>, remote_keypad_config <%d>, sourceName <%s>\n",
       __FUNCTION__, g_ctrlm.last_key_info.controller_id, g_ctrlm.last_key_info.source_key_code, g_ctrlm.last_key_info.source_type, g_ctrlm.last_key_info.timestamp, g_ctrlm.last_key_info.is_screen_bind_mode, g_ctrlm.last_key_info.remote_keypad_config, g_ctrlm.last_key_info.source_name);
   }
}

void ctrlm_update_last_key_info(int controller_id, guchar source_type, guint32 source_key_code, const char *source_name, gboolean is_screen_bind_mode, gboolean write_last_key_info) {
   // Get a epoch-based millisecond timestamp for this event
   long long key_time = time(NULL) * 1000LL;
   ctrlm_remote_keypad_config remote_keypad_config;
   errno_t safec_rc = -1;

   // Update the LastKeyInfo
   g_ctrlm.last_key_info.controller_id       = controller_id;
   g_ctrlm.last_key_info.source_type         = source_type;
   g_ctrlm.last_key_info.source_key_code     = source_key_code;
   g_ctrlm.last_key_info.timestamp           = key_time;
   g_ctrlm.last_key_info.is_screen_bind_mode = is_screen_bind_mode;
   if(source_name != NULL) {
      safec_rc = strcpy_s(g_ctrlm.last_key_info.source_name, sizeof(g_ctrlm.last_key_info.source_name), source_name);
      ERR_CHK(safec_rc);
   } else {
      safec_rc = strcpy_s(g_ctrlm.last_key_info.source_name, sizeof(g_ctrlm.last_key_info.source_name), "Invalid");
      ERR_CHK(safec_rc);
   }
   g_ctrlm.last_key_info.source_name[CTRLM_RCU_RIB_ATTR_LEN_PRODUCT_NAME - 1] = '\0';
#if 0
   //Do not update remote_keypad_config here as we want to keep what it was set to by an IR remote
   //and not clear it when a remote is paired.  We will override this to N/A for paired remotes
   //when the iarm call to get the last_key_info is called
   g_ctrlm.last_key_info.remote_keypad_config = remote_keypad_config;
#else
   if(source_type == IARM_BUS_IRMGR_KEYSRC_RF) {
      remote_keypad_config = ctrlm_get_remote_keypad_config(g_ctrlm.last_key_info.source_name);
      is_screen_bind_mode  = false;
   } else {
      remote_keypad_config = g_ctrlm.last_key_info.remote_keypad_config;
      is_screen_bind_mode  = g_ctrlm.last_key_info.is_screen_bind_mode;
   }
#endif

   if(ctrlm_is_key_code_mask_enabled()) {
      LOG_INFO("%s: controller_id <%d>, key_code <*>, key_src <%d>, timestamp <%lldms>, is_screen_bind_mode <%d>, remote_keypad_config <%d>, sourceName <%s>\n",
       __FUNCTION__, g_ctrlm.last_key_info.controller_id, g_ctrlm.last_key_info.source_type, g_ctrlm.last_key_info.timestamp, is_screen_bind_mode, remote_keypad_config, g_ctrlm.last_key_info.source_name);
   } else {
      LOG_INFO("%s: controller_id <%d>, key_code <%d>, key_src <%d>, timestamp <%lldms>, is_screen_bind_mode <%d>, remote_keypad_config <%d>, sourceName <%s>\n",
       __FUNCTION__, g_ctrlm.last_key_info.controller_id, g_ctrlm.last_key_info.source_key_code, g_ctrlm.last_key_info.source_type, g_ctrlm.last_key_info.timestamp, is_screen_bind_mode, remote_keypad_config, g_ctrlm.last_key_info.source_name);
   }

   if(write_last_key_info) {
      ctrlm_property_write_last_key_info();
   }
}

gboolean ctrlm_main_iarm_call_control_service_set_values(ctrlm_main_iarm_call_control_service_settings_t *settings) {
   if(settings == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_control_service_settings_t *msg = (ctrlm_main_queue_msg_main_control_service_settings_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_control_service_settings_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_SET_VALUES;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->settings          = settings;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_SET_VALUES request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

gboolean ctrlm_main_iarm_call_control_service_get_values(ctrlm_main_iarm_call_control_service_settings_t *settings) {
   if(settings == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_control_service_settings_t *msg = (ctrlm_main_queue_msg_main_control_service_settings_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_control_service_settings_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_GET_VALUES;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->settings          = settings;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_GET_VALUES request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

gboolean ctrlm_main_iarm_call_control_service_can_find_my_remote(ctrlm_main_iarm_call_control_service_can_find_my_remote_t *can_find_my_remote) {
   if(can_find_my_remote == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_control_service_can_find_my_remote_t *msg = (ctrlm_main_queue_msg_main_control_service_can_find_my_remote_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_control_service_can_find_my_remote_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type        = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_CAN_FIND_MY_REMOTE;
   msg->header.network_id  = ctrlm_network_id_get(can_find_my_remote->network_type);
   msg->can_find_my_remote = can_find_my_remote;
   msg->semaphore          = &semaphore;
   msg->cmd_result         = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_CAN_FIND_MY_REMOTE request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

gboolean ctrlm_main_iarm_call_control_service_start_pairing_mode(ctrlm_main_iarm_call_control_service_pairing_mode_t *pairing) {
   if(pairing == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_control_service_pairing_mode_t *msg = (ctrlm_main_queue_msg_main_control_service_pairing_mode_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_control_service_pairing_mode_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_START_PAIRING_MODE;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->pairing           = pairing;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_START_PAIRING_MODE request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

gboolean ctrlm_main_iarm_call_control_service_end_pairing_mode(ctrlm_main_iarm_call_control_service_pairing_mode_t *pairing) {
   if(pairing == NULL) {
      LOG_ERROR("%s: NULL parameter\n", __FUNCTION__);
      return(false);
   }
   LOG_INFO("%s: \n", __FUNCTION__);

   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_main_control_service_pairing_mode_t *msg = (ctrlm_main_queue_msg_main_control_service_pairing_mode_t *)g_malloc(sizeof(ctrlm_main_queue_msg_main_control_service_pairing_mode_t));

   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   sem_init(&semaphore, 0, 0);

   msg->header.type       = CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_END_PAIRING_MODE;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->pairing           = pairing;
   msg->semaphore         = &semaphore;
   msg->cmd_result        = &cmd_result;

   ctrlm_main_queue_msg_push(msg);

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process CTRLM_MAIN_QUEUE_MSG_TYPE_MAIN_CONTROL_SERVICE_END_PAIRING_MODE request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result == CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      return(true);
   }
   return(false);
}

void ctrlm_main_iarm_call_control_service_start_pairing_mode_(ctrlm_main_iarm_call_control_service_pairing_mode_t *pairing) {
   if(pairing->network_id != CTRLM_MAIN_NETWORK_ID_ALL && !ctrlm_network_id_is_valid(pairing->network_id)) {
      pairing->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: network id - Out of range %u\n", __FUNCTION__, pairing->network_id);
      return;
   }

   /////////////////////////////////////////////////////
   //Stop any current pairing window and start a new one
   /////////////////////////////////////////////////////

   if(g_ctrlm.binding_button) {
      ctrlm_timeout_destroy(&g_ctrlm.binding_button_timeout_tag);
      g_ctrlm.binding_button = false;
      LOG_INFO("%s: BINDING BUTTON <INACTIVE>\n", __FUNCTION__);
   }
   if(g_ctrlm.binding_screen_active) {
      ctrlm_timeout_destroy(&g_ctrlm.screen_bind_timeout_tag);
      g_ctrlm.binding_screen_active = false;
      LOG_INFO("%s: SCREEN BIND STATE <INACTIVE>\n", __FUNCTION__);
   }
   if(g_ctrlm.one_touch_autobind_active) {
      ctrlm_stop_one_touch_autobind_(CTRLM_MAIN_NETWORK_ID_ALL);
      LOG_INFO("%s: ONE TOUCH AUTOBIND <INACTIVE>\n", __FUNCTION__);
   }
   g_ctrlm.pairing_window.active = true;
   g_ctrlm.pairing_window.pairing_mode = (ctrlm_pairing_modes_t)pairing->pairing_mode;
   g_ctrlm.pairing_window.restrict_by_remote = (ctrlm_pairing_restrict_by_remote_t)pairing->restrict_by_remote;
   ctrlm_pairing_window_bind_status_set_(CTRLM_BIND_STATUS_NO_DISCOVERY_REQUEST);
   g_ctrlm.autobind = false;

   switch(pairing->pairing_mode) {
      case CTRLM_PAIRING_MODE_BUTTON_BUTTON_BIND: {
         pairing->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         g_ctrlm.binding_button = true;
         // Set a timer to limit the binding mode window
         g_ctrlm.binding_button_timeout_tag = ctrlm_timeout_create(g_ctrlm.binding_button_timeout_val, ctrlm_timeout_binding_button, NULL);
         LOG_INFO("%s: BINDING BUTTON <ACTIVE>\n", __FUNCTION__);
         break;
      }
      case CTRLM_PAIRING_MODE_SCREEN_BIND: {
         pairing->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
         g_ctrlm.binding_screen_active = true;
         // Set a timer to limit the binding mode window
         g_ctrlm.screen_bind_timeout_tag = ctrlm_timeout_create(g_ctrlm.screen_bind_timeout_val, ctrlm_timeout_screen_bind, NULL);
         LOG_INFO("%s: SCREEN BIND STATE <ACTIVE>\n", __FUNCTION__);
         break;
      }
      case CTRLM_PAIRING_MODE_ONE_TOUCH_AUTO_BIND: {
         ctrlm_controller_bind_config_t config;
         g_ctrlm.one_touch_autobind_timeout_tag = ctrlm_timeout_create(g_ctrlm.one_touch_autobind_timeout_val, ctrlm_timeout_one_touch_autobind, NULL);
         g_ctrlm.one_touch_autobind_active = true;

         pairing->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
         config.mode = CTRLM_PAIRING_MODE_ONE_TOUCH_AUTO_BIND;
         config.data.autobind.enable         = true;
         config.data.autobind.pass_threshold = 1;
         config.data.autobind.fail_threshold = 5;

         for(auto const &itr : g_ctrlm.networks) {
            if(pairing->network_id == itr.first || pairing->network_id == CTRLM_MAIN_NETWORK_ID_ALL) {
               itr.second->binding_config_set(config);
               pairing->result = CTRLM_IARM_CALL_RESULT_SUCCESS;
            }
         }
         break;
      }
      default: {
         LOG_ERROR("%s: Invalid Pairing Mode %d\n", __FUNCTION__, pairing->pairing_mode);
         pairing->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      }
   }
}

void ctrlm_main_iarm_call_control_service_end_pairing_mode_(ctrlm_main_iarm_call_control_service_pairing_mode_t *pairing) {
   if(pairing->network_id != CTRLM_MAIN_NETWORK_ID_ALL && !ctrlm_network_id_is_valid(pairing->network_id)) {
      pairing->result = CTRLM_IARM_CALL_RESULT_ERROR_INVALID_PARAMETER;
      LOG_ERROR("%s: network id - Out of range %u\n", __FUNCTION__, pairing->network_id);
      return;
   }
   ctrlm_close_pairing_window_(pairing->network_id, CTRLM_CLOSE_PAIRING_WINDOW_REASON_END);
   pairing->result       = CTRLM_IARM_CALL_RESULT_SUCCESS;
   pairing->pairing_mode = g_ctrlm.pairing_window.pairing_mode;
   pairing->bind_status  = g_ctrlm.pairing_window.bind_status;

   switch(g_ctrlm.pairing_window.pairing_mode) {
      case CTRLM_PAIRING_MODE_BUTTON_BUTTON_BIND: {
         LOG_INFO("%s: BINDING BUTTON <INACTIVE>\n", __FUNCTION__);
         break;
      }
      case CTRLM_PAIRING_MODE_SCREEN_BIND: {
         LOG_INFO("%s: SCREEN BIND STATE <INACTIVE>\n", __FUNCTION__);
         break;
      }
      case CTRLM_PAIRING_MODE_ONE_TOUCH_AUTO_BIND: {
         LOG_INFO("%s: ONE TOUCH AUTOBIND STATE <INACTIVE>\n", __FUNCTION__);
         break;
      }
      default: {
         LOG_ERROR("%s: Invalid Pairing Mode %d\n", __FUNCTION__, g_ctrlm.pairing_window.pairing_mode);
      }
   }
}

gboolean ctrlm_pairing_window_active_get() {
   return(g_ctrlm.pairing_window.active);
}

ctrlm_pairing_restrict_by_remote_t restrict_pairing_by_remote_get() {
   return(g_ctrlm.pairing_window.restrict_by_remote);
}

void ctrlm_stop_one_touch_autobind_(ctrlm_network_id_t network_id) {
   if (g_ctrlm.one_touch_autobind_active) {
      LOG_INFO("%s: One Touch Autobind mode stopped.\n", __FUNCTION__);
      ctrlm_timeout_destroy(&g_ctrlm.one_touch_autobind_timeout_tag);
      g_ctrlm.one_touch_autobind_active = FALSE;
      ctrlm_controller_bind_config_t config;
      config.mode = CTRLM_PAIRING_MODE_ONE_TOUCH_AUTO_BIND;
      config.data.autobind.enable         = false;
      config.data.autobind.pass_threshold = 3;
      config.data.autobind.fail_threshold = 5;
      for(auto const &itr : g_ctrlm.networks) {
         if(network_id == itr.first || network_id == CTRLM_MAIN_NETWORK_ID_ALL) {
            itr.second->binding_config_set(config);
         }
      }
   }
}

void ctrlm_close_pairing_window_(ctrlm_network_id_t network_id, ctrlm_close_pairing_window_reason reason) {
   errno_t safec_rc = -1;
   if(g_ctrlm.pairing_window.active) {
      g_ctrlm.pairing_window.active             = false;
      g_ctrlm.pairing_window.restrict_by_remote = CTRLM_PAIRING_RESTRICT_NONE;
      switch(reason) {
        case(CTRLM_CLOSE_PAIRING_WINDOW_REASON_PAIRING_SUCCESS): {
            ctrlm_pairing_window_bind_status_set_(CTRLM_BIND_STATUS_SUCCESS);
            break;
         }
         case(CTRLM_CLOSE_PAIRING_WINDOW_REASON_END): {
            g_ctrlm.autobind = true;
            break;
         }
         case(CTRLM_CLOSE_PAIRING_WINDOW_REASON_TIMEOUT): {
            ctrlm_pairing_window_bind_status_set_(CTRLM_BIND_STATUS_BIND_WINDOW_TIMEOUT);
            g_ctrlm.autobind = true;
            ctrlm_rcu_iarm_event_rf4ce_pairing_window_timeout();
            break;
         }
         default: {
            ctrlm_pairing_window_bind_status_set_(CTRLM_BIND_STATUS_NO_DISCOVERY_REQUEST);
            break;
         }
      }
      LOG_INFO("%s: reason <%s>, bind status <%s>\n", __FUNCTION__, ctrlm_close_pairing_window_reason_str(reason), ctrlm_bind_status_str(g_ctrlm.pairing_window.bind_status));
   }
   if (g_ctrlm.binding_button) {
      ctrlm_timeout_destroy(&g_ctrlm.binding_button_timeout_tag);
      g_ctrlm.binding_button = FALSE;
      //Save the non screen bind pairing metrics
      if(g_ctrlm.pairing_window.bind_status != CTRLM_BIND_STATUS_SUCCESS) {
         g_ctrlm.pairing_metrics.num_non_screenbind_failures++;
         g_ctrlm.pairing_metrics.last_non_screenbind_error_timestamp = time(NULL);
         g_ctrlm.pairing_metrics.last_non_screenbind_error_code = g_ctrlm.pairing_window.bind_status;
         g_ctrlm.pairing_metrics.last_non_screenbind_error_binding_type = CTRLM_RCU_BINDING_TYPE_BUTTON;
         if(g_ctrlm.pairing_window.bind_status != CTRLM_BIND_STATUS_NO_DISCOVERY_REQUEST) {
            safec_rc = strcpy_s(g_ctrlm.pairing_metrics.last_non_screenbind_remote_type, sizeof(g_ctrlm.pairing_metrics.last_non_screenbind_remote_type), g_ctrlm.discovery_remote_type);
            ERR_CHK(safec_rc);
         } else {
            g_ctrlm.pairing_metrics.last_non_screenbind_remote_type[0] = '\0';
         }
         ctrlm_property_write_pairing_metrics();
      }
   }
   if (g_ctrlm.binding_screen_active) {
      ctrlm_timeout_destroy(&g_ctrlm.screen_bind_timeout_tag);
      g_ctrlm.binding_screen_active = FALSE;
      //Save the screen bind pairing metrics
      if(g_ctrlm.pairing_window.bind_status != CTRLM_BIND_STATUS_SUCCESS) {
         g_ctrlm.pairing_metrics.num_screenbind_failures++;
         g_ctrlm.pairing_metrics.last_screenbind_error_timestamp = time(NULL);
         g_ctrlm.pairing_metrics.last_screenbind_error_code = g_ctrlm.pairing_window.bind_status;
         if(g_ctrlm.pairing_window.bind_status != CTRLM_BIND_STATUS_NO_DISCOVERY_REQUEST) {
            safec_rc = strcpy_s(g_ctrlm.pairing_metrics.last_screenbind_remote_type, sizeof(g_ctrlm.pairing_metrics.last_screenbind_remote_type), g_ctrlm.discovery_remote_type);
            ERR_CHK(safec_rc);
         } else {
            g_ctrlm.pairing_metrics.last_screenbind_remote_type[0] = '\0';
         }
         ctrlm_property_write_pairing_metrics();
      }
   }
   if (g_ctrlm.one_touch_autobind_active) {
      ctrlm_stop_one_touch_autobind_(network_id);
      //Save the non screen bind pairing metrics
      if(g_ctrlm.pairing_window.bind_status != CTRLM_BIND_STATUS_SUCCESS) {
         g_ctrlm.pairing_metrics.num_non_screenbind_failures++;
         g_ctrlm.pairing_metrics.last_non_screenbind_error_timestamp = time(NULL);
         g_ctrlm.pairing_metrics.last_non_screenbind_error_code = g_ctrlm.pairing_window.bind_status;
         g_ctrlm.pairing_metrics.last_non_screenbind_error_binding_type = CTRLM_RCU_BINDING_TYPE_AUTOMATIC;
         if(g_ctrlm.pairing_window.bind_status != CTRLM_BIND_STATUS_NO_DISCOVERY_REQUEST) {
            safec_rc = strcpy_s(g_ctrlm.pairing_metrics.last_non_screenbind_remote_type, sizeof(g_ctrlm.pairing_metrics.last_non_screenbind_remote_type), g_ctrlm.discovery_remote_type);
            ERR_CHK(safec_rc);
         } else {
            g_ctrlm.pairing_metrics.last_non_screenbind_remote_type[0] = '\0';
         }
         ctrlm_property_write_pairing_metrics();
      }
   }
}

void ctrlm_pairing_window_bind_status_set_(ctrlm_bind_status_t bind_status) {
   // Don't overwrite the ASB failure with HAL failure
   if((bind_status == CTRLM_BIND_STATUS_HAL_FAILURE) && (g_ctrlm.pairing_window.bind_status == CTRLM_BIND_STATUS_ASB_FAILURE)) {
      LOG_INFO("%s: Ignore Binding Status <%s>.  This is really an <%s>.\n", __FUNCTION__, ctrlm_bind_status_str(bind_status), ctrlm_bind_status_str(g_ctrlm.pairing_window.bind_status));
      return;
   }

   switch(bind_status) {
      case CTRLM_BIND_STATUS_NO_DISCOVERY_REQUEST:
      case CTRLM_BIND_STATUS_NO_PAIRING_REQUEST:
      case CTRLM_BIND_STATUS_UNKNOWN_FAILURE:
         //These are likely intermediate or temporary status's.  Only log with DEBUG on.
         LOG_DEBUG("%s: Binding Status <%s>.\n", __FUNCTION__, ctrlm_bind_status_str(bind_status));
         break;
      case CTRLM_BIND_STATUS_SUCCESS:
      case CTRLM_BIND_STATUS_HAL_FAILURE:
      case CTRLM_BIND_STATUS_CTRLM_BLACKOUT:
      case CTRLM_BIND_STATUS_ASB_FAILURE:
      case CTRLM_BIND_STATUS_STD_KEY_EXCHANGE_FAILURE:
      case CTRLM_BIND_STATUS_PING_FAILURE:
      case CTRLM_BIND_STATUS_VALILDATION_FAILURE:
      case CTRLM_BIND_STATUS_RIB_UPDATE_FAILURE:
      case CTRLM_BIND_STATUS_BIND_WINDOW_TIMEOUT:
         LOG_INFO("%s: Binding Status <%s>.\n", __FUNCTION__, ctrlm_bind_status_str(bind_status));
         break;
      default:
         break;
   }
   g_ctrlm.pairing_window.bind_status = bind_status;
}

void ctrlm_discovery_remote_type_set_(const char *remote_type_str) {
   // Don't overwrite the ASB failure with HAL failure
   if(remote_type_str == NULL) {
      LOG_ERROR("%s: remote_type_str is NULL.\n", __FUNCTION__);
      return;
   }
   errno_t safec_rc = strcpy_s(g_ctrlm.discovery_remote_type, sizeof(g_ctrlm.discovery_remote_type), remote_type_str);
   ERR_CHK(safec_rc);
   g_ctrlm.discovery_remote_type[CTRLM_HAL_RF4CE_USER_STRING_SIZE-1] = '\0';
   LOG_INFO("%s: discovery_remote_type <%s>.\n", __FUNCTION__, g_ctrlm.discovery_remote_type);
}

void ctrlm_property_write_shutdown_time(void) {
   guchar data[CTRLM_RF4CE_LEN_SHUTDOWN_TIME];
   data[0]  = (guchar)(g_ctrlm.shutdown_time);
   data[1]  = (guchar)(g_ctrlm.shutdown_time >> 8);
   data[2]  = (guchar)(g_ctrlm.shutdown_time >> 16);
   data[3]  = (guchar)(g_ctrlm.shutdown_time >> 24);

   ctrlm_db_shutdown_time_write(data, CTRLM_RF4CE_LEN_SHUTDOWN_TIME);
}

guchar ctrlm_property_write_shutdown_time(guchar *data, guchar length) {
   if(data == NULL || length != CTRLM_RF4CE_LEN_SHUTDOWN_TIME) {
      LOG_ERROR("%s: INVALID PARAMETERS\n", __FUNCTION__);
      return(0);
   }
   time_t shutdown_time = ((data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0]);
   
   if(g_ctrlm.shutdown_time != shutdown_time) {
      // Store the data
      g_ctrlm.shutdown_time = shutdown_time;

      if(!g_ctrlm.loading_db) {
         ctrlm_property_write_shutdown_time();
      }
   }

   return(length);
}

void ctrlm_property_read_shutdown_time(void) {
   guchar *data = NULL;
   guint32 length;
   ctrlm_db_shutdown_time_read(&data, &length);

   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - IR shutdown time\n", __FUNCTION__);
   } else {
      ctrlm_property_write_shutdown_time(data, length);
      ctrlm_db_free(data);
      data = NULL;
   }
}

time_t ctrlm_shutdown_time_get(void) {
   return(g_ctrlm.shutdown_time);
}


#if CTRLM_HAL_RF4CE_API_VERSION >= 9
void ctrlm_crash_recovery_check() {
   guint    crash_count      = 0;
   guint    invalid_hal_nvm  = 0;
   guint    invalid_ctrlm_db = 0;
   //FILE *fd;
   LOG_INFO("%s: Checking if recovering from pre-init crash\n", __FUNCTION__);

#ifdef CTRLM_NETWORK_HAS_HAL_NVM
   ctrlm_recovery_property_get(CTRLM_RECOVERY_INVALID_HAL_NVM, &invalid_hal_nvm);
   // Clear invalid NVM flag
   if(invalid_hal_nvm) {
      invalid_hal_nvm = 0;
      ctrlm_recovery_property_set(CTRLM_RECOVERY_INVALID_HAL_NVM, &invalid_hal_nvm);
      invalid_hal_nvm = 1;
   }
#endif
   ctrlm_recovery_property_get(CTRLM_RECOVERY_INVALID_CTRLM_DB, &invalid_ctrlm_db);
   // Clear invalid DB flag
   if(invalid_ctrlm_db) {
      invalid_ctrlm_db = 0;
      ctrlm_recovery_property_set(CTRLM_RECOVERY_INVALID_CTRLM_DB, &invalid_ctrlm_db);
      invalid_ctrlm_db = 1;
   }

   // Get crash count
   ctrlm_recovery_property_get(CTRLM_RECOVERY_CRASH_COUNT, &crash_count);

   // Start logic for recovery
   if(crash_count >= 2 * g_ctrlm.crash_recovery_threshold && (invalid_hal_nvm || invalid_ctrlm_db)) { // Using the backup did not work.. Worst case scenario
      LOG_FATAL("%s: failed to recover with backup NVM, %s\n", __FUNCTION__, (invalid_hal_nvm ? "failure due to hal NVM.. Worst case scenario removing HAL NVM and ctrlm DB" : "failure due to ctrlm DB.. Removing ctrlm DB"));
      // Remove both NVM and ctrlm DB
      if(!ctrlm_file_delete(g_ctrlm.db_path.c_str(), true)) {
         LOG_WARN("%s: Failed to remove ctrlm DB.. It is possible it no longer exists\n", __FUNCTION__);
      }
      // Set recovery mode in rf4ce object
      if(invalid_hal_nvm) {
         for(auto const &itr : g_ctrlm.networks) {
            if(itr.second->type_get() == CTRLM_NETWORK_TYPE_RF4CE) {
               itr.second->recovery_set(CTRLM_RECOVERY_TYPE_RESET);
            }
         }
      }
      // Set crash back to 0
      crash_count = 0;
      ctrlm_recovery_property_set(CTRLM_RECOVERY_CRASH_COUNT, &crash_count);
   }
   else if(crash_count >= 2 * g_ctrlm.crash_recovery_threshold) {
      LOG_FATAL("%s: Entered a reboot loop in which both NVM and ctrlm DB are valid\n", __FUNCTION__);
   }
   else if(crash_count >= g_ctrlm.crash_recovery_threshold) { // We entered a rebooting loop and met the threshold
      LOG_ERROR("%s: Failed to initialize %u times, trying to restore backup of HAL NVM and ctrlm DB\n", __FUNCTION__, crash_count);

      // Check Timestamps to ensure consistency across backup files
      guint64    hal_nvm_ts  = 0;
      guint64    ctrlm_db_ts = 0;

#ifdef CTRLM_NETWORK_HAS_HAL_NVM
      if(FALSE == ctrlm_file_timestamp_get(HAL_NVM_BACKUP, &hal_nvm_ts)) {
         LOG_ERROR("%s: Failed to read timestamp of HAL NVM backup\n", __FUNCTION__);
         // Increment crash count
         crash_count = crash_count + 1;
         ctrlm_recovery_property_set(CTRLM_RECOVERY_CRASH_COUNT, &crash_count);
         return;
      }
#endif
      if(FALSE == ctrlm_file_timestamp_get(CTRLM_NVM_BACKUP, &ctrlm_db_ts)) {
         LOG_ERROR("%s: Failed to read timestamp of ctrlm db backup\n", __FUNCTION__);
         // Increment crash count
         crash_count = crash_count + 1;
         ctrlm_recovery_property_set(CTRLM_RECOVERY_CRASH_COUNT, &crash_count);
         return;
      }

      // Compare the timestamps
      if(hal_nvm_ts != ctrlm_db_ts) {
         LOG_ERROR("%s: The timestamps of the hal nvm and ctrlm db files do not match.. Cannot restore inconsistent backups\n", __FUNCTION__);
         crash_count = 2 * g_ctrlm.crash_recovery_threshold;
         ctrlm_recovery_property_set(CTRLM_RECOVERY_CRASH_COUNT, &crash_count);
         return;
      }

      // Set Recovery mode in rf4ce object
      for(auto const &itr : g_ctrlm.networks) {
         if(itr.second->type_get() == CTRLM_NETWORK_TYPE_RF4CE) {
            itr.second->recovery_set(CTRLM_RECOVERY_TYPE_BACKUP);
         }
      }
      // Restore backup for ctrlm NVM
      if(FALSE == ctrlm_file_copy(CTRLM_NVM_BACKUP, g_ctrlm.db_path.c_str(), TRUE, TRUE)) {
         LOG_WARN("%s: Failed to restore ctrlm DB backup, removing current DB...\n", __FUNCTION__);
         ctrlm_file_delete(g_ctrlm.db_path.c_str(), true);
      }
      // Increment crash count
      crash_count = crash_count + 1;
      ctrlm_recovery_property_set(CTRLM_RECOVERY_CRASH_COUNT, &crash_count);
   }
   else {
      LOG_WARN("%s: Failed to initialize %u times\n", __FUNCTION__, crash_count);
      // Increment crash count
      crash_count = crash_count + 1;
      ctrlm_recovery_property_set(CTRLM_RECOVERY_CRASH_COUNT, &crash_count);
   }

}

void ctrlm_backup_data() {

#if ( __GLIBC__ == 2 ) && ( __GLIBC_MINOR__ >= 20 )
   gint64 t;
#else
   GTimeVal t;
#endif
   // Back up network NVM files
   for(auto const &itr : g_ctrlm.networks) {
      if(itr.second->type_get() == CTRLM_NETWORK_TYPE_RF4CE) {
         if(FALSE == itr.second->backup_hal_nvm()) {
            LOG_ERROR("%s: Failed to back up RF4CE HAL NVM\n", __FUNCTION__);
            return;
         }
      }
   }

   // Back up ctrlm db
   if(FALSE == ctrlm_db_backup()) {
      ctrlm_file_delete(CTRLM_NVM_BACKUP, true);
#ifdef CTRLM_NETWORK_HAS_HAL_NVM
      remove(HAL_NVM_BACKUP);
#endif
      return;
   }
glong tv_sec = 0;
   // Get timestamps so we know backup data is consistent
#if ( __GLIBC__ == 2 ) && ( __GLIBC_MINOR__ >= 20 )
   t = g_get_real_time ();
   tv_sec = t/G_USEC_PER_SEC;
#else
   g_get_current_time(&t);
   tv_sec = t.tv_sec;
#endif
#ifdef CTRLM_NETWORK_HAS_HAL_NVM
   if(FALSE == ctrlm_file_timestamp_set(HAL_NVM_BACKUP, tv_sec)) {
      ctrlm_file_delete(CTRLM_NVM_BACKUP, true);
      remove(HAL_NVM_BACKUP);
      return;
   }
#endif

   if(FALSE == ctrlm_file_timestamp_set(CTRLM_NVM_BACKUP, tv_sec)) {
      ctrlm_file_delete(CTRLM_NVM_BACKUP, true);
#ifdef CTRLM_NETWORK_HAS_HAL_NVM
      remove(HAL_NVM_BACKUP);
#endif
      return;
   }
}
#endif

gboolean ctrlm_message_queue_delay(gpointer data) {
   if(data) {
      ctrlm_main_queue_msg_push(data);
   }
   return(FALSE);
}

gboolean ctrlm_ntp_check(gpointer user_data) {
   ctrlm_main_queue_msg_header_t *msg = (ctrlm_main_queue_msg_header_t *)malloc(sizeof(ctrlm_main_queue_msg_header_t));
   if(msg) {
      msg->type = CTRLM_MAIN_QUEUE_MSG_TYPE_NTP_CHECK;
      msg->network_id = CTRLM_MAIN_NETWORK_ID_ALL;
      ctrlm_main_queue_msg_push(msg);
   } else {
      LOG_ERROR("%s: failed to allocate time check\n", __FUNCTION__);
   }
   return(false);
}

gboolean ctrlm_power_state_change(ctrlm_power_state_t power_state, gboolean system) {
   //Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_power_state_change_t *msg = (ctrlm_main_queue_power_state_change_t *)g_malloc(sizeof(ctrlm_main_queue_power_state_change_t));
   if(NULL == msg) {
      LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
      g_assert(0);
      return(false);
   }

   msg->header.type = CTRLM_MAIN_QUEUE_MSG_TYPE_POWER_STATE_CHANGE;
   msg->header.network_id = CTRLM_MAIN_NETWORK_ID_ALL;
   msg->system = system;
   msg->new_state = power_state;
   ctrlm_main_queue_msg_push(msg);
   return(true);
}

ctrlm_controller_id_t ctrlm_last_used_controller_get(ctrlm_network_type_t network_type) {
   if (network_type == CTRLM_NETWORK_TYPE_RF4CE) {
      return g_ctrlm.last_key_info.controller_id;
   }
   LOG_ERROR("%s: Not supported for %s\n", __FUNCTION__, ctrlm_network_type_str(network_type).c_str());
   return CTRLM_MAIN_CONTROLLER_ID_INVALID;
}

void ctrlm_controller_product_name_get(ctrlm_controller_id_t controller_id, char *source_name) {
   // Signal completion of the operation
   sem_t semaphore;
   ctrlm_main_status_cmd_result_t cmd_result = CTRLM_MAIN_STATUS_REQUEST_PENDING;

   // Allocate a message and send it to Control Manager's queue
   ctrlm_main_queue_msg_product_name_t msg;
   errno_t safec_rc = memset_s(&msg, sizeof(msg), 0, sizeof(msg));
   ERR_CHK(safec_rc);

   sem_init(&semaphore, 0, 0);
   msg.controller_id   = controller_id;
   msg.product_name    = source_name;
   msg.semaphore       = &semaphore;
   msg.cmd_result      = &cmd_result;

   ctrlm_main_queue_handler_push(CTRLM_HANDLER_NETWORK, (ctrlm_msg_handler_network_t)&ctrlm_obj_network_t::req_process_controller_product_name, &msg, sizeof(msg), NULL, 1); // TODO: Hack, using default RF4CE network ID.

   // Wait for the result condition to be signaled
   while(CTRLM_MAIN_STATUS_REQUEST_PENDING == cmd_result) {
      LOG_DEBUG("%s: Waiting for main thread to process CONTROLLER_TYPE request\n", __FUNCTION__);
      sem_wait(&semaphore);
   }

   sem_destroy(&semaphore);

   if(cmd_result != CTRLM_MAIN_STATUS_REQUEST_SUCCESS) {
      LOG_ERROR("%s: ERROR getting product name!!\n", __FUNCTION__);
      return;
   }
}

void control_service_values_read_from_db() {
   guchar *data = NULL;
   guint32 length;

#ifdef ASB
   //ASB enabled
   gboolean asb_enabled = CTRLM_ASB_ENABLED_DEFAULT;
   ctrlm_db_asb_enabled_read(&data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - ASB Enabled.  Using default of <%s>.\n", __FUNCTION__, asb_enabled ? "true" : "false");
      // Write new asb_enabled flag to NVM
      ctrlm_db_asb_enabled_write((guchar *)&asb_enabled, CTRLM_ASB_ENABLED_LEN); 
   } else {
      asb_enabled = data[0];
      ctrlm_db_free(data);
      data = NULL;
      LOG_INFO("%s: ASB Enabled read from DB <%s>\n", __FUNCTION__, asb_enabled ? "YES" : "NO");
   }
   g_ctrlm.cs_values.asb_enable = asb_enabled;
#endif
   // Open Chime Enabled
   gboolean open_chime_enabled = CTRLM_OPEN_CHIME_ENABLED_DEFAULT;
   ctrlm_db_open_chime_enabled_read(&data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Open Chime Enabled.  Using default of <%s>.\n", __FUNCTION__, open_chime_enabled ? "true" : "false");
      // Write new open_chime_enabled flag to NVM
      ctrlm_db_open_chime_enabled_write((guchar *)&open_chime_enabled, CTRLM_OPEN_CHIME_ENABLED_LEN); 
   } else {
      open_chime_enabled = data[0];
      ctrlm_db_free(data);
      data = NULL;
      LOG_INFO("%s: Open Chime Enabled read from DB <%s>\n", __FUNCTION__, open_chime_enabled ? "YES" : "NO");
   }
   g_ctrlm.cs_values.chime_open_enable = open_chime_enabled;

   // Close Chime Enabled
   gboolean close_chime_enabled = CTRLM_CLOSE_CHIME_ENABLED_DEFAULT;
   ctrlm_db_close_chime_enabled_read(&data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Close Chime Enabled.  Using default of <%s>.\n", __FUNCTION__, close_chime_enabled ? "true" : "false");
      // Write new close_chime_enabled flag to NVM
      ctrlm_db_close_chime_enabled_write((guchar *)&close_chime_enabled, CTRLM_CLOSE_CHIME_ENABLED_LEN); 
   } else {
      close_chime_enabled = data[0];
      ctrlm_db_free(data);
      data = NULL;
      LOG_INFO("%s: Close Chime Enabled read from DB <%s>\n", __FUNCTION__, close_chime_enabled ? "YES" : "NO");
   }
   g_ctrlm.cs_values.chime_close_enable = close_chime_enabled;

   // Privacy Chime Enabled
   gboolean privacy_chime_enabled = CTRLM_PRIVACY_CHIME_ENABLED_DEFAULT;
   ctrlm_db_privacy_chime_enabled_read(&data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Privacy Chime Enabled.  Using default of <%s>.\n", __FUNCTION__, privacy_chime_enabled ? "true" : "false");
      // Write new privacy_chime_enabled flag to NVM
      ctrlm_db_privacy_chime_enabled_write((guchar *)&privacy_chime_enabled, CTRLM_PRIVACY_CHIME_ENABLED_LEN); 
   } else {
      privacy_chime_enabled = data[0];
      ctrlm_db_free(data);
      data = NULL;
      LOG_INFO("%s: Privacy Chime Enabled read from DB <%s>\n", __FUNCTION__, privacy_chime_enabled ? "YES" : "NO");
   }
   g_ctrlm.cs_values.chime_privacy_enable = privacy_chime_enabled;

   // Conversational Mode
   unsigned char conversational_mode = CTRLM_CONVERSATIONAL_MODE_DEFAULT;
   ctrlm_db_conversational_mode_read(&data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Conversational Mode.  Using default of <%u>.\n", __FUNCTION__, conversational_mode);
      // Write new conversational mode to NVM
      ctrlm_db_conversational_mode_write((guchar *)&conversational_mode, CTRLM_CONVERSATIONAL_MODE_LEN); 
   } else {
      conversational_mode = data[0];
      ctrlm_db_free(data);
      data = NULL;
      if((conversational_mode < CTRLM_MIN_CONVERSATIONAL_MODE) || (conversational_mode > CTRLM_MAX_CONVERSATIONAL_MODE)) {
         conversational_mode = CTRLM_MAX_CONVERSATIONAL_MODE;
         LOG_WARN("%s: Conversational Mode from DB was out of range, falling back to default <%u>\n", __FUNCTION__, conversational_mode);
      } else {
         LOG_INFO("%s: Conversational Mode read from DB <%u>\n", __FUNCTION__, conversational_mode);
      }
   }
   g_ctrlm.cs_values.conversational_mode = conversational_mode;

   // Chime Volume
   ctrlm_chime_volume_t chime_volume = CTRLM_CHIME_VOLUME_DEFAULT;
   ctrlm_db_chime_volume_read(&data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - Chime Volume.  Using default of <%u>.\n", __FUNCTION__, chime_volume);
      // Write new chime_volume to NVM
      ctrlm_db_chime_volume_write((guchar *)&chime_volume, CTRLM_CHIME_VOLUME_LEN); 
   } else {
      chime_volume = (ctrlm_chime_volume_t)data[0];
      ctrlm_db_free(data);
      data = NULL;
      if((chime_volume < CTRLM_CHIME_VOLUME_LOW) || (chime_volume > CTRLM_CHIME_VOLUME_HIGH)) {
         chime_volume = CTRLM_CHIME_VOLUME_DEFAULT;
         LOG_WARN("%s: Chime Volume from DB was out of range, falling back to default <%u>\n", __FUNCTION__, chime_volume);
      } else {
         LOG_INFO("%s: Chime Volume read from DB <%u>\n", __FUNCTION__, chime_volume);
      }
   }
   g_ctrlm.cs_values.chime_volume = chime_volume;

   // IR Command Repeats
   unsigned char ir_command_repeats = CTRLM_IR_COMMAND_REPEATS_DEFAULT;
   ctrlm_db_ir_command_repeats_read(&data, &length);
   if(data == NULL) {
      LOG_WARN("%s: Not read from DB - IR Command Repeats.  Using default of <%u>.\n", __FUNCTION__, ir_command_repeats);
      // Write new ir_command_repeats to NVM
      ctrlm_db_ir_command_repeats_write((guchar *)&ir_command_repeats, CTRLM_IR_COMMAND_REPEATS_LEN); 
   } else {
      ir_command_repeats = data[0];
      ctrlm_db_free(data);
      data = NULL;
      if((ir_command_repeats < CTRLM_MIN_IR_COMMAND_REPEATS) || (ir_command_repeats > CTRLM_MAX_IR_COMMAND_REPEATS)) {
         ir_command_repeats = CTRLM_IR_COMMAND_REPEATS_DEFAULT;
         LOG_WARN("%s: IR Command Repeats from DB was out of range, falling back to default <%u>\n", __FUNCTION__, ir_command_repeats);
      } else {
         LOG_INFO("%s: IR Command Repeats read from DB <%u>\n", __FUNCTION__, ir_command_repeats);
      }
   }
   g_ctrlm.cs_values.ir_repeats = ir_command_repeats;

   // Call the network cs_values funcitons
   for(auto const &itr : g_ctrlm.networks) {
      itr.second->cs_values_set(&g_ctrlm.cs_values, true);
   }
}

ctrlm_voice_t *ctrlm_get_voice_obj() {
   return(g_ctrlm.voice_session);
}

gboolean ctrlm_start_iarm(gpointer user_data) {
   // Register iarm calls and events
   LOG_INFO("%s : Register iarm calls and events\n", __FUNCTION__);
   // IARM Events that we are listening to from other processes
   IARM_Bus_RegisterEventHandler(IARM_BUS_IRMGR_NAME, IARM_BUS_IRMGR_EVENT_IRKEY, ctrlm_event_handler_ir);
   IARM_Bus_RegisterEventHandler(IARM_BUS_IRMGR_NAME, IARM_BUS_IRMGR_EVENT_CONTROL, ctrlm_event_handler_ir);
   IARM_Bus_RegisterEventHandler(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_EVENT_KEYCODE_LOGGING_CHANGED, ctrlm_event_handler_system);
   IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG, ctrlm_event_handler_system);
   ctrlm_main_iarm_init();
#ifdef SYSTEMD_NOTIFY
   LOG_INFO("%s: Notifying systemd of successful initialization\n", __FUNCTION__);
   sd_notifyf(0, "READY=1\nSTATUS=ctrlm-main has successfully initialized\nMAINPID=%lu", (unsigned long)getpid());
#endif
   return false;
}

ctrlm_power_state_t ctrlm_main_get_power_state(void) {
   return(g_ctrlm.power_state);
}

void ctrlm_trigger_startup_actions(void) {
   ctrlm_main_queue_msg_header_t *msg = (ctrlm_main_queue_msg_header_t *)g_malloc(sizeof(ctrlm_main_queue_msg_header_t));
   if(msg == NULL) {
      LOG_ERROR("%s: Out of memory\n", __FUNCTION__);
   } else {
      msg->type       = CTRLM_MAIN_QUEUE_MSG_TYPE_STARTUP;
      msg->network_id = CTRLM_MAIN_NETWORK_ID_ALL;
      ctrlm_main_queue_msg_push((gpointer)msg);
   }
}
