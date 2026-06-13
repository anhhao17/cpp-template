#include "etlx/net/tls.hpp"

#include <cstring>

namespace etlx::net {

TlsSocket::TlsSocket(Socket& transport) : transport_(transport) {
    mbedtls_entropy_init(&entropy_);
    mbedtls_ctr_drbg_init(&drbg_);
    mbedtls_ssl_init(&ssl_);
    mbedtls_ssl_config_init(&conf_);
    mbedtls_x509_crt_init(&ca_);
    mbedtls_x509_crt_init(&own_cert_);
    mbedtls_pk_init(&own_key_);
}

TlsSocket::~TlsSocket() {
    mbedtls_ssl_free(&ssl_);
    mbedtls_ssl_config_free(&conf_);
    mbedtls_x509_crt_free(&ca_);
    mbedtls_x509_crt_free(&own_cert_);
    mbedtls_pk_free(&own_key_);
    mbedtls_ctr_drbg_free(&drbg_);
    mbedtls_entropy_free(&entropy_);
}

int TlsSocket::BioSend(void* ctx, const unsigned char* buf, size_t len) {
    auto* self = static_cast<TlsSocket*>(ctx);
    if (auto s = self->transport_.Send(ConstByteSpan{buf, len}); !s) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    return static_cast<int>(len);  // transport.Send is all-or-error
}

int TlsSocket::BioRecv(void* ctx, unsigned char* buf, size_t len) {
    auto* self = static_cast<TlsSocket*>(ctx);
    auto  r    = self->transport_.Recv(ByteSpan{buf, len});
    if (!r) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    return static_cast<int>(r.value());  // 0 => peer closed
}

Status TlsSocket::Configure(const TlsConfig& cfg) {
    if (mbedtls_ctr_drbg_seed(&drbg_, mbedtls_entropy_func, &entropy_, nullptr, 0) != 0) {
        return Fail(Category, TlsError, "drbg seed failed");
    }
    if (mbedtls_ssl_config_defaults(&conf_, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        return Fail(Category, TlsError, "ssl config defaults failed");
    }
    mbedtls_ssl_conf_rng(&conf_, mbedtls_ctr_drbg_random, &drbg_);

    if (cfg.skip_verify) {
        mbedtls_ssl_conf_authmode(&conf_, MBEDTLS_SSL_VERIFY_NONE);
    } else {
        // x509 PEM parsing needs the terminating NUL counted in the length.
        if (mbedtls_x509_crt_parse(
                &ca_, reinterpret_cast<const unsigned char*>(cfg.ca_cert_pem.data()),
                cfg.ca_cert_pem.size() + 1) != 0) {
            return Fail(Category, TlsError, "parse CA cert failed");
        }
        mbedtls_ssl_conf_ca_chain(&conf_, &ca_, nullptr);
        mbedtls_ssl_conf_authmode(&conf_, MBEDTLS_SSL_VERIFY_REQUIRED);
    }

    // Mutual TLS: present a client certificate + key if configured.
    // The key is either parsed from PEM (client_key_pem) or an already-set-up
    // external pk context registered via UseExternalKey() (e.g. SE-backed opaque).
    const bool have_key = !cfg.client_key_pem.empty() || external_key_ != nullptr;
    if (!cfg.client_cert_pem.empty() && have_key) {
        if (mbedtls_x509_crt_parse(
                &own_cert_, reinterpret_cast<const unsigned char*>(cfg.client_cert_pem.data()),
                cfg.client_cert_pem.size() + 1) != 0) {
            return Fail(Category, TlsError, "parse client cert failed");
        }
        mbedtls_pk_context* key_ptr;
        if (external_key_ != nullptr) {
            key_ptr = external_key_;
        } else {
            if (mbedtls_pk_parse_key(
                    &own_key_,
                    reinterpret_cast<const unsigned char*>(cfg.client_key_pem.data()),
                    cfg.client_key_pem.size() + 1, nullptr, 0,
                    mbedtls_ctr_drbg_random, &drbg_) != 0) {
                return Fail(Category, TlsError, "parse client key failed");
            }
            key_ptr = &own_key_;
        }
        if (mbedtls_ssl_conf_own_cert(&conf_, &own_cert_, key_ptr) != 0) {
            return Fail(Category, TlsError, "set client cert failed");
        }
    }

    if (mbedtls_ssl_setup(&ssl_, &conf_) != 0) {
        return Fail(Category, TlsError, "ssl setup failed");
    }
    server_name_ = cfg.server_name;
    configured_  = true;
    return Ok();
}

Status TlsSocket::Connect(etl::string_view host, uint16_t port) {
    if (!configured_) return Fail(Category, TlsError, "Configure not called");

    if (auto s = transport_.Connect(host, port); !s) return s;

    // SNI / hostname verification target: server_name override, else the host.
    etl::string_view name = server_name_.empty() ? host : server_name_;
    char namez[256];
    const size_t n = name.size() < sizeof(namez) - 1 ? name.size() : sizeof(namez) - 1;
    std::memcpy(namez, name.data(), n);
    namez[n] = '\0';
    if (mbedtls_ssl_set_hostname(&ssl_, namez) != 0) {
        return Fail(Category, TlsError, "set hostname failed");
    }

    mbedtls_ssl_set_bio(&ssl_, this, BioSend, BioRecv, nullptr);

    int ret;
    while ((ret = mbedtls_ssl_handshake(&ssl_)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            return Fail(Category, TlsError, "TLS handshake failed");
        }
    }
    handshaked_ = true;
    return Ok();
}

Status TlsSocket::Send(ConstByteSpan src) {
    size_t sent = 0;
    while (sent < src.size()) {
        int ret = mbedtls_ssl_write(&ssl_, src.data() + sent, src.size() - sent);
        if (ret > 0) {
            sent += static_cast<size_t>(ret);
            continue;
        }
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        return Fail(Category, SendFailed, "ssl write failed");
    }
    return Ok();
}

Result<size_t> TlsSocket::Recv(ByteSpan dst) {
    for (;;) {
        int ret = mbedtls_ssl_read(&ssl_, dst.data(), dst.size());
        if (ret >= 0) return static_cast<size_t>(ret);  // 0 = clean EOF
        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return static_cast<size_t>(0);
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        return Fail(Category, RecvFailed, "ssl read failed");
    }
}

void TlsSocket::Close() {
    if (handshaked_) {
        mbedtls_ssl_close_notify(&ssl_);
        handshaked_ = false;
    }
    transport_.Close();
}

} // namespace etlx::net
