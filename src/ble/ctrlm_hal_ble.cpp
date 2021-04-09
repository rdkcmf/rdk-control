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
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <dirent.h>

#include <gio/gunixfdlist.h>
#include <glib/gstdio.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include "ctrlm_ble_utils.h"
#include "ctrlm_hal_ble.h"
#include "../ctrlm.h"
#include "../ctrlm_hal.h"
#include "../ctrlm_utils.h"
#include "../json_config.h"
#include "ctrlm_ble_network.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CTRLM_HAL_BLE_SKY_RCU_DBUS_NAME                     "com.sky.blercu"
#define CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_CONTROLLER     "com.sky.blercu.Controller1"
#define CTRLM_HAL_BLE_SKY_RCU_DBUS_OBJECT_PATH_BASE         "/com/sky/blercu"
#define CTRLM_HAL_BLE_SKY_RCU_DBUS_OBJECT_PATH_CONTROLLER   "/com/sky/blercu/controller"
#define CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_DEVICE         "com.sky.blercu.Device1"
#define CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_INFRARED       "com.sky.blercu.Infrared1"
#define CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_DEBUG          "com.sky.blercu.Debug1"
#define CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_UPGRADE        "com.sky.blercu.Upgrade1"

#define CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_PROPERTIES     "org.freedesktop.DBus.Properties"
#define CTRLM_HAL_BLE_SKY_RCU_DBUS_GET_PROPERTIES           "org.freedesktop.DBus.Properties.Get"

#define CODE_VOICE_KEY      66
#define KEY_INPUT_DEVICE_BASE_DIR    "/dev/input/"
#define KEY_INPUT_DEVICE_BASE_FILE   "event"

// Timeout (in ms) for all synchronous dbus calls to the BleRcuDaemon
#define CTRLM_BLE_G_DBUS_CALL_TIMEOUT_DEFAULT   2000
#define CTRLM_BLE_G_DBUS_CALL_TIMEOUT_LONG      10000

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function Prototypes
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ctrlm_hal_result_t   ctrlm_hal_ble_DaemonInit();

static bool                 ctrlm_hal_ble_dbus_init(void);

static bool ctrlm_hal_ble_HandleAddedDevice(unsigned long long ieee_address);
static void ctrlm_hal_ble_HandleRemovedDevice(unsigned long long ieee_address);
static void ctrlm_hal_ble_ParseVariantToRcuProperty(std::string prop, GVariant *value, ctrlm_hal_ble_RcuStatusData_t &rcu_status);
static void ctrlm_hal_ble_ReadDbusDictRcuProperties(GVariant *iter, ctrlm_hal_ble_RcuStatusData_t &rcu_data);

static int      ctrlm_hal_ble_OpenKeyInputDevice(unsigned long long ieee_address);
static void*    ctrlm_hal_ble_KeyMonitorThread(void *data);
static int      ctrlm_hal_ble_HandleKeypress(struct input_event *ev, unsigned long long macAddr);

static bool ctrlm_hal_ble_SetupDeviceDbusProxy(unsigned long long ieee_address, GDBusProxy **gdbus_proxy);
static bool ctrlm_hal_ble_SetupUpgradeDbusProxy(unsigned long long ieee_address, GDBusProxy **gdbus_proxy);

static void ctrlm_hal_ble_DBusOnSignalReceivedCB (GDBusProxy *proxy,
                                                    gchar      *sender_name,
                                                    gchar      *signal_name,
                                                    GVariant   *parameters,
                                                    gpointer    user_data);
static void ctrlm_hal_ble_DBusOnPropertyChangedCB (GDBusProxy *proxy,
                                                    GVariant   *changed_properties,
                                                    GStrv       invalidated_properties,
                                                    gpointer    user_data);

static ctrlm_hal_result_t ctrlm_hal_ble_req_StartThreads(void);
static ctrlm_hal_result_t ctrlm_hal_ble_req_PropertyGet(ctrlm_hal_network_property_t property, void **value);
static ctrlm_hal_result_t ctrlm_hal_ble_req_PropertySet(ctrlm_hal_network_property_t property, void *value);
static ctrlm_hal_result_t ctrlm_hal_ble_req_GetDevices(std::vector<unsigned long long> &ieee_addresses);
static ctrlm_hal_result_t ctrlm_hal_ble_GetAllRcuProperties(ctrlm_hal_ble_RcuStatusData_t &rcu_data);
static ctrlm_hal_result_t ctrlm_hal_ble_req_StartDiscovery(ctrlm_hal_ble_StartDiscovery_params_t params);
static ctrlm_hal_result_t ctrlm_hal_ble_req_PairWithCode(ctrlm_hal_ble_PairWithCode_params_t params);
static ctrlm_hal_result_t ctrlm_hal_ble_req_Unpair(unsigned long long ieee_address);
static ctrlm_hal_result_t ctrlm_hal_ble_req_StartAudioStream(ctrlm_hal_ble_StartAudioStream_params_t *params);
static ctrlm_hal_result_t ctrlm_hal_ble_req_StopAudioStream(ctrlm_hal_ble_StopAudioStream_params_t params);
static ctrlm_hal_result_t ctrlm_hal_ble_req_GetAudioStats(ctrlm_hal_ble_GetAudioStats_params_t *params);

static void ctrlm_hal_ble_IR_ResultCB(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data);
static ctrlm_hal_result_t ctrlm_hal_ble_req_IRSetCode(ctrlm_hal_ble_IRSetCode_params_t params);
static ctrlm_hal_result_t ctrlm_hal_ble_req_IRClear(ctrlm_hal_ble_IRClear_params_t params);

static ctrlm_hal_result_t ctrlm_hal_ble_req_FindMe(ctrlm_hal_ble_FindMe_params_t params);
static ctrlm_hal_result_t ctrlm_hal_ble_req_GetDaemonLogLevel(daemon_logging_t *logging);
static ctrlm_hal_result_t ctrlm_hal_ble_req_SetDaemonLogLevel(daemon_logging_t logging);

static ctrlm_hal_result_t ctrlm_hal_ble_req_FwUpgrade(ctrlm_hal_ble_FwUpgrade_params_t params);

static ctrlm_hal_result_t ctrlm_hal_ble_req_GetRcuUnpairReason(ctrlm_hal_ble_GetRcuUnpairReason_params_t *params);
static ctrlm_hal_result_t ctrlm_hal_ble_req_GetRcuRebootReason(ctrlm_hal_ble_GetRcuRebootReason_params_t *params);
static void ctrlm_hal_ble_RcuAction_ResultCB(GDBusProxy *proxy, GAsyncResult *res, gpointer user_data);
static ctrlm_hal_result_t ctrlm_hal_ble_req_SendRcuAction(ctrlm_hal_ble_SendRcuAction_params_t params);

static ctrlm_hal_result_t ctrlm_hal_ble_req_Terminate(void);


// -----------------------------------
// Utility functions
// -----------------------------------
static void ctrlm_hal_ble_UnitTestSkyIarm() __attribute__((unused));

static void ctrlm_hal_ble_queue_msg_destroy(gpointer msg);

static ctrlm_hal_result_t ctrlm_hal_ble_dbusSendMethodCall (GDBusProxy         *gdbus_proxy,
                                                            const char*         apcMethod,
                                                            GVariant           *params,
                                                            GVariant          **reply,
                                                            unsigned int        dbus_call_timeout = CTRLM_BLE_G_DBUS_CALL_TIMEOUT_DEFAULT,
                                                            GUnixFDList       **out_fds = NULL,
                                                            GUnixFDList        *in_fds = NULL,
                                                            bool                call_async = false,
                                                            GAsyncReadyCallback callback = NULL);

static ctrlm_hal_result_t ctrlm_hal_ble_GetDbusProperty(GDBusProxy   *gdbus_proxy,
                                                        const char   *interface,
                                                        const char   *propertyName,
                                                        GVariant    **propertyValue);
static ctrlm_hal_result_t ctrlm_hal_ble_SetDbusProperty(GDBusProxy   *gdbus_proxy,
                                                        const char   *interface,
                                                        const char   *propertyName,
                                                        GVariant     *value);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Structs and Globals
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class rcu_metadata_t {
    public:
        rcu_metadata_t() {
            LOG_DEBUG ("%s: constructor...\n", __FUNCTION__);
            gdbus_proxy_device_ifce = NULL;
            gdbus_proxy_upgrade_ifce = NULL;
            input_device_fd = -1;
            device_minor_id = -1;
        }
        ~rcu_metadata_t() {
            // LOG_WARN ("%s: destructor...\n", __FUNCTION__);
        }

        GDBusProxy *gdbus_proxy_device_ifce;
        GDBusProxy *gdbus_proxy_upgrade_ifce;
        int         input_device_fd;
        int         device_minor_id;
};

class ctrlm_hal_ble_global_t{
    public:
        ctrlm_hal_ble_global_t() {
            LOG_DEBUG ("%s: constructor...\n", __FUNCTION__);

            gdbus_proxy_controller_ifce     = NULL;
            gdbus_proxy_controller_props    = NULL;

            key_thread_id       = NULL;
            key_thread_queue    = NULL;

            g_atomic_int_set(&running, 0);
            g_mutex_init(&mutex_metadata);
        }
        ~ctrlm_hal_ble_global_t() {
            if (NULL != gdbus_proxy_controller_ifce) {
                g_object_unref(gdbus_proxy_controller_ifce);
                gdbus_proxy_controller_ifce = NULL;
            }
            if (NULL != gdbus_proxy_controller_props) {
                g_object_unref(gdbus_proxy_controller_props);
                gdbus_proxy_controller_props = NULL;
            }
            LOG_INFO ("%s: waiting for HAL threads to exit...\n", __FUNCTION__);
            g_atomic_int_set(&running, 0);
            if (NULL != key_thread_id) {
                g_thread_join(key_thread_id);
                key_thread_id = NULL;
            }

            for (auto it = rcu_metadata.begin(); it != rcu_metadata.end(); it++) {
                if (it->second.input_device_fd >= 0) {
                    close(it->second.input_device_fd);
                }
                if (NULL != it->second.gdbus_proxy_device_ifce) {
                    g_object_unref(it->second.gdbus_proxy_device_ifce);
                }
                if (NULL != it->second.gdbus_proxy_upgrade_ifce) {
                    g_object_unref(it->second.gdbus_proxy_upgrade_ifce);
                }
            }
            rcu_metadata.clear();

            g_mutex_clear(&mutex_metadata);
        }

