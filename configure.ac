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
AC_INIT([ctrlm-main], [1.0], [David_Wolaver@cable.comcast.com])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_PROG_AR
LT_INIT

AC_PROG_CXX

AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

AC_ARG_ENABLE([rdkxlogger],
[  --enable-rdkxlogger    Turn on RDKX logger support],
[case "${enableval}" in
  yes) rdkxlogger=true ;;
  no)  rdkxlogger=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-rdkxlogger]) ;;
esac],[rdkxlogger=false])
AM_CONDITIONAL([RDKX_LOGGER_ENABLED], [test x$rdkxlogger = xtrue])

AC_ARG_ENABLE([rf4ce],
[  --enable-rf4ce    Turn on RDKX logger support],
[case "${enableval}" in
  yes) rf4ce=true ;;
  no)  rf4ce=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-rf4ce]) ;;
esac],[rf4ce=false])
AM_CONDITIONAL([RF4CE_ENABLED], [test x$rf4ce = xtrue])

AC_ARG_ENABLE([xrsr_sdt],
[  --enable-xrsr_sdt    Turn on Support Voice Destination ALSA],
[case "${enableval}" in
  yes) xrsr_sdt=true ;;
  no)  xrsr_sdt=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-xrsr_sdt]) ;;
esac],[xrsr_sdt=false])
AM_CONDITIONAL([SUPPORT_VOICE_DEST_ALSA], [test x$xrsr_sdt = xtrue])

AC_ARG_ENABLE([ble],
[  --enable-ble    Turn on RDKX logger support],
[case "${enableval}" in
  yes) ble=true ;;
  no)  ble=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-ble]) ;;
esac],[ble=false])
AM_CONDITIONAL([BLE_ENABLED], [test x$ble = xtrue])

AC_ARG_ENABLE([ip],
[  --enable-ip    Turn on RDKX logger support],
[case "${enableval}" in
  yes) ip=true ;;
  no)  ip=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-ip]) ;;
esac],[ip=false])
AM_CONDITIONAL([IP_ENABLED], [test x$ip = xtrue])

AC_ARG_ENABLE([cpc],
[  --enable-cpc    Turn on comcast private component support],
[case "${enableval}" in
  yes) cpc=true ;;
  no)  cpc=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-cpc]) ;;
esac],[cpc=false])
AM_CONDITIONAL([CPC_ENABLED], [test x$cpc = xtrue])

AC_ARG_ENABLE([voicesdk],
[  --enable-voicesdk    Turn on voice SDK support],
[case "${enableval}" in
  yes) voicesdk=true ;;
  no)  voicesdk=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-voicesdk]) ;;
esac],[voicesdk=false])
AM_CONDITIONAL([VOICE_SDK_ENABLED], [test x$voicesdk = xtrue])

AC_ARG_ENABLE([asb],
[  --enable-asb    Turn on advanced secure binding support],
[case "${enableval}" in
  yes) asb=true ;;
  no)  asb=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-asb]) ;;
esac],[asb=false])
AM_CONDITIONAL([ASB_ENABLED], [test x$asb = xtrue])

AC_ARG_ENABLE([thunder],
[  --enable-thunder    Enable thunder support],
[thunder=true],
[thunder=false])
AM_CONDITIONAL([THUNDER_ENABLED], [test x$thunder = xtrue])

AC_ARG_ENABLE([auth],
[  --enable-auth    Turn on auth service support],
[case "${enableval}" in
  yes) auth=true ;;
  no)  auth=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-auth]) ;;
esac],[auth=false])
AM_CONDITIONAL([AUTH_ENABLED], [test x$auth = xtrue])

AC_ARG_ENABLE([auththunder],
[  --enable-auththunder    Enable thunder auth support],
[case "${enableval}" in
  yes) auththunder=true ;;
  no)  auththunder=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-auththunder]) ;;
esac],[auththunder=false])
AM_CONDITIONAL([AUTH_THUNDER], [test x$auththunder = xtrue])

AC_ARG_ENABLE([auth-receiver-id],
[  --enable-auth-receiver-id    Enable auth receiver id support],
[auth_receiver_id=true],
[auth_receiver_id=false])
AM_CONDITIONAL([AUTH_RECEIVER_ID], [test x$auth_receiver_id = xtrue])

AC_ARG_ENABLE([auth-device-id],
[  --enable-auth-device-id    Enable auth device id support],
[auth_device_id=true],
[auth_device_id=false])
AM_CONDITIONAL([AUTH_DEVICE_ID], [test x$auth_device_id = xtrue])

