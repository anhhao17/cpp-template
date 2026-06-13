// Example: drive the etlx http::Client over a Boost.Asio transport. A tiny
// loopback HTTP server returns a canned response; the client connects with an
// AsioTcpSocket, issues a GET, and the response is parsed by the same bounded
// http parser used everywhere else. Demonstrates that swapping the transport
// (POSIX -> Asio) needs no change to http. Build target: example_asio_http_get.
#include "asio/asio_tcp.hpp"
#include "etlx/http/http.hpp"

#include <cstdio>
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

    // Boost.Asio transport + the etlx HTTP client.
    etlx::ports::asio::AsioTcpSocket sock;
    int rc = 1;
    if (auto c = sock.Connect("127.0.0.1", port); !c) {
        std::printf("connect failed: %s\n", c.error().message.c_str());
    } else {
        etlx::http::Client client;
        uint8_t buf[512];
        auto resp = client.Get(sock, "127.0.0.1", "/health", etlx::ByteSpan{buf, sizeof(buf)});
        if (!resp) {
            std::printf("http error: %s\n", resp.error().message.c_str());
        } else {
            std::printf("status: %d %.*s\n", resp.value().status,
                        static_cast<int>(resp.value().reason.size()), resp.value().reason.data());
            std::printf("body:   %.*s\n", static_cast<int>(resp.value().body.size()),
                        reinterpret_cast<const char*>(resp.value().body.data()));
            rc = 0;
        }
    }

    server.join();
    ::close(listen_fd);
    return rc;
}
