#pragma once

#include <etlx/se/connection.hpp>

#include <cstdint>

namespace etlx::se {

// One full set of 128-bit Platform SCP03 static keys (ENC/MAC/DEK).
struct Scp03KeySet {
    uint8_t enc[16];
    uint8_t mac[16];
    uint8_t dek[16];
};

// Key file: read/write the three 128-bit SCP03 keys from/to a text file.
// Format (one key per line, space-separated tag + 32 lowercase hex digits):
//   ENC 00112233...
//   MAC 00112233...
//   DEK 00112233...
namespace scp03_keyfile {

// Read all three keys from path.  Returns error if any key line is missing or
// malformed.
Result<Scp03KeySet> Read(const char *path);

// Read just the DEK (the rotation flow wraps new keys with it).
Status ReadDek(const char *path, uint8_t dek[16]);

// Atomically write keys to path: rename existing to <path>.bak, write a tmp
// file, then rename tmp → path so a crash never corrupts the key file.
Status Write(const char *path, const Scp03KeySet &keys);

} // namespace scp03_keyfile

// One-call Platform SCP03 key rotation entry point.  Owns a dedicated ISD
// session (applet not selected) and runs GP PUT KEY over it.
//
// The rotation is IRREVERSIBLE: after success the SE only accepts the new keys.
// The current session continues until closed but the NEXT session must use the
// new keys.  Always persist the new key set (via Scp03Admin::Rotate with a
// key_file path) before closing the current session.
class Scp03Admin {
public:
    // Open a connection targeting the ISD (skip_select_applet=1).
    static Result<Scp03Admin> Open(const char *port);

    // Install newKeys.  If key_file is non-null and dry_run is false, atomically
    // writes the new key set to the file on success.
    Status Rotate(const Scp03KeySet &new_keys, bool dry_run, const char *key_file = nullptr);

    Scp03Admin(Scp03Admin &&) = default;
    ~Scp03Admin() = default;
    Scp03Admin(const Scp03Admin &) = delete;
    Scp03Admin &operator=(const Scp03Admin &) = delete;

private:
    explicit Scp03Admin(Connection &&c) : conn_(std::move(c)) {}
    Connection conn_;
};

} // namespace etlx::se
