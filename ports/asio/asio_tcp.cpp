#include "asio/asio_tcp.hpp"

#include <string>

#include <boost/asio.hpp>

// This translation unit is compiled with exceptions enabled (it does not link
// etlx_embedded_flags), and uses Asio's non-throwing error_code overloads
// throughout, so no exception ever propagates out into the etlx Result world.
namespace etlx::ports::asio {

namespace basio = boost::asio;
using tcp = basio::ip::tcp;
namespace net = etlx::net;

struct AsioTcpSocket::Impl {
    basio::io_context io;
    tcp::socket       sock{io};
};

AsioTcpSocket::AsioTcpSocket() : impl_(new Impl) {}

AsioTcpSocket::~AsioTcpSocket() {
    Close();
    delete impl_;
}

etlx::Status AsioTcpSocket::Connect(etl::string_view host, uint16_t port) {
    boost::system::error_code ec;

    tcp::resolver resolver(impl_->io);
    auto endpoints = resolver.resolve(std::string(host.data(), host.size()),
                                      std::to_string(port), ec);
    if (ec) return etlx::Fail(net::Category, net::ResolveFailed, ec.message().c_str());

    basio::connect(impl_->sock, endpoints, ec);
    if (ec) return etlx::Fail(net::Category, net::ConnectFailed, ec.message().c_str());
    return etlx::Ok();
}

etlx::Status AsioTcpSocket::Send(etlx::ConstByteSpan src) {
    boost::system::error_code ec;
    basio::write(impl_->sock, basio::buffer(src.data(), src.size()), ec);
    if (ec) return etlx::Fail(net::Category, net::SendFailed, ec.message().c_str());
    return etlx::Ok();
}

etlx::Result<size_t> AsioTcpSocket::Recv(etlx::ByteSpan dst) {
    boost::system::error_code ec;
    const size_t n = impl_->sock.read_some(basio::buffer(dst.data(), dst.size()), ec);
    if (ec == basio::error::eof) return static_cast<size_t>(0);  // peer closed
    if (ec) return etlx::Fail(net::Category, net::RecvFailed, ec.message().c_str());
    return n;
}

void AsioTcpSocket::Close() {
    if (impl_->sock.is_open()) {
        boost::system::error_code ec;
        impl_->sock.shutdown(tcp::socket::shutdown_both, ec);
        impl_->sock.close(ec);
    }
}

} // namespace etlx::ports::asio
