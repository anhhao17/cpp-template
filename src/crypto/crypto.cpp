#include "etlx/crypto/crypto.hpp"

namespace etlx::crypto {

const error::Category Category{"crypto"};

bool ConstTimeEqual(ConstByteSpan a, ConstByteSpan b) {
    if (a.size() != b.size()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

} // namespace etlx::crypto
