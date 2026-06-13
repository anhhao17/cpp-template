// Example: propagating Result<T> / Status / Error across calls without
// exceptions. Build target: example_result_error.
#include "etlx/core/result.hpp"

#include <cstdio>

namespace {

const etlx::error::Category ParseCategory{"parse"};

enum ParseCode { BadDigit = 1, Overflow = 2 };

// Parses a base-10 unsigned int, returning a value or an Error.
etlx::Result<uint32_t> ParseU32(etl::string_view s) {
    if (s.empty()) return etlx::Fail(ParseCategory, BadDigit, "empty input");
    uint32_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') {
            return etlx::Fail(ParseCategory, BadDigit, "non-digit character");
        }
        v = v * 10 + static_cast<uint32_t>(c - '0');
    }
    return v;  // implicit success
}

void Try(etl::string_view in) {
    auto r = ParseU32(in);
    if (r) {
        std::printf("ok:    \"%.*s\" -> %u\n", static_cast<int>(in.size()),
                    in.data(), r.value());
    } else {
        std::printf("error: \"%.*s\" -> [%s/%d] %s\n", static_cast<int>(in.size()),
                    in.data(), r.error().category->name, r.error().code,
                    r.error().message.c_str());
    }
}

} // namespace

int main() {
    Try("42");
    Try("");
    Try("12x4");
    return 0;
}
