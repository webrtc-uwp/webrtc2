/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnectionFactory+Private.h"

#import "NSString+StdString.h"
#import "RTCAVFoundationVideoSource+Private.h"
#import "RTCAudioSource+Private.h"
#import "RTCAudioTrack+Private.h"
#import "RTCMediaConstraints+Private.h"
#import "RTCMediaStream+Private.h"
#import "RTCPeerConnection+Private.h"
#import "RTCVideoSource+Private.h"
#import "RTCVideoTrack+Private.h"
#import "WebRTC/RTCAudioDecoderFactory.h"
#import "WebRTC/RTCAudioEncoderFactory.h"
#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoCodecFactory.h"
#ifndef HAVE_NO_MEDIA
#import "WebRTC/RTCBuiltinAudioDecoderFactory.h"
#import "WebRTC/RTCBuiltinAudioEncoderFactory.h"
#import "WebRTC/RTCVideoCodecH264.h"
#endif

#include "Video/objcvideotracksource.h"
#include "VideoToolbox/objc_video_decoder_factory.h"
#include "VideoToolbox/objc_video_encoder_factory.h"
#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/api/videosourceproxy.h"
// Adding the nogncheck to disable the including header check.
// The no-media version PeerConnectionFactory doesn't depend on media related
// C++ target.
// TODO(zhihuang): Remove nogncheck once MediaEngineInterface is moved to C++
// API layer.
#include "webrtc/media/engine/webrtcmediaengine.h" // nogncheck

@implementation RTCPeerConnectionFactory {
  std::unique_ptr<rtc::Thread> _networkThread;
  std::unique_ptr<rtc::Thread> _workerThread;
  std::unique_ptr<rtc::Thread> _signalingThread;
  BOOL _hasStartedAecDump;
}

@synthesize nativeFactory = _nativeFactory;

- (instancetype)init {
#ifdef HAVE_NO_MEDIA
  return [self initWithAudioEncoderFactory:nil
                       audioDecoderFactory:nil
                       videoEncoderFactory:nil
                       videoDecoderFactory:nil];
#else
  return [self
      initWithAudioEncoderFactory:[[RTCBuiltinAudioEncoderFactory alloc] init]
              audioDecoderFactory:[[RTCBuiltinAudioDecoderFactory alloc] init]
              videoEncoderFactory:[[RTCVideoEncoderFactoryH264 alloc] init]
              videoDecoderFactory:[[RTCVideoDecoderFactoryH264 alloc] init]];
#endif
}

- (instancetype)
initWithAudioEncoderFactory:(id<RTCAudioEncoderFactory>)audioEncoderFactory
        audioDecoderFactory:(id<RTCAudioDecoderFactory>)audioDecoderFactory
        videoEncoderFactory:(id<RTCVideoEncoderFactory>)videoEncoderFactory
        videoDecoderFactory:(id<RTCVideoDecoderFactory>)videoDecoderFactory {
  if (self = [super init]) {
    _networkThread = rtc::Thread::CreateWithSocketServer();
    BOOL result = _networkThread->Start();
    NSAssert(result, @"Failed to start network thread.");

    _workerThread = rtc::Thread::Create();
    result = _workerThread->Start();
    NSAssert(result, @"Failed to start worker thread.");

    _signalingThread = rtc::Thread::Create();
    result = _signalingThread->Start();
    NSAssert(result, @"Failed to start signaling thread.");
#ifdef HAVE_NO_MEDIA
    _nativeFactory = webrtc::CreateModularPeerConnectionFactory(
        _networkThread.get(), _workerThread.get(), _signalingThread.get(),
        nullptr, // default_adm
        nullptr, // audio_encoder_factory
        nullptr, // audio_decoder_factory
        nullptr, // video_encoder_factory
        nullptr, // video_decoder_factory
        nullptr, // audio_mixer
        std::unique_ptr<cricket::MediaEngineInterface>(),
        std::unique_ptr<webrtc::CallFactoryInterface>(),
        std::unique_ptr<webrtc::RtcEventLogFactoryInterface>());
#else
    rtc::scoped_refptr<webrtc::AudioEncoderFactory>
        platform_audio_encoder_factory = nullptr;
    rtc::scoped_refptr<webrtc::AudioDecoderFactory>
        platform_audio_decoder_factory = nullptr;
    if (audioEncoderFactory) {
      platform_audio_encoder_factory =
          [audioEncoderFactory nativeAudioEncoderFactory];
    }
    if (audioDecoderFactory) {
      platform_audio_decoder_factory =
          [audioDecoderFactory nativeAudioDecoderFactory];
    }
    cricket::WebRtcVideoEncoderFactory *platform_video_encoder_factory =
        nullptr;
    cricket::WebRtcVideoDecoderFactory *platform_video_decoder_factory =
        nullptr;
    if (videoEncoderFactory) {
      platform_video_encoder_factory =
          new webrtc::ObjCVideoEncoderFactory(videoEncoderFactory);
    }
    if (videoDecoderFactory) {
      platform_video_decoder_factory =
          new webrtc::ObjCVideoDecoderFactory(videoDecoderFactory);
    }

    // Ownership of encoder/decoder factories is passed on to the
    // peerconnectionfactory, that handles deleting them.
    _nativeFactory = webrtc::CreatePeerConnectionFactory(
        _networkThread.get(), _workerThread.get(), _signalingThread.get(),
        nullptr, // audio device module
        platform_audio_encoder_factory, platform_audio_decoder_factory,
        platform_video_encoder_factory, platform_video_decoder_factory);
#endif
    NSAssert(_nativeFactory, @"Failed to initialize PeerConnectionFactory!");
  }
  return self;
}

