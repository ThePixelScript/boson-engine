cmake_minimum_required(VERSION 3.25)
project(Boson VERSION 0.1.0 LANGUAGES CXX)

# Enforce strict compliance with C++23 specifications
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Binary distribution layout
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Zero-tolerance compiler warning and optimization pipeline
if(MSVC)
    add_compile_options(
        /W4          # Maximum warning level
        /WX          # Treat warnings as errors
        /Oi          # Enable intrinsic functions
        /O2          # Maximum speed optimization
        /permissive- # Enforce strict standard conformance
    )
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror      # Treat warnings as errors
        -O3          # Aggressive optimization
        -mavx2       # Target AVX2 instruction set vector lines
        -mbmi2       # Enable PEXT/PDEP bit manipulation instructions
        -mpopcnt     # Hardware-accelerated bit counting
    )
endif()

include_directories(include)

# Gather source implementation files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Core engine target configuration
add_executable(boson src/main.cpp ${SOURCES})