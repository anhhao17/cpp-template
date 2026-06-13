#include "etlx/storage/kv.hpp"
#include "host/host_kv.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

namespace {

etlx::ConstByteSpan Bytes(const char* s) {
    return etlx::ConstByteSpan{reinterpret_cast<const uint8_t*>(s), std::strlen(s)};
}

std::string GetStr(etlx::storage::KeyValueStore& kv, const char* key) {
    uint8_t buf[256];
    auto n = kv.Get(key, etlx::ByteSpan{buf, sizeof(buf)});
    if (!n) return std::string("<err:") + std::to_string(n.error().code) + ">";
    return std::string(reinterpret_cast<char*>(buf), n.value());
}

TEST(MemoryKv, SetGetHasRemove) {
    etlx::storage::MemoryKeyValueStore kv;

    EXPECT_FALSE(kv.Has("a"));
    EXPECT_TRUE(kv.Set("a", Bytes("alpha")).has_value());
    EXPECT_TRUE(kv.Has("a"));
    EXPECT_EQ(GetStr(kv, "a"), "alpha");

    // Overwrite.
    EXPECT_TRUE(kv.Set("a", Bytes("alpha2")).has_value());
    EXPECT_EQ(GetStr(kv, "a"), "alpha2");

    EXPECT_TRUE(kv.Remove("a").has_value());
    EXPECT_FALSE(kv.Has("a"));
}

TEST(MemoryKv, GetMissingIsNotFound) {
    etlx::storage::MemoryKeyValueStore kv;
    uint8_t buf[8];
    auto r = kv.Get("nope", etlx::ByteSpan{buf, sizeof(buf)});
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, etlx::storage::NotFound);
}

TEST(MemoryKv, GetIntoTooSmallBufferFails) {
    etlx::storage::MemoryKeyValueStore kv;
    kv.Set("k", Bytes("0123456789"));
    uint8_t small[4];
    auto r = kv.Get("k", etlx::ByteSpan{small, sizeof(small)});
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, etlx::storage::TooLarge);
}

TEST(MemoryKv, RejectsOversizedValue) {
    etlx::storage::MemoryKeyValueStore kv;
    uint8_t big[ETLX_KV_VALUE_CAPACITY + 1] = {};
    auto r = kv.Set("k", etlx::ConstByteSpan{big, sizeof(big)});
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, etlx::storage::TooLarge);
}

TEST(MemoryKv, FullWhenAtCapacity) {
    etlx::storage::MemoryKeyValueStore kv;
    char key[8];
    for (int i = 0; i < ETLX_KV_MAX_ENTRIES; ++i) {
        std::snprintf(key, sizeof(key), "k%d", i);
        ASSERT_TRUE(kv.Set(key, Bytes("v")).has_value());
    }
    auto r = kv.Set("overflow", Bytes("v"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code, etlx::storage::Full);
}

TEST(FileKv, PersistsAcrossReopen) {
    const char* path = "/tmp/etlx_kv_test.bin";
    std::remove(path);

    {
        etlx::ports::host::FileKeyValueStore kv(path);
        kv.SetString("artifact", "img-1.2.3");
        kv.SetString("state", "Download_Enter");
        ASSERT_TRUE(kv.Commit().has_value());
    }
    {
        etlx::ports::host::FileKeyValueStore kv(path);  // reload from disk
        EXPECT_EQ(GetStr(kv, "artifact"), "img-1.2.3");
        EXPECT_EQ(GetStr(kv, "state"), "Download_Enter");
    }
    std::remove(path);
}

TEST(FileKv, UncommittedChangesAreLost) {
    const char* path = "/tmp/etlx_kv_test2.bin";
    std::remove(path);

    {
        etlx::ports::host::FileKeyValueStore kv(path);
        kv.SetString("x", "committed");
        ASSERT_TRUE(kv.Commit().has_value());
        kv.SetString("x", "dirty");  // not committed
    }
    {
        etlx::ports::host::FileKeyValueStore kv(path);
        EXPECT_EQ(GetStr(kv, "x"), "committed");
    }
    std::remove(path);
}

} // namespace
