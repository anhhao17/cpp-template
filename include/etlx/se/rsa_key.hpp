#pragma once

#include <etlx/etlx_config.hpp>
#include <etlx/se/key_object.hpp>

#include <etl/string.h>
#include <etl/vector.h>

extern "C" {
#include <fsl_sss_api.h>
}

namespace etlx::se {

// Key size selector for RSA generation.
enum class RsaBits : uint32_t {
    k2048 = 2048,
    k4096 = 4096,
};

// Key policy controlling which SE operations are permitted on the stored key.
enum class KeyPolicy {
    Full,        // sign + decrypt + export; no SCP03 requirement (dev/test only)
    SignOnly,    // only signing; req_Sm=1 requires SCP03 for every operation
    SignDecrypt, // sign + decrypt; req_Sm=1
};

// Operations on a single RSA key pair stored in the SE05x. Move-only.
// The destructor does NOT erase the SE-resident key object.
class RsaKey {
public:
    using SpkiDer = etl::vector<uint8_t, ETLX_SE_SPKI_CAPACITY>;
    using SigBytes = etl::vector<uint8_t, ETLX_SE_SIG_CAPACITY>;
    using CsrPem = etl::string<ETLX_SE_CSR_PEM_CAPACITY>;

    // Generate a fresh RSA key pair and persist it in the SE.
    static Result<RsaKey> Generate(Connection &conn, uint32_t key_id, RsaBits bits,
                                   KeyPolicy policy = KeyPolicy::Full);

    // Bind to an existing persisted RSA key.
    static Result<RsaKey> Open(Connection &conn, uint32_t key_id);

    // Sign a 32-byte SHA-256 digest (RSASSA-PKCS1-v1_5-SHA256).
    Result<SigBytes> Sign(const uint8_t *digest, size_t digest_len);

    // Verify a PKCS#1 v1.5 signature over a SHA-256 digest.
    bool Verify(const uint8_t *digest, size_t digest_len,
                const uint8_t *sig, size_t sig_len);

    // Export the RSA public key as DER SubjectPublicKeyInfo.
    Result<SpkiDer> PublicKeyDer();

    // Generate a PKCS#10 CSR signed by the SE key (the private key never leaves
    // the SE).  subject_dn is a RFC 4514 distinguished name string, e.g.
    // "CN=device-001,O=Acme,C=US".
    Result<CsrPem> MakeCsr(const char *subject_dn);

    ~RsaKey();
    RsaKey(RsaKey &&) noexcept;
    RsaKey &operator=(RsaKey &&) = delete;
    RsaKey(const RsaKey &) = delete;
    RsaKey &operator=(const RsaKey &) = delete;

private:
    explicit RsaKey(Connection &conn) : obj_(conn), bits_(2048) {}

    KeyObject obj_;
    size_t bits_;
};

} // namespace etlx::se
