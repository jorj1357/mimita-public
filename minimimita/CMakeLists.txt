cmake_minimum_required(VERSION 3.15)
project(mini-mimita-collision LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")

include_directories(
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/../include"
    "C:/important/glfw-3.4.bin.WIN64/include"
)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

set(SOURCES
    src/main.cpp
    src/physics.cpp
    src/render.cpp
    src/maps.cpp
    src/glb-loader.cpp
    ../src/glad.c
)

add_executable(mini-mimita-collision ${SOURCES})

target_link_directories(mini-mimita-collision PRIVATE
    "C:/important/glfw-3.4.bin.WIN64/lib-mingw-w64"
)

target_link_libraries(mini-mimita-collision
    glfw3
    opengl32
    gdi32
    user32
    dwmapi
)
