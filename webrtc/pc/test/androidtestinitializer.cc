/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/test/androidtestinitializer.h"

#include <pthread.h>

#include "webrtc/modules/utility/include/jvm_android.h"
#include "webrtc/rtc_base/checks.h"

namespace webrtc {

namespace {

static pthread_once_t g_initialize_once = PTHREAD_ONCE_INIT;

// There can only be one JNI_OnLoad in each binary. So since this is a GTEST
// C++ runner binary, we want to initialize the same global objects we normally
// do if this had been a Java binary.
void EnsureInitializedOnce() {
  RTC_CHECK(::base::android::IsVMInitialized());
  JNIEnv* jni = ::base::android::AttachCurrentThread();
  JavaVM* jvm = NULL;
  RTC_CHECK_EQ(0, jni->GetJavaVM(&jvm));

  RTC_CHECK(rtc::InitializeSSL()) << "Failed to InitializeSSL()";

  webrtc::JVM::Initialize(jvm);
}

}  // anonymous namespace

void InitializeAndroidObjects() {
  RTC_CHECK_EQ(0, pthread_once(&g_initialize_once, &EnsureInitializedOnce));
}

}  // namespace webrtc
