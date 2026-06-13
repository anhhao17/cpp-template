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
