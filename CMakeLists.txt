cmake_minimum_required(VERSION 3.20)
project(mimita_hot_reload LANGUAGES CXX)

add_library(mimita-game SHARED
    src/effects/effect-part.cpp
    src/gui/gui-coord.cpp
    src/replay/replay-factory.cpp
    src/video/video-settings.cpp
)

target_compile_features(mimita-game PRIVATE cxx_std_17)
target_compile_definitions(mimita-game PRIVATE
    MIMITA_GAME_DLL
    GLM_ENABLE_EXPERIMENTAL
)
target_include_directories(mimita-game PRIVATE
    include
    src
)
set_target_properties(mimita-game PROPERTIES
    PREFIX ""
    OUTPUT_NAME "mimita-game"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
)
