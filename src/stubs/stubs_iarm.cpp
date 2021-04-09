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
#include <glib.h>
#include "libIBus.h"

IARM_Result_t IARM_Bus_Init(const char *name) {
   printf("%s: STUB Name %s\n", __FUNCTION__, name);
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t IARM_Bus_Term(void) {
   printf("%s: STUB\n", __FUNCTION__);
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t IARM_Bus_Connect(void) {
   printf("%s: STUB\n", __FUNCTION__);
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t IARM_Bus_Disconnect(void) {
   printf("%s: STUB\n", __FUNCTION__);
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t IARM_Bus_GetContext(void **context) {
   printf("%s: STUB\n", __FUNCTION__);
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t IARM_Bus_BroadcastEvent(const char *ownerName, IARM_EventId_t eventId, void *data, size_t len) {
   printf("%s: STUB Name %s Event Id %d\n", __FUNCTION__, ownerName, eventId);
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t IARM_Bus_IsConnected(const char *memberName, int *isRegistered) {
   printf("%s: STUB Name %s\n", __FUNCTION__, memberName);
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t IARM_Bus_RegisterEventHandler(const char *ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
   printf("%s: STUB Name %s Event Id %d\n", __FUNCTION__, ownerName, eventId);
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t IARM_Bus_UnRegisterEventHandler(const char *ownerName, IARM_EventId_t eventId) {
   printf("%s: STUB Name %s Event Id %d\n", __FUNCTION__, ownerName, eventId);
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t IARM_Bus_RegisterCall(const char *methodName, IARM_BusCall_t handler) {
   printf("%s: STUB Method %s\n", __FUNCTION__, methodName);
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t IARM_Bus_Call(const char *ownerName,  const char *methodName, void *arg, size_t argLen) {
   printf("%s: STUB Name %s Method %s\n", __FUNCTION__, ownerName, methodName);
   return(IARM_RESULT_SUCCESS);
}

IARM_Result_t IARM_Bus_RegisterEvent(IARM_EventId_t maxEventId) {
   printf("%s: STUB Event Id %d\n", __func__, maxEventId);
   return(IARM_RESULT_SUCCESS);
}
