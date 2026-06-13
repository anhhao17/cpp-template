#include <etlx/se/random.hpp>
#include <etlx/log/log.hpp>

extern "C" {
#include <fsl_sss_api.h>
}

namespace etlx::se {

Status SeRandom::Fill(ByteSpan out) {
    if (out.empty())
        return Ok();

    sss_rng_context_t rng{};
    sss_status_t st = sss_rng_context_init(&rng, conn_.session());
    if (st != kStatus_SSS_Success) {
        ETLX_LOG_ERROR("se: sss_rng_context_init failed (0x%04x)", static_cast<unsigned>(st));
        return SeFail(kRngFailed, "sss_rng_context_init failed");
    }

    st = sss_rng_get_random(&rng, out.data(), out.size());
    sss_rng_context_free(&rng);

    if (st != kStatus_SSS_Success) {
        ETLX_LOG_ERROR("se: sss_rng_get_random failed (0x%04x)", static_cast<unsigned>(st));
        return SeFail(kRngFailed, "sss_rng_get_random failed");
    }

    return Ok();
}

} // namespace etlx::se