- (RTCAudioSource *)audioSourceWithConstraints:
    (nullable RTCMediaConstraints *)constraints {
  std::unique_ptr<webrtc::MediaConstraints> nativeConstraints;
  if (constraints) {
    nativeConstraints = constraints.nativeConstraints;
  }
  rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
      _nativeFactory->CreateAudioSource(nativeConstraints.get());
  return [[RTCAudioSource alloc] initWithNativeAudioSource:source];
}

- (RTCAudioTrack *)audioTrackWithTrackId:(NSString *)trackId {
  RTCAudioSource *audioSource = [self audioSourceWithConstraints:nil];
  return [self audioTrackWithSource:audioSource trackId:trackId];
}

- (RTCAudioTrack *)audioTrackWithSource:(RTCAudioSource *)source
                                trackId:(NSString *)trackId {
  return [[RTCAudioTrack alloc] initWithFactory:self
                                         source:source
                                        trackId:trackId];
}

- (RTCAVFoundationVideoSource *)avFoundationVideoSourceWithConstraints:
    (nullable RTCMediaConstraints *)constraints {
#ifdef HAVE_NO_MEDIA
  return nil;
#else
  return [[RTCAVFoundationVideoSource alloc] initWithFactory:self
                                                 constraints:constraints];
#endif
}

- (RTCVideoSource *)videoSource {
  rtc::scoped_refptr<webrtc::ObjcVideoTrackSource> objcVideoTrackSource(
      new rtc::RefCountedObject<webrtc::ObjcVideoTrackSource>());
  return [[RTCVideoSource alloc]
      initWithNativeVideoSource:webrtc::VideoTrackSourceProxy::Create(
                                    _signalingThread.get(), _workerThread.get(),
                                    objcVideoTrackSource)];
}

- (RTCVideoTrack *)videoTrackWithSource:(RTCVideoSource *)source
                                trackId:(NSString *)trackId {
  return [[RTCVideoTrack alloc] initWithFactory:self
                                         source:source
                                        trackId:trackId];
}

- (RTCMediaStream *)mediaStreamWithStreamId:(NSString *)streamId {
  return [[RTCMediaStream alloc] initWithFactory:self streamId:streamId];
}

- (RTCPeerConnection *)
peerConnectionWithConfiguration:(RTCConfiguration *)configuration
                    constraints:(RTCMediaConstraints *)constraints
                       delegate:
                           (nullable id<RTCPeerConnectionDelegate>)delegate {
  return [[RTCPeerConnection alloc] initWithFactory:self
                                      configuration:configuration
                                        constraints:constraints
                                           delegate:delegate];
}

- (BOOL)startAecDumpWithFilePath:(NSString *)filePath
                  maxSizeInBytes:(int64_t)maxSizeInBytes {
  RTC_DCHECK(filePath.length);
  RTC_DCHECK_GT(maxSizeInBytes, 0);

  if (_hasStartedAecDump) {
    RTCLogError(@"Aec dump already started.");
    return NO;
  }
  int fd = open(filePath.UTF8String, O_WRONLY | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR);
  if (fd < 0) {
    RTCLogError(@"Error opening file: %@. Error: %d", filePath, errno);
    return NO;
  }
  _hasStartedAecDump = _nativeFactory->StartAecDump(fd, maxSizeInBytes);
  return _hasStartedAecDump;
}

- (void)stopAecDump {
  _nativeFactory->StopAecDump();
  _hasStartedAecDump = NO;
}

@end
