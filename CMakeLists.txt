cmake_minimum_required(VERSION 3.20)
project(mimita_hot_reload LANGUAGES C CXX)

# exports to clang ;PP
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# \/ installing glad
add_library(glad STATIC
    src/glad.c
)

target_include_directories(glad PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(glad PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
# target_link_libraries(mimita PRIVATE glad)
# /\
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_library(mimita-game SHARED ${GAME_SOURCES})

target_compile_features(mimita-game PRIVATE cxx_std_17)

target_compile_definitions(mimita-game PRIVATE
    MIMITA_GAME_DLL
    GLM_ENABLE_EXPERIMENTAL
)

target_include_directories(mimita-game PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(mimita-game PROPERTIES
    PREFIX ""
    OUTPUT_NAME "mimita-game"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
)

target_link_libraries(mimita-game PRIVATE glad)
add_executable(mimita
    src/main.cpp
)
target_include_directories(mimita PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(mimita PRIVATE 
    mimita-game
)
