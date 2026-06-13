#include "etlx/crypto/hmac.hpp"

#include <cstring>

namespace etlx::crypto {

namespace {
constexpr size_t kBlock = 64;  // SHA-256 block size
}

void HmacSha256::Init(ConstByteSpan key) {
    uint8_t k[kBlock] = {};

    // Keys longer than the block are hashed down; shorter keys are zero-padded.
    if (key.size() > kBlock) {
        Digest256 kh = Sha256::Hash(key);
        std::memcpy(k, kh.data(), kh.size());
    } else if (!key.empty()) {
        std::memcpy(k, key.data(), key.size());
    }

    uint8_t ikey[kBlock];
    for (size_t i = 0; i < kBlock; ++i) {
        ikey[i]  = static_cast<uint8_t>(k[i] ^ 0x36);
        okey_[i] = static_cast<uint8_t>(k[i] ^ 0x5c);
    }
    inner_.Reset();
    inner_.Update(ConstByteSpan{ikey, kBlock});
}

Status HmacSha256::Update(ConstByteSpan data) {
    return inner_.Update(data);
}

Result<Digest256> HmacSha256::Final() {
    auto inner = inner_.Final();
    if (!inner) return Unexpected<>{inner.error()};

    Sha256 outer;
    outer.Update(ConstByteSpan{okey_, kBlock});
    outer.Update(ConstByteSpan{inner.value().data(), inner.value().size()});
    return outer.Final();
}

Digest256 HmacSha256::Tag(ConstByteSpan key, ConstByteSpan data) {
    HmacSha256 h(key);
    h.Update(data);
    return h.Final().value();
}

Status HmacVerifier::Verify(ConstByteSpan data, ConstByteSpan sig) {
    Digest256 tag = HmacSha256::Tag(key_, data);
    if (!ConstTimeEqual(ConstByteSpan{tag.data(), tag.size()}, sig)) {
        return Fail(Category, VerifyFailed, "HMAC mismatch");
    }
    return Ok();
}

} // namespace etlx::crypto
