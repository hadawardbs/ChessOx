cmake_minimum_required(VERSION 3.20)

project(OxtaCore VERSION 1.1 LANGUAGES CXX)

# C++23 Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force Binary Output to Build Root
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Compiler Options
if(MSVC)
    add_compile_options(/O2 /W4)
else()
    add_compile_options(-O3 -march=native -Wall -Wextra -pedantic)
endif()

# Architecture Information
message(STATUS "System Processor: ${CMAKE_SYSTEM_PROCESSOR}")

# Sources
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Executable
add_executable(OxtaCore ${SOURCES})

# Include Directories
target_include_directories(OxtaCore PUBLIC src)

# Testing
enable_testing()
add_library(nnue_lib src/nnue/nnue.cpp)
