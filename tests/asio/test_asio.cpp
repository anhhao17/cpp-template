#include "asio/asio_tcp.hpp"

#include <cstring>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

#if ETLX_TEST_WITH_TLS
#  include "etlx/net/tls.hpp"
#  include "tests/net/test_certs.hpp"
#  include "tests/support/tls_echo_server.hpp"
#endif

namespace {

// Plain TCP round-trip over the Boost.Asio transport, against an in-process
// loopback listener (connect completes at the handshake, so single-threaded).
TEST(AsioTcpSocket, LoopbackRoundTrip) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listen_fd, 0);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = 0;
    ASSERT_EQ(0, ::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));
    ASSERT_EQ(0, ::listen(listen_fd, 1));
    socklen_t len = sizeof(addr);
    ::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len);
    const uint16_t port = ntohs(addr.sin_port);

    etlx::ports::asio::AsioTcpSocket client;
    ASSERT_TRUE(client.Connect("127.0.0.1", port).has_value());

    int server_fd = ::accept(listen_fd, nullptr, nullptr);
    ASSERT_GE(server_fd, 0);

    const char* greeting = "asio-hello";
    ASSERT_EQ(static_cast<ssize_t>(std::strlen(greeting)),
              ::send(server_fd, greeting, std::strlen(greeting), 0));

    uint8_t buf[64];
    auto n = client.Recv(etlx::ByteSpan{buf, sizeof(buf)});
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), n.value()), "asio-hello");

    ASSERT_TRUE(client.Send(etlx::ConstByteSpan{
        reinterpret_cast<const uint8_t*>("pong"), 4}).has_value());
    char rbuf[8] = {};
    ASSERT_EQ(4, ::recv(server_fd, rbuf, sizeof(rbuf), 0));
    EXPECT_STREQ(rbuf, "pong");

    ::close(server_fd);
    auto eof = client.Recv(etlx::ByteSpan{buf, sizeof(buf)});
    ASSERT_TRUE(eof.has_value());
    EXPECT_EQ(eof.value(), 0u);  // peer close surfaces as 0-length read

    client.Close();
    ::close(listen_fd);
}

TEST(AsioTcpSocket, ConnectToBadHostFails) {
    etlx::ports::asio::AsioTcpSocket sock;
    EXPECT_FALSE(sock.Connect("no.such.host.invalid", 80).has_value());
}

#if ETLX_TEST_WITH_TLS
// The headline composition: mutual TLS over an Asio transport. TlsSocket wraps
// the AsioTcpSocket and presents a client certificate; the loopback server
// requires one. Same TlsSocket code as over PosixTcpSocket — only the
// transport differs.
TEST(AsioMutualTls, ClientCertOverAsioTransport) {
    using namespace etlx_test_certs;
    etlx_test_support::TlsEchoServer server(/*require_client_cert=*/true);
    const uint16_t port = server.Start();

    etlx::ports::asio::AsioTcpSocket asio_transport;
    etlx::net::TlsSocket             tls(asio_transport);

    etlx::net::TlsConfig cfg;
    cfg.ca_cert_pem     = kCaCertPem;
    cfg.client_cert_pem = kClientCertPem;
    cfg.client_key_pem  = kClientKeyPem;
    cfg.server_name     = "localhost";
    ASSERT_TRUE(tls.Configure(cfg).has_value());
    ASSERT_TRUE(tls.Connect("127.0.0.1", port).has_value());

    const char* msg = "mtls-over-asio";
    ASSERT_TRUE(tls.Send(etlx::ConstByteSpan{
        reinterpret_cast<const uint8_t*>(msg), std::strlen(msg)}).has_value());
    uint8_t buf[64];
    auto n = tls.Recv(etlx::ByteSpan{buf, sizeof(buf)});
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), n.value()), "mtls-over-asio");

    tls.Close();
    server.Wait();
    EXPECT_TRUE(server.handshake_ok());
}
#endif // ETLX_TEST_WITH_TLS

} // namespace
