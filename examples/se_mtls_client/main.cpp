// Example: SE-backed mTLS client.
//
// Demonstrates wiring an SE05x RSA key into TlsSocket as an opaque mbedTLS pk
// context so the private key never leaves the secure element.  When SE hardware
// is absent (Connection::Open fails), the example falls back to a software key
// from the test certificates so the TLS path is still exercised.
//
// Requires: ETLX_WITH_SE05X=ON, ETLX_WITH_TLS=ON.
//
// With real hardware: provision first with se_factory, set EX_SSS_BOOT_SSS_PORT
// to the port string, then run.

#include <etlx/log/log.hpp>
#include <etlx/net/tcp_socket.hpp>
#include <etlx/net/tls.hpp>
#include <etlx/se/connection.hpp>
#include <etlx/se/rsa_key.hpp>
#include <etlx/se/object_store.hpp>
#include <etlx/se/se_pk.hpp>
#include "host/host_io.hpp"
#include "tests/net/test_certs.hpp"

#include <cstdlib>
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

int FdSend(void *c, const unsigned char *b, size_t l) {
    ssize_t n = ::send(*static_cast<int *>(c), b, l, MSG_NOSIGNAL);
    return n < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : static_cast<int>(n);
}
int FdRecv(void *c, unsigned char *b, size_t l) {
    ssize_t n = ::recv(*static_cast<int *>(c), b, l, 0);
    return n < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : static_cast<int>(n);
}

// Loopback mTLS server that requires a client cert (signed by the test CA).
void RunServer(int listen_fd) {
    int conn = ::accept(listen_fd, nullptr, nullptr);
    if (conn < 0) return;

    using namespace etlx_test_certs;
    mbedtls_entropy_context  e; mbedtls_entropy_init(&e);
    mbedtls_ctr_drbg_context d; mbedtls_ctr_drbg_init(&d);
    mbedtls_ssl_context      s; mbedtls_ssl_init(&s);
    mbedtls_ssl_config       c; mbedtls_ssl_config_init(&c);
    mbedtls_x509_crt crt, ca;  mbedtls_x509_crt_init(&crt); mbedtls_x509_crt_init(&ca);
    mbedtls_pk_context       k; mbedtls_pk_init(&k);

    mbedtls_ctr_drbg_seed(&d, mbedtls_entropy_func, &e, nullptr, 0);
    mbedtls_ssl_config_defaults(&c, MBEDTLS_SSL_IS_SERVER,
                                MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_rng(&c, mbedtls_ctr_drbg_random, &d);
    mbedtls_x509_crt_parse(&crt,
        reinterpret_cast<const unsigned char *>(kServerCertPem), std::strlen(kServerCertPem) + 1);
    mbedtls_pk_parse_key(&k,
        reinterpret_cast<const unsigned char *>(kServerKeyPem), std::strlen(kServerKeyPem) + 1,
        nullptr, 0, mbedtls_ctr_drbg_random, &d);
    mbedtls_ssl_conf_own_cert(&c, &crt, &k);
    mbedtls_x509_crt_parse(&ca,
        reinterpret_cast<const unsigned char *>(kCaCertPem), std::strlen(kCaCertPem) + 1);
    mbedtls_ssl_conf_ca_chain(&c, &ca, nullptr);
    mbedtls_ssl_conf_authmode(&c, MBEDTLS_SSL_VERIFY_REQUIRED);
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
    static etlx::ports::host::StderrLogSink sink;
    etlx::log::SetSink(&sink);
    etlx::log::SetLevel(etlx::log::Level::Info);

    // SE key + connection kept alive for the TLS handshake duration (the
    // opaque-pk sign callback holds a pointer into the RsaKey object).
    bool using_se = false;
    mbedtls_pk_context opaque_pk;
    mbedtls_pk_init(&opaque_pk);
    etlx::Result<etlx::se::Connection> conn_res =
        etlx::se::SeFail(etlx::se::kOpenFailed, "not attempted");
    etlx::Result<etlx::se::RsaKey> key_res =
        etlx::se::SeFail(etlx::se::kKeyFailed, "not attempted");

    // Only attempt SE open when EX_SSS_BOOT_SSS_PORT is set.  Probing a missing
    // I2C device leaves NXP middleware atexit state that segfaults on exit.
    const char *se_port = std::getenv("EX_SSS_BOOT_SSS_PORT");
    if (se_port != nullptr) {
        conn_res = etlx::se::Connection::Open(se_port);
        if (conn_res) {
            key_res = etlx::se::RsaKey::Open(conn_res.value(), etlx::se::kRsaKeyId);
            if (key_res) {
                auto st = etlx::se::SetupOpaquePk(opaque_pk, key_res.value());
                if (st) {
                    using_se = true;
                    ETLX_LOG_INFO("se_mtls_client: SE-backed RSA key id=0x%08x",
                                  static_cast<unsigned>(etlx::se::kRsaKeyId));
                } else {
                    ETLX_LOG_WARN("se_mtls_client: SetupOpaquePk: %s; software fallback",
                                  st.error().message.c_str());
                }
            } else {
                ETLX_LOG_INFO("se_mtls_client: no provisioned SE key; software fallback");
            }
        } else {
            ETLX_LOG_WARN("se_mtls_client: SE open failed: %s; software fallback",
                          conn_res.error().message.c_str());
        }
    } else {
        ETLX_LOG_INFO("se_mtls_client: EX_SSS_BOOT_SSS_PORT not set; software fallback");
    }

    // Loopback server uses test certificates.
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ::bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    ::listen(listen_fd, 1);
    socklen_t slen = sizeof(addr);
    ::getsockname(listen_fd, reinterpret_cast<sockaddr *>(&addr), &slen);
    const uint16_t port = ntohs(addr.sin_port);
    std::thread server(RunServer, listen_fd);

    etlx::net::TcpSocket tcp;
    etlx::net::TlsSocket tls(tcp);

    etlx::net::TlsConfig cfg;
    cfg.ca_cert_pem     = etlx_test_certs::kCaCertPem;
    cfg.client_cert_pem = etlx_test_certs::kClientCertPem;
    cfg.server_name     = "localhost";

    if (using_se) {
        // SE path: register the opaque pk; Configure() skips PEM key parsing.
        tls.UseExternalKey(opaque_pk);
        // client_key_pem intentionally left empty — the SE key is used instead.
    } else {
        cfg.client_key_pem = etlx_test_certs::kClientKeyPem;
    }

    int rc = 1;
    if (auto s = tls.Configure(cfg); !s) {
        ETLX_LOG_ERROR("configure: %s", s.error().message.c_str());
    } else if (auto c = tls.Connect("127.0.0.1", port); !c) {
        ETLX_LOG_ERROR("handshake: %s", c.error().message.c_str());
    } else {
        const char *mode = using_se ? "SE-backed mTLS" : "software mTLS";
        ETLX_LOG_INFO("se_mtls_client: %s handshake OK", mode);
        const char *msg = "GET /manifest over SE-mTLS";
        tls.Send(etlx::ConstByteSpan{reinterpret_cast<const uint8_t *>(msg), std::strlen(msg)});
        uint8_t buf[128];
        if (auto n = tls.Recv(etlx::ByteSpan{buf, sizeof(buf)}); n && n.value() > 0) {
            ETLX_LOG_INFO("server echoed: %.*s", static_cast<int>(n.value()), buf);
            rc = 0;
        }
        tls.Close();
    }

    server.join();
    ::close(listen_fd);
    mbedtls_pk_free(&opaque_pk);
    return rc;
}