        GDBusProxy* getDbusDeviceIfceProxy (unsigned long long ieee_address) {
            if (rcu_metadata.end() == rcu_metadata.find(ieee_address)) {
                LOG_WARN ("%s, %d: gdbus proxy for device (0x%llX) NOT found, returning NULL!!!!!!!\n", __FUNCTION__, __LINE__, ieee_address);
                return NULL;
            } else {
                return rcu_metadata[ieee_address].gdbus_proxy_device_ifce;
            }
        }
        GDBusProxy* getDbusUpgradeIfceProxy (unsigned long long ieee_address) {
            if (rcu_metadata.end() == rcu_metadata.find(ieee_address)) {
                LOG_WARN ("%s, %d: gdbus proxy for device (0x%llX) NOT found, returning NULL!!!!!!!\n", __FUNCTION__, __LINE__, ieee_address);
                return NULL;
            } else {
                return rcu_metadata[ieee_address].gdbus_proxy_upgrade_ifce;
            }
        }


        bool is_running() {
            return (g_atomic_int_get(&running) == 1) ? true : false;
        }
        void set_running(bool r) {
            g_atomic_int_set(&running, r ? 1 : 0);
        }

        ctrlm_hal_ble_main_init_t main_init;

        volatile int    running;
        GDBusProxy     *gdbus_proxy_controller_ifce;
        GDBusProxy     *gdbus_proxy_controller_props;

        GThread        *key_thread_id;
        GAsyncQueue    *key_thread_queue;

        GMutex          mutex_metadata;

        std::map <unsigned long long, rcu_metadata_t> rcu_metadata;    // indexed with ieee_address of the RCU
};
static ctrlm_hal_ble_global_t *g_ctrlm_hal_ble;

typedef struct {
   ctrlm_controller_id_t      controller_id;
   unsigned long long         ieee_address;
   int                        fd;
   sem_t                      semaphore;
} ctrlm_hal_ble_ReadVoiceSocket_params_t;

typedef enum {
    CTRLM_BLE_KEY_QUEUE_MSG_TYPE_TICKLE
} ctrlm_ble_key_queue_msg_type_t;

typedef struct {
   ctrlm_ble_key_queue_msg_type_t type;
} ctrlm_ble_key_queue_msg_header_t;

typedef struct {
    ctrlm_ble_key_queue_msg_header_t    header;
    ctrlm_hal_thread_monitor_response_t *response;
} ctrlm_ble_key_thread_monitor_msg_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ctrlm_hal_result_t ctrlm_hal_ble_req_Terminate(void)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);

    delete(g_ctrlm_hal_ble);
    return CTRLM_HAL_RESULT_SUCCESS;
}


// ==================================================================================================================================================================
// Init functions of the BLE HAL
// ==================================================================================================================================================================

bool ctrlm_hal_ble_dbus_init(void)
{
    bool result = true;
    GError *error = NULL;
    GDBusProxy *gdbus_proxy = NULL;

    gdbus_proxy = g_dbus_proxy_new_for_bus_sync (   G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_NAME,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_OBJECT_PATH_CONTROLLER,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_CONTROLLER,
                                                    NULL, &error );
    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        result = false;
    } else {
        g_ctrlm_hal_ble->gdbus_proxy_controller_ifce = gdbus_proxy;

        if ( 0 > g_signal_connect ( g_ctrlm_hal_ble->gdbus_proxy_controller_ifce, "g-signal",
                                    G_CALLBACK (ctrlm_hal_ble_DBusOnSignalReceivedCB), NULL ) ) {
            LOG_ERROR ("%s: Failed connecting to signals (%s) on interface %s!!!\n", __FUNCTION__, "g-signal", CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_CONTROLLER);
            result = false;
        } else {
            LOG_INFO ("%s: Successfully connected to signals (%s) on interface %s\n", __FUNCTION__, "g-signal", CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_CONTROLLER);
        }
        if ( 0 > g_signal_connect ( g_ctrlm_hal_ble->gdbus_proxy_controller_ifce, "g-properties-changed",
                                    G_CALLBACK (ctrlm_hal_ble_DBusOnPropertyChangedCB), NULL ) ) {
            LOG_ERROR ("%s: Failed connecting to signals (%s) on interface %s!!!\n", __FUNCTION__, "g-properties-changed", CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_CONTROLLER);
            result = false;
        } else {
            LOG_INFO ("%s: Successfully connected to signals (%s) on interface %s\n", __FUNCTION__, "g-properties-changed", CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_CONTROLLER);
        }
    }
    gdbus_proxy = g_dbus_proxy_new_for_bus_sync (   G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_NAME,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_OBJECT_PATH_CONTROLLER,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_PROPERTIES,
                                                    NULL, &error );
    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        result = false;
    } else {
        g_ctrlm_hal_ble->gdbus_proxy_controller_props = gdbus_proxy;
    }

    if (false == result) {
        LOG_ERROR ("%s: an error occurred, unref all gdbus proxies...\n", __FUNCTION__);
        if (NULL != g_ctrlm_hal_ble->gdbus_proxy_controller_ifce) {
            g_object_unref(g_ctrlm_hal_ble->gdbus_proxy_controller_ifce);
        }
        if (NULL != g_ctrlm_hal_ble->gdbus_proxy_controller_props) {
            g_object_unref(g_ctrlm_hal_ble->gdbus_proxy_controller_props);
        }
    }

    return result;
}

static ctrlm_hal_result_t ctrlm_hal_ble_DaemonInit() {
    LOG_INFO("%s: Enter...\n", __FUNCTION__);
    ctrlm_hal_result_t result = CTRLM_HAL_RESULT_SUCCESS;

    LOG_INFO("%s: Register DBus signal handlers...\n", __FUNCTION__);
    if (true == ctrlm_hal_ble_dbus_init()) {
        GVariant *reply;
        // Lets not hold up ctrlm init if BleRcuDaemon is not ready.  DBus calls to the daemon will fail, but it recovers automatically once the daemon is initialized.
        if (CTRLM_HAL_RESULT_SUCCESS == ctrlm_hal_ble_dbusSendMethodCall (  g_ctrlm_hal_ble->gdbus_proxy_controller_ifce, "IsReady", NULL, &reply ) ) {
            LOG_INFO("%s: IsReady() returned SUCCESS, so BleRcuDaemon is ready\n", __FUNCTION__);
        } else {
            LOG_WARN("%s: IsReady() returned ERROR, so BleRcuDaemon is not ready, dbus calls to it will probably fail...\n", __FUNCTION__);
        }
    }
    LOG_INFO("%s: Get list of currently connected devices, and register RCU Device interface DBus signal handlers...\n", __FUNCTION__);
    vector<unsigned long long> devices;
    ctrlm_hal_ble_req_GetDevices(devices);

    for (auto &it : devices) {
        LOG_INFO ("%s: Setting up HAL device metadata for ieee_address = 0x%llX\n", __FUNCTION__, it);
        if (false == ctrlm_hal_ble_HandleAddedDevice(it)) {
            result = CTRLM_HAL_RESULT_ERROR;
        }
    }
    return result;
}

