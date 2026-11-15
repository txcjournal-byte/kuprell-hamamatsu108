#include "KuprellLookAndFeel.h"

namespace kuprell
{
    LookAndFeel::LookAndFeel()
    {
        using namespace colours;

        setColour (juce::ResizableWindow::backgroundColourId, window);
        setColour (juce::TextButton::buttonColourId, control);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, onAccent);
        setColour (juce::ComboBox::backgroundColourId, controlLo);
        setColour (juce::ComboBox::outlineColourId, border);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1e21));
        setColour (juce::PopupMenu::textColourId, textSoft);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.18f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0141619));
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, border);
    }

    void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool isMouseOverButton, bool isButtonDown)
    {
        using namespace colours;

        auto r = button.getLocalBounds().toFloat().reduced (0.5f);
        const float corner = 7.0f;
        const bool isAccent = button.getComponentID() == "accent";

        juce::Colour top    = isAccent ? accent     : control;
        juce::Colour bottom = isAccent ? accentDeep : controlLo;

        if (isMouseOverButton) { top = top.brighter (0.12f); bottom = bottom.brighter (0.12f); }
        if (isButtonDown)      { std::swap (top, bottom); }

        g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);

        g.setColour (isAccent ? accentDeep : border);
        g.drawRoundedRectangle (r, corner, 1.0f);

        // thin top highlight, like a machined edge
        g.setColour (juce::Colours::white.withAlpha (isAccent ? 0.25f : 0.06f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
    }

    juce::Font LookAndFeel::getTextButtonFont (juce::TextButton& button, int)
    {
        return uiFont (button.getComponentID() == "accent" ? 11.5f : 12.5f, true);
    }

    juce::Font LookAndFeel::uiFont (float height, bool bold)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font LookAndFeel::monoFont (float height)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
    }
}
