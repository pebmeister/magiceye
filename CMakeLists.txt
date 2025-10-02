if(APPLE)
    cmake_minimum_required(VERSION 3.5)
else()
    cmake_minimum_required(VERSION 3.16)
endif()

# Improve handling of framework properties on macOS
if(POLICY CMP0112)
  cmake_policy(SET CMP0112 NEW)
endif()

project(magic_eye)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Find OpenGL for all platforms
find_package(OpenGL REQUIRED)

# --- Fetch stb libraries ---
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# --- Fetch obj libraries ---
FetchContent_Declare(
    obj
    GIT_REPOSITORY https://github.com/guybrush77/rapidobj.git
    GIT_TAG master
)
FetchContent_MakeAvailable(obj)

# --- Fetch the pebmeister STL project ---
FetchContent_Declare(
    pebmeister_stl
    GIT_REPOSITORY https://github.com/pebmeister/stl.git
    GIT_TAG master
)
FetchContent_MakeAvailable(pebmeister_stl)

# Force static runtime for all MSVC targets (including dependencies)
if(MSVC)
    set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "Use MSVC runtime DLL")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime")
endif()

# --- Fetch GLM (header-only) ---
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# --- Fetch imgui libraries ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG master
)
FetchContent_MakeAvailable(imgui)

# --- Fetch GLFW ---
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# --- Create STL library here in root since it depends on FetchContent ---
add_library(stl_lib
    ${pebmeister_stl_SOURCE_DIR}/stl.cpp
    ${pebmeister_stl_SOURCE_DIR}/stl.h
)

target_include_directories(stl_lib PUBLIC
    ${pebmeister_stl_SOURCE_DIR}
    ${obj_SOURCE_DIR}/include
)

# --- Detect ImGui OpenGL loader from examples (GLAD2, GLAD1, or GL3W).
# If not found (current imgui master removed them), fall back to GLEW. ---
set(IMGUI_GL_LOADER_SRC "")
set(IMGUI_GL_LOADER_INCLUDE_DIR "")
set(IMGUI_GL_LOADER_DEFINE "")
set(IMGUI_GLEW_TARGET "")  # actual GLEW target to link if we fall back

if(EXISTS "${imgui_SOURCE_DIR}/examples/libs/glad/src/gl.c")
    # GLAD2 layout
    set(IMGUI_GL_LOADER_SRC "${imgui_SOURCE_DIR}/examples/libs/glad/src/gl.c")
    set(IMGUI_GL_LOADER_INCLUDE_DIR "${imgui_SOURCE_DIR}/examples/libs/glad/include")
    set(IMGUI_GL_LOADER_DEFINE "IMGUI_IMPL_OPENGL_LOADER_GLAD2")
elseif(EXISTS "${imgui_SOURCE_DIR}/examples/libs/glad/src/glad.c")
    # GLAD1 layout
    set(IMGUI_GL_LOADER_SRC "${imgui_SOURCE_DIR}/examples/libs/glad/src/glad.c")
    set(IMGUI_GL_LOADER_INCLUDE_DIR "${imgui_SOURCE_DIR}/examples/libs/glad/include")
    set(IMGUI_GL_LOADER_DEFINE "IMGUI_IMPL_OPENGL_LOADER_GLAD")
elseif(EXISTS "${imgui_SOURCE_DIR}/examples/libs/gl3w/GL/gl3w.c")
    # GL3W fallback
    set(IMGUI_GL_LOADER_SRC "${imgui_SOURCE_DIR}/examples/libs/gl3w/GL/gl3w.c")
    set(IMGUI_GL_LOADER_INCLUDE_DIR "${imgui_SOURCE_DIR}/examples/libs/gl3w")
    set(IMGUI_GL_LOADER_DEFINE "IMGUI_IMPL_OPENGL_LOADER_GL3W")
else()
    message(STATUS "ImGui example OpenGL loaders not found. Falling back to GLEW.")
    # Fetch and build GLEW from source
    FetchContent_Declare(
        glew
        GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
        GIT_TAG glew-cmake-2.2.0
    )
    FetchContent_MakeAvailable(glew)
    set(IMGUI_GL_LOADER_INCLUDE_DIR "${glew_SOURCE_DIR}/include")
    set(IMGUI_GL_LOADER_DEFINE "IMGUI_IMPL_OPENGL_LOADER_GLEW")

    # Determine the actual GLEW target name provided by glew-cmake
    if(TARGET glew_s)
        set(IMGUI_GLEW_TARGET glew_s)
    elseif(TARGET libglew_static)
        set(IMGUI_GLEW_TARGET libglew_static)
    elseif(TARGET glew)
        set(IMGUI_GLEW_TARGET glew) # shared
    endif()
endif()

# --- Create imgui library with GLFW + OpenGL3 backends and detected/fallback loader ---
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp

    # ImGui backends
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp

    # Detected OpenGL loader source if present (GLAD2/GLAD1/GL3W)
    ${IMGUI_GL_LOADER_SRC}
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${IMGUI_GL_LOADER_INCLUDE_DIR}
)

# Tell the OpenGL3 backend which loader we use
if(IMGUI_GL_LOADER_DEFINE)
    target_compile_definitions(imgui PUBLIC ${IMGUI_GL_LOADER_DEFINE})
endif()

# If using GLEW fallback, link to the correct target and define GLEW_STATIC
if(IMGUI_GL_LOADER_DEFINE STREQUAL "IMGUI_IMPL_OPENGL_LOADER_GLEW")
    if(IMGUI_GLEW_TARGET STREQUAL "")
        message(FATAL_ERROR "GLEW fallback selected but no known GLEW CMake target was found (expected glew_s, libglew_static, or glew).")
    endif()
    target_link_libraries(imgui PUBLIC ${IMGUI_GLEW_TARGET})
    target_compile_definitions(imgui PUBLIC GLEW_STATIC)
endif()

# Backends need GLFW and OpenGL; link them so consumers don’t have to
target_link_libraries(imgui PUBLIC
    glfw
    OpenGL::GL
)

# --- Add subdirectories ---
add_subdirectory(lib)
add_subdirectory(cli)
add_subdirectory(gui)
