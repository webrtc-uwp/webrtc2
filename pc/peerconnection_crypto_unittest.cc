/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/fakeportallocator.h"
#include "pc/mediasession.h"
#include "pc/peerconnectionunittestfixture.h"
#include "pc/test/fakertccertificategenerator.h"
#include "rtc_base/ptr_util.h"

namespace {

using RTCConfiguration = webrtc::PeerConnectionInterface::RTCConfiguration;
using ::testing::Values;
using ::testing::Combine;
using webrtc::MockCreateSessionDescriptionObserver;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionFactoryInterface;
using webrtc::SessionDescriptionInterface;

int kGenerateCertTimeout = 1000;

class PeerConnectionWrapperForCryptoUnitTest
    : public webrtc::PeerConnectionWrapper {
 public:
  PeerConnectionWrapperForCryptoUnitTest(
      rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory)
      : PeerConnectionWrapper(pc_factory) {}

  FakeRTCCertificateGenerator* fake_certificate_generator_ = nullptr;
};

class PeerConnectionCryptoUnitTest
    : public webrtc::PeerConnectionUnitTestFixture {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForCryptoUnitTest> WrapperPtr;

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    std::unique_ptr<FakeRTCCertificateGenerator> fake_certificate_generator;
    if (config.enable_dtls_srtp.value_or(false) &&
        config.certificates.empty()) {
      fake_certificate_generator.reset(new FakeRTCCertificateGenerator());
    }
    return CreatePeerConnection(config, std::move(fake_certificate_generator));
  }

  WrapperPtr CreatePeerConnection(
      const RTCConfiguration& config,
      std::unique_ptr<FakeRTCCertificateGenerator> fake_certificate_generator) {
    auto fake_port_allocator = rtc::MakeUnique<cricket::FakePortAllocator>(
        rtc::Thread::Current(), nullptr);
    auto wrapper =
        rtc::MakeUnique<PeerConnectionWrapperForCryptoUnitTest>(pc_factory_);
    wrapper->fake_certificate_generator_ = fake_certificate_generator.get();
    if (!wrapper->InitializePeerConnection(
            config, std::move(fake_port_allocator),
            std::move(fake_certificate_generator))) {
      return nullptr;
    }
    return wrapper;
  }

  // Accepts the same arguments as CreatePeerConnection and adds default audio
  // and video tracks.
  template <typename... Args>
  WrapperPtr CreatePeerConnectionWithAudioVideo(Args&&... args) {
    auto wrapper = CreatePeerConnection(std::forward<Args>(args)...);
    if (!wrapper) {
      return nullptr;
    }
    wrapper->AddAudioVideoStream("s", "a", "v");
    return wrapper;
  }

  typedef std::function<bool(const cricket::ContentInfo*,
                             const cricket::TransportInfo*)>
      SdpContentPredicate;

  bool SdpContentsAll(SdpContentPredicate pred,
                      const cricket::SessionDescription* desc) {
    for (const auto& content : desc->contents()) {
      const auto* transport_info = desc->GetTransportInfoByName(content.name);
      if (!pred(&content, transport_info)) {
        return false;
      }
    }
    return true;
  }

  bool SdpContentsNone(SdpContentPredicate pred,
                       const cricket::SessionDescription* desc) {
    return SdpContentsAll(std::not2(pred), desc);
  }

  SdpContentPredicate HaveDtlsFingerprint() {
    return [](const cricket::ContentInfo* content,
              const cricket::TransportInfo* transport) {
      return transport->description.identity_fingerprint != nullptr;
    };
  }

  SdpContentPredicate HaveSdesCryptos() {
    return [](const cricket::ContentInfo* content,
              const cricket::TransportInfo* transport) {
      const auto* media_desc =
          static_cast<const cricket::MediaContentDescription*>(
              content->description);
      return !media_desc->cryptos().empty();
    };
  }

