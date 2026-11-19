cmake_minimum_required(VERSION 3.22)

# Politique CMake pour activer CMAKE_MSVC_RUNTIME_LIBRARY proprement
cmake_policy(SET CMP0091 NEW)

project(KalyxSSLConsole VERSION 1.0.0 LANGUAGES C CXX)

# ==============================================================================
# 1. Standard C++20 & Règles de Portabilité Globales (Windows / Linux)
# ==============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Obligatoire sous Linux pour lier des bibliothèques statiques dans un binaire partagé (.vst3 / .lv2 / .clap)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Windows : Linkage statique du runtime MSVC (/MT et /MTd) pour éviter toute erreur de DLL manquante
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /permissive- /EHsc /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Linux : Vérification des paquets systèmes standard via pkg-config
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
# 2. Récupération Automatique de JUCE 8 & clap-juce-extensions via FetchContent
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

# ==============================================================================
# 3. Déclaration de la Cible Plugin (VST3, LV2, Standalone + CLAP)
# ==============================================================================
juce_add_plugin(KalyxSSLConsole
    COMPANY_NAME                "Kalyx Audio"
    BUNDLE_ID                   "com.kalyxaudio.KalyxSSLConsole"
    COMPANY_WEBSITE             "https://kalyx-audio.example.com"
    COMPANY_EMAIL               "dsp@kalyx-audio.example.com"
    PLUGIN_MANUFACTURER_CODE    Kalx
    PLUGIN_CODE                 Kssl
    FORMATS                     VST3 LV2 Standalone
    PRODUCT_NAME                "Kalyx SSL Console"
    LV2URI                      "https://kalyx-audio.example.com/plugins/kalyx-ssl-console"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

clap_juce_extensions_plugin(
    TARGET        KalyxSSLConsole
    CLAP_ID       "com.kalyxaudio.KalyxSSLConsole"
    CLAP_FEATURES audio-effect equalizer compressor limiter filter distortion mixing mastering
)

target_sources(KalyxSSLConsole
    PRIVATE
        Source/DSP/SSLChannelStripDSP.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(KalyxSSLConsole
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

# ==============================================================================
# 4. Définitions de Préprocesseur Strictes (Zéro OpenGL, Zéro WebBrowser/Curl)
# ==============================================================================
target_compile_definitions(KalyxSSLConsole
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_XRANDR=1
        JUCE_USE_XINERAMA=1
        JUCE_USE_XCURSOR=1
)

# ==============================================================================
# 5. Linkage des Modules JUCE & Bibliothèques Systèmes
# ==============================================================================
target_link_libraries(KalyxSSLConsole
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_data_structures
        juce::juce_events
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(UNIX AND NOT APPLE)
    target_include_directories(KalyxSSLConsole PUBLIC
        ${ALSA_INCLUDE_DIRS}
        ${FREETYPE2_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
    )
    target_link_directories(KalyxSSLConsole PUBLIC
        ${ALSA_LIBRARY_DIRS}
        ${FREETYPE2_LIBRARY_DIRS}
        ${FONTCONFIG_LIBRARY_DIRS}
        ${X11_LIBRARY_DIRS}
    )
    target_link_libraries(KalyxSSLConsole PUBLIC
        ${ALSA_LIBRARIES}
        ${FREETYPE2_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        ${X11_LIBRARIES}
    )
endif()
