cmake_minimum_required(VERSION 3.20)

# Adopt modern behavior for target_sources relative paths
cmake_policy(SET CMP0076 NEW)
# Header-only interface library for core functionality
add_library(magic_eye_lib INTERFACE)

# Public headers
set(MAGIC_EYE_HEADERS
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
list(TRANSFORM MAGIC_EYE_HEADERS PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
target_sources(magic_eye_lib INTERFACE ${MAGIC_EYE_HEADERS})

# Consumers inherit these include directories
target_include_directories(magic_eye_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${obj_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

# Link interface deps so consumers inherit them
target_link_libraries(magic_eye_lib INTERFACE
    stl_lib
    glm::glm
)

# Ensure consumers build with at least C++20 (matches top-level)
target_compile_features(magic_eye_lib INTERFACE cxx_std_20)
