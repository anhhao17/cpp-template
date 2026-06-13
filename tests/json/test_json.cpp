#include "etlx/json/json.hpp"

#include <gtest/gtest.h>

namespace json = etlx::json;

namespace {

// Parses into a fixed token array and returns a root view; fails the test on
// parse error. The token storage must outlive the returned view, so callers
// pass their own array.
json::Json Root(etl::string_view js, etlx::Span<json::Token> toks) {
    auto r = json::ParseToView(js, toks);
    EXPECT_TRUE(r.has_value()) << (r.has_value() ? "" : r.error().message.c_str());
    return r.has_value() ? r.value() : json::Json{};
}

TEST(Json, ScalarObjectFields) {
    json::Token toks[16];
    auto doc = Root("{\"name\":\"abc\",\"n\":42,\"ok\":true,\"x\":null}", toks);

    ASSERT_TRUE(doc.is_object());
    EXPECT_EQ(doc.size(), 4u);
    EXPECT_EQ(doc["name"].AsString(), "abc");

    auto n = doc["n"].AsInt();
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n.value(), 42);

    auto ok = doc["ok"].AsBool();
    ASSERT_TRUE(ok.has_value());
    EXPECT_TRUE(ok.value());

    EXPECT_TRUE(doc["x"].is_null());
}

TEST(Json, MissingMemberIsInvalid) {
    json::Token toks[8];
    auto doc = Root("{\"a\":1}", toks);
    EXPECT_FALSE(doc["nope"].valid());
    EXPECT_EQ(doc["nope"].type(), json::Type::Undefined);
}

TEST(Json, Arrays) {
    json::Token toks[16];
    auto doc = Root("{\"xs\":[10,20,30]}", toks);
    auto xs = doc["xs"];
    ASSERT_TRUE(xs.is_array());
    EXPECT_EQ(xs.size(), 3u);
    EXPECT_EQ(xs[0].AsInt().value(), 10);
    EXPECT_EQ(xs[2].AsInt().value(), 30);
    EXPECT_FALSE(xs[3].valid());  // out of range
}

TEST(Json, NestedObjects) {
    json::Token toks[24];
    auto doc = Root("{\"a\":{\"b\":{\"c\":7}}}", toks);
    EXPECT_EQ(doc["a"]["b"]["c"].AsInt().value(), 7);
}

TEST(Json, NegativeInt) {
    json::Token toks[8];
    auto doc = Root("{\"t\":-273}", toks);
    EXPECT_EQ(doc["t"].AsInt().value(), -273);
}

TEST(Json, WrongTypeAccessFails) {
    json::Token toks[8];
    auto doc = Root("{\"s\":\"text\"}", toks);
    auto n = doc["s"].AsInt();  // string is not a primitive number
    EXPECT_FALSE(n.has_value());
    EXPECT_EQ(n.error().code, json::WrongType);
}

TEST(Json, NoMemoryWhenTokensExhausted) {
    json::Token toks[2];  // too few for this document
    auto r = json::ParseToView("{\"a\":1,\"b\":2,\"c\":3}", toks);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, json::NoMemory);
}

TEST(Json, PartialInputRejected) {
    json::Token toks[8];
    auto r = json::ParseToView("{\"a\":", toks);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, json::Partial);
}

TEST(Json, MismatchedBracketRejected) {
    json::Token toks[8];
    auto r = json::ParseToView("{\"a\":[1,2}", toks);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, json::Invalid);
}

} // namespace
