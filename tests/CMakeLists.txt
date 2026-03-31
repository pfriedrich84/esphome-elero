cmake_minimum_required(VERSION 3.14)
project(elero_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Include component headers (so #include "elero/elero_crypto.h" works)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../components)

add_subdirectory(unit)
