add_executable(se_factory main.cpp)
target_link_libraries(se_factory PRIVATE etlx::se05x etlx::host etlx::transport)
target_include_directories(se_factory PRIVATE ${CMAKE_SOURCE_DIR}/ports)
