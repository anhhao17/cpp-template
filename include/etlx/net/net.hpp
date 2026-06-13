#ifndef ETLX_NET_NET_HPP
#define ETLX_NET_NET_HPP

#include "etlx/core/result.hpp"
#include "etlx/core/span.hpp"

#include <cstdint>

#include <etl/string_view.h>

// Transport abstraction over sockets. The http client speaks to a Socket; a
// concrete backend (host POSIX, lwIP, a TLS wrapper) implements it. A TlsSocket
// would wrap a plain Socket plus an mbedTLS context behind the same interface.
namespace etlx::net {

extern const error::Category Category;

enum Code : error::Code {
    None          = 0,
    ResolveFailed = 1,
    ConnectFailed = 2,
    SendFailed    = 3,
    RecvFailed    = 4,
    Closed        = 5,  // peer closed the connection
};

// A bidirectional byte stream to a remote endpoint. Send transmits the whole
// span or fails; Recv returns the bytes read (0 means the peer closed).
struct Socket {
    virtual Status         Connect(etl::string_view host, uint16_t port) = 0;
    virtual Status         Send(ConstByteSpan src) = 0;
    virtual Result<size_t> Recv(ByteSpan dst) = 0;
    virtual void           Close() = 0;
    virtual ~Socket() = default;
};

} // namespace etlx::net

#endif // ETLX_NET_NET_HPP
