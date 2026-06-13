// Example: parse a small config/manifest blob into fields with no heap. The
// token array lives on the stack; the Json view navigates it by reference.
// Output via etlx::log. Build target: example_json_config.
#include "etlx/json/json.hpp"
#include "etlx/log/log.hpp"
#include "host/host_io.hpp"

#include <etl/string.h>

int main() {
    static etlx::ports::host::StderrLogSink sink;
    etlx::log::SetSink(&sink);

    static const char kManifest[] =
        "{"
        "  \"artifact_name\": \"device-image-1.1.0\","
        "  \"version\": 3,"
        "  \"compatible\": [\"rpi4\", \"imx8\"],"
        "  \"signed\": true"
        "}";

    etlx::json::Token tokens[32];
    auto root = etlx::json::ParseToView(kManifest, etlx::Span<etlx::json::Token>{tokens, 32});
    if (!root) {
        ETLX_LOG_ERROR("parse error: %s", root.error().message.c_str());
        return 1;
    }

    const etlx::json::Json& doc = root.value();
    auto name      = doc["artifact_name"].AsString();
    auto version   = doc["version"].AsInt();
    auto is_signed = doc["signed"].AsBool();

    ETLX_LOG_INFO("artifact_name = %.*s", static_cast<int>(name.size()), name.data());
    if (version)   ETLX_LOG_INFO("version       = %lld", static_cast<long long>(version.value()));
    if (is_signed) ETLX_LOG_INFO("signed        = %s", is_signed.value() ? "yes" : "no");

    auto compatible = doc["compatible"];
    etl::string<64> list;
    for (size_t i = 0; i < compatible.size(); ++i) {
        auto e = compatible[i].AsString();
        if (i) list.append(", ");
        list.append(e.data(), e.size());
    }
    ETLX_LOG_INFO("compatible    = %zu entries: %s", compatible.size(), list.c_str());
    return 0;
}