AC_ARG_ENABLE([auth-account-id],
[  --enable-auth-account-id    Enable auth account id support],
[auth_account_id=true],
[auth_account_id=false])
AM_CONDITIONAL([AUTH_ACCOUNT_ID], [test x$auth_account_id = xtrue])

AC_ARG_ENABLE([auth-partner-id],
[  --enable-auth-partner-id    Enable auth partner id support],
[auth_partner_id=true],
[auth_partner_id=false])
AM_CONDITIONAL([AUTH_PARTNER_ID], [test x$auth_partner_id = xtrue])

AC_ARG_ENABLE([auth-experience],
[  --enable-auth-experience    Enable auth experience support],
[auth_experience=true],
[auth_experience=false])
AM_CONDITIONAL([AUTH_EXPERIENCE], [test x$auth_experience = xtrue])

AC_ARG_ENABLE([auth-sat-token],
[  --enable-auth-sat-token    Enable auth sat token support],
[auth_sat_token=true],
[auth_sat_token=false])
AM_CONDITIONAL([AUTH_SAT_TOKEN], [test x$auth_sat_token = xtrue])

AC_ARG_ENABLE([voice-nextgen-mac],
[  --enable-voice-nextgen-mac    Enable mac address for voice nextgen api],
[voice_nextgen_mac=true],
[voice_nextgen_mac=false])
AM_CONDITIONAL([VOICE_NEXTGEN_MAC], [test x$voice_nextgen_mac = xtrue])

AC_ARG_ENABLE([voice-keyword-beep],
[  --enable-voice-keyword-beep    Enable audible feedback on keyword detect/verification],
[voice_keyword_beep=true],
[voice_keyword_beep=false])
AM_CONDITIONAL([VOICE_KEYWORD_BEEP], [test x$voice_keyword_beep = xtrue])

AC_ARG_ENABLE([irdb],
[  --enable-irdb    Turn on IRDB support],
[case "${enableval}" in
  uei)     irdb=uei ;;
  ruwido)  irdb=ruwido ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-irdb]) ;;
esac],[irdb=false])
AM_CONDITIONAL([IRDB_UEI_ENABLED],    [test x$irdb = xuei])
AM_CONDITIONAL([IRDB_RUWIDO_ENABLED], [test x$irdb = xruwido])

AC_ARG_ENABLE([irdb-mode],
[  --enable-irdb-mode    Configure IRDB mode],
[case "${enableval}" in
  hybrid)     irdb_mode=hybrid ;;
  offline)    irdb_mode=offline ;;
  online)     irdb_mode=online ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-irdb-mode]) ;;
esac],[irdb_mode=false])
AM_CONDITIONAL([IRDB_MODE_HYBRID],  [test x$irdb_mode = xhybrid])
AM_CONDITIONAL([IRDB_MODE_OFFLINE], [test x$irdb_mode = xoffline])
AM_CONDITIONAL([IRDB_MODE_ONLINE],  [test x$irdb_mode = xonline])

AC_ARG_ENABLE([platform],
[  --enable-platform    Turn on platform support],
[case "${enableval}" in
  stb) platform=stb ;;
  tv)  platform=tv ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-platform]) ;;
esac],[platform=false])
AM_CONDITIONAL([PLATFORM_STB], [test x$platform = xstb])
AM_CONDITIONAL([PLATFORM_TV],  [test x$platform = xtv])

AC_ARG_ENABLE([breakpad],
[  --enable-breakpad    Turn on breakpad support],
[case "${enableval}" in
  yes) breakpad=true ;;
  no)  breakpad=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-breakpad]) ;;
esac],[breakpad=false])
AM_CONDITIONAL([BREAKPAD_ENABLED], [test x$breakpad = xtrue])

AC_ARG_VAR(VSDK_UTILS_JSON_TO_HEADER, script to create header from json object)
AC_ARG_VAR(VSDK_UTILS_JSON_COMBINE,   script to combine multiple json files)

AC_ARG_VAR(CTRLM_CONFIG_JSON_VSDK, vsdk json configuration file)
AC_ARG_VAR(CTRLM_CONFIG_JSON_OEM_SUB, oem sub json configuration file)
AC_ARG_VAR(CTRLM_CONFIG_JSON_OEM_ADD, oem add json configuration file)
AC_ARG_VAR(CTRLM_CONFIG_JSON_CPC_SUB, cpc sub json configuration file)
AC_ARG_VAR(CTRLM_CONFIG_JSON_CPC_ADD, cpc add json configuration file)

AC_ARG_VAR(GIT_BRANCH, git branch name)

AC_ARG_VAR(CTRLM_HAL_RF4CE_LIBRARY, RF4CE HAL library)

AC_OUTPUT
