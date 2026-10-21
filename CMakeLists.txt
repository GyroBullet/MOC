cmake_minimum_required(VERSION 3.16)
project(regex_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization for release builds
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")

add_executable(regex_engine main.cpp)
target_include_directories(regex_engine PRIVATE ${CMAKE_SOURCE_DIR})
