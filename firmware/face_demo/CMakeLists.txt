cmake_minimum_required(VERSION 3.16)

set(CMAKE_BUILD_TYPE RELEASE)

# Target board for YAHAL
set(YAHAL_BOARD "rp2350-launchpad")

# Locate the YAHAL library that ships with the course material.
if (NOT YAHAL_DIR AND NOT DEFINED ENV{YAHAL_DIR})
    set(YAHAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../yahal_with_examples/YAHAL)
endif ()

include(${CMAKE_CURRENT_SOURCE_DIR}/../YAHAL_import.cmake)

project(face_demo C CXX ASM)

add_executable(face_demo
    ../src/face_demo.cpp
    ../src/face.cpp
)
target_include_directories(face_demo PRIVATE ../src)

yahal_add_driver(face_demo lcd)        # ST7735S display driver
yahal_add_me(face_demo)
yahal_add_library(face_demo uGUI)      # uGUI graphics library

yahal_add_custom_targets(face_demo)
yahal_add_extra_outputs(face_demo)
