cmake_minimum_required(VERSION 3.22)

cmake_policy(SET CMP0091 NEW)

project(KalyxBassRig VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /permissive- /EHsc /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

if(UNIX AND NOT APPLE)
    if(EXISTS "$ENV{HOME}/.local/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig")
        set(ENV{PKG_CONFIG_PATH} "$ENV{HOME}/.local/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    endif()

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED alsa)
    pkg_check_modules(FREETYPE2 REQUIRED freetype2)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    pkg_check_modules(X11 REQUIRED x11 xext xrandr xinerama xcursor)
endif()

# ==============================================================================
# Impulse Responses (Resources/IRs/*.wav) + Fallback Generator
# ==============================================================================
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Resources/IRs")
file(GLOB BASS_RIG_IRS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/IRs/*.wav")
if(NOT BASS_RIG_IRS)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_fallback_irs.py"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        file(GLOB BASS_RIG_IRS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/IRs/*.wav")
    endif()
endif()

# ==============================================================================
# JUCE 8.0.12 & clap-juce-extensions
# ==============================================================================
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE clap_juce_extensions)

if(BASS_RIG_IRS)
    juce_add_binary_data(KalyxBassRigBinaryData
        HEADER_NAME "BinaryData.h"
        NAMESPACE   BinaryData
        SOURCES     ${BASS_RIG_IRS}
    )
endif()

juce_add_plugin(KalyxBassRig
    COMPANY_NAME                "Kalyx Audio"
    BUNDLE_ID                   "com.kalyxaudio.KalyxBassRig"
    COMPANY_WEBSITE             "https://kalyx-audio.example.com"
    COMPANY_EMAIL               "dsp@kalyx-audio.example.com"
    PLUGIN_MANUFACTURER_CODE    Kalx
    PLUGIN_CODE                 Kbsr
    FORMATS                     VST3 LV2 Standalone
    PRODUCT_NAME                "Kalyx Bass Rig"
    LV2URI                      "https://kalyx-audio.example.com/plugins/kalyx-bass-rig"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

clap_juce_extensions_plugin(
    TARGET        KalyxBassRig
    CLAP_ID       "com.kalyxaudio.KalyxBassRig"
    CLAP_FEATURES audio-effect amp-simulator bass distortion equalizer compressor limiter
)

target_sources(KalyxBassRig
    PRIVATE
        Source/Core/PresetManager.cpp
        Source/DSP/TubeAmpModels.cpp
        Source/DSP/DIPathDSP.cpp
        Source/DSP/DualCabDSP.cpp
        Source/DSP/MasterAndTunerDSP.cpp
        Source/UI/KalyxLookAndFeel.cpp
        Source/UI/DataWidgets.cpp
        Source/UI/BassRigWidgets.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(KalyxBassRig
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_compile_definitions(KalyxBassRig
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_XRANDR=1
        JUCE_USE_XINERAMA=1
        JUCE_USE_XCURSOR=1
)

target_link_libraries(KalyxBassRig
    PRIVATE
        $<$<TARGET_EXISTS:KalyxBassRigBinaryData>:KalyxBassRigBinaryData>
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_data_structures
        juce::juce_events
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(UNIX AND NOT APPLE)
    target_include_directories(KalyxBassRig PUBLIC
        ${ALSA_INCLUDE_DIRS}
        ${FREETYPE2_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
    )
    target_link_directories(KalyxBassRig PUBLIC
        ${ALSA_LIBRARY_DIRS}
        ${FREETYPE2_LIBRARY_DIRS}
        ${FONTCONFIG_LIBRARY_DIRS}
        ${X11_LIBRARY_DIRS}
    )
    target_link_libraries(KalyxBassRig PUBLIC
        ${ALSA_LIBRARIES}
        ${FREETYPE2_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        ${X11_LIBRARIES}
    )
endif()
