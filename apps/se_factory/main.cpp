// se_factory: factory provisioning CLI for the NXP SE05x secure element.
//
// Commands:
//   provision    Generate RSA key, create CSR, print CSR PEM
//   write-cert   Write a DER certificate file to the SE cert object
//   write-info   Write an arbitrary binary blob to the SE device-info object
//   read-info    Read and hex-dump the device-info object
//   rotate-scp03 Rotate Platform SCP03 keys (GP PUT KEY)
//   uid          Print the 18-byte chip UID
//   verify       Verify the binding between the SE key and a DER certificate
//
// All commands require EX_SSS_BOOT_SCP03_PATH to be set (Platform SCP03).
// The --port flag overrides the connection string (default: env / middleware).

#include <etlx/se/connection.hpp>
#include <etlx/se/object_store.hpp>
#include <etlx/se/rsa_key.hpp>
#include <etlx/se/scp03.hpp>
#include <etlx/log/log.hpp>
#include "host/host_io.hpp"

#include <cstdio>
#include <cstring>

using namespace etlx;
using namespace etlx::se;

static void Usage(const char *prog) {
    std::fprintf(stderr,
                 "Usage: %s [--port <p>] <command> [args]\n"
                 "Commands:\n"
                 "  provision [--bits 2048|4096] [--policy full|sign-only|sign-decrypt]\n"
                 "            [--subject <dn>]   Generate key + print CSR PEM\n"
                 "  write-cert <cert.der>         Write DER cert to SE (id=0x%08x)\n"
                 "  write-info <blob.bin>         Write binary blob (id=0x%08x)\n"
                 "  read-info                     Hex-dump device-info blob\n"
                 "  rotate-scp03 <newkeys.txt>   Rotate SCP03 keys from file\n"
                 "                [--dry-run]     Build APDU but do not send\n"
                 "  uid                           Print the 18-byte chip UID\n"
                 "  verify <cert.der>             Verify SE key matches cert\n",
                 prog,
                 static_cast<unsigned>(kRsaCertId),
                 static_cast<unsigned>(kDeviceInfoId));
}

// Read a binary file into a fixed-size buffer; returns bytes read or 0 on error.
static size_t ReadFile(const char *path, uint8_t *buf, size_t capacity) {
    FILE *fp = std::fopen(path, "rb");
    if (!fp) {
        ETLX_LOG_ERROR("se_factory: cannot open '%s'", path);
        return 0;
    }
    size_t n = std::fread(buf, 1, capacity, fp);
    std::fclose(fp);
    return n;
}

static int CmdProvision(Connection &conn, int argc, char **argv) {
    RsaBits bits = RsaBits::k2048;
    KeyPolicy policy = KeyPolicy::SignOnly;
    const char *subject_dn = "CN=SE05x-Device";

    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "--bits") == 0 && i + 1 < argc) {
            ++i;
            if (std::strcmp(argv[i], "4096") == 0)
                bits = RsaBits::k4096;
        } else if (std::strcmp(argv[i], "--policy") == 0 && i + 1 < argc) {
            ++i;
            if (std::strcmp(argv[i], "full") == 0) policy = KeyPolicy::Full;
            else if (std::strcmp(argv[i], "sign-decrypt") == 0) policy = KeyPolicy::SignDecrypt;
        } else if (std::strcmp(argv[i], "--subject") == 0 && i + 1 < argc) {
            subject_dn = argv[++i];
        }
    }

    auto key_res = RsaKey::Generate(conn, kRsaKeyId, bits, policy);
    if (!key_res) {
        ETLX_LOG_ERROR("provision: %s", key_res.error().message.c_str());
        return 1;
    }
    auto csr_res = key_res.value().MakeCsr(subject_dn);
    if (!csr_res) {
        ETLX_LOG_ERROR("provision: CSR: %s", csr_res.error().message.c_str());
        return 1;
    }
    std::fputs(csr_res.value().c_str(), stdout);
    return 0;
}

static int CmdWriteCert(Connection &conn, const char *path) {
    static uint8_t buf[ETLX_SE_CERT_CAPACITY];
    size_t n = ReadFile(path, buf, sizeof(buf));
    if (n == 0) return 1;
    ObjectStore store(conn);
    auto st = store.WriteCert(kRsaCertId, buf, n);
    if (!st) { ETLX_LOG_ERROR("write-cert: %s", st.error().message.c_str()); return 1; }
    ETLX_LOG_INFO("se_factory: cert written (%zu bytes)", n);
    return 0;
}

static int CmdWriteInfo(Connection &conn, const char *path) {
    static uint8_t buf[ETLX_SE_BLOB_CAPACITY];
    size_t n = ReadFile(path, buf, sizeof(buf));
    if (n == 0) return 1;
    ObjectStore store(conn);
    auto res = store.WriteBinary(kDeviceInfoId, buf, n, /*force=*/true);
    if (!res) { ETLX_LOG_ERROR("write-info: %s", res.error().message.c_str()); return 1; }
    ETLX_LOG_INFO("se_factory: info blob written (%zu bytes)", n);
    return 0;
}

