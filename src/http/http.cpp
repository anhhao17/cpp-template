#include "etlx/http/http.hpp"

#include <cstdint>

namespace etlx::http {

const error::Category Category{"http"};

namespace {

const char* MethodName(Method m) {
    switch (m) {
        case Method::Get:    return "GET";
        case Method::Post:   return "POST";
        case Method::Put:    return "PUT";
        case Method::Delete: return "DELETE";
        case Method::Head:   return "HEAD";
    }
    return "GET";
}

// Bounded append cursor over a byte buffer; flips to overflow on first overrun.
struct Appender {
    ByteSpan buf;
    size_t   pos      = 0;
    bool     overflow = false;

    void Put(etl::string_view s) {
        if (overflow) return;
        if (s.size() > buf.size() - pos) { overflow = true; return; }
        for (char c : s) buf[pos++] = static_cast<uint8_t>(c);
    }
    void Put(ConstByteSpan s) {
        if (overflow) return;
        if (s.size() > buf.size() - pos) { overflow = true; return; }
        for (uint8_t b : s) buf[pos++] = b;
    }
    void PutUInt(size_t v) {
        char tmp[20];
        int  i = 0;
        if (v == 0) tmp[i++] = '0';
        while (v > 0) { tmp[i++] = static_cast<char>('0' + (v % 10)); v /= 10; }
        char rev[20];
        for (int j = 0; j < i; ++j) rev[j] = tmp[i - 1 - j];
        Put(etl::string_view{rev, static_cast<size_t>(i)});
    }
};

bool IEqual(etl::string_view a, etl::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) return false;
    }
    return true;
}

etl::string_view Trim(etl::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

} // namespace

etl::string_view Response::GetHeader(etl::string_view name) const {
    for (const auto& h : headers) {
        if (IEqual(h.name, name)) return h.value;
    }
    return {};
}

Result<size_t> BuildRequest(const Request& req, ByteSpan out) {
    Appender a{out};

    a.Put(MethodName(req.method));
    a.Put(" ");
    a.Put(req.path);
    a.Put(" HTTP/1.1\r\n");

    a.Put("Host: ");
    a.Put(req.host);
    a.Put("\r\n");
    a.Put("Connection: close\r\n");

    for (const auto& h : req.headers) {
        a.Put(h.name);
        a.Put(": ");
        a.Put(h.value);
        a.Put("\r\n");
    }

    if (!req.body.empty()) {
        a.Put("Content-Length: ");
        a.PutUInt(req.body.size());
        a.Put("\r\n");
    }
    a.Put("\r\n");
    if (!req.body.empty()) a.Put(req.body);

    if (a.overflow) return Fail(Category, BufferTooSmall, "request buffer too small");
    return a.pos;
}

Result<Response> ParseResponse(ConstByteSpan raw) {
    etl::string_view text{reinterpret_cast<const char*>(raw.data()), raw.size()};

    const size_t boundary = text.find("\r\n\r\n");
    if (boundary == etl::string_view::npos) {
        return Fail(Category, Incomplete, "headers not terminated");
    }

    Response resp;

    // Status line: "HTTP/1.1 200 OK".
    size_t line_end = text.find("\r\n");
    etl::string_view line = text.substr(0, line_end);
    size_t sp1 = line.find(' ');
    if (sp1 == etl::string_view::npos) return Fail(Category, Malformed, "no status code");
    etl::string_view rest = line.substr(sp1 + 1);
    size_t sp2 = rest.find(' ');
    etl::string_view code = (sp2 == etl::string_view::npos) ? rest : rest.substr(0, sp2);
    if (code.size() < 3) return Fail(Category, Malformed, "bad status code");
    int status = 0;
    for (size_t i = 0; i < 3; ++i) {
        if (code[i] < '0' || code[i] > '9') return Fail(Category, Malformed, "non-digit status");
        status = status * 10 + (code[i] - '0');
    }
    resp.status = status;
    resp.reason = (sp2 == etl::string_view::npos) ? etl::string_view{} : Trim(rest.substr(sp2 + 1));

    // Header lines between the status line and the blank line.
    size_t pos = line_end + 2;
    while (pos < boundary) {
        size_t eol = text.find("\r\n", pos);
        if (eol == etl::string_view::npos || eol > boundary) eol = boundary;
        etl::string_view hline = text.substr(pos, eol - pos);
        pos = eol + 2;
        if (hline.empty()) break;

        size_t colon = hline.find(':');
        if (colon == etl::string_view::npos) continue;  // tolerate junk lines
        Header h{Trim(hline.substr(0, colon)), Trim(hline.substr(colon + 1))};
        if (resp.headers.full()) return Fail(Category, TooManyHeaders, "too many headers");
        resp.headers.push_back(h);
    }

    // Body follows the blank line.
    const size_t body_start = boundary + 4;
    ConstByteSpan body{raw.data() + body_start, raw.size() - body_start};

    etl::string_view cl = resp.GetHeader("Content-Length");
    if (!cl.empty()) {
        size_t len = 0;
        bool   ok  = !cl.empty();
        for (char c : cl) {
            if (c < '0' || c > '9') { ok = false; break; }
            len = len * 10 + static_cast<size_t>(c - '0');
        }
        if (ok) {
            resp.content_length = len;
            if (len < body.size()) body = ConstByteSpan{body.data(), len};
        }
    }
    resp.body = body;
    return resp;
}

Result<Response> Client::Do(net::Socket& sock, const Request& req, ByteSpan recv_buf) {
    uint8_t reqbuf[1024];
    auto built = BuildRequest(req, ByteSpan{reqbuf, sizeof(reqbuf)});
    if (!built) return Unexpected<>{built.error()};

    if (auto s = sock.Send(ConstByteSpan{reqbuf, built.value()}); !s) {
        return Unexpected<>{s.error()};
    }

    // Read until the peer closes (we sent Connection: close) or the buffer fills.
    size_t total = 0;
    while (total < recv_buf.size()) {
        auto n = sock.Recv(ByteSpan{recv_buf.data() + total, recv_buf.size() - total});
        if (!n) return Unexpected<>{n.error()};
        if (n.value() == 0) break;  // closed
        total += n.value();
    }
    return ParseResponse(ConstByteSpan{recv_buf.data(), total});
}

Result<Response> Client::Get(net::Socket& sock, etl::string_view host,
                             etl::string_view path, ByteSpan recv_buf) {
    Request req;
    req.method = Method::Get;
    req.host   = host;
    req.path   = path;
    return Do(sock, req, recv_buf);
}

} // namespace etlx::http
