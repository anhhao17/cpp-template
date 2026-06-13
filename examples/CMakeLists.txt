# Per-module example programs. Each is a tiny main() that builds on host and
# under qemu-aarch64. Helper keeps the boilerplate to one line per example.
function(etlx_add_example name)
  add_executable(example_${name} ${name}/main.cpp)
  target_include_directories(example_${name} PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(example_${name}
    PRIVATE etlx::etlx etlx::host etlx_embedded_flags)
endfunction()

etlx_add_example(result_error)
etlx_add_example(cli_minimal)
etlx_add_example(log_uart)
etlx_add_example(io_copy)
etlx_add_example(kv_persist)
etlx_add_example(json_config)
etlx_add_example(sha256_stream)
etlx_add_example(http_get)
etlx_add_example(update_fsm)
etlx_add_example(event_loop)

# The Asio transport example.
if(ETLX_WITH_ASIO)
  add_executable(example_asio_http_get asio_http_get/main.cpp)
  target_include_directories(example_asio_http_get PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(example_asio_http_get
    PRIVATE etlx::asio etlx_embedded_flags)
endif()

# The mTLS example needs the TLS library and threads.
if(ETLX_WITH_TLS)
  add_executable(example_mtls_client mtls_client/main.cpp)
  target_include_directories(example_mtls_client PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(example_mtls_client
    PRIVATE etlx::tls etlx::host etlx_embedded_flags)
endif()
