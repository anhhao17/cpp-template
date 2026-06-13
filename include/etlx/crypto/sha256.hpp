#ifndef ETLX_CRYPTO_SHA256_HPP
#define ETLX_CRYPTO_SHA256_HPP

#include "etlx/crypto/crypto.hpp"

#include <cstdint>

// Dependency-free software SHA-256, implementing the Hasher interface. State is
// a fixed 64-byte block plus eight words, so it allocates nothing and is safe
// on bare-metal. Portable across native and aarch64 (no intrinsics).
namespace etlx::crypto {

class Sha256 final : public Hasher {
public:
    Sha256() { Reset(); }

    void   Reset();
    Status Update(ConstByteSpan data) override;
    Result<Digest256> Final() override;

    // One-shot convenience.
    static Digest256 Hash(ConstByteSpan data);

private:
    void Compress(const uint8_t block[64]);

    uint32_t state_[8];
    uint64_t bitlen_ = 0;
    uint8_t  buffer_[64];
    size_t   buffered_ = 0;
    bool     finalized_ = false;
};

} // namespace etlx::crypto

#endif // ETLX_CRYPTO_SHA256_HPP
