// Example: mutual-TLS client. Layers net::TlsSocket over a PosixTcpSocket,
// presents a client certificate + key, and verifies the server against a CA --
// the same shape the Mender client uses (server_cert / client_cert /
// client_cert_key). To run offline it spins up a tiny loopback TLS echo server
// that *requires* a client certificate, then connects to it with mTLS.
//
// To talk to a real endpoint instead, drop the server and call
//   tls.Connect("device.example.com", 443);
// Build target: example_mtls_client.
#include "etlx/net/tcp_socket.hpp"
#include "etlx/net/tls.hpp"
#include "tests/net/test_certs.hpp"

#include <cstdio>
#include <cstring>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

namespace {

int FdSend(void* c, const unsigned char* b, size_t l) {
    ssize_t n = ::send(*static_cast<int*>(c), b, l, MSG_NOSIGNAL);
    return n < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : static_cast<int>(n);
}
int FdRecv(void* c, unsigned char* b, size_t l) {
    ssize_t n = ::recv(*static_cast<int*>(c), b, l, 0);
    return n < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : static_cast<int>(n);
}

// One-shot loopback TLS echo server that demands a client certificate.
void RunServer(int listen_fd) {
    int conn = ::accept(listen_fd, nullptr, nullptr);
    if (conn < 0) return;

    using namespace etlx_test_certs;
    mbedtls_entropy_context  e;  mbedtls_entropy_init(&e);
    mbedtls_ctr_drbg_context d;  mbedtls_ctr_drbg_init(&d);
    mbedtls_ssl_context      s;  mbedtls_ssl_init(&s);
    mbedtls_ssl_config       c;  mbedtls_ssl_config_init(&c);
    mbedtls_x509_crt    crt, ca; mbedtls_x509_crt_init(&crt); mbedtls_x509_crt_init(&ca);
    mbedtls_pk_context       k;  mbedtls_pk_init(&k);

    mbedtls_ctr_drbg_seed(&d, mbedtls_entropy_func, &e, nullptr, 0);
    mbedtls_ssl_config_defaults(&c, MBEDTLS_SSL_IS_SERVER,
                                MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_rng(&c, mbedtls_ctr_drbg_random, &d);
    mbedtls_x509_crt_parse(&crt, reinterpret_cast<const unsigned char*>(kServerCertPem),
                           std::strlen(kServerCertPem) + 1);
    mbedtls_pk_parse_key(&k, reinterpret_cast<const unsigned char*>(kServerKeyPem),
                         std::strlen(kServerKeyPem) + 1, nullptr, 0, mbedtls_ctr_drbg_random, &d);
    mbedtls_ssl_conf_own_cert(&c, &crt, &k);
    mbedtls_x509_crt_parse(&ca, reinterpret_cast<const unsigned char*>(kCaCertPem),
                           std::strlen(kCaCertPem) + 1);
    mbedtls_ssl_conf_ca_chain(&c, &ca, nullptr);
    mbedtls_ssl_conf_authmode(&c, MBEDTLS_SSL_VERIFY_REQUIRED);  // require client cert
    mbedtls_ssl_setup(&s, &c);
    mbedtls_ssl_set_bio(&s, &conn, FdSend, FdRecv, nullptr);

    int ret;
    while ((ret = mbedtls_ssl_handshake(&s)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) break;
    }
    if (ret == 0) {
        unsigned char buf[128];
        int n = mbedtls_ssl_read(&s, buf, sizeof(buf));
        if (n > 0) mbedtls_ssl_write(&s, buf, static_cast<size_t>(n));
        mbedtls_ssl_close_notify(&s);
    }
    ::close(conn);
    mbedtls_ssl_free(&s); mbedtls_ssl_config_free(&c);
    mbedtls_x509_crt_free(&crt); mbedtls_x509_crt_free(&ca);
    mbedtls_pk_free(&k); mbedtls_ctr_drbg_free(&d); mbedtls_entropy_free(&e);
}

} // namespace

int main() {
    // Bind a loopback listener and start the mTLS-requiring server.
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::listen(listen_fd, 1);
    socklen_t len = sizeof(addr);
    ::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len);
    const uint16_t port = ntohs(addr.sin_port);
    std::thread server(RunServer, listen_fd);

    // Client: TLS over the selected TCP transport (posix or asio), presenting a
    // client certificate for mutual auth.
    etlx::net::TcpSocket tcp;
    etlx::net::TlsSocket tls(tcp);

    etlx::net::TlsConfig cfg;
    cfg.ca_cert_pem     = etlx_test_certs::kCaCertPem;      // verify the server
    cfg.client_cert_pem = etlx_test_certs::kClientCertPem;  // our identity...
    cfg.client_key_pem  = etlx_test_certs::kClientKeyPem;   // ...and its key
    cfg.server_name     = "localhost";

    int rc = 1;
    if (auto s = tls.Configure(cfg); !s) {
        std::printf("configure failed: %s\n", s.error().message.c_str());
    } else if (auto c = tls.Connect("127.0.0.1", port); !c) {
        std::printf("handshake failed: %s\n", c.error().message.c_str());
    } else {
        std::printf("mutual TLS handshake OK (presented client cert)\n");
        const char* msg = "GET /manifest over mTLS";
        tls.Send(etlx::ConstByteSpan{reinterpret_cast<const uint8_t*>(msg), std::strlen(msg)});
        uint8_t buf[128];
        if (auto n = tls.Recv(etlx::ByteSpan{buf, sizeof(buf)}); n && n.value() > 0) {
            std::printf("server echoed: %.*s\n", static_cast<int>(n.value()), buf);
            rc = 0;
        }
        tls.Close();
    }

    server.join();
    ::close(listen_fd);
    return rc;
}
