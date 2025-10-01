if(WIN32)
    cmake_minimum_required(VERSION 3.16)
elseif(UNIX)
    cmake_minimum_required(VERSION 3.16)
elseif(APPLE)
    cmake_minimum_required(VERSION 3.5)
endif()

# gui/CMakeLists.txt

# Find OpenGL
find_package(OpenGL REQUIRED)

# Create the executable

if (WIN32)
add_executable(magic_eye_gui WIN32
    main.cpp
    openfile.cpp
    openfile.h
    customwidgets.cpp
    customwidgets.h
    # Any other files you have in the gui directory
)

else()

add_executable(magic_eye_gui
    main.cpp
    openfile.cpp
    openfile.h
    customwidgets.cpp
    customwidgets.h
    # Any other files you have in the gui directory
)
endif()


# Add the backend source files from the ImGui backends
target_sources(magic_eye_gui PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    
)

# Include directories for ImGui and the backends
target_include_directories(magic_eye_gui PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/examples
)

# Link libraries: your magic eye library (from lib directory), stl_lib, imgui, glfw, OpenGL
target_link_libraries(magic_eye_gui
    magic_eye_lib   # This is the library from your lib/ directory, adjust if needed
    stl_lib
    imgui
    glfw
    OpenGL::GL
)

# On Windows, link against opengl32
if (WIN32)
    if(MSVC)
        set_target_properties(magic_eye_gui PROPERTIES
            LINK_FLAGS "/ENTRY:mainCRTStartup"
    )
    endif()
    target_link_libraries(magic_eye_gui opengl32 user32)
endif()
