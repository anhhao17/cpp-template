#ifndef ETLX_STORAGE_KV_HPP
#define ETLX_STORAGE_KV_HPP

#include "etlx/core/result.hpp"
#include "etlx/core/span.hpp"
#include "etlx/etlx_config.hpp"

#include <etl/flat_map.h>
#include <etl/string.h>
#include <etl/string_view.h>
#include <etl/vector.h>

// Small persistent key-value store, the analogue of Mender's
// key_value_database. Used to hold a handful of state entries (current
// artifact, update step) across reboots. Capacities are fixed at compile time
// so the whole store lives in .bss with no heap.
namespace etlx::storage {

extern const error::Category Category;

enum Code : error::Code {
    None     = 0,
    NotFound = 1,  // Get on a missing key
    Full     = 2,  // no room for another entry
    TooLarge = 3,  // key or value exceeds its capacity
    IoError  = 4,  // backend persistence failed
};

using Key   = etl::string<ETLX_KV_KEY_CAPACITY>;
using Value = etl::vector<uint8_t, ETLX_KV_VALUE_CAPACITY>;

// Abstract store. Get copies into the caller's buffer (no lifetime games);
// Commit flushes to the backing medium for durable backends.
struct KeyValueStore {
    // Copies the value for `key` into `out`, returning the byte count, or a
    // NotFound error. Fails with TooLarge if `out` is smaller than the value.
    virtual Result<size_t> Get(etl::string_view key, ByteSpan out) = 0;

    // True if the key is present.
    virtual bool Has(etl::string_view key) = 0;

    // Inserts or overwrites. Fails Full/TooLarge on capacity limits.
    virtual Status Set(etl::string_view key, ConstByteSpan value) = 0;

    // Removes the key if present; removing an absent key is not an error.
    virtual Status Remove(etl::string_view key) = 0;

    // Persists pending changes. A no-op for purely in-memory stores.
    virtual Status Commit() = 0;

    virtual ~KeyValueStore() = default;
};

// In-memory store backed by an etl::flat_map. Durable backends derive from this
// and override Commit()/load to add persistence.
class MemoryKeyValueStore : public KeyValueStore {
public:
    Result<size_t> Get(etl::string_view key, ByteSpan out) override;
    bool           Has(etl::string_view key) override;
    Status         Set(etl::string_view key, ConstByteSpan value) override;
    Status         Remove(etl::string_view key) override;
    Status         Commit() override { return Ok(); }

    size_t size() const { return map_.size(); }

    // Convenience helpers for string values (config, artifact names).
    Status               SetString(etl::string_view key, etl::string_view value);
    Result<etl::string_view> GetStringInto(etl::string_view key, ByteSpan scratch);

protected:
    using Map = etl::flat_map<Key, Value, ETLX_KV_MAX_ENTRIES>;
    Map map_;
};

} // namespace etlx::storage

#endif // ETLX_STORAGE_KV_HPP
