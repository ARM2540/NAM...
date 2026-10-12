cmake_minimum_required(VERSION 3.22)
project(NAMAudioProcessor VERSION 0.2.0 LANGUAGES C CXX)

include(FetchContent)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(NeuralAmpModelerCore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG v0.6.0
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE)
FetchContent_GetProperties(NeuralAmpModelerCore)
if(NOT neuralampmodelercore_POPULATED)
    FetchContent_Populate(NeuralAmpModelerCore)
endif()

# Upstream NAM Core v0.6.0 is primarily arranged as sources for its CLI tools,
# not as a reusable CMake library target. Compile its NAM sources into this
# plugin's static dependency target.
file(GLOB NAM_SOURCES CONFIGURE_DEPENDS
    "${neuralampmodelercore_SOURCE_DIR}/NAM/*.cpp"
    "${neuralampmodelercore_SOURCE_DIR}/NAM/*.c"
    "${neuralampmodelercore_SOURCE_DIR}/NAM/*/*.cpp")

add_library(namcore STATIC
    ${NAM_SOURCES}
    "${neuralampmodelercore_SOURCE_DIR}/Dependencies/AudioDSPTools/dsp/wav.cpp")
target_compile_features(namcore PUBLIC cxx_std_20)
# Keep NAM_SAMPLE consistent between NAM Core and the plug-in translation units.
# JUCE AudioBuffer<float> supplies float samples to DSP::process().
target_compile_definitions(namcore PUBLIC NAM_SAMPLE_FLOAT=1)
target_include_directories(namcore PUBLIC
    "${neuralampmodelercore_SOURCE_DIR}"
    "${neuralampmodelercore_SOURCE_DIR}/NAM"
    "${neuralampmodelercore_SOURCE_DIR}/Dependencies/eigen"
    "${neuralampmodelercore_SOURCE_DIR}/Dependencies/nlohmann"
    "${neuralampmodelercore_SOURCE_DIR}/Dependencies/AudioDSPTools/dsp")
if(WIN32)
    target_compile_definitions(namcore PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

juce_add_plugin(NAMAudioProcessor
    COMPANY_NAME "NAM Audio Tools"
    BUNDLE_ID "audio.namtools.namaudioprocessor"
    PLUGIN_MANUFACTURER_CODE Namt
    PLUGIN_CODE Namp
    FORMATS VST3 Standalone
    PRODUCT_NAME "NAM Audio Processor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(NAMAudioProcessor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_features(NAMAudioProcessor PRIVATE cxx_std_20)
target_compile_definitions(NAMAudioProcessor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(NAMAudioProcessor PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    namcore
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

# Generate JuceHeader.h only after all JUCE modules have been linked so the
# generated header reflects the target's actual module set.
juce_generate_juce_header(NAMAudioProcessor)
