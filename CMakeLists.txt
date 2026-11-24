cmake_minimum_required(VERSION 3.22)
project(NeptuneMB VERSION 1.0.0)

# JUCE lives next to this file (git clone https://github.com/juce-framework/JUCE.git)
add_subdirectory(JUCE)

if(APPLE)
    set(CPP_FORMATS VST3 AU Standalone)
else()
    set(CPP_FORMATS VST3 Standalone)
endif()

juce_add_plugin(NeptuneMB
    COMPANY_NAME "Konner Hall"
    BUNDLE_ID com.konnerhall.neptunemb
    PLUGIN_MANUFACTURER_CODE Khal
    PLUGIN_CODE Nptn
    PRODUCT_NAME "Neptune MB"
    IS_SYNTH FALSE                # audio effect -> shows under Effects > Distortion
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    FORMATS ${CPP_FORMATS})

# Inter font (SIL Open Font License) embedded so the UI looks identical on every machine
juce_add_binary_data(NepFonts SOURCES Resources/Inter-Light.ttf Resources/Inter-SemiBold.ttf Resources/Inter-Bold.ttf)

target_sources(NeptuneMB PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(NeptuneMB PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(NeptuneMB
    PRIVATE NepFonts juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Optional helper that renders the editor to PNGs and runs a DSP self-test
option(CPP_BUILD_SNAPSHOT "Build UI snapshot tool" OFF)
if(CPP_BUILD_SNAPSHOT)
    juce_add_gui_app(UISnapshot PRODUCT_NAME "UISnapshot")
    target_sources(UISnapshot PRIVATE Tools/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(UISnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 CPP_SNAPSHOT=1)
    target_link_libraries(UISnapshot PRIVATE NepFonts juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
