/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_

#include <assert.h>

#include <deque>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/thread_wrapper.h"
#include "webrtc/test/fake_network_pipe.h"
#include "webrtc/transport.h"

namespace webrtc {

class Call;
class Clock;
class PacketReceiver;

namespace test {

class DirectTransport : public Transport {
 public:
  explicit DirectTransport(Call* send_call);
  DirectTransport(const FakeNetworkPipe::Config& config, Call* send_call);
  ~DirectTransport();

  void SetConfig(const FakeNetworkPipe::Config& config);

  virtual void StopSending();
  // TODO(holmer): Look into moving this to the constructor.
  virtual void SetReceiver(PacketReceiver* receiver);

  bool SendRtp(const uint8_t* data,
               size_t length,
               const PacketOptions& options) override;
  bool SendRtcp(const uint8_t* data, size_t length) override;

 private:
  static bool NetworkProcess(void* transport);
  bool SendPackets();

  rtc::CriticalSection lock_;
  Call* const send_call_;
  rtc::scoped_ptr<EventWrapper> packet_event_;
  rtc::scoped_ptr<ThreadWrapper> thread_;
  Clock* const clock_;

  bool shutting_down_;

  FakeNetworkPipe fake_network_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_
