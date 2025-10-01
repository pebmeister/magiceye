cmake_minimum_required(VERSION 3.20)
project(magic_eye_gui LANGUAGES CXX)

include(FetchContent)

# OpenGL
find_package(OpenGL REQUIRED)

# GLFW: use an existing target if present; otherwise fetch it
if(NOT TARGET glfw)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
  )
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(glfw)
endif()

# Sources (add .mm if you have any Objective‑C++ files for macOS)
file(GLOB GUI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.mm"
)

add_executable(magic_eye_gui ${GUI_SOURCES})

# Link against the interface library, GLFW, and OpenGL
target_link_libraries(magic_eye_gui PRIVATE
    magic_eye_lib
    glfw
    OpenGL::GL
)

# Be explicit with macOS frameworks if needed (GLFW usually adds them automatically)
if(APPLE)
  target_link_options(magic_eye_gui PRIVATE
    "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

target_compile_features(magic_eye_gui PRIVATE cxx_std_17)
