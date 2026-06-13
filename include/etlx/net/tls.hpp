#ifndef ETLX_NET_TLS_HPP
#define ETLX_NET_TLS_HPP

#include "etlx/net/net.hpp"

#include <etl/string_view.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

// TLS / mutual-TLS transport, backed by mbedTLS. A TlsSocket layers the TLS
// record protocol over any net::Socket (so it composes with PosixTcpSocket),
// exposing the same net::Socket interface to the http client. Configuration
// mirrors the Mender client: a CA to verify the server, an optional client
// certificate + key for mutual auth, a server name for SNI/verification, and a
// skip-verify escape hatch.
namespace etlx::net {

struct TlsConfig {
    // CA bundle (PEM) used to verify the server certificate. Mender:
    // server_cert_path. Must be NUL-terminated (string literals and
    // null-terminated file reads qualify).
    etl::string_view ca_cert_pem;

    // Client certificate + private key (PEM) presented for mutual TLS. Leave
    // both empty for server-only TLS. Mender: client_cert_path /
    // client_cert_key_path.
    etl::string_view client_cert_pem;
    etl::string_view client_key_pem;

    // Expected server name: used for SNI and certificate hostname verification.
    // Defaults to the Connect() host when empty.
    etl::string_view server_name;

    // Disables server certificate verification. For testing only. Mender:
    // skip_verify.
    bool skip_verify = false;
};

class TlsSocket final : public Socket {
public:
    explicit TlsSocket(Socket& transport);
    ~TlsSocket() override;

    TlsSocket(const TlsSocket&)            = delete;
    TlsSocket& operator=(const TlsSocket&) = delete;

    // Loads the configuration (certs, verification mode). Call once before
    // Connect. Returns TlsError if a PEM blob fails to parse.
    Status Configure(const TlsConfig& config);

    // Opens the underlying transport, then performs the TLS handshake (sending
    // the client certificate if one was configured).
    Status         Connect(etl::string_view host, uint16_t port) override;
    Status         Send(ConstByteSpan src) override;
    Result<size_t> Recv(ByteSpan dst) override;
    void           Close() override;

    // True once the configured client certificate has been sent and the
    // handshake completed.
    bool handshaked() const { return handshaked_; }

private:
    static int BioSend(void* ctx, const unsigned char* buf, size_t len);
    static int BioRecv(void* ctx, unsigned char* buf, size_t len);

    Socket& transport_;

    mbedtls_entropy_context  entropy_;
    mbedtls_ctr_drbg_context drbg_;
    mbedtls_ssl_context      ssl_;
    mbedtls_ssl_config       conf_;
    mbedtls_x509_crt         ca_;
    mbedtls_x509_crt         own_cert_;
    mbedtls_pk_context       own_key_;

    etl::string_view server_name_;
    bool             configured_ = false;
    bool             handshaked_ = false;
};

} // namespace etlx::net

#endif // ETLX_NET_TLS_HPP
