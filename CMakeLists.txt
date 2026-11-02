cmake_minimum_required(VERSION 3.22)
project(mask_scope VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.15 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(MASK_FORMATS VST3)
if(APPLE)
  set(MASK_FORMATS AU VST3)
endif()

juce_add_plugin(MaskScope
  COMPANY_NAME "mask"
  PRODUCT_NAME "mask-scope"
  PLUGIN_NAME "mask::scope"
  DESCRIPTION "Analyseur : spectrogramme, spectre, champ stéréo, sonie, historique, prédiction d'export"
  BUNDLE_ID com.mask.scope
  PLUGIN_MANUFACTURER_CODE Mask
  PLUGIN_CODE Mscp
  FORMATS ${MASK_FORMATS}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  AU_MAIN_TYPE kAudioUnitType_Effect
  VST3_CATEGORIES Fx Analyzer
  COPY_PLUGIN_AFTER_BUILD FALSE
  NEEDS_CURL FALSE
  NEEDS_WEB_BROWSER FALSE)

set(MASK_SOURCES
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/EditorSpectrogram.cpp
  Source/EditorSpectrum.cpp
  Source/EditorSoundField.cpp
  Source/EditorLoudness.cpp
  Source/EditorExport.cpp)
target_sources(MaskScope PRIVATE ${MASK_SOURCES})

set(MASK_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
if(UNIX AND NOT APPLE)
  # le build Linux sert aux tests ; les en-têtes X11 optionnels peuvent manquer
  list(APPEND MASK_DEFS JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()
target_compile_definitions(MaskScope PUBLIC ${MASK_DEFS})
target_link_libraries(MaskScope PRIVATE
  juce::juce_audio_processors juce::juce_gui_basics
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---- outils de test (ne font pas partie du plugin livré)
option(MASK_BUILD_TESTS "Construire les tests d'interface" ON)
if(MASK_BUILD_TESTS)
  juce_add_console_app(EditorTest PRODUCT_NAME "editor_test")
  target_sources(EditorTest PRIVATE Tests/editor_test.cpp ${MASK_SOURCES})
  target_compile_definitions(EditorTest PRIVATE ${MASK_DEFS} JUCE_STANDALONE_APPLICATION=1 MASK_TESTING=1)
  target_link_libraries(EditorTest PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)
  juce_add_console_app(LoadTest PRODUCT_NAME "load_test")
  target_sources(LoadTest PRIVATE Tests/load_test.cpp)
  target_compile_definitions(LoadTest PRIVATE ${MASK_DEFS} JUCE_STANDALONE_APPLICATION=1 JUCE_PLUGINHOST_VST3=1)
  target_link_libraries(LoadTest PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()
