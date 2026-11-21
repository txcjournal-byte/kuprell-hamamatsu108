#pragma once
// HARPOLOGY hardware kit: everything on the main page is drawn as a physical part of one instrument.
// One light, from the top left. Raised parts: light top edge, dark bottom edge, a short drop shadow down-right.
// Cut-in parts (screens, slots): the reverse. No glow except where a value lives (phosphor, a lit LED).
// Calm on purpose: no grunge, no lens flares, no decoration that does nothing.
#include <juce_gui_basics/juce_gui_basics.h>

namespace hama::hw
{
    using C = juce::Colour;
    inline C metalHi()  { return C (0xff5a5a5e); }
    inline C metalMid() { return C (0xff35353a); }
    inline C metalLo()  { return C (0xff1d1d20); }
    inline C amber()    { return C (0xffe9a64a); }   // section headers on the dark console, printed, not lit
    inline C printDark(){ return C (0xff1b1a18); }   // ink printed on the concrete
    inline C printLight(){ return C (0xffd9d6ce); }  // ink printed on the metal

    // a raised part: drop shadow, body gradient, light top edge, dark bottom edge
    inline void raised (juce::Graphics& g, juce::Rectangle<float> r, C top, C bottom, float corner = 2.0f, float drop = 2.5f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (drop * 0.6f, drop), corner);
        g.setGradientFill (juce::ColourGradient (top, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillRect (r.reduced (corner, 0.0f).withHeight (1.0f));
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (r.reduced (corner, 0.0f).withTop (r.getBottom() - 1.0f));
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (r.expanded (0.5f), corner + 0.5f, 1.0f);
    }

    // a cut-in part (screen bezel, fader slot): dark rim, shadow inside at the top, light lip at the bottom
    inline void recess (juce::Graphics& g, juce::Rectangle<float> r, C fill, float rim = 3.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillRect (r.withTop (r.getBottom()).withHeight (1.0f).expanded (1.0f, 0.0f));
        g.setColour (C (0xff050505));
        g.fillRect (r);
        auto in = r.reduced (rim);
        g.setColour (fill);
        g.fillRect (in);
        for (int i = 0; i < 6; ++i)   // shadow cast by the top edge onto the glass
        {
            g.setColour (juce::Colours::black.withAlpha (0.16f * (1.0f - (float) i / 6.0f)));
            g.fillRect (in.withHeight (1.0f).translated (0.0f, (float) i));
        }
    }

    // a screw head: slot turned a little, flat, small
    inline void screw (juce::Graphics& g, juce::Point<float> c, float angle = 0.6f)
    {
        const float r = 4.0f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - r + 0.8f, c.y - r + 1.2f, 2 * r, 2 * r);
        g.setGradientFill (juce::ColourGradient (C (0xff8e8d8a), c.x, c.y - r, C (0xff3c3b39), c.x, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (C (0xff1a1a1a));
        g.drawLine ({ c.translated (-r * 0.7f * std::cos (angle), -r * 0.7f * std::sin (angle)), c.translated (r * 0.7f * std::cos (angle), r * 0.7f * std::sin (angle)) }, 1.2f);
    }

    // printed label with thin rules either side, like silkscreen on a panel
    inline void silk (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, C ink)
    {
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)).withExtraKerningFactor (0.22f));
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 18.0f;
        g.setColour (ink.withAlpha (0.55f));
        const float y = r.getCentreY();
        g.fillRect (r.getX(), y, (r.getWidth() - tw) * 0.5f, 1.0f);
        g.fillRect (r.getCentreX() + tw * 0.5f, y, (r.getWidth() - tw) * 0.5f, 1.0f);
        g.setColour (ink);
        g.drawText (text.toUpperCase(), r, juce::Justification::centred, false);
    }

    // a section header on the dark console: amber print, a rule under it
    inline void header (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text)
    {
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)).withExtraKerningFactor (0.2f));
        g.setColour (amber());
        g.drawText (text.toUpperCase(), r.withTrimmedLeft (2.0f), juce::Justification::centredLeft, false);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRect (r.withTop (r.getBottom() - 1.0f));
    }

    // a vertical divider on the console: a fine groove
    inline void groove (juce::Graphics& g, float x, float y0, float y1)
    {
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRect (x, y0, 1.0f, y1 - y0);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (x + 1.0f, y0, 1.0f, y1 - y0);
    }

    // an LED in its own little bezel
    inline void led (juce::Graphics& g, juce::Point<float> c, bool on, C colour, float d = 6.0f)
    {
        g.setColour (C (0xff0a0a0a));
        g.fillEllipse (c.x - d * 0.5f - 1.0f, c.y - d * 0.5f - 1.0f, d + 2.0f, d + 2.0f);
        if (on)
        {
            g.setColour (colour.withAlpha (0.28f));
            g.fillEllipse (c.x - d, c.y - d, d * 2.0f, d * 2.0f);
            g.setGradientFill (juce::ColourGradient (colour.brighter (0.9f), c.x - d * 0.2f, c.y - d * 0.25f, colour.darker (0.6f), c.x + d * 0.5f, c.y + d * 0.5f, true));
        }
        else
            g.setGradientFill (juce::ColourGradient (colour.darker (2.2f), c.x, c.y - d * 0.4f, C (0xff140806), c.x, c.y + d * 0.5f, false));
        g.fillEllipse (c.x - d * 0.5f, c.y - d * 0.5f, d, d);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.6f : 0.15f));
        g.fillEllipse (c.x - d * 0.25f, c.y - d * 0.32f, d * 0.3f, d * 0.22f);
    }

    // a push key (dark rubber-cap button). pressed = sits lower, its top a touch lighter
    inline void key (juce::Graphics& g, juce::Rectangle<float> r, bool down = false)
    {
        if (down) r = r.translated (0.0f, 1.0f);
        raised (g, r, down ? C (0xff4b4b50) : C (0xff47474c), C (0xff202023), 2.0f, down ? 1.2f : 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (r.reduced (3.0f).withHeight (r.getHeight() * 0.35f));
    }

    // a fader cap: black, a white index line, a lighter top face
    inline void faderCap (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRect (r.translated (1.5f, 3.0f));
        g.setGradientFill (juce::ColourGradient (C (0xff5b5b5f), 0.0f, r.getY(), C (0xff141416), 0.0f, r.getBottom(), false));
        g.fillRect (r);
        g.setColour (C (0xff2c2c2f));
        g.fillRect (r.reduced (2.0f, 0.0f).withTop (r.getCentreY()).withHeight (r.getHeight() * 0.5f - 2.0f));
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.fillRect (r.withHeight (1.0f));
        g.setColour (C (0xfff2f0ea));
        g.fillRect (r.getX() + 2.0f, r.getCentreY() - 1.0f, r.getWidth() - 4.0f, 2.0f);
        g.setColour (C (0xff050505));
        g.drawRect (r.expanded (0.5f), 1.0f);
    }

    // the dark gunmetal chassis with cheeks and the concrete face set into it
    inline void chassis (juce::Graphics& g, juce::Rectangle<float> body, float cheekW)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (body.translated (0.0f, 8.0f), 16.0f);
        g.setGradientFill (juce::ColourGradient (metalHi(), 0.0f, body.getY(), metalLo(), 0.0f, body.getBottom(), false));
        g.fillRoundedRectangle (body, 16.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (body.reduced (0.5f), 16.0f, 1.0f);
        // cheeks: a slanted block each side, light on the face, dark on the inner edge
        for (int side = 0; side < 2; ++side)
        {
            const bool left = side == 0;
            auto c = left ? body.withWidth (cheekW) : body.withLeft (body.getRight() - cheekW);
            juce::Path p;
            p.addRoundedRectangle (c.getX(), c.getY(), c.getWidth(), c.getHeight(), 16.0f, 16.0f, left, ! left, left, ! left);
            g.setGradientFill (juce::ColourGradient (left ? C (0xff4a4a4f) : C (0xff222225), c.getX(), 0.0f, left ? C (0xff222225) : C (0xff4a4a4f), c.getRight(), 0.0f, false));
            g.fillPath (p);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillRect (left ? c.getRight() - 1.0f : c.getX(), c.getY() + 10.0f, 1.0f, c.getHeight() - 20.0f);
        }
    }

    // a machined knob: tick scale, value arc, knurled skirt, smooth cap, white index line.
    // printedInk: colour of the tick scale (dark on concrete, light on metal)
    inline void knob (juce::Graphics& g, juce::Rectangle<float> area, float value, C arc, C printedInk, bool enabled = true)
    {
        const auto c = area.getCentre();
        const float R = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
        const float a0 = -juce::MathConstants<float>::pi * 0.75f, span = juce::MathConstants<float>::pi * 1.5f;
        // tick scale
        g.setColour (printedInk.withAlpha (0.7f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = a0 + span * (float) i / 10.0f;
            const float r0 = R - 1.5f, r1 = R + (i % 5 == 0 ? 1.5f : 0.3f);
            g.drawLine ({ c.translated (r0 * std::sin (a), -r0 * std::cos (a)), c.translated (r1 * std::sin (a), -r1 * std::cos (a)) }, 1.0f);
        }
        // value arc, just inside the scale
        const float ar = R - 4.0f;
        juce::Path track, val;
        track.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, a0 + span, true);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (track, juce::PathStrokeType (2.0f));
        if (enabled && value > 0.002f)
        {
            val.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, a0 + span * value, true);
            g.setColour (arc.withAlpha (0.25f));
            g.strokePath (val, juce::PathStrokeType (4.0f));
            g.setColour (arc);
            g.strokePath (val, juce::PathStrokeType (2.0f));
        }
        // the knob: shadow, knurled skirt, cap
        const float kr = R - 7.0f;
        if (kr < 3.0f) return;
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (c.x - kr + 1.2f, c.y - kr + 2.2f, kr * 2.0f, kr * 2.0f);
        g.setGradientFill (juce::ColourGradient (C (0xff5d5d62), c.x, c.y - kr, C (0xff151517), c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
        const int ridges = juce::jmax (16, (int) (kr * 3.0f));
        for (int i = 0; i < ridges; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.drawLine ({ c.translated ((kr - 0.5f) * std::sin (a), -(kr - 0.5f) * std::cos (a)), c.translated ((kr * 0.78f) * std::sin (a), -(kr * 0.78f) * std::cos (a)) }, 0.8f);
        }
        const float cr = kr * 0.74f;
        g.setGradientFill (juce::ColourGradient (C (0xff4c4c51), c.x - cr * 0.4f, c.y - cr, C (0xff1c1c1f), c.x + cr * 0.4f, c.y + cr, false));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 0.7f);
        g.setColour (juce::Colours::black);
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 0.8f);
        // index line
        const float ang = a0 + span * value;
        g.setColour (enabled ? C (0xfff4f2ec) : C (0xff6a6a6a));
        g.drawLine ({ c.translated (cr * 0.15f * std::sin (ang), -cr * 0.15f * std::cos (ang)), c.translated ((kr - 1.0f) * std::sin (ang), -(kr - 1.0f) * std::cos (ang)) }, juce::jmax (1.4f, kr * 0.16f));
    }
}
