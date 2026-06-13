#pragma once

#include <etlx/crypto/crypto.hpp>
#include <etlx/se/connection.hpp>

namespace etlx::se {

// Implements etlx::crypto::Random backed by the SE05x hardware TRNG.
// The SE session must outlive this object.
class SeRandom final : public crypto::Random {
public:
    explicit SeRandom(Connection &conn) : conn_(conn) {}

    // Fills out with random bytes from the SE05x hardware TRNG.
    Status Fill(ByteSpan out) override;

private:
    Connection &conn_;
};

} // namespace etlx::se
