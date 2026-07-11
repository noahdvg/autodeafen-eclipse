cmake_minimum_required(VERSION 3.29)
project(autodeafen-eclipse VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK environment variable is not set")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

add_library(${PROJECT_NAME} SHARED src/main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE include)
setup_geode_mod(${PROJECT_NAME})
