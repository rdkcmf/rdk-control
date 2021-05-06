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

#ifndef __CTRLM_THUNDER_LOG_H__
#define __CTRLM_THUNDER_LOG_H__

// This file is here to make the transition to controlMgr logging easier when namespace issues are resolved.
#if 0
#include "ctrlm.h"
#else
#include <stdio.h>
#endif

#if 0
#define THUNDER_LOG_DEBUG(...)   LOG_DEBUG(__VA_ARGS__)
#define THUNDER_LOG_INFO(...)    LOG_INFO(__VA_ARGS__)
#define THUNDER_LOG_WARN(...)    LOG_WARN(__VA_ARGS__)
#define THUNDER_LOG_ERROR(...)   LOG_ERROR(__VA_ARGS__)
#define THUNDER_LOG_FATAL(...)   LOG_FATAL(__VA_ARGS__)
#define THUNDER_LOG_RAW(...)     LOG_RAW(__VA_ARGS__)
#else
#define THUNDER_LOG_DEBUG(...)   printf("THUNDER - DEBUG: " __VA_ARGS__)
#define THUNDER_LOG_INFO(...)    printf("THUNDER - INFO: " __VA_ARGS__)
#define THUNDER_LOG_WARN(...)    printf("THUNDER - WARN: " __VA_ARGS__)
#define THUNDER_LOG_ERROR(...)   printf("THUNDER - ERROR: " __VA_ARGS__)
#define THUNDER_LOG_FATAL(...)   printf("THUNDER - FATAL: " __VA_ARGS__)
#define THUNDER_LOG_RAW(...)     printf(__VA_ARGS__)
#endif

#endif