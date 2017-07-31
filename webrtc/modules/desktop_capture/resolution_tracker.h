/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_RESOLUTION_TRACKER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_RESOLUTION_TRACKER_H_

#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/modules/desktop_capture/state_monitor.h"

namespace webrtc {

class ResolutionTracker final : public StateMonitor<DesktopSize> {
 public:
  // Sets the resolution to |size|. Returns true if a previous size was recorded
  // and differs from |size|.
  bool SetResolution(DesktopSize size);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_RESOLUTION_TRACKER_H_
