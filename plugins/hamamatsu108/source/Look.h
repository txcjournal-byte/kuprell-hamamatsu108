#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "KuprellLookAndFeel.h"
#include "Instrument.h"

// Shared drawing helpers for the Hamamatsu 144 views (hardware look).
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

    // brutalist skin: no shouting labels. "LAYER A  ·  ENGINE" -> "Layer A", hints after the dot are dropped
    inline juce::String quiet (const juce::String& t)
    {
        auto head = t.upToFirstOccurrenceOf (juce::String::fromUTF8 ("  \xc2\xb7"), false, false).trim();
        juce::StringArray w; w.addTokens (head, " ", "");
        static const juce::StringArray keep { "A", "B", "LFO", "FX", "MIDI", "ADSR", "1", "2", "3", "4", "5", "6", "7", "8" };
        for (int i = 0; i < w.size(); ++i)
        {
            if (keep.contains (w[i])) continue;
            const auto lower = w[i].toLowerCase();
            w.set (i, i == 0 ? lower.substring (0, 1).toUpperCase() + lower.substring (1) : lower);
        }
        return w.joinIntoString (" ").replace ("&", "and");
    }

    inline float plain (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue (id)->load(); }

    // recessed area with an inner shadow
    inline void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        if (th().concrete)
        {
            // a cut into the concrete: flat dark void, one hard edge of shadow on top, one of light below
            g.setColour (juce::Colour (0xff1d1d1c));
            g.fillRect (r);
            g.setColour (juce::Colour (0xff0e0e0d));
            g.fillRect (r.withHeight (3.0f));
            g.fillRect (r.withWidth (2.0f));
            g.setColour (juce::Colour (0xff6f6d68));
            g.fillRect (r.withTop (r.getBottom() - 1.0f));
            return;
        }
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
        if (th().concrete)
        {
            // a flat cast tile: in the light when on, with the red lamp
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRect (r.translated (1.5f, 2.0f));
            g.setColour (lit ? page::lit() : juce::Colour (0xff4b4a46));
            g.fillRect (r);
            g.setColour (lit ? juce::Colours::white.withAlpha (0.6f) : juce::Colour (0xff6c6a65));
            g.fillRect (r.withHeight (1.0f));
            if (text.isEmpty()) return;
            g.setFont (LookAndFeel::uiFont (10.5f));
            const auto label = quiet (text);
            const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label);
            const float x0 = r.getCentreX() - (tw + 11.0f) * 0.5f;
            g.setColour (lit ? page::led() : juce::Colour (0xff2a2927));
            g.fillEllipse (x0, r.getCentreY() - 2.5f, 5.0f, 5.0f);
            g.setColour (! enabled ? page::dim() : lit ? page::litInk() : page::ink());
            g.drawText (label, juce::Rectangle<float> (x0 + 11.0f, r.getY(), tw + 4.0f, r.getHeight()), juce::Justification::centredLeft, false);
            return;
        }
        const float side = 4.0f;
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 6.0f);
        g.setColour (juce::Colour (0xff0a0b0c));
        g.fillRoundedRectangle (r, 6.0f);
        auto cap = r.withTrimmedBottom (side);
        if (th().lcd) stripLed = true;   // screen skins: every button has the same backlit bar
        g.setGradientFill (th().lcd ? juce::ColourGradient (juce::Colour (0xff2c2e32), cap.getX(), cap.getY(), juce::Colour (0xff1c1d20), cap.getX(), cap.getBottom(), false)
                                    : juce::ColourGradient (juce::Colour (0xff4a4f56), cap.getX(), cap.getY(),
                                                 juce::Colour (0xff2c2f34), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + 5.0f, cap.getRight() - 5.0f);

        if (text.isEmpty()) return;
        g.setColour (enabled ? juce::Colour (0xffecedef) : juce::Colour (0xff7e8187));
        g.setFont (spaced (9.5f, true, 0.06f));
        if (stripLed)
        {
            auto strip = juce::Rectangle<float> (th().lcd ? juce::jmax (16.0f, cap.getWidth() * 0.36f) : 16.0f, 3.0f).withCentre ({ cap.getCentreX(), cap.getY() + 9.0f });
            if (lit)
            {
                g.setColour (th().accent.withAlpha (0.4f));
                g.fillRoundedRectangle (strip.expanded (3.0f, 2.0f), 3.0f);
                g.setColour (th().accent);
            }
            else
                g.setColour (th().lcd ? juce::Colour (0xff3e4146) : juce::Colour (0xff24272b));
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

   #ifndef LCD_TOP
    #define LCD_TOP 0xff6ffaf3
    #define LCD_MID 0xff10e8e0
    #define LCD_BOT 0xff05bdb8
    #define LCD_INK 0xff012422
   #endif
    inline void drawDisplay (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        if (th().concrete)
        {
            g.setColour (juce::Colour (0xff0a0f0c));
            g.fillRect (r);
            g.setColour (juce::Colour (0xff070807));
            g.fillRect (r.withHeight (1.5f));
            g.setColour (juce::Colour (0xff3b3a37));
            g.fillRect (r.withTop (r.getBottom() - 1.0f));
            return;
        }
        if (th().lcdNow)
        {
            // backlit LCD: pale aqua glass, a soft inner shadow at the top
            // glow around the glass, then a strong aqua backlight
            g.setColour (th().accent.withAlpha (0.35f));
            g.fillRoundedRectangle (r.expanded (2.5f), radius + 2.5f);
            juce::ColourGradient lcd (juce::Colour (LCD_TOP), r.getX(), r.getY(), juce::Colour (LCD_BOT), r.getX(), r.getBottom(), false);
            lcd.addColour (0.45, juce::Colour (LCD_MID));
            g.setGradientFill (lcd);
            g.fillRoundedRectangle (r, radius);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.22f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 8.0f, false));
            g.fillRoundedRectangle (r, radius);
            g.setColour (juce::Colours::black.withAlpha (0.85f));
            g.drawRoundedRectangle (r, radius, 1.5f);
            return;
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff040607), r.getX(), r.getY(),
                                                 juce::Colour (0xff0a0e10), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, radius, 1.0f);
    }

    inline void glowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j, float alpha = 1.0f)
    {
        if (th().concrete)
        {
            g.setColour (page::phosphor().withAlpha (0.16f * alpha));
            g.drawText (text, r.translated (0.0f, 0.8f), j, false);
            g.setColour (page::phosphor().withAlpha (alpha));
            g.drawText (text, r, j, false);
            return;
        }
        if (th().lcdNow)
        {
            g.setColour (juce::Colours::black.withAlpha (0.08f * alpha));
            g.drawText (text, r.translated (0.8f, 0.8f), j, false);   // LCD segment shadow
            g.setColour (juce::Colour (LCD_INK).withAlpha (0.95f * alpha));
            g.drawText (text, r, j, false);
            return;
        }
        g.setColour (th().accent.withAlpha (0.22f * alpha));
        g.drawText (text, r.translated (0.0f, 0.6f), j, false);
        g.setColour (th().soft.withAlpha (alpha));
        g.drawText (text, r, j, false);
    }

}
