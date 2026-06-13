#pragma once

#include <etlx/se/rsa_key.hpp>
#include <mbedtls/pk.h>

namespace etlx::se {

// Wire a SE-resident RSA key into a mbedTLS pk context so it can serve as the
// client private key in a TlsSocket mTLS handshake.  All signing is performed
// inside the SE — the private key never leaves the chip.
//
// Preconditions: pk must be mbedtls_pk_init'd, key must outlive pk.
Status SetupOpaquePk(mbedtls_pk_context &pk, RsaKey &key);

} // namespace etlx::se
