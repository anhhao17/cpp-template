# The etlx library: platform-independent implementations of the core + service
# modules. Built once and linked by apps, examples and tests.
add_library(etlx
  io/io.cpp
  log/log.cpp
  conf/cli.cpp
  storage/kv.cpp
  json/json.cpp
  crypto/crypto.cpp
  crypto/sha256.cpp
  crypto/hmac.cpp
  net/net.cpp
  http/http.cpp
  events/loop.cpp
  sm/update_fsm.cpp
)

add_library(etlx::etlx ALIAS etlx)

target_include_directories(etlx
  PUBLIC ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(etlx
  PUBLIC  etlx_flags etl::etl
  PRIVATE etlx_embedded_flags
)

# TLS/mTLS support is a separate target so the core library stays free of any
# crypto-library dependency. It is built only when ETLX_WITH_TLS is on, and
# links the fetched mbedTLS.
if(ETLX_WITH_TLS)
  add_library(etlx_tls net/tls.cpp)
  add_library(etlx::tls ALIAS etlx_tls)
  target_link_libraries(etlx_tls
    PUBLIC  etlx::etlx mbedtls
    PRIVATE etlx_embedded_flags
  )
endif()
