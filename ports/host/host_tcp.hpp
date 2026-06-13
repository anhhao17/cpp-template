#ifndef ETLX_PORTS_HOST_TCP_HPP
#define ETLX_PORTS_HOST_TCP_HPP

#include "etlx/net/net.hpp"

// Blocking POSIX TCP socket, implementing net::Socket. Works on the native host
// and on aarch64 Linux (and under qemu) since it only uses libc socket calls.
// A non-blocking / lwIP variant would implement the same interface for an RTOS
// target.
namespace etlx::ports::host {

class PosixTcpSocket final : public etlx::net::Socket {
public:
    PosixTcpSocket() = default;
    ~PosixTcpSocket() override { Close(); }

    etlx::Status         Connect(etl::string_view host, uint16_t port) override;
    etlx::Status         Send(etlx::ConstByteSpan src) override;
    etlx::Result<size_t> Recv(etlx::ByteSpan dst) override;
    void                 Close() override;

    int fd() const { return fd_; }

private:
    int fd_ = -1;
};

} // namespace etlx::ports::host

#endif // ETLX_PORTS_HOST_TCP_HPP
