// Example: build an HTTP GET, parse a response, and stream the JSON body into
// the json reader, the full check-update path. To keep it deterministic and
// offline, the transport is a canned in-memory socket; swap in
// ports::host::PosixTcpSocket and Client::Get(sock, host, path, buf) to fetch a
// real URL. Build target: example_http_get.
#include "etlx/http/http.hpp"
#include "etlx/json/json.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

// A net::Socket that ignores what is sent and replays a fixed response.
class CannedSocket final : public etlx::net::Socket {
public:
    explicit CannedSocket(const char* response)
        : data_(reinterpret_cast<const uint8_t*>(response)),
          size_(std::strlen(response)) {}

    etlx::Status Connect(etl::string_view, uint16_t) override { return etlx::Ok(); }
    etlx::Status Send(etlx::ConstByteSpan) override { return etlx::Ok(); }
    etlx::Result<size_t> Recv(etlx::ByteSpan dst) override {
        const size_t n = std::min(dst.size(), size_ - pos_);
        std::memcpy(dst.data(), data_ + pos_, n);
        pos_ += n;
        return n;
    }
    void Close() override {}

private:
    const uint8_t* data_;
    size_t         size_;
    size_t         pos_ = 0;
};

} // namespace

int main() {
    static const char kResponse[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 44\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"artifact_name\":\"img-1.2.0\",\"version\":7}";

    CannedSocket sock(kResponse);
    etlx::http::Client client;

    uint8_t recv_buf[512];
    auto resp = client.Get(sock, "device.example.com", "/manifest.json",
                           etlx::ByteSpan{recv_buf, sizeof(recv_buf)});
    if (!resp) {
        std::printf("http error: %s\n", resp.error().message.c_str());
        return 1;
    }

    std::printf("status: %d %.*s\n", resp.value().status,
                static_cast<int>(resp.value().reason.size()),
                resp.value().reason.data());
    auto ctype = resp.value().GetHeader("Content-Type");
    std::printf("content-type: %.*s\n", static_cast<int>(ctype.size()), ctype.data());

    // Stream the body into the JSON reader.
    etl::string_view body{reinterpret_cast<const char*>(resp.value().body.data()),
                          resp.value().body.size()};
    etlx::json::Token tokens[16];
    auto doc = etlx::json::ParseToView(body, etlx::Span<etlx::json::Token>{tokens, 16});
    if (!doc) {
        std::printf("json error: %s\n", doc.error().message.c_str());
        return 1;
    }
    auto name = doc.value()["artifact_name"].AsString();
    auto ver  = doc.value()["version"].AsInt();
    std::printf("manifest artifact_name = %.*s\n",
                static_cast<int>(name.size()), name.data());
    if (ver) std::printf("manifest version       = %lld\n",
                         static_cast<long long>(ver.value()));
    return 0;
}
