/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_PULSE_LINUX_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_PULSE_LINUX_H

#include <memory>

#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/modules/audio_device/linux/audio_mixer_manager_pulse_linux.h"
#include "webrtc/rtc_base/criticalsection.h"
#include "webrtc/rtc_base/platform_thread.h"
#include "webrtc/rtc_base/thread_checker.h"

#include <X11/Xlib.h>
#include <pulse/pulseaudio.h>

// We define this flag if it's missing from our headers, because we want to be
// able to compile against old headers but still use PA_STREAM_ADJUST_LATENCY
// if run against a recent version of the library.
#ifndef PA_STREAM_ADJUST_LATENCY
#define PA_STREAM_ADJUST_LATENCY 0x2000U
#endif
#ifndef PA_STREAM_START_MUTED
#define PA_STREAM_START_MUTED 0x1000U
#endif

// Set this constant to 0 to disable latency reading
const uint32_t WEBRTC_PA_REPORT_LATENCY = 1;

// Constants from implementation by Tristan Schmelcher [tschmelcher@google.com]

// First PulseAudio protocol version that supports PA_STREAM_ADJUST_LATENCY.
const uint32_t WEBRTC_PA_ADJUST_LATENCY_PROTOCOL_VERSION = 13;

// Some timing constants for optimal operation. See
// https://tango.0pointer.de/pipermail/pulseaudio-discuss/2008-January/001170.html
// for a good explanation of some of the factors that go into this.

// Playback.

// For playback, there is a round-trip delay to fill the server-side playback
// buffer, so setting too low of a latency is a buffer underflow risk. We will
// automatically increase the latency if a buffer underflow does occur, but we
// also enforce a sane minimum at start-up time. Anything lower would be
// virtually guaranteed to underflow at least once, so there's no point in
// allowing lower latencies.
const uint32_t WEBRTC_PA_PLAYBACK_LATENCY_MINIMUM_MSECS = 20;

// Every time a playback stream underflows, we will reconfigure it with target
// latency that is greater by this amount.
const uint32_t WEBRTC_PA_PLAYBACK_LATENCY_INCREMENT_MSECS = 20;

// We also need to configure a suitable request size. Too small and we'd burn
// CPU from the overhead of transfering small amounts of data at once. Too large
// and the amount of data remaining in the buffer right before refilling it
// would be a buffer underflow risk. We set it to half of the buffer size.
const uint32_t WEBRTC_PA_PLAYBACK_REQUEST_FACTOR = 2;

// Capture.

// For capture, low latency is not a buffer overflow risk, but it makes us burn
// CPU from the overhead of transfering small amounts of data at once, so we set
// a recommended value that we use for the kLowLatency constant (but if the user
// explicitly requests something lower then we will honour it).
// 1ms takes about 6-7% CPU. 5ms takes about 5%. 10ms takes about 4.x%.
const uint32_t WEBRTC_PA_LOW_CAPTURE_LATENCY_MSECS = 10;

// There is a round-trip delay to ack the data to the server, so the
// server-side buffer needs extra space to prevent buffer overflow. 20ms is
// sufficient, but there is no penalty to making it bigger, so we make it huge.
// (750ms is libpulse's default value for the _total_ buffer size in the
// kNoLatencyRequirements case.)
const uint32_t WEBRTC_PA_CAPTURE_BUFFER_EXTRA_MSECS = 750;

const uint32_t WEBRTC_PA_MSECS_PER_SEC = 1000;

// Init _configuredLatencyRec/Play to this value to disable latency requirements
const int32_t WEBRTC_PA_NO_LATENCY_REQUIREMENTS = -1;

// Set this const to 1 to account for peeked and used data in latency calculation
const uint32_t WEBRTC_PA_CAPTURE_BUFFER_LATENCY_ADJUSTMENT = 0;

namespace webrtc
{
class EventWrapper;

class AudioDeviceLinuxPulse: public AudioDeviceGeneric
{
public:
    AudioDeviceLinuxPulse(const int32_t id);
    virtual ~AudioDeviceLinuxPulse();

    // Retrieve the currently utilized audio layer
    int32_t ActiveAudioLayer(
        AudioDeviceModule::AudioLayer& audioLayer) const override;

    // Main initializaton and termination
    InitStatus Init() override;
    int32_t Terminate() override;
    bool Initialized() const override;

    // Device enumeration
    int16_t PlayoutDevices() override;
    int16_t RecordingDevices() override;
    int32_t PlayoutDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) override;
    int32_t RecordingDeviceName(uint16_t index,
                                char name[kAdmMaxDeviceNameSize],
                                char guid[kAdmMaxGuidSize]) override;

    // Device selection
    int32_t SetPlayoutDevice(uint16_t index) override;
    int32_t SetPlayoutDevice(
        AudioDeviceModule::WindowsDeviceType device) override;
    int32_t SetRecordingDevice(uint16_t index) override;
    int32_t SetRecordingDevice(
        AudioDeviceModule::WindowsDeviceType device) override;

