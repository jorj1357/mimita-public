cmake_minimum_required(VERSION 3.16)
project(mimita_escrow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(escrow_core STATIC
    src/escrow.cpp
)
target_include_directories(escrow_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

add_executable(escrow
    src/main.cpp
)
target_link_libraries(escrow PRIVATE escrow_core)
if(MINGW)
    target_link_options(escrow PRIVATE -static)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(escrow_tests
        tests/escrow_tests.cpp
    )
    target_link_libraries(escrow_tests PRIVATE escrow_core)
    if(MINGW)
        target_link_options(escrow_tests PRIVATE -static)
    endif()
    add_test(NAME escrow_tests COMMAND escrow_tests)
endif()

install(TARGETS escrow RUNTIME DESTINATION bin)
