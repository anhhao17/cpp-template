// Example: parse a small config/manifest blob into fields with no heap. The
// token array lives on the stack; the Json view navigates it by reference.
// Build target: example_json_config.
#include "etlx/json/json.hpp"

#include <cstdio>

int main() {
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
        std::printf("parse error: %s\n", root.error().message.c_str());
        return 1;
    }

    const etlx::json::Json& doc = root.value();
    auto name    = doc["artifact_name"].AsString();
    auto version = doc["version"].AsInt();
    auto is_signed = doc["signed"].AsBool();

    std::printf("artifact_name = %.*s\n", static_cast<int>(name.size()), name.data());
    if (version)   std::printf("version       = %lld\n", static_cast<long long>(version.value()));
    if (is_signed) std::printf("signed        = %s\n", is_signed.value() ? "yes" : "no");

    auto compatible = doc["compatible"];
    std::printf("compatible    = %zu entries:", compatible.size());
    for (size_t i = 0; i < compatible.size(); ++i) {
        auto e = compatible[i].AsString();
        std::printf(" %.*s", static_cast<int>(e.size()), e.data());
    }
    std::printf("\n");
    return 0;
}
