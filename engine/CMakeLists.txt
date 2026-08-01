cmake_minimum_required(VERSION 3.25)
project(Boson VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    add_compile_options(
        /W4          
        /WX          
        /Oi          
        /O2          
        /permissive- 
    )
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror      
        -O3          
        -mavx2       
        -mbmi2       
        -mpopcnt     
    )
endif()

# Global include directory for headers
include_directories(include)

# 1. GATHER ALL ENGINE SOURCE FILES
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# Core engine target configuration (Includes main.cpp)
add_executable(boson ${SOURCES})

# ---------------------------------------------------------------------------
# 2. TEST RUNNER TARGET (Excluding main.cpp)
# ---------------------------------------------------------------------------
# Copy the source list and filter out main.cpp to prevent LNK2005 multiply defined symbols
set(ENGINE_TEST_SOURCES ${SOURCES})
list(REMOVE_ITEM ENGINE_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

set(TEST_SOURCES
    ../tests/TestRunner.cpp
    ${ENGINE_TEST_SOURCES}
)

add_executable(boson_tests ${TEST_SOURCES})

# Ensure the test target can see the headers cleanly
target_include_directories(boson_tests PRIVATE include)