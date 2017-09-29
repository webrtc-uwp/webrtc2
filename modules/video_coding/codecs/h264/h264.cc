/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "modules/video_coding/codecs/h264/include/h264.h"

#include "api/video_codecs/sdp_video_format.h"

#if defined(WEBRTC_USE_H264)
#include "modules/video_coding/codecs/h264/h264_decoder_impl.h"
#include "modules/video_coding/codecs/h264/h264_encoder_impl.h"
#endif

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

#if defined(WEBRTC_USE_H264)
bool g_rtc_use_h264 = true;
#endif

// If any H.264 codec is supported (iOS HW or OpenH264/FFmpeg).
bool IsH264CodecSupported() {
#if defined(WEBRTC_USE_H264)
  return g_rtc_use_h264;
#else
  return false;
#endif
}

}  // namespace

void DisableRtcUseH264() {
#if defined(WEBRTC_USE_H264)
  g_rtc_use_h264 = false;
#endif
}

std::vector<SdpVideoFormat> SupportedH264Codecs() {
  if (!IsH264CodecSupported())
    return std::vector<SdpVideoFormat>();
  std::vector<SdpVideoFormat> codecs;

  codecs.push_back(SdpVideoFormat(
      cricket::kH264CodecName, {{cricket::kH264FmtpProfileLevelId,
                                 cricket::kH264ProfileLevelConstrainedBaseline},
                                {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
                                {cricket::kH264FmtpPacketizationMode, "1"}}));

  return codecs;
}

H264Encoder* H264Encoder::Create(const cricket::VideoCodec& codec) {
  RTC_DCHECK(H264Encoder::IsSupported());
#if defined(WEBRTC_USE_H264)
  RTC_CHECK(g_rtc_use_h264);
  LOG(LS_INFO) << "Creating H264EncoderImpl.";
  return new H264EncoderImpl(codec);
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

bool H264Encoder::IsSupported() {
  return IsH264CodecSupported();
}

H264Decoder* H264Decoder::Create() {
  RTC_DCHECK(H264Decoder::IsSupported());
#if defined(WEBRTC_USE_H264)
  RTC_CHECK(g_rtc_use_h264);
  LOG(LS_INFO) << "Creating H264DecoderImpl.";
  return new H264DecoderImpl();
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

bool H264Decoder::IsSupported() {
  return IsH264CodecSupported();
}

}  // namespace webrtc