void *ctrlm_hal_ble_main(ctrlm_hal_ble_main_init_t *main_init_)
{
    LOG_INFO("%s: Network id: %u\n", __FUNCTION__, (unsigned)main_init_->network_id);
    ctrlm_hal_result_t result = CTRLM_HAL_RESULT_SUCCESS;

    g_ctrlm_hal_ble = new ctrlm_hal_ble_global_t();

    ctrlm_hal_ble_DaemonInit();
    g_ctrlm_hal_ble->set_running(true);

    memcpy(&g_ctrlm_hal_ble->main_init, main_init_, sizeof(ctrlm_hal_ble_main_init_t));
    if (g_ctrlm_hal_ble->main_init.cfm_init != 0) {
        ctrlm_hal_ble_cfm_init_params_t params;
        params.result = result;
        strcpy(params.version,"1.0.0.0");

        params.term = ctrlm_hal_ble_req_Terminate;
        params.property_get = ctrlm_hal_ble_req_PropertyGet;
        params.property_set = ctrlm_hal_ble_req_PropertySet;
        params.start_threads = ctrlm_hal_ble_req_StartThreads;
        params.get_devices = ctrlm_hal_ble_req_GetDevices;
        params.get_all_rcu_props = ctrlm_hal_ble_GetAllRcuProperties;
        params.start_discovery = ctrlm_hal_ble_req_StartDiscovery;
        params.pair_with_code = ctrlm_hal_ble_req_PairWithCode;
        params.unpair = ctrlm_hal_ble_req_Unpair;
        params.start_audio_stream = ctrlm_hal_ble_req_StartAudioStream;
        params.stop_audio_stream = ctrlm_hal_ble_req_StopAudioStream;
        params.get_audio_stats = ctrlm_hal_ble_req_GetAudioStats;
        params.set_ir_codes = ctrlm_hal_ble_req_IRSetCode;
        params.clear_ir_codes = ctrlm_hal_ble_req_IRClear;
        params.find_me = ctrlm_hal_ble_req_FindMe;
        params.get_daemon_log_levels = ctrlm_hal_ble_req_GetDaemonLogLevel;
        params.set_daemon_log_levels = ctrlm_hal_ble_req_SetDaemonLogLevel;
        params.fw_upgrade = ctrlm_hal_ble_req_FwUpgrade;
        params.get_rcu_unpair_reason = ctrlm_hal_ble_req_GetRcuUnpairReason;
        params.get_rcu_reboot_reason = ctrlm_hal_ble_req_GetRcuRebootReason;
        params.send_rcu_action = ctrlm_hal_ble_req_SendRcuAction;

        g_ctrlm_hal_ble->main_init.cfm_init(g_ctrlm_hal_ble->main_init.network_id, params);
    }
    return NULL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BEGIN - Incoming callbacks from Sky Bluetooth RCU Daemon
// -------------------------------------------------------------------------------------------------------------

static void ctrlm_hal_ble_DBusOnPropertyChangedCB ( GDBusProxy *proxy,
                                                    GVariant   *changed_properties,
                                                    GStrv       invalidated_properties,
                                                    gpointer    user_data)
{
    GVariantIter *iter;
    GVariant *value;
    gchar *key;

    string obj_path = string(g_dbus_proxy_get_object_path(proxy));
    LOG_DEBUG("%s: Recieved signal from object path = <%s>, variant type: '%s'\n", __FUNCTION__, obj_path.c_str(), g_variant_get_type_string (changed_properties));

    if (false == g_variant_is_of_type (changed_properties, G_VARIANT_TYPE_DICTIONARY)) {
        LOG_WARN("%s: changed_properties is NOT type dictionary\n", __FUNCTION__);
    } else {
        g_variant_get (changed_properties, "a{sv}", &iter);
        while (g_variant_iter_loop (iter, "{sv}", &key, &value)) {

            string prop(key);

            ctrlm_hal_ble_RcuStatusData_t rcu_status;
            rcu_status.rcu_data.ieee_address = ctrlm_ble_utils_GetIEEEAddressFromObjPath(obj_path);

            ctrlm_hal_ble_ParseVariantToRcuProperty(prop, value, rcu_status);

            if (CTRLM_HAL_BLE_PROPERTY_CONNECTED == rcu_status.property_updated && true == rcu_status.rcu_data.connected) {
                g_mutex_lock(&g_ctrlm_hal_ble->mutex_metadata);
                if (g_ctrlm_hal_ble->rcu_metadata.end() != g_ctrlm_hal_ble->rcu_metadata.find(rcu_status.rcu_data.ieee_address)) {
                    if (g_ctrlm_hal_ble->rcu_metadata[rcu_status.rcu_data.ieee_address].input_device_fd >= 0) {
                        LOG_INFO ("%s: RCU <0x%llX> RE-CONNECTED, closing key input device so key monitor thread can reopen...\n", __FUNCTION__, rcu_status.rcu_data.ieee_address);
                        close(g_ctrlm_hal_ble->rcu_metadata[rcu_status.rcu_data.ieee_address].input_device_fd);
                        g_ctrlm_hal_ble->rcu_metadata[rcu_status.rcu_data.ieee_address].input_device_fd = -1;
                    }
                }
                g_mutex_unlock(&g_ctrlm_hal_ble->mutex_metadata);
            }

            if (NULL != g_ctrlm_hal_ble->main_init.ind_status && CTRLM_HAL_BLE_PROPERTY_UNKNOWN != rcu_status.property_updated) {
                // send updated property up to the network one at a time
                if (CTRLM_HAL_RESULT_SUCCESS != g_ctrlm_hal_ble->main_init.ind_status(g_ctrlm_hal_ble->main_init.network_id, &rcu_status)) {
                    LOG_ERROR("%s: RCU status indication failed\n", __FUNCTION__);
                }
            } else {
                LOG_WARN("%s: NOT indicating status change to the network.\n", __FUNCTION__);
            }
        }
        g_variant_iter_free (iter);
    }
}

static void ctrlm_hal_ble_DBusOnSignalReceivedCB (  GDBusProxy *proxy,
                                                    gchar      *sender_name,
                                                    gchar      *signal_name,
                                                    GVariant   *parameters,
                                                    gpointer    user_data)
{
    LOG_DEBUG ("%s: Enter, signal: %s\n", __FUNCTION__, signal_name);

    gchar *objPath;
    gchar *macAddr;

    if (0 == g_strcmp0(signal_name, "DeviceAdded") ||
        0 == g_strcmp0(signal_name, "DeviceRemoved")) {

        g_variant_get (parameters, "(os)", &objPath, &macAddr);
        LOG_DEBUG("%s: objPath = <%s>, macAddr = <%s>\n", __FUNCTION__, objPath, macAddr);
        unsigned long long ieee_address = ctrlm_convert_mac_string_to_long(macAddr);
        g_free(objPath);
        g_free(macAddr);

        if (0 == g_strcmp0(signal_name, "DeviceAdded")) {
            ctrlm_hal_ble_RcuStatusData_t rcu_status;
            memset(&rcu_status, 0, sizeof(rcu_status));
            rcu_status.rcu_data.ieee_address = ieee_address;
            ctrlm_hal_ble_GetAllRcuProperties(rcu_status);

            //Setup GDBus proxy object and register Property-Changed callback for the device that was added
            if (true == ctrlm_hal_ble_HandleAddedDevice(ieee_address)) {
                if (NULL == g_ctrlm_hal_ble->main_init.ind_paired) {
                    LOG_ERROR("%s: ind_paired is NULL!!\n", __FUNCTION__);
                } else {
                    ctrlm_hal_ble_IndPaired_params_t params;
                    params.rcu_data = rcu_status.rcu_data;

                    // Indicate up to the ControlMgr BLE network
                    if (CTRLM_HAL_RESULT_SUCCESS != g_ctrlm_hal_ble->main_init.ind_paired(g_ctrlm_hal_ble->main_init.network_id, &params)) {
                        LOG_ERROR("%s: RCU paired indication failed\n", __FUNCTION__);
                    }
                }
            } else {
                LOG_ERROR("%s: Failed to setup HAL metadata for device: 0x%llX, not sending indication up to network\n", __FUNCTION__,ieee_address);
            }
        } else {    // DeviceRemoved
            ctrlm_hal_ble_HandleRemovedDevice(ieee_address);

            if (NULL == g_ctrlm_hal_ble->main_init.ind_unpaired) {
                LOG_ERROR("%s: ind_unpaired is NULL!!\n", __FUNCTION__);
            } else {
                ctrlm_hal_ble_IndUnPaired_params_t params;
                params.ieee_address = ieee_address;

                // Indicate up to the ControlMgr BLE network
                if (CTRLM_HAL_RESULT_SUCCESS != g_ctrlm_hal_ble->main_init.ind_unpaired(g_ctrlm_hal_ble->main_init.network_id, &params)) {
                    LOG_ERROR("%s: RCU unpaired indication failed\n", __FUNCTION__);
                }
            }
        }
    } else if (0 == g_strcmp0(signal_name, "Ready")) {
        LOG_INFO("%s: BleRcuDaemon \"Ready\" signal received\n", __FUNCTION__);
    } else if (0 == g_strcmp0(signal_name, "UpgradeError")) {
        gchar *upgrade_error;
        g_variant_get (parameters, "(s)", &upgrade_error);
        LOG_WARN("%s: RCU Firmware Upgrade Error = <%s>\n", __FUNCTION__, upgrade_error);
        g_free(upgrade_error);
    } else {
        LOG_DEBUG("%s: Ignoring irrelevant signal: %s\n", __FUNCTION__,signal_name);
    }
}

// -------------------------------------------------------------------------------------------------------------
// END - Incoming callbacks from Sky Bluetooth RCU Daemon
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BEGIN - HAL API Function definitions that are invoked by the network
// -------------------------------------------------------------------------------------------------------------

ctrlm_hal_result_t ctrlm_hal_ble_req_StartThreads()
{
    // Create an asynchronous queue to receive incoming messages
    g_ctrlm_hal_ble->key_thread_queue = g_async_queue_new_full(ctrlm_hal_ble_queue_msg_destroy);

    // Start key monitor thread.  This has to occur after the network as already added all the controllers
    // because this thread will indicate up the device node ID when it finds it.
    g_ctrlm_hal_ble->key_thread_id = g_thread_new("CTRLM_HAL_BLE_KeyMonitor", (void* (*)(void*))ctrlm_hal_ble_KeyMonitorThread, g_ctrlm_hal_ble);
    return CTRLM_HAL_RESULT_SUCCESS;
}

ctrlm_hal_result_t ctrlm_hal_ble_req_PropertyGet(ctrlm_hal_network_property_t property, void **value)
{
    LOG_INFO("%s: STUB, property: %s \n", __FUNCTION__,ctrlm_hal_network_property_str(property));
    return CTRLM_HAL_RESULT_ERROR_NOT_IMPLEMENTED;
}

ctrlm_hal_result_t ctrlm_hal_ble_req_PropertySet(ctrlm_hal_network_property_t property, void *value)
{
    LOG_DEBUG("%s: Property: %s \n", __FUNCTION__,ctrlm_hal_network_property_str(property));
    if (value == 0) {
        LOG_ERROR("%s: Property value is 0\n", __FUNCTION__);
        return CTRLM_HAL_RESULT_ERROR_INVALID_PARAMS;
    }
    if (property == CTRLM_HAL_NETWORK_PROPERTY_THREAD_MONITOR) {
        LOG_DEBUG("%s: ctrlm checks whether we are still alive\n", __FUNCTION__);

        if (g_ctrlm_hal_ble->key_thread_queue != NULL) {
            ctrlm_ble_key_thread_monitor_msg_t *msg = (ctrlm_ble_key_thread_monitor_msg_t *)g_malloc(sizeof(ctrlm_ble_key_thread_monitor_msg_t));
            if(NULL == msg) {
                LOG_FATAL("%s: Out of memory\n", __FUNCTION__);
                return CTRLM_HAL_RESULT_ERROR_OUT_OF_MEMORY;
            }
            msg->header.type = CTRLM_BLE_KEY_QUEUE_MSG_TYPE_TICKLE;
            msg->response = ((ctrlm_hal_network_property_thread_monitor_t *)value)->response;
            g_async_queue_push(g_ctrlm_hal_ble->key_thread_queue, msg);
            return CTRLM_HAL_RESULT_SUCCESS;
        } else {
            LOG_WARN("%s: key_thread_queue is not yet created\n", __FUNCTION__);
            return CTRLM_HAL_RESULT_ERROR;
        }
    }
    return CTRLM_HAL_RESULT_ERROR_INVALID_PARAMS;
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_StartDiscovery(ctrlm_hal_ble_StartDiscovery_params_t params)
{
    LOG_INFO("%s: Enter... params.timeout_ms = %d\n", __FUNCTION__, params.timeout_ms);

    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    guint32 timeout;
    if (params.timeout_ms <= 0) {
        timeout = CTRLM_HAL_BLE_DEFAULT_DISCOVERY_TIMEOUT_MS;
    } else {
        timeout = params.timeout_ms;
    }

    ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->gdbus_proxy_controller_ifce,
                                            "StartScanning",
                                            g_variant_new ("(u)",
                                                           timeout),
                                            &reply);
    if (NULL != reply) { g_variant_unref(reply); }
    return ret;
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_PairWithCode(ctrlm_hal_ble_PairWithCode_params_t params)
{
    LOG_INFO("%s: Enter... params.pair_code = 0x%X\n", __FUNCTION__, params.pair_code);

    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    guchar pair_code = (guchar) params.pair_code;

    ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->gdbus_proxy_controller_ifce,
                                            "StartPairingMacHash",
                                            g_variant_new ("(y)",
                                                           pair_code),
                                            &reply);
    if (NULL != reply) { g_variant_unref(reply); }
    return ret;
}

ctrlm_hal_result_t ctrlm_hal_ble_req_Unpair(unsigned long long ieee_address)
{
    LOG_INFO("%s: Enter... requesting to unpair device = 0x%llX\n", __FUNCTION__, ieee_address);
    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->gdbus_proxy_controller_ifce,
                                            "Unpair",
                                            g_variant_new ("(s)",
                                                           ctrlm_convert_mac_long_to_string(ieee_address).c_str()),
                                            &reply);
    if (NULL != reply) { g_variant_unref(reply); }
    return ret;
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_StartAudioStream(ctrlm_hal_ble_StartAudioStream_params_t *params)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);

    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    // encoding options, currently VSDK doesn't support 100 byte ADPCM, so we'll let the sky daemon decode to PCM for us.
    // enum class StreamAudioRequestType
    // {
    //     ADPCM = 1,
    //     PCM = 2
    // };
    GUnixFDList *fd_list;
    
    ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->getDbusDeviceIfceProxy(params->ieee_address),
                                            "StartAudioStreaming",
                                            g_variant_new ("(u)",
                                                        (guint32) (params->encoding == CTRLM_HAL_BLE_ENCODING_PCM ? 2 : 1)),
                                            &reply,
                                            CTRLM_BLE_G_DBUS_CALL_TIMEOUT_DEFAULT,
                                            &fd_list);
    if (CTRLM_HAL_RESULT_SUCCESS == ret && NULL != reply) {
        gint fd = -1;
        GError *error = NULL;
        fd = g_unix_fd_list_get (fd_list, 0, &error);
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
            ret = CTRLM_HAL_RESULT_ERROR;
            g_clear_error (&error);
        } else if (fd >= 0) {
            params->fd = fd;
            LOG_INFO("%s: Received valid voice data file descriptor, returning with params->fd = <%d>\n", __FUNCTION__, params->fd);
        } else {
            LOG_ERROR ("%s: Received invalid voice data file descriptor <-1>\n", __FUNCTION__);
            ret = CTRLM_HAL_RESULT_ERROR;
        }
    } else {
        LOG_ERROR("%s: FAILED!!!\n", __FUNCTION__);
    }
    if (NULL != reply) { g_variant_unref(reply); }
    return ret;
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_StopAudioStream(ctrlm_hal_ble_StopAudioStream_params_t params)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);

    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    // dbus API for sky daemon prefers that StopAudioStreaming not be called, and instead simply close the fd.
    // It will detect the close and send stop message to RCU.
    // In practice however, this takes too long, so I'm doing both here (close fd done in VSDK and send StopAudioStreaming here).

    ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->getDbusDeviceIfceProxy(params.ieee_address),
                                            "StopAudioStreaming",
                                            NULL,
                                            &reply);

    if (NULL != reply) { g_variant_unref(reply); }
    return ret;
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_GetAudioStats(ctrlm_hal_ble_GetAudioStats_params_t *params)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);

    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->getDbusDeviceIfceProxy(params->ieee_address),
                                            "GetAudioStatus",
                                            NULL,
                                            &reply);

    guint32 error_status, packets_received, packets_expected;
    g_variant_get (reply, "(uuu)", &error_status, &packets_received, &packets_expected);
    params->error_status = error_status;
    params->packets_received = packets_received;
    params->packets_expected = packets_expected;

    if (NULL != reply) { g_variant_unref(reply); }
    return ret;
}

