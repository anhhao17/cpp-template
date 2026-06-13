#include "etlx/storage/kv.hpp"

#include <cstring>

namespace etlx::storage {

const error::Category Category{"storage"};

namespace {

// Builds a fixed Key from a view, or reports TooLarge if it would not fit.
Result<Key> MakeKey(etl::string_view key) {
    if (key.size() > ETLX_KV_KEY_CAPACITY) {
        return Fail(Category, TooLarge, "key too long");
    }
    Key k;
    k.assign(key.data(), key.size());
    return k;
}

} // namespace

Result<size_t> MemoryKeyValueStore::Get(etl::string_view key, ByteSpan out) {
    auto k = MakeKey(key);
    if (!k) return Unexpected<>{k.error()};

    auto it = map_.find(k.value());
    if (it == map_.end()) {
        return Fail(Category, NotFound, "key not found");
    }
    const Value& v = it->second;
    if (v.size() > out.size()) {
        return Fail(Category, TooLarge, "output buffer too small");
    }
    if (!v.empty()) std::memcpy(out.data(), v.data(), v.size());
    return v.size();
}

bool MemoryKeyValueStore::Has(etl::string_view key) {
    auto k = MakeKey(key);
    if (!k) return false;
    return map_.find(k.value()) != map_.end();
}

Status MemoryKeyValueStore::Set(etl::string_view key, ConstByteSpan value) {
    auto k = MakeKey(key);
    if (!k) return Unexpected<>{k.error()};
    if (value.size() > ETLX_KV_VALUE_CAPACITY) {
        return Fail(Category, TooLarge, "value too long");
    }

    auto it = map_.find(k.value());
    if (it == map_.end()) {
        if (map_.full()) return Fail(Category, Full, "store full");
        Value v;
        v.assign(value.begin(), value.end());
        map_.insert({k.value(), v});
    } else {
        it->second.assign(value.begin(), value.end());
    }
    return Ok();
}

Status MemoryKeyValueStore::Remove(etl::string_view key) {
    auto k = MakeKey(key);
    if (!k) return Unexpected<>{k.error()};
    map_.erase(k.value());  // erasing an absent key is harmless
    return Ok();
}

Status MemoryKeyValueStore::SetString(etl::string_view key, etl::string_view value) {
    return Set(key, ConstByteSpan{reinterpret_cast<const uint8_t*>(value.data()),
                                  value.size()});
}

Result<etl::string_view> MemoryKeyValueStore::GetStringInto(etl::string_view key,
                                                            ByteSpan scratch) {
    auto n = Get(key, scratch);
    if (!n) return Unexpected<>{n.error()};
    return etl::string_view{reinterpret_cast<const char*>(scratch.data()),
                            n.value()};
}

} // namespace etlx::storage
