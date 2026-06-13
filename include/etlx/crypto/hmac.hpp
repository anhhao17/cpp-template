#ifndef ETLX_CRYPTO_HMAC_HPP
#define ETLX_CRYPTO_HMAC_HPP

#include "etlx/crypto/crypto.hpp"
#include "etlx/crypto/sha256.hpp"

// HMAC-SHA256 built on the software SHA-256, plus a Verifier that checks an
// HMAC tag in constant time. Dependency-free and heap-free. This is the demo's
// stand-in for image-signature verification; an asymmetric (ECDSA) Verifier
// would be a separate backend implementing the same interface.
namespace etlx::crypto {

class HmacSha256 final : public Hasher {
public:
    explicit HmacSha256(ConstByteSpan key) { Init(key); }

    Status            Update(ConstByteSpan data) override;
    Result<Digest256> Final() override;

    // One-shot convenience.
    static Digest256 Tag(ConstByteSpan key, ConstByteSpan data);

private:
    void Init(ConstByteSpan key);

    Sha256  inner_;
    uint8_t okey_[64];  // key XOR opad, for the outer hash
};

// Verifies that `sig` is the correct HMAC-SHA256 tag for the message.
class HmacVerifier final : public Verifier {
public:
    explicit HmacVerifier(ConstByteSpan key) : key_(key) {}
    Status Verify(ConstByteSpan data, ConstByteSpan sig) override;

private:
    ConstByteSpan key_;
};

} // namespace etlx::crypto

#endif // ETLX_CRYPTO_HMAC_HPP