  SdpContentPredicate HaveProtocol(const std::string& protocol) {
    return [protocol](const cricket::ContentInfo* content,
                      const cricket::TransportInfo* transport) {
      const auto* media_desc =
          static_cast<const cricket::MediaContentDescription*>(
              content->description);
      return media_desc->protocol() == protocol;
    };
  }

  SdpContentPredicate HaveSdesGcmCryptos(size_t num_crypto_suites) {
    return [num_crypto_suites](const cricket::ContentInfo* content,
                               const cricket::TransportInfo* transport) {
      const auto* media_desc =
          static_cast<const cricket::MediaContentDescription*>(
              content->description);
      if (media_desc->cryptos().size() != num_crypto_suites) {
        return false;
      }
      const cricket::CryptoParams first_params = media_desc->cryptos()[0];
      return first_params.key_params.size() == 67U &&
             first_params.cipher_suite == "AEAD_AES_256_GCM";
    };
  }

  typedef std::function<void(cricket::ContentInfo*, cricket::TransportInfo*)>
      SdpContentMutator;

  void SdpContentsForEach(SdpContentMutator fn,
                          cricket::SessionDescription* desc) {
    for (auto& content : desc->contents()) {
      auto* transport_info = desc->GetTransportInfoByName(content.name);
      fn(&content, transport_info);
    }
  }

  std::unique_ptr<SessionDescriptionInterface> SdpMutateContents(
      SdpContentMutator fn,
      std::unique_ptr<SessionDescriptionInterface> sdesc) {
    SdpContentsForEach(fn, sdesc->description());
    return sdesc;
  }

  SdpContentMutator RemoveSdesCryptos() {
    return [](cricket::ContentInfo* content,
              cricket::TransportInfo* transport) {
      auto* media_desc =
          static_cast<cricket::MediaContentDescription*>(content->description);
      media_desc->set_cryptos({});
    };
  }

  SdpContentMutator RemoveDtlsFingerprint() {
    return
        [](cricket::ContentInfo* content, cricket::TransportInfo* transport) {
          transport->description.identity_fingerprint.reset();
        };
  }
};

// When DTLS is enabled, the SDP offer/answer should have a DTLS fingerprint and
// no SDES cryptos.
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInOfferWhenDtlsEnabled) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveDtlsFingerprint(), offer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), offer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolDtlsSavpf),
                             offer->description()));
}
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInAnswerWhenDtlsEnabled) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveDtlsFingerprint(), answer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), answer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolDtlsSavpf),
                             answer->description()));
}

// When DTLS is disabled, the SDP offer/answer should include SDES cryptos and
// should not have a DTLS fingerprint.
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInOfferWhenDtlsDisabled) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesCryptos(), offer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), offer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolSavpf),
                             offer->description()));
}
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInAnswerWhenDtlsDisabled) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesCryptos(), answer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), answer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolSavpf),
                             answer->description()));
}

// When encryption is disabled, the SDP offer/answer should have neither a DTLS
// fingerprint nor any SDES crypto options.
TEST_F(PeerConnectionCryptoUnitTest,
       CorrectCryptoInOfferWhenEncryptionDisabled) {
  PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), offer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), offer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolAvpf),
                             offer->description()));
}
TEST_F(PeerConnectionCryptoUnitTest,
       CorrectCryptoInAnswerWhenEncryptionDisabled) {
  PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), answer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), answer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolAvpf),
                             answer->description()));
}

// When DTLS is disabled and GCM cipher suites are enabled, the SDP offer/answer
// should have the correct ciphers in the SDES crypto options.
// With GCM cipher suites enabled, there will be 3 cryptos in the offer and 1
// in the answer.
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInOfferWhenSdesAndGcm) {
  PeerConnectionFactoryInterface::Options options;
  options.crypto_options.enable_gcm_crypto_suites = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesGcmCryptos(3), offer->description()));
}
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInAnswerWhenSdesAndGcm) {
  PeerConnectionFactoryInterface::Options options;
  options.crypto_options.enable_gcm_crypto_suites = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesGcmCryptos(1), answer->description()));
}

