#include "etlx/net/tls.hpp"
#include "host/host_tcp.hpp"
#include "tests/net/test_certs.hpp"

#include <atomic>
#include <cstring>
#include <string>
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

#include <gtest/gtest.h>

namespace {

// Raw-fd bio callbacks so the test server drives mbedTLS over a plain accepted
// socket (mirrors how TlsSocket bridges to net::Socket on the client side).
int FdSend(void* ctx, const unsigned char* b, size_t l) {
    const ssize_t n = ::send(*static_cast<int*>(ctx), b, l, MSG_NOSIGNAL);
    return n < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : static_cast<int>(n);
}
int FdRecv(void* ctx, unsigned char* b, size_t l) {
    const ssize_t n = ::recv(*static_cast<int*>(ctx), b, l, 0);
    return n < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : static_cast<int>(n);
}

// A minimal one-shot TLS echo server on loopback. It always presents the server
// certificate; require_client_cert toggles whether it demands mutual auth.
class TlsEchoServer {
public:
    explicit TlsEchoServer(bool require_client_cert)
        : require_client_cert_(require_client_cert) {}

    ~TlsEchoServer() {
        if (thread_.joinable()) thread_.join();
        if (listen_fd_ >= 0) ::close(listen_fd_);
    }

    // Binds loopback:0, returns the chosen port, and starts serving one client.
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
        const uint16_t port = ntohs(addr.sin_port);

        thread_ = std::thread([this] { Serve(); });
        return port;
    }

    // Blocks until the server thread has finished handling its one connection.
    void Wait() { if (thread_.joinable()) thread_.join(); }

    bool handshake_ok() const { return handshake_ok_.load(); }

private:
    void Serve() {
        int conn = ::accept(listen_fd_, nullptr, nullptr);
        if (conn < 0) return;

        mbedtls_entropy_context  entropy;  mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_context drbg;     mbedtls_ctr_drbg_init(&drbg);
        mbedtls_ssl_context      ssl;      mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config       conf;     mbedtls_ssl_config_init(&conf);
        mbedtls_x509_crt         srvcert;  mbedtls_x509_crt_init(&srvcert);
        mbedtls_x509_crt         ca;       mbedtls_x509_crt_init(&ca);
        mbedtls_pk_context       pkey;     mbedtls_pk_init(&pkey);

        mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, nullptr, 0);
        mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
        mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);

        using namespace etlx_test_certs;
        mbedtls_x509_crt_parse(&srvcert, reinterpret_cast<const unsigned char*>(kServerCertPem),
                               std::strlen(kServerCertPem) + 1);
        mbedtls_pk_parse_key(&pkey, reinterpret_cast<const unsigned char*>(kServerKeyPem),
                             std::strlen(kServerKeyPem) + 1, nullptr, 0,
                             mbedtls_ctr_drbg_random, &drbg);
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
        mbedtls_ssl_set_bio(&ssl, &conn, FdSend, FdRecv, nullptr);

        int ret;
        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) break;
        }
        if (ret == 0) {
            handshake_ok_ = true;
            unsigned char buf[128];
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

using namespace etlx_test_certs;

// Happy path: the server requires a client certificate, the client presents
// one signed by the trusted CA, and an application message round-trips.
TEST(TlsMutualAuth, ClientCertAcceptedAndEcho) {
    TlsEchoServer server(/*require_client_cert=*/true);
    const uint16_t port = server.Start();

    etlx::ports::host::PosixTcpSocket tcp;
    etlx::net::TlsSocket             tls(tcp);

    etlx::net::TlsConfig cfg;
    cfg.ca_cert_pem     = kCaCertPem;
    cfg.client_cert_pem = kClientCertPem;
    cfg.client_key_pem  = kClientKeyPem;
    cfg.server_name     = "localhost";
    ASSERT_TRUE(tls.Configure(cfg).has_value());

    ASSERT_TRUE(tls.Connect("127.0.0.1", port).has_value());
    EXPECT_TRUE(tls.handshaked());

    const char* msg = "hello-over-mtls";
    ASSERT_TRUE(tls.Send(etlx::ConstByteSpan{
        reinterpret_cast<const uint8_t*>(msg), std::strlen(msg)}).has_value());

    uint8_t buf[64];
    auto n = tls.Recv(etlx::ByteSpan{buf, sizeof(buf)});
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buf), n.value()), "hello-over-mtls");

    tls.Close();
    server.Wait();
    EXPECT_TRUE(server.handshake_ok());
}

// Server demands a client certificate; client presents none. The server
// rejects mutual auth. (Under TLS 1.3 the client handshake can complete before
// the rejection arrives, so the failure may surface on the first data
// exchange instead of at Connect.) Either way the exchange must not succeed and
// the server must not consider the handshake complete.
TEST(TlsMutualAuth, MissingClientCertRejected) {
    TlsEchoServer server(/*require_client_cert=*/true);
    const uint16_t port = server.Start();

    etlx::ports::host::PosixTcpSocket tcp;
    etlx::net::TlsSocket             tls(tcp);

    etlx::net::TlsConfig cfg;
    cfg.ca_cert_pem = kCaCertPem;     // verifies the server...
    cfg.server_name = "localhost";    // ...but sends no client cert
    ASSERT_TRUE(tls.Configure(cfg).has_value());

    bool succeeded = tls.Connect("127.0.0.1", port).has_value();
    if (succeeded) {
        const char* msg = "should-not-be-accepted";
        tls.Send(etlx::ConstByteSpan{reinterpret_cast<const uint8_t*>(msg), std::strlen(msg)});
        uint8_t buf[64];
        auto n = tls.Recv(etlx::ByteSpan{buf, sizeof(buf)});
        succeeded = n.has_value() && n.value() > 0;  // got a real echo back?
    }
    tls.Close();
    server.Wait();

    EXPECT_FALSE(succeeded);
    EXPECT_FALSE(server.handshake_ok());  // server never completed mutual auth
}

// Server-only TLS (no client cert required) still succeeds and verifies the
// server against the CA.
TEST(TlsMutualAuth, ServerOnlyTlsSucceeds) {
    TlsEchoServer server(/*require_client_cert=*/false);
    const uint16_t port = server.Start();

    etlx::ports::host::PosixTcpSocket tcp;
    etlx::net::TlsSocket             tls(tcp);

    etlx::net::TlsConfig cfg;
    cfg.ca_cert_pem = kCaCertPem;
    cfg.server_name = "localhost";
    ASSERT_TRUE(tls.Configure(cfg).has_value());

    EXPECT_TRUE(tls.Connect("127.0.0.1", port).has_value());
    tls.Close();
}

// Verification on, but the client trusts the wrong CA (here: the client's own
// cert as a bogus CA) -> server cert fails to verify -> handshake fails.
TEST(TlsServerVerify, UntrustedServerRejected) {
    TlsEchoServer server(/*require_client_cert=*/false);
    const uint16_t port = server.Start();

    etlx::ports::host::PosixTcpSocket tcp;
    etlx::net::TlsSocket             tls(tcp);

    etlx::net::TlsConfig cfg;
    cfg.ca_cert_pem = kClientCertPem;  // not the CA that signed the server
    cfg.server_name = "localhost";
    ASSERT_TRUE(tls.Configure(cfg).has_value());

    EXPECT_FALSE(tls.Connect("127.0.0.1", port).has_value());
}

} // namespace
