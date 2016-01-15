/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

using webrtc::RTCPUtility::PT_APP;
using webrtc::RTCPUtility::PT_IJ;
using webrtc::RTCPUtility::PT_PSFB;
using webrtc::RTCPUtility::PT_RTPFB;
using webrtc::RTCPUtility::PT_SDES;
using webrtc::RTCPUtility::PT_SR;

using webrtc::RTCPUtility::RTCPPacketAPP;
using webrtc::RTCPUtility::RTCPPacketPSFBRPSI;
using webrtc::RTCPUtility::RTCPPacketReportBlockItem;
using webrtc::RTCPUtility::RTCPPacketRTPFBNACK;
using webrtc::RTCPUtility::RTCPPacketRTPFBNACKItem;
using webrtc::RTCPUtility::RTCPPacketSR;

namespace webrtc {
namespace rtcp {
namespace {
void AssignUWord8(uint8_t* buffer, size_t* offset, uint8_t value) {
  buffer[(*offset)++] = value;
}
void AssignUWord16(uint8_t* buffer, size_t* offset, uint16_t value) {
  ByteWriter<uint16_t>::WriteBigEndian(buffer + *offset, value);
  *offset += 2;
}
void AssignUWord32(uint8_t* buffer, size_t* offset, uint32_t value) {
  ByteWriter<uint32_t>::WriteBigEndian(buffer + *offset, value);
  *offset += 4;
}

//  Sender report (SR) (RFC 3550).
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|    RC   |   PT=SR=200   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                         SSRC of sender                        |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |              NTP timestamp, most significant word             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |             NTP timestamp, least significant word             |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                         RTP timestamp                         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                     sender's packet count                     |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                      sender's octet count                     |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

void CreateSenderReport(const RTCPPacketSR& sr,
                        uint8_t* buffer,
                        size_t* pos) {
  AssignUWord32(buffer, pos, sr.SenderSSRC);
  AssignUWord32(buffer, pos, sr.NTPMostSignificant);
  AssignUWord32(buffer, pos, sr.NTPLeastSignificant);
  AssignUWord32(buffer, pos, sr.RTPTimestamp);
  AssignUWord32(buffer, pos, sr.SenderPacketCount);
  AssignUWord32(buffer, pos, sr.SenderOctetCount);
}

//  Report block (RFC 3550).
//
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |                 SSRC_1 (SSRC of first source)                 |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | fraction lost |       cumulative number of packets lost       |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |           extended highest sequence number received           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                      interarrival jitter                      |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                         last SR (LSR)                         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                   delay since last SR (DLSR)                  |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+

void CreateReportBlocks(const std::vector<ReportBlock>& blocks,
                        uint8_t* buffer,
                        size_t* pos) {
  for (const ReportBlock& block : blocks) {
    block.Create(buffer + *pos);
    *pos += ReportBlock::kLength;
  }
}

// Source Description (SDES) (RFC 3550).
//
//         0                   1                   2                   3
//         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// header |V=2|P|    SC   |  PT=SDES=202  |             length            |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// chunk  |                          SSRC/CSRC_1                          |
//   1    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                           SDES items                          |
//        |                              ...                              |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// chunk  |                          SSRC/CSRC_2                          |
//   2    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                           SDES items                          |
//        |                              ...                              |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//
// Canonical End-Point Identifier SDES Item (CNAME)
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |    CNAME=1    |     length    | user and domain name        ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void CreateSdes(const std::vector<Sdes::Chunk>& chunks,
                uint8_t* buffer,
                size_t* pos) {
  const uint8_t kSdesItemType = 1;
  for (std::vector<Sdes::Chunk>::const_iterator it = chunks.begin();
       it != chunks.end(); ++it) {
    AssignUWord32(buffer, pos, (*it).ssrc);
    AssignUWord8(buffer, pos, kSdesItemType);
    AssignUWord8(buffer, pos, (*it).name.length());
    memcpy(buffer + *pos, (*it).name.data(), (*it).name.length());
    *pos += (*it).name.length();
    memset(buffer + *pos, 0, (*it).null_octets);
    *pos += (*it).null_octets;
  }
}

// Reference picture selection indication (RPSI) (RFC 4585).
//
// FCI:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |      PB       |0| Payload Type|    Native RPSI bit string     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   defined per codec          ...                | Padding (0) |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void CreateRpsi(const RTCPPacketPSFBRPSI& rpsi,
                uint8_t padding_bytes,
                uint8_t* buffer,
                size_t* pos) {
  // Native bit string should be a multiple of 8 bits.
  assert(rpsi.NumberOfValidBits % 8 == 0);
  AssignUWord32(buffer, pos, rpsi.SenderSSRC);
  AssignUWord32(buffer, pos, rpsi.MediaSSRC);
  AssignUWord8(buffer, pos, padding_bytes * 8);
  AssignUWord8(buffer, pos, rpsi.PayloadType);
  memcpy(buffer + *pos, rpsi.NativeBitString, rpsi.NumberOfValidBits / 8);
  *pos += rpsi.NumberOfValidBits / 8;
  memset(buffer + *pos, 0, padding_bytes);
  *pos += padding_bytes;
}
}  // namespace

void RtcpPacket::Append(RtcpPacket* packet) {
  assert(packet);
  appended_packets_.push_back(packet);
}

rtc::scoped_ptr<RawPacket> RtcpPacket::Build() const {
  size_t length = 0;
  rtc::scoped_ptr<RawPacket> packet(new RawPacket(IP_PACKET_SIZE));

  class PacketVerifier : public PacketReadyCallback {
   public:
    explicit PacketVerifier(RawPacket* packet)
        : called_(false), packet_(packet) {}
    virtual ~PacketVerifier() {}
    void OnPacketReady(uint8_t* data, size_t length) override {
      RTC_CHECK(!called_) << "Fragmentation not supported.";
      called_ = true;
      packet_->SetLength(length);
    }

   private:
    bool called_;
    RawPacket* const packet_;
  } verifier(packet.get());
  CreateAndAddAppended(packet->MutableBuffer(), &length, packet->BufferLength(),
                       &verifier);
  OnBufferFull(packet->MutableBuffer(), &length, &verifier);
  return packet;
}

bool RtcpPacket::Build(PacketReadyCallback* callback) const {
  uint8_t buffer[IP_PACKET_SIZE];
  return BuildExternalBuffer(buffer, IP_PACKET_SIZE, callback);
}

bool RtcpPacket::BuildExternalBuffer(uint8_t* buffer,
                                     size_t max_length,
                                     PacketReadyCallback* callback) const {
  size_t index = 0;
  if (!CreateAndAddAppended(buffer, &index, max_length, callback))
    return false;
  return OnBufferFull(buffer, &index, callback);
}

bool RtcpPacket::CreateAndAddAppended(uint8_t* packet,
                                      size_t* index,
                                      size_t max_length,
                                      PacketReadyCallback* callback) const {
  if (!Create(packet, index, max_length, callback))
    return false;
  for (RtcpPacket* appended : appended_packets_) {
    if (!appended->CreateAndAddAppended(packet, index, max_length, callback))
      return false;
  }
  return true;
}

bool RtcpPacket::OnBufferFull(uint8_t* packet,
                              size_t* index,
                              RtcpPacket::PacketReadyCallback* callback) const {
  if (*index == 0)
    return false;
  callback->OnPacketReady(packet, *index);
  *index = 0;
  return true;
}

size_t RtcpPacket::HeaderLength() const {
  size_t length_in_bytes = BlockLength();
  // Length in 32-bit words minus 1.
  assert(length_in_bytes > 0);
  return ((length_in_bytes + 3) / 4) - 1;
}

// From RFC 3550, RTP: A Transport Protocol for Real-Time Applications.
//
// RTP header format.
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P| RC/FMT  |      PT       |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void RtcpPacket::CreateHeader(
    uint8_t count_or_format,  // Depends on packet type.
    uint8_t packet_type,
    size_t length,
    uint8_t* buffer,
    size_t* pos) {
  assert(length <= 0xffff);
  const uint8_t kVersion = 2;
  AssignUWord8(buffer, pos, (kVersion << 6) + count_or_format);
  AssignUWord8(buffer, pos, packet_type);
  AssignUWord16(buffer, pos, length);
}

bool SenderReport::Create(uint8_t* packet,
                          size_t* index,
                          size_t max_length,
                          RtcpPacket::PacketReadyCallback* callback) const {
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  CreateHeader(sr_.NumberOfReportBlocks, PT_SR, HeaderLength(), packet, index);
  CreateSenderReport(sr_, packet, index);
  CreateReportBlocks(report_blocks_, packet, index);
  return true;
}

bool SenderReport::WithReportBlock(const ReportBlock& block) {
  if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
    LOG(LS_WARNING) << "Max report blocks reached.";
    return false;
  }
  report_blocks_.push_back(block);
  sr_.NumberOfReportBlocks = report_blocks_.size();
  return true;
}

