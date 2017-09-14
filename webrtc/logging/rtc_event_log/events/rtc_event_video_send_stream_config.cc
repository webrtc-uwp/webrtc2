/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"

namespace webrtc {

RtcEvent::Type RtcEventVideoSendStreamConfig::GetType() const {
  return RtcEvent::Type::VideoSendStreamConfig;
}

bool RtcEventVideoSendStreamConfig::IsConfigEvent() const {
  return true;
}

}  // namespace webrtc
