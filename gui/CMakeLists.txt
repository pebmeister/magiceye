if(APPLE)
    cmake_minimum_required(VERSION 3.5)
else()
    cmake_minimum_required(VERSION 3.16)
endif()


add_executable(magic_eye_gui
    main.cpp
    openfile.cpp
    customwidgets.cpp
)

# Link to our libraries. imgui already links to glfw and OpenGL::GL publicly,
# but linking explicitly to glfw is harmless and keeps intent clear.
target_link_libraries(magic_eye_gui PRIVATE
    imgui
    glfw
    OpenGL::GL
    stl_lib
)

# Properly link macOS frameworks (do NOT use quoted "-framework Cocoa")
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