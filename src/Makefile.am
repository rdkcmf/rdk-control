##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

AM_CPPFLAGS = -I$(top_srcdir)/                     \
              -I$(top_srcdir)/include/             \
              -I$(top_srcdir)/src/                 \
              -I$(top_srcdir)/src/irdb/            \
              -I$(top_srcdir)/src/irdb/ipc/        \
              -I$(top_srcdir)/src/voice/           \
              -I$(top_srcdir)/src/voice/endpoints/ \
              -I$(top_srcdir)/src/voice/ipc/       \
              -I$(top_srcdir)/src/thunder/         \
              -I$(top_srcdir)/src/thunder/plugins

AM_LDFLAGS = -larchive

bin_PROGRAMS       = controlMgr
controlMgr_SOURCES = ctrlm_main.cpp                           \
                     ctrlm_main_iarm.cpp                      \
                     ctrlm_voice.cpp                          \
                     ctrlm_voice_iarm.cpp                     \
                     ctrlm_voice_session.cpp                  \
                     ctrlm_utils.cpp                          \
                     ctrlm_network.cpp                        \
                     ctrlm_network_iarm.cpp                   \
                     ctrlm_controller.cpp                     \
                     ctrlm_rcu.cpp                            \
                     ctrlm_rcu_iarm.cpp                       \
                     ctrlm_database.cpp                       \
                     ctrlm_validation.cpp                     \
                     ctrlm_device_update.cpp                  \
                     ctrlm_device_update_iarm.cpp             \
                     ctrlm_recovery.cpp                       \
                     ctrlm_tr181.cpp                          \
                     ctrlm_rfc.cpp                            \
                     json_config.cpp                          \
                     irdb/ctrlm_irdb.cpp                      \
                     irdb/ctrlm_irdb_stub.cpp                 \
                     irdb/ipc/ctrlm_irdb_ipc_iarm_thunder.cpp 

if PLATFORM_STB
AM_CPPFLAGS += -DPLATFORM_STB
endif

if PLATFORM_TV
AM_CPPFLAGS += -DPLATFORM_TV
endif

if VOICE_SDK_ENABLED
AM_CPPFLAGS += -DUSE_VOICE_SDK

controlMgr_SOURCES  += voice/ctrlm_voice_obj.cpp                           \
                       voice/ctrlm_voice_obj_generic.cpp                   \
                       voice/ipc/ctrlm_voice_ipc_iarm_legacy.cpp           \
                       voice/ipc/ctrlm_voice_ipc_iarm_thunder.cpp          \
                       voice/ipc/ctrlm_voice_ipc_iarm_all.cpp              \
                       voice/endpoints/ctrlm_voice_endpoint.cpp            \
                       voice/endpoints/ctrlm_voice_endpoint_http.cpp       \
                       voice/endpoints/ctrlm_voice_endpoint_ws.cpp         \
                       voice/endpoints/ctrlm_voice_endpoint_ws_nextgen.cpp
endif

if SUPPORT_VOICE_DEST_ALSA
controlMgr_SOURCES  += voice/endpoints/ctrlm_voice_endpoint_sdt.cpp
endif

if RF4CE_ENABLED
AM_CPPFLAGS += -DCTRLM_NETWORK_RF4CE
AM_LDFLAGS  += $(CTRLM_HAL_RF4CE_LIBRARY)

controlMgr_SOURCES  += rf4ce/ctrlm_rf4ce_battery.cpp       \
                       rf4ce/ctrlm_rf4ce_controller.cpp    \
                       rf4ce/ctrlm_rf4ce_device_update.cpp \
                       rf4ce/ctrlm_rf4ce_discovery.cpp     \
                       rf4ce/ctrlm_rf4ce_indication.cpp    \
                       rf4ce/ctrlm_rf4ce_network.cpp       \
                       rf4ce/ctrlm_rf4ce_pairing.cpp       \
                       rf4ce/ctrlm_rf4ce_rcu.cpp           \
                       rf4ce/ctrlm_rf4ce_rib.cpp           \
                       rf4ce/ctrlm_rf4ce_utils.cpp         \
                       rf4ce/ctrlm_rf4ce_validation.cpp    \
                       rf4ce/ctrlm_rf4ce_ir_rf_db.cpp      \
                       rf4ce/ctrlm_rf4ce_ir_rf_db_entry.cpp
endif

if BLE_ENABLED
AM_CPPFLAGS += -DCTRLM_NETWORK_BLE

