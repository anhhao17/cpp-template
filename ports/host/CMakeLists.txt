# Host port: concrete io/log backends for the native host and qemu-aarch64.
# Note: host_tcp.cpp (PosixTcpSocket) is NOT built here. It is compiled into the
# etlx::transport target only when ETLX_TRANSPORT=posix, so the POSIX and Asio
# transports are never linked into the same binary.
add_library(etlx_host
  host_io.cpp
  host_kv.cpp
  host_random.cpp
  host_clock.cpp
)

add_library(etlx::host ALIAS etlx_host)

# Headers are included as "host/host_io.hpp".
target_include_directories(etlx_host
  PUBLIC ${CMAKE_SOURCE_DIR}/ports
)

target_link_libraries(etlx_host
  PUBLIC  etlx::etlx
  PRIVATE etlx_embedded_flags
)
