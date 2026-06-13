# democli: the example application built on the etlx template.
#
# The command logic lives in a library so the unit tests can drive Main /
# ParseArguments directly; main.cpp is a thin entry point on top.
add_library(democli_lib
  cli.cpp
  actions/actions.cpp
)
target_include_directories(democli_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(democli_lib
  PUBLIC  etlx::etlx etlx::host
  PRIVATE etlx_embedded_flags
)

add_executable(democli main.cpp)
target_include_directories(democli PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(democli PRIVATE democli_lib etlx_embedded_flags)
