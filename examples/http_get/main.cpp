// Example: build an HTTP GET, parse a response, and stream the JSON body into
// the json reader, the full check-update path. To keep it deterministic and
// offline, the transport is a canned in-memory socket; swap in
// etlx::net::TcpSocket and Client::Get(sock, host, path, buf) to fetch a real
// URL. Output via etlx::log. Build target: example_http_get.
#include "etlx/http/http.hpp"
#include "etlx/json/json.hpp"
#include "etlx/log/log.hpp"
#include "host/host_io.hpp"

#include <cstdint>
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
    static etlx::ports::host::StderrLogSink sink;
    etlx::log::SetSink(&sink);

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
        ETLX_LOG_ERROR("http error: %s", resp.error().message.c_str());
        return 1;
    }

    ETLX_LOG_INFO("status: %d %.*s", resp.value().status,
                  static_cast<int>(resp.value().reason.size()), resp.value().reason.data());
    auto ctype = resp.value().GetHeader("Content-Type");
    ETLX_LOG_INFO("content-type: %.*s", static_cast<int>(ctype.size()), ctype.data());

    // Stream the body into the JSON reader.
    etl::string_view body{reinterpret_cast<const char*>(resp.value().body.data()),
                          resp.value().body.size()};
    etlx::json::Token tokens[16];
    auto doc = etlx::json::ParseToView(body, etlx::Span<etlx::json::Token>{tokens, 16});
    if (!doc) {
        ETLX_LOG_ERROR("json error: %s", doc.error().message.c_str());
        return 1;
    }
    auto name = doc.value()["artifact_name"].AsString();
    auto ver  = doc.value()["version"].AsInt();
    ETLX_LOG_INFO("manifest artifact_name = %.*s", static_cast<int>(name.size()), name.data());
    if (ver) ETLX_LOG_INFO("manifest version       = %lld", static_cast<long long>(ver.value()));
    return 0;
}
