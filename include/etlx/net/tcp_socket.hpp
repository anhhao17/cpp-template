#ifndef ETLX_NET_TCP_SOCKET_HPP
#define ETLX_NET_TCP_SOCKET_HPP

// Transport-agnostic TCP socket alias. Code that just needs "the" TCP transport
// uses etlx::net::TcpSocket and links etlx::transport; the concrete backend is
// fixed at build time by ETLX_TRANSPORT (posix or asio). Only the selected
// backend is compiled, so the two are never combined in one binary.
//
// ETLX_TRANSPORT_ASIO is defined by the etlx::transport target for the asio
// build; otherwise the POSIX backend is used.
#if defined(ETLX_TRANSPORT_ASIO)
#  include "asio/asio_tcp.hpp"
namespace etlx::net {
using TcpSocket = etlx::ports::asio::AsioTcpSocket;
}
#else
#  include "host/host_tcp.hpp"
namespace etlx::net {
using TcpSocket = etlx::ports::host::PosixTcpSocket;
}
#endif

#endif // ETLX_NET_TCP_SOCKET_HPP
