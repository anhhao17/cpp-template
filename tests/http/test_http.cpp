#include "etlx/http/http.hpp"

#include <cstdint>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

namespace http = etlx::http;

namespace {

std::string Built(const http::Request& req) {
    uint8_t buf[512];
    auto n = http::BuildRequest(req, etlx::ByteSpan{buf, sizeof(buf)});
    EXPECT_TRUE(n.has_value());
    return std::string(reinterpret_cast<char*>(buf), n.has_value() ? n.value() : 0);
}

etlx::ConstByteSpan Bytes(const char* s) {
    return etlx::ConstByteSpan{reinterpret_cast<const uint8_t*>(s), std::strlen(s)};
}

TEST(BuildRequest, GetIncludesHostAndConnectionClose) {
    http::Request req;
    req.method = http::Method::Get;
    req.host   = "example.com";
    req.path   = "/manifest.json";

    auto s = Built(req);
    EXPECT_EQ(s.rfind("GET /manifest.json HTTP/1.1\r\n", 0), 0u);  // starts with
    EXPECT_NE(s.find("Host: example.com\r\n"), std::string::npos);
    EXPECT_NE(s.find("Connection: close\r\n"), std::string::npos);
    EXPECT_NE(s.find("\r\n\r\n"), std::string::npos);
}

TEST(BuildRequest, BodyAddsContentLength) {
    http::Request req;
    req.method = http::Method::Post;
    req.host   = "h";
    req.path   = "/x";
    req.body   = Bytes("hello");

    auto s = Built(req);
    EXPECT_EQ(s.rfind("POST /x HTTP/1.1\r\n", 0), 0u);
    EXPECT_NE(s.find("Content-Length: 5\r\n"), std::string::npos);
    EXPECT_NE(s.find("\r\n\r\nhello"), std::string::npos);
}

TEST(BuildRequest, BufferTooSmall) {
    http::Request req;
    req.host = "example.com";
    req.path = "/some/long/path";
    uint8_t tiny[8];
    auto n = http::BuildRequest(req, etlx::ByteSpan{tiny, sizeof(tiny)});
    ASSERT_FALSE(n.has_value());
    EXPECT_EQ(n.error().code, http::BufferTooSmall);
}

TEST(ParseResponse, StatusHeadersBody) {
    auto raw = Bytes(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello");
    auto r = http::ParseResponse(raw);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r.value().status, 200);
    EXPECT_EQ(r.value().reason, "OK");
    EXPECT_EQ(r.value().GetHeader("content-type"), "text/plain");  // case-insensitive
    ASSERT_TRUE(r.value().content_length.has_value());
    EXPECT_EQ(r.value().content_length.value(), 5u);
    etl::string_view body{reinterpret_cast<const char*>(r.value().body.data()),
                          r.value().body.size()};
    EXPECT_EQ(body, "hello");
}

TEST(ParseResponse, BodyTruncatedToContentLength) {
    auto raw = Bytes("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabcdef");
    auto r = http::ParseResponse(raw);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r.value().body.size(), 3u);  // ignores the trailing "def"
}

TEST(ParseResponse, IncompleteHeaders) {
    auto r = http::ParseResponse(Bytes("HTTP/1.1 200 OK\r\nContent-Len"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, http::Incomplete);
}

TEST(ParseResponse, Malformed) {
    auto r = http::ParseResponse(Bytes("NOTHTTP\r\n\r\n"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, http::Malformed);
}

// A Socket that hands back canned bytes in small chunks, exercising the recv
// loop in Client::Do.
class ChunkedMockSocket final : public etlx::net::Socket {
public:
    explicit ChunkedMockSocket(const char* data)
        : data_(reinterpret_cast<const uint8_t*>(data)), size_(std::strlen(data)) {}
    etlx::Status Connect(etl::string_view, uint16_t) override { return etlx::Ok(); }
    etlx::Status Send(etlx::ConstByteSpan) override { return etlx::Ok(); }
    etlx::Result<size_t> Recv(etlx::ByteSpan dst) override {
        size_t n = std::min<size_t>({dst.size(), size_ - pos_, 7});  // 7-byte chunks
        std::memcpy(dst.data(), data_ + pos_, n);
        pos_ += n;
        return n;
    }
    void Close() override {}

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
};

TEST(Client, GetParsesResponseAcrossChunks) {
    ChunkedMockSocket sock(
        "HTTP/1.1 204 No Content\r\n"
        "X-Test: yes\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "hello world");
    http::Client client;
    uint8_t buf[256];
    auto r = client.Get(sock, "h", "/", etlx::ByteSpan{buf, sizeof(buf)});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r.value().status, 204);
    EXPECT_EQ(r.value().GetHeader("X-Test"), "yes");
    etl::string_view body{reinterpret_cast<const char*>(r.value().body.data()),
                          r.value().body.size()};
    EXPECT_EQ(body, "hello world");
}

} // namespace
