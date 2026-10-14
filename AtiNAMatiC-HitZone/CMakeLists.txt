cmake_minimum_required(VERSION 3.22)

project(AtiNAMatiCHitZone VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)

# Build for the host hardware architecture by default, even if CMake itself
# is an x86_64 binary running under Rosetta (e.g. anaconda's cmake on Apple Silicon).
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    execute_process(COMMAND /usr/sbin/sysctl -in hw.optional.arm64
                    OUTPUT_VARIABLE _is_arm64 OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_is_arm64 STREQUAL "1")
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    else()
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "" FORCE)
    endif()
    message(STATUS "AtiNAMatiC-HitZone: building for ${CMAKE_OSX_ARCHITECTURES}")
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
# CLAP support -- clap-juce-extensions wraps the JUCE plugin as a CLAP target.
# Requires internet on first cmake configure (FetchContent clones the repo).
# ============================================================================
include(FetchContent)
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY         https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG                7adee3a1bd4684d4caa5601100e364abccff4b4d   # pinned (was: main)
    GIT_SHALLOW            FALSE
    GIT_SUBMODULES_RECURSE YES
)
FetchContent_MakeAvailable(clap-juce-extensions)

# ============================================================================
# AtiNAMatiC-HitZone -- input level conditioner for NAM models.
# ============================================================================
if(APPLE)
    set(_FORMATS Standalone VST3 AU)
elseif(LINUX)
    set(_FORMATS VST3 LV2)
else()
    set(_FORMATS Standalone VST3)
endif()

juce_add_plugin(AtiNAMatiCHitZone
    COMPANY_NAME            "Atij666Studio"
    BUNDLE_ID               "com.ati.atinamatichizone"
    PLUGIN_MANUFACTURER_CODE Ati0
    PLUGIN_CODE             NhZ1
    FORMATS                 ${_FORMATS}
    PRODUCT_NAME            "AtiNAMatiC-HitZone"
    DESCRIPTION             "Always hit the perfect level for NAM"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    VST3_CATEGORIES         "Fx" "Dynamics" "Guitar"
    ICON_BIG                "${CMAKE_CURRENT_SOURCE_DIR}/Resources/HitZone.png"
    ICON_SMALL              "${CMAKE_CURRENT_SOURCE_DIR}/Resources/HitZone.png"
    LV2URI                 "https://atij666studio.com/lv2/atinamatic-hitzone"
)

juce_generate_juce_header(AtiNAMatiCHitZone)

juce_add_binary_data(AtiNAMatiCHitZoneData SOURCES
    Resources/Background.png
    ../Shared/Fonts/BarlowCondensed-Medium.ttf
    ../Shared/Fonts/BarlowCondensed-SemiBold.ttf
    ../Shared/Fonts/IBMPlexMono-Medium.ttf
)

target_sources(AtiNAMatiCHitZone PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/HitZoneDSP.h
    ../Shared/OneKnobDSP.h
    ../Shared/OneKnobParams.h
    ../Shared/OneKnobUI.h
)

# Shared ONE KNOB series code (DSP helpers + UI kit)
target_include_directories(AtiNAMatiCHitZone PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../Shared")

target_compile_definitions(AtiNAMatiCHitZone PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(AtiNAMatiCHitZone PRIVATE
    AtiNAMatiCHitZoneData
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

clap_juce_extensions_plugin(
    TARGET        AtiNAMatiCHitZone
    CLAP_ID       "com.ati.atinamatichizone"
    CLAP_FEATURES audio-effect dynamics guitar
)

# Rename standalone exe to short readable label (avoids "AtiNAMati..." truncation
# on desktop shortcuts). Keep PRODUCT_NAME above so VST3/CLAP names are unchanged.
if(TARGET AtiNAMatiCHitZone_Standalone)
    set_target_properties(AtiNAMatiCHitZone_Standalone PROPERTIES
        OUTPUT_NAME       "A-HitZone"
        JUCE_PRODUCT_NAME "A-HitZone")
endif()

# Linux system libraries (VST3 / CLAP / LV2 plugin builds)
if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    target_link_libraries(AtiNAMatiCHitZone PRIVATE
        ${FREETYPE_LIBRARIES}
        dl pthread)
    target_include_directories(AtiNAMatiCHitZone PRIVATE ${FREETYPE_INCLUDE_DIRS})
endif()