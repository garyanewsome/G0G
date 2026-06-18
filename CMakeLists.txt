cmake_minimum_required(VERSION 3.22)
project(G0G VERSION 1.0.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure FetchContent to fetch JUCE
include(FetchContent)

# We use the stable juce8 release tag
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)

# Tell JUCE not to build extras/examples to save compilation time
set(JUCE_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(JUCE_BUILD_EXTRAS OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(JUCE)

# Add the G0G audio plugin target
juce_add_plugin(G0G
    VERSION "1.0.0"
    COMPANY_NAME "Antigravity"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "G0G"
)

# Source files
target_sources(G0G PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/GlideUnit.h
    Source/SubEngine.h
    Source/ClickEngine.h
    Source/SaturationCore.h
)

# Compile definitions
target_compile_definitions(G0G PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Include directories
target_include_directories(G0G PRIVATE
    Source
)

# Link JUCE modules and DSP library
target_link_libraries(G0G PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
)
