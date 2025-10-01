cmake_minimum_required(VERSION 3.20)

# Adopt modern behavior for target_sources relative paths
cmake_policy(SET CMP0076 NEW)

include(FetchContent)

# Header-only library
add_library(magic_eye_lib INTERFACE)

# Interface headers (use absolute paths to avoid policy warnings)
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

# If RapidObj is available as a target, use its include dirs; otherwise keep obj_SOURCE_DIR include
# (This line safely no-ops if the target doesn't exist.)
target_include_directories(magic_eye_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${stb_SOURCE_DIR}              # ok if empty; remove if you don't use stb via FetchContent
    ${obj_SOURCE_DIR}/include
    $<TARGET_PROPERTY:rapidobj::rapidobj,INTERFACE_INCLUDE_DIRECTORIES>
)

# Link interface deps so consumers inherit them automatically
# (Assumes stl_lib and glm::glm exist elsewhere in your project)
target_link_libraries(magic_eye_lib INTERFACE
    stl_lib
    glm::glm
    rapidobj::rapidobj
)
