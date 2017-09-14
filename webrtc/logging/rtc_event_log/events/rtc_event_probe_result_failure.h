/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROBE_RESULT_FAILURE_H_
#define WEBRTC_LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROBE_RESULT_FAILURE_H_

#include "webrtc/logging/rtc_event_log/events/rtc_event.h"

namespace webrtc {

enum ProbeFailureReason {
  kInvalidSendReceiveInterval,
  kInvalidSendReceiveRatio,
  kTimeout
};

class RtcEventProbeResultFailure final : public RtcEvent {
 public:
  RtcEventProbeResultFailure(int id, ProbeFailureReason failure_reason);
  ~RtcEventProbeResultFailure() override = default;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  const int id_;
  const ProbeFailureReason failure_reason_;
};

}  // namespace webrtc

#endif  // WEBRTC_LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_PROBE_RESULT_FAILURE_H_
