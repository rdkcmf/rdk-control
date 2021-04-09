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
/*
/ This file is for voice types that are shared between voice components (object, IPC, etc)
*/
#ifndef __CTRLM_VOICE_TYPES_H__
#define __CTRLM_VOICE_TYPES_H__

typedef enum {
    CTRLM_VOICE_DEVICE_PTT,
    CTRLM_VOICE_DEVICE_FF,
    CTRLM_VOICE_DEVICE_MICROPHONE,
    CTRLM_VOICE_DEVICE_INVALID,
} ctrlm_voice_device_t;

#endif
