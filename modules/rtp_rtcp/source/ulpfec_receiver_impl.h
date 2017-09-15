/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_ULPFEC_RECEIVER_IMPL_H_
#define MODULES_RTP_RTCP_SOURCE_ULPFEC_RECEIVER_IMPL_H_

#include <memory>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/include/ulpfec_receiver.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "rtc_base/criticalsection.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class UlpfecReceiverImpl : public UlpfecReceiver {
 public:
  explicit UlpfecReceiverImpl(uint32_t ssrc, RecoveredPacketReceiver* callback);
  virtual ~UlpfecReceiverImpl();

  int32_t AddReceivedRedPacket(const RTPHeader& rtp_header,
                               const uint8_t* incoming_rtp_packet,
                               size_t packet_length,
                               uint8_t ulpfec_payload_type) override;

  int32_t ProcessReceivedFec() override;

  FecPacketCounter GetPacketCounter() const override;

 private:
  const uint32_t ssrc_;

  rtc::CriticalSection crit_sect_;
  RecoveredPacketReceiver* recovered_packet_callback_;
  std::unique_ptr<ForwardErrorCorrection> fec_;
  // TODO(holmer): In the current version |received_packets_| is never more
  // than one packet, since we process FEC every time a new packet
  // arrives. We should remove the list.
  ForwardErrorCorrection::ReceivedPacketList received_packets_;
  ForwardErrorCorrection::RecoveredPacketList recovered_packets_;
  FecPacketCounter packet_counter_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_ULPFEC_RECEIVER_IMPL_H_
