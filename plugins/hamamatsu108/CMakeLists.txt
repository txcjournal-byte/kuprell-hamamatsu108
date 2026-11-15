juce_add_plugin(Hamamatsu108
    COMPANY_NAME                "Kuprell"
    BUNDLE_ID                   com.kuprell.hamamatsu108
    PLUGIN_MANUFACTURER_CODE    Kprl
    PLUGIN_CODE                 Hm08
    PRODUCT_NAME                "Hamamatsu 108"
    FORMATS                     VST3 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    ICON_BIG                    ${KUPRELL_APP_ICON})

target_sources(Hamamatsu108 PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp)

target_include_directories(Hamamatsu108 PRIVATE source)

target_compile_definitions(Hamamatsu108 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Hamamatsu108
    PRIVATE
        kuprell_core
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
