#ifndef ETLX_PORTS_HOST_RANDOM_HPP
#define ETLX_PORTS_HOST_RANDOM_HPP

#include "etlx/crypto/crypto.hpp"

// Host RNG backend: fills buffers from /dev/urandom. An MCU port would instead
// wrap the hardware RNG peripheral behind the same crypto::Random interface.
namespace etlx::ports::host {

class UrandomSource final : public etlx::crypto::Random {
public:
    etlx::Status Fill(etlx::ByteSpan out) override;
};

} // namespace etlx::ports::host

#endif // ETLX_PORTS_HOST_RANDOM_HPP
