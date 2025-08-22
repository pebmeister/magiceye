cmake_minimum_required(VERSION 3.14)

project(magiceye_unittests LANGUAGES CXX)

message(STATUS "Setting C++ Standard 20")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# For Windows: Disable security warnings
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Create test executable
add_executable(
  unittest
  magiceyetests.cpp
)

# Include directories - same as CLI
target_include_directories(unittest 
  PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib"
)

# CRITICAL: Link to the same library as the CLI executable
# This will bring in all the necessary dependencies including GLM
target_link_libraries(
  unittest
  PRIVATE
    magic_eye_lib  # This is what provides the GLM includes
    GTest::gtest_main
)

# Discover and add tests
include(GoogleTest)
gtest_discover_tests(unittest)
