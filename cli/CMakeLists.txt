if(APPLE)
    cmake_minimum_required(VERSION 3.5)
else()
    cmake_minimum_required(VERSION 3.16)
endif()

# --- Create the CLI executable ---
add_executable(magic_eye_cli stl_to_sirds.cpp ParseArgs.h)

# --- Add include directory for cleaner includes ---
target_include_directories(magic_eye_cli PRIVATE ${CMAKE_SOURCE_DIR}/lib)

# --- Link the headers-only library to the CLI executable ---
target_link_libraries(magic_eye_cli PRIVATE magic_eye_lib)
