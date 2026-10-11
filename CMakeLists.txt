cmake_minimum_required(VERSION 3.22)

# Build a universal binary (Intel + Apple Silicon) on macOS. This must be set
# before project() — CMake locks in the target architecture(s) at that point,
# so setting it later has no effect on macOS.
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for macOS" FORCE)
endif()

project(G0G VERSION 1.1.1 LANGUAGES C CXX OBJC OBJCXX)

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
    VERSION "1.1.1"
    COMPANY_NAME "Gary A. Newsome"
    COMPANY_COPYRIGHT "Gary A. Newsome"
    BUNDLE_ID "com.garyanewsome.G0G"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE "Gary"
    PLUGIN_CODE "G0G "
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "G0G"
)

# Source files
target_sources(G0G PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Presets.h
    Source/G0GLookAndFeel.h
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

# Headless engine tests (not part of the default build):
#   cmake --build <dir> --target G0GTests && <dir>/G0GTests_artefacts/G0GTests
juce_add_console_app(G0GTests PRODUCT_NAME "G0GTests")
set_target_properties(G0GTests PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(G0GTests PRIVATE
    Tests/G0GTests.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)
target_include_directories(G0GTests PRIVATE Source)
target_compile_definitions(G0GTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_UNIT_TESTS=0
)
target_link_libraries(G0GTests PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
    juce::juce_recommended_config_flags
)
