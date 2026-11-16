cmake_minimum_required(VERSION 3.22)
project(MPC3000_SIMULATOR VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MPC3000_SIMULATOR
    PRODUCT_NAME "MPC3000 SIMULATOR"
    COMPANY_NAME "MPC3000Sim"
    PLUGIN_MANUFACTURER_CODE Mpcs
    PLUGIN_CODE M3ks
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Sampler)

juce_add_binary_data(MPCAssets SOURCES Resources/Fonts/VT323-Regular.ttf)

target_sources(MPC3000_SIMULATOR PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Sampler/SamplerEngine.cpp
    Source/Library/LibraryManager.cpp
    Source/Programs/XpmImporter.cpp)

target_compile_definitions(MPC3000_SIMULATOR PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MPC3000_SIMULATOR PRIVATE
    MPCAssets
    juce::juce_audio_utils juce::juce_audio_processors
    juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

option(MPC_BUILD_TESTS "Build headless engine test" OFF)
if(MPC_BUILD_TESTS)
    juce_add_console_app(EngineTest PRODUCT_NAME "EngineTest")
    target_sources(EngineTest PRIVATE Tests/EngineTest.cpp Source/Sampler/SamplerEngine.cpp Source/Programs/XpmImporter.cpp)
    target_compile_definitions(EngineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(EngineTest PRIVATE juce::juce_audio_formats juce::juce_audio_basics juce::juce_data_structures juce::juce_core juce::juce_recommended_config_flags)
endif()

if(MPC_BUILD_TESTS)
    juce_add_console_app(Snapshot PRODUCT_NAME "Snapshot")
    target_sources(Snapshot PRIVATE Tests/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp
        Source/Sampler/SamplerEngine.cpp Source/Library/LibraryManager.cpp Source/Programs/XpmImporter.cpp)
    target_compile_definitions(Snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(Snapshot PRIVATE MPCAssets juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()
