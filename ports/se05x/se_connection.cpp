#include <etlx/se/connection.hpp>
#include <etlx/log/log.hpp>

extern "C" {
#include <ex_sss_boot.h>
}

namespace etlx::se {

Result<Connection> Connection::Open(const char *port, bool select_applet) {
    Connection c;

    if (!select_applet)
        c.ctx_->se05x_open_ctx.skip_select_applet = 1;

    sss_status_t st = ex_sss_boot_open(c.ctx_.get(), port);
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
        ex_sss_session_close(c.ctx_.get());
        c.opened_ = false;
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