controlMgr_SOURCES  += ble/ctrlm_ble_controller.cpp \
                       ble/ctrlm_ble_iarm.cpp       \
                       ble/ctrlm_ble_network.cpp    \
                       ble/ctrlm_ble_utils.cpp      \
                       ble/ctrlm_hal_ble.cpp
endif

if IP_ENABLED
AM_CPPFLAGS += -DCTRLM_NETWORK_IP -DCTRLM_IP_HAL_LOG_ENABLED
endif

if THUNDER_ENABLED
AM_CPPFLAGS += -DCTRLM_THUNDER
controlMgr_SOURCES +=  thunder/ctrlm_thunder_plugin.cpp    \
                       thunder/ctrlm_thunder_controller.cpp \
                       thunder/plugins/ctrlm_thunder_plugin_display_settings.cpp \
                       thunder/plugins/ctrlm_thunder_plugin_hdmi_input.cpp \
                       thunder/plugins/ctrlm_thunder_plugin_cec_sink.cpp
endif

if VOICE_KEYWORD_BEEP
controlMgr_SOURCES +=  thunder/ctrlm_thunder_plugin_system_audio_player.cpp
AM_CPPFLAGS += -DBEEP_ON_KWD_ENABLED
endif

if CPC_ENABLED
AM_CPPFLAGS += -DCPC_ENABLED
AM_CPPFLAGS += -I$(top_srcdir)/cpc/plugins
AM_LDFLAGS += -lsecure_wrapper

controlMgr_SOURCES += ../cpc/ctrlm_rf4ce_voice_packet_analysis.cpp \
                      ../cpc/ctrlm_vendor_network_factory.cpp      \
                      ../cpc/irdb/ctrlm_irdb_factory.cpp

if IP_ENABLED
controlMgr_SOURCES += ../cpc/ip/ctrlm_hal_ip.cpp                   \
                      ../cpc/ip/ctrlm_ip_json.cpp                  \
                      ../cpc/ip/ctrlm_hal_ip_log.cpp               \
                      ../cpc/ip/ctrlm_ip_network.cpp               \
                      ../cpc/ip/ctrlm_ip_controller.cpp            \
                      ../cpc/ip/ctrlm_ip_util.cpp
endif

if THUNDER_ENABLED
controlMgr_SOURCES +=  ../cpc/plugins/ctrlm_thunder_plugin_authservice.cpp \
                       ../cpc/plugins/ctrlm_thunder_plugin_rams.cpp \
                       ../cpc/plugins/ctrlm_thunder_plugin_remote_control.cpp
endif

if ASB_ENABLED
AM_CPPFLAGS += -DASB

controlMgr_SOURCES += ../cpc/asb/advanced_secure_binding.cpp
endif

if AUTH_ENABLED
AM_CPPFLAGS += -DAUTH_ENABLED -I$(top_srcdir)/cpc/auth/

controlMgr_SOURCES += ../cpc/auth/ctrlm_auth.cpp

if AUTH_THUNDER
AM_CPPFLAGS += -DAUTH_THUNDER
controlMgr_SOURCES += ../cpc/auth/ctrlm_auth_thunder.cpp
else
controlMgr_SOURCES += ../cpc/auth/ctrlm_auth_legacy.cpp
endif

if AUTH_RECEIVER_ID
AM_CPPFLAGS += -DAUTH_RECEIVER_ID
endif

if AUTH_DEVICE_ID
AM_CPPFLAGS += -DAUTH_DEVICE_ID
endif

if AUTH_ACCOUNT_ID
AM_CPPFLAGS += -DAUTH_ACCOUNT_ID
endif

if AUTH_PARTNER_ID
AM_CPPFLAGS += -DAUTH_PARTNER_ID
endif

if AUTH_EXPERIENCE
AM_CPPFLAGS += -DAUTH_EXPERIENCE
endif

if AUTH_SAT_TOKEN
AM_CPPFLAGS += -DAUTH_SAT_TOKEN
endif
endif

if IRDB_UEI_ENABLED
AM_CPPFLAGS += -DIRDB_UEI
controlMgr_SOURCES += ../cpc/irdb/ctrlm_irdb_uei.cpp
endif

if IRDB_RUWIDO_ENABLED
AM_CPPFLAGS += -DIRDB_RUWIDO
controlMgr_SOURCES += ../cpc/irdb/ctrlm_irdb_ruwido.cpp
endif

else

controlMgr_SOURCES += irdb/ctrlm_irdb_factory.cpp           \
                      stubs/stubs_voice_packet_analysis.cpp

