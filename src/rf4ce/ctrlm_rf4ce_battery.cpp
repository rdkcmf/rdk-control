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
#include <time.h>
#include <glib.h>
#include <string.h>
#include <vector>
#include "../ctrlm.h"
#include "../ctrlm_utils.h"
#include "../ctrlm_rcu.h"
#include "ctrlm_rf4ce_network.h"
#include "../ctrlm_voice.h"
#include "../ctrlm_device_update.h"
#include "../ctrlm_database.h"

using namespace std;

#define V2P_INDEX_XR2_LINEAR_PERCENT_ZERO (128)
#define V2P_INDEX_XR2_LINEAR_PERCENT_100  (191)

const char v2p_table_xr2_linear[64] = {
     1, // Index 128 Voltage 2.01
     2, // Index 129 Voltage 2.02
     4, // Index 130 Voltage 2.04
     5, // Index 131 Voltage 2.05
     7, // Index 132 Voltage 2.07
     9, // Index 133 Voltage 2.09
    10, // Index 134 Voltage 2.10
    12, // Index 135 Voltage 2.12
    13, // Index 136 Voltage 2.13
    15, // Index 137 Voltage 2.15
    16, // Index 138 Voltage 2.16
    18, // Index 139 Voltage 2.18
    20, // Index 140 Voltage 2.20
    21, // Index 141 Voltage 2.21
    23, // Index 142 Voltage 2.23
    24, // Index 143 Voltage 2.24
    26, // Index 144 Voltage 2.26
    27, // Index 145 Voltage 2.27
    29, // Index 146 Voltage 2.29
    31, // Index 147 Voltage 2.31
    32, // Index 148 Voltage 2.32
    34, // Index 149 Voltage 2.34
    35, // Index 150 Voltage 2.35
    37, // Index 151 Voltage 2.37
    38, // Index 152 Voltage 2.38
    40, // Index 153 Voltage 2.40
    42, // Index 154 Voltage 2.42
    43, // Index 155 Voltage 2.43
    45, // Index 156 Voltage 2.45
    46, // Index 157 Voltage 2.46
    48, // Index 158 Voltage 2.48
    49, // Index 159 Voltage 2.49
    51, // Index 160 Voltage 2.51
    53, // Index 161 Voltage 2.53
    54, // Index 162 Voltage 2.54
    56, // Index 163 Voltage 2.56
    57, // Index 164 Voltage 2.57
    59, // Index 165 Voltage 2.59
    60, // Index 166 Voltage 2.60
    62, // Index 167 Voltage 2.62
    64, // Index 168 Voltage 2.64
    65, // Index 169 Voltage 2.65
    67, // Index 170 Voltage 2.67
    68, // Index 171 Voltage 2.68
    70, // Index 172 Voltage 2.70
    71, // Index 173 Voltage 2.71
    73, // Index 174 Voltage 2.73
    75, // Index 175 Voltage 2.75
    76, // Index 176 Voltage 2.76
    78, // Index 177 Voltage 2.78
    79, // Index 178 Voltage 2.79
    81, // Index 179 Voltage 2.81
    82, // Index 180 Voltage 2.82
    84, // Index 181 Voltage 2.84
    85, // Index 182 Voltage 2.85
    87, // Index 183 Voltage 2.87
    89, // Index 184 Voltage 2.89
    90, // Index 185 Voltage 2.90
    92, // Index 186 Voltage 2.92
    93, // Index 187 Voltage 2.93
    95, // Index 188 Voltage 2.95
    96, // Index 189 Voltage 2.96
    98, // Index 190 Voltage 2.98
   100, // Index 191 Voltage 3.00
};
#define V2P_INDEX_XR11_REG_PERCENT_ZERO (147)
#define V2P_INDEX_XR11_REG_PERCENT_100  (191)

const char v2p_table_xr11_reg[45] = {
     3, // Index 147 Voltage 2.31
     6, // Index 148 Voltage 2.32
    10, // Index 149 Voltage 2.34
    13, // Index 150 Voltage 2.35
    17, // Index 151 Voltage 2.37
    21, // Index 152 Voltage 2.38
    24, // Index 153 Voltage 2.40
    28, // Index 154 Voltage 2.42
    32, // Index 155 Voltage 2.43
    36, // Index 156 Voltage 2.45
    40, // Index 157 Voltage 2.46
    43, // Index 158 Voltage 2.48
    47, // Index 159 Voltage 2.49
    51, // Index 160 Voltage 2.51
    54, // Index 161 Voltage 2.53
    58, // Index 162 Voltage 2.54
    61, // Index 163 Voltage 2.56
    64, // Index 164 Voltage 2.57
    67, // Index 165 Voltage 2.59
    70, // Index 166 Voltage 2.60
    72, // Index 167 Voltage 2.62
    75, // Index 168 Voltage 2.64
    77, // Index 169 Voltage 2.65
    79, // Index 170 Voltage 2.67
    81, // Index 171 Voltage 2.68
    83, // Index 172 Voltage 2.70
    85, // Index 173 Voltage 2.71
    86, // Index 174 Voltage 2.73
    88, // Index 175 Voltage 2.75
    89, // Index 176 Voltage 2.76
    90, // Index 177 Voltage 2.78
    91, // Index 178 Voltage 2.79
    92, // Index 179 Voltage 2.81
    93, // Index 180 Voltage 2.82
    94, // Index 181 Voltage 2.84
    94, // Index 182 Voltage 2.85
    95, // Index 183 Voltage 2.87
    95, // Index 184 Voltage 2.89
    96, // Index 185 Voltage 2.90
    96, // Index 186 Voltage 2.92
    97, // Index 187 Voltage 2.93
    97, // Index 188 Voltage 2.95
    98, // Index 189 Voltage 2.96
    99, // Index 190 Voltage 2.98
   100, // Index 191 Voltage 3.00
};
#define V2P_INDEX_XR11_FIX_PERCENT_ZERO (137)
#define V2P_INDEX_XR11_FIX_PERCENT_100  (191)

