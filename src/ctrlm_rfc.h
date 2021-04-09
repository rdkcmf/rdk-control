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
#ifndef _CTRLM_RFC_H_
#define _CTRLM_RFC_H_

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <jansson.h>
#include "ctrlm.h"
// End Includes

// Structs
typedef struct {
   gboolean enabled;
   guint    pairing_fail_threshold;
   guint    blackout_reboot_threshold;
   guint    blackout_time;
} ctrlm_rfc_rf4ce_blackout_t;

typedef struct {
   gboolean  enabled;
   json_t   *revoke_list;
} ctrlm_rfc_ip_t;

typedef struct {
   gboolean  stored;
   guint     attempts;
   union {
      ctrlm_rfc_rf4ce_blackout_t rf4ce_blackout;
      ctrlm_rfc_ip_t             ip;
   } rfc;
} ctrlm_rfc_t;
// End Structs

// Function Declarations
// Call init on boot, this allocates the memory for RFC structs.
gboolean ctrlm_rfc_init();
// Returns reference to RF4CE RFC Blackout struct. Do not free.
gboolean ctrlm_rfc_rf4ce_blackout_get(ctrlm_rfc_rf4ce_blackout_t **rf4ce_blackout);
// Returns reference to IP RFC struct. Do not free.
gboolean ctrlm_rfc_ip_get(ctrlm_rfc_ip_t **ip);
// Call on shutdown, this frees the RFC structs.
void     ctrlm_rfc_destroy();
// End Function Declarations


#endif
