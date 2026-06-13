#ifndef ETLX_PORTS_HOST_KV_HPP
#define ETLX_PORTS_HOST_KV_HPP

#include "etlx/storage/kv.hpp"

#include <etl/string.h>

// Host file-backed key-value store. Loads its contents from a file on
// construction and rewrites that file on Commit(), so it stands in for flash /
// NVS when running tests or examples on a PC.
namespace etlx::ports::host {

class FileKeyValueStore final : public etlx::storage::MemoryKeyValueStore {
public:
    // Opens the store backed by `path`. A missing file is treated as empty.
    explicit FileKeyValueStore(etl::string_view path);

    // Writes all entries back to the file atomically (write temp + rename).
    etlx::Status Commit() override;

    // Reloads from disk, discarding uncommitted in-memory changes.
    etlx::Status Load();

private:
    etl::string<256> path_;
};

} // namespace etlx::ports::host

#endif // ETLX_PORTS_HOST_KV_HPP
