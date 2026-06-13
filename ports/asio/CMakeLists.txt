# Boost.Asio transport port. Compiled WITHOUT etlx_embedded_flags: it is a
# hosted-OS backend and Boost.Asio expects exceptions/RTTI, so this TU keeps
# them enabled and uses Asio's non-throwing error_code overloads internally.
add_library(etlx_asio
  asio_tcp.cpp
)

add_library(etlx::asio ALIAS etlx_asio)

# Headers are included as "asio/asio_tcp.hpp".
target_include_directories(etlx_asio PUBLIC ${CMAKE_SOURCE_DIR}/ports)

# Boost headers stay private (consumers include only asio_tcp.hpp), but the
# pthread dependency must reach the final executable, so Threads is PUBLIC.
target_link_libraries(etlx_asio
  PUBLIC  etlx::etlx Threads::Threads
  PRIVATE etlx_boost_asio
)
