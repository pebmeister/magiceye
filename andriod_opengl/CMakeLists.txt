cmake_minimum_required(VERSION 3.22)

project(MagicEye LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build all objects as PIC (needed when static libs are linked into a shared lib on Android)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)


# --- Fetch obj (rapidobj) libraries ---
FetchContent_Declare(
    obj
    GIT_REPOSITORY https://github.com/guybrush77/rapidobj.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(obj)

# --- Fetch the pebmeister STL project ---
FetchContent_Declare(
    pebmeister_stl
    GIT_REPOSITORY https://github.com/pebmeister/stl.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pebmeister_stl)

# --- Fetch stb libraries ---
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

# Fetch Dear ImGui (no CMake targets provided by the repo)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.8
  GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()
set(IMGUI_DIR ${imgui_SOURCE_DIR})

# ImGui sources (include demo for convenience; remove if not needed)
set(IMGUI_SOURCES
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_android.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
)

# Build Dear ImGui as a static library
add_library(dear_imgui STATIC ${IMGUI_SOURCES})
target_include_directories(dear_imgui PUBLIC
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)
target_compile_definitions(dear_imgui PUBLIC
  IMGUI_IMPL_OPENGL_ES3
)

# Native app glue (for NativeActivity)
# Try ANDROID_NDK first; AGP also sets CMAKE_ANDROID_NDK
set(_NDK_DIR ${ANDROID_NDK})
if(NOT EXISTS "${_NDK_DIR}/sources/android/native_app_glue/android_native_app_glue.c")
  set(_NDK_DIR ${CMAKE_ANDROID_NDK})
endif()

add_library(native_app_glue STATIC
  ${_NDK_DIR}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(native_app_glue PUBLIC
  ${_NDK_DIR}/sources/android/native_app_glue
)

# --- Create STL library here in root since it depends on FetchContent ---
add_library(stl_lib
    ${pebmeister_stl_SOURCE_DIR}/stl.cpp
    ${pebmeister_stl_SOURCE_DIR}/stl.h
)

target_include_directories(stl_lib PUBLIC
    ${pebmeister_stl_SOURCE_DIR}
    ${obj_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

# Your shared library
set(MAGIC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib)
set(GUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../gui)

add_library(${CMAKE_PROJECT_NAME} SHARED
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${GUI_DIR}/openfile.cpp
  ${GUI_DIR}/customwidgets.cpp
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GUI_DIR}
    ${MAGIC_DIR}
    ${stb_SOURCE_DIR}
)

# Ensure ANativeActivity_onCreate is exported from your .so
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-u" "ANativeActivity_onCreate")

# Link everything
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  dear_imgui
  native_app_glue
  android
  EGL
  GLESv3
  log
  stl_lib
)
