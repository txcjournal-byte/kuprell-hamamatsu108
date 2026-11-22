juce_add_plugin(Hamamatsu108
    COMPANY_NAME                "Kuprell"
    BUNDLE_ID                   com.kuprell.harpology
    PLUGIN_MANUFACTURER_CODE    Kprl
    PLUGIN_CODE                 Hrpl
    PRODUCT_NAME                "Harpology"
    FORMATS                     VST3 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    ICON_BIG                    ${KUPRELL_APP_ICON})

target_sources(Hamamatsu108 PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/Instrument.cpp
    source/EditView.cpp
    source/Presets.cpp
    source/BrowserView.cpp
    source/Overlays.cpp
    source/ConsoleView.cpp
    ${CMAKE_SOURCE_DIR}/third_party/mts-esp/libMTSClient.cpp)

# MTS-ESP microtuning client (ODDSound, permissive licence)
target_include_directories(Hamamatsu108 PRIVATE ${CMAKE_SOURCE_DIR}/third_party/mts-esp)

target_include_directories(Hamamatsu108 PRIVATE source)

target_compile_definitions(Hamamatsu108 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    HAMA_MTS=1)

# the rendered console (design/v15): panel, knob filmstrip, fader caps
juce_add_binary_data(HarpologyArt
    HEADER_NAME HarpArt.h
    NAMESPACE HarpArt
    SOURCES
        art/console_bg.jpg
        art/knob_strip.png
        art/fadercap.png
        art/fadercap_dark.png)

target_link_libraries(Hamamatsu108
    PRIVATE
        HarpologyArt
        kuprell_core
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