static void ctrlm_hal_ble_IR_ResultCB(GDBusProxy   *proxy,
                                      GAsyncResult *res,
                                      gpointer      user_data)
{
    LOG_DEBUG ("%s, Enter...\n", __FUNCTION__);
    GError *error = NULL;
    GVariant *reply;
    bool success = false;

    reply = g_dbus_proxy_call_finish (proxy, res, &error);
    if (NULL == reply) {
        // Will return NULL if there's an error reported
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        success = false;
    } else {
        success = true;
    }
    if (NULL != reply) { g_variant_unref(reply); }

    ctrlm_hal_ble_RcuStatusData_t rcu_status;
    rcu_status.ir_state = success ? CTRLM_IR_STATE_COMPLETE : CTRLM_IR_STATE_FAILED;
    rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_IR_STATE;
    if (NULL != g_ctrlm_hal_ble->main_init.ind_status) {
        // report up to the network
        if (CTRLM_HAL_RESULT_SUCCESS != g_ctrlm_hal_ble->main_init.ind_status(g_ctrlm_hal_ble->main_init.network_id, &rcu_status)) {
            LOG_ERROR("%s: RCU status indication failed\n", __FUNCTION__);
        }
    } else {
        LOG_WARN("%s: status callback to the network is NULL\n", __FUNCTION__);
    }
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_IRSetCode(ctrlm_hal_ble_IRSetCode_params_t params)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);

    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    if(!params.ir_codes) {
        LOG_ERROR("%s: params.ir_codes is NULL!!!\n", __FUNCTION__);
        return CTRLM_HAL_RESULT_ERROR_INVALID_PARAMS;
    }

    GVariantBuilder key_array_builder;
    //g_dbus_proxy_call requires that variant parameter be wrapped in a tuple
    g_variant_builder_init(&key_array_builder, G_VARIANT_TYPE("(a{qay})"));
    g_variant_builder_open(&key_array_builder, G_VARIANT_TYPE("a{qay}"));
    for(const auto &it_key : *params.ir_codes) {
        LOG_INFO("%s: key = <0x%X>, it_key.second.size() = <%d>\n", __FUNCTION__, it_key.first, it_key.second.size());
        ctrlm_hal_ble_IrKeyCodes_t key_code;
        switch (it_key.first) {
            case CTRLM_KEY_CODE_INPUT_SELECT:
                key_code = CTRLM_HAL_BLE_IR_KEY_INPUT;
                break;
            case CTRLM_KEY_CODE_TV_POWER:
            case CTRLM_KEY_CODE_AVR_POWER_TOGGLE:
                key_code = CTRLM_HAL_BLE_IR_KEY_POWER;
                break;
            case CTRLM_KEY_CODE_VOL_UP:
                key_code = CTRLM_HAL_BLE_IR_KEY_VOL_UP;
                break;
            case CTRLM_KEY_CODE_VOL_DOWN:
                key_code = CTRLM_HAL_BLE_IR_KEY_VOL_DOWN;
                break;
            case CTRLM_KEY_CODE_MUTE:
                key_code = CTRLM_HAL_BLE_IR_KEY_MUTE;
                break;
            default:
                LOG_WARN("%s: Unhandled key received <0x%X>, ignoring...\n", __FUNCTION__, it_key.first);
                continue;
        }
        g_variant_builder_open(&key_array_builder, G_VARIANT_TYPE("{qay}"));
        g_variant_builder_add(&key_array_builder, "q", (guint16)key_code);
        g_variant_builder_open(&key_array_builder, G_VARIANT_TYPE("ay"));
        for(const auto &it_code : it_key.second) {
            g_variant_builder_add(&key_array_builder, "y", it_code);
        }
        g_variant_builder_close(&key_array_builder);
        g_variant_builder_close(&key_array_builder);
    }
    g_variant_builder_close(&key_array_builder);

    //g_dbus_proxy_call will consume the floating Variant reference returned by g_variant_builder_end, so no need to unref
    ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->getDbusDeviceIfceProxy(params.ieee_address),
                                            "ProgramIrSignalWaveforms",
                                            g_variant_builder_end(&key_array_builder),
                                            &reply,
                                            CTRLM_BLE_G_DBUS_CALL_TIMEOUT_DEFAULT,
                                            NULL, NULL,
                                            true, (GAsyncReadyCallback)ctrlm_hal_ble_IR_ResultCB);
    
    // indicate preliminary status up to the network
    ctrlm_hal_ble_RcuStatusData_t rcu_status;
    rcu_status.ir_state = (CTRLM_HAL_RESULT_SUCCESS == ret) ? CTRLM_IR_STATE_WAITING : CTRLM_IR_STATE_FAILED;
    rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_IR_STATE;
    if (NULL != g_ctrlm_hal_ble->main_init.ind_status) {
        // report up to the network
        if (CTRLM_HAL_RESULT_SUCCESS != g_ctrlm_hal_ble->main_init.ind_status(g_ctrlm_hal_ble->main_init.network_id, &rcu_status)) {
            LOG_ERROR("%s: RCU status indication failed\n", __FUNCTION__);
        }
    } else {
        LOG_WARN("%s: status callback to the network is NULL\n", __FUNCTION__);
    }
    
    if (NULL != reply) { g_variant_unref(reply); }
    return ret;
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_IRClear(ctrlm_hal_ble_IRClear_params_t params)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);
    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->getDbusDeviceIfceProxy(params.ieee_address),
                                            "EraseIrSignals",
                                            NULL,
                                            &reply,
                                            CTRLM_BLE_G_DBUS_CALL_TIMEOUT_DEFAULT,
                                            NULL, NULL,
                                            true, (GAsyncReadyCallback)ctrlm_hal_ble_IR_ResultCB);
    if (NULL != reply) { g_variant_unref(reply); }

    // indicate preliminary status up to the network
    ctrlm_hal_ble_RcuStatusData_t rcu_status;
    rcu_status.ir_state = (CTRLM_HAL_RESULT_SUCCESS == ret) ? CTRLM_IR_STATE_WAITING : CTRLM_IR_STATE_FAILED;
    rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_IR_STATE;
    if (NULL != g_ctrlm_hal_ble->main_init.ind_status) {
        // report up to the network
        if (CTRLM_HAL_RESULT_SUCCESS != g_ctrlm_hal_ble->main_init.ind_status(g_ctrlm_hal_ble->main_init.network_id, &rcu_status)) {
            LOG_ERROR("%s: RCU status indication failed\n", __FUNCTION__);
        }
    } else {
        LOG_WARN("%s: status callback to the network is NULL\n", __FUNCTION__);
    }
    return ret;
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_FindMe(ctrlm_hal_ble_FindMe_params_t params)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);
    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->getDbusDeviceIfceProxy(params.ieee_address),
                                            "FindMe",
                                            g_variant_new ("(yi)",
                                                            (guchar)params.level,
                                                            (gint32)params.duration),
                                            &reply);
    if (NULL != reply) { g_variant_unref(reply); }
    return ret;
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_GetDaemonLogLevel(daemon_logging_t *logging)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);
    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    GError *error = NULL;
    GDBusProxy *gdbus_proxy = NULL;

    gdbus_proxy = g_dbus_proxy_new_for_bus_sync (   G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_NAME,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_OBJECT_PATH_CONTROLLER,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_PROPERTIES,
                                                    NULL,
                                                    &error );
    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        return CTRLM_HAL_RESULT_ERROR;
    } else {
        if (CTRLM_HAL_RESULT_SUCCESS == (ret = ctrlm_hal_ble_GetDbusProperty(gdbus_proxy, CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_DEBUG, "LogLevels", &reply))) {
            guint32 levels_ = 0;
            GVariant  *v = NULL;
            g_variant_get (reply, "(v)", &v);
            g_variant_get (v, "u", &levels_);
            logging->log_levels = levels_;
            if (NULL != v) { g_variant_unref(v); }
            LOG_INFO("%s: log_levels = 0x%X - debug=%s, info=%s, milestone=%s, warning=%s, error=%s, fatal=%s\n", __FUNCTION__, logging->log_levels,
                (logging->level.debug) ?  "TRUE" : "FALSE",
                (logging->level.info) ?  "TRUE" : "FALSE",
                (logging->level.milestone) ?  "TRUE" : "FALSE",
                (logging->level.warning) ?  "TRUE" : "FALSE",
                (logging->level.error) ?  "TRUE" : "FALSE",
                (logging->level.fatal) ?  "TRUE" : "FALSE"  );
        }
    }

    if (NULL != reply) { g_variant_unref(reply); }
    g_object_unref(gdbus_proxy);

    return ret;
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_SetDaemonLogLevel(daemon_logging_t logging)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);
    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    GError *error = NULL;
    GDBusProxy *gdbus_proxy = NULL;

    gdbus_proxy = g_dbus_proxy_new_for_bus_sync (   G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_NAME,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_OBJECT_PATH_CONTROLLER,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_PROPERTIES,
                                                    NULL,
                                                    &error );
    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        return CTRLM_HAL_RESULT_ERROR;
    } else {
        ret = ctrlm_hal_ble_SetDbusProperty(gdbus_proxy, CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_DEBUG, "LogLevels", g_variant_new("u", logging.log_levels));
    }

    if (NULL != reply) { g_variant_unref(reply); }
    g_object_unref(gdbus_proxy);

    return ret;
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_FwUpgrade(ctrlm_hal_ble_FwUpgrade_params_t params)
{
    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;

    int fd = -1;
    fd = g_open (params.file_path.c_str(), O_CLOEXEC | O_RDONLY, 0);
    if (fd < 0) {
        LOG_ERROR("%s: Error opening %s\n", __FUNCTION__, params.file_path.c_str());
        ret = CTRLM_HAL_RESULT_ERROR_INVALID_PARAMS;
    } else {
        LOG_INFO("%s: Successfully opened firmware file:  %s, fd = %d\n", __FUNCTION__, params.file_path.c_str(), fd);
        GUnixFDList *fd_list = NULL;
        fd_list = g_unix_fd_list_new();

        GError *error = NULL;
        if (g_unix_fd_list_append (fd_list, fd, &error) < 0) {
            if (NULL != error) {
                LOG_ERROR ("%s, %d: failed appending file descriptor to list, error = <%s>\n", __FUNCTION__, __LINE__, error->message);
            }
            g_clear_error (&error);
            ret = CTRLM_HAL_RESULT_ERROR;
        } else {
            GVariant  *reply = NULL;
            // g_dbus quirk: when sending a GUniuxFDList as a param to a method call, need to use g_dbus_proxy_call_with_unix_fd_list_sync
            // and put the fd_list in the in_fds param and for the variant parameter, it needs to be this: g_variant_new ("(h)", 0)
            // Anything other than 0 in the variant will not send the fd properly
            ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->getDbusUpgradeIfceProxy(params.ieee_address),
                                                    "StartUpgrade",
                                                    g_variant_new ("(h)", 0),
                                                    &reply,
                                                    CTRLM_BLE_G_DBUS_CALL_TIMEOUT_DEFAULT,
                                                    NULL,
                                                    fd_list);
            if (NULL != reply) { g_variant_unref(reply); }
        }
        if (NULL != fd_list) { g_object_unref(fd_list); }
        //g_unix_fd_list_append() duplicated the fd using dup(), so need to close the one opened here.
        close(fd);
    }
    return ret;
}


