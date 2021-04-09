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
#ifndef CTRLM_VOICE_PACKET_ANALYSIS_H_
#define CTRLM_VOICE_PACKET_ANALYSIS_H_

#include <glib.h>

enum ctrlm_voice_packet_analysis_result_t {
   CTRLM_VOICE_PACKET_ANALYSIS_GOOD,
   CTRLM_VOICE_PACKET_ANALYSIS_DUPLICATE,
   CTRLM_VOICE_PACKET_ANALYSIS_DISCONTINUITY,
   CTRLM_VOICE_PACKET_ANALYSIS_BAD,
   CTRLM_VOICE_PACKET_ANALYSIS_ERROR
};

struct ctrlm_voice_packet_analysis_stats_t {
   guint32 total_packets;
   guint32 bad_packets;
   guint32 duplicated_packets;
   guint32 lost_packets;
   guint32 sequence_error_count;
};

class ctrlm_voice_packet_analysis {
   public:
      ctrlm_voice_packet_analysis(){};
      virtual ~ctrlm_voice_packet_analysis(){};
      virtual void reset()=0;
      virtual ctrlm_voice_packet_analysis_result_t packet_check(const void* header, unsigned long header_len, const void* data, unsigned long data_len) =0;
      virtual void stats_get(ctrlm_voice_packet_analysis_stats_t& stats) const=0;
};

ctrlm_voice_packet_analysis* ctrlm_voice_packet_analysis_factory();

#endif /* CTRLM_VOICE_PACKET_ANALYSIS_H_ */
