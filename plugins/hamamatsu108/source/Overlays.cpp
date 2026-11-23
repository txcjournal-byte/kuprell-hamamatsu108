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

    // HARPOLOGY settings: everything that sets the instrument up, as rows of flat tiles in three columns.
    juce::Rectangle<float> SettingsView::swatch (int) const { return {}; }
    juce::Rectangle<float> SettingsView::zoomButton (int i) const { return { 0.0f, 0.0f, (float) i, 0.0f }; }
    juce::Rectangle<float> SettingsView::rowButton (int, int) const { return {}; }

    namespace
    {
        void box (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
        {
            if (th().concrete) { cs::plate (g, r, title); return; }
            drawWell (g, r, 0.0f);
            g.setFont (LookAndFeel::uiFont (13.0f));
            g.setColour (page::ink());
            g.drawText (title, r.reduced (16.0f, 0.0f).withHeight (34.0f), juce::Justification::centredLeft, false);
            g.setColour (juce::Colour (0xff2e2d2a));
            g.fillRect (r.getX() + 16.0f, r.getY() + 34.0f, r.getWidth() - 32.0f, 1.0f);
        }
        void tile (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on)
        {
            if (th().concrete) { cs::key (g, r, t, on, true, 10.5f); return; }
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRect (r.translated (1.5f, 2.0f));
            g.setColour (on ? page::lit() : juce::Colour (0xff4b4a46));
            g.fillRect (r);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.5f : 0.12f));
            g.fillRect (r.withHeight (1.0f));
            g.setFont (LookAndFeel::uiFont (12.0f));
            g.setColour (on ? page::litInk() : page::ink());
            g.drawText (t, r, juce::Justification::centred, false);
        }

        struct Row
        {
            juce::String label, note;
            juce::StringArray options;
            std::function<int()> get;
            std::function<void (int)> set;
        };
    }

    // one place that lays the page out; paint and clicks both walk it
    void SettingsView::walk (const std::function<void (const juce::String& kind, juce::Rectangle<float>, const juce::String&, bool, std::function<void()>)>& visit)
    {
        auto& p = processor;
        auto choiceOf = [&p] (const char* id) { return dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id)); };
        auto paramRow = [&] (const juce::String& label, const char* id, juce::StringArray shortNames, const juce::String& note) -> Row
        {
            auto* c = choiceOf (id);
            juce::String pid (id);
            return { label, note, shortNames.isEmpty() && c != nullptr ? c->choices : shortNames,
                     [c] { return c != nullptr ? c->getIndex() : 0; },
                     [this, pid] (int i) { setParam (processor.apvts, pid, (float) i); } };
        };
        auto onOff = [] (std::atomic<bool>& a) { return juce::StringArray { "On", "Off" }; };
        auto flag = [] (const juce::String& label, std::atomic<bool>& a, const juce::String& note) -> Row
        {
            return { label, note, { "On", "Off" }, [&a] { return a.load() ? 0 : 1; }, [&a] (int i) { a.store (i == 0); } };
        };
        juce::ignoreUnused (onOff);

        std::vector<std::pair<juce::String, std::vector<Row>>> cols;
        // only what a producer sets once: how to record it, the strum keys, how many voices
        cols.push_back ({ "Recording and MIDI", {
            flag ("MIDI out", p.midiOut, "record what you play (mouse too) in the DAW"),
            paramRow ("Strum keys", "strumZone", {}, "a low octave of keys that strum"),
            { "Voices", {}, { "16", "32", "48", "64" }, [&p] { const int v = p.maxVoices.load(); return v <= 16 ? 0 : v <= 32 ? 1 : v <= 48 ? 2 : 3; }, [&p] (int i) { p.maxVoices.store (16 * (i + 1)); } },
            { "MIDI learn", "right-click a knob, then move a knob on your controller", { "Clear all" }, [] { return -1; }, [&p] (int) { for (auto& cc : p.ccMap) cc.store (-1); } },
        } });

        const float colW = 604.0f, x0 = 24.0f, y0 = 20.0f, h = 340.0f;
        for (size_t c = 0; c < cols.size(); ++c)
        {
            const float cx = x0 + (float) c * (colW + 24.0f);
            visit ("box", { cx, y0, colW, h }, cols[c].first, false, {});
            float y = y0 + 48.0f;
            for (auto& row : cols[c].second)
            {
                visit ("label", { cx + 16.0f, y, colW - 32.0f, 16.0f }, row.label, false, {});
                if (row.note.isNotEmpty()) visit ("note", { cx + 16.0f, y, colW - 32.0f, 16.0f }, row.note, false, {});
                y += 20.0f;
                float x = cx + 16.0f;
                const int cur = row.get();
                for (int i = 0; i < row.options.size(); ++i)
                {
                    const float w = juce::jmax (44.0f, 14.0f + 7.0f * (float) row.options[i].length());
                    if (x + w > cx + colW - 16.0f) { x = cx + 16.0f; y += 32.0f; }
                    auto set = row.set;
                    visit ("tile", { x, y, w, 26.0f }, row.options[i], i == cur, [set, i] { set (i); });
                    x += w + 6.0f;
                }
                y += 40.0f;
            }
        }

        // third column: tuning, window, MIDI learn, back
        const float cx = x0 + (float) cols.size() * (colW + 24.0f), w3 = 1232.0f - (float) cols.size() * (colW + 24.0f);
        visit ("box", { cx, y0, w3, h }, "Tuning and window", false, {});
        float y = y0 + 48.0f;
        visit ("label", { cx + 16.0f, y, 200.0f, 16.0f }, "A4", false, {});
        y += 20.0f;
        visit ("tile", { cx + 16.0f, y, 36.0f, 26.0f }, juce::String::fromUTF8 ("\xe2\x88\x92"), false, [&p] { p.tuningA4.store (juce::jmax (400.0f, p.tuningA4.load() - 0.5f)); });
        visit ("display", { cx + 58.0f, y, 110.0f, 26.0f }, juce::String (p.tuningA4.load(), 1) + " Hz", false, {});
        visit ("tile", { cx + 174.0f, y, 36.0f, 26.0f }, "+", false, [&p] { p.tuningA4.store (juce::jmin (480.0f, p.tuningA4.load() + 0.5f)); });
        visit ("tile", { cx + 216.0f, y, 56.0f, 26.0f }, "440", std::abs (p.tuningA4.load() - 440.0f) < 0.01f, [&p] { p.tuningA4.store (440.0f); });
        visit ("tile", { cx + 278.0f, y, 56.0f, 26.0f }, "432", std::abs (p.tuningA4.load() - 432.0f) < 0.01f, [&p] { p.tuningA4.store (432.0f); });
        y += 60.0f;
        visit ("label", { cx + 16.0f, y, 200.0f, 16.0f }, "Window size", false, {});
        y += 20.0f;
        const int z = currentZoom ? currentZoom() : 100;
        for (int i = 0; i < 6; ++i)
        {
            const int zz = zooms[i];
            visit ("tile", { cx + 16.0f + (float) (i % 3) * 78.0f, y + (float) (i / 3) * 32.0f, 72.0f, 26.0f }, juce::String (zz) + " %", std::abs (z - zz) <= 2,
                   [this, zz] { if (onZoom) onZoom (zz / 100.0); });
        }
        y += 64.0f;
        visit ("tile", { cx + 16.0f, y, 228.0f, 26.0f }, "Fit to screen", false, [this] { if (onFit) onFit(); });
        visit ("back", { cx + 16.0f, y0 + h - 56.0f, w3 - 32.0f, 36.0f }, "Back to play", true, [this] { if (onClose) onClose(); });
    }

    void SettingsView::paint (juce::Graphics& g)
    {
        cs::pageBackground (g, getLocalBounds().toFloat());
        walk ([&g] (const juce::String& kind, juce::Rectangle<float> r, const juce::String& t, bool on, std::function<void()>)
        {
            if (kind == "box") box (g, r, t);
            else if (kind == "label") { g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.08f)); g.setColour (cs::bone.withAlpha (0.85f)); g.drawText (t, r, juce::Justification::centredLeft, false); }
            else if (kind == "note") { g.setFont (LookAndFeel::uiFont (10.5f)); g.setColour (page::dim().brighter (0.25f)); g.drawText (t, r, juce::Justification::centredRight, false); }
            else if (kind == "display") { drawDisplay (g, r, 0.0f); g.setFont (LookAndFeel::monoFont (13.0f)); glowText (g, t, r, juce::Justification::centred); }
            else tile (g, r, t, on);
        });
    }

    void SettingsView::mouseDown (const juce::MouseEvent& e)
    {
        std::function<void()> hit;
        walk ([&] (const juce::String&, juce::Rectangle<float> r, const juce::String&, bool, std::function<void()> act) { if (act && r.contains (e.position)) hit = act; });
        if (hit)
        {
            const bool pcBefore = processor.pcKeys.load();
            hit();
            if (pcBefore != processor.pcKeys.load() && onPcKeys) onPcKeys();
            processor.saveUserDefaults();
        }
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
