cmake_minimum_required(VERSION 3.22)

project(AtiNAMatiCSquash VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)

if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    execute_process(COMMAND /usr/sbin/sysctl -in hw.optional.arm64
                    OUTPUT_VARIABLE _is_arm64 OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_is_arm64 STREQUAL "1")
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    else()
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "" FORCE)
    endif()
    message(STATUS "AtiNAMatiCSquash: building for ${CMAKE_OSX_ARCHITECTURES}")
endif()

set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../AtiNAMatiC/juce-vst3-bridge/JUCE"
    CACHE PATH "Path to JUCE source")

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_PATH}. Override -DJUCE_PATH=...")
endif()

add_subdirectory("${JUCE_PATH}" JUCE EXCLUDE_FROM_ALL)

# ---- CLAP support -------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(clap_juce_extensions)

# ---- Platform-specific plugin formats -----------------------------------
if(APPLE)
    set(_FORMATS Standalone VST3 AU)
else()
    set(_FORMATS Standalone VST3)
endif()

# ============================================================================
# Plugin
# ============================================================================
juce_add_plugin(AtiNAMatiCSquash
    COMPANY_NAME             "Atij666Studio"
    BUNDLE_ID                "com.ati.atinamaticsquash"
    PLUGIN_MANUFACTURER_CODE Ati0
    PLUGIN_CODE              NSq1
    FORMATS                  ${_FORMATS}
    PRODUCT_NAME             "AtiNAMatiC-Squash"
    DESCRIPTION              "One-knob bus and mix compressor"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    VST3_CATEGORIES          "Fx" "Dynamics"
    ICON_BIG                 "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Squash.png"
    ICON_SMALL               "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Squash.png"
)

juce_generate_juce_header(AtiNAMatiCSquash)

juce_add_binary_data(AtiNAMatiCSquashData SOURCES
    Resources/Background.png
)

target_sources(AtiNAMatiCSquash PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SquashDSP.h
    Source/SquashLookAndFeel.h
)

target_compile_definitions(AtiNAMatiCSquash PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(AtiNAMatiCSquash PRIVATE
    AtiNAMatiCSquashData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    clap_juce_extensions
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

clap_juce_extensions_plugin(
    TARGET        AtiNAMatiCSquash
    CLAP_ID       "com.ati.atinamaticsquash"
    CLAP_FEATURES audio-effect dynamics compressor
)

# Rename standalone exe to short readable label.
if(TARGET AtiNAMatiCSquash_Standalone)
    set_target_properties(AtiNAMatiCSquash_Standalone PROPERTIES
        OUTPUT_NAME       "A-Squash"
        JUCE_PRODUCT_NAME "A-Squash")
endif()
