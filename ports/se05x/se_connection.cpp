#include <etlx/se/connection.hpp>
#include <etlx/se/scp03.hpp>
#include <etlx/log/log.hpp>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

extern "C" {
#include <ex_sss_boot.h>
}

namespace etlx::se {

// SE051C part SSS_PFSCP_ENABLE_SE051C_0005A8FA.
static constexpr Scp03KeySet kDefaultKeys = {
    { 0xBF, 0xC2, 0xDB, 0xE1, 0x82, 0x8E, 0x03, 0x5D, 0x3E, 0x7F, 0xA3, 0x6B, 0x90, 0x2A, 0x05, 0xC6 },
    { 0xBE, 0xF8, 0x5B, 0xD7, 0xBA, 0x04, 0x97, 0xD6, 0x28, 0x78, 0x1C, 0xE4, 0x7B, 0x18, 0x8C, 0x96 },
    { 0xD8, 0x73, 0xF3, 0x16, 0xBE, 0x29, 0x7F, 0x2F, 0xC9, 0xC0, 0xE4, 0x5F, 0x54, 0x71, 0x06, 0x99 },
};

Result<Connection> Connection::Open(const char *port, bool select_applet) {
    char path[64];
    std::snprintf(path, sizeof(path), "/tmp/.etlx_scp03_%d", static_cast<int>(getpid()));
    if (!scp03_keyfile::Write(path, kDefaultKeys))
        return SeFail(kOpenFailed, "SCP03 key write failed");
    setenv("EX_SSS_BOOT_SCP03_PATH", path, 1);

    Connection c;
    if (!select_applet)
        c.ctx_->se05x_open_ctx.skip_select_applet = 1;

    sss_status_t st = ex_sss_boot_open(c.ctx_.get(), port);

    unsetenv("EX_SSS_BOOT_SCP03_PATH");
    char bak[72];
    std::snprintf(bak, sizeof(bak), "%s.bak", path);
    std::remove(path);
    std::remove(bak);

    if (st != kStatus_SSS_Success) {
        ETLX_LOG_ERROR("se: ex_sss_boot_open failed (port=%s, status=0x%04x)",
                       port ? port : "(default)", static_cast<unsigned>(st));
        return SeFail(kOpenFailed, "ex_sss_boot_open failed");
    }
    c.opened_ = true;

    st = ex_sss_key_store_and_object_init(c.ctx_.get());
    if (st != kStatus_SSS_Success) {
        ETLX_LOG_ERROR("se: key_store_and_object_init failed (status=0x%04x)",
                       static_cast<unsigned>(st));
        return SeFail(kStoreFailed, "ex_sss_key_store_and_object_init failed");
    }

    ETLX_LOG_INFO("se: session opened (port=%s)", port ? port : "(default)");
    return c;
}

Connection::~Connection() {
    if (opened_) {
        ex_sss_session_close(ctx_.get());
        ETLX_LOG_DEBUG("se: session closed");
    }
}

} // namespace etlx::se
