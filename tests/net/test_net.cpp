#include "host/host_tcp.hpp"

#include <cstdint>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

namespace {

// Drives the real POSIX socket port against an in-process loopback listener.
// connect() completes at the TCP handshake (kernel backlog), so this works
// single-threaded, including under qemu user-mode networking.
TEST(PosixTcpSocket, LoopbackRoundTrip) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listen_fd, 0);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = 0;  // let the kernel pick a free port
    ASSERT_EQ(0, ::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));
    ASSERT_EQ(0, ::listen(listen_fd, 1));

    socklen_t len = sizeof(addr);
    ASSERT_EQ(0, ::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len));
    const uint16_t port = ntohs(addr.sin_port);

    etlx::ports::host::PosixTcpSocket client;
    ASSERT_TRUE(client.Connect("127.0.0.1", port).has_value());

    int server_fd = ::accept(listen_fd, nullptr, nullptr);
    ASSERT_GE(server_fd, 0);

    // Server -> client.
    const char* greeting = "hello-from-server";
    ASSERT_EQ(static_cast<ssize_t>(std::strlen(greeting)),
              ::send(server_fd, greeting, std::strlen(greeting), 0));

    uint8_t buf[64];
    auto n = client.Recv(etlx::ByteSpan{buf, sizeof(buf)});
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), n.value()), "hello-from-server");

    // Client -> server.
    ASSERT_TRUE(client.Send(etlx::ConstByteSpan{
        reinterpret_cast<const uint8_t*>("ping"), 4}).has_value());
    char rbuf[8] = {};
    ASSERT_EQ(4, ::recv(server_fd, rbuf, sizeof(rbuf), 0));
    EXPECT_STREQ(rbuf, "ping");

    // Peer close is surfaced as a 0-length read.
    ::close(server_fd);
    auto eof = client.Recv(etlx::ByteSpan{buf, sizeof(buf)});
    ASSERT_TRUE(eof.has_value());
    EXPECT_EQ(eof.value(), 0u);

    client.Close();
    ::close(listen_fd);
}

TEST(PosixTcpSocket, ConnectToBadHostFails) {
    etlx::ports::host::PosixTcpSocket sock;
    auto r = sock.Connect("no.such.host.invalid", 80);
    EXPECT_FALSE(r.has_value());
}

} // namespace
