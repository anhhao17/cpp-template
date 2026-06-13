#include "etlx/crypto/hmac.hpp"
#include "etlx/crypto/sha256.hpp"
#include "etlx/crypto/stream.hpp"
#include "host/host_random.hpp"

#include <cstdint>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

namespace crypto = etlx::crypto;

namespace {

etlx::ConstByteSpan Bytes(const char* s) {
    return etlx::ConstByteSpan{reinterpret_cast<const uint8_t*>(s), std::strlen(s)};
}

std::string Hex(etlx::ConstByteSpan d) {
    static const char* h = "0123456789abcdef";
    std::string out;
    for (uint8_t b : d) { out += h[b >> 4]; out += h[b & 0xF]; }
    return out;
}

std::string Hex(const crypto::Digest256& d) {
    return Hex(etlx::ConstByteSpan{d.data(), d.size()});
}

// Known SHA-256 vectors.
TEST(Sha256, KnownVectors) {
    EXPECT_EQ(Hex(crypto::Sha256::Hash(Bytes("abc"))),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(Hex(crypto::Sha256::Hash(Bytes(""))),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256, MultiBlockInput) {
    // 56 'a's straddles the padding boundary; 1,000,000 chars would be slow, so
    // use a longer-than-block string with a known digest.
    std::string s(64 * 3 + 5, 'a');
    auto d = crypto::Sha256::Hash(
        etlx::ConstByteSpan{reinterpret_cast<const uint8_t*>(s.data()), s.size()});
    // Cross-check: incremental in odd chunks must equal the one-shot result.
    crypto::Sha256 h;
    size_t off = 0;
    for (size_t step : {1u, 7u, 13u, 64u, 100u}) {
        size_t n = std::min(step, s.size() - off);
        h.Update(etlx::ConstByteSpan{reinterpret_cast<const uint8_t*>(s.data()) + off, n});
        off += n;
    }
    h.Update(etlx::ConstByteSpan{reinterpret_cast<const uint8_t*>(s.data()) + off, s.size() - off});
    EXPECT_EQ(Hex(h.Final().value()), Hex(d));
}

TEST(Sha256, UpdateAfterFinalIsBadState) {
    crypto::Sha256 h;
    h.Update(Bytes("x"));
    ASSERT_TRUE(h.Final().has_value());
    auto s = h.Update(Bytes("y"));
    ASSERT_FALSE(s.has_value());
    EXPECT_EQ(s.error().code, crypto::BadState);
}

TEST(HmacSha256, Rfc4231Case2) {
    // key="Jefe", data="what do ya want for nothing?"
    auto tag = crypto::HmacSha256::Tag(Bytes("Jefe"),
                                       Bytes("what do ya want for nothing?"));
    EXPECT_EQ(Hex(tag),
              "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

TEST(HmacVerifier, AcceptsGoodRejectsBad) {
    auto key  = Bytes("shared");
    auto data = Bytes("payload");
    auto tag  = crypto::HmacSha256::Tag(key, data);

    crypto::HmacVerifier v(key);
    EXPECT_TRUE(v.Verify(data, etlx::ConstByteSpan{tag.data(), tag.size()}).has_value());

    // Flip one tag byte.
    crypto::Digest256 bad = tag;
    bad[0] ^= 0x01;
    auto r = v.Verify(data, etlx::ConstByteSpan{bad.data(), bad.size()});
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, crypto::VerifyFailed);
}

TEST(ConstTimeEqual, LengthAndContent) {
    EXPECT_TRUE(crypto::ConstTimeEqual(Bytes("abc"), Bytes("abc")));
    EXPECT_FALSE(crypto::ConstTimeEqual(Bytes("abc"), Bytes("abd")));
    EXPECT_FALSE(crypto::ConstTimeEqual(Bytes("abc"), Bytes("ab")));
}

TEST(HashStream, MatchesOneShot) {
    auto data = Bytes("stream this through small chunks please");
    etlx::io::SpanReader reader(data);
    crypto::Sha256 h;
    uint8_t scratch[8];
    auto d = crypto::HashStream(reader, h, etlx::ByteSpan{scratch, sizeof(scratch)});
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(Hex(d.value()), Hex(crypto::Sha256::Hash(data)));
}

TEST(UrandomSource, FillsAndVaries) {
    etlx::ports::host::UrandomSource rng;
    uint8_t a[32] = {};
    uint8_t b[32] = {};
    ASSERT_TRUE(rng.Fill(etlx::ByteSpan{a, sizeof(a)}).has_value());
    ASSERT_TRUE(rng.Fill(etlx::ByteSpan{b, sizeof(b)}).has_value());
    // Two 32-byte draws being identical is astronomically unlikely.
    EXPECT_NE(0, std::memcmp(a, b, sizeof(a)));
}

} // namespace
