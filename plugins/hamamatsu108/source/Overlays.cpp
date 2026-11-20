#include "Overlays.h"
#include "Skins.h"
#include "KuprellBrandAssets.h"

using namespace kuprell;
using namespace hama::look;

namespace hama
{
    namespace
    {
        const int zooms[] = { 60, 75, 85, 100, 125, 150 };
        const int voiceOptions[] = { 16, 32, 48, 64 };

        void panelBox (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            drawWell (g, r, 14.0f);
            g.setFont (spaced (9.0f, true, 0.28f));
            g.setColour (engrave);
            g.drawText (title, r.reduced (16.0f, 0.0f).withHeight (30.0f), juce::Justification::centredLeft, false);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawHorizontalLine ((int) r.getY() + 30, r.getX() + 14.0f, r.getRight() - 14.0f);
        }

        void label (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r)
        {
            g.setFont (spaced (9.0f, true, 0.18f));
            g.setColour (juce::Colour (0xff9aa0a7));
            g.drawText (t, r, juce::Justification::centredLeft, false);
        }

        void choiceButton (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on)
        {
            g.setColour (on ? th().accent.withAlpha (0.18f) : juce::Colour (0xff17191c));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (on ? th().accent : juce::Colour (0xff2c2f34));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, on ? 1.5f : 1.0f);
            g.setFont (LookAndFeel::uiFont (12.5f, on));
            g.setColour (on ? th().soft : juce::Colour (0xffc9ccd1));
            g.drawText (t, r, juce::Justification::centred, false);
        }
    }

    //==========================================================================
    SettingsView::SettingsView (Hamamatsu108Processor& p) : processor (p) { startTimerHz (4); }
    SettingsView::~SettingsView() { stopTimer(); }
    void SettingsView::timerCallback() { if (isVisible()) repaint(); }

    // HARPOLOGY settings: only what a player changes. Theme, window, tuning, voices, computer keys, MIDI learn.
    juce::Rectangle<float> SettingsView::swatch (int i) const { return { 44.0f + (float) i * 124.0f, 68.0f, 112.0f, 190.0f }; }
    juce::Rectangle<float> SettingsView::zoomButton (int i) const { return { 830.0f + (i % 3) * 136.0f, 68.0f + (i / 3) * 42.0f, 126.0f, 34.0f }; }
    juce::Rectangle<float> SettingsView::rowButton (int row, int i) const { return { 180.0f + i * 92.0f, 384.0f + row * 50.0f, 86.0f, 34.0f }; }

    namespace
    {
        void box (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            drawWell (g, r, 0.0f);
            g.setFont (LookAndFeel::uiFont (13.0f));
            g.setColour (page::ink());
            g.drawText (title, r.reduced (16.0f, 0.0f).withHeight (34.0f), juce::Justification::centredLeft, false);
        }
        void tile (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on)
        {
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRect (r.translated (1.5f, 2.0f));
            g.setColour (on ? page::lit() : juce::Colour (0xff4b4a46));
            g.fillRect (r);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.5f : 0.12f));
            g.fillRect (r.withHeight (1.0f));
            g.setFont (LookAndFeel::uiFont (12.5f));
            g.setColour (on ? page::litInk() : page::ink());
            g.drawText (t, r, juce::Justification::centred, false);
        }
        void rowLabel (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r)
        {
            g.setFont (LookAndFeel::uiFont (12.5f));
            g.setColour (page::dim().brighter (0.3f));
            g.drawText (t, r, juce::Justification::centredLeft, false);
        }
        constexpr int firstTheme = 14;   // skins 14..19 are the Harpology themes
    }

    void SettingsView::paint (juce::Graphics& g)
    {
        g.fillAll (page::theme().hall);
        g.setColour (page::theme().slab);
        g.fillRect (getLocalBounds().toFloat().reduced (8.0f));
        g.setColour (page::theme().slab.brighter (0.45f));
        g.fillRect (getLocalBounds().toFloat().reduced (8.0f).withHeight (2.0f));

        // themes: each one a small cast block with its screen and lamp
        box (g, { 24.0f, 20.0f, 770.0f, 270.0f }, "Theme");
        const int cur = processor.skin.load() - firstTheme;
        for (int i = 0; i < (int) page::themes().size(); ++i)
        {
            const auto& t = page::themes()[(size_t) i];
            auto r = swatch (i);
            auto block = r.withTrimmedBottom (30.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRect (block.translated (3.0f, 4.0f));
            g.setColour (t.slab);
            g.fillRect (block);
            g.setColour (t.slab.brighter (0.45f));
            g.fillRect (block.withHeight (1.5f));
            auto scr = block.reduced (12.0f).withHeight (64.0f);
            g.setColour (t.screenGlass);
            g.fillRect (scr);
            g.setColour (t.phosphor);
            juce::Path wave;
            for (int k = 0; k <= 40; ++k)
            {
                const float x = scr.getX() + 6.0f + (scr.getWidth() - 12.0f) * (float) k / 40.0f;
                const float y = scr.getCentreY() + std::sin ((float) k * 0.45f) * 14.0f * std::exp (-(float) k * 0.03f);
                if (k == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
            }
            g.strokePath (wave, juce::PathStrokeType (1.2f));
            g.setColour (t.led);
            g.fillEllipse (block.getX() + 12.0f, block.getBottom() - 22.0f, 7.0f, 7.0f);
            g.setColour (t.wordmark);
            g.setFont (LookAndFeel::uiFont (10.0f));
            g.drawText ("Harpology", block.withTrimmedLeft (26.0f).withTop (block.getBottom() - 30.0f).withHeight (24.0f), juce::Justification::centredLeft, false);
            if (i == cur) { g.setColour (page::lit()); g.drawRect (block.expanded (4.0f), 2.0f); }
            g.setFont (LookAndFeel::uiFont (13.0f));
            g.setColour (i == cur ? page::lit() : page::ink().withAlpha (0.75f));
            g.drawText (t.name, r.withTop (r.getBottom() - 24.0f), juce::Justification::centred, false);
        }

        box (g, { 806.0f, 20.0f, 450.0f, 170.0f }, "Window size");
        const int z = currentZoom ? currentZoom() : 100;
        for (int i = 0; i < 6; ++i) tile (g, zoomButton (i), juce::String (zooms[i]) + " %", std::abs (z - zooms[i]) <= 2);
        tile (g, { 830.0f, 152.0f, 398.0f, 28.0f }, "Fit to screen", false);

        box (g, { 806.0f, 202.0f, 450.0f, 88.0f }, "Tuning");
        tile (g, { 960.0f, 240.0f, 40.0f, 34.0f }, juce::String::fromUTF8 ("\xe2\x88\x92"), false);
        auto tv = juce::Rectangle<float> (1006.0f, 240.0f, 110.0f, 34.0f);
        drawDisplay (g, tv, 0.0f);
        g.setFont (LookAndFeel::monoFont (14.0f));
        glowText (g, juce::String (processor.tuningA4.load(), 1) + " Hz", tv, juce::Justification::centred);
        tile (g, { 1122.0f, 240.0f, 40.0f, 34.0f }, "+", false);
        tile (g, { 1168.0f, 240.0f, 66.0f, 34.0f }, "440", false);
        rowLabel (g, "A4", { 830.0f, 240.0f, 120.0f, 34.0f });

        box (g, { 24.0f, 302.0f, 770.0f, 220.0f }, "Playing");
        rowLabel (g, "Voices", { 40.0f, 384.0f, 130.0f, 34.0f });
        for (int i = 0; i < 4; ++i) tile (g, rowButton (0, i), juce::String (voiceOptions[i]), processor.maxVoices.load() == voiceOptions[i]);
        rowLabel (g, "Computer keys", { 40.0f, 434.0f, 130.0f, 34.0f });
        tile (g, rowButton (1, 0), "On", processor.pcKeys.load());
        tile (g, rowButton (1, 1), "Off", ! processor.pcKeys.load());
        rowLabel (g, "MIDI learn", { 40.0f, 340.0f, 130.0f, 34.0f });
        int learnedCount = 0;
        for (auto& c : processor.ccMap) if (c.load() >= 0) ++learnedCount;
        g.setFont (LookAndFeel::uiFont (12.0f));
        g.setColour (page::ink().withAlpha (0.7f));
        g.drawText (processor.learnParam.load() >= 0 ? juce::String ("Move a knob on your controller")
                    : learnedCount == 0 ? juce::String ("Right-click a knob > MIDI Learn") : juce::String (learnedCount) + " controls learned",
                    180, 340, 360, 34, juce::Justification::centredLeft, false);
        tile (g, { 550.0f, 340.0f, 128.0f, 34.0f }, "Clear", false);

        tile (g, { 806.0f, 302.0f, 450.0f, 40.0f }, "Back to play", true);
    }

    void SettingsView::mouseDown (const juce::MouseEvent& e)
    {
        const auto p = e.position;
        bool changed = false;
        for (int i = 0; i < (int) page::themes().size(); ++i) if (swatch (i).contains (p)) { processor.skin.store (firstTheme + i); changed = true; }
        for (int i = 0; i < 6; ++i) if (zoomButton (i).contains (p) && onZoom) onZoom (zooms[i] / 100.0);
        if (juce::Rectangle<float> (830.0f, 152.0f, 398.0f, 28.0f).contains (p) && onFit) onFit();
        if (juce::Rectangle<float> (960.0f, 240.0f, 40.0f, 34.0f).contains (p)) { processor.tuningA4.store (juce::jmax (400.0f, processor.tuningA4.load() - 0.5f)); changed = true; }
        if (juce::Rectangle<float> (1122.0f, 240.0f, 40.0f, 34.0f).contains (p)) { processor.tuningA4.store (juce::jmin (480.0f, processor.tuningA4.load() + 0.5f)); changed = true; }
        if (juce::Rectangle<float> (1168.0f, 240.0f, 66.0f, 34.0f).contains (p)) { processor.tuningA4.store (440.0f); changed = true; }
        for (int i = 0; i < 4; ++i) if (rowButton (0, i).contains (p)) { processor.maxVoices.store (voiceOptions[i]); changed = true; }
        if (rowButton (1, 0).contains (p)) { processor.pcKeys.store (true); changed = true; if (onPcKeys) onPcKeys(); }
        if (rowButton (1, 1).contains (p)) { processor.pcKeys.store (false); changed = true; if (onPcKeys) onPcKeys(); }
        if (juce::Rectangle<float> (550.0f, 340.0f, 128.0f, 34.0f).contains (p)) for (auto& c : processor.ccMap) c.store (-1);
        if (juce::Rectangle<float> (806.0f, 302.0f, 450.0f, 40.0f).contains (p) && onClose) onClose();
        if (changed) processor.saveUserDefaults();
        repaint();
    }

    //==========================================================================
    AboutView::AboutView()
    {
        logo = juce::Drawable::createFromImageData (KuprellBrand::kuprelllogocream_svg, KuprellBrand::kuprelllogocream_svgSize);
    }

    void AboutView::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colours::black.withAlpha (0.7f));
        auto card = getLocalBounds().toFloat().withSizeKeepingCentre (520.0f, 360.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (card.translated (0.0f, 10.0f), 18.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2d32), 0.0f, card.getY(), juce::Colour (0xff141517), 0.0f, card.getBottom(), false));
        g.fillRoundedRectangle (card, 18.0f);
        g.setColour (juce::Colour (0xff3a3d41));
        g.drawRoundedRectangle (card, 18.0f, 1.0f);

        auto in = card.reduced (40.0f, 36.0f);
        if (logo != nullptr)
            logo->drawWithin (g, in.removeFromTop (40.0f), juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yTop, 1.0f);
        in.removeFromTop (18.0f);
        g.setColour (juce::Colour (0xffe6e7e9));
        g.setFont (LookAndFeel::uiFont (26.0f, true));
        g.drawText ("Hamamatsu 144", in.removeFromTop (34.0f), juce::Justification::centredLeft, false);
        g.setFont (spaced (11.0f, true, 0.3f));
        g.setColour (th().soft);
        g.drawText ("ELECTRONIC STRUM HARP", in.removeFromTop (20.0f), juce::Justification::centredLeft, false);
        in.removeFromTop (16.0f);
        g.setFont (LookAndFeel::uiFont (13.0f));
        g.setColour (juce::Colour (0xffb9bec4));
        g.drawFittedText ("144 chords, a living strum hand and six eras of sound. Inspired by the electronic strum harps of the 1980s, "
                          "made from pure synthesis.", in.removeFromTop (60.0f).toNearestInt(), juce::Justification::topLeft, 4);
        in.removeFromTop (10.0f);
        g.setFont (LookAndFeel::monoFont (12.0f));
        g.setColour (juce::Colour (0xff7e8187));
        g.drawText ("Version " + juce::String (JucePlugin_VersionString) + juce::String::fromUTF8 ("  \xc2\xb7  VST3 / Standalone"), in.removeFromTop (20.0f), juce::Justification::centredLeft, false);
        g.drawText (juce::String::fromUTF8 ("\xc2\xa9 2026 Kuprell  \xc2\xb7  MTS-ESP by ODDSound"), in.removeFromTop (20.0f), juce::Justification::centredLeft, false);
        g.setFont (LookAndFeel::uiFont (11.0f));
        g.drawText ("Click anywhere to close", card.withTop (card.getBottom() - 30.0f), juce::Justification::centred, false);
    }

    //==========================================================================
    void HelpView::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));
        struct Bubble { float x, y; const char* title; const char* text; };
        // positions in PLAY page coordinates (instrument area starts at y = 52)
        static const Bubble bubbles[] = {
            { 140, 215, "Chord buttons", "Click = latch a chord, right-click = more types, Ctrl+click = combine. A dot = fits the key." },
            { 845, 330, "Strum plate", "Try the strings with the mouse. To really play, use the keys (see below)." },
            { 1030, 208, "Voice", "The lit screen: sound and what the strum does now. VOICE, MORPH, RATE, FEEL, SPEED." },
            { 1000, 360, "MIDI capture", "Records what you play. Drag it into the piano roll. Menu: Detail / Smooth / Reset." },
            { 140, 380, "Arp / gate", "ARP plays the chord note by note, GATE chops it. Drag the bars = step strength." },
            { 140, 318, "Progression", "Click a slot, then chord buttons. PLAY = with the song, STRUM / ARP / MELODY, DRAG 4 / 8 bars into the piano roll." },
            { 1100, 112, "EDIT", "Sound, arp & gate, filter, modulation and FX." },
            { 140, 560, "Sound deck", "Harp, envelope (ADSR), chord layer, bass, FX, macros and master volume." },
            { 520, 112, "Presets", "Click the name for the browser, arrows for next / previous, Save for your own bank." },
            { 140, 560, "Filter", "Cutoff, resonance and saturation on the whole instrument." } };
        int n = 1;
        for (auto& b : bubbles)
        {
            auto c = juce::Rectangle<float> (26.0f, 26.0f).withCentre ({ b.x, b.y });
            g.setColour (th().accent.withAlpha (0.35f));
            g.fillEllipse (c.expanded (5.0f));
            g.setColour (th().accent);
            g.fillEllipse (c);
            g.setColour (juce::Colour (0xff06201d));
            g.setFont (LookAndFeel::uiFont (13.0f, true));
            g.drawText (juce::String (n), c, juce::Justification::centred, false);
            ++n;
        }
        // legend
        auto legend = juce::Rectangle<float> (172.0f, 150.0f, 646.0f, 500.0f);
        g.setColour (juce::Colour (0xf0141517));
        g.fillRoundedRectangle (legend, 14.0f);
        g.setColour (th().accent.withAlpha (0.6f));
        g.drawRoundedRectangle (legend, 14.0f, 1.0f);
        auto in = legend.reduced (22.0f, 16.0f);
        g.setFont (spaced (10.0f, true, 0.3f));
        g.setColour (th().soft);
        g.drawText (juce::String::fromUTF8 ("HELP  \xc2\xb7  CLICK ANYWHERE TO CLOSE"), in.removeFromTop (22.0f), juce::Justification::centredLeft, false);
        n = 1;
        for (auto& b : bubbles)
        {
            auto row = in.removeFromTop (26.0f);
            g.setFont (LookAndFeel::uiFont (12.5f, true));
            g.setColour (th().accent);
            g.drawText (juce::String (n++) + "  " + b.title, row.removeFromLeft (150.0f), juce::Justification::centredLeft, false);
            g.setFont (LookAndFeel::uiFont (12.0f));
            g.setColour (juce::Colour (0xffc9ccd1));
            g.drawFittedText (b.text, row.toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
        }
        // playing with keys (the real way to play; the mouse is for trying)
        in.removeFromTop (8.0f);
        g.setFont (spaced (10.0f, true, 0.3f));
        g.setColour (th().soft);
        g.drawText ("PLAY WITH KEYS", in.removeFromTop (20.0f), juce::Justification::centredLeft, false);
        static const char* keys[][2] = {
            { "MIDI keyboard", "YOUR KEYS = Split: left hand (below C5, middle C) picks the chord - one key = the chord of the key on that degree." },
            { "", "Right hand from C5: white keys = single strings (low to high), black keys = whole strum (C#, F# down, D# G# A# up)." },
            { "Wheels", "Pitch wheel or mod wheel = a finger across the strings. Harder key = louder strum." },
            { "PC keyboard", "Q-row major, A-row minor, Z-row 7th (Shift = add), number row = strings, - / = = strum down / up." } };
        for (auto& k : keys)
        {
            auto row = in.removeFromTop (30.0f);
            g.setFont (LookAndFeel::uiFont (12.5f, true));
            g.setColour (th().accent);
            g.drawText (k[0], row.removeFromLeft (150.0f), juce::Justification::centredLeft, false);
            g.setFont (LookAndFeel::uiFont (12.0f));
            g.setColour (juce::Colour (0xffc9ccd1));
            g.drawFittedText (k[1], row.toNearestInt(), juce::Justification::centredLeft, 2, 0.85f);
        }
        g.setFont (LookAndFeel::uiFont (11.5f));
        g.setColour (juce::Colour (0xff9aa0a7));
        g.drawText ("Tip: hover any control for a tooltip, right-click a knob for reset, typing a value, MIDI Learn and lock.", in, juce::Justification::centredLeft, false);
    }
}
