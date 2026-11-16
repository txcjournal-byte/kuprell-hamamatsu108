#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "KuprellColours.h"

namespace kuprell
{
    // Hardware-style knob: value ring, knurled black body, polished chrome cap, pointer.
    // Label and value are printed under it. Works with any APVTS parameter (float, int or choice).
    // Double-click resets; right-click opens Reset / Type value; mouse wheel and drag change it.
    class Knob : public juce::Component, private juce::Slider::Listener
    {
    public:
        struct Style
        {
            juce::Colour ring = colours::accent;
            juce::Colour track { 0x1fffffff };
            juce::Colour pointer = juce::Colours::white;
            juce::Colour label { 0xffb9bec4 };
            juce::Colour value = colours::accentSoft;
            float labelHeight = 9.5f;
            bool showValue = true;
        };

        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& label, int knobSize, Style);
        // placeholder knob for a feature that arrives later (drawn dimmed, not editable)
        Knob (const juce::String& label, int knobSize, Style, const juce::String& comingSoonText);
        ~Knob() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        int getKnobSize() const noexcept { return knobSize; }
        void setLabel (const juce::String& newLabel) { if (newLabel != label) { label = newLabel; repaint(); } }
        void setStyle (const Style& s);
        juce::Slider& getSlider() noexcept;
        juce::String getDisplayName() const;
        juce::String getValueText() const;

        // called while the user hovers or turns the knob (for the status bar)
        std::function<void (const Knob&)> onInteract;

    private:
        class Dial;
        void sliderValueChanged (juce::Slider*) override;
        void showMenu();

        juce::RangedAudioParameter* parameter = nullptr;
        juce::String label, comingSoon;
        int knobSize;
        Style style;
        std::unique_ptr<Dial> dial;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
    };
}
