// Example: persist state across a simulated reboot. Writes a couple of keys,
// commits to a file, then opens a fresh store from the same file and reads them
// back, exactly the commit/resume pattern an OTA flow relies on.
// Build target: example_kv_persist.
#include "host/host_kv.hpp"

#include <cstdint>
#include <cstdio>

int main() {
    const char* path = "/tmp/etlx_kv_demo.bin";
    std::remove(path);  // start clean

    // First boot: stage some state and commit it.
    {
        etlx::ports::host::FileKeyValueStore kv(path);
        kv.SetString("artifact", "device-image-1.1.0");
        kv.SetString("update_state", "ArtifactInstall_Enter");
        if (auto s = kv.Commit(); !s) {
            std::printf("commit failed: %s\n", s.error().message.c_str());
            return 1;
        }
        std::printf("committed %zu entries\n", kv.size());
    }

    // Second boot: a brand new store, loading from the same file.
    {
        etlx::ports::host::FileKeyValueStore kv(path);
        uint8_t buf[128];
        auto v = kv.GetStringInto("update_state", etlx::ByteSpan{buf, sizeof(buf)});
        if (!v) {
            std::printf("state lost across reboot: %s\n", v.error().message.c_str());
            return 1;
        }
        std::printf("resumed update_state = %.*s\n",
                    static_cast<int>(v.value().size()), v.value().data());
    }
    return 0;
}
