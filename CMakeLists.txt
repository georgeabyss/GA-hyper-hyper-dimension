cmake_minimum_required(VERSION 3.22)

# Universal build: one binary for Apple Silicon and Intel Macs, macOS 11 (Big Sur) or newer.
# (Set before project(), so it also applies to the JUCE helper builds.)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")   # the oldest macOS the current C++ library supports

project(HyperHyperDimension VERSION 0.7.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to your JUCE checkout. Override with: cmake -B build -DJUCE_DIR=/path/to/JUCE
set(JUCE_DIR "$ENV{HOME}/Downloads/JUCE" CACHE PATH "Path to the JUCE folder")

add_subdirectory(${JUCE_DIR} JUCE)

# NOTE: no post-build copy. /Library/Audio is system-wide and needs admin rights,
# so installing is done by ./install.sh (uses sudo) after the build.
juce_add_plugin(HyperDim
    COMPANY_NAME "GA"
    BUNDLE_ID com.ga.hyperhyperdimension
    PLUGIN_MANUFACTURER_CODE Gaau
    PLUGIN_CODE Hhd1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Hyper! Hyper! Dimension"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_COPY_DIR "/Library/Audio/Plug-Ins/VST3"
    AU_COPY_DIR "/Library/Audio/Plug-Ins/Components"
    AU_MAIN_TYPE kAudioUnitType_MusicEffect   # aumf: AU hosts only send MIDI (needed for Retrig) to music effects
    VST3_CATEGORIES Fx)

file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp)
target_sources(HyperDim PRIVATE ${SOURCES})

target_compile_definitions(HyperDim
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(HyperDim
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags)
