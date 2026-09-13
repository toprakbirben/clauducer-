cmake_minimum_required(VERSION 3.22)
project(Clauducer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Clauducer
    COMPANY_NAME "Clauducer"
    PLUGIN_MANUFACTURER_CODE Clau
    PLUGIN_CODE Cldc
    FORMATS VST3 AU
    PRODUCT_NAME "Clauducer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(Clauducer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ResultsListComponent.cpp
    Source/BackendClient.cpp
    Source/CircularCaptureButton.cpp
    Source/RippleSphere.cpp
)

target_compile_definitions(Clauducer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Clauducer PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_opengl
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
