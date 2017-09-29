/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <WebRTC/RTCMacros.h>
#import <WebRTC/RTCMediaStreamTrack.h>
#include "webrtc/rtc_base/scoped_ref_ptr.h"

NS_ASSUME_NONNULL_BEGIN

namespace rtc {

class BitrateAllocationStrategy;
}

RTC_EXPORT
@interface RTCBitrateAllocationStrategy : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWith:(rtc::BitrateAllocationStrategy*)strategy;

/** The audio source for this audio track. */
@property(nonatomic, readonly) rtc::BitrateAllocationStrategy* strategy;

@end

NS_ASSUME_NONNULL_END
