
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
#ifndef _CTRLM_LOG_H_
#define _CTRLM_LOG_H_

#include <stdio.h>

#define XLOG_OMIT_FUNCTION
#ifdef ANSI_CODES_DISABLED
#define XLOG_OPTS_DEFAULT (XLOG_OPTS_DATE | XLOG_OPTS_TIME | XLOG_OPTS_MOD_NAME | XLOG_OPTS_LEVEL)
#else
#define XLOG_OPTS_DEFAULT (XLOG_OPTS_DATE | XLOG_OPTS_TIME | XLOG_OPTS_MOD_NAME | XLOG_OPTS_LEVEL | XLOG_OPTS_COLOR)
#endif
#include "rdkx_logger.h"
#define LOG_FATAL(...)    XLOGD_FATAL(__VA_ARGS__)
#define LOG_ERROR(...)    XLOGD_ERROR(__VA_ARGS__)
#define LOG_WARN(...)     XLOGD_WARN(__VA_ARGS__)
#define LOG_INFO(...)     XLOGD_INFO(__VA_ARGS__)
#define LOG_DEBUG(...)    XLOGD_DEBUG(__VA_ARGS__)
#define LOG_RAW(...)      XLOG_RAW(__VA_ARGS__)

#define LOG_PREFIX_BLACKOUT  "BLACKOUT: "
#define LOG_BLACKOUT(...) LOG_WARN(LOG_PREFIX_BLACKOUT __VA_ARGS__)

#endif
