// SE05x connection tests — runs against both the NXP JRCP socket simulator
// and real I2C hardware; skip automatically when EX_SSS_BOOT_SSS_PORT is unset.
#include <etlx/se/connection.hpp>
#include <etlx/se/object_store.hpp>
#include <gtest/gtest.h>
#include <cstdlib>
#include <memory>

using namespace etlx::se;

class SeConnectionTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        if (!getenv("EX_SSS_BOOT_SSS_PORT")) return;
        auto r = Connection::Open(nullptr);
        if (!r) { ADD_FAILURE() << "Connection::Open failed"; return; }
        conn_ = std::make_unique<Connection>(std::move(*r));
    }
    static void TearDownTestSuite() { conn_.reset(); }

protected:
    void SetUp() override {
        if (!getenv("EX_SSS_BOOT_SSS_PORT"))
            GTEST_SKIP() << "EX_SSS_BOOT_SSS_PORT not set";
        ASSERT_NE(conn_.get(), nullptr) << "Connection setup failed";
    }
    static Connection &Conn() { return *conn_; }
    inline static std::unique_ptr<Connection> conn_;
};

TEST_F(SeConnectionTest, Open_Succeeds) {
    // If we got here, SetUpTestSuite connected. Verify the handle is live.
    EXPECT_NE(Conn().session(), nullptr);
    EXPECT_NE(Conn().keystore(), nullptr);
}

TEST_F(SeConnectionTest, GetUid_Returns18ByteNonZeroId) {
    ObjectStore store(Conn());
    auto uid = store.GetUid();
    ASSERT_TRUE(uid.has_value()) << "GetUid failed";
    EXPECT_EQ(uid->size(), 18u);
    bool all_zero = true;
    for (uint8_t b : *uid) if (b) { all_zero = false; break; }
    EXPECT_FALSE(all_zero) << "UID is all zeros — chip not responding";
}

TEST_F(SeConnectionTest, GetUid_StableAcrossCalls) {
    ObjectStore store(Conn());
    auto uid1 = store.GetUid();
    auto uid2 = store.GetUid();
    ASSERT_TRUE(uid1.has_value());
    ASSERT_TRUE(uid2.has_value());
    EXPECT_EQ(*uid1, *uid2) << "UID changed between calls";
}
