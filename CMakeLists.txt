cmake_minimum_required(VERSION 3.22)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/framework/cmake/GaDefaults.cmake)
    message(FATAL_ERROR "framework/ is empty. Get the GA framework with: git submodule update --init")
endif()

# Universal build (Apple Silicon + Intel), macOS 11 or newer: GA defaults, before project().
include(framework/cmake/GaDefaults.cmake)

project(HyperHyperDimension VERSION 0.9.0)

# JUCE and ga_add_plugin(). Point it at your JUCE checkout: cmake -B build -DJUCE_DIR=/path/to/JUCE
# No post-build copy: /Library/Audio needs admin rights, so ./install.sh (sudo) installs after the build.
include(framework/cmake/GaPlugin.cmake)

file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp)

ga_add_plugin(HyperDim
    PRODUCT_NAME "Hyper! Hyper! Dimension"
    PLUGIN_CODE Hhd1
    BUNDLE_ID com.ga.hyperhyperdimension
    NEEDS_MIDI_INPUT TRUE
    AU_MAIN_TYPE kAudioUnitType_MusicEffect   # aumf: AU hosts only send MIDI (needed for Retrig) to music effects
    SOURCES ${SOURCES})

# Screenshot tool: README images (docs/) and the reference screenshots (tests/reference/screenshots/).
# Build it in its own folder, so it never ends up in the plug-ins: see tools/make_screenshots.sh
ga_add_tool(HyperDim HyperDimScreenshots
    OPTION HHD_SCREENSHOTS DESCRIPTION "Build the tool that renders the README screenshots"
    SOURCES tools/Screenshots.cpp MAIN tools/ScreenshotsMain.cpp)

# Null test (refactor safety net): renders fixed test audio through the processor and compares it with
# tests/reference/. Built in its own folder too: see tools/null_test.sh
ga_add_tool(HyperDim HyperDimNullTest
    OPTION HHD_NULLTEST DESCRIPTION "Build the null test"
    SOURCES tools/NullTest.cpp MAIN tools/NullTestMain.cpp)
