message(STATUS "Top Level.")

#----------------------------------------------------------------
# CMakeLists.txt (top-level)
# Auto-resolve latest stable dependency versions from GitHub.
# Provides:
#  - MAGIC_EYE_DEPS_LOCK (OFF): if ON, use pinned known-good versions.
#  - MAGIC_EYE_DEPS_REFRESH (OFF): if ON, re-query GitHub even if cached.
#  - <DEP>_TAG override variables for each dependency (e.g. IMGUI_TAG).
#
# Note: This uses GitHub API without authentication and may be rate-limited.
#       If rate-limited or offline, it falls back to tags or default branch.


cmake_minimum_required(VERSION 3.16)

# On macOS, force Apple Clang to ensure -framework flags are accepted
# Do this before project() so it takes effect during the first configure.
if(APPLE)
    # Force compilers into the cache so CC/CXX from the environment don't override them in CI.
    set(CMAKE_C_COMPILER "/usr/bin/clang" CACHE FILEPATH "C compiler" FORCE)
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE FILEPATH "C++ compiler" FORCE)
endif()

# Improve handling of framework properties on macOS
if(POLICY CMP0112)
  cmake_policy(SET CMP0112 NEW)
endif()

project(magic_eye)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Options to control dependency resolution behavior
option(MAGIC_EYE_DEPS_LOCK "Use pinned known-good versions for all dependencies" ON)
option(MAGIC_EYE_DEPS_REFRESH "Re-query GitHub for latest tags even if cached" ON)

# Find OpenGL for all platforms
find_package(OpenGL REQUIRED)

