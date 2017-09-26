/* Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
/*
 * This file defines the interface for doing temporal layers with VP8.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_
#define MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_

#include <vector>

#include "typedefs.h"  // NOLINT(build/include)

struct vpx_codec_enc_cfg;
typedef struct vpx_codec_enc_cfg vpx_codec_enc_cfg_t;

namespace webrtc {

struct CodecSpecificInfoVP8;

class TemporalLayers {
 public:
  enum BufferFlags {
    kNone = 0,
    kReference = 1,
    kUpdate = 2,
    kReferenceAndUpdate = kReference | kUpdate,
  };
  enum FreezeEntropy { kFreezeEntropy };

  struct FrameConfig {
    FrameConfig();

    FrameConfig(BufferFlags last, BufferFlags golden, BufferFlags arf);
    FrameConfig(BufferFlags last,
                BufferFlags golden,
                BufferFlags arf,
                FreezeEntropy);

    bool drop_frame;
    BufferFlags last_buffer_flags;
    BufferFlags golden_buffer_flags;
    BufferFlags arf_buffer_flags;

    // The encoder layer ID is used to utilize the correct bitrate allocator
    // inside the encoder. It does not control references nor determine which
    // "actual" temporal layer this is. The packetizer temporal index determines
    // which layer the encoded frame should be packetized into.
    // Normally these are the same, but current temporal-layer strategies for
    // screenshare use one bitrate allocator for all layers, but attempt to
    // packetize / utilize references to split a stream into multiple layers,
    // with different quantizer settings, to hit target bitrate.
    // TODO(pbos): Screenshare layers are being reconsidered at the time of
    // writing, we might be able to remove this distinction, and have a temporal
    // layer imply both (the normal case).
    int encoder_layer_id;
    int packetizer_temporal_idx;

    bool layer_sync;

    bool freeze_entropy;

    bool operator==(const FrameConfig& o) const {
      return drop_frame == o.drop_frame &&
             last_buffer_flags == o.last_buffer_flags &&
             golden_buffer_flags == o.golden_buffer_flags &&
             arf_buffer_flags == o.arf_buffer_flags &&
             layer_sync == o.layer_sync && freeze_entropy == o.freeze_entropy &&
             encoder_layer_id == o.encoder_layer_id &&
             packetizer_temporal_idx == o.packetizer_temporal_idx;
    }
    bool operator!=(const FrameConfig& o) const { return !(*this == o); }

   private:
    FrameConfig(BufferFlags last,
                BufferFlags golden,
                BufferFlags arf,
                bool freeze_entropy);
  };

  // Factory for TemporalLayer strategy. Default behavior is a fixed pattern
  // of temporal layers. See default_temporal_layers.cc
  virtual ~TemporalLayers() {}

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  virtual FrameConfig UpdateLayerConfig(uint32_t timestamp) = 0;

  // Update state based on new bitrate target and incoming framerate.
  // Returns the bitrate allocation for the active temporal layers.
  virtual std::vector<uint32_t> OnRatesUpdated(int bitrate_kbps,
                                               int max_bitrate_kbps,
                                               int framerate) = 0;

  // Update the encoder configuration with target bitrates or other parameters.
  // Returns true iff the configuration was actually modified.
  virtual bool UpdateConfiguration(vpx_codec_enc_cfg_t* cfg) = 0;

  virtual void PopulateCodecSpecific(
      bool is_keyframe,
      const TemporalLayers::FrameConfig& tl_config,
      CodecSpecificInfoVP8* vp8_info,
      uint32_t timestamp) = 0;

  virtual void FrameEncoded(unsigned int size, int qp) = 0;

  // Returns the current tl0_pic_idx, so it can be reused in future
  // instantiations.
  virtual uint8_t Tl0PicIdx() const = 0;
};

class TemporalLayersListener;
class TemporalLayersFactory {
 public:
  TemporalLayersFactory() : listener_(nullptr) {}
  virtual ~TemporalLayersFactory() {}
  virtual TemporalLayers* Create(int simulcast_id,
                                 int temporal_layers,
                                 uint8_t initial_tl0_pic_idx) const;
  void SetListener(TemporalLayersListener* listener);

 protected:
  TemporalLayersListener* listener_;
};

class ScreenshareTemporalLayersFactory : public webrtc::TemporalLayersFactory {
 public:
  ScreenshareTemporalLayersFactory() {}
  virtual ~ScreenshareTemporalLayersFactory() {}

  webrtc::TemporalLayers* Create(int simulcast_id,
                                 int num_temporal_layers,
                                 uint8_t initial_tl0_pic_idx) const override;
};

class TemporalLayersListener {
 public:
  TemporalLayersListener() {}
  virtual ~TemporalLayersListener() {}

  virtual void OnTemporalLayersCreated(int simulcast_id,
                                       TemporalLayers* layers) = 0;
};

class TemporalLayersChecker {
 public:
  TemporalLayersChecker() {}
  virtual ~TemporalLayersChecker() {}

  virtual bool CheckOnFrameEncoded(
      bool frame_is_keyframe,
      const CodecSpecificInfoVP8& codec_specific,
      TemporalLayers::FrameConfig& frame_config) = 0;
};

}  // namespace webrtc
#endif  // MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_
