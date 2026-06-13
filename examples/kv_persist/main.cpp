// Example: persist state across a simulated reboot. Writes a couple of keys,
// commits to a file, then opens a fresh store from the same file and reads them
// back, exactly the commit/resume pattern an OTA flow relies on. Output via
// etlx::log. Build target: example_kv_persist.
#include "etlx/log/log.hpp"
#include "host/host_io.hpp"
#include "host/host_kv.hpp"

#include <cstdint>
#include <cstdio>

int main() {
    static etlx::ports::host::StderrLogSink sink;
    etlx::log::SetSink(&sink);

    const char* path = "/tmp/etlx_kv_demo.bin";
    std::remove(path);  // start clean

    // First boot: stage some state and commit it.
    {
        etlx::ports::host::FileKeyValueStore kv(path);
        kv.SetString("artifact", "device-image-1.1.0");
        kv.SetString("update_state", "ArtifactInstall_Enter");
        if (auto s = kv.Commit(); !s) {
            ETLX_LOG_ERROR("commit failed: %s", s.error().message.c_str());
            return 1;
        }
        ETLX_LOG_INFO("committed %zu entries", kv.size());
    }

    // Second boot: a brand new store, loading from the same file.
    {
        etlx::ports::host::FileKeyValueStore kv(path);
        uint8_t buf[128];
        auto v = kv.GetStringInto("update_state", etlx::ByteSpan{buf, sizeof(buf)});
        if (!v) {
            ETLX_LOG_ERROR("state lost across reboot: %s", v.error().message.c_str());
            return 1;
        }
        ETLX_LOG_INFO("resumed update_state = %.*s",
                      static_cast<int>(v.value().size()), v.value().data());
    }
    return 0;
}