static ctrlm_hal_result_t ctrlm_hal_ble_req_GetRcuUnpairReason(ctrlm_hal_ble_GetRcuUnpairReason_params_t *params)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);
    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    params->reason = CTRLM_BLE_RCU_UNPAIR_REASON_INVALID;

    GVariant  *reply = NULL;
    GError *error = NULL;
    GDBusProxy *gdbus_proxy = NULL;

    gdbus_proxy = g_dbus_proxy_new_for_bus_sync (   G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_NAME,
                                                    ctrlm_ble_utils_BuildDBusDeviceObjectPath(CTRLM_HAL_BLE_SKY_RCU_DBUS_OBJECT_PATH_BASE, params->ieee_address).c_str(),
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_PROPERTIES,
                                                    NULL,
                                                    &error );
    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        return CTRLM_HAL_RESULT_ERROR;
    } else {
        if (CTRLM_HAL_RESULT_SUCCESS == (ret = ctrlm_hal_ble_GetDbusProperty(gdbus_proxy, CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_DEVICE, "UnpairReason", &reply))) {
            guint8 reason_ = 0;
            GVariant  *v = NULL;
            g_variant_get (reply, "(v)", &v);
            g_variant_get (v, "y", &reason_);
            params->reason = (ctrlm_ble_RcuUnpairReason_t)reason_;
            if (NULL != v) { g_variant_unref(v); }
        }
    }
    if (NULL != reply) { g_variant_unref(reply); }
    g_object_unref(gdbus_proxy);

    return ret;
}
static ctrlm_hal_result_t ctrlm_hal_ble_req_GetRcuRebootReason(ctrlm_hal_ble_GetRcuRebootReason_params_t *params)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);
    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    params->reason = CTRLM_BLE_RCU_REBOOT_REASON_INVALID;

    GVariant  *reply = NULL;
    GError *error = NULL;
    GDBusProxy *gdbus_proxy = NULL;

    gdbus_proxy = g_dbus_proxy_new_for_bus_sync (   G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_NAME,
                                                    ctrlm_ble_utils_BuildDBusDeviceObjectPath(CTRLM_HAL_BLE_SKY_RCU_DBUS_OBJECT_PATH_BASE, params->ieee_address).c_str(),
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_PROPERTIES,
                                                    NULL,
                                                    &error );
    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        return CTRLM_HAL_RESULT_ERROR;
    } else {
        if (CTRLM_HAL_RESULT_SUCCESS == (ret = ctrlm_hal_ble_GetDbusProperty(gdbus_proxy, CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_DEVICE, "RebootReason", &reply))) {
            guint8 reason_ = 0;
            GVariant  *v = NULL;
            g_variant_get (reply, "(v)", &v);
            g_variant_get (v, "y", &reason_);
            params->reason = (ctrlm_ble_RcuRebootReason_t)reason_;
            if (NULL != v) { g_variant_unref(v); }
        }
    }
    if (NULL != reply) { g_variant_unref(reply); }
    g_object_unref(gdbus_proxy);

    return ret;
}

static void ctrlm_hal_ble_RcuAction_ResultCB(GDBusProxy   *proxy,
                                             GAsyncResult *res,
                                             gpointer      user_data)
{
    LOG_DEBUG ("%s, Enter...\n", __FUNCTION__);
    GError *error = NULL;
    GVariant *reply;

    reply = g_dbus_proxy_call_finish (proxy, res, &error);
    if (NULL == reply) {
        // Will return NULL if there's an error reported
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        LOG_ERROR("%s, RCU Action FAILED to send!!\n", __FUNCTION__);
    } else {
        LOG_INFO("%s, RCU Action sent SUCCESSFULLY.\n", __FUNCTION__);
    }
    if (NULL != reply) { g_variant_unref(reply); }
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_SendRcuAction(ctrlm_hal_ble_SendRcuAction_params_t params)
{
    LOG_INFO("%s: Enter... sending RCU action = <%s>\n", __FUNCTION__, ctrlm_ble_rcu_action_str(params.action));
    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;

    GVariant  *reply = NULL;
    if (params.wait_for_reply) {
        // Since this could be called from a factory reset script, we want to wait long enough for the remote to receive 
        // the message.  If not, the factory reset script could delete bluez remote cache before the remote checks in.
        // In practice, I've seen this call take up to 8 seconds to return.
        ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->getDbusDeviceIfceProxy(params.ieee_address),
                                                "SendRcuAction",
                                                g_variant_new ("(y)",
                                                            (guint8) params.action),
                                                &reply,
                                                CTRLM_BLE_G_DBUS_CALL_TIMEOUT_LONG);
        if (CTRLM_HAL_RESULT_SUCCESS == ret) { LOG_INFO("%s, RCU Action sent SUCCESSFULLY.\n", __FUNCTION__); }
    } else {
        ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->getDbusDeviceIfceProxy(params.ieee_address),
                                                "SendRcuAction",
                                                g_variant_new ("(y)",
                                                            (guint8) params.action),
                                                &reply,
                                                CTRLM_BLE_G_DBUS_CALL_TIMEOUT_DEFAULT,
                                                NULL, NULL,
                                                true, (GAsyncReadyCallback)ctrlm_hal_ble_RcuAction_ResultCB);
    }
    if (NULL != reply) { g_variant_unref(reply); }
    return ret;
}


// -------------------------------------------------------------------------------------------------------------
// END - HAL API Function definitions that are invoked by the network
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BEGIN - Utility Function Definitions
// -------------------------------------------------------------------------------------------------------------

void ctrlm_hal_ble_queue_msg_destroy(gpointer msg) {
   if(msg) {
      LOG_DEBUG("%s: Free %p\n", __FUNCTION__, msg);
      g_free(msg);
   }
}

void ctrlm_hal_ble_UnitTestSkyIarm()
{
    unsigned long long rcuMacAddr = 0xE80FC80F5258;
    // ctrlm_hal_ble_req_StartDiscovery(5000);
    // {
    //     ctrlm_hal_ble_IRClear_params_t params;
    //     params.ieee_address = rcuMacAddr;
    //     ctrlm_hal_ble_req_IRClear(params);
    // }
    // {
    //     ctrlm_hal_ble_IRSetCode_params_t params;
    //     params.ieee_address = rcuMacAddr;
    //     params.type = CTRLM_IR_DEVICE_TV;
    //     params.code = 10647;
    //     ctrlm_hal_ble_req_IRSetCode(params);
    // }
    {
        ctrlm_hal_ble_StartAudioStream_params_t params;
        params.ieee_address = rcuMacAddr;
        params.encoding = CTRLM_HAL_BLE_ENCODING_PCM;
        ctrlm_hal_ble_req_StartAudioStream(&params);
    }
    g_usleep(5 * 1000000);
    {
        ctrlm_hal_ble_StopAudioStream_params_t params;
        params.ieee_address = rcuMacAddr;
        ctrlm_hal_ble_req_StopAudioStream(params);
    }
}

