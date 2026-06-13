// Example: hash a buffer by streaming it through an io::Reader into a
// crypto::Hasher, then verify an HMAC tag over the same data, the gate an OTA
// flow uses before applying an image. Build target: example_sha256_stream.
#include "etlx/crypto/hmac.hpp"
#include "etlx/crypto/sha256.hpp"
#include "etlx/crypto/stream.hpp"

#include <cstdint>
#include <cstdio>

namespace {

void PrintHex(const char* label, etlx::ConstByteSpan d) {
    std::printf("%s", label);
    for (uint8_t b : d) std::printf("%02x", b);
    std::printf("\n");
}

} // namespace

int main() {
    const char* msg = "the quick brown fox jumps over the lazy dog";
    etlx::ConstByteSpan data{reinterpret_cast<const uint8_t*>(msg), __builtin_strlen(msg)};

    // Stream the data through an 8-byte scratch buffer into a SHA-256 hasher.
    etlx::io::SpanReader reader(data);
    etlx::crypto::Sha256 hasher;
    uint8_t scratch[8];
    auto digest = etlx::crypto::HashStream(reader, hasher, etlx::ByteSpan{scratch, sizeof(scratch)});
    if (!digest) {
        std::printf("hash failed: %s\n", digest.error().message.c_str());
        return 1;
    }
    PrintHex("sha256 = ", etlx::ConstByteSpan{digest.value().data(), digest.value().size()});

    // Compute and verify an HMAC tag with a shared key.
    const char* keystr = "shared-secret";
    etlx::ConstByteSpan key{reinterpret_cast<const uint8_t*>(keystr), __builtin_strlen(keystr)};
    auto tag = etlx::crypto::HmacSha256::Tag(key, data);
    PrintHex("hmac   = ", etlx::ConstByteSpan{tag.data(), tag.size()});

    etlx::crypto::HmacVerifier verifier(key);
    auto ok = verifier.Verify(data, etlx::ConstByteSpan{tag.data(), tag.size()});
    std::printf("verify = %s\n", ok.has_value() ? "ok" : "FAILED");
    return ok.has_value() ? 0 : 1;
}
