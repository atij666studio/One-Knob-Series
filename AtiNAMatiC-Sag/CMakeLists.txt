cmake_minimum_required(VERSION 3.22)

project(AtiNAMatiCSag VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)

# Build for the host hardware architecture by default, even if CMake itself
# is an x86_64 binary running under Rosetta (e.g. anaconda's cmake on Apple
# Silicon).
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    execute_process(COMMAND /usr/sbin/sysctl -in hw.optional.arm64
                    OUTPUT_VARIABLE _is_arm64 OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_is_arm64 STREQUAL "1")
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    else()
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "" FORCE)
    endif()
    message(STATUS "AtiNAMatiC-Sag: building for ${CMAKE_OSX_ARCHITECTURES}")
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
# CLAP support — clap-juce-extensions wraps the JUCE plugin as a CLAP target.
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
# AtiNAMatiC-Sag — power-supply sag and bloom (placed BEFORE the amp).
# Pure DSP, no NAM inference dependency.
# ============================================================================
if(APPLE)
    set(_FORMATS VST3 AU LV2)
elseif(LINUX)
    set(_FORMATS VST3 LV2)
else()
    set(_FORMATS VST3 LV2)
endif()

juce_add_plugin(AtiNAMatiCSag
    COMPANY_NAME            "Atij666Studio"
    BUNDLE_ID               "com.ati.atinamaticsag"
    PLUGIN_MANUFACTURER_CODE Ati0
    PLUGIN_CODE             SAGG
    FORMATS                 ${_FORMATS}
    PRODUCT_NAME            "AtiNAMatiC-Sag"
    DESCRIPTION             "Power-supply sag, bloom and touch response"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    VST3_CATEGORIES         "Fx" "Dynamics" "Guitar"
    ICON_BIG                "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Sag.png"
    ICON_SMALL              "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Sag.png"
    LV2URI                 "https://atij666studio.com/lv2/atinamatic-sag"
)

juce_generate_juce_header(AtiNAMatiCSag)

juce_add_binary_data(AtiNAMatiCSagData SOURCES
    Resources/Background.png
    ../Shared/Fonts/BarlowCondensed-Medium.ttf
    ../Shared/Fonts/BarlowCondensed-SemiBold.ttf
    ../Shared/Fonts/IBMPlexMono-Medium.ttf
)

target_sources(AtiNAMatiCSag PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SagDSP.h
    ../Shared/OneKnobDSP.h
    ../Shared/OneKnobParams.h
    ../Shared/OneKnobUI.h
)

# Shared ONE KNOB series code (DSP helpers + UI kit)
target_include_directories(AtiNAMatiCSag PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../Shared")

target_compile_definitions(AtiNAMatiCSag PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(AtiNAMatiCSag PRIVATE
    AtiNAMatiCSagData
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

# ── CLAP ─────────────────────────────────────────────────────────────────────
clap_juce_extensions_plugin(TARGET AtiNAMatiCSag
    CLAP_ID          "com.ati.atinamaticsag"
    CLAP_FEATURES    audio-effect compressor distortion guitar
    CLAP_MANUAL_URL  "https://atij666studio.example"
    CLAP_SUPPORT_URL "https://atij666studio.example"
)

# ── Standalone shortcut/exe name ────────────────────────────────────────────
# Per-series convention: AtiNAMatiC plugins ship the standalone as "A-<Name>.exe"
# (Nex Series uses "N-<Name>.exe"). This keeps desktop shortcuts visually
# distinct between sibling One-Knob plugins. VST3/CLAP names are unchanged.
if(TARGET AtiNAMatiCSag_Standalone)
    set_target_properties(AtiNAMatiCSag_Standalone PROPERTIES
        OUTPUT_NAME           "A-Sag"
        JUCE_PRODUCT_NAME     "A-Sag")
endif()

# Linux system libraries (VST3 / CLAP / LV2 plugin builds)
if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    target_link_libraries(AtiNAMatiCSag PRIVATE
        ${FREETYPE_LIBRARIES}
        dl pthread)
    target_include_directories(AtiNAMatiCSag PRIVATE ${FREETYPE_INCLUDE_DIRS})
endif()
