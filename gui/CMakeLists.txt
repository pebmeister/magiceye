# written by Paul Baxter
# gui/CMakeLists.txt
project(magic_eye_gui LANGUAGES CXX)

message (STATUS "gui/CMakeLists.txt")

# OpenGL
find_package(OpenGL REQUIRED)

# Sources (add .mm if you have ObjectiveC++ files for macOS)
file(GLOB GUI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.mm"
)

add_executable(magic_eye_gui ${GUI_SOURCES})

# ImGui and GLFW are provided by the top-level; just link them
target_link_libraries(magic_eye_gui PRIVATE
    magic_eye_lib
    imgui
    glfw
    OpenGL::GL
)

# macOS frameworks (GLFW usually adds them; keeping here for robustness)
if(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    if(NOT COCOA_FRAMEWORK OR NOT IOKIT_FRAMEWORK OR NOT COREVIDEO_FRAMEWORK)
        message(FATAL_ERROR "Required macOS frameworks not found (Cocoa, IOKit, CoreVideo).")
    endif()

    target_link_libraries(magic_eye_gui PRIVATE
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
    )
endif()

# Match the top-level standard
target_compile_features(magic_eye_gui PRIVATE cxx_std_20)
