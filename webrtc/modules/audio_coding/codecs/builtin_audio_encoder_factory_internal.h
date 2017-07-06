/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_BUILTIN_AUDIO_ENCODER_FACTORY_INTERNAL_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_BUILTIN_AUDIO_ENCODER_FACTORY_INTERNAL_H_

#include "webrtc/api/audio_codecs/audio_encoder_factory.h"
#include "webrtc/rtc_base/scoped_ref_ptr.h"

namespace webrtc {

// Creates a new factory that can create the built-in types of audio encoders.
// NOTE: This function is still under development and may change without notice.
rtc::scoped_refptr<AudioEncoderFactory>
CreateBuiltinAudioEncoderFactoryInternal();

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_BUILTIN_AUDIO_ENCODER_FACTORY_INTERNAL_H_
