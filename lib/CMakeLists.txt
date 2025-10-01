if(WIN32)
    cmake_minimum_required(VERSION 3.16)
elseif(UNIX)
    cmake_minimum_required(VERSION 3.16)
elseif(APPLE)
    cmake_minimum_required(VERSION 3.5)
endif()


include(FetchContent)


# --- Create an INTERFACE library for magic_eye (headers-only) ---
add_library(magic_eye_lib INTERFACE)

# --- Fetch stb libraries ---
FetchContent_Declare(
    obj
    GIT_REPOSITORY https://github.com/nothings/obj.git
    GIT_TAG master
)
FetchContent_MakeAvailable(obj)

# --- Add source files to the interface library ---
target_sources(magic_eye_lib INTERFACE
    Camera.h
    DepthMapGenerator.h
    EdgeSmoother.h
    Laplace.h
    logger.h
    objtostl.h
    Options.h
    SIRDSGenerator.h
    StereogramGenerator.h
    Stlsmoother.h
    TextureSampler.h
    vectorutils.h   
    stb_image_impl.h
)


# --- Add include directories ---
target_include_directories(magic_eye_lib INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${stb_SOURCE_DIR}
    ${obj_SOURCE_DIR}/include   # <-- expose rapidobj headers
 )

# --- Link dependencies ---
target_link_libraries(magic_eye_lib INTERFACE stl_lib glm::glm)
