if(APPLE)
    cmake_minimum_required(VERSION 3.5)
else()
    cmake_minimum_required(VERSION 3.16)
endif()
project(magic_eye_cli LANGUAGES CXX)

# Collect sources
file(GLOB CLI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

add_executable(magic_eye_cli ${CLI_SOURCES})

# Link to the header-only interface library; it propagates includes and deps
target_link_libraries(magic_eye_cli PRIVATE magic_eye_lib)

# Match the top-level standard
target_compile_features(magic_eye_cli PRIVATE cxx_std_20)


File: gui/CMakeLists.txt
--------------------------------
project(magic_eye_gui LANGUAGES CXX)

include(FetchContent)

# OpenGL
find_package(OpenGL REQUIRED)

# GLFW: use existing target if present; otherwise fetch it (version matches your top-level)
if(NOT TARGET glfw)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
  )
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glfw)
endif()

# Sources (add .mm if you have Objective‑C++ files for macOS)
file(GLOB GUI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.mm"
)

add_executable(magic_eye_gui ${GUI_SOURCES})

# Ensure ImGui headers are visible to this target and link the ImGui library
# This fixes the "cannot find imgui.h" error across all platforms.
target_include_directories(magic_eye_gui PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Link against core libs, GLFW, OpenGL, and ImGui
target_link_libraries(magic_eye_gui PRIVATE
    magic_eye_lib
    imgui
    glfw
    OpenGL::GL
)

# macOS frameworks (GLFW usually adds them; keeping here for robustness)
if(APPLE)
  target_link_options(magic_eye_gui PRIVATE
    "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

# Match the top-level standard
target_compile_features(magic_eye_gui PRIVATE cxx_std_20)