static void ctrlm_hal_ble_ParseVariantToRcuProperty(std::string prop, GVariant *value, ctrlm_hal_ble_RcuStatusData_t &rcu_status)
{
    gchar *str_variant = NULL;
    guchar char_variant;
    gboolean bool_variant;
    guint32 uint_variant;
    gint32 int_variant;

    rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_UNKNOWN;

    if (0 == prop.compare("Address")) {
        g_variant_get (value, "s", &str_variant);
        // IEEE address should not be changing, plus we get the IEEE address from the proxy object.  So ignore.
        LOG_WARN("%s: Item '%s' = <%s>, IGNORED...\n", __FUNCTION__, prop.c_str(), str_variant);
    } else if (0 == prop.compare("Manufacturer")) {
        g_variant_get (value, "s", &str_variant);
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), str_variant);
        strncpy(rcu_status.rcu_data.manufacturer, str_variant, sizeof(rcu_status.rcu_data.manufacturer));
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_MANUFACTURER;
    } else if (0 == prop.compare("Model")) {
        g_variant_get (value, "s", &str_variant);
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), str_variant);
        strncpy(rcu_status.rcu_data.model, str_variant, sizeof(rcu_status.rcu_data.model));
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_MODEL;
    } else if (0 == prop.compare("Name")) {
        g_variant_get (value, "s", &str_variant);
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), str_variant);
        strncpy(rcu_status.rcu_data.name, str_variant, sizeof(rcu_status.rcu_data.name));
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_NAME;
    } else if (0 == prop.compare("SerialNumber")) {
        g_variant_get (value, "s", &str_variant);
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), str_variant);
        strncpy(rcu_status.rcu_data.serial_number, str_variant, sizeof(rcu_status.rcu_data.serial_number));
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_SERIAL_NUMBER;
    } else if (0 == prop.compare("HardwareRevision")) {
        g_variant_get (value, "s", &str_variant);
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), str_variant);
        strncpy(rcu_status.rcu_data.hw_revision, str_variant, sizeof(rcu_status.rcu_data.hw_revision));
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_HW_REVISION;
    } else if (0 == prop.compare("FirmwareRevision")) {
        g_variant_get (value, "s", &str_variant);
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), str_variant);
        strncpy(rcu_status.rcu_data.fw_revision, str_variant, sizeof(rcu_status.rcu_data.fw_revision));
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_FW_REVISION;
    } else if (0 == prop.compare("SoftwareRevision")) {
        g_variant_get (value, "s", &str_variant);
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), str_variant);
        strncpy(rcu_status.rcu_data.sw_revision, str_variant, sizeof(rcu_status.rcu_data.sw_revision));
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_SW_REVISION;
    } else if (0 == prop.compare("BatteryLevel")) {
        g_variant_get (value, "y", &char_variant);
        LOG_DEBUG("%s: Item '%s' = <%u>\n", __FUNCTION__, prop.c_str(), char_variant);
        rcu_status.rcu_data.battery_level = (int)char_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_BATTERY_LEVEL;
    } else if (0 == prop.compare("Connected")) {
        g_variant_get (value, "b", &bool_variant);
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), bool_variant ? "TRUE":"FALSE");
        rcu_status.rcu_data.connected = bool_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_CONNECTED;
    } else if (0 == prop.compare("AudioGainLevel")) {
        g_variant_get (value, "i", &int_variant);
        LOG_INFO("%s: Item '%s' = <%d>\n", __FUNCTION__, prop.c_str(), int_variant);
        rcu_status.rcu_data.audio_gain_level = int_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_AUDIO_GAIN_LEVEL;
    } else if (0 == prop.compare("AudioStreaming")) {
        g_variant_get (value, "b", &bool_variant);
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), bool_variant ? "TRUE":"FALSE");
        rcu_status.rcu_data.audio_streaming = bool_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_AUDIO_STREAMING;
    } else if (0 == prop.compare("Controller")) {
        g_variant_get (value, "o", &str_variant);
        // Ignore this, we don't care about the dbus path to controller interface
        LOG_WARN("%s: Item '%s' = <%s>, IGNORED...\n", __FUNCTION__, prop.c_str(), str_variant);
    } else if (0 == prop.compare("IrCode")) {
        g_variant_get (value, "i", &int_variant);
        LOG_INFO("%s: Item '%s' = <%d>\n", __FUNCTION__, prop.c_str(), int_variant);
        rcu_status.rcu_data.ir_code = int_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_IR_CODE;
    } else if (0 == prop.compare("TouchMode")) {
        g_variant_get (value, "u", &uint_variant);
        LOG_INFO("%s: Item '%s' = <%u>\n", __FUNCTION__, prop.c_str(), uint_variant);
        rcu_status.rcu_data.touch_mode = uint_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_TOUCH_MODE;
    } else if (0 == prop.compare("TouchModeSettable")) {
        g_variant_get (value, "b", &bool_variant);
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), bool_variant ? "TRUE":"FALSE");
        rcu_status.rcu_data.touch_mode_settable = bool_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_TOUCH_MODE_SETTABLE;
    } else if (0 == prop.compare("Pairing")) {
        g_variant_get (value, "b", &bool_variant);
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), bool_variant ? "TRUE":"FALSE");
        rcu_status.is_pairing = bool_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_IS_PAIRING;
    } else if (0 == prop.compare("PairingCode")) {
        g_variant_get (value, "y", &char_variant);
        LOG_INFO("%s: Item '%s' = <%u>\n", __FUNCTION__, prop.c_str(), char_variant);
        rcu_status.pairing_code = (int)char_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_PAIRING_CODE;
    } else if (0 == prop.compare("State")) {
        g_variant_get (value, "u", &uint_variant);
        rcu_status.state = (ctrlm_ble_state_t)uint_variant;
        LOG_INFO("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), ctrlm_ble_utils_RcuStateToString(rcu_status.state));
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_STATE;
    } else if (0 == prop.compare("Upgrading")) {
        g_variant_get (value, "b", &bool_variant);
        LOG_DEBUG("%s: Item '%s' = <%s>\n", __FUNCTION__, prop.c_str(), bool_variant ? "TRUE":"FALSE");
        rcu_status.rcu_data.is_upgrading = bool_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_IS_UPGRADING;
    } else if (0 == prop.compare("Progress")) {
        g_variant_get (value, "i", &int_variant);
        LOG_DEBUG("%s: Item '%s' = <%d>\n", __FUNCTION__, prop.c_str(), int_variant);
        rcu_status.rcu_data.upgrade_progress = int_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_UPGRADE_PROGRESS;
    } else if (0 == prop.compare("UnpairReason")) {
        g_variant_get (value, "y", &char_variant);
        LOG_DEBUG("%s: Item '%s' = <%u>\n", __FUNCTION__, prop.c_str(), char_variant);
        rcu_status.rcu_data.unpair_reason = (ctrlm_ble_RcuUnpairReason_t)char_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_UNPAIR_REASON;
    } else if (0 == prop.compare("RebootReason")) {
        g_variant_get (value, "y", &char_variant);
        LOG_DEBUG("%s: Item '%s' = <%u>\n", __FUNCTION__, prop.c_str(), char_variant);
        rcu_status.rcu_data.reboot_reason = (ctrlm_ble_RcuRebootReason_t)char_variant;
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_REBOOT_REASON;
    } else {
        LOG_WARN("%s: Item '%s' with type '%s' is unhandled signal!!!!!!!\n", __FUNCTION__, prop.c_str(), g_variant_get_type_string (value));
        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_UNKNOWN;
    }
    if (NULL != str_variant) { g_free(str_variant); }
}

static ctrlm_hal_result_t ctrlm_hal_ble_dbusSendMethodCall (GDBusProxy         *gdbus_proxy,
                                                            const char*         apcMethod,
                                                            GVariant           *params,
                                                            GVariant          **reply,
                                                            unsigned int        dbus_call_timeout,
                                                            GUnixFDList       **out_fds,
                                                            GUnixFDList        *in_fds,
                                                            bool                call_async,
                                                            GAsyncReadyCallback callback)
{
    GError *error = NULL;
    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        return CTRLM_HAL_RESULT_ERROR;
    }
    if (true == call_async) {
        // Use g_dbus default timeout (25 seconds) for async calls, because programming IR can take a while.
        g_dbus_proxy_call (gdbus_proxy, apcMethod, params, G_DBUS_CALL_FLAGS_NONE, -1, NULL, callback, NULL);
    } else {
        *reply = g_dbus_proxy_call_with_unix_fd_list_sync (gdbus_proxy, apcMethod, params, G_DBUS_CALL_FLAGS_NONE, dbus_call_timeout, in_fds, out_fds, NULL, &error);
        if (NULL == *reply) {
            // g_dbus_proxy_call_sync will return NULL if there's an error reported
            if (NULL != error) {
                LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
            }
            g_clear_error (&error);
            return CTRLM_HAL_RESULT_ERROR;
        }
    }
    return CTRLM_HAL_RESULT_SUCCESS;
}

static ctrlm_hal_result_t ctrlm_hal_ble_GetDbusProperty(GDBusProxy   *gdbus_proxy,
                                                        const char   *interface,
                                                        const char   *propertyName,
                                                        GVariant    **propertyValue)
{
    LOG_DEBUG("%s: Enter\n", __FUNCTION__);
    GError *error = NULL;
    GVariant  *reply = NULL;

    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        return CTRLM_HAL_RESULT_ERROR;
    }

    reply = g_dbus_proxy_call_sync (gdbus_proxy,
                                    "Get",
                                    g_variant_new ("(ss)",
                                                   interface,
                                                   propertyName),
                                    G_DBUS_CALL_FLAGS_NONE, CTRLM_BLE_G_DBUS_CALL_TIMEOUT_DEFAULT,
                                    NULL, &error);
    if (NULL != reply) {
        LOG_DEBUG("%s: reply has type '%s'\n", __FUNCTION__, g_variant_get_type_string (reply));
        *propertyValue = reply;
    } else {
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        LOG_ERROR("%s, %d: Failed to retrieve RCU property (%s) !!\n", __FUNCTION__,__LINE__, propertyName);
        return CTRLM_HAL_RESULT_ERROR;
    }
    return CTRLM_HAL_RESULT_SUCCESS;
}

static ctrlm_hal_result_t ctrlm_hal_ble_SetDbusProperty(GDBusProxy   *gdbus_proxy,
                                                        const char   *interface,
                                                        const char   *propertyName,
                                                        GVariant     *value)
{
    LOG_DEBUG("%s: Enter\n", __FUNCTION__);
    GError *error = NULL;
    GVariant  *reply = NULL;

    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        return CTRLM_HAL_RESULT_ERROR;
    }

    reply = g_dbus_proxy_call_sync (gdbus_proxy,
                                    "Set",
                                    g_variant_new ("(ssv)",
                                                   interface,
                                                   propertyName,
                                                   value),
                                    G_DBUS_CALL_FLAGS_NONE, CTRLM_BLE_G_DBUS_CALL_TIMEOUT_DEFAULT,
                                    NULL, &error);
    if (NULL == reply) {
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        LOG_ERROR("%s, %d: Failed to set RCU property (%s) !!\n", __FUNCTION__,__LINE__, propertyName);
        return CTRLM_HAL_RESULT_ERROR;
    }
    return CTRLM_HAL_RESULT_SUCCESS;
}

static ctrlm_hal_result_t ctrlm_hal_ble_GetAllRcuProperties(ctrlm_hal_ble_RcuStatusData_t &rcu_status)
{
    LOG_DEBUG("%s: Enter\n", __FUNCTION__);
    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    unsigned long long ieee_address = rcu_status.rcu_data.ieee_address;

    GDBusProxy *gdbus_proxy = NULL;
    GError *error = NULL;
    gdbus_proxy = g_dbus_proxy_new_for_bus_sync (   G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_NAME,
                                                    ctrlm_ble_utils_BuildDBusDeviceObjectPath(CTRLM_HAL_BLE_SKY_RCU_DBUS_OBJECT_PATH_BASE, ieee_address).c_str(),
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_PROPERTIES,
                                                    NULL,
                                                    &error );
    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        return CTRLM_HAL_RESULT_ERROR;
    }

    GVariant  *reply = NULL;
    reply = g_dbus_proxy_call_sync (gdbus_proxy,
                                    "GetAll",
                                    g_variant_new ("(s)",
                                                   CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_DEVICE),
                                    G_DBUS_CALL_FLAGS_NONE, CTRLM_BLE_G_DBUS_CALL_TIMEOUT_DEFAULT,
                                    NULL, &error);

    if (NULL != reply) {
        ctrlm_hal_ble_ReadDbusDictRcuProperties(reply, rcu_status);
    } else {
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        LOG_ERROR("%s, %d: Failed to retrieve RCU property!!\n", __FUNCTION__,__LINE__);
        ret = CTRLM_HAL_RESULT_ERROR;
    }
    if (NULL != reply) { g_variant_unref(reply); }
    g_object_unref(gdbus_proxy);
    return ret;
}

