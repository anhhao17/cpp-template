#ifndef ETLX_CORE_SPAN_HPP
#define ETLX_CORE_SPAN_HPP

#include <cstdint>

#include <etl/span.h>

// Non-owning views over contiguous memory, used throughout the io/crypto/http
// modules so buffers are passed without copies. Aliases over etl::span.
namespace etlx {

template <typename T>
using Span = etl::span<T>;

// Common byte-buffer views.
using ByteSpan      = etl::span<uint8_t>;
using ConstByteSpan = etl::span<const uint8_t>;

} // namespace etlx

#endif // ETLX_CORE_SPAN_HPP
