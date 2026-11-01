cmake_minimum_required(VERSION 3.22)
project(Organika VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(ORG_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND ORG_FORMATS AU)
endif()

juce_add_plugin(Organika
  COMPANY_NAME "NICEGUY"
  BUNDLE_ID com.niceguy.organika
  PLUGIN_MANUFACTURER_CODE Ngy1
  PLUGIN_CODE Orgk
  FORMATS ${ORG_FORMATS}
  PRODUCT_NAME "Organika"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Modulation
  AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Organika PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Presets.cpp)

target_include_directories(Organika PRIVATE Source)

target_compile_definitions(Organika PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

# ---------------------------------------------------------------------------
# Fonts (SIL Open Font License) are fetched from the google/fonts repository at
# configure time. If the download fails the UI falls back to system fonts.
# ---------------------------------------------------------------------------
set(ORG_FONT_DIR "${CMAKE_BINARY_DIR}/fonts")
file(MAKE_DIRECTORY "${ORG_FONT_DIR}")
set(ORG_FONT_FILES "")

function(org_fetch_font name url)
  set(dst "${ORG_FONT_DIR}/${name}")
  if(NOT EXISTS "${dst}")
    file(DOWNLOAD "${url}" "${dst}" STATUS st TIMEOUT 120)
    list(GET st 0 code)
    if(NOT code EQUAL 0)
      file(REMOVE "${dst}")
    endif()
  endif()
  if(EXISTS "${dst}")
    file(SIZE "${dst}" sz)
    if(sz GREATER 10000)
      set(ORG_FONT_FILES ${ORG_FONT_FILES} "${dst}" PARENT_SCOPE)
      return()
    endif()
  endif()
  message(WARNING "Could not fetch font ${name} - falling back to system fonts")
endfunction()

org_fetch_font(display.ttf "https://raw.githubusercontent.com/google/fonts/main/ofl/alegreyasc/AlegreyaSC-Bold.ttf")
org_fetch_font(body.ttf    "https://raw.githubusercontent.com/google/fonts/main/ofl/alegreyasans/AlegreyaSans-Medium.ttf")
org_fetch_font(mono.ttf    "https://raw.githubusercontent.com/google/fonts/main/ofl/jetbrainsmono/JetBrainsMono%5Bwght%5D.ttf")

list(LENGTH ORG_FONT_FILES ORG_NUM_FONTS)
if(ORG_NUM_FONTS EQUAL 3)
  juce_add_binary_data(OrgFonts HEADER_NAME BinaryData.h NAMESPACE BinaryData SOURCES ${ORG_FONT_FILES})
  target_link_libraries(Organika PRIVATE OrgFonts)
  target_compile_definitions(Organika PRIVATE ORG_HAS_FONTS=1)
endif()

target_link_libraries(Organika
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
