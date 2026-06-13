# SE05x host port (Phases 0-1: build spike + session/RNG).
#
# Builds a curated static library from NXP's Plug & Trust SSS sources, compiled
# against etlx's fetched mbedTLS 3.6.6 (the 3.x API surface, SSS_HAVE_MBEDTLS_2_X
# = 0). This reuses the upstream simw_lib.cmake source/include lists and its
# fsl_sss_ftr.h generation, then swaps the OpenSSL keystore for the mbedTLS one,
# exactly as the standalone SE051 project does.
#
# Like ports/asio this is a hosted-OS port: it links a C middleware that uses
# the heap, so it is NOT compiled with the no-exceptions/no-heap core flags.

set(SIMW_LIB_DIR ${plugandtrust_SOURCE_DIR})

# Match the hardware/middleware (see CLAUDE.md in the SE051 project).
set(PTMW_Applet     SE05X_C   CACHE STRING "" FORCE)
set(PTMW_SE05X_Ver  07_02     CACHE STRING "" FORCE)  # 07_xx = SE051/SE052
set(PTMW_HostCrypto MBEDTLS   CACHE STRING "" FORCE)
set(PTMW_SE05X_Auth PlatfSCP03 CACHE STRING "" FORCE) # Platform SCP03 channel

# Select the mbedTLS 3.x API in the SSS sources (our mbedTLS is 3.6.6).
set(SSS_HAVE_MBEDTLS_2_X 0)

# Pulls in SIMW_SE_SOURCES / SIMW_SE_AUTH_SOURCES / SIMW_INC_DIR, the required
# ADD_DEFINITIONS (SSS_USE_FTR_FILE, T1oI2C, ...), and generates fsl_sss_ftr.h.
include(${SIMW_LIB_DIR}/simw_lib.cmake)

# simw_lib.cmake generates fsl_sss_ftr.h with SSS_HAVE_MBEDTLS_3_X=0 because the
# NXP build system has no mbedTLS 3.x knowledge.  We are on 3.6.6, so patch the
# generated file to 1 immediately after generation.  The patch is idempotent
# (string(REPLACE) is a no-op when the old string is absent) and runs every
# configure, after simw_lib.cmake, so compilation always sees the correct value.
file(READ "${SIMW_LIB_DIR}/fsl_sss_ftr.h" _ftr)
string(REPLACE
  "#define SSS_HAVE_MBEDTLS_3_X 0"
  "#define SSS_HAVE_MBEDTLS_3_X 1"
  _ftr "${_ftr}")
file(WRITE "${SIMW_LIB_DIR}/fsl_sss_ftr.h" "${_ftr}")
unset(_ftr)

# Swap the OpenSSL host-crypto + keystore for the mbedTLS implementation (the
# mbedTLS keystore lives inside fsl_sss_mbedtls_apis.c).
list(REMOVE_ITEM SIMW_SE_SOURCES
  ${SIMW_LIB_DIR}/sss/src/openssl/fsl_sss_openssl_apis.c
  ${SIMW_LIB_DIR}/sss/src/keystore/keystore_openssl.c)
list(APPEND SIMW_SE_SOURCES
  ${SIMW_LIB_DIR}/sss/src/mbedtls/fsl_sss_mbedtls_apis.c)

add_library(etlx_simw STATIC ${SIMW_SE_SOURCES} ${SIMW_SE_AUTH_SOURCES})

target_include_directories(etlx_simw SYSTEM PUBLIC
  ${SIMW_LIB_DIR}
  ${SIMW_INC_DIR}
)

# mbedTLS targets come from the fetched mbedtls (ETLX_WITH_TLS). SSS host-crypto
# uses mbedcrypto/x509; tls is harmless to include.
target_link_libraries(etlx_simw PUBLIC mbedtls mbedx509 mbedcrypto)

# MBEDTLS_ALLOW_PRIVATE_ACCESS: inside the 3.x code paths, NXP accesses
# mbedtls_ecp_keypair fields as ->grp / ->d / ->Q using the old names.  In
# mbedTLS 3.x those fields are declared as MBEDTLS_PRIVATE(grp) which normally
# expands to private_grp.  This flag makes MBEDTLS_PRIVATE(x) expand to just x
# (the documented mbedTLS escape hatch for legacy code), so the struct fields
# keep their pre-3.x names when NXP TUs parse the mbedTLS headers.
target_compile_definitions(etlx_simw PRIVATE MBEDTLS_ALLOW_PRIVATE_ACCESS)

# NXP C sources are not warning-clean; keep them from failing our build and
# don't impose our strict flags on third-party code.
target_compile_options(etlx_simw PRIVATE -w)

# etlx_se05x: the C++ façade over the NXP middleware.
# Phase 0: se05x_probe.cpp (build-spike TU, verifies SSS headers link)
# Phase 1: se_connection.cpp (Connection RAII), se_random.cpp (SeRandom)
# Phase 2: rsa_key.cpp (RSA generate/open/sign/CSR), object_store.cpp (cert/blob)
# Phase 3: scp03.cpp (GP PUT KEY rotation), scp03_keyfile.cpp (key file I/O)
add_library(etlx_se05x STATIC
  se05x_probe.cpp
  se_connection.cpp
  se_random.cpp
  rsa_key.cpp
  object_store.cpp
  scp03.cpp
  scp03_keyfile.cpp
  se_pk.cpp
)
add_library(etlx::se05x ALIAS etlx_se05x)
target_include_directories(etlx_se05x PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(etlx_se05x PUBLIC etlx::etlx etlx::host etlx_simw)
