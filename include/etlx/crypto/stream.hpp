#ifndef ETLX_CRYPTO_STREAM_HPP
#define ETLX_CRYPTO_STREAM_HPP

#include "etlx/crypto/crypto.hpp"
#include "etlx/io/io.hpp"

// Bridges the io and crypto modules: an io::Writer that feeds everything
// written into a Hasher. Combined with io::Copy it lets you hash a file/flash
// region streaming through a small buffer, never holding the whole payload.
namespace etlx::crypto {

class HasherWriter final : public io::Writer {
public:
    explicit HasherWriter(Hasher& hasher) : hasher_(hasher) {}
    Status Write(ConstByteSpan src) override { return hasher_.Update(src); }

private:
    Hasher& hasher_;
};

// Streams a reader through `scratch` into the hasher and returns the digest.
inline Result<Digest256> HashStream(io::Reader& src, Hasher& hasher,
                                    ByteSpan scratch) {
    HasherWriter sink(hasher);
    if (auto n = io::Copy(src, sink, scratch); !n) {
        return Unexpected<>{n.error()};
    }
    return hasher.Final();
}

} // namespace etlx::crypto

#endif // ETLX_CRYPTO_STREAM_HPP
