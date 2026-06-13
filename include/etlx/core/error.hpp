#ifndef ETLX_CORE_ERROR_HPP
#define ETLX_CORE_ERROR_HPP

#include "etlx/etlx_config.hpp"

#include <etl/string.h>
#include <etl/string_view.h>

// Value-based error model, mirroring Mender's category + code + message errors
// but with a fixed-capacity message so an Error is cheap to copy and never
// allocates. An Error with a null category means "no error".
namespace etlx::error {

using Code = int;

// Groups related error codes, much like std::error_category but trivial. A
// category is a static object; errors hold a pointer to it.
struct Category {
    const char* name = "";
};

using Message = etl::string<ETLX_ERROR_MESSAGE_CAPACITY>;

struct Error {
    const Category* category = nullptr;  // nullptr means NoError
    Code            code     = 0;
    Message         message;

    // True when this represents success (no error).
    constexpr bool ok() const { return category == nullptr; }

    // Truthy when there IS an error, so callers can write `if (err) { ... }`.
    explicit operator bool() const { return !ok(); }
};

// The canonical "no error" value.
inline const Error NoError{};

// Builds an Error, truncating the message to the fixed message capacity.
inline Error Make(const Category& cat, Code code, etl::string_view msg) {
    Error e;
    e.category = &cat;
    e.code     = code;
    e.message.assign(msg.data(), msg.size());
    return e;
}

} // namespace etlx::error

#endif // ETLX_CORE_ERROR_HPP