static void ctrlm_hal_ble_ReadDbusDictRcuProperties(GVariant *variant, ctrlm_hal_ble_RcuStatusData_t &rcu_status)
{
    GVariantIter *iter;
    GVariant *value;
    gchar *key;

    g_variant_get (variant, "(a{sv})", &iter);
    while (g_variant_iter_loop (iter, "{sv}", &key, &value)) {
        string prop(key);

        // LOG_DEBUG("%s: Item '%s' has type '%s'\n", __FUNCTION__, key, g_variant_get_type_string (value));
        ctrlm_hal_ble_ParseVariantToRcuProperty(prop, value, rcu_status);
    }
    g_variant_iter_free (iter);
}

static ctrlm_hal_result_t ctrlm_hal_ble_req_GetDevices(std::vector<unsigned long long> &ieee_addresses)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);

    ctrlm_hal_result_t ret = CTRLM_HAL_RESULT_SUCCESS;
    GVariant  *reply = NULL;

    ret = ctrlm_hal_ble_dbusSendMethodCall (g_ctrlm_hal_ble->gdbus_proxy_controller_ifce,
                                            "GetDevices",
                                            NULL,
                                            &reply);
    if (CTRLM_HAL_RESULT_SUCCESS == ret && NULL != reply) {

        GVariantIter *iter;
        gchar *objPath;

        g_variant_get (reply, "(ao)", &iter);
        while (g_variant_iter_loop (iter, "o", &objPath)) {
            LOG_DEBUG("%s: Device connected having objPath = %s\n", __FUNCTION__, objPath);
            ieee_addresses.push_back(ctrlm_ble_utils_GetIEEEAddressFromObjPath(string(objPath)));
            // no need to free objPath here as long as we aren't breaking out of the loop prematurely
        }
        g_variant_iter_free (iter);
    }
    if (NULL != reply) { g_variant_unref(reply); }
    return ret;
}

static bool ctrlm_hal_ble_HandleAddedDevice(unsigned long long ieee_address) {
    bool result = false;

    LOG_DEBUG ("%s: device 0x%llX\n", __FUNCTION__, ieee_address);
    if (g_ctrlm_hal_ble->rcu_metadata.end() != g_ctrlm_hal_ble->rcu_metadata.find(ieee_address)) {
        LOG_WARN ("%s: already have metadata for device 0x%llX, delete it and re-initialize.\n", __FUNCTION__, ieee_address);
        ctrlm_hal_ble_HandleRemovedDevice(ieee_address);
    }
    GDBusProxy *gdbus_proxy_device = NULL;
    GDBusProxy *gdbus_proxy_upgrade = NULL;
    if (true == ctrlm_hal_ble_SetupDeviceDbusProxy(ieee_address, &gdbus_proxy_device)) {
        if (true == ctrlm_hal_ble_SetupUpgradeDbusProxy(ieee_address, &gdbus_proxy_upgrade)) {
            LOG_DEBUG ("%s: creating metadata for RCU 0x%llX\n", __FUNCTION__, ieee_address);
            g_mutex_lock(&g_ctrlm_hal_ble->mutex_metadata);
            g_ctrlm_hal_ble->rcu_metadata[ieee_address].gdbus_proxy_device_ifce = gdbus_proxy_device;
            g_ctrlm_hal_ble->rcu_metadata[ieee_address].gdbus_proxy_upgrade_ifce = gdbus_proxy_upgrade;
            g_mutex_unlock(&g_ctrlm_hal_ble->mutex_metadata);
            result = true;
        }
    } 
    if (false == result) {
        LOG_ERROR ("%s: Failed setting up GDBus proxy for device 0x%llX!!!\n", __FUNCTION__, ieee_address);
        if (NULL != gdbus_proxy_device) {
            g_object_unref(gdbus_proxy_device);
        }
        if (NULL != gdbus_proxy_upgrade) {
            g_object_unref(gdbus_proxy_upgrade);
        }
    }
    return result;
}
static void ctrlm_hal_ble_HandleRemovedDevice(unsigned long long ieee_address) {
    if (g_ctrlm_hal_ble->rcu_metadata.end() != g_ctrlm_hal_ble->rcu_metadata.find(ieee_address)) {
        g_mutex_lock(&g_ctrlm_hal_ble->mutex_metadata);
        LOG_DEBUG ("%s: erasing metadata for RCU 0x%llX\n", __FUNCTION__, ieee_address);

        if (g_ctrlm_hal_ble->rcu_metadata[ieee_address].input_device_fd >= 0) {
            close(g_ctrlm_hal_ble->rcu_metadata[ieee_address].input_device_fd);
        }
        if (NULL != g_ctrlm_hal_ble->rcu_metadata[ieee_address].gdbus_proxy_device_ifce) {
            g_object_unref(g_ctrlm_hal_ble->rcu_metadata[ieee_address].gdbus_proxy_device_ifce);
        }
        if (NULL != g_ctrlm_hal_ble->rcu_metadata[ieee_address].gdbus_proxy_upgrade_ifce) {
            g_object_unref(g_ctrlm_hal_ble->rcu_metadata[ieee_address].gdbus_proxy_upgrade_ifce);
        }
        g_ctrlm_hal_ble->rcu_metadata.erase(ieee_address);
        g_mutex_unlock(&g_ctrlm_hal_ble->mutex_metadata);
    }
}


static bool ctrlm_hal_ble_SetupDeviceDbusProxy(unsigned long long ieee_address, GDBusProxy **proxy)
{
    bool result = true;
    GError *error = NULL;
    GDBusProxy *gdbus_proxy = NULL;

    LOG_DEBUG ("%s: Creating GDBus proxy for device 0x%llX\n", __FUNCTION__, ieee_address);
    gdbus_proxy = g_dbus_proxy_new_for_bus_sync (   G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_NAME,
                                                    ctrlm_ble_utils_BuildDBusDeviceObjectPath(CTRLM_HAL_BLE_SKY_RCU_DBUS_OBJECT_PATH_BASE, ieee_address).c_str(),
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_DEVICE,
                                                    NULL, &error );
    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        result = false;
    } else {
        if ( 0 > g_signal_connect ( gdbus_proxy, "g-properties-changed",
                                    G_CALLBACK (ctrlm_hal_ble_DBusOnPropertyChangedCB), NULL ) ) {
            LOG_ERROR ("%s: Failed connecting to signals (%s) on interface %s!!!\n", __FUNCTION__, "g-properties-changed", CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_DEVICE);
            result = false;
        } else {
            LOG_INFO ("%s: Successfully connected to signals (%s) on interface %s\n", __FUNCTION__, "g-properties-changed", CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_DEVICE);
        }
    }
    *proxy = gdbus_proxy;
    return result;
}

static bool ctrlm_hal_ble_SetupUpgradeDbusProxy(unsigned long long ieee_address, GDBusProxy **proxy)
{
    bool result = true;
    GError *error = NULL;
    GDBusProxy *gdbus_proxy = NULL;

    LOG_DEBUG ("%s: Creating GDBus proxy for device 0x%llX\n", __FUNCTION__, ieee_address);
    gdbus_proxy = g_dbus_proxy_new_for_bus_sync (   G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_NAME,
                                                    ctrlm_ble_utils_BuildDBusDeviceObjectPath(CTRLM_HAL_BLE_SKY_RCU_DBUS_OBJECT_PATH_BASE, ieee_address).c_str(),
                                                    CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_UPGRADE,
                                                    NULL, &error );
    if (NULL == gdbus_proxy) {
        LOG_ERROR ("%s: NULL == gdbus_proxy\n", __FUNCTION__);
        if (NULL != error) {
            LOG_ERROR ("%s, %d: error = <%s>\n", __FUNCTION__, __LINE__, error->message);
        }
        g_clear_error (&error);
        result = false;
    } else {
        if ( 0 > g_signal_connect ( gdbus_proxy, "g-properties-changed",
                                    G_CALLBACK (ctrlm_hal_ble_DBusOnPropertyChangedCB), NULL ) ) {
            LOG_ERROR ("%s: Failed connecting to signals (%s) on interface %s!!!\n", __FUNCTION__, "g-properties-changed", CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_UPGRADE);
            result = false;
        } else if ( 0 > g_signal_connect ( gdbus_proxy, "g-signal",
                                    G_CALLBACK (ctrlm_hal_ble_DBusOnSignalReceivedCB), NULL ) ) {
            LOG_ERROR ("%s: Failed connecting to signals (%s) on interface %s!!!\n", __FUNCTION__, "g-signal", CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_UPGRADE);
            result = false;
        } else {
            LOG_INFO ("%s: Successfully connected to signals on interface %s\n", __FUNCTION__, CTRLM_HAL_BLE_SKY_RCU_DBUS_INTERFACE_UPGRADE);
        }
    }
    *proxy = gdbus_proxy;
    return result;
}

static int ctrlm_hal_ble_OpenKeyInputDevice(unsigned long long ieee_address)
{
    int retry = 0;
    do {
        string keyInputBaseDir(KEY_INPUT_DEVICE_BASE_DIR);
        DIR *dir_p = opendir(keyInputBaseDir.c_str());
        if (NULL == dir_p) {
            return -1;
        }
        dirent *file_p;
        while ((file_p = readdir(dir_p)) != NULL) {
            if(strstr(file_p->d_name, KEY_INPUT_DEVICE_BASE_FILE) != NULL) {
                //this is one of the event devices, open it and see if it belongs to this MAC
                string keyInputFilename = keyInputBaseDir + file_p->d_name;
                int input_fd = open(keyInputFilename.c_str(), O_RDONLY|O_NONBLOCK);
                if (input_fd >= 0) {
                    struct libevdev *evdev = NULL;
                    int rc = libevdev_new_from_fd(input_fd, &evdev);
                    if (rc < 0) {
                        LOG_ERROR("%s: Failed to init libevdev (%s)\n", __FUNCTION__, strerror(-rc));   //on failure, rc is negative errno
                    } else {
                        // LOG_DEBUG("%s: Input device name: <%s> ID: bus %#x vendor %#x product %#x, phys = <%s>, unique = <%s>\n", __FUNCTION__,libevdev_get_name(evdev),libevdev_get_id_bustype(evdev),libevdev_get_id_vendor(evdev),libevdev_get_id_product(evdev),libevdev_get_phys(evdev),libevdev_get_uniq(evdev));
                        unsigned long long evdev_macaddr = ctrlm_convert_mac_string_to_long(libevdev_get_uniq(evdev));
                        if (evdev_macaddr == ieee_address) {
                            LOG_INFO("%s, %d: Input Dev Node (%s) for device (0x%llX) FOUND, returning file descriptor: <%d>\n", 
                                    __FUNCTION__, __LINE__, keyInputFilename.c_str(), ieee_address, input_fd);

                            if (NULL != evdev) {
                                libevdev_free(evdev);
                                evdev = NULL;
                            }
                            closedir(dir_p);
                            return input_fd;
                        }
                    }
                    close(input_fd);
                    if (NULL != evdev) {
                        libevdev_free(evdev);
                        evdev = NULL;
                    }
                }
            }
        }
        closedir(dir_p);
        retry++;
    } while (retry < 1);

    return -1;
}