static int CmdReadInfo(Connection &conn) {
    ObjectStore store(conn);
    auto res = store.ReadBinary(kDeviceInfoId);
    if (!res) { ETLX_LOG_ERROR("read-info: %s", res.error().message.c_str()); return 1; }
    const auto &blob = res.value();
    for (size_t i = 0; i < blob.size(); ++i) {
        if (i && i % 16 == 0) std::putchar('\n');
        std::printf("%02x ", blob[i]);
    }
    std::putchar('\n');
    return 0;
}

static int CmdRotateScp03(const char *port, const char *newkeys_path, bool dry_run) {
    auto admin_res = Scp03Admin::Open(port);
    if (!admin_res) {
        ETLX_LOG_ERROR("rotate-scp03: open: %s", admin_res.error().message.c_str());
        return 1;
    }
    auto new_keys_res = scp03_keyfile::Read(newkeys_path);
    if (!new_keys_res) {
        ETLX_LOG_ERROR("rotate-scp03: %s", new_keys_res.error().message.c_str());
        return 1;
    }
    auto st = admin_res.value().Rotate(new_keys_res.value(), dry_run,
                                        dry_run ? nullptr : newkeys_path);
    if (!st) { ETLX_LOG_ERROR("rotate-scp03: %s", st.error().message.c_str()); return 1; }
    return 0;
}

static int CmdUid(Connection &conn) {
    ObjectStore store(conn);
    auto res = store.GetUid();
    if (!res) { ETLX_LOG_ERROR("uid: %s", res.error().message.c_str()); return 1; }
    for (size_t i = 0; i < ETLX_SE_UID_LEN; ++i)
        std::printf("%02x", res.value()[i]);
    std::putchar('\n');
    return 0;
}

static int CmdVerify(Connection &conn, const char *cert_path) {
    static uint8_t buf[ETLX_SE_CERT_CAPACITY];
    size_t n = ReadFile(cert_path, buf, sizeof(buf));
    if (n == 0) return 1;
    ObjectStore store(conn);
    auto res = store.VerifyBinding(kRsaKeyId, buf, n);
    if (!res) { ETLX_LOG_ERROR("verify: %s", res.error().message.c_str()); return 1; }
    std::puts(res.value() ? "BINDING OK" : "BINDING FAIL");
    return res.value() ? 0 : 2;
}

int main(int argc, char **argv) {
    static ports::host::StderrLogSink log_sink;
    log::SetSink(&log_sink);
    log::SetLevel(log::Level::Info);

    const char *port = nullptr;
    int argi = 1;

    while (argi < argc && std::strncmp(argv[argi], "--", 2) == 0) {
        if (std::strcmp(argv[argi], "--port") == 0 && argi + 1 < argc)
            port = argv[++argi];
        ++argi;
    }

    if (argi >= argc) { Usage(argv[0]); return 1; }
    const char *cmd = argv[argi++];

    // rotate-scp03 needs its own ISD session (skip_select_applet).
    if (std::strcmp(cmd, "rotate-scp03") == 0) {
        if (argi >= argc) { Usage(argv[0]); return 1; }
        const char *keyfile = argv[argi++];
        bool dry = (argi < argc && std::strcmp(argv[argi], "--dry-run") == 0);
        return CmdRotateScp03(port, keyfile, dry);
    }

    // All other commands share one normal (applet-selected) session.
    auto conn_res = Connection::Open(port);
    if (!conn_res) {
        ETLX_LOG_ERROR("se_factory: %s", conn_res.error().message.c_str());
        return 1;
    }
    Connection &conn = conn_res.value();

    if (std::strcmp(cmd, "provision") == 0)
        return CmdProvision(conn, argc - argi, argv + argi);
    if (std::strcmp(cmd, "write-cert") == 0) {
        if (argi >= argc) { Usage(argv[0]); return 1; }
        return CmdWriteCert(conn, argv[argi]);
    }
    if (std::strcmp(cmd, "write-info") == 0) {
        if (argi >= argc) { Usage(argv[0]); return 1; }
        return CmdWriteInfo(conn, argv[argi]);
    }
    if (std::strcmp(cmd, "read-info") == 0) return CmdReadInfo(conn);
    if (std::strcmp(cmd, "uid") == 0) return CmdUid(conn);
    if (std::strcmp(cmd, "verify") == 0) {
        if (argi >= argc) { Usage(argv[0]); return 1; }
        return CmdVerify(conn, argv[argi]);
    }

    std::fprintf(stderr, "Unknown command: %s\n", cmd);
    Usage(argv[0]);
    return 1;
}
