#ifndef ETLX_CRYPTO_CRYPTO_HPP
#define ETLX_CRYPTO_CRYPTO_HPP

#include "etlx/core/result.hpp"
#include "etlx/core/span.hpp"

#include <cstddef>

#include <etl/array.h>

// Crypto interfaces: hashing, signature verification, and randomness. Concrete
// backends are swappable — this tree ships a dependency-free software SHA-256 /
// HMAC implementation (crypto/sha256.hpp, crypto/hmac.hpp) and a host RNG, but
// an mbedTLS / wolfSSL port can implement the same interfaces.
namespace etlx::crypto {

extern const error::Category Category;

enum Code : error::Code {
    None         = 0,
    BadState     = 1,  // e.g. Update after Final
    VerifyFailed = 2,  // signature/tag mismatch
    RandomFailed = 3,  // RNG source unavailable
};

inline constexpr size_t kSha256DigestSize = 32;
using Digest256 = etl::array<uint8_t, kSha256DigestSize>;

// Incremental hash: feed data with Update, then take the digest with Final.
// Using the hasher after Final returns BadState until Reset/reconstruction.
struct Hasher {
    virtual Status            Update(ConstByteSpan data) = 0;
    virtual Result<Digest256> Final() = 0;
    virtual ~Hasher() = default;
};

// Verifies a signature/tag over a message. The meaning of `sig` depends on the
// concrete verifier (HMAC tag, ECDSA signature, ...).
struct Verifier {
    virtual Status Verify(ConstByteSpan data, ConstByteSpan sig) = 0;
    virtual ~Verifier() = default;
};

// Fills a buffer with cryptographically-random bytes.
struct Random {
    virtual Status Fill(ByteSpan out) = 0;
    virtual ~Random() = default;
};

// Constant-time comparison, so verification doesn't leak timing. Returns true
// iff the spans are equal in length and content.
bool ConstTimeEqual(ConstByteSpan a, ConstByteSpan b);

} // namespace etlx::crypto

#endif // ETLX_CRYPTO_CRYPTO_HPP
