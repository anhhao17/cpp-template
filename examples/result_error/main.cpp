// Example: propagating Result<T> / Status / Error across calls without
// exceptions. Output goes through etlx::log. Build target: example_result_error.
#include "etlx/core/result.hpp"
#include "etlx/log/log.hpp"
#include "host/host_io.hpp"

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
        ETLX_LOG_INFO("ok:    \"%.*s\" -> %u",
                      static_cast<int>(in.size()), in.data(), r.value());
    } else {
        ETLX_LOG_ERROR("error: \"%.*s\" -> [%s/%d] %s",
                       static_cast<int>(in.size()), in.data(),
                       r.error().category->name, r.error().code,
                       r.error().message.c_str());
    }
}

} // namespace

int main() {
    static etlx::ports::host::StderrLogSink sink;
    etlx::log::SetSink(&sink);

    Try("42");
    Try("");
    Try("12x4");
    return 0;
}
