#ifndef ETLX_HTTP_HTTP_HPP
#define ETLX_HTTP_HTTP_HPP

#include "etlx/core/optional.hpp"
#include "etlx/core/result.hpp"
#include "etlx/core/span.hpp"
#include "etlx/etlx_config.hpp"
#include "etlx/net/net.hpp"

#include <etl/string_view.h>
#include <etl/vector.h>

// Minimal HTTP/1.1 client, modelled on Mender's http module: build a Request,
// send it over a net::Socket, and parse the Response from a fixed buffer. The
// parser is hand-rolled and bounded (no llhttp, no heap); header views point
// into the caller's receive buffer, so the buffer must outlive the Response.
namespace etlx::http {

extern const error::Category Category;

enum Code : error::Code {
    None          = 0,
    BufferTooSmall = 1,
    Malformed     = 2,  // response is not valid HTTP/1.1
    Incomplete    = 3,  // headers not fully received
    TooManyHeaders = 4,
};

enum class Method { Get, Post, Put, Delete, Head };

struct Header {
    etl::string_view name;
    etl::string_view value;
};

using HeaderList = etl::vector<Header, ETLX_HTTP_MAX_HEADERS>;

struct Request {
    Method           method = Method::Get;
    etl::string_view host;          // e.g. "example.com"
    etl::string_view path = "/";    // e.g. "/manifest.json"
    HeaderList       headers;       // extra headers beyond Host/Connection
    ConstByteSpan    body;          // empty if none
};

struct Response {
    int              status = 0;          // e.g. 200
    etl::string_view reason;              // e.g. "OK"
    HeaderList       headers;
    ConstByteSpan    body;
    Optional<size_t> content_length;

    // Case-insensitive header lookup; empty view if absent.
    etl::string_view GetHeader(etl::string_view name) const;
};

// Serialises `req` into `out` as raw HTTP/1.1 bytes; returns the byte count.
// Adds Host, Connection: close, and Content-Length (when a body is present).
Result<size_t> BuildRequest(const Request& req, ByteSpan out);

// Parses a complete response from `raw`. Body views reference `raw`.
Result<Response> ParseResponse(ConstByteSpan raw);

// High-level client: builds the request, sends it, reads the whole response
// into recv_buf (relying on Connection: close), and parses it.
class Client {
public:
    Result<Response> Do(net::Socket& sock, const Request& req, ByteSpan recv_buf);

    // Shorthand for a GET to host/path.
    Result<Response> Get(net::Socket& sock, etl::string_view host,
                         etl::string_view path, ByteSpan recv_buf);
};

} // namespace etlx::http

#endif // ETLX_HTTP_HTTP_HPP
