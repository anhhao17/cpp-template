#ifndef ETLX_CORE_RESULT_HPP
#define ETLX_CORE_RESULT_HPP

#include "etlx/core/error.hpp"

#include <etl/expected.h>

// Result/Status types built on etl::expected, the heap-free analogue of
// Mender's expected::Expected. A function returns a value on success or an
// error::Error on failure, with no exceptions involved.
namespace etlx {

// Holds a T on success or an error::Error on failure.
template <typename T>
using Result = etl::expected<T, error::Error>;

// Holds nothing on success or an error::Error on failure (for void-returning
// operations).
using Status = etl::expected<void, error::Error>;

// Wraps an error so it can be returned where a Result/Status is expected:
//   return etlx::Unexpected{etlx::error::Make(cat, code, "msg")};
template <typename E = error::Error>
using Unexpected = etl::unexpected<E>;

// Convenience: a successful Status.
inline Status Ok() { return Status{}; }

// Convenience: build a failed Result/Status from category/code/message.
inline Unexpected<> Fail(const error::Category& cat, error::Code code,
                         etl::string_view msg) {
    return Unexpected<>{error::Make(cat, code, msg)};
}

} // namespace etlx

#endif // ETLX_CORE_RESULT_HPP
