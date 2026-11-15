#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "KuprellLookAndFeel.h"
#include "KuprellTopBar.h"

// Placeholder for the instrument area. In step 2 it is replaced by the 3D instrument.
class SkeletonStage : public juce::Component
{
public:
    void setActivity (int lastNote, int heldNotes, float level);
    void setMessage (const juce::String& text);
    void paint (juce::Graphics&) override;

private:
    int note = -1, held = 0;
    float meter = 0.0f;
    juce::String message { "Play your MIDI keyboard: you should hear a soft test chime." };
};

class Hamamatsu108Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int baseWidth = 1280, baseHeight = 820;

    explicit Hamamatsu108Editor (Hamamatsu108Processor&);
    ~Hamamatsu108Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void say (const juce::String& text);

    Hamamatsu108Processor& audioProcessor;

    kuprell::LookAndFeel lookAndFeel;   // declared first, destroyed last
    juce::Component content;            // drawn at 1280 x 820 and scaled as a whole (75-200 %)
    kuprell::TopBar topBar { "Hamamatsu 108" };
    SkeletonStage stage;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Hamamatsu108Editor)
};
