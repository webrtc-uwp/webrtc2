/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_OUTPUT_RTC_EVENT_LOG_OUTPUT_FILE_H_
#define LOGGING_RTC_EVENT_LOG_OUTPUT_RTC_EVENT_LOG_OUTPUT_FILE_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "logging/rtc_event_log/output/rtc_event_log_output.h"
#include "rtc_base/platform_file.h"  // Can't neatly forward PlatformFile.

namespace webrtc {

class FileWrapper;

class RtcEventLogOutputFile final : public RtcEventLogOutput {
 public:
  explicit RtcEventLogOutputFile(const std::string& file_name);
  RtcEventLogOutputFile(const std::string& file_name, int64_t max_size_bytes);

  explicit RtcEventLogOutputFile(rtc::PlatformFile file);
  RtcEventLogOutputFile(rtc::PlatformFile file, int64_t max_size_bytes);

  ~RtcEventLogOutputFile() override;

  bool IsActive() const override;

  bool Write(const std::string& output) override;

 private:
  // IsActive() can be called either from outside or from inside, but we don't
  // want to incur the overhead of a virtual function call if called from inside
  // some other function of this class.
  inline bool IsActiveInternal() const;

  // TODO(eladalon): !!! Discuss with Bjorn - do we want to manage the max size
  // ourselves, to allow us to LOG() the correct error message, or do we want
  // to offload that on FileWrapper (it can take a max file-size).
  const size_t max_size_bytes_;
  size_t written_bytes_{0};
  std::unique_ptr<FileWrapper> file_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_OUTPUT_RTC_EVENT_LOG_OUTPUT_FILE_H_
