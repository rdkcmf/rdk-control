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
#include <string.h>
#include "ctrlm_hal_rf4ce.h"
#include "../ctrlm_log.h"

static ctrlm_hal_rf4ce_main_init_t main_init;

ctrlm_hal_result_t ctrlm_hal_req_term(void)
{
   LOG_INFO("%s: STUB\n", __FUNCTION__);
   return CTRLM_HAL_RESULT_ERROR_NOT_IMPLEMENTED;
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_req_pair(void){
   LOG_INFO("%s: STUB\n", __FUNCTION__);
   return CTRLM_HAL_RESULT_ERROR_NOT_IMPLEMENTED;
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_req_unpair(ctrlm_controller_id_t controller_id){
   LOG_INFO("%s: STUB, controller id: %u \n", __FUNCTION__,(unsigned)controller_id);
   return CTRLM_HAL_RESULT_ERROR_NOT_IMPLEMENTED;
}

ctrlm_hal_result_t ctrlm_hal_req_property_get(ctrlm_hal_network_property_t property, void **value){
   LOG_INFO("%s: STUB, property: %s \n", __FUNCTION__,ctrlm_hal_network_property_str(property));
   return CTRLM_HAL_RESULT_ERROR_NOT_IMPLEMENTED;
}

ctrlm_hal_result_t ctrlm_hal_req_property_set(ctrlm_hal_network_property_t property, void *value){
   LOG_INFO("%s: STUB, property: %s \n", __FUNCTION__,ctrlm_hal_network_property_str(property));
   if (value == 0) {
      LOG_ERROR("%s: STUB, Property value is 0\n", __FUNCTION__);
      return CTRLM_HAL_RESULT_ERROR_INVALID_PARAMS;
   }
   if (property == CTRLM_HAL_NETWORK_PROPERTY_THREAD_MONITOR) {
      LOG_INFO("%s: STUB, ctrlm checks whether we are still alive\n", __FUNCTION__);
      *((ctrlm_hal_network_property_thread_monitor_t *)value)->response = CTRLM_HAL_THREAD_MONITOR_RESPONSE_ALIVE;
      return CTRLM_HAL_RESULT_SUCCESS;
   }
   return CTRLM_HAL_RESULT_ERROR_NOT_IMPLEMENTED;
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_req_data(ctrlm_hal_rf4ce_req_data_params_t params){
   LOG_INFO("%s: STUB \n", __FUNCTION__);
   return CTRLM_HAL_RESULT_ERROR_NOT_IMPLEMENTED;
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_rib_data_import(ctrlm_hal_rf4ce_rib_data_import_params_t *params){
   LOG_INFO("%s: STUB \n", __FUNCTION__);
   return CTRLM_HAL_RESULT_ERROR_NOT_IMPLEMENTED;
}

ctrlm_hal_result_t ctrlm_hal_rf4ce_rib_data_export(ctrlm_hal_rf4ce_rib_data_export_params_t *params){
   LOG_INFO("%s: STUB \n", __FUNCTION__);
   return CTRLM_HAL_RESULT_ERROR_NOT_IMPLEMENTED;
}

void *ctrlm_hal_rf4ce_main(ctrlm_hal_rf4ce_main_init_t *main_init_) {

   LOG_INFO("%s: STUB, Network id: %u\n", __FUNCTION__, (unsigned)main_init.network_id);

   memcpy(&main_init, main_init_, sizeof (ctrlm_hal_rf4ce_main_init_t));

   if (main_init.cfm_init != 0) {
      ctrlm_hal_rf4ce_cfm_init_params_t params;
      params.result = CTRLM_HAL_RESULT_SUCCESS;
      strcpy(params.version,"0.0.0.0");
      strcpy(params.chipset,"generic");
      params.pan_id = 0;
      params.ieee_address = 0;
      params.short_address = 0;
      params.term = ctrlm_hal_req_term;
      params.pair = ctrlm_hal_rf4ce_req_pair;
      params.unpair = ctrlm_hal_rf4ce_req_unpair;
      params.property_get = ctrlm_hal_req_property_get;
      params.property_set = ctrlm_hal_req_property_set;
      params.data = ctrlm_hal_rf4ce_req_data;
      params.rib_data_import = ctrlm_hal_rf4ce_rib_data_import;
      params.rib_data_export = ctrlm_hal_rf4ce_rib_data_export;
      params.nvm_backup_data = 0;
      params.nvm_backup_len = 0;
      main_init.cfm_init(main_init.network_id,params);
   }
   return NULL;
}
