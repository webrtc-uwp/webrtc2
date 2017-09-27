/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "NSString+StdString.h"
#import "RTCInternalSoftwareVideoDecoder+Private.h"
#import "RTCVideoCodec+Private.h"

#include "rtc_base/timeutils.h"
#include "sdk/objc/Framework/Classes/Video/objc_frame_buffer.h"

class WrappedDecodeCompleteCallback : public webrtc::DecodedImageCallback {
 public:
  int32_t Decoded(webrtc::VideoFrame &decodedImage) {
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer =
        decodedImage.video_frame_buffer();
    rtc::scoped_refptr<webrtc::ObjCFrameBuffer> objc_frame_buffer(
        static_cast<webrtc::ObjCFrameBuffer *>(video_frame_buffer.get()));
    id<RTCVideoFrameBuffer> rtcFrameBuffer =
        (id<RTCVideoFrameBuffer>)objc_frame_buffer->wrapped_frame_buffer();

    RTCVideoFrame *videoFrame = [[RTCVideoFrame alloc]
        initWithBuffer:rtcFrameBuffer
              rotation:static_cast<RTCVideoRotation>(decodedImage.rotation())
           timeStampNs:decodedImage.timestamp_us() * rtc::kNumNanosecsPerMicrosec];
    videoFrame.timeStamp = decodedImage.timestamp();

    callback(videoFrame);

    return 0;
  }

  RTCVideoDecoderCallback callback;
};

@implementation RTCInternalSoftwareVideoDecoder {
  std::unique_ptr<webrtc::VideoDecoder> _wrappedDecoder;
  std::unique_ptr<WrappedDecodeCompleteCallback> _decodeCompleteCallback;
}

- (instancetype)initWithNativeDecoder:(std::unique_ptr<webrtc::VideoDecoder>)decoder {
  if (self = [super init]) {
    _wrappedDecoder = std::move(decoder);
  }

  return self;
}

- (std::unique_ptr<webrtc::VideoDecoder>)wrappedDecoder {
  return std::move(_wrappedDecoder);
}

#pragma mark - RTCVideoDecoder

- (void)setCallback:(RTCVideoDecoderCallback)callback {
  _decodeCompleteCallback.reset(new WrappedDecodeCompleteCallback());
  _decodeCompleteCallback->callback = callback;
  _wrappedDecoder->RegisterDecodeCompleteCallback(_decodeCompleteCallback.get());
}

- (NSInteger)startDecodeWithSettings:(RTCVideoEncoderSettings *)settings
                       numberOfCores:(int)numberOfCores {
  webrtc::VideoCodec codecSettings = [settings nativeVideoCodec];
  return _wrappedDecoder->InitDecode(&codecSettings, numberOfCores);
}

- (NSInteger)releaseDecoder {
  return _wrappedDecoder->Release();
}

- (NSInteger)decode:(RTCEncodedImage *)encodedImage
          missingFrames:(BOOL)missingFrames
    fragmentationHeader:(RTCRtpFragmentationHeader *)fragmentationHeader
      codecSpecificInfo:(__nullable id<RTCCodecSpecificInfo>)info
           renderTimeMs:(int64_t)renderTimeMs {
  webrtc::EncodedImage image = [encodedImage nativeEncodedImage];

  // Handle types that can be converted into one of webrtc::CodecSpecificInfo's hard coded cases.
  webrtc::CodecSpecificInfo codecSpecificInfo;
  if ([info isKindOfClass:[RTCCodecSpecificInfoH264 class]]) {
    codecSpecificInfo = [(RTCCodecSpecificInfoH264 *)info nativeCodecSpecificInfo];
  }

  std::unique_ptr<webrtc::RTPFragmentationHeader> header =
      [fragmentationHeader createNativeFragmentationHeader];

  return _wrappedDecoder->Decode(
      image, missingFrames, header.get(), &codecSpecificInfo, renderTimeMs);
}

- (NSString *)implementationName {
  return [NSString stringForStdString:_wrappedDecoder->ImplementationName()];
}

@end
