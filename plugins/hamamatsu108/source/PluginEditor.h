#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Instrument.h"
#include "KuprellLookAndFeel.h"
#include "KuprellTopBar.h"

// Status bar: what you are touching, plus voices and zoom.
class StatusBar : public juce::Component
{
public:
    static constexpr int height = 28;
    void setText (const juce::String& name, const juce::String& value);
    void setInfo (int voices, int zoomPercent);
    void paint (juce::Graphics&) override;

private:
    juce::String name { "Click a chord button to start" }, value;
    int voices = 0, zoom = 100;
};

class Hamamatsu108Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int baseWidth = 1280, baseHeight = 820;
    static constexpr double minScale = 0.5, maxScale = 2.0;

    explicit Hamamatsu108Editor (Hamamatsu108Processor&);
    ~Hamamatsu108Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    int initialWidth() const;
    void say (const juce::String& text);

    Hamamatsu108Processor& audioProcessor;

    kuprell::LookAndFeel lookAndFeel;   // declared first, destroyed last
    juce::Component content;            // drawn at 1280 x 820 and scaled as a whole (50-200 %)
    kuprell::TopBar topBar { "Hamamatsu 108" };
    hama::InstrumentView instrument;
    StatusBar status;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Hamamatsu108Editor)
};
