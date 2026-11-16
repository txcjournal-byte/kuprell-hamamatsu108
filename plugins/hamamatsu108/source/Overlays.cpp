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

    juce::Rectangle<float> SettingsView::swatch (int i) const
    {
        return { 40.0f + (i % 5) * 150.0f, 60.0f + (i / 5) * 132.0f, 138.0f, 120.0f };
    }
    juce::Rectangle<float> SettingsView::zoomButton (int i) const { return { 830.0f + (i % 3) * 136.0f, 70.0f + (i / 3) * 44.0f, 126.0f, 36.0f }; }
    juce::Rectangle<float> SettingsView::rowButton (int row, int i) const
    {
        return { 180.0f + i * 92.0f, 404.0f + row * 48.0f, 86.0f, 34.0f };
    }

    void SettingsView::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff0b0c0d));

        // skins
        panelBox (g, { 24.0f, 20.0f, 770.0f, 330.0f }, juce::String::fromUTF8 ("SKIN  \xc2\xb7  THEMED EDITIONS"));
        const int cur = processor.skin.load();
        for (int i = 0; i < 10; ++i)
        {
            const auto& sk = skins::get (i);
            auto r = swatch (i);
            if (i == cur) { g.setColour (sk.accent.main.withAlpha (0.35f)); g.fillRoundedRectangle (r.expanded (3.0f), 12.0f); }
            auto body = r.withTrimmedBottom (26.0f).reduced (6.0f);
            // mini instrument: cheeks, panel, deck, light
            auto cheekL = body.removeFromLeft (12.0f), cheekR = body.removeFromRight (12.0f);
            for (auto c : { cheekL, cheekR })
            {
                g.setColour (sk.cheek == skins::Cheek::walnut ? juce::Colour (0xff6a3f22) : sk.cheekA);
                g.fillRect (c);
            }
            g.setGradientFill (juce::ColourGradient (sk.panelTop, 0.0f, body.getY(), sk.panelBottom, 0.0f, body.getBottom(), false));
            g.fillRect (body);
            skins::paintPattern (g, body, sk.pattern);
            auto deck = body.removeFromBottom (18.0f).reduced (4.0f, 2.0f);
            g.setGradientFill (juce::ColourGradient (sk.deckTop, 0.0f, deck.getY(), sk.deckBottom, 0.0f, deck.getBottom(), false));
            g.fillRoundedRectangle (deck, 3.0f);
            g.setColour (sk.accent.main);
            g.fillEllipse (body.getRight() - 14.0f, body.getY() + 6.0f, 7.0f, 7.0f);
            auto well = body.reduced (8.0f, 14.0f).withTrimmedRight (20.0f);
            g.setColour (juce::Colour (0xff0d0f11));
            g.fillRoundedRectangle (well, 3.0f);
            g.setColour (i == cur ? juce::Colours::white : juce::Colour (0xffc9ccd1));
            g.setFont (LookAndFeel::uiFont (12.0f, i == cur));
            g.drawText (sk.name, r.withTop (r.getBottom() - 24.0f), juce::Justification::centred, false);
        }

        // window size
        panelBox (g, { 806.0f, 20.0f, 450.0f, 180.0f }, "WINDOW SIZE");
        const int z = currentZoom ? currentZoom() : 100;
        for (int i = 0; i < 6; ++i) choiceButton (g, zoomButton (i), juce::String (zooms[i]) + " %", std::abs (z - zooms[i]) <= 2);
        choiceButton (g, { 830.0f, 158.0f, 398.0f, 32.0f }, "Fit to screen", false);

        // about the plugin version / tuning
        panelBox (g, { 806.0f, 212.0f, 450.0f, 138.0f }, "TUNING");
        label (g, "A4 REFERENCE", { 830.0f, 250.0f, 120.0f, 34.0f });
        choiceButton (g, { 960.0f, 250.0f, 40.0f, 34.0f }, juce::String::fromUTF8 ("\xe2\x88\x92"), false);
        auto tv = juce::Rectangle<float> (1006.0f, 250.0f, 110.0f, 34.0f);
        drawDisplay (g, tv, 5.0f);
        g.setFont (LookAndFeel::monoFont (14.0f));
        glowText (g, juce::String (processor.tuningA4.load(), 1) + " Hz", tv, juce::Justification::centred);
        choiceButton (g, { 1122.0f, 250.0f, 40.0f, 34.0f }, "+", false);
        choiceButton (g, { 1168.0f, 250.0f, 66.0f, 34.0f }, "440", false);
        label (g, "MTS-ESP", { 830.0f, 298.0f, 120.0f, 34.0f });
        choiceButton (g, { 960.0f, 298.0f, 80.0f, 34.0f }, processor.mtsEnabled.load() ? "On" : "Off", processor.mtsEnabled.load());
        g.setFont (LookAndFeel::uiFont (11.5f));
        g.setColour (juce::Colour (0xff9aa0a7));
        g.drawText (processor.mtsConnected() ? "Connected to a microtuning master" : "No MTS-ESP master running", 1050, 298, 200, 34, juce::Justification::centredLeft, false);

        // MIDI
        panelBox (g, { 24.0f, 362.0f, 770.0f, 300.0f }, "MIDI AND VOICES");
        label (g, "CHORD ZONES", { 40.0f, 404.0f, 130.0f, 34.0f });
        choiceButton (g, rowButton (0, 0), "On", processor.chordZones.load());
        choiceButton (g, rowButton (0, 1), "Off", ! processor.chordZones.load());
        g.setFont (LookAndFeel::uiFont (11.5f));
        g.setColour (juce::Colour (0xff9aa0a7));
        g.drawText ("Off: every key plays the harp", 380, 404, 300, 34, juce::Justification::centredLeft, false);

        label (g, "ZONES START AT", { 40.0f, 452.0f, 130.0f, 34.0f });
        for (int i = 0; i < 4; ++i)
            choiceButton (g, rowButton (1, i), "C" + juce::String (i + 1) + " (FL)", processor.zoneOctave.load() == i);

        label (g, "VOICES", { 40.0f, 500.0f, 130.0f, 34.0f });
        for (int i = 0; i < 4; ++i)
            choiceButton (g, rowButton (2, i), juce::String (voiceOptions[i]), processor.maxVoices.load() == voiceOptions[i]);

        label (g, "MIDI LEARN", { 40.0f, 552.0f, 130.0f, 34.0f });
        juce::StringArray learned;
        const auto& params = processor.getParameters();
        for (int cc = 0; cc < 128; ++cc)
        {
            const int idx = processor.ccMap[(size_t) cc].load();
            if (idx >= 0 && idx < params.size()) learned.add ("CC " + juce::String (cc) + juce::String::fromUTF8 (" \xe2\x86\x92 ") + params[idx]->getName (32));
        }
        g.setFont (LookAndFeel::uiFont (12.0f));
        g.setColour (learned.isEmpty() ? juce::Colour (0xff6e737a) : th().soft);
        g.drawFittedText (learned.isEmpty() ? juce::String ("Nothing learned yet. Right-click any knob and choose MIDI Learn, then move a knob on your controller.")
                                            : learned.joinIntoString ("    "),
                          juce::Rectangle<int> (180, 552, 460, 60), juce::Justification::topLeft, 4);
        choiceButton (g, { 650.0f, 552.0f, 128.0f, 34.0f }, "Clear all", false);
        if (processor.learnParam.load() >= 0)
        {
            g.setColour (th().accent);
            g.drawText ("Waiting for a controller...", 180, 620, 400, 24, juce::Justification::centredLeft, false);
        }

        // back
        panelBox (g, { 806.0f, 362.0f, 450.0f, 300.0f }, "ABOUT THESE SETTINGS");
        g.setFont (LookAndFeel::uiFont (12.0f));
        g.setColour (juce::Colour (0xff9aa0a7));
        g.drawFittedText ("Skin, window size, zones, tuning and voices are saved with your project and become the defaults for new instances. "
                          "Presets never change them.", juce::Rectangle<int> (830, 400, 400, 100), juce::Justification::topLeft, 6);
        drawHardButton (g, { 830.0f, 600.0f, 398.0f, 40.0f }, "BACK TO PLAY", true, true, false);
    }

    void SettingsView::mouseDown (const juce::MouseEvent& e)
    {
        const auto p = e.position;
        bool changed = false;
        for (int i = 0; i < 10; ++i) if (swatch (i).contains (p)) { processor.skin.store (i); changed = true; }
        for (int i = 0; i < 6; ++i) if (zoomButton (i).contains (p) && onZoom) onZoom (zooms[i] / 100.0);
        if (juce::Rectangle<float> (830.0f, 158.0f, 398.0f, 32.0f).contains (p) && onFit) onFit();
        if (juce::Rectangle<float> (960.0f, 250.0f, 40.0f, 34.0f).contains (p)) { processor.tuningA4.store (juce::jmax (400.0f, processor.tuningA4.load() - 0.5f)); changed = true; }
        if (juce::Rectangle<float> (1122.0f, 250.0f, 40.0f, 34.0f).contains (p)) { processor.tuningA4.store (juce::jmin (480.0f, processor.tuningA4.load() + 0.5f)); changed = true; }
        if (juce::Rectangle<float> (1168.0f, 250.0f, 66.0f, 34.0f).contains (p)) { processor.tuningA4.store (440.0f); changed = true; }
        if (juce::Rectangle<float> (960.0f, 298.0f, 80.0f, 34.0f).contains (p)) { processor.mtsEnabled.store (! processor.mtsEnabled.load()); changed = true; }
        if (rowButton (0, 0).contains (p)) { processor.chordZones.store (true); changed = true; }
        if (rowButton (0, 1).contains (p)) { processor.chordZones.store (false); changed = true; }
        for (int i = 0; i < 4; ++i)
        {
            if (rowButton (1, i).contains (p)) { processor.zoneOctave.store (i); changed = true; }
            if (rowButton (2, i).contains (p)) { processor.maxVoices.store (voiceOptions[i]); changed = true; }
        }
        if (juce::Rectangle<float> (650.0f, 552.0f, 128.0f, 34.0f).contains (p)) for (auto& c : processor.ccMap) c.store (-1);
        if (juce::Rectangle<float> (830.0f, 600.0f, 398.0f, 40.0f).contains (p) && onClose) onClose();
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
        g.drawText ("Hamamatsu 108", in.removeFromTop (34.0f), juce::Justification::centredLeft, false);
        g.setFont (spaced (11.0f, true, 0.3f));
        g.setColour (th().soft);
        g.drawText ("ELECTRONIC STRUM HARP", in.removeFromTop (20.0f), juce::Justification::centredLeft, false);
        in.removeFromTop (16.0f);
        g.setFont (LookAndFeel::uiFont (13.0f));
        g.setColour (juce::Colour (0xffb9bec4));
        g.drawFittedText ("108 chords, a living strum hand and six eras of sound. Inspired by the electronic strum harps of the 1980s, "
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
            { 150, 200, "Chord buttons", "108 chords: rows are types, columns are roots. Click one to play it. A dot = fits the key." },
            { 845, 200, "Strum plate", "Drag the mouse across the strings. It lights up whenever a string sounds." },
            { 1040, 180, "Voice", "VOICE picks one of 10 harp sounds, MORPH blends layer A and B when DUAL is on." },
            { 1040, 360, "Strum mode", "LIVE: the hand strums in time. ARP: arpeggio. MEMO: plays the memory. OFF: only you." },
            { 200, 612, "Sound deck", "Harp, chord layer, bass, vibrato, FX and the AGE / DREAM / WOBBLE macros." },
            { 400, 708, "Chord memory", "8 banks of 16 chords. REC, click chords, then MEMO plays them. DRAG MIDI exports them." },
            { 740, 130, "MIDI capture", "Records what the plugin plays. Drag it into FL Studio to get a MIDI clip." },
            { 1000, 130, "Model", "Six eras of the instrument: click to switch from Early 81 to Modern Strum." },
            { 560, 26, "Presets", "144 factory sounds. Click the name for the browser, arrows for next / previous." },
            { 1225, 26, "EDIT", "Deep editing: engine, strum, chords, memory, modulation, FX and character." } };
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
        auto legend = juce::Rectangle<float> (170.0f, 222.0f, 640.0f, 360.0f);
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
            auto row = in.removeFromTop (28.0f);
            g.setFont (LookAndFeel::uiFont (12.5f, true));
            g.setColour (th().accent);
            g.drawText (juce::String (n++) + "  " + b.title, row.removeFromLeft (150.0f), juce::Justification::centredLeft, false);
            g.setFont (LookAndFeel::uiFont (12.0f));
            g.setColour (juce::Colour (0xffc9ccd1));
            g.drawFittedText (b.text, row.toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
        }
        g.setFont (LookAndFeel::uiFont (11.5f));
        g.setColour (juce::Colour (0xff9aa0a7));
        g.drawText ("Tip: hover any control for a tooltip, right-click a knob for reset, typing a value, MIDI Learn and lock.", in, juce::Justification::centredLeft, false);
    }
}
