// SE05x RSA key tests — transport-agnostic (socket simulator or I2C hardware).
// Generates one RSA-2048 key in SetUpTestSuite, shares it across all tests,
// erases it in TearDownTestSuite.
#include <etlx/se/connection.hpp>
#include <etlx/se/object_store.hpp>
#include <etlx/se/rsa_key.hpp>
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <memory>

using namespace etlx::se;

// Test-only object ID — does not conflict with factory provisioning range.
static constexpr uint32_t kTestRsaKeyId = 0xFE00FF01u;

// Deterministic 32-byte "hash" used as the sign/verify input.
static const uint8_t kTestDigest[32] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
};

static const uint8_t kOtherDigest[32] = {
    0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8,
    0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0xf0,
    0xef, 0xee, 0xed, 0xec, 0xeb, 0xea, 0xe9, 0xe8,
    0xe7, 0xe6, 0xe5, 0xe4, 0xe3, 0xe2, 0xe1, 0xe0,
};

class SeRsaKeyTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        if (!getenv("EX_SSS_BOOT_SSS_PORT")) return;
        auto r = Connection::Open(nullptr);
        if (!r) { ADD_FAILURE() << "Connection::Open failed"; return; }
        conn_ = std::make_unique<Connection>(std::move(*r));

        // Clean up any leftover key from a previous (possibly crashed) run.
        {
            ObjectStore cleanup(*conn_);
            cleanup.Erase(kTestRsaKeyId);  // no-op if absent
        }

        // Generate the test key (kept open for the lifetime of this suite).
        auto k = RsaKey::Generate(*conn_, kTestRsaKeyId, RsaBits::k2048, KeyPolicy::Full);
        if (!k) { ADD_FAILURE() << "RsaKey::Generate failed"; return; }
        key_ = std::make_unique<RsaKey>(std::move(*k));
    }

    static void TearDownTestSuite() {
        key_.reset();
        if (conn_) {
            ObjectStore cleanup(*conn_);
            cleanup.Erase(kTestRsaKeyId);
        }
        conn_.reset();
    }

protected:
    void SetUp() override {
        if (!getenv("EX_SSS_BOOT_SSS_PORT"))
            GTEST_SKIP() << "EX_SSS_BOOT_SSS_PORT not set";
        ASSERT_NE(conn_.get(), nullptr) << "Connection setup failed";
        ASSERT_NE(key_.get(), nullptr) << "Key generation failed";
    }

    static Connection &Conn() { return *conn_; }
    static RsaKey &Key() { return *key_; }

    inline static std::unique_ptr<Connection> conn_;
    inline static std::unique_ptr<RsaKey> key_;
};

TEST_F(SeRsaKeyTest, Sign_ProducesRsa2048Length) {
    auto sig = Key().Sign(kTestDigest, sizeof(kTestDigest));
    ASSERT_TRUE(sig.has_value()) << "Sign failed";
    EXPECT_EQ(sig->size(), 256u) << "RSA-2048 signature must be 256 bytes";
}

TEST_F(SeRsaKeyTest, Verify_AcceptsOwnSignature) {
    auto sig = Key().Sign(kTestDigest, sizeof(kTestDigest));
    ASSERT_TRUE(sig.has_value());
    auto v = Key().Verify(kTestDigest, sizeof(kTestDigest), sig->data(), sig->size());
    ASSERT_TRUE(v.has_value()) << "Verify hardware error";
    EXPECT_TRUE(v.value());
}

TEST_F(SeRsaKeyTest, Verify_RejectsWrongDigest) {
    auto sig = Key().Sign(kTestDigest, sizeof(kTestDigest));
    ASSERT_TRUE(sig.has_value());
    // Signature over kTestDigest must not verify against kOtherDigest.
    auto v = Key().Verify(kOtherDigest, sizeof(kOtherDigest), sig->data(), sig->size());
    ASSERT_TRUE(v.has_value()) << "Verify hardware error";
    EXPECT_FALSE(v.value());
}

TEST_F(SeRsaKeyTest, Verify_RejectsCorruptedSignature) {
    auto sig = Key().Sign(kTestDigest, sizeof(kTestDigest));
    ASSERT_TRUE(sig.has_value());
    // Flip one bit in the signature.
    (*sig)[0] ^= 0x01u;
    auto v = Key().Verify(kTestDigest, sizeof(kTestDigest), sig->data(), sig->size());
    ASSERT_TRUE(v.has_value()) << "Verify hardware error";
    EXPECT_FALSE(v.value());
}

TEST_F(SeRsaKeyTest, PublicKeyDer_HasRsa2048SpkiSize) {
    auto spki = Key().PublicKeyDer();
    ASSERT_TRUE(spki.has_value()) << "PublicKeyDer failed";
    // RSA-2048 SubjectPublicKeyInfo DER: typically 294 bytes.
    EXPECT_GT(spki->size(), 200u);
    EXPECT_LE(spki->size(), 400u);
    // DER sequence tag.
    EXPECT_EQ((*spki)[0], 0x30u) << "SPKI must start with DER SEQUENCE tag";
}

TEST_F(SeRsaKeyTest, ModulusBytes_Is256ForRsa2048) {
    EXPECT_EQ(Key().modulus_bytes(), 256u);
}

TEST_F(SeRsaKeyTest, MakeCsr_ReturnsPemWithHeader) {
    auto csr = Key().MakeCsr("CN=test-device,O=Test,C=US");
    ASSERT_TRUE(csr.has_value()) << "MakeCsr failed";
    // PEM wrapping — BEGIN header must be present.
    EXPECT_NE(csr->find("CERTIFICATE REQUEST"), csr->npos)
        << "CSR PEM must contain CERTIFICATE REQUEST marker";
}

TEST_F(SeRsaKeyTest, Open_BindsExistingKey) {
    // The test key was generated in SetUpTestSuite. Verify we can re-open it.
    auto k = RsaKey::Open(Conn(), kTestRsaKeyId);
    ASSERT_TRUE(k.has_value()) << "RsaKey::Open failed";
    EXPECT_EQ(k->modulus_bytes(), 256u);
}