const char v2p_table_xr11_fix[55] = {
     1, // Index 137 Voltage 2.15
     2, // Index 138 Voltage 2.16
     3, // Index 139 Voltage 2.18
     4, // Index 140 Voltage 2.20
     6, // Index 141 Voltage 2.21
     8, // Index 142 Voltage 2.23
    10, // Index 143 Voltage 2.24
    12, // Index 144 Voltage 2.26
    15, // Index 145 Voltage 2.27
    17, // Index 146 Voltage 2.29
    20, // Index 147 Voltage 2.31
    23, // Index 148 Voltage 2.32
    26, // Index 149 Voltage 2.34
    29, // Index 150 Voltage 2.35
    32, // Index 151 Voltage 2.37
    35, // Index 152 Voltage 2.38
    38, // Index 153 Voltage 2.40
    41, // Index 154 Voltage 2.42
    44, // Index 155 Voltage 2.43
    47, // Index 156 Voltage 2.45
    50, // Index 157 Voltage 2.46
    53, // Index 158 Voltage 2.48
    56, // Index 159 Voltage 2.49
    59, // Index 160 Voltage 2.51
    61, // Index 161 Voltage 2.53
    64, // Index 162 Voltage 2.54
    67, // Index 163 Voltage 2.56
    69, // Index 164 Voltage 2.57
    72, // Index 165 Voltage 2.59
    74, // Index 166 Voltage 2.60
    76, // Index 167 Voltage 2.62
    78, // Index 168 Voltage 2.64
    80, // Index 169 Voltage 2.65
    82, // Index 170 Voltage 2.67
    84, // Index 171 Voltage 2.68
    85, // Index 172 Voltage 2.70
    87, // Index 173 Voltage 2.71
    88, // Index 174 Voltage 2.73
    90, // Index 175 Voltage 2.75
    91, // Index 176 Voltage 2.76
    92, // Index 177 Voltage 2.78
    93, // Index 178 Voltage 2.79
    94, // Index 179 Voltage 2.81
    94, // Index 180 Voltage 2.82
    95, // Index 181 Voltage 2.84
    96, // Index 182 Voltage 2.85
    96, // Index 183 Voltage 2.87
    97, // Index 184 Voltage 2.89
    97, // Index 185 Voltage 2.90
    98, // Index 186 Voltage 2.92
    98, // Index 187 Voltage 2.93
    98, // Index 188 Voltage 2.95
    99, // Index 189 Voltage 2.96
    99, // Index 190 Voltage 2.98
   100, // Index 191 Voltage 3.00
};

guchar ctrlm_obj_controller_rf4ce_t::battery_level_percent(unsigned char voltage_loaded) {
   switch(controller_type_) {
      case RF4CE_CONTROLLER_TYPE_XR11: {
         version_hardware_t xr11_version_min;
         xr11_version_min.manufacturer = 2;
         xr11_version_min.model        = 2;
         xr11_version_min.hw_revision  = 2;
         xr11_version_min.lot_code     = 0;
         if(version_compare(version_hardware_, xr11_version_min) >= 0) {
            if(voltage_loaded > V2P_INDEX_XR11_FIX_PERCENT_100) {
               return(100);
            } else if(voltage_loaded < V2P_INDEX_XR11_FIX_PERCENT_ZERO) {
               return(0);
            } else {
               return(v2p_table_xr11_fix[voltage_loaded - V2P_INDEX_XR11_FIX_PERCENT_ZERO]);
            }
         } else {
            if(voltage_loaded > V2P_INDEX_XR11_REG_PERCENT_100) {
               return(100);
            } else if(voltage_loaded < V2P_INDEX_XR11_REG_PERCENT_ZERO) {
               return(0);
            } else {
               return(v2p_table_xr11_reg[voltage_loaded - V2P_INDEX_XR11_REG_PERCENT_ZERO]);
            }
         }
         break;
      }
      case RF4CE_CONTROLLER_TYPE_XR15:
      case RF4CE_CONTROLLER_TYPE_XR15V2: 
      case RF4CE_CONTROLLER_TYPE_XR16: {
         //XR15 and XR16 uses the same table as XR11 FIX
         if(voltage_loaded > V2P_INDEX_XR11_FIX_PERCENT_100) {
            return(100);
         } else if(voltage_loaded < V2P_INDEX_XR11_FIX_PERCENT_ZERO) {
            return(0);
         } else {
            return(v2p_table_xr11_fix[voltage_loaded - V2P_INDEX_XR11_FIX_PERCENT_ZERO]);
         }
         break;
      }
      default: {
         if(voltage_loaded > V2P_INDEX_XR2_LINEAR_PERCENT_100) {
            return(100);
         } else if(voltage_loaded < V2P_INDEX_XR2_LINEAR_PERCENT_ZERO) {
            return(0);
         } else {
            return(v2p_table_xr2_linear[voltage_loaded - V2P_INDEX_XR2_LINEAR_PERCENT_ZERO]);
         }
         break;
      }
   }
}
