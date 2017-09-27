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
#import "WebRTC/RTCInternalSoftwareVideoCodec.h"

#import "WebRTC/RTCVideoCodec.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/codec.h"

@interface RTCInternalSoftwareVideoEncoder ()

- (instancetype)initWithNativeEncoder:(std::unique_ptr<webrtc::VideoEncoder>)encoder;
- (std::unique_ptr<webrtc::VideoEncoder>)wrappedEncoder;

// TODO(andersc): Remove this if/when ObjCVideoEncoderFactory::QueryVideoEncoder does not need
// to use it anymore.
+ (NSSet<NSString *> *)supportedFormats;

@end