bool Sdes::Create(uint8_t* packet,
                  size_t* index,
                  size_t max_length,
                  RtcpPacket::PacketReadyCallback* callback) const {
  assert(!chunks_.empty());
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  CreateHeader(chunks_.size(), PT_SDES, HeaderLength(), packet, index);
  CreateSdes(chunks_, packet, index);
  return true;
}

bool Sdes::WithCName(uint32_t ssrc, const std::string& cname) {
  assert(cname.length() <= 0xff);
  if (chunks_.size() >= kMaxNumberOfChunks) {
    LOG(LS_WARNING) << "Max SDES chunks reached.";
    return false;
  }
  // In each chunk, the list of items must be terminated by one or more null
  // octets. The next chunk must start on a 32-bit boundary.
  // CNAME (1 byte) | length (1 byte) | name | padding.
  int null_octets = 4 - ((2 + cname.length()) % 4);
  Chunk chunk;
  chunk.ssrc = ssrc;
  chunk.name = cname;
  chunk.null_octets = null_octets;
  chunks_.push_back(chunk);
  return true;
}

size_t Sdes::BlockLength() const {
  // Header (4 bytes).
  // Chunk:
  // SSRC/CSRC (4 bytes) | CNAME (1 byte) | length (1 byte) | name | padding.
  size_t length = kHeaderLength;
  for (const Chunk& chunk : chunks_)
    length += 6 + chunk.name.length() + chunk.null_octets;
  assert(length % 4 == 0);
  return length;
}