    // Audio transport initialization
    int32_t PlayoutIsAvailable(bool& available) override;
    int32_t InitPlayout() override;
    bool PlayoutIsInitialized() const override;
    int32_t RecordingIsAvailable(bool& available) override;
    int32_t InitRecording() override;
    bool RecordingIsInitialized() const override;

    // Audio transport control
    int32_t StartPlayout() override;
    int32_t StopPlayout() override;
    bool Playing() const override;
    int32_t StartRecording() override;
    int32_t StopRecording() override;
    bool Recording() const override;

    // Microphone Automatic Gain Control (AGC)
    int32_t SetAGC(bool enable) override;
    bool AGC() const override;

    // Volume control based on the Windows Wave API (Windows only)
    int32_t SetWaveOutVolume(uint16_t volumeLeft,
                             uint16_t volumeRight) override;
    int32_t WaveOutVolume(uint16_t& volumeLeft,
                          uint16_t& volumeRight) const override;

    // Audio mixer initialization
    int32_t InitSpeaker() override;
    bool SpeakerIsInitialized() const override;
    int32_t InitMicrophone() override;
    bool MicrophoneIsInitialized() const override;

    // Speaker volume controls
    int32_t SpeakerVolumeIsAvailable(bool& available) override;
    int32_t SetSpeakerVolume(uint32_t volume) override;
    int32_t SpeakerVolume(uint32_t& volume) const override;
    int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override;
    int32_t MinSpeakerVolume(uint32_t& minVolume) const override;
    int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const override;

    // Microphone volume controls
    int32_t MicrophoneVolumeIsAvailable(bool& available) override;
    int32_t SetMicrophoneVolume(uint32_t volume) override;
    int32_t MicrophoneVolume(uint32_t& volume) const override;
    int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const override;
    int32_t MinMicrophoneVolume(uint32_t& minVolume) const override;
    int32_t MicrophoneVolumeStepSize(uint16_t& stepSize) const override;

    // Speaker mute control
    int32_t SpeakerMuteIsAvailable(bool& available) override;
    int32_t SetSpeakerMute(bool enable) override;
    int32_t SpeakerMute(bool& enabled) const override;

    // Microphone mute control
    int32_t MicrophoneMuteIsAvailable(bool& available) override;
    int32_t SetMicrophoneMute(bool enable) override;
    int32_t MicrophoneMute(bool& enabled) const override;

    // Microphone boost control
    int32_t MicrophoneBoostIsAvailable(bool& available) override;
    int32_t SetMicrophoneBoost(bool enable) override;
    int32_t MicrophoneBoost(bool& enabled) const override;

    // Stereo support
    int32_t StereoPlayoutIsAvailable(bool& available) override;
    int32_t SetStereoPlayout(bool enable) override;
    int32_t StereoPlayout(bool& enabled) const override;
    int32_t StereoRecordingIsAvailable(bool& available) override;
    int32_t SetStereoRecording(bool enable) override;
    int32_t StereoRecording(bool& enabled) const override;

    // Delay information and control
    int32_t SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                             uint16_t sizeMS) override;
    int32_t PlayoutBuffer(AudioDeviceModule::BufferType& type,
                          uint16_t& sizeMS) const override;
    int32_t PlayoutDelay(uint16_t& delayMS) const override;
    int32_t RecordingDelay(uint16_t& delayMS) const override;

    // CPU load
    int32_t CPULoad(uint16_t& load) const override;

    bool PlayoutWarning() const override;
    bool PlayoutError() const override;
    bool RecordingWarning() const override;
    bool RecordingError() const override;
    void ClearPlayoutWarning() override;
    void ClearPlayoutError() override;
    void ClearRecordingWarning() override;
    void ClearRecordingError() override;

   void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

