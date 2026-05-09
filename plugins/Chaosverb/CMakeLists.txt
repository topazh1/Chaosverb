cmake_minimum_required(VERSION 3.15)

# CRITICAL: Do NOT include project() or add_subdirectory(JUCE) here.
# JUCE is added at the root CMakeLists.txt level.

juce_add_plugin(Chaosverb
    COMPANY_NAME "NBS"
    PLUGIN_MANUFACTURER_CODE NbsA
    PLUGIN_CODE Chao
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Chaosverb"
    VERSION "0.0.8"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_WEB_BROWSER TRUE
)

# Source files
target_sources(Chaosverb
    PRIVATE
        Source/PluginProcessor.cpp
        Source/ChaosverbEQ.cpp
        Source/ChaosverbWowFlutter.cpp
        Source/ChaosverbSpectralSquash.cpp
        Source/ChaosverbSaturation.cpp
        Source/ChaosverbChorus.cpp
        Source/PluginEditor.cpp
        Source/ChaosverbFDN.h
)

# Include paths
target_include_directories(Chaosverb
    PRIVATE
        Source
)

# Required JUCE modules
target_link_libraries(Chaosverb
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# CRITICAL: Generate JuceHeader.h — MUST come after target_link_libraries() (Pattern 1)
juce_generate_juce_header(Chaosverb)

# Compile definitions
target_compile_definitions(Chaosverb
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
)

# WebView UI Resources — Stage 3 GUI integration
juce_add_binary_data(Chaosverb_UIResources
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
)

target_link_libraries(Chaosverb
    PRIVATE
        Chaosverb_UIResources
)
