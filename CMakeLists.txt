cmake_minimum_required(VERSION 3.15)
project(mimita VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
elseif(UNIX)
    set(PLATFORM_LINUX TRUE)
endif()

# Find GLFW
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    # Fallback to pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLFW3 QUIET glfw3)
    endif()
endif()

if(NOT GLFW3_FOUND AND NOT glfw3_FOUND)
    message(FATAL_ERROR "GLFW3 not found. Please install GLFW3 development libraries.\n"
        "On Linux: sudo apt-get install libglfw3-dev (or equivalent)\n"
        "On macOS: brew install glfw\n"
        "On Windows: Install GLFW and set GLFW_ROOT environment variable")
endif()

# Set include directories
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/include
)

if(glfw3_FOUND)
    list(APPEND INCLUDE_DIRS ${glfw3_INCLUDE_DIRS})
elseif(GLFW3_FOUND)
    list(APPEND INCLUDE_DIRS ${GLFW3_INCLUDE_DIRS})
endif()

include_directories(${INCLUDE_DIRS})

# Find OpenGL
find_package(OpenGL REQUIRED)

# Source files
file(GLOB_RECURSE CPP_SOURCES "src/*.cpp")
file(GLOB_RECURSE C_SOURCES "src/*.c")

# Create executable
add_executable(${PROJECT_NAME} ${CPP_SOURCES} ${C_SOURCES})

# Link libraries
if(glfw3_FOUND)
    target_link_libraries(${PROJECT_NAME} ${glfw3_LIBRARIES})
elseif(GLFW3_FOUND)
    target_link_libraries(${PROJECT_NAME} ${GLFW3_LIBRARIES})
endif()

target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})

# Platform-specific linking
if(PLATFORM_MACOS)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    target_link_libraries(${PROJECT_NAME} ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
elseif(PLATFORM_WINDOWS)
    target_link_libraries(${PROJECT_NAME} opengl32 gdi32 user32 dwmapi)
elseif(PLATFORM_LINUX)
    find_package(X11 REQUIRED)
    target_link_libraries(${PROJECT_NAME} ${X11_LIBRARIES})
endif()

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Copy assets to build directory (optional, for development)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
        COMMENT "Copying assets to build directory"
    )
endif()

# Installation (optional)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(DIRECTORY assets
    DESTINATION share/${PROJECT_NAME}
)

