#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "HarpArt.h"

// HARPOLOGY console style for every page that opens over the console (presets, setup, effects):
// brushed graphite, raised plates with a mint title strip, rubber keys with a red LED, green LCD glass.
namespace hama::cs
{
    inline const juce::Colour mint { 0xff72f2bd }, bone { 0xfff0eee8 }, ledRed { 0xffee2a35 }, ink { 0xffcfd2d4 }, dim { 0xff8d9296 };
    inline const juce::Colour lcdTop { 0xff0f3326 }, lcdBottom { 0xff06170f };

    // brushed graphite, a fine horizontal grain (one cached tile)
    inline const juce::Image& grain()
    {
        static juce::Image tile = []
        {
            juce::Image img (juce::Image::ARGB, 512, 256, true);
            juce::Graphics g (img);
            juce::Random rnd (81);
            for (int y = 0; y < 256; ++y)
            {
                float x = 0.0f;
                while (x < 512.0f)
                {
                    const float len = 20.0f + rnd.nextFloat() * 160.0f;
                    const float a = rnd.nextFloat() * 0.06f;
                    g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (a));
                    g.fillRect (x, (float) y, len, 1.0f);
                    x += len;
                }
            }
            return img;
        }();
        return tile;
    }

    inline void graphite (juce::Graphics& g, juce::Rectangle<float> r, float lift = 0.0f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35383b).brighter (lift), r.getX(), r.getY(),
                                                 juce::Colour (0xff222426).brighter (lift), r.getX(), r.getBottom(), false));
        g.fillRect (r);
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());
        const auto& t = grain();
        for (float y = r.getY(); y < r.getBottom(); y += (float) t.getHeight())
            for (float x = r.getX(); x < r.getRight(); x += (float) t.getWidth())
                g.drawImageAt (t, (int) x, (int) y);
    }

    // the dark frame around a page, with a bevel like the console body
    inline void pageBackground (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colour (0xff121314));
        g.fillRect (r);
        auto body = r.reduced (8.0f);
        graphite (g, body);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.fillRect (body.withHeight (1.5f));
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRect (body.withTop (body.getBottom() - 2.0f));
    }

    // a raised plate with its title in a mint strip (the sections of the console)
    inline void plate (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 4.0f);
        juce::Path p; p.addRoundedRectangle (r, 4.0f);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (p);
            graphite (g, r, 0.06f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 4.0f, r.getRight() - 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        if (title.isEmpty()) return;
        auto strip = r.reduced (8.0f, 0.0f).withY (r.getY() + 7.0f).withHeight (18.0f);
        g.setColour (juce::Colour (0xff141617));
        g.fillRoundedRectangle (strip, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawHorizontalLine ((int) strip.getBottom(), strip.getX(), strip.getRight());
        g.setColour (mint);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.16f));
        g.drawText (title.toUpperCase(), strip, juce::Justification::centred, false);
    }

    // a recessed well (the strings bed look: dark green glass under the plate)
    inline void well (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b1c16), r.getX(), r.getY(), juce::Colour (0xff10261d), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 12.0f, false));
        g.fillRoundedRectangle (r.withHeight (12.0f), 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawRoundedRectangle (r, 3.0f, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine ((int) r.getBottom() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);
    }

    inline void led (juce::Graphics& g, juce::Point<float> c, bool on, float d = 7.0f)
    {
        auto r = juce::Rectangle<float> (d, d).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.fillEllipse (r.expanded (1.5f));
        if (on) { g.setColour (ledRed.withAlpha (0.3f)); g.fillEllipse (r.expanded (d * 0.6f)); }
        g.setGradientFill (on ? juce::ColourGradient (juce::Colour (0xffffb0a8), r.getX() + d * 0.35f, r.getY() + d * 0.3f, ledRed.darker (0.3f), r.getRight(), r.getBottom(), true)
                              : juce::ColourGradient (juce::Colour (0xff5a2427), r.getX() + d * 0.35f, r.getY() + d * 0.3f, juce::Colour (0xff2a1012), r.getRight(), r.getBottom(), true));
        g.fillEllipse (r);
    }

    // a rubber key of the console: dark cap, soft top light, a red LED, the label on the cap
    inline void key (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool lit, bool enabled = true, float fontH = 11.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.5f), 4.0f);
        g.setColour (juce::Colour (0xff0c0d0e));
        g.fillRoundedRectangle (r, 4.0f);
        auto cap = r.reduced (1.0f).withTrimmedBottom (2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b4e51), cap.getX(), cap.getY(), juce::Colour (0xff2c2e30), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.5f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + 4.0f, cap.getRight() - 4.0f);
        if (text.isEmpty()) { led (g, cap.getCentre(), lit); return; }
        g.setFont (juce::Font (juce::FontOptions (fontH, juce::Font::bold)).withExtraKerningFactor (0.08f));
        const auto label = text.toUpperCase();
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label);
        const float x0 = cap.getCentreX() - (tw + 14.0f) * 0.5f;
        led (g, { x0 + 3.0f, cap.getCentreY() }, lit, 6.0f);
        g.setColour (enabled ? (lit ? bone : ink) : dim.withAlpha (0.6f));
        g.drawText (label, juce::Rectangle<float> (x0 + 12.0f, cap.getY(), tw + 4.0f, cap.getHeight()), juce::Justification::centredLeft, false);
    }

    // the green LCD of the console
    inline void lcd (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colour (0xff050606));
        g.fillRoundedRectangle (r.expanded (3.0f), 4.0f);
        g.setGradientFill (juce::ColourGradient (lcdTop, r.getX(), r.getY(), lcdBottom, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 2.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.45f, false));
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f), 2.5f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 2.5f, 1.0f);
    }
    inline void lcdText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, float alpha = 1.0f)
    {
        g.setColour (mint.withAlpha (0.18f * alpha));
        g.drawText (t, r.translated (0.0f, 0.7f).expanded (0.6f, 0.0f), j, false);
        g.setColour (mint.withAlpha (alpha));
        g.drawText (t, r, j, false);
    }

    // the console knob (3D filmstrip, 61 frames, the knob is 60 of 84 px)
    inline void knob (juce::Graphics& g, juce::Rectangle<float> r, float v, bool enabled)
    {
        static juce::Image strip = juce::ImageCache::getFromMemory (HarpArt::knob_strip_png, HarpArt::knob_strip_pngSize);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        const float side = d * 84.0f / 60.0f;
        const int w = strip.getWidth(), frames = strip.getHeight() / juce::jmax (1, w);
        const int f = juce::jlimit (0, frames - 1, juce::roundToInt (juce::jlimit (0.0f, 1.0f, v) * (float) (frames - 1)));
        const auto dst = juce::Rectangle<float> (side, side).withCentre (r.getCentre()).toNearestInt();
        g.setOpacity (enabled ? 1.0f : 0.45f);
        g.drawImage (strip, dst.getX(), dst.getY(), dst.getWidth(), dst.getHeight(), 0, f * w, w, w);
        g.setOpacity (1.0f);
    }
}
