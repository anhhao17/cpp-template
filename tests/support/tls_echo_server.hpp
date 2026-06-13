#ifndef ETLX_TEST_TLS_ECHO_SERVER_HPP
#define ETLX_TEST_TLS_ECHO_SERVER_HPP

#include "tests/net/test_certs.hpp"

#include <atomic>
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

// Shared test fixture: a one-shot loopback TLS echo server, used by both the
// TLS and Asio mutual-TLS tests. It always presents the server certificate;
// require_client_cert toggles whether it demands mutual auth.
namespace etlx_test_support {

inline int TlsFdSend(void* ctx, const unsigned char* b, size_t l) {
    const ssize_t n = ::send(*static_cast<int*>(ctx), b, l, MSG_NOSIGNAL);
    return n < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : static_cast<int>(n);
}
inline int TlsFdRecv(void* ctx, unsigned char* b, size_t l) {
    const ssize_t n = ::recv(*static_cast<int*>(ctx), b, l, 0);
    return n < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : static_cast<int>(n);
}

class TlsEchoServer {
public:
    explicit TlsEchoServer(bool require_client_cert)
        : require_client_cert_(require_client_cert) {}

    ~TlsEchoServer() {
        Wait();
        if (listen_fd_ >= 0) ::close(listen_fd_);
    }

    // Binds loopback:0, returns the chosen port, and serves one client async.
    uint16_t Start() {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = 0;
        ::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        ::listen(listen_fd_, 1);
        socklen_t len = sizeof(addr);
        ::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len);
        thread_ = std::thread([this] { Serve(); });
        return ntohs(addr.sin_port);
    }

    void Wait() { if (thread_.joinable()) thread_.join(); }
    bool handshake_ok() const { return handshake_ok_.load(); }

private:
    void Serve() {
        int conn = ::accept(listen_fd_, nullptr, nullptr);
        if (conn < 0) return;

        using namespace etlx_test_certs;
        mbedtls_entropy_context  entropy;  mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_context drbg;     mbedtls_ctr_drbg_init(&drbg);
        mbedtls_ssl_context      ssl;      mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config       conf;     mbedtls_ssl_config_init(&conf);
        mbedtls_x509_crt         srvcert;  mbedtls_x509_crt_init(&srvcert);
        mbedtls_x509_crt         ca;       mbedtls_x509_crt_init(&ca);
        mbedtls_pk_context       pkey;     mbedtls_pk_init(&pkey);

        mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, nullptr, 0);
        mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER,
                                    MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
        mbedtls_x509_crt_parse(&srvcert, reinterpret_cast<const unsigned char*>(kServerCertPem),
                               std::strlen(kServerCertPem) + 1);
        mbedtls_pk_parse_key(&pkey, reinterpret_cast<const unsigned char*>(kServerKeyPem),
                             std::strlen(kServerKeyPem) + 1, nullptr, 0, mbedtls_ctr_drbg_random, &drbg);
        mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey);

        if (require_client_cert_) {
            mbedtls_x509_crt_parse(&ca, reinterpret_cast<const unsigned char*>(kCaCertPem),
                                   std::strlen(kCaCertPem) + 1);
            mbedtls_ssl_conf_ca_chain(&conf, &ca, nullptr);
            mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        } else {
            mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
        }

        mbedtls_ssl_setup(&ssl, &conf);
        mbedtls_ssl_set_bio(&ssl, &conn, TlsFdSend, TlsFdRecv, nullptr);

        int ret;
        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) break;
        }
        if (ret == 0) {
            handshake_ok_ = true;
            unsigned char buf[256];
            int n = mbedtls_ssl_read(&ssl, buf, sizeof(buf));
            if (n > 0) mbedtls_ssl_write(&ssl, buf, static_cast<size_t>(n));  // echo
            mbedtls_ssl_close_notify(&ssl);
        }

        ::close(conn);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_x509_crt_free(&srvcert);
        mbedtls_x509_crt_free(&ca);
        mbedtls_pk_free(&pkey);
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
    }

    bool              require_client_cert_;
    int               listen_fd_ = -1;
    std::thread       thread_;
    std::atomic<bool> handshake_ok_{false};
};

} // namespace etlx_test_support

#endif // ETLX_TEST_TLS_ECHO_SERVER_HPP
