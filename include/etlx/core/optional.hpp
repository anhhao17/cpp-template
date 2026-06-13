#ifndef ETLX_CORE_OPTIONAL_HPP
#define ETLX_CORE_OPTIONAL_HPP

#include <etl/optional.h>

// A maybe-present value, with no heap use. Thin alias over etl::optional so the
// rest of the codebase refers to a single etlx type.
namespace etlx {

template <typename T>
using Optional = etl::optional<T>;

inline constexpr etl::nullopt_t NullOpt = etl::nullopt;

} // namespace etlx

#endif // ETLX_CORE_OPTIONAL_HPP
