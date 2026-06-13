#include "host/host_random.hpp"

#include <cstdio>

namespace etlx::ports::host {

etlx::Status UrandomSource::Fill(etlx::ByteSpan out) {
    if (out.empty()) return etlx::Ok();

    std::FILE* f = std::fopen("/dev/urandom", "rb");
    if (f == nullptr) {
        return etlx::Fail(etlx::crypto::Category, etlx::crypto::RandomFailed,
                          "cannot open /dev/urandom");
    }
    const size_t n = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (n != out.size()) {
        return etlx::Fail(etlx::crypto::Category, etlx::crypto::RandomFailed,
                          "short read from /dev/urandom");
    }
    return etlx::Ok();
}

} // namespace etlx::ports::host
