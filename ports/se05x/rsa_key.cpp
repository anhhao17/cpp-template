#include <etlx/se/rsa_key.hpp>
#include <etlx/log/log.hpp>

#include <cstring>

extern "C" {
#include <fsl_sss_api.h>
#include <fsl_sss_se05x_policy.h>
#include "mbedtls/asn1write.h"
#include "mbedtls/oid.h"
#include "mbedtls/pem.h"
#include "mbedtls/sha256.h"
#include "mbedtls/x509.h"
}

// mbedtls_x509_write_names was removed from the public header in mbedTLS 3.x
// but is still compiled into the library (library/x509_create.c line 453).
// We need it to encode the subject DN into the CertificationRequestInfo.
extern "C" int mbedtls_x509_write_names(unsigned char **p, unsigned char *start,
                                         mbedtls_asn1_named_data *first);

namespace etlx::se {

namespace {

Status ApplyPolicy(Connection &conn, KeyObject &obj, uint32_t key_id, RsaBits bits,
                   KeyPolicy policy) {
    if (policy == KeyPolicy::Full) {
        sss_status_t st =
            sss_key_store_generate_key(conn.keystore(), obj.raw(),
                                       static_cast<uint32_t>(bits), nullptr);
        if (st != kStatus_SSS_Success)
            return SeFail(kKeyFailed, "sss_key_store_generate_key failed");
        return Ok();
    }

    sss_policy_u asym{};
    asym.type = KPolicy_Asym_Key;
    asym.policy.asymmkey.can_Sign = 1;
    asym.policy.asymmkey.can_Decrypt = (policy == KeyPolicy::SignDecrypt) ? 1 : 0;
    asym.policy.asymmkey.can_Gen = 1;

    sss_policy_u common{};
    common.type = KPolicy_Common;
    common.policy.common.can_Read = 1;
    common.policy.common.can_Write = 0;
    common.policy.common.can_Delete = 0;
    common.policy.common.req_Sm = 1;

    sss_policy_t pol{};
    pol.policies[0] = &asym;
    pol.policies[1] = &common;
    pol.nPolicies = 2;

    sss_status_t st =
        sss_key_store_generate_key(conn.keystore(), obj.raw(),
                                   static_cast<uint32_t>(bits), &pol);
    if (st != kStatus_SSS_Success)
        return SeFail(kKeyFailed, "sss_key_store_generate_key (policy) failed");
    return Ok();
}

} // namespace

Result<RsaKey> RsaKey::Generate(Connection &conn, uint32_t key_id, RsaBits bits,
                                 KeyPolicy policy) {
    RsaKey k(conn);
    k.bits_ = static_cast<size_t>(bits);

    auto st = k.obj_.Allocate(key_id, kSSS_KeyPart_Pair, kSSS_CipherType_RSA, k.bits_ / 8);
    if (!st)
        return SeFail(kKeyFailed, "key_object_allocate_handle failed");

    auto gen_st = ApplyPolicy(conn, k.obj_, key_id, bits, policy);
    if (!gen_st)
        return Unexpected<>{gen_st.error()};

    ETLX_LOG_INFO("se: generated RSA-%zu key id=0x%08x", k.bits_,
                  static_cast<unsigned>(key_id));
    return k;
}

Result<RsaKey> RsaKey::Open(Connection &conn, uint32_t key_id) {
    RsaKey k(conn);
    auto st = k.obj_.Open(key_id);
    if (!st)
        return Unexpected<>{st.error()};

    ETLX_LOG_DEBUG("se: opened RSA key id=0x%08x", static_cast<unsigned>(key_id));
    return k;
}

Result<RsaKey::SigBytes> RsaKey::Sign(const uint8_t *digest, size_t digest_len) {
    sss_asymmetric_t ctx{};
    sss_status_t st = sss_asymmetric_context_init(&ctx, obj_.conn().session(), obj_.raw(),
                                                  kAlgorithm_SSS_RSASSA_PKCS1_V1_5_SHA256,
                                                  kMode_SSS_Sign);
    if (st != kStatus_SSS_Success)
        return SeFail(kSignFailed, "asymmetric_context_init(sign) failed");

    SigBytes sig;
    sig.resize(bits_ / 8);
    size_t sig_len = sig.size();
    st = sss_asymmetric_sign_digest(&ctx, const_cast<uint8_t *>(digest), digest_len,
                                    sig.data(), &sig_len);
    sss_asymmetric_context_free(&ctx);

    if (st != kStatus_SSS_Success)
        return SeFail(kSignFailed, "asymmetric_sign_digest failed");

    sig.resize(sig_len);
    return sig;
}

bool RsaKey::Verify(const uint8_t *digest, size_t digest_len,
                    const uint8_t *sig, size_t sig_len) {
    sss_asymmetric_t ctx{};
    if (sss_asymmetric_context_init(&ctx, obj_.conn().session(), obj_.raw(),
                                    kAlgorithm_SSS_RSASSA_PKCS1_V1_5_SHA256,
                                    kMode_SSS_Verify) != kStatus_SSS_Success)
        return false;

    sss_status_t st = sss_asymmetric_verify_digest(
        &ctx, const_cast<uint8_t *>(digest), digest_len,
        const_cast<uint8_t *>(sig), sig_len);
    sss_asymmetric_context_free(&ctx);
    return st == kStatus_SSS_Success;
}

Result<RsaKey::SpkiDer> RsaKey::PublicKeyDer() {
    SpkiDer buf;
    buf.resize(ETLX_SE_SPKI_CAPACITY);
    size_t len = buf.size();
    size_t bits = 0;
    sss_status_t st =
        sss_key_store_get_key(obj_.conn().keystore(), obj_.raw(), buf.data(), &len, &bits);
    if (st != kStatus_SSS_Success)
        return SeFail(kKeyFailed, "sss_key_store_get_key(SPKI) failed");
    buf.resize(len);
    return buf;
}

// CSR generation: ASN.1/PEM helper internals.
// The private key never leaves the SE — only the digest is signed.
namespace {

#define CHK(expr)                           \
    do {                                    \
        int _r = (expr);                    \
        if (_r < 0) return _r;              \
        written += _r;                      \
    } while (0)

// Build DER CertificationRequestInfo into buf[0..buf_size), writing
// backwards from buf+buf_size. Returns number of bytes written or <0 on error.
int BuildCri(uint8_t *buf, size_t buf_size, const char *subject_dn,
             const uint8_t *spki, size_t spki_len) {
    unsigned char *start = buf;
    unsigned char *c = buf + buf_size;
    int written = 0;

    mbedtls_asn1_named_data *names = nullptr;
    if (mbedtls_x509_string_to_names(&names, subject_dn) != 0)
        return -1;

    // attributes [0] -- empty SET
    CHK(mbedtls_asn1_write_len(&c, start, 0));
    CHK(mbedtls_asn1_write_tag(&c, start,
        MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_ASN1_CONSTRUCTED | 0));

    // subjectPKInfo
    if (static_cast<size_t>(c - start) < spki_len) {
        mbedtls_asn1_free_named_data_list(&names);
        return -1;
    }
    c -= spki_len;
    std::memcpy(c, spki, spki_len);
    written += static_cast<int>(spki_len);

    // subject Name
    int r = mbedtls_x509_write_names(&c, start, names);
    mbedtls_asn1_free_named_data_list(&names);
    if (r < 0) return r;
    written += r;

    // version INTEGER 0
    CHK(mbedtls_asn1_write_int(&c, start, 0));

    // wrap in SEQUENCE
    CHK(mbedtls_asn1_write_len(&c, start, written));
    CHK(mbedtls_asn1_write_tag(&c, start, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    return written;
}

// Assemble DER CertificationRequest into buf, then PEM-encode into pem_out.
bool AssembleCsr(const uint8_t *cri, size_t cri_len,
                 const uint8_t *sig, size_t sig_len,
                 char *pem_out, size_t pem_capacity) {
    uint8_t buf[ETLX_SE_CSR_PEM_CAPACITY];
    unsigned char *start = buf;
    unsigned char *c = buf + sizeof(buf);
    int written = 0;

    CHK(mbedtls_asn1_write_bitstring(&c, start,
        reinterpret_cast<const unsigned char *>(sig), sig_len * 8));

    // signatureAlgorithm: sha256WithRSAEncryption
    {
        const char *oid = MBEDTLS_OID_PKCS1_SHA256;
        size_t oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_PKCS1_SHA256);
        int ar = mbedtls_asn1_write_algorithm_identifier(&c, start, oid, oid_len, 0);
        if (ar < 0) return false;
        written += ar;
    }

    if (static_cast<size_t>(c - start) < cri_len) return false;
    c -= cri_len;
    std::memcpy(c, cri, cri_len);
    written += static_cast<int>(cri_len);

    CHK(mbedtls_asn1_write_len(&c, start, written));
    CHK(mbedtls_asn1_write_tag(&c, start, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

    size_t olen = 0;
    int r = mbedtls_pem_write_buffer(
        "-----BEGIN CERTIFICATE REQUEST-----\n",
        "-----END CERTIFICATE REQUEST-----\n",
        c, written, reinterpret_cast<unsigned char *>(pem_out), pem_capacity, &olen);
    return (r == 0);
}

#undef CHK

} // namespace

Result<RsaKey::CsrPem> RsaKey::MakeCsr(const char *subject_dn) {
    auto spki_res = PublicKeyDer();
    if (!spki_res)
        return Unexpected<>{spki_res.error()};
    const SpkiDer &spki = spki_res.value();

    // Build CRI into a stack buffer.
    uint8_t cri_buf[ETLX_SE_CSR_PEM_CAPACITY];
    int cri_len = BuildCri(cri_buf, sizeof(cri_buf), subject_dn, spki.data(), spki.size());
    if (cri_len < 0)
        return SeFail(kKeyFailed, "buildCri failed");

    // SHA-256 the CRI.
    uint8_t digest[32];
    if (mbedtls_sha256(cri_buf + (sizeof(cri_buf) - static_cast<size_t>(cri_len)),
                       static_cast<size_t>(cri_len), digest, 0) != 0)
        return SeFail(kKeyFailed, "sha256 over CRI failed");

    // Sign the digest with the SE key.
    auto sig_res = Sign(digest, sizeof(digest));
    if (!sig_res)
        return Unexpected<>{sig_res.error()};
    const SigBytes &sig = sig_res.value();

    // Assemble and PEM-encode.
    CsrPem pem;
    pem.resize(ETLX_SE_CSR_PEM_CAPACITY);
    const uint8_t *cri_start =
        cri_buf + sizeof(cri_buf) - static_cast<size_t>(cri_len);
    if (!AssembleCsr(cri_start, static_cast<size_t>(cri_len),
                     sig.data(), sig.size(), pem.data(), pem.size()))
        return SeFail(kKeyFailed, "AssembleCsr/PEM failed");

    pem.resize(std::strlen(pem.data()));
    ETLX_LOG_INFO("se: CSR generated (%zu bytes)", pem.size());
    return pem;
}

RsaKey::RsaKey(RsaKey &&o) noexcept : obj_(std::move(o.obj_)), bits_(o.bits_) {}

RsaKey::~RsaKey() = default;

} // namespace etlx::se
