// SE05x ObjectStore tests — transport-agnostic (socket simulator or I2C hardware).
#include <etlx/se/connection.hpp>
#include <etlx/se/object_store.hpp>
#include <gtest/gtest.h>
#include <cstdlib>
#include <memory>

using namespace etlx::se;

// Test-only object IDs — does not conflict with factory provisioning range.
static constexpr uint32_t kTestBlobId = 0xFE00FF10u;
static constexpr uint32_t kTestCertId = 0xFE00FF11u;

// Minimal valid DER-encoded X.509 stub for WriteCert tests.
// Real certs are thousands of bytes; SE05x stores raw bytes so any blob works.
static const uint8_t kFakeCertDer[] = {
    0x30, 0x0a,  // SEQUENCE(10)
    0x02, 0x01, 0x01,  // INTEGER 1 (version stub)
    0x02, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05,  // INTEGER serial
};

class SeObjectStoreTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        if (!getenv("EX_SSS_BOOT_SSS_PORT")) return;
        auto r = Connection::Open(nullptr);
        if (!r) { ADD_FAILURE() << "Connection::Open failed"; return; }
        conn_ = std::make_unique<Connection>(std::move(*r));
        // Clean up objects from a previous (possibly crashed) run.
        ObjectStore cleanup(*conn_);
        cleanup.Erase(kTestBlobId);
        cleanup.Erase(kTestCertId);
    }

    static void TearDownTestSuite() {
        if (conn_) {
            ObjectStore cleanup(*conn_);
            cleanup.Erase(kTestBlobId);
            cleanup.Erase(kTestCertId);
        }
        conn_.reset();
    }

protected:
    void SetUp() override {
        if (!getenv("EX_SSS_BOOT_SSS_PORT"))
            GTEST_SKIP() << "EX_SSS_BOOT_SSS_PORT not set";
        ASSERT_NE(conn_.get(), nullptr) << "Connection setup failed";
        // Each test gets a fresh store view; objects may persist between tests.
        store_ = std::make_unique<ObjectStore>(*conn_);
    }

    void TearDown() override {
        // Ensure blob and cert are cleaned up between tests so they don't
        // interfere with the next test in the suite.
        if (store_) {
            store_->Erase(kTestBlobId);
            store_->Erase(kTestCertId);
        }
        store_.reset();
    }

    static Connection &Conn() { return *conn_; }
    ObjectStore &Store() { return *store_; }

    inline static std::unique_ptr<Connection> conn_;
    std::unique_ptr<ObjectStore> store_;
};

TEST_F(SeObjectStoreTest, WriteBinary_And_ReadBack) {
    static const uint8_t kData[] = {0xDE, 0xAD, 0xBE, 0xEF,
                                     0x01, 0x02, 0x03, 0x04};
    auto wr = Store().WriteBinary(kTestBlobId, kData, sizeof(kData), false);
    ASSERT_TRUE(wr.has_value()) << "WriteBinary failed";
    EXPECT_TRUE(*wr) << "Expected write to succeed (first time)";

    auto rd = Store().ReadBinary(kTestBlobId);
    ASSERT_TRUE(rd.has_value()) << "ReadBinary failed";
    ASSERT_EQ(rd->size(), sizeof(kData));
    EXPECT_EQ(std::memcmp(rd->data(), kData, sizeof(kData)), 0);
}

TEST_F(SeObjectStoreTest, WriteBinary_NoForce_SkipsExisting) {
    static const uint8_t kData1[] = {0x11, 0x22};
    static const uint8_t kData2[] = {0x33, 0x44};
    auto wr1 = Store().WriteBinary(kTestBlobId, kData1, sizeof(kData1), false);
    ASSERT_TRUE(wr1.has_value() && *wr1);

    // Second write without force — must return false (no-op).
    auto wr2 = Store().WriteBinary(kTestBlobId, kData2, sizeof(kData2), false);
    ASSERT_TRUE(wr2.has_value());
    EXPECT_FALSE(*wr2) << "Expected no-op write when force=false and object exists";

    // Data must still be kData1.
    auto rd = Store().ReadBinary(kTestBlobId);
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(std::memcmp(rd->data(), kData1, sizeof(kData1)), 0);
}

TEST_F(SeObjectStoreTest, WriteBinary_Force_Overwrites) {
    static const uint8_t kData1[] = {0xAA, 0xBB};
    static const uint8_t kData2[] = {0xCC, 0xDD};
    Store().WriteBinary(kTestBlobId, kData1, sizeof(kData1), false);
    auto wr = Store().WriteBinary(kTestBlobId, kData2, sizeof(kData2), true);
    ASSERT_TRUE(wr.has_value() && *wr);
    auto rd = Store().ReadBinary(kTestBlobId);
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(std::memcmp(rd->data(), kData2, sizeof(kData2)), 0);
}

TEST_F(SeObjectStoreTest, Exists_FalseBeforeWrite_TrueAfterWrite) {
    EXPECT_FALSE(Store().Exists(kTestBlobId));
    static const uint8_t kData[] = {0x42};
    Store().WriteBinary(kTestBlobId, kData, sizeof(kData), false);
    EXPECT_TRUE(Store().Exists(kTestBlobId));
}

TEST_F(SeObjectStoreTest, Erase_RemovesObject) {
    static const uint8_t kData[] = {0x55};
    Store().WriteBinary(kTestBlobId, kData, sizeof(kData), false);
    ASSERT_TRUE(Store().Exists(kTestBlobId));
    auto st = Store().Erase(kTestBlobId);
    ASSERT_TRUE(st.has_value()) << "Erase failed";
    EXPECT_FALSE(Store().Exists(kTestBlobId));
}

TEST_F(SeObjectStoreTest, WriteCert_And_ReadBack) {
    auto st = Store().WriteCert(kTestCertId, kFakeCertDer, sizeof(kFakeCertDer));
    ASSERT_TRUE(st.has_value()) << "WriteCert failed";
    auto rd = Store().ReadBinary(kTestCertId);
    ASSERT_TRUE(rd.has_value()) << "ReadBinary after WriteCert failed";
    ASSERT_EQ(rd->size(), sizeof(kFakeCertDer));
    EXPECT_EQ(std::memcmp(rd->data(), kFakeCertDer, sizeof(kFakeCertDer)), 0);
}

TEST_F(SeObjectStoreTest, WriteCert_Overwrites_ExistingCert) {
    static const uint8_t kCert1[] = {0x30, 0x02, 0x01, 0x01};
    static const uint8_t kCert2[] = {0x30, 0x02, 0x01, 0x02};
    Store().WriteCert(kTestCertId, kCert1, sizeof(kCert1));
    auto st = Store().WriteCert(kTestCertId, kCert2, sizeof(kCert2));
    ASSERT_TRUE(st.has_value()) << "Second WriteCert failed";
    auto rd = Store().ReadBinary(kTestCertId);
    ASSERT_TRUE(rd.has_value());
    EXPECT_EQ(std::memcmp(rd->data(), kCert2, sizeof(kCert2)), 0);
}
