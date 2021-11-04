#include "ctrlm_thunder_helpers.h"
#include "ctrlm_thunder_log.h"
#include <iostream>
#include <stdio.h>
#include <vector>
#include <array>
#include <utility>

bool Thunder::Helpers::is_systemd_process_active(const char *process) {
    bool ret = false;
    std::string status_str = "systemctl status ";
    status_str += process;
    FILE *pSystem = popen(status_str.c_str(), "r"); // can't use v_secure_popen, as you can use a variable as argument for it.
    if(pSystem) {
        std::string pSystemOutput;
        std::array<char, 256> pSystemBuffer;

        while(fgets(pSystemBuffer.data(), 256, pSystem) != NULL) {
            pSystemOutput += pSystemBuffer.data();
        }
        pclose(pSystem);
        if(pSystemOutput.find("Active: active (running)") != std::string::npos) {
            THUNDER_LOG_INFO("%s: %s is active\n", __FUNCTION__, process);
            ret = true;
        } else {
            THUNDER_LOG_WARN("%s: %s is not active yet\n", __FUNCTION__, process);
        }
    } else {
        THUNDER_LOG_ERROR("%s: Failed to open systemctl\n", __FUNCTION__);
    }
   return ret;
}