bool Rpsi::Create(uint8_t* packet,
                  size_t* index,
                  size_t max_length,
                  RtcpPacket::PacketReadyCallback* callback) const {
  assert(rpsi_.NumberOfValidBits > 0);
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  const uint8_t kFmt = 3;
  CreateHeader(kFmt, PT_PSFB, HeaderLength(), packet, index);
  CreateRpsi(rpsi_, padding_bytes_, packet, index);
  return true;
}

void Rpsi::WithPictureId(uint64_t picture_id) {
  const uint32_t kPidBits = 7;
  const uint64_t k7MsbZeroMask = 0x1ffffffffffffffULL;
  uint8_t required_bytes = 0;
  uint64_t shifted_pid = picture_id;
  do {
    ++required_bytes;
    shifted_pid = (shifted_pid >> kPidBits) & k7MsbZeroMask;
  } while (shifted_pid > 0);

  // Convert picture id to native bit string (natively defined by the video
  // codec).
  int pos = 0;
  for (int i = required_bytes - 1; i > 0; i--) {
    rpsi_.NativeBitString[pos++] =
        0x80 | static_cast<uint8_t>(picture_id >> (i * kPidBits));
  }
  rpsi_.NativeBitString[pos++] = static_cast<uint8_t>(picture_id & 0x7f);
  rpsi_.NumberOfValidBits = pos * 8;

  // Calculate padding bytes (to reach next 32-bit boundary, 1, 2 or 3 bytes).
  padding_bytes_ = 4 - ((2 + required_bytes) % 4);
  if (padding_bytes_ == 4) {
    padding_bytes_ = 0;
  }
}

RawPacket::RawPacket(size_t buffer_length)
    : buffer_length_(buffer_length), length_(0) {
  buffer_.reset(new uint8_t[buffer_length]);
}

RawPacket::RawPacket(const uint8_t* packet, size_t packet_length)
    : buffer_length_(packet_length), length_(packet_length) {
  buffer_.reset(new uint8_t[packet_length]);
  memcpy(buffer_.get(), packet, packet_length);
}

const uint8_t* RawPacket::Buffer() const {
  return buffer_.get();
}

uint8_t* RawPacket::MutableBuffer() {
  return buffer_.get();
}

size_t RawPacket::BufferLength() const {
  return buffer_length_;
}

size_t RawPacket::Length() const {
  return length_;
}

void RawPacket::SetLength(size_t length) {
  assert(length <= buffer_length_);
  length_ = length;
}

}  // namespace rtcp
}  // namespace webrtc
