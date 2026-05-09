cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment version")

project(Chaosverb VERSION 0.0.8)

# JUCE: clone next to this folder, or pass -DJUCE_DIR=<path>
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Path to JUCE framework")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at: ${JUCE_DIR}\n"
        "Clone JUCE next to this folder, or pass -DJUCE_DIR=<path>:\n"
        "   git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git ${JUCE_DIR}"
    )
endif()

add_subdirectory(${JUCE_DIR} JUCE)

file(GLOB PLUGIN_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*")
foreach(PLUGIN_DIR ${PLUGIN_DIRS})
    if(IS_DIRECTORY ${PLUGIN_DIR} AND EXISTS "${PLUGIN_DIR}/CMakeLists.txt")
        add_subdirectory(${PLUGIN_DIR})
    endif()
endforeach()
