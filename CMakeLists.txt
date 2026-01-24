cmake_minimum_required(VERSION 3.24)

# Change to your project name
project(ulichperc VERSION 1.0.0)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# We're going to use CPM as our package manager to bring in JUCE
# Check to see if we have CPM installed already.  Bring it in if we don't.
set(CPM_DOWNLOAD_VERSION 0.34.0)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if (NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif ()

include(${CPM_DOWNLOAD_LOCATION})

# Bring in JUCE locally
CPMAddPackage(
    NAME juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG origin/master
)

# Make sure you include any new source files here
set(SourceFiles
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/CustomLookAndFeel.h
        Source/SampleMetadata.h
        Source/SampleMetadata.cpp
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

# Change these to your own preferences
juce_add_plugin(${PROJECT_NAME}
        COMPANY_NAME shroomnoise
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
        PRODUCT_NAME "ulichperc realtime"
        VST3_CATEGORIES Instrument
)

# How we want our SourceFiles to appear in our IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SourceFiles})

set(RB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/rubberband)

# Make the SourceFiles buildable
target_sources(${PROJECT_NAME} PRIVATE 
    ${SourceFiles}
    ${RB_DIR}/single/RubberBandSingle.cpp
)


# Headers live in external/rubberband/rubberband/...
# so we add external/rubberband to include path.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${RB_DIR}
)

# These are some toggleable options from the JUCE CMake API
target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

file(GLOB SAMPLE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/samples/*.wav")
file(GLOB TRANSIENT_FILES  "${CMAKE_CURRENT_SOURCE_DIR}/transients/*.json")

# Pass them to BinaryData
juce_add_binary_data(${PROJECT_NAME}Data
    SOURCES
        ui/bg.png
        ui/knob.png
        ${SAMPLE_FILES}
        ${TRANSIENT_FILES}
)

# JUCE libraries to bring into our project
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
        ${PROJECT_NAME}Data
)