# Helper: download a URL to a string (JSON)
function(_me_download URL OUT_CONTENT OUT_OK)
    string(MD5 _h "${URL}")
    set(_tmp "${CMAKE_BINARY_DIR}/_me_dl_${_h}.json")
    set(_st)
    file(DOWNLOAD "${URL}" "${_tmp}" STATUS _st TLS_VERIFY ON)
    list(LENGTH _st _len)
    if(_len GREATER 0)
        list(GET _st 0 _code)
    else()
        set(_code 1)
    endif()
    if(_code EQUAL 0 AND EXISTS "${_tmp}")
        file(READ "${_tmp}" _data)
        set(${OUT_CONTENT} "${_data}" PARENT_SCOPE)
        set(${OUT_OK} TRUE PARENT_SCOPE)
    else()
        set(${OUT_CONTENT} "" PARENT_SCOPE)
        set(${OUT_OK} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Helper: parse first occurrence of a JSON "key":"value" pair (naive regex)
function(_me_json_extract_first INPUT KEY OUT_VALUE OUT_FOUND)
    set(_re "\"${KEY}\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"")
    string(REGEX MATCH "${_re}" _m "${INPUT}")
    if(_m)
        set(${OUT_VALUE} "${CMAKE_MATCH_1}" PARENT_SCOPE)
        set(${OUT_FOUND} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VALUE} "" PARENT_SCOPE)
        set(${OUT_FOUND} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Resolve latest stable ref for a GitHub repo (prefer releases, then tags, then default branch)
function(_me_github_latest_ref OWNER REPO OUT_REF)
    # Cache key
    string(TOUPPER "${OWNER}_${REPO}" _key)
    set(_cache_var "ME_CACHE_${_key}_LATEST_REF")

    if(DEFINED ${_cache_var} AND NOT MAGIC_EYE_DEPS_REFRESH)
        set(${OUT_REF} "${${_cache_var}}" PARENT_SCOPE)
        return()
    endif()

    set(_resolved "")

    # 1) Try releases/latest
    set(_url "https://api.github.com/repos/${OWNER}/${REPO}/releases/latest")
    _me_download("${_url}" _json _ok)
    if(_ok)
        _me_json_extract_first("${_json}" "tag_name" _tag _found)
        if(_found AND NOT "${_tag}" STREQUAL "")
            set(_resolved "${_tag}")
        endif()
    endif()

    # 2) Fallback to tags (first entry)
    if("${_resolved}" STREQUAL "")
        set(_url "https://api.github.com/repos/${OWNER}/${REPO}/tags?per_page=1")
        _me_download("${_url}" _json2 _ok2)
        if(_ok2)
            _me_json_extract_first("${_json2}" "name" _tag2 _found2)
            if(_found2 AND NOT "${_tag2}" STREQUAL "")
                set(_resolved "${_tag2}")
            endif()
        endif()
    endif()

    # 3) Fallback to default branch
    if("${_resolved}" STREQUAL "")
        set(_url "https://api.github.com/repos/${OWNER}/${REPO}")
        _me_download("${_url}" _json3 _ok3)
        if(_ok3)
            _me_json_extract_first("${_json3}" "default_branch" _branch _found3)
            if(_found3 AND NOT "${_branch}" STREQUAL "")
                set(_resolved "${_branch}")
            endif()
        endif()
    endif()

    if("${_resolved}" STREQUAL "")
        # Last-ditch fallback
        set(_resolved "master")
    endif()

    set(${OUT_REF} "${_resolved}" PARENT_SCOPE)
    set(${_cache_var} "${_resolved}" CACHE STRING "Cached latest ref for ${OWNER}/${REPO}" FORCE)
endfunction()

# Resolve a dependency tag:
# - If <NAME>_TAG is set by user, use it.
# - Else if MAGIC_EYE_DEPS_LOCK is ON and PINNED_<NAME>_TAG exists, use it.
# - Else query GitHub for latest stable ref.
function(_me_resolve_dep_tag NAME OWNER REPO DEFAULT_FALLBACK OUT_VAR)
    string(TOUPPER "${NAME}" _NAMEU)
    set(_user_var "${_NAMEU}_TAG")
    set(_pin_var  "PINNED_${_NAMEU}_TAG")

    if(DEFINED ${_user_var} AND NOT "${${_user_var}}" STREQUAL "")
        set(${OUT_VAR} "${${_user_var}}" PARENT_SCOPE)
        return()
    endif()

    if(MAGIC_EYE_DEPS_LOCK AND DEFINED ${_pin_var} AND NOT "${${_pin_var}}" STREQUAL "")
        set(${OUT_VAR} "${${_pin_var}}" PARENT_SCOPE)
        return()
    endif()

    _me_github_latest_ref("${OWNER}" "${REPO}" _ref)
    if(NOT "${_ref}" STREQUAL "")
        set(${OUT_VAR} "${_ref}" PARENT_SCOPE)
    else()
        set(${OUT_VAR} "${DEFAULT_FALLBACK}" PARENT_SCOPE)
    endif()
endfunction()

# Known-good pins (used only if MAGIC_EYE_DEPS_LOCK=ON)
set(PINNED_IMGUI_TAG             "v1.91.8")     # example known-good
set(PINNED_GLFW_TAG              "3.4")         # example known-good
set(PINNED_GLM_TAG               "1.0.1")       # example known-good
set(PINNED_STB_TAG               "master")      # stb doesn't do releases regularly
set(PINNED_RAPIDOBJ_TAG          "v0.9.1")      # example known-good
set(PINNED_PEBMEISTER_STL_TAG    "master")      # no releases; default branch
set(PINNED_GLEW_CMAKE_TAG        "glew-cmake-2.2.0")

# Force static runtime for all MSVC targets (including dependencies)
if(MSVC)
    set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "Use MSVC runtime DLL")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime")
endif()

# --- Resolve tags dynamically ---
_me_resolve_dep_tag("stb"             "nothings"  "stb"          "master"           STB_TAG_RESOLVED)
_me_resolve_dep_tag("obj"             "guybrush77" "rapidobj"     "master"           RAPIDOBJ_TAG_RESOLVED)
_me_resolve_dep_tag("pebmeister_stl"  "pebmeister" "stl"          "master"           PEBMEISTER_STL_TAG_RESOLVED)
_me_resolve_dep_tag("glm"             "g-truc"    "glm"          "master"           GLM_TAG_RESOLVED)
_me_resolve_dep_tag("imgui"           "ocornut"   "imgui"        "master"           IMGUI_TAG_RESOLVED)
_me_resolve_dep_tag("glfw"            "glfw"      "glfw"         "master"           GLFW_TAG_RESOLVED)
# GLEW is only fetched if we need it; pre-resolve anyway for consistency
_me_resolve_dep_tag("glew_cmake"      "Perlmint"  "glew-cmake"   "glew-cmake-2.2.0" GLEW_CMAKE_TAG_RESOLVED)

message(STATUS "Dependency tags resolved:")
message(STATUS "  stb:             ${STB_TAG_RESOLVED}")
message(STATUS "  rapidobj:        ${RAPIDOBJ_TAG_RESOLVED}")
message(STATUS "  pebmeister_stl:  ${PEBMEISTER_STL_TAG_RESOLVED}")
message(STATUS "  glm:             ${GLM_TAG_RESOLVED}")
message(STATUS "  imgui:           ${IMGUI_TAG_RESOLVED}")
message(STATUS "  glfw:            ${GLFW_TAG_RESOLVED}")

# --- Fetch stb libraries ---
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        ${STB_TAG_RESOLVED}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

# --- Fetch obj (rapidobj) libraries ---
FetchContent_Declare(
    obj
    GIT_REPOSITORY https://github.com/guybrush77/rapidobj.git
    GIT_TAG        ${RAPIDOBJ_TAG_RESOLVED}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(obj)

# --- Fetch the pebmeister STL project ---
FetchContent_Declare(
    pebmeister_stl
    GIT_REPOSITORY https://github.com/pebmeister/stl.git
    GIT_TAG        ${PEBMEISTER_STL_TAG_RESOLVED}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pebmeister_stl)

# --- Fetch GLM (header-only) ---
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        ${GLM_TAG_RESOLVED}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# --- Fetch imgui libraries ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        ${IMGUI_TAG_RESOLVED}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# --- Fetch GLFW ---
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        ${GLFW_TAG_RESOLVED}
    GIT_SHALLOW    TRUE
)
# Common GLFW options to speed up configure
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
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
# If not found (current imgui releases may omit them), fall back to GLEW. ---
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
        GIT_TAG        ${GLEW_CMAKE_TAG_RESOLVED}
        GIT_SHALLOW    TRUE
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
