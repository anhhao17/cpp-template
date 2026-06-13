#include "host/host_tcp.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <etl/string.h>

namespace etlx::ports::host {

namespace net = etlx::net;

etlx::Status PosixTcpSocket::Connect(etl::string_view host, uint16_t port) {
    Close();

    // getaddrinfo wants NUL-terminated strings.
    etl::string<256> host_z;
    host_z.assign(host.data(), host.size());
    char port_z[8];
    std::snprintf(port_z, sizeof(port_z), "%u", static_cast<unsigned>(port));

    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    if (getaddrinfo(host_z.c_str(), port_z, &hints, &res) != 0 || res == nullptr) {
        return etlx::Fail(net::Category, net::ResolveFailed, "getaddrinfo failed");
    }

    int fd = -1;
    for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        return etlx::Fail(net::Category, net::ConnectFailed, "connect failed");
    }
    fd_ = fd;
    return etlx::Ok();
}

etlx::Status PosixTcpSocket::Send(etlx::ConstByteSpan src) {
    size_t sent = 0;
    while (sent < src.size()) {
        const ssize_t n = ::send(fd_, src.data() + sent, src.size() - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return etlx::Fail(net::Category, net::SendFailed, std::strerror(errno));
        }
        sent += static_cast<size_t>(n);
    }
    return etlx::Ok();
}

etlx::Result<size_t> PosixTcpSocket::Recv(etlx::ByteSpan dst) {
    for (;;) {
        const ssize_t n = ::recv(fd_, dst.data(), dst.size(), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return etlx::Fail(net::Category, net::RecvFailed, std::strerror(errno));
        }
        return static_cast<size_t>(n);  // 0 means the peer closed
    }
}

void PosixTcpSocket::Close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace etlx::ports::host
