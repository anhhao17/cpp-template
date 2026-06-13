// Example: drive the etlx http::Client over the selected TCP transport. A tiny
// loopback HTTP server returns a canned response; the client connects with an
// etlx::net::TcpSocket -- the POSIX socket or the Boost.Asio socket depending
// on the build-time ETLX_TRANSPORT setting -- issues a GET, and the response is
// parsed by the same bounded http parser. Swapping the transport needs no
// source change. Output via etlx::log. Build target: example_tcp_http_get.
#include "etlx/http/http.hpp"
#include "etlx/log/log.hpp"
#include "etlx/net/tcp_socket.hpp"
#include "host/host_io.hpp"

#include <cstring>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

void RunHttpServer(int listen_fd) {
    int conn = ::accept(listen_fd, nullptr, nullptr);
    if (conn < 0) return;
    char req[512];
    ::recv(conn, req, sizeof(req), 0);  // read and ignore the request
    static const char kResp[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 27\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"status\":\"up\",\"build\":42}";
    ::send(conn, kResp, std::strlen(kResp), 0);
    ::close(conn);
}

} // namespace

int main() {
    static etlx::ports::host::StderrLogSink sink;
    etlx::log::SetSink(&sink);

    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::listen(listen_fd, 1);
    socklen_t len = sizeof(addr);
    ::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len);
    const uint16_t port = ntohs(addr.sin_port);
    std::thread server(RunHttpServer, listen_fd);

    // The selected transport (POSIX or Asio) feeding the etlx HTTP client.
    etlx::net::TcpSocket sock;
    int rc = 1;
    if (auto c = sock.Connect("127.0.0.1", port); !c) {
        ETLX_LOG_ERROR("connect failed: %s", c.error().message.c_str());
    } else {
        etlx::http::Client client;
        uint8_t buf[512];
        auto resp = client.Get(sock, "127.0.0.1", "/health", etlx::ByteSpan{buf, sizeof(buf)});
        if (!resp) {
            ETLX_LOG_ERROR("http error: %s", resp.error().message.c_str());
        } else {
            ETLX_LOG_INFO("status: %d %.*s", resp.value().status,
                          static_cast<int>(resp.value().reason.size()), resp.value().reason.data());
            ETLX_LOG_INFO("body:   %.*s", static_cast<int>(resp.value().body.size()),
                          reinterpret_cast<const char*>(resp.value().body.data()));
            rc = 0;
        }
    }

    server.join();
    ::close(listen_fd);
    return rc;
}
