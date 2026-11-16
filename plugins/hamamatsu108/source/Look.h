#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "KuprellLookAndFeel.h"
#include "Instrument.h"

// Shared drawing helpers for the Hamamatsu 108 views (hardware look).
namespace hama::look
{
    using kuprell::LookAndFeel;

    inline const juce::Colour engrave { 0xffa7aeb6 };
    inline const juce::Colour wellTop { 0xff0d0f11 }, wellBottom { 0xff17191c };

    inline juce::Font spaced (float h, bool bold, float kerning)
    {
        return LookAndFeel::uiFont (h, bold).withExtraKerningFactor (kerning);
    }

    inline void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float plainValue)
    {
        if (auto* p = s.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
            p->endChangeGesture();
        }
    }

    inline float plain (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue (id)->load(); }

    // recessed area with an inner shadow
    inline void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        g.setGradientFill (juce::ColourGradient (wellTop, r.getX(), r.getY(), wellBottom, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);

        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, radius);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.85f), r.getX(), r.getY(),
                                                 juce::Colours::transparentBlack, r.getX(), r.getY() + 16.0f, false));
        g.fillRect (r.withHeight (16.0f));
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) r.getBottom() - 2, r.getX() + radius, r.getRight() - radius);
    }

    inline void drawLed (juce::Graphics& g, juce::Rectangle<float> r, bool on)
    {
        if (on)
        {
            g.setColour (th().accent.withAlpha (0.35f));
            g.fillEllipse (r.expanded (r.getWidth() * 0.6f));
            juce::ColourGradient led (juce::Colours::white, r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.35f,
                                      th().deep, r.getRight(), r.getBottom(), true);
            led.addColour (0.55, th().accent);
            g.setGradientFill (led);
        }
        else
            g.setColour (juce::Colour (0xff24272b));
        g.fillEllipse (r);
    }

    // 3D push button (raised cap with a dark side), optional LED strip or dot
    inline void drawHardButton (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool lit, bool enabled, bool stripLed)
    {
        const float side = 4.0f;
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 6.0f);
        g.setColour (juce::Colour (0xff0a0b0c));
        g.fillRoundedRectangle (r, 6.0f);
        auto cap = r.withTrimmedBottom (side);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4f56), cap.getX(), cap.getY(),
                                                 juce::Colour (0xff2c2f34), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + 5.0f, cap.getRight() - 5.0f);

        if (text.isEmpty()) return;
        g.setColour (enabled ? juce::Colour (0xffecedef) : juce::Colour (0xff7e8187));
        g.setFont (spaced (9.5f, true, 0.06f));
        if (stripLed)
        {
            auto strip = juce::Rectangle<float> (16.0f, 3.0f).withCentre ({ cap.getCentreX(), cap.getY() + 9.0f });
            if (lit)
            {
                g.setColour (th().accent.withAlpha (0.4f));
                g.fillRoundedRectangle (strip.expanded (3.0f, 2.0f), 3.0f);
                g.setColour (th().accent);
            }
            else
                g.setColour (juce::Colour (0xff24272b));
            g.fillRoundedRectangle (strip, 1.5f);
            g.setColour (enabled ? juce::Colour (0xffecedef) : juce::Colour (0xff7e8187));
            g.drawText (text, cap.withTrimmedTop (12.0f), juce::Justification::centred, false);
        }
        else
        {
            const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text);
            const float x0 = cap.getCentreX() - (tw + 15.0f) * 0.5f;
            drawLed (g, { x0, cap.getCentreY() - 4.0f, 8.0f, 8.0f }, lit);
            g.setColour (enabled ? juce::Colour (0xffecedef) : juce::Colour (0xff7e8187));
            g.drawText (text, juce::Rectangle<float> (x0 + 15.0f, cap.getY(), tw + 4.0f, cap.getHeight()), juce::Justification::centredLeft, false);
        }
    }

    inline void drawDisplay (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff040607), r.getX(), r.getY(),
                                                 juce::Colour (0xff0a0e10), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, radius, 1.0f);
    }

    inline void glowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j, float alpha = 1.0f)
    {
        g.setColour (th().accent.withAlpha (0.22f * alpha));
        g.drawText (text, r.translated (0.0f, 0.6f), j, false);
        g.setColour (th().soft.withAlpha (alpha));
        g.drawText (text, r, j, false);
    }

}
