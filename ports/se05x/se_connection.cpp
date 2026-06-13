#include <etlx/se/connection.hpp>
#include <etlx/log/log.hpp>

extern "C" {
#include <ex_sss_boot.h>
}

namespace etlx::se {

Connection::~Connection() {
    if (opened_) {
        ex_sss_session_close(ctx_.get());
        ETLX_LOG_DEBUG("se: session closed");
    }
}

} // namespace etlx::se
