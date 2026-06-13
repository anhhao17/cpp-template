// Example: hash a buffer by streaming it through an io::Reader into a
// crypto::Hasher, then verify an HMAC tag over the same data, the gate an OTA
// flow uses before applying an image. Output via etlx::log.
// Build target: example_sha256_stream.
#include "etlx/crypto/hmac.hpp"
#include "etlx/crypto/sha256.hpp"
#include "etlx/crypto/stream.hpp"
#include "etlx/log/log.hpp"
#include "host/host_io.hpp"

#include <cstdint>

#include <etl/string.h>

namespace {

// Formats bytes as lowercase hex into a fixed string for logging.
etl::string<2 * etlx::crypto::kSha256DigestSize + 1> Hex(etlx::ConstByteSpan d) {
    static const char* digits = "0123456789abcdef";
    etl::string<2 * etlx::crypto::kSha256DigestSize + 1> out;
    for (uint8_t b : d) {
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 0x0F]);
    }
    return out;
}

} // namespace

int main() {
    static etlx::ports::host::StderrLogSink sink;
    etlx::log::SetSink(&sink);

    const char* msg = "the quick brown fox jumps over the lazy dog";
    etlx::ConstByteSpan data{reinterpret_cast<const uint8_t*>(msg), __builtin_strlen(msg)};

    // Stream the data through an 8-byte scratch buffer into a SHA-256 hasher.
    etlx::io::SpanReader reader(data);
    etlx::crypto::Sha256 hasher;
    uint8_t scratch[8];
    auto digest = etlx::crypto::HashStream(reader, hasher, etlx::ByteSpan{scratch, sizeof(scratch)});
    if (!digest) {
        ETLX_LOG_ERROR("hash failed: %s", digest.error().message.c_str());
        return 1;
    }
    ETLX_LOG_INFO("sha256 = %s",
                  Hex(etlx::ConstByteSpan{digest.value().data(), digest.value().size()}).c_str());

    // Compute and verify an HMAC tag with a shared key.
    const char* keystr = "shared-secret";
    etlx::ConstByteSpan key{reinterpret_cast<const uint8_t*>(keystr), __builtin_strlen(keystr)};
    auto tag = etlx::crypto::HmacSha256::Tag(key, data);
    ETLX_LOG_INFO("hmac   = %s", Hex(etlx::ConstByteSpan{tag.data(), tag.size()}).c_str());

    etlx::crypto::HmacVerifier verifier(key);
    auto ok = verifier.Verify(data, etlx::ConstByteSpan{tag.data(), tag.size()});
    ETLX_LOG_INFO("verify = %s", ok.has_value() ? "ok" : "FAILED");
    return ok.has_value() ? 0 : 1;
}
