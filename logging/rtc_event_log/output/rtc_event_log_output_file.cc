/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/file_wrapper.h"

namespace webrtc {

namespace {
constexpr int64_t NoFileSizeLimit = -1;
}  // namespace

// TODO(eladalon): !!! Discuss with Bjorn - I am changing the behavior of
// calling StartLogging() twice. Now, the second time would cause an implicit
// StopLogging on the previous output. Approved?

RtcEventLogOutputFile::RtcEventLogOutputFile(const std::string& file_name)
    : RtcEventLogOutputFile(file_name, NoFileSizeLimit) {}

RtcEventLogOutputFile::RtcEventLogOutputFile(const std::string& file_name,
                                             int64_t max_size_bytes)
    : max_size_bytes_(max_size_bytes), file_(FileWrapper::Create()) {
  if (!file_->OpenFile(file_name.c_str(), false)) {
    LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    file_.reset();
    return;
  }
}

RtcEventLogOutputFile::RtcEventLogOutputFile(rtc::PlatformFile file)
    : RtcEventLogOutputFile(file, NoFileSizeLimit) {}

RtcEventLogOutputFile::RtcEventLogOutputFile(rtc::PlatformFile file,
                                             int64_t max_size_bytes)
    : max_size_bytes_(max_size_bytes), file_(FileWrapper::Create()) {
  FILE* file_handle = rtc::FdopenPlatformFileForWriting(file);
  if (!file_handle) {
    LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    // Even though we failed to open a FILE*, the file is still open
    // and needs to be closed.
    if (!rtc::ClosePlatformFile(file)) {
      LOG(LS_ERROR) << "Can't close file.";
      file_.reset();
    }
    return;
  }
  if (!file_->OpenFromFileHandle(file_handle)) {
    LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    file_.reset();
    return;
  }
}

RtcEventLogOutputFile::~RtcEventLogOutputFile() {
  if (file_) {
    file_->CloseFile();
    file_.reset();
  }
}

bool RtcEventLogOutputFile::IsActive() const {
  return IsActiveInternal();
}

bool RtcEventLogOutputFile::Write(const std::string& output) {
  RTC_DCHECK(IsActiveInternal());
  if (written_bytes_ + output.length() <= max_size_bytes_) {
    if (!file_->Write(output.c_str(), output.size())) {
      LOG(LS_ERROR) << "FileWrapper failed to write WebRtcEventLog file.";
      // The current FileWrapper implementation closes the file on error.
      RTC_DCHECK(!file_->is_open());
      return true;
    }
    written_bytes_ += output.length();
    return true;
  } else {
    file_->CloseFile();
    file_.reset();
    return false;
  }
}

bool RtcEventLogOutputFile::IsActiveInternal() const {
  return file_ && file_->is_open();
}

}  // namespace webrtc
