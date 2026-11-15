#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "KuprellColours.h"

namespace kuprell
{
    // Shared look and feel for the Kuprell frame: dark graphite controls, one turquoise accent.
    // Give a TextButton the component ID "accent" to draw it as the highlighted (turquoise) button.
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                   bool isMouseOverButton, bool isButtonDown) override;

        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

        static juce::Font uiFont (float height, bool bold = false);
        static juce::Font monoFont (float height);
    };
}
