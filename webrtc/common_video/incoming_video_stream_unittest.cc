/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/event.h"
#include "webrtc/common_video/include/incoming_video_stream.h"
#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/video_frame.h"

namespace webrtc {

// Basic test that checks if the no-smoothing implementation delivers a frame.
TEST(IncomingVideoStreamTest, NoSmoothingOneFrame) {
  class TestCallback : public rtc::VideoSinkInterface<VideoFrame> {
   public:
    TestCallback() : event_(false, false) {}
    ~TestCallback() override {}

    bool WaitForFrame(int milliseconds) { return event_.Wait(milliseconds); }

   private:
    void OnFrame(const VideoFrame& frame) override {
      event_.Set();
    }

    rtc::Event event_;
  } callback;
  IncomingVideoStreamNoSmoothing stream(&callback);

  rtc::VideoSinkInterface<VideoFrame>* stream_sink = &stream;
  stream_sink->OnFrame(VideoFrame());

  EXPECT_TRUE(callback.WaitForFrame(500));
}

// Tests that if the renderer is too slow, that frames will be dropped and
// the "producer thread" (main test thread), will not be blocked from delivering
// frames.
TEST(IncomingVideoStreamTest, NoSmoothingTooManyFrames) {
  class TestCallback : public rtc::VideoSinkInterface<VideoFrame> {
   public:
    TestCallback() : event_(false, false) {}
    ~TestCallback() override {}

    void Continue() { event_.Set(); }
    int frame_count() const { return frame_count_; }

   private:
    void OnFrame(const VideoFrame& frame) override {
      ++frame_count_;
      if (frame_count_ == 1) {
        // Block delivery of frames until we're allowed to continue.
        event_.Wait(rtc::Event::kForever);
      }
    }

    rtc::Event event_;
    int frame_count_ = 0;
  } callback;

  {
    IncomingVideoStreamNoSmoothing stream(&callback);

    rtc::VideoSinkInterface<VideoFrame>* stream_sink = &stream;
    for (int i = 0; i < 100; ++i)
      stream_sink->OnFrame(VideoFrame());
    // We need to set the 'continue' event before |stream| goes out of scope
    // since we're currently blocking the queue (i.e. stream can't be deleted).
    callback.Continue();
  }

  // Once |stream| has been deleted, we're guaranteed that no more calls to
  // the OnFrame callback will be made, so we can safely check the frame count
  // without having to worry about synchronization.

  // In practice frame_count will be ~1.
  EXPECT_LT(callback.frame_count(), 100);
  EXPECT_GE(callback.frame_count(), 1);
}

}  // namespace webrtc
