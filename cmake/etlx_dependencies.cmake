# Third-party dependencies, pulled via FetchContent so the tree builds without
# manual submodule juggling. Pinned to specific tags for reproducibility.
include(FetchContent)

# ETL: the Embedded Template Library (heap-free containers/types).
FetchContent_Declare(
  etl
  GIT_REPOSITORY https://github.com/ETLCPP/etl.git
  GIT_TAG        20.47.1
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(etl)

# GoogleTest, used by the host unit tests only.
if(ETLX_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.16.0
    GIT_SHALLOW    TRUE
  )
  # Don't install gtest with our project; keep it a build-only dependency.
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()
