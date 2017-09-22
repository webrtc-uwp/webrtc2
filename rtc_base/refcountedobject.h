/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_REFCOUNTEDOBJECT_H_
#define RTC_BASE_REFCOUNTEDOBJECT_H_

#include <utility>

#include "rtc_base/refcounter.h"

namespace rtc {

template <class T>
class RefCountedObject : public T {
 public:
  RefCountedObject() {}

  template <class P0>
  explicit RefCountedObject(P0&& p0) : T(std::forward<P0>(p0)) {}

  template <class P0, class P1, class... Args>
  RefCountedObject(P0&& p0, P1&& p1, Args&&... args)
      : T(std::forward<P0>(p0),
          std::forward<P1>(p1),
          std::forward<Args>(args)...) {}

  // TODO(nisse): What would it take to change return values here?
  virtual int AddRef() const { return ref_count_.AddRef(); }

  virtual int Release() const {
    int count = ref_count_.Release();
    if (!count) {
      delete this;
    }
    return count;
  }

  // Return whether the reference count is one. If the reference count is used
  // in the conventional way, a reference count of 1 implies that the current
  // thread owns the reference and no other thread shares it. This call
  // performs the test for a reference count of one, and performs the memory
  // barrier needed for the owning thread to act on the object, knowing that it
  // has exclusive access to the object.
  virtual bool HasOneRef() const { return ref_count_.HasOneRef(); }

 protected:
  virtual ~RefCountedObject() {}

  mutable webrtc::webrtc_impl::RefCounter ref_count_;
};

}  // namespace rtc

#endif  // RTC_BASE_REFCOUNTEDOBJECT_H_