endif

if IRDB_MODE_HYBRID
AM_CPPFLAGS += -DIRDB_MODE_HYBRID
endif

if IRDB_MODE_OFFLINE
AM_CPPFLAGS += -DIRDB_MODE_OFFLINE
endif

if IRDB_MODE_ONLINE
AM_CPPFLAGS += -DIRDB_MODE_ONLINE
endif

if VOICE_NEXTGEN_MAC
AM_CPPFLAGS += -DVOICE_NEXTGEN_MAC
endif

if BREAKPAD_ENABLED
AM_CPPFLAGS += -DBREAKPAD_SUPPORT

AM_LDFLAGS += -lbreakpad_client
endif

if SUPPORT_VOICE_DEST_ALSA
AM_CPPFLAGS += -DSUPPORT_VOICE_DEST_ALSA
AM_LDFLAGS  += -lvirtualmic
endif

CTRLM_CONFIG_JSON        =ctrlm_config.json
CTRLM_CONFIG_JSON_DEF    =ctrlm_config_default.json
CTRLM_CONFIG_JSON_TMPL   =ctrlm_config.json.template
CTRLM_CONFIG_H           =ctrlm_config.h
CTRLM_VERSION_H          =ctrlm_version.h

BUILT_SOURCES = $(CTRLM_VERSION_H) $(CTRLM_CONFIG_H) $(CTRLM_CONFIG_JSON_TMPL) $(CTRLM_CONFIG_JSON)
CLEANFILES    = $(CTRLM_VERSION_H) $(CTRLM_CONFIG_H) $(CTRLM_CONFIG_JSON_TMPL) $(CTRLM_CONFIG_JSON)

dist_sysconf_DATA = $(CTRLM_CONFIG_JSON_TMPL)

$(CTRLM_VERSION_H):
	echo "#define CTRLM_MAIN_VER_MAJOR \"1\"" >$@
	echo "#define CTRLM_MAIN_VER_MINOR \"`git -C ${srcdir} rev-list --count 4b825dc642cb6eb9a060e54bf8d69288fbee4904..HEAD``git -C ${srcdir} diff --quiet || echo \"++\"`\"" >>$@
	echo "#define CTRLM_MAIN_BRANCH    \"${GIT_BRANCH}\"" >>$@
	echo "#define CTRLM_MAIN_COMMIT_ID \"`git -C ${srcdir} log --format="%H" -n 1`\"" >>$@
	echo "#define CTRLM_MAIN_VERSION (CTRLM_MAIN_VER_MAJOR \".\" CTRLM_MAIN_VER_MINOR)" >>$@
if CPC_ENABLED
	echo "#define CTRLM_CPC_VER_MAJOR \"1\"" >>$@
	echo "#define CTRLM_CPC_VER_MINOR \"`git -C ${srcdir}/../cpc rev-list --count 4b825dc642cb6eb9a060e54bf8d69288fbee4904..HEAD``git -C ${srcdir}/../cpc diff --quiet || echo \"++\"`\"" >>$@
	echo "#define CTRLM_CPC_BRANCH    \"${GIT_BRANCH}\"" >>$@
	echo "#define CTRLM_CPC_COMMIT_ID \"`git -C ${srcdir}/../cpc log --format="%H" -n 1`\"" >>$@
	echo "#define CTRLM_CPC_VERSION (CTRLM_CPC_VER_MAJOR \".\" CTRLM_CPC_VER_MINOR)" >>$@
endif

$(CTRLM_CONFIG_JSON): $(CTRLM_CONFIG_JSON_DEF)
	python3 "${CTRLM_UTILS_JSON_COMBINE}" -i $< -a "$(CTRLM_CONFIG_JSON_VSDK):vsdk" -s "$(CTRLM_CONFIG_JSON_CPC_SUB)" -a "$(CTRLM_CONFIG_JSON_CPC_ADD)" -s "$(CTRLM_CONFIG_JSON_OEM_SUB)" -a "$(CTRLM_CONFIG_JSON_OEM_ADD)" -o $@

$(CTRLM_CONFIG_JSON_TMPL): $(CTRLM_CONFIG_JSON)
	cp $< $@

$(CTRLM_CONFIG_H): $(CTRLM_CONFIG_JSON_TMPL)
	python3 "${CTRLM_UTILS_JSON_TO_HEADER}" -i $< -o $@ -v "ctrlm_global,network_rf4ce,network_ip,network_ble,voice,device_update"
