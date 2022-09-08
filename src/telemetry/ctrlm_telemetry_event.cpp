#include "ctrlm_telemetry_event.h"
#include "ctrlm_log.h"

template <>
bool ctrlm_telemetry_event_t<int>::event() const {
    LOG_INFO("%s: telemetry event <%s, %d>\n", __FUNCTION__, this->marker.c_str(), this->value);
    return(t2_event_d((char *)this->marker.c_str(), this->value) == T2ERROR_SUCCESS);
}

template <>
bool ctrlm_telemetry_event_t<double>::event() const {
    LOG_INFO("%s: telemetry event <%s, %f>\n", __FUNCTION__, this->marker.c_str(), this->value);
    return(t2_event_f((char *)this->marker.c_str(), this->value) == T2ERROR_SUCCESS);
}

template <>
bool ctrlm_telemetry_event_t<std::string>::event() const {
    LOG_INFO("%s: telemetry event <%s, %s>\n", __FUNCTION__, this->marker.c_str(), this->value.c_str());
    return(t2_event_s((char *)this->marker.c_str(), (char *)this->value.c_str()) == T2ERROR_SUCCESS);
}