#include "etlx/core/optional.hpp"
#include "etlx/core/result.hpp"

#include <gtest/gtest.h>

namespace {

const etlx::error::Category TestCat{"test"};

TEST(Error, NoErrorIsOk) {
    EXPECT_TRUE(etlx::error::NoError.ok());
    EXPECT_FALSE(static_cast<bool>(etlx::error::NoError));
}

TEST(Error, MakeSetsFieldsAndIsTruthy) {
    auto e = etlx::error::Make(TestCat, 7, "boom");
    EXPECT_FALSE(e.ok());
    EXPECT_TRUE(static_cast<bool>(e));
    EXPECT_EQ(e.code, 7);
    EXPECT_STREQ(e.category->name, "test");
    EXPECT_EQ(e.message, "boom");
}

TEST(Error, MessageTruncatesToCapacity) {
    // ETLX_ERROR_MESSAGE_CAPACITY is 64; a longer message is clipped, not heap.
    etl::string<200> big;
    big.append(200, 'x');
    auto e = etlx::error::Make(TestCat, 1, etl::string_view{big.data(), big.size()});
    EXPECT_EQ(e.message.size(), ETLX_ERROR_MESSAGE_CAPACITY);
}

etlx::Result<int> Halve(int n) {
    if (n % 2 != 0) return etlx::Fail(TestCat, 2, "odd");
    return n / 2;
}

TEST(Result, CarriesValueOnSuccess) {
    auto r = Halve(10);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), 5);
}

TEST(Result, CarriesErrorOnFailure) {
    auto r = Halve(3);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, 2);
    EXPECT_EQ(r.error().message, "odd");
}

TEST(Status, OkAndFail) {
    EXPECT_TRUE(etlx::Ok().has_value());
    etlx::Status s = etlx::Fail(TestCat, 9, "nope");
    EXPECT_FALSE(s.has_value());
    EXPECT_EQ(s.error().code, 9);
}

TEST(Optional, BasicUse) {
    etlx::Optional<int> o = etlx::NullOpt;
    EXPECT_FALSE(o.has_value());
    o = 42;
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ(*o, 42);
}

} // namespace
