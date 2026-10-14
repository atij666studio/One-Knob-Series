cmake_minimum_required(VERSION 3.22)

project(AtiNAMatiCBoost VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        execute_process(COMMAND /usr/sbin/sysctl -in hw.optional.arm64
                        OUTPUT_VARIABLE _is_arm64 OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_is_arm64 STREQUAL "1")
            set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
        else()
            set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "" FORCE)
        endif()
        message(STATUS "AtiNAMatiC-Boost: building for ${CMAKE_OSX_ARCHITECTURES}")
    endif()
endif()

# JUCE native formats (CLAP is handled separately via clap-juce-extensions)
if(APPLE)
    set(_FORMATS Standalone VST3 AU)
elseif(LINUX)
    set(_FORMATS VST3 LV2)
else()
    set(_FORMATS Standalone VST3)
endif()

# Reuse the JUCE checkout that already lives in the AtiNAMatiC project.
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../AtiNAMatiC/juce-vst3-bridge/JUCE"
    CACHE PATH "Path to JUCE source")

if(EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" JUCE EXCLUDE_FROM_ALL)
else()
    message(STATUS "JUCE not found at ${JUCE_PATH} -- fetching 8.0.4 from GitHub")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ============================================================================
# CLAP — clap-juce-extensions (open format, no approval needed)
# ============================================================================
include(FetchContent)
FetchContent_Declare(
    clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(clap_juce_extensions)

# ============================================================================
# Plugin — pure DSP boost, no NAM inference dependency.
# ============================================================================
juce_add_plugin(AtiNAMatiCBoost
    COMPANY_NAME            "Atij666Studio"
    BUNDLE_ID               "com.ati.atinamaticboost"
    PLUGIN_MANUFACTURER_CODE Ati0
    PLUGIN_CODE             NBs1
    FORMATS                 ${_FORMATS}
    PRODUCT_NAME            "AtiNAMatiC-Boost"
    ICON_BIG                "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Boost.png"
    ICON_SMALL              "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Boost.png"
    DESCRIPTION             "Life, punch and clarity into any NAM amp"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    VST3_CATEGORIES         "Fx" "Distortion" "Guitar"
    LV2URI                 "https://atij666studio.com/lv2/atinamatic-boost"
)

juce_generate_juce_header(AtiNAMatiCBoost)

juce_add_binary_data(AtiNAMatiCBoostData SOURCES
    Resources/Background.png
)

target_sources(AtiNAMatiCBoost PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/BoostDSP.h
    Source/BoostLookAndFeel.h
)

# Shared ONE KNOB series code (DSP helpers + UI kit)
target_include_directories(AtiNAMatiCBoost PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../Shared")

target_compile_definitions(AtiNAMatiCBoost PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Wire up CLAP target (creates AtiNAMatiCBoost_CLAP alongside VST3)
clap_juce_extensions_plugin(
    TARGET        AtiNAMatiCBoost
    CLAP_ID       "com.ati.atinamaticboost"
    CLAP_FEATURES audio-effect distortion guitar stereo
)

target_link_libraries(AtiNAMatiCBoost PRIVATE
    AtiNAMatiCBoostData
    clap_juce_extensions
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
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Standalone exe shortname (desktop shortcut label).
# VST3/CLAP keep the full PRODUCT_NAME so DAWs still identify them as AtiNAMatiC-Boost.
if(TARGET AtiNAMatiCBoost_Standalone)
    set_target_properties(AtiNAMatiCBoost_Standalone PROPERTIES
        OUTPUT_NAME       "A-Boost"
        JUCE_PRODUCT_NAME "A-Boost")
endif()

# Linux system libraries (VST3 / CLAP / LV2 plugin builds)
if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    target_link_libraries(AtiNAMatiCBoost PRIVATE
        ${FREETYPE_LIBRARIES}
        dl pthread)
    target_include_directories(AtiNAMatiCBoost PRIVATE ${FREETYPE_INCLUDE_DIRS})
endif()