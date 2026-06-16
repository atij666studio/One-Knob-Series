cmake_minimum_required(VERSION 3.22)

project(AtiNAMatiCAir VERSION 0.1.0)

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
    message(STATUS "AtiNAMatiC-Air: building for ${CMAKE_OSX_ARCHITECTURES}")
endif()

# Reuse the JUCE checkout that already lives in the AtiNAMatiC project.
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../AtiNAMatiC/juce-vst3-bridge/JUCE"
    CACHE PATH "Path to JUCE source")

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_PATH}. Override -DJUCE_PATH=...")
endif()

add_subdirectory("${JUCE_PATH}" JUCE EXCLUDE_FROM_ALL)

# ============================================================================
# CLAP support — clap-juce-extensions wraps the JUCE plugin as a CLAP target.
# Requires internet on first cmake configure (FetchContent clones the repo).
# ============================================================================
include(FetchContent)
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY         https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG                main
    GIT_SHALLOW            FALSE
    GIT_SUBMODULES_RECURSE YES
)
FetchContent_MakeAvailable(clap-juce-extensions)

# ============================================================================
# AtiNAMatiC-Air — cab feel enhancer (placed AFTER cab/IR).
# Pure DSP, no NAM inference dependency.
# ============================================================================
if(APPLE)
    set(_FORMATS Standalone VST3 AU)
else()
    set(_FORMATS Standalone VST3)
endif()

juce_add_plugin(AtiNAMatiCAir
    COMPANY_NAME            "Atij666Studio"
    BUNDLE_ID               "com.ati.atinamatichair"
    PLUGIN_MANUFACTURER_CODE Ati0
    PLUGIN_CODE             NAir
    FORMATS                 ${_FORMATS}
    PRODUCT_NAME            "AtiNAMatiC-Air"
    DESCRIPTION             "Amp-in-the-room air, depth and speaker feel"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    VST3_CATEGORIES         "Fx" "EQ" "Guitar"
    ICON_BIG                "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Air.png"
    ICON_SMALL              "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Air.png"
)

juce_generate_juce_header(AtiNAMatiCAir)

juce_add_binary_data(AtiNAMatiCAirData SOURCES
    Resources/Background.png
)

target_sources(AtiNAMatiCAir PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/AirDSP.h
    Source/AirLookAndFeel.h
)

target_compile_definitions(AtiNAMatiCAir PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(AtiNAMatiCAir PRIVATE
    AtiNAMatiCAirData
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
clap_juce_extensions_plugin(TARGET AtiNAMatiCAir
    CLAP_ID          "com.ati.atinamatichair"
    CLAP_FEATURES    audio-effect equalizer guitar
    CLAP_MANUAL_URL  "https://atij666studio.example"
    CLAP_SUPPORT_URL "https://atij666studio.example"
)

# ── Standalone shortcut/exe name ────────────────────────────────────────────
# Per-series convention: AtiNAMatiC plugins ship the standalone as "A-<Name>.exe"
# (Nex Series uses "N-<Name>.exe"). This keeps desktop shortcuts visually
# distinct between sibling One-Knob plugins. VST3/CLAP names are unchanged.
if(TARGET AtiNAMatiCAir_Standalone)
    set_target_properties(AtiNAMatiCAir_Standalone PROPERTIES
        OUTPUT_NAME           "A-Air"
        JUCE_PRODUCT_NAME     "A-Air")
endif()

# ── Linux system libraries ────────────────────────────────────────────────────
# Install deps: sudo apt-get install -y libasound2-dev libfreetype-dev
#   libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev
#   libgl1-mesa-dev libwebkit2gtk-4.0-dev libgtk-3-dev
if(LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA      REQUIRED alsa)
    pkg_check_modules(FREETYPE  REQUIRED freetype2)
    target_link_libraries(AtiNAMatiCAir PRIVATE
        ${ALSA_LIBRARIES}
        ${FREETYPE_LIBRARIES}
        dl pthread)
endif()