private:
    void Lock() EXCLUSIVE_LOCK_FUNCTION(_critSect) {
        _critSect.Enter();
    }
    void UnLock() UNLOCK_FUNCTION(_critSect) {
        _critSect.Leave();
    }
    void WaitForOperationCompletion(pa_operation* paOperation) const;
    void WaitForSuccess(pa_operation* paOperation) const;

    bool KeyPressed() const;

    static void PaContextStateCallback(pa_context *c, void *pThis);
    static void PaSinkInfoCallback(pa_context *c, const pa_sink_info *i,
                                   int eol, void *pThis);
    static void PaSourceInfoCallback(pa_context *c, const pa_source_info *i,
                                     int eol, void *pThis);
    static void PaServerInfoCallback(pa_context *c, const pa_server_info *i,
                                     void *pThis);
    static void PaStreamStateCallback(pa_stream *p, void *pThis);
    void PaContextStateCallbackHandler(pa_context *c);
    void PaSinkInfoCallbackHandler(const pa_sink_info *i, int eol);
    void PaSourceInfoCallbackHandler(const pa_source_info *i, int eol);
    void PaServerInfoCallbackHandler(const pa_server_info *i);
    void PaStreamStateCallbackHandler(pa_stream *p);

    void EnableWriteCallback();
    void DisableWriteCallback();
    static void PaStreamWriteCallback(pa_stream *unused, size_t buffer_space,
                                      void *pThis);
    void PaStreamWriteCallbackHandler(size_t buffer_space);
    static void PaStreamUnderflowCallback(pa_stream *unused, void *pThis);
    void PaStreamUnderflowCallbackHandler();
    void EnableReadCallback();
    void DisableReadCallback();
    static void PaStreamReadCallback(pa_stream *unused1, size_t unused2,
                                     void *pThis);
    void PaStreamReadCallbackHandler();
    static void PaStreamOverflowCallback(pa_stream *unused, void *pThis);
    void PaStreamOverflowCallbackHandler();
    int32_t LatencyUsecs(pa_stream *stream);
    int32_t ReadRecordedData(const void* bufferData, size_t bufferSize);
    int32_t ProcessRecordedData(int8_t *bufferData,
                                uint32_t bufferSizeInSamples,
                                uint32_t recDelay);

    int32_t CheckPulseAudioVersion();
    int32_t InitSamplingFrequency();
    int32_t GetDefaultDeviceInfo(bool recDevice, char* name, uint16_t& index);
    int32_t InitPulseAudio();
    int32_t TerminatePulseAudio();

    void PaLock();
    void PaUnLock();

    static bool RecThreadFunc(void*);
    static bool PlayThreadFunc(void*);
    bool RecThreadProcess();
    bool PlayThreadProcess();

    AudioDeviceBuffer* _ptrAudioBuffer;

    rtc::CriticalSection _critSect;
    EventWrapper& _timeEventRec;
    EventWrapper& _timeEventPlay;
    EventWrapper& _recStartEvent;
    EventWrapper& _playStartEvent;

    // TODO(pbos): Remove unique_ptr and use directly without resetting.
    std::unique_ptr<rtc::PlatformThread> _ptrThreadPlay;
    std::unique_ptr<rtc::PlatformThread> _ptrThreadRec;
    int32_t _id;

    AudioMixerManagerLinuxPulse _mixerManager;

    uint16_t _inputDeviceIndex;
    uint16_t _outputDeviceIndex;
    bool _inputDeviceIsSpecified;
    bool _outputDeviceIsSpecified;

    int sample_rate_hz_;
    uint8_t _recChannels;
    uint8_t _playChannels;

    AudioDeviceModule::BufferType _playBufType;

    // Stores thread ID in constructor.
    // We can then use ThreadChecker::CalledOnValidThread() to ensure that
    // other methods are called from the same thread.
    // Currently only does RTC_DCHECK(thread_checker_.CalledOnValidThread()).
    rtc::ThreadChecker thread_checker_;

    bool _initialized;
    bool _recording;
    bool _playing;
    bool _recIsInitialized;
    bool _playIsInitialized;
    bool _startRec;
    bool _stopRec;
    bool _startPlay;
    bool _stopPlay;
    bool _AGC;
    bool update_speaker_volume_at_startup_;

    uint16_t _playBufDelayFixed; // fixed playback delay

    uint32_t _sndCardPlayDelay;
    uint32_t _sndCardRecDelay;

    int32_t _writeErrors;
    uint16_t _playWarning;
    uint16_t _playError;
    uint16_t _recWarning;
    uint16_t _recError;

    uint16_t _deviceIndex;
    int16_t _numPlayDevices;
    int16_t _numRecDevices;
    char* _playDeviceName;
    char* _recDeviceName;
    char* _playDisplayDeviceName;
    char* _recDisplayDeviceName;
    char _paServerVersion[32];

    int8_t* _playBuffer;
    size_t _playbackBufferSize;
    size_t _playbackBufferUnused;
    size_t _tempBufferSpace;
    int8_t* _recBuffer;
    size_t _recordBufferSize;
    size_t _recordBufferUsed;
    const void* _tempSampleData;
    size_t _tempSampleDataSize;
    int32_t _configuredLatencyPlay;
    int32_t _configuredLatencyRec;

    // PulseAudio
    uint16_t _paDeviceIndex;
    bool _paStateChanged;

    pa_threaded_mainloop* _paMainloop;
    pa_mainloop_api* _paMainloopApi;
    pa_context* _paContext;

    pa_stream* _recStream;
    pa_stream* _playStream;
    uint32_t _recStreamFlags;
    uint32_t _playStreamFlags;
    pa_buffer_attr _playBufferAttr;
    pa_buffer_attr _recBufferAttr;

    char _oldKeyState[32];
    Display* _XDisplay;
};

}

#endif  // MODULES_AUDIO_DEVICE_MAIN_SOURCE_LINUX_AUDIO_DEVICE_PULSE_LINUX_H_
