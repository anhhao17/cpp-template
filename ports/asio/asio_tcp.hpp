#ifndef ETLX_PORTS_ASIO_TCP_HPP
#define ETLX_PORTS_ASIO_TCP_HPP

#include "etlx/net/net.hpp"

// Boost.Asio TCP socket, implementing net::Socket. An alternative to the
// PosixTcpSocket port for host/Linux targets, modelled on the fact that the
// real Mender client builds its HTTP transport on Boost.Asio (via Beast).
//
// This is a hosted-OS port (it uses Asio's reactor and the heap). The Boost
// headers are confined to the .cpp via a PIMPL, so including this header does
// not drag Boost into the rest of the build. Like any net::Socket it can be
// wrapped by net::TlsSocket to get TLS / mutual TLS over an Asio transport.
namespace etlx::ports::asio {

class AsioTcpSocket final : public etlx::net::Socket {
public:
    AsioTcpSocket();
    ~AsioTcpSocket() override;

    AsioTcpSocket(const AsioTcpSocket&)            = delete;
    AsioTcpSocket& operator=(const AsioTcpSocket&) = delete;

    etlx::Status         Connect(etl::string_view host, uint16_t port) override;
    etlx::Status         Send(etlx::ConstByteSpan src) override;
    etlx::Result<size_t> Recv(etlx::ByteSpan dst) override;
    void                 Close() override;

private:
    struct Impl;
    Impl* impl_;  // owns the io_context + tcp::socket (host heap)
};

} // namespace etlx::ports::asio

#endif // ETLX_PORTS_ASIO_TCP_HPP
