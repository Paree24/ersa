cmake_minimum_required(VERSION 3.22)
project(ERSA VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(ErsaAssets
  HEADER_NAME ErsaAssets.h
  NAMESPACE ErsaAssets
  SOURCES
    Assets/Lato-Regular.ttf
    Assets/Lato-Bold.ttf
    Assets/Lato-Black.ttf
)

juce_add_plugin(ERSA
  COMPANY_NAME "Ersa"
  BUNDLE_ID "com.ersa.ersa"
  PLUGIN_MANUFACTURER_CODE Ersa
  PLUGIN_CODE Ersa
  FORMATS VST3 Standalone
  PRODUCT_NAME "ERSA"
  DESCRIPTION "Dual-VCO vintage analog polysynth with extended FX"
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_SYNTH TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(ERSA)

target_sources(ERSA PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/PresetBrowser.cpp
  Source/Presets.cpp
  Source/DSPEngine.cpp
  Source/ErsaLookAndFeel.cpp
)

target_include_directories(ERSA PRIVATE Source)

target_compile_definitions(ERSA PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ERSA PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
  juce_audio_plugin_client
  ErsaAssets
)

# Ship factory presets inside the VST3 bundle (runtime resolves them
# bundle-relative, with repo/dev fallbacks when run uninstalled)
add_custom_command(TARGET ERSA POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "${CMAKE_CURRENT_BINARY_DIR}/ERSA_artefacts/$<CONFIG>/VST3/ERSA.vst3/Contents/Resources/Factory"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Factory"
    "${CMAKE_CURRENT_BINARY_DIR}/ERSA_artefacts/$<CONFIG>/VST3/ERSA.vst3/Contents/Resources/Factory"
)

# Headless smoke-test harness (links the real plugin sources)
juce_add_console_app(ErsaHarness PRODUCT_NAME "ErsaHarness")
target_sources(ErsaHarness PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/PresetBrowser.cpp
  Source/Presets.cpp
  Source/DSPEngine.cpp
  Source/ErsaLookAndFeel.cpp
  Test/Harness.cpp
)
target_include_directories(ErsaHarness PRIVATE Source)
target_compile_definitions(ErsaHarness PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(ErsaHarness PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
  ErsaAssets
)
