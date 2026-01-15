cmake_minimum_required(VERSION 3.24)

project(ulichperc VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# =========================
# CPM + JUCE
# =========================
set(CPM_DOWNLOAD_VERSION 0.34.0)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if (NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
    )
endif()

include(${CPM_DOWNLOAD_LOCATION})

CPMAddPackage(
    NAME juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG origin/master
)

# =========================
# Sources
# =========================
set(SourceFiles
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/CustomLookAndFeel.h
    Source/TransientMetadata.h
    Source/TransientMetadata.cpp
    Source/MetaSamplerSound.h
    Source/MetaSamplerSound.cpp
    Source/MetaSamplerVoice.h
    Source/MetaSamplerVoice.cpp
)

if (WIN32)
    set(PLUGIN_FORMATS VST3)
elseif (APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME dsnoise
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    JUCE_VST3_CAN_REPLACE_VST2 FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Tap1
    PLUGIN_CODE Reg0
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "ulichperc"
    VST3_CATEGORIES Instrument
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SourceFiles})
target_sources(${PROJECT_NAME} PRIVATE ${SourceFiles})

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# =========================
# BinaryData
# =========================
file(GLOB SAMPLE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/samples/*.wav")
file(GLOB TRANSIENT_FILES  "${CMAKE_CURRENT_SOURCE_DIR}/transients/*.json")

juce_add_binary_data(${PROJECT_NAME}Data
    SOURCES
        ui/bg.png
        ui/knob.png
        ${SAMPLE_FILES}
        ${TRANSIENT_FILES}
)

# =========================
# Rubber Band via ExternalProject (Meson/Ninja)
# =========================
include(ExternalProject)

set(RB_SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/external/rubberband)
set(RB_BUILD_DIR   ${CMAKE_BINARY_DIR}/rb_build)
set(RB_INSTALL_DIR ${CMAKE_BINARY_DIR}/rb_install)

# Make include dir exist at configure time (prevents CMake errors)
file(MAKE_DIRECTORY ${RB_INSTALL_DIR}/include)

ExternalProject_Add(rubberband_ext
    SOURCE_DIR  ${RB_SRC_DIR}
    BINARY_DIR  ${RB_BUILD_DIR}
    INSTALL_DIR ${RB_INSTALL_DIR}

    CONFIGURE_COMMAND
        meson setup ${RB_BUILD_DIR} ${RB_SRC_DIR}
            --prefix=${RB_INSTALL_DIR}
            --libdir=lib
            --default-library=static

    BUILD_COMMAND
        ninja -C ${RB_BUILD_DIR}

    INSTALL_COMMAND
        ninja -C ${RB_BUILD_DIR} install

    # This is important for Xcode/VS multi-step builds
    STEP_TARGETS configure build install
)

# Imported library target we link against
add_library(rubberband STATIC IMPORTED GLOBAL)

# Ensure install step runs before anything tries to use headers/libs
add_dependencies(rubberband rubberband_ext-install)

# Also ensure your plugin waits for Rubber Band install (this fixes header-not-found)
add_dependencies(${PROJECT_NAME} rubberband_ext-install)

# Library file path (may vary by platform/version)
if (WIN32)
    set(RB_LIB "${RB_INSTALL_DIR}/lib/rubberband.lib")
else()
    set(RB_LIB "${RB_INSTALL_DIR}/lib/librubberband.a")
endif()

set_target_properties(rubberband PROPERTIES
    IMPORTED_LOCATION "${RB_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${RB_INSTALL_DIR}/include"
)

# As a belt-and-suspenders include path (avoids propagation/order issues)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${RB_INSTALL_DIR}/include
)

# =========================
# Link JUCE + Rubber Band + BinaryData
# =========================
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        juce::juce_analytics
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_data_structures
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        rubberband
        ${PROJECT_NAME}Data
)