// -------------------------------------------------------------------------------------------------------------
// END - Utility Function Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BEGIN - Key Monitor Thread
// -------------------------------------------------------------------------------------------------------------
static int ctrlm_hal_ble_HandleKeypress(struct input_event *event, unsigned long long macAddr)
{
    // These are 'seperator' events.  Ignore for now.
    if (event->code == 0) {
        return 0;
    }
    switch (event->code) {
        case CODE_VOICE_KEY:
            if (1 == event->value) {
                LOG_INFO("%s: ------------------------------------------------------------------------\n", __FUNCTION__);
                LOG_INFO("%s: CODE_VOICE_KEY button PRESSED event for device: 0x%llX\n", __FUNCTION__, macAddr);
                LOG_INFO("%s: ------------------------------------------------------------------------\n", __FUNCTION__);

                if (NULL == g_ctrlm_hal_ble->main_init.req_voice_session_begin) {
                    LOG_WARN("%s: req_voice_session_begin is NULL!!\n", __FUNCTION__);
                } else {
                    ctrlm_hal_ble_ReqVoiceSession_params_t req_params;
                    req_params.ieee_address = macAddr;
                    // Send voice session begin request up to the BLE network
                    if (CTRLM_HAL_RESULT_SUCCESS != g_ctrlm_hal_ble->main_init.req_voice_session_begin(g_ctrlm_hal_ble->main_init.network_id, &req_params)) {
                        LOG_ERROR("%s: voice session begin request FAILED\n", __FUNCTION__);
                    }
                }
            }
            else if (0 == event->value) {
                LOG_INFO("%s: ------------------------------------------------------------------------\n", __FUNCTION__);
                LOG_INFO("%s: CODE_VOICE_KEY button RELEASED event for device: 0x%llX\n", __FUNCTION__, macAddr);
                LOG_INFO("%s: ------------------------------------------------------------------------\n", __FUNCTION__);

                if (NULL == g_ctrlm_hal_ble->main_init.req_voice_session_end) {
                    LOG_WARN("%s: req_voice_session_end is NULL!!\n", __FUNCTION__);
                } else {
                    ctrlm_hal_ble_ReqVoiceSession_params_t req_params;
                    req_params.ieee_address = macAddr;
                    // Send voice session begin request up to the BLE network
                    if (CTRLM_HAL_RESULT_SUCCESS != g_ctrlm_hal_ble->main_init.req_voice_session_end(g_ctrlm_hal_ble->main_init.network_id, &req_params)) {
                        LOG_ERROR("%s: voice session end request FAILED\n", __FUNCTION__);
                    }
                }
            }
            else if (2 == event->value) {
                // This is the key repeat event sent while holding down the button
            }
            break;
        default: break;
    }

    if (event->value >= 0 && event->value < 3) {
        ctrlm_hal_ble_IndKeypress_params_t req_params;
        req_params.ieee_address = macAddr;
        req_params.event = *event;
        // Send voice session begin request up to the BLE network
        if (CTRLM_HAL_RESULT_SUCCESS != g_ctrlm_hal_ble->main_init.ind_keypress(g_ctrlm_hal_ble->main_init.network_id, &req_params)) {
            LOG_ERROR("%s: indicate key press to network FAILED\n", __FUNCTION__);
        }
    }
    return 0;
}

void keyMonitorThread_ReadMessageQueue()
{
    gpointer msg = g_async_queue_try_pop(g_ctrlm_hal_ble->key_thread_queue);
    if (NULL != msg) {
        ctrlm_ble_key_queue_msg_header_t *hdr = (ctrlm_ble_key_queue_msg_header_t *) msg;
        switch(hdr->type) {
            case CTRLM_BLE_KEY_QUEUE_MSG_TYPE_TICKLE: {
                LOG_DEBUG("%s: message type CTRLM_BLE_KEY_QUEUE_MSG_TYPE_TICKLE\n", __FUNCTION__);
                ctrlm_ble_key_thread_monitor_msg_t *thread_monitor_msg = (ctrlm_ble_key_thread_monitor_msg_t *) msg;
                *thread_monitor_msg->response = CTRLM_HAL_THREAD_MONITOR_RESPONSE_ALIVE;
                break;
            }
            default: {
                LOG_DEBUG("%s: Unknown message type %u\n", __FUNCTION__, hdr->type);
                break;
            }
        }
        ctrlm_hal_ble_queue_msg_destroy(msg);
    }
}

void keyMonitorThread_FindRcuInputDevices(ctrlm_hal_ble_global_t *metadata, std::map <unsigned long long, rcu_metadata_t> &rcu_metadata, fd_set &rfds, int &nfds)
{   
    // Since the metadata map can be updated outside this thread, we need to lock with a mutex and then make a copy.
    // But only after getting the fd and device minor ID which need to be written to the metadata.
    // Otherwise, removing/adding an entry to the map while iterating will invalidate the iterator
    g_mutex_lock(&metadata->mutex_metadata);
    for (auto it = metadata->rcu_metadata.begin(); it != metadata->rcu_metadata.end(); it++) {
        if (it->second.input_device_fd < 0) {
            // We have an rcu in the metadata without an input device opened.
            // Loop through the linux input device nodes to find the one corresponding to this ieee_address
            int input_device_fd = ctrlm_hal_ble_OpenKeyInputDevice(it->first);
            if (input_device_fd < 0) {
                // LOG_DEBUG("%s: Did not find input device for RCU 0x%llX\n", __FUNCTION__, it->first);
            } else {
                // add this fd to select
                FD_SET(input_device_fd,  &rfds);
                nfds = MAX(nfds, input_device_fd);
                it->second.input_device_fd = input_device_fd;

                // Get the device minor ID, which gets reported up to the app as deviceid
                struct stat sb;
                if (-1 == fstat(it->second.input_device_fd, &sb)) {
                    int errsv = errno;
                    LOG_ERROR("%s: fstat() failed: error = <%d>, <%s>", __FUNCTION__, errsv, strerror(errsv));
                } else {
                    it->second.device_minor_id = minor(sb.st_rdev);
                    LOG_INFO("%s: it->second.device_minor_id = <%d>, reporting status change up to the network\n", __FUNCTION__, it->second.device_minor_id);
                    if (NULL != metadata->main_init.ind_status) {
                        // send deviceid up to the network
                        ctrlm_hal_ble_RcuStatusData_t rcu_status;
                        rcu_status.property_updated = CTRLM_HAL_BLE_PROPERTY_DEVICE_ID;
                        rcu_status.rcu_data.ieee_address = it->first;
                        rcu_status.rcu_data.device_id = it->second.device_minor_id;

                        if (CTRLM_HAL_RESULT_SUCCESS != metadata->main_init.ind_status(metadata->main_init.network_id, &rcu_status)) {
                            LOG_ERROR("%s: RCU status indication failed\n", __FUNCTION__);
                        }
                    }
                }
            }
        } else {
            FD_SET(it->second.input_device_fd,  &rfds);
            nfds = MAX(nfds, it->second.input_device_fd);
        }
    }
    rcu_metadata = metadata->rcu_metadata;
    g_mutex_unlock(&metadata->mutex_metadata);
}

void *ctrlm_hal_ble_KeyMonitorThread(void *data)
{
    LOG_INFO("%s: Enter...\n", __FUNCTION__);

    ctrlm_hal_ble_global_t *metadata = (ctrlm_hal_ble_global_t *)data;
    std::map <unsigned long long, rcu_metadata_t> rcu_metadata;

    struct input_event event;
    struct timeval tv;

    do {
        fd_set rfds;
        FD_ZERO(&rfds);
        int nfds = -1;

        keyMonitorThread_ReadMessageQueue();
        keyMonitorThread_FindRcuInputDevices(metadata, rcu_metadata, rfds, nfds);
        
        //nfds is the highest-numbered file descriptor in the set, plus 1.
        nfds++;
        if (nfds <= 0) {
            // This means no file descriptors were added, sleep and start loop over
            g_usleep (G_USEC_PER_SEC * 0.5);
            continue;
        }

        // Needs to be reinitialized before each call to select() because select() will update the timeout argument to indicate how much time was left
        tv.tv_usec = 200 * (1000);
        tv.tv_sec = 0;

        int ret = select(nfds, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            int errsv = errno;
            LOG_DEBUG("%s: select() failed: error = <%d>, <%s>\n", __FUNCTION__, errsv, strerror(errsv));
            g_usleep (G_USEC_PER_SEC * 0.5);
            continue;
        }
        // Need to loop again to find which fd has data to read
        for (auto it = rcu_metadata.begin(); it != rcu_metadata.end(); it++) {
            if (it->second.input_device_fd >= 0) {
                if (FD_ISSET(it->second.input_device_fd, &rfds)) {
                    memset((void*) &event, 0, sizeof(event));
                    ret = read(it->second.input_device_fd, (void*)&event, sizeof(event));
                    if (ret < 0) {
                        // int errsv = errno;
                        // LOG_ERROR("%s: Error reading event: error = <%d>, <%s>\n", __FUNCTION__, errsv, strerror(errsv));
                    } else {
                        ctrlm_hal_ble_HandleKeypress(&event, it->first);
                    }
                }
            }
        }
    } while ( metadata->is_running() );

    if (metadata->is_running()) {
        // If the thread should still be running and we broke out of the loop, then an error occurred.
        LOG_ERROR("%s: key monitor thread broke out of loop without being told, an error occurred...\n", __FUNCTION__);
    } else {
        LOG_INFO("%s: thread told to exit...\n", __FUNCTION__);
    }

    LOG_INFO("%s: Exit...\n", __FUNCTION__);
    return NULL;
}

// -------------------------------------------------------------------------------------------------------------
// END - Key Monitor Thread
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
