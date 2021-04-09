
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
#ifdef USE_VOICE_SDK
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
#else

#define LOG_PREFIX           "CTRLM: "
#define LOG_PREFIX_FATAL     "CTRLM:FATAL: "
#define LOG_PREFIX_ERROR     "CTRLM:ERROR: "
#define LOG_PREFIX_WARN      "CTRLM:WARN: "
#define LOG_PREFIX_BLACKOUT  "BLACKOUT: "
#ifdef RDK_LOGGER_ENABLED
   #include "rdk_debug.h"
   #include "iarmUtil.h"

   extern int b_rdk_logger_enabled;

   #define LOG(...) if(b_rdk_logger_enabled) {\
                       RDK_LOG(RDK_LOG_DEBUG, "LOG.RDK.CTRLM", __VA_ARGS__);\
                    } else {\
                       char log_buffer[13]; ctrlm_get_log_time(log_buffer); printf("%s " FORMAT, log_buffer, ##__VA_ARGS__); \
                    }
   #define LOG_FLUSH() fflush(stdout)
#else
   #define LOG(FORMAT, ...); {char log_buffer[13]; ctrlm_get_log_time(log_buffer); printf("%s " FORMAT, log_buffer, ##__VA_ARGS__);}
   #define LOG_FLUSH() fflush(stdout)
#endif

#define LOG_LEVEL_NONE  (6)
#define LOG_LEVEL_FATAL (5)
#define LOG_LEVEL_ERROR (4)
#define LOG_LEVEL_WARN  (3)
#define LOG_LEVEL_INFO  (2)
#define LOG_LEVEL_DEBUG (1)
#define LOG_LEVEL_ALL   (0)

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#if (LOG_LEVEL >= LOG_LEVEL_NONE)
   #define LOG_FATAL(...)
   #define LOG_ERROR(...)
   #define LOG_WARN(...)
   #define LOG_INFO(...)
   #define LOG_DEBUG(...)
#elif (LOG_LEVEL >= LOG_LEVEL_FATAL)
   #define LOG_FATAL(...) LOG(LOG_PREFIX_FATAL __VA_ARGS__); LOG_FLUSH()
   #define LOG_ERROR(...)
   #define LOG_WARN(...)
   #define LOG_INFO(...)
   #define LOG_DEBUG(...)
#elif (LOG_LEVEL >= LOG_LEVEL_ERROR)
   #define LOG_FATAL(...) LOG(LOG_PREFIX_FATAL __VA_ARGS__); LOG_FLUSH()
   #define LOG_ERROR(...) LOG(LOG_PREFIX_ERROR __VA_ARGS__)
   #define LOG_WARN(...)  LOG(LOG_PREFIX_WARN  __VA_ARGS__)
   #define LOG_INFO(...)
   #define LOG_DEBUG(...)
#elif (LOG_LEVEL >= LOG_LEVEL_WARN)
   #define LOG_FATAL(...) LOG(LOG_PREFIX_FATAL __VA_ARGS__); LOG_FLUSH()
   #define LOG_ERROR(...) LOG(LOG_PREFIX_ERROR __VA_ARGS__)
   #define LOG_WARN(...)  LOG(LOG_PREFIX_WARN  __VA_ARGS__)
   #define LOG_INFO(...)
   #define LOG_DEBUG(...)
#elif (LOG_LEVEL >= LOG_LEVEL_INFO)
   #define LOG_FATAL(...) LOG(LOG_PREFIX_FATAL __VA_ARGS__); LOG_FLUSH()
   #define LOG_ERROR(...) LOG(LOG_PREFIX_ERROR __VA_ARGS__)
   #define LOG_WARN(...)  LOG(LOG_PREFIX_WARN  __VA_ARGS__)
   #define LOG_INFO(...)  LOG(LOG_PREFIX       __VA_ARGS__)
   #define LOG_DEBUG(...)
#else
   #define LOG_FATAL(...) LOG(LOG_PREFIX_FATAL __VA_ARGS__); LOG_FLUSH()
   #define LOG_ERROR(...) LOG(LOG_PREFIX_ERROR __VA_ARGS__)
   #define LOG_WARN(...)  LOG(LOG_PREFIX_WARN  __VA_ARGS__)
   #define LOG_INFO(...)  LOG(LOG_PREFIX       __VA_ARGS__)
   #define LOG_DEBUG(...) LOG(LOG_PREFIX       __VA_ARGS__)
#endif

#define LOG_BLACKOUT(...) LOG_WARN(LOG_PREFIX_BLACKOUT __VA_ARGS__)

#ifdef __cplusplus
extern "C"
{
#endif

void ctrlm_get_log_time(char *log_buffer);

#ifdef __cplusplus
}
#endif

#endif
#endif
