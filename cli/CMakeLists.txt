# cli/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(magic_eye_cli LANGUAGES CXX)

# Collect sources
file(GLOB CLI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(magic_eye_cli ${CLI_SOURCES})

# Link to the header-only interface library; it propagates includes and deps
target_link_libraries(magic_eye_cli PRIVATE magic_eye_lib)

# Match the top-level standard
target_compile_features(magic_eye_cli PRIVATE cxx_std_20)
