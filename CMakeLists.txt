cmake_minimum_required(VERSION 3.22)

# ---- Platform settings that must come before project() ---------------------
# Static MSVC runtime: users do not need to install the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(EchoThrow VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ECHOTHROW_BUILD_TESTS "Build the DSP / UI test executables" OFF)
set(JUCE_SOURCE_DIR "" CACHE PATH "Use a local JUCE checkout instead of downloading one")

# ---- JUCE ------------------------------------------------------------------
if(JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.8
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(juce)
endif()

# ---- Core library: everything except the plugin entry point ----------------
add_library(EchoThrowCore STATIC
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/DSP/DelayEngine.cpp
    Source/DSP/CaptureEngine.cpp
    Source/DSP/RenderManager.cpp
    Source/PluginProcessor.cpp
    Source/UI/Theme.cpp
    Source/UI/Widgets.cpp
    Source/UI/WaveformView.cpp
    Source/UI/EqView.cpp
    Source/PluginEditor.cpp)

target_include_directories(EchoThrowCore PUBLIC Source)

target_compile_definitions(EchoThrowCore PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

if(UNIX AND NOT APPLE)
    # Only needed so the headless Linux test build works without every X11 extension installed.
    target_compile_definitions(EchoThrowCore PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0)
endif()

target_link_libraries(EchoThrowCore PUBLIC
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_data_structures
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_dsp
    PRIVATE
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ---- The plugin ------------------------------------------------------------
juce_add_plugin(EchoThrow
    COMPANY_NAME                "EchoThrow Audio"
    BUNDLE_ID                   com.echothrow.echothrow
    PLUGIN_MANUFACTURER_CODE    Ecth
    PLUGIN_CODE                 Etdl
    FORMATS                     VST3
    PRODUCT_NAME                "EchoThrow"
    VST3_CATEGORIES             Fx Delay
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(EchoThrow PRIVATE Source/PluginEntry.cpp)
target_link_libraries(EchoThrow PRIVATE EchoThrowCore)

# ---- Tests (optional) ------------------------------------------------------
if(ECHOTHROW_BUILD_TESTS)
    # lets the tests pump the message loop (never defined for the real plugin)
    target_compile_definitions(EchoThrowCore PUBLIC JUCE_MODAL_LOOPS_PERMITTED=1)
    add_executable(DspTest Tests/DspTest.cpp)
    target_link_libraries(DspTest PRIVATE EchoThrowCore)
    add_executable(ProcessorTest Tests/ProcessorTest.cpp)
    target_link_libraries(ProcessorTest PRIVATE EchoThrowCore)
    add_executable(UiSnapshot Tests/UiSnapshot.cpp)
    target_link_libraries(UiSnapshot PRIVATE EchoThrowCore)
endif()