TEST_F(PeerConnectionCryptoUnitTest, CanSetSdesGcmRemoteOfferAndLocalAnswer) {
  PeerConnectionFactoryInterface::Options options;
  options.crypto_options.enable_gcm_crypto_suites = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(callee->SetLocalDescription(std::move(answer)));
}

// The following group tests that two PeerConnections can successfully exchange
// an offer/answer when DTLS is off and that they will refuse any offer/answer
// applied locally/remotely if it does not include SDES cryptos.
TEST_F(PeerConnectionCryptoUnitTest, ExchangeOfferAnswerWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetLocalOfferWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveSdesCryptos(), offer->description());

  EXPECT_FALSE(caller->SetLocalDescription(std::move(offer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetRemoteOfferWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveSdesCryptos(), offer->description());

  EXPECT_FALSE(callee->SetRemoteDescription(std::move(offer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetLocalAnswerWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswer();
  SdpContentsForEach(RemoveSdesCryptos(), answer->description());
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetRemoteAnswerWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswerAndSetAsLocal();
  SdpContentsForEach(RemoveSdesCryptos(), answer->description());

  EXPECT_FALSE(caller->SetRemoteDescription(std::move(answer)));
}

// The following group tests that two PeerConnections can successfully exchange
// an offer/answer when DTLS is on and that they will refuse any offer/answer
// applied locally/remotely if it does not include a DTLS fingerprint.
TEST_F(PeerConnectionCryptoUnitTest, ExchangeOfferAnswerWhenDtlsOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetLocalOfferWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveDtlsFingerprint(), offer->description());

  EXPECT_FALSE(caller->SetLocalDescription(std::move(offer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetRemoteOfferWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveDtlsFingerprint(), offer->description());

  EXPECT_FALSE(callee->SetRemoteDescription(std::move(offer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetLocalAnswerWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswer();
  SdpContentsForEach(RemoveDtlsFingerprint(), answer->description());
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetRemoteAnswerWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswerAndSetAsLocal();
  SdpContentsForEach(RemoveDtlsFingerprint(), answer->description());

  EXPECT_FALSE(caller->SetRemoteDescription(std::move(answer)));
}

// Test that an offer/answer can be exchanged when encryption is disabled.
TEST_F(PeerConnectionCryptoUnitTest, ExchangeOfferAnswerWhenNoEncryption) {
  PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}

// Tests that a DTLS call can be established when the certificate is specified
// in the PeerConnection config and no certificate generator is specified.
TEST_F(PeerConnectionCryptoUnitTest,
       ExchangeOfferAnswerWhenDtlsCertificateInConfig) {
  RTCConfiguration caller_config;
  caller_config.enable_dtls_srtp.emplace(true);
  caller_config.certificates.push_back(
      FakeRTCCertificateGenerator::GenerateCertificate());
  auto caller = CreatePeerConnectionWithAudioVideo(caller_config);

  RTCConfiguration callee_config;
  callee_config.enable_dtls_srtp.emplace(true);
  callee_config.certificates.push_back(
      FakeRTCCertificateGenerator::GenerateCertificate());
  auto callee = CreatePeerConnectionWithAudioVideo(callee_config);

  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}

// The following parameterized test verifies that CreateOffer/CreateAnswer
// returns successfully (or with failure if the underlying certificate generator
// fails) no matter when the DTLS certificate is generated.

// Whether the test will call CreateOffer or CreateAnswer.
enum class SdpType { kOffer, kAnswer };
std::ostream& operator<<(std::ostream& out, SdpType value) {
  switch (value) {
    case SdpType::kOffer:
      return out << "offer";
    case SdpType::kAnswer:
      return out << "answer";
  }
}

// Whether the certificate will be generated before calling CreateOffer or
// while CreateOffer is executing.
enum class CertGenTime { kBefore, kDuring };
std::ostream& operator<<(std::ostream& out, CertGenTime value) {
  switch (value) {
    case CertGenTime::kBefore:
      return out << "before";
    case CertGenTime::kDuring:
      return out << "during";
  }
}

// Whether the fake certificate generator will produce a certificate or fail.
enum class CertGenResult { kSucceed, kFail };
std::ostream& operator<<(std::ostream& out, CertGenResult value) {
  switch (value) {
    case CertGenResult::kSucceed:
      return out << "succeed";
    case CertGenResult::kFail:
      return out << "fail";
  }
}

class PeerConnectionCryptoDtlsCertGenUnitTest
    : public PeerConnectionCryptoUnitTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<SdpType, CertGenTime, CertGenResult, int>> {
 protected:
  PeerConnectionCryptoDtlsCertGenUnitTest() {
    sdp_type_ = ::testing::get<0>(GetParam());
    cert_gen_time_ = ::testing::get<1>(GetParam());
    cert_gen_result_ = ::testing::get<2>(GetParam());
    concurrent_calls_ = ::testing::get<3>(GetParam());
  }

  SdpType sdp_type_;
  CertGenTime cert_gen_time_;
  CertGenResult cert_gen_result_;
  int concurrent_calls_;
};

TEST_P(PeerConnectionCryptoDtlsCertGenUnitTest, TestCertificateGeneration) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto fake_certificate_generator =
      rtc::MakeUnique<FakeRTCCertificateGenerator>();
  fake_certificate_generator->set_should_fail(cert_gen_result_ ==
                                              CertGenResult::kFail);
  fake_certificate_generator->set_should_wait(cert_gen_time_ ==
                                              CertGenTime::kDuring);
  WrapperPtr pc;
  if (sdp_type_ == SdpType::kOffer) {
    pc = CreatePeerConnectionWithAudioVideo(
        config, std::move(fake_certificate_generator));
  } else {
    auto caller = CreatePeerConnectionWithAudioVideo(config);
    pc = CreatePeerConnectionWithAudioVideo(
        config, std::move(fake_certificate_generator));
    pc->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  }
  if (cert_gen_time_ == CertGenTime::kBefore) {
    EXPECT_TRUE_WAIT(
        pc->fake_certificate_generator_->generated_certificates() +
                pc->fake_certificate_generator_->generated_failures() >
            0,
        kGenerateCertTimeout);
  } else {
    EXPECT_EQ(pc->fake_certificate_generator_->generated_certificates(), 0);
    pc->fake_certificate_generator_->set_should_wait(false);
  }
  rtc::scoped_refptr<MockCreateSessionDescriptionObserver>
      observers[concurrent_calls_];
  for (auto& observer : observers) {
    observer =
        new rtc::RefCountedObject<MockCreateSessionDescriptionObserver>();
    if (sdp_type_ == SdpType::kOffer) {
      pc->pc()->CreateOffer(observer, nullptr);
    } else {
      pc->pc()->CreateAnswer(observer, nullptr);
    }
  }
  for (auto& observer : observers) {
    EXPECT_TRUE_WAIT(observer->called(), 1000);
    if (cert_gen_result_ == CertGenResult::kSucceed) {
      ASSERT_TRUE(observer->result());
    } else {
      ASSERT_FALSE(observer->result());
    }
  }
}

INSTANTIATE_TEST_CASE_P(
    PeerConnectionCryptoUnitTest,
    PeerConnectionCryptoDtlsCertGenUnitTest,
    Combine(Values(SdpType::kOffer, SdpType::kAnswer),
            Values(CertGenTime::kBefore, CertGenTime::kDuring),
            Values(CertGenResult::kSucceed, CertGenResult::kFail),
            Values(1, 3)));

}  // namespace
