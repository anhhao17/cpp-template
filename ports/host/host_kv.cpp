#include "host/host_kv.hpp"

#include <cstdint>
#include <cstdio>

namespace etlx::ports::host {
namespace {

namespace st = etlx::storage;

constexpr uint8_t kMagic[4] = {'E', 'K', 'V', '1'};

// Little-endian helpers so the file format is identical across native and
// aarch64 builds regardless of host endianness.
void PutU16(std::FILE* f, uint16_t v) {
    std::fputc(v & 0xFF, f);
    std::fputc((v >> 8) & 0xFF, f);
}
void PutU32(std::FILE* f, uint32_t v) {
    for (int i = 0; i < 4; ++i) std::fputc((v >> (8 * i)) & 0xFF, f);
}
bool GetU16(std::FILE* f, uint16_t& out) {
    int a = std::fgetc(f), b = std::fgetc(f);
    if (a == EOF || b == EOF) return false;
    out = static_cast<uint16_t>(a | (b << 8));
    return true;
}
bool GetU32(std::FILE* f, uint32_t& out) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        int c = std::fgetc(f);
        if (c == EOF) return false;
        v |= static_cast<uint32_t>(c) << (8 * i);
    }
    out = v;
    return true;
}

} // namespace

FileKeyValueStore::FileKeyValueStore(etl::string_view path) {
    path_.assign(path.data(), path.size());
    Load();  // best effort; a missing file just leaves the store empty
}

etlx::Status FileKeyValueStore::Load() {
    map_.clear();

    std::FILE* f = std::fopen(path_.c_str(), "rb");
    if (f == nullptr) return etlx::Ok();  // no file yet: empty store

    uint8_t magic[4];
    if (std::fread(magic, 1, 4, f) != 4 ||
        magic[0] != kMagic[0] || magic[1] != kMagic[1] ||
        magic[2] != kMagic[2] || magic[3] != kMagic[3]) {
        std::fclose(f);
        return etlx::Fail(st::Category, st::IoError, "bad kv file magic");
    }

    uint32_t count = 0;
    if (!GetU32(f, count)) {
        std::fclose(f);
        return etlx::Fail(st::Category, st::IoError, "truncated kv header");
    }

    etlx::Status result = etlx::Ok();
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t klen = 0;
        uint32_t vlen = 0;
        char keybuf[ETLX_KV_KEY_CAPACITY];
        uint8_t valbuf[ETLX_KV_VALUE_CAPACITY];

        if (!GetU16(f, klen) || klen > ETLX_KV_KEY_CAPACITY ||
            std::fread(keybuf, 1, klen, f) != klen ||
            !GetU32(f, vlen) || vlen > ETLX_KV_VALUE_CAPACITY ||
            std::fread(valbuf, 1, vlen, f) != vlen) {
            result = etlx::Fail(st::Category, st::IoError, "truncated kv entry");
            break;
        }
        Set(etl::string_view{keybuf, klen}, etlx::ConstByteSpan{valbuf, vlen});
    }
    std::fclose(f);
    return result;
}

etlx::Status FileKeyValueStore::Commit() {
    etl::string<260> tmp = path_;
    tmp.append(".tmp");

    std::FILE* f = std::fopen(tmp.c_str(), "wb");
    if (f == nullptr) {
        return etlx::Fail(st::Category, st::IoError, "cannot open temp kv file");
    }

    std::fwrite(kMagic, 1, 4, f);
    PutU32(f, static_cast<uint32_t>(map_.size()));
    for (const auto& kv : map_) {
        PutU16(f, static_cast<uint16_t>(kv.first.size()));
        std::fwrite(kv.first.data(), 1, kv.first.size(), f);
        PutU32(f, static_cast<uint32_t>(kv.second.size()));
        if (!kv.second.empty()) std::fwrite(kv.second.data(), 1, kv.second.size(), f);
    }

    const bool ok = (std::fflush(f) == 0);
    std::fclose(f);
    if (!ok) return etlx::Fail(st::Category, st::IoError, "kv write failed");

    if (std::rename(tmp.c_str(), path_.c_str()) != 0) {
        return etlx::Fail(st::Category, st::IoError, "kv rename failed");
    }
    return etlx::Ok();
}

} // namespace etlx::ports::host
