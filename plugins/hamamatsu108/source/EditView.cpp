#include "EditView.h"
#include "KuprellBrandAssets.h"

using namespace kuprell;
using namespace hama::look;

namespace hama
{
    //==========================================================================
    // Base for one tab: panels (titled wells) and parameter controls.
    class EditPage : public juce::Component
    {
    public:
        EditPage (Hamamatsu108Processor& p, EditView& o) : processor (p), owner (o) {}
        virtual void tick() {}

        void paint (juce::Graphics& g) override
        {
            for (auto& pn : panels)
            {
                drawWell (g, pn.area.toFloat(), 14.0f);
                g.setFont (spaced (9.0f, true, 0.28f));
                g.setColour (engrave);
                g.drawText (pn.title, pn.area.toFloat().reduced (16.0f, 0.0f).withHeight (30.0f), juce::Justification::centredLeft, false);
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.drawHorizontalLine (pn.area.getY() + 30, (float) pn.area.getX() + 14.0f, (float) pn.area.getRight() - 14.0f);
            }
            paintExtras (g);
        }
        virtual void paintExtras (juce::Graphics&) {}

    protected:
        struct Panel { juce::String title; juce::Rectangle<int> area; };
        juce::Rectangle<int> panel (const char* title, int x, int y, int w, int h) { return panel (juce::String::fromUTF8 (title), x, y, w, h); }
        juce::Rectangle<int> panel (const juce::String& title, int x, int y, int w, int h)
        {
            panels.push_back ({ title, { x, y, w, h } });
            return { x + 16, y + 40, w - 32, h - 52 };
        }

        Knob* knob (const juce::String& id, const juce::String& label, int size, juce::Rectangle<int> bounds)
        {
            Knob::Style st;
            st.labelHeight = 9.0f;
            auto* k = knobs.add (new Knob (processor.apvts, id, label, size, st));
            k->onInteract = [this] (const Knob& kn) { owner.status (kn.getDisplayName(), kn.getValueText()); };
            addAndMakeVisible (k);
            k->setBounds (bounds);
            return k;
        }

        // knob centred in a column at (x, y), width w
        Knob* knobAt (const juce::String& id, const juce::String& label, int size, int x, int y, int w = 0)
        {
            const int cw = juce::jmax (w, size + 18);
            return knob (id, label, size, { x, y, cw, size + 14 + 28 });
        }

        ChoiceBox* choice (const juce::String& id, const juce::String& label, juce::Rectangle<int> bounds)
        {
            auto* c = choices.add (new ChoiceBox (processor.apvts, id, label));
            c->onInteract = [this] (const juce::String& n, const juce::String& v) { owner.status (n, v); };
            addAndMakeVisible (c);
            c->setBounds (bounds);
            return c;
        }

        ToggleKey* toggle (const juce::String& id, const juce::String& text, juce::Rectangle<int> bounds)
        {
            auto* t = toggles.add (new ToggleKey (processor.apvts, id, text));
            addAndMakeVisible (t);
            t->setBounds (bounds);
            return t;
        }

        void text (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, float size = 11.5f, juce::Colour c = juce::Colour (0xff9aa0a7))
        {
            g.setFont (LookAndFeel::uiFont (size));
            g.setColour (c);
            g.drawFittedText (t, r, juce::Justification::topLeft, 6, 1.0f);
        }

        Hamamatsu108Processor& processor;
        EditView& owner;
        std::vector<Panel> panels;
        juce::OwnedArray<Knob> knobs;
        juce::OwnedArray<ChoiceBox> choices;
        juce::OwnedArray<ToggleKey> toggles;
    };

    //==========================================================================
    class EnginePage : public EditPage
    {
    public:
        EnginePage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            auto a = panel ("LAYER A  \xc2\xb7  HARP", 0, 0, 400, 250);
            choice ("harpVoice", "VOICE", a.removeFromTop (44).withWidth (200));
            knobAt ("harpTone", "Tone", 44, a.getX(), a.getY() + 20, 100);
            knobAt ("harpDecay", "Decay", 44, a.getX() + 120, a.getY() + 20, 100);
            knobAt ("harpRange", "Range", 44, a.getX() + 240, a.getY() + 20, 100);

            auto b = panel (juce::String::fromUTF8 ("LAYER B  \xc2\xb7  DUAL"), 412, 0, 400, 250);
            toggle ("dual", "DUAL", { b.getX(), b.getY() + 14, 110, 30 });
            choice ("harpVoice2", "VOICE B", { b.getX() + 130, b.getY(), 200, 44 });
            knobAt ("morph", "MORPH  A / B", 60, b.getX() + 120, b.getY() + 60, 130);

            auto m = panel ("MODEL", 824, 0, 408, 250);
            modelBox = choice ("model", "MODEL", m.removeFromTop (44).withWidth (220));
            modelBox->onChange = [this] { repaint(); };
            modelText = { m.getX(), m.getY() + 10, m.getWidth(), 140 };

            auto c = panel ("CHORD LAYER", 0, 262, 300, 268);
            choice ("chordType", "SOUND", { c.getX(), c.getY(), 124, 44 });
            choice ("voicing", "VOICING", { c.getX() + 136, c.getY(), 132, 44 });
            knobAt ("chordLevel", "Level", 44, c.getX() + 10, c.getY() + 64, 110);
            knobAt ("chordFilter", "Filter", 44, c.getX() + 140, c.getY() + 64, 110);

            auto bs = panel ("BASS", 312, 262, 300, 268);
            choice ("bassType", "SOUND", { bs.getX(), bs.getY(), 124, 44 });
            choice ("bassMode", "PATTERN", { bs.getX() + 136, bs.getY(), 132, 44 });
            knobAt ("bassLevel", "Level", 44, bs.getX() + 10, bs.getY() + 64, 110);
            knobAt ("bassTone", "Tone", 44, bs.getX() + 140, bs.getY() + 64, 110);

            auto v = panel ("VIBRATO", 624, 262, 180, 268);
            knobAt ("vibRate", "Rate", 40, v.getX() + 30, v.getY(), 90);
            knobAt ("vibDepth", "Depth", 40, v.getX() + 30, v.getY() + 100, 90);

            auto mt = panel ("MORPH TARGETS", 816, 262, 416, 268);
            for (int i = 0; i < mod::morphSlots; ++i)
            {
                const juce::String n (i + 1);
                const int y = mt.getY() + i * 27;
                choice ("morph" + n + "Dst", {}, { mt.getX() + 22, y, 230, 23 });
                Knob::Style st; st.showValue = false; st.labelHeight = 1.0f;
                auto* k = knobs.add (new Knob (processor.apvts, "morph" + n + "Amt", {}, 12, st));
                k->onInteract = [this] (const Knob& kn) { owner.status (kn.getDisplayName(), kn.getValueText()); };
                addAndMakeVisible (k);
                k->setBounds (mt.getX() + 262, y - 2, 30, 28);
                amountRows.push_back ({ "morph" + n + "Amt", mt.getX() + 300, y });
            }
        }

        void tick() override { repaint (amountArea()); }
        juce::Rectangle<int> amountArea() const { return { 816 + 316, 262 + 40, 100, 220 }; }

        void paintExtras (juce::Graphics& g) override
        {
            // model description and character bars
            const int m = juce::jmax (0, modelBox->getIndex());
            static const char* desc[] = {
                "The first generation: dark and warm, a narrow image, a little hiss and drift. Like the early 80s original.",
                "The classic chime everybody knows. Balanced, slightly lo-fi, a gentle stereo spread.",
                "Late 80s sound bank: brighter top end, wider stereo, cleaner converters.",
                "The clean 90s reissue: smooth, full range, almost no noise.",
                "Cartridge era grit: 8-bit converters, crunchy and charming.",
                "Today's take: hi-fi, extra wide, long and shiny decay." };
            text (g, desc[m], modelText.withHeight (48));
            const auto& sp = Hamamatsu108Processor::modelSpec (m);
            const float vals[] = { sp.cutoff / 20000.0f, juce::jmin (1.0f, sp.width / 1.2f), juce::jmin (1.0f, sp.hiss / 0.0009f), (16.0f - sp.bits) / 8.0f };
            const char* names[] = { "BRIGHTNESS", "WIDTH", "NOISE", "GRIT" };
            for (int i = 0; i < 4; ++i)
            {
                auto row = juce::Rectangle<int> (modelText.getX(), modelText.getY() + 60 + i * 20, modelText.getWidth(), 14);
                g.setFont (spaced (8.5f, true, 0.15f));
                g.setColour (juce::Colour (0xff9aa0a7));
                g.drawText (names[i], row.removeFromLeft (90), juce::Justification::centredLeft, false);
                auto bar = row.toFloat().withSizeKeepingCentre ((float) row.getWidth(), 5.0f);
                g.setColour (juce::Colour (0xff22252a));
                g.fillRoundedRectangle (bar, 2.5f);
                g.setColour (th().accent);
                g.fillRoundedRectangle (bar.withWidth (juce::jmax (3.0f, bar.getWidth() * vals[i])), 2.5f);
            }

            // morph amounts as text
            g.setFont (LookAndFeel::monoFont (11.0f));
            for (auto& r : amountRows)
            {
                auto* p = processor.apvts.getParameter (r.id);
                g.setColour (th().soft);
                g.drawText (p->getCurrentValueAsText(), r.x, r.y, 90, 23, juce::Justification::centredLeft, false);
            }
            g.setColour (juce::Colour (0xff6e737a));
            g.setFont (LookAndFeel::uiFont (10.5f));
            for (int i = 0; i < mod::morphSlots; ++i)
                g.drawText (juce::String (i + 1), 816 + 16, 262 + 40 + i * 27, 18, 23, juce::Justification::centredLeft, false);
        }

    private:
        struct AmountRow { juce::String id; int x, y; };
        std::vector<AmountRow> amountRows;
        ChoiceBox* modelBox = nullptr;
        juce::Rectangle<int> modelText;
    };

    //==========================================================================
    class StrumPage : public EditPage
    {
    public:
        StrumPage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            auto s = panel ("LIVE STRUM", 0, 0, 600, 250);
            choice ("strumMode", "MODE", { s.getX(), s.getY(), 140, 44 });
            toggle ("hold", "HOLD", { s.getX() + 156, s.getY() + 14, 100, 30 });
            knobAt ("strumRate", "Rate", 48, s.getX(), s.getY() + 64, 120);
            knobAt ("strumFeel", "Feel", 48, s.getX() + 140, s.getY() + 64, 120);
            knobAt ("strumSpeed", "Speed", 48, s.getX() + 280, s.getY() + 64, 120);
            helpS = { s.getX() + 410, s.getY() + 56, 150, 120 };

            preview = panel ("HAND PREVIEW", 612, 0, 620, 250);

            auto a = panel ("ARPEGGIO", 0, 262, 600, 268);
            choice ("arpPattern", "PATTERN", { a.getX(), a.getY(), 160, 44 });
            choice ("arpRegen", "REGEN", { a.getX() + 176, a.getY(), 130, 44 });
            knobAt ("strumRate", "Rate", 48, a.getX(), a.getY() + 64, 120);
            knobAt ("arpChance", "Chance", 48, a.getX() + 140, a.getY() + 64, 120);
            helpA = { a.getX() + 290, a.getY() + 66, 270, 140 };

            pattern = panel ("PATTERN", 612, 262, 620, 268);
        }

        void tick() override
        {
            juce::String sig;
            for (auto* id : { "strumFeel", "strumSpeed", "strumRate", "arpPattern", "arpChance", "arpRegen", "harpRange" })
                sig << processor.apvts.getRawParameterValue (id)->load() << ",";
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }

        void paintExtras (juce::Graphics& g) override
        {
            text (g, "Pick a chord and the hand plays it in time. FEEL loosens the hand (timing, force, skipped up-strokes), SPEED is how fast it sweeps the strings.", helpS);
            text (g, "ARP plays the chord tones one by one in the PATTERN. CHANCE drops random notes; REGEN rolls a new variation every 1, 2 or 4 bars (Off keeps one variation).", helpA);

            // hand preview: two beats of strokes, strings bottom (low) to top (high)
            auto r = preview.toFloat();
            const float feel = plain (processor.apvts, "strumFeel"), speed = plain (processor.apvts, "strumSpeed");
            const int rateIdx = juce::roundToInt (plain (processor.apvts, "strumRate"));
            static const double rq[] = { 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25 };
            const double stepQ = rq[juce::jlimit (0, 4, rateIdx)];
            const int strokes = juce::jlimit (1, 16, (int) std::round (2.0 / stepQ));
            const float beatW = r.getWidth() / 2.0f;
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            for (int b = 0; b <= 2; ++b) g.drawVerticalLine ((int) (r.getX() + b * beatW), r.getY(), r.getBottom());
            juce::Random rnd (42);
            const int n = 12;
            for (int s = 0; s < strokes; ++s)
            {
                const bool down = s % 2 == 0;
                if (! down && rnd.nextFloat() < 0.3f * feel) continue;
                const float x0 = r.getX() + (float) (s * stepQ) * beatW + (rnd.nextFloat() - 0.3f) * feel * 10.0f;
                const float vel = juce::jlimit (0.15f, 1.0f, (down ? 0.85f : 0.62f) + (rnd.nextFloat() - 0.5f) * 0.4f * feel);
                const int count = down ? juce::jmax (2, (int) (n * (0.62f + 0.38f * rnd.nextFloat()))) : juce::jmax (2, (int) (n * (0.35f + 0.35f * rnd.nextFloat())));
                float x = x0;
                for (int i = 0; i < count; ++i)
                {
                    const int str = down ? i : n - 1 - i;
                    const float y = r.getBottom() - 8.0f - (r.getHeight() - 16.0f) * (float) str / (float) (n - 1);
                    const float v = vel * (1.0f - 0.25f * (float) i / (float) count);
                    g.setColour (th().accent.withAlpha (0.25f + 0.75f * v));
                    g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
                    x += speed * 0.001f * beatW * 2.0f * (1.0f + (rnd.nextFloat() - 0.5f) * 0.5f * feel);
                }
            }

            // arp pattern: 16 steps over 4 chord tones x range
            auto pr = pattern.toFloat();
            const int pat = juce::roundToInt (plain (processor.apvts, "arpPattern"));
            const float chance = plain (processor.apvts, "arpChance");
            const int toneCount = 3 * juce::roundToInt (plain (processor.apvts, "harpRange")) + 1;
            const float colW = pr.getWidth() / 16.0f;
            for (int c = 0; c < 16; ++c)
            {
                int idx = 0;
                const int nT = toneCount, span = juce::jmax (1, 2 * nT - 2);
                switch (pat)
                {
                    case 0: idx = c % nT; break;
                    case 1: idx = nT - 1 - c % nT; break;
                    case 2: { const int p2 = c % span; idx = p2 < nT ? p2 : span - p2; break; }
                    case 3: idx = c % 2 == 0 ? 0 : 1 + (c / 2) % juce::jmax (1, nT - 1); break;
                    case 4: { const int p2 = c % (2 * nT); idx = p2 < nT ? p2 : 2 * nT - 1 - p2; break; }
                    default: idx = rnd.nextInt (nT); break;
                }
                const bool plays = c == 0 || rnd.nextFloat() <= chance;
                const float h = (pr.getHeight() - 20.0f) * (0.15f + 0.85f * (float) idx / (float) juce::jmax (1, nT - 1));
                auto bar = juce::Rectangle<float> (pr.getX() + c * colW + 3.0f, pr.getBottom() - 10.0f - h, colW - 6.0f, h);
                g.setColour (plays ? th().accent.withAlpha (0.85f) : juce::Colour (0xff2a2d32));
                g.fillRoundedRectangle (bar, 3.0f);
            }
        }

    private:
        juce::Rectangle<int> preview, pattern, helpS, helpA;
        juce::String lastSig;
    };

    //==========================================================================
    class ChordsPage : public EditPage
    {
    public:
        ChordsPage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            auto k = panel ("KEY", 0, 0, 300, 250);
            choice ("keyRoot", "KEY", { k.getX(), k.getY(), 120, 44 });
            choice ("keyMode", "SCALE", { k.getX() + 136, k.getY(), 132, 44 });
            choice ("transpose", "TRANSPOSE", { k.getX(), k.getY() + 64, 120, 44 });
            helpK = { k.getX(), k.getY() + 124, k.getWidth(), 70 };

            auto v = panel ("VOICING", 312, 0, 300, 250);
            choice ("voicing", "CHORD LAYER VOICING", { v.getX(), v.getY(), 268, 44 });
            voiceArea = { v.getX(), v.getY() + 60, v.getWidth(), 130 };

            inKey = panel ("CHORDS IN KEY  \xc2\xb7  CLICK TO PLAY", 624, 0, 608, 250);
            keyboard = panel ("KEYBOARD  \xc2\xb7  ZONES AND SOUNDING NOTES", 0, 262, 1232, 268);
        }

        juce::Rectangle<float> diatonicButton (int i) const
        {
            const float w = (inKey.getWidth() - 6 * 8.0f) / 7.0f;
            return { inKey.getX() + i * (w + 8.0f), (float) inKey.getY() + 20.0f, w, 90.0f };
        }

        int diatonicChord (int i) const
        {
            const int key = juce::roundToInt (plain (processor.apvts, "keyRoot"));
            const bool minor = plain (processor.apvts, "keyMode") > 0.5f;
            static const int majSteps[] = { 0, 2, 4, 5, 7, 9, 11 }, minSteps[] = { 0, 2, 3, 5, 7, 8, 10 };
            static const int majTypes[] = { 0, 1, 1, 0, 0, 1, 5 }, minTypes[] = { 1, 5, 0, 1, 1, 0, 0 };
            const int root = (key + (minor ? minSteps[i] : majSteps[i])) % 12;
            return chords::index (root, minor ? minTypes[i] : majTypes[i]);
        }

        void tick() override
        {
            juce::String sig;
            sig << processor.activeChord.load() << "," << plain (processor.apvts, "keyRoot") << "," << plain (processor.apvts, "keyMode") << ","
                << plain (processor.apvts, "voicing") << "," << plain (processor.apvts, "transpose") << "," << plain (processor.apvts, "harpRange");
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < 7; ++i)
                if (diatonicButton (i).contains (e.position)) { pressed = diatonicChord (i); processor.clickChord (pressed); return; }
        }
        void mouseUp (const juce::MouseEvent&) override { if (pressed >= 0) processor.releaseChord (pressed); pressed = -1; }

        void paintExtras (juce::Graphics& g) override
        {
            text (g, "Chords that fit the key get a turquoise dot on the chord buttons. TRANSPOSE shifts everything the harp plays.", helpK);

            const int active = processor.activeChord.load();
            const int tr = juce::roundToInt (plain (processor.apvts, "transpose"));
            const int inv = juce::roundToInt (plain (processor.apvts, "voicing"));
            std::vector<int> voiced;
            if (active >= 0) voiced = chords::voicing (active, tr, inv, {});
            g.setFont (LookAndFeel::monoFont (20.0f));
            auto va = voiceArea.toFloat();
            drawDisplay (g, va.withHeight (54.0f), 7.0f);
            juce::String names;
            for (int n : voiced) names << juce::MidiMessage::getMidiNoteName (n, true, true, 4) << "  ";
            glowText (g, active >= 0 ? chords::shortName (active) : juce::String::fromUTF8 ("\xe2\x80\x94"), va.withHeight (54.0f).reduced (14.0f, 0.0f), juce::Justification::centredLeft);
            g.setFont (LookAndFeel::monoFont (13.0f));
            glowText (g, names.trim(), va.withHeight (54.0f).reduced (14.0f, 0.0f), juce::Justification::centredRight, 0.8f);
            text (g, "Auto = voice leading: each new chord picks the inversion closest to the last one, so the chord layer moves smoothly.", voiceArea.withTrimmedTop (66));

            // diatonic chords
            static const char* majRoman[] = { "I", "ii", "iii", "IV", "V", "vi", "vii\xc2\xb0" };
            static const char* minRoman[] = { "i", "ii\xc2\xb0", "III", "iv", "v", "VI", "VII" };
            const bool minor = plain (processor.apvts, "keyMode") > 0.5f;
            for (int i = 0; i < 7; ++i)
            {
                auto b = diatonicButton (i);
                const int c = diatonicChord (i);
                const bool on = c == active;
                drawHardButton (g, b, {}, false, true, false);
                if (on) { g.setColour (th().accent.withAlpha (0.25f)); g.fillRoundedRectangle (b.withTrimmedBottom (4.0f), 6.0f); g.setColour (th().accent); g.drawRoundedRectangle (b.withTrimmedBottom (4.0f), 6.0f, 2.0f); }
                g.setColour (juce::Colour (0xff9aa0a7));
                g.setFont (spaced (11.0f, true, 0.1f));
                g.drawText (juce::String::fromUTF8 (minor ? minRoman[i] : majRoman[i]), b.withHeight (30.0f).translated (0.0f, 8.0f), juce::Justification::centred, false);
                g.setColour (juce::Colours::white);
                g.setFont (LookAndFeel::uiFont (16.0f, true));
                g.drawText (chords::shortName (c), b.withTrimmedTop (36.0f).withTrimmedBottom (20.0f), juce::Justification::centred, false);
            }
            text (g, "These seven chords always sound good together in this key.", { inKey.getX(), inKey.getY() + 130, inKey.getWidth(), 40 });

            paintKeyboard (g, keyboard.toFloat().withTrimmedTop (34.0f), voiced, active);
        }

        void paintKeyboard (juce::Graphics& g, juce::Rectangle<float> r, const std::vector<int>& voiced, int active)
        {
            const int first = 24, last = 96;   // C1 .. C7 (FL: C2 .. C8)
            int whites = 0;
            for (int n = first; n <= last; ++n) if (! juce::MidiMessage::isMidiNoteBlack (n)) ++whites;
            const float ww = r.getWidth() / (float) whites;
            const float kh = r.getHeight() - 26.0f;
            const float ky = r.getY() + 26.0f;

            // sounding harp tones of the chord
            std::vector<int> tones;
            if (active >= 0)
            {
                static const int lows[] = { 60, 60, 54, 48, 48 };
                const int range = juce::roundToInt (plain (processor.apvts, "harpRange"));
                const int low = lows[juce::jlimit (1, 4, range)];
                const int mask = chords::pitchClassMask (active, juce::roundToInt (plain (processor.apvts, "transpose")));
                for (int n = low; n <= low + 12 * range; ++n) if (mask & (1 << (n % 12))) tones.push_back (n);
            }
            auto has = [] (const std::vector<int>& v, int n) { return std::find (v.begin(), v.end(), n) != v.end(); };

            auto keyRect = [&] (int n)
            {
                int wi = 0;
                for (int m = first; m < n; ++m) if (! juce::MidiMessage::isMidiNoteBlack (m)) ++wi;
                if (! juce::MidiMessage::isMidiNoteBlack (n)) return juce::Rectangle<float> (r.getX() + wi * ww, ky, ww, kh);
                return juce::Rectangle<float> (r.getX() + wi * ww - ww * 0.32f, ky, ww * 0.64f, kh * 0.62f);
            };

            // zones above the keys
            auto zone = [&] (int lo, int hi, const juce::String& name, juce::Colour c)
            {
                const auto a = keyRect (lo), b = keyRect (hi);
                auto z = juce::Rectangle<float> (a.getX() + 1.0f, r.getY() + 4.0f, b.getRight() - a.getX() - 2.0f, 16.0f);
                g.setColour (c.withAlpha (0.18f));
                g.fillRoundedRectangle (z, 4.0f);
                g.setColour (c);
                g.setFont (spaced (8.5f, true, 0.15f));
                g.drawText (name, z, juce::Justification::centred, false);
            };
            if (processor.chordZones.load())
            {
                zone (processor.rootZoneLow(), processor.rootZoneHigh(), "CHORD ROOT", th().accent);
                zone (processor.typeZoneLow(), processor.typeZoneHigh(), "CHORD TYPE", th().soft);
                zone (juce::jmin (last, processor.typeZoneHigh() + 1), last, "HARP NOTES (EXACT PITCH)", juce::Colour (0xff9aa0a7));
            }
            else
                zone (first, last, "EVERY KEY PLAYS THE HARP (CHORD ZONES OFF)", juce::Colour (0xff9aa0a7));

            for (int pass = 0; pass < 2; ++pass)
                for (int n = first; n <= last; ++n)
                {
                    const bool black = juce::MidiMessage::isMidiNoteBlack (n);
                    if ((pass == 0) == black) continue;
                    auto k = keyRect (n);
                    if (! black)
                    {
                        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6f1e6), 0.0f, k.getY(), juce::Colour (0xffd9d2c3), 0.0f, k.getBottom(), false));
                        g.fillRect (k.reduced (0.5f, 0.0f));
                        g.setColour (juce::Colour (0xff6a655c));
                        g.drawRect (k, 0.6f);
                        if (n % 12 == 0)
                        {
                            g.setColour (juce::Colour (0xff6a655c));
                            g.setFont (LookAndFeel::uiFont (8.5f));
                            g.drawText ("C" + juce::String (n / 12), k.withTrimmedTop (k.getHeight() - 14.0f), juce::Justification::centred, false);
                        }
                    }
                    else
                    {
                        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2a2c), 0.0f, k.getY(), juce::Colour (0xff0c0c0d), 0.0f, k.getBottom(), false));
                        g.fillRoundedRectangle (k, 2.0f);
                    }
                    // markers above / on the key (the keys themselves are never recoloured)
                    const bool isVoiced = has (voiced, n), isTone = has (tones, n);
                    if (isVoiced || isTone)
                    {
                        auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ k.getCentreX(), k.getY() + (black ? k.getHeight() - 10.0f : 10.0f) });
                        g.setColour (th().accent.withAlpha (0.35f));
                        g.fillEllipse (dot.expanded (3.0f));
                        g.setColour (isVoiced ? th().accent : th().soft.withAlpha (0.7f));
                        g.fillEllipse (dot);
                    }
                    if (active >= 0 && processor.chordZones.load() && n == processor.rootZoneLow() + chords::rootOf (active))
                    {
                        g.setColour (th().accent);
                        g.fillRect (k.withTrimmedTop (k.getHeight() - 4.0f));
                    }
                }
        }

    private:
        juce::Rectangle<int> helpK, voiceArea, inKey, keyboard;
        juce::String lastSig;
        int pressed = -1;
    };

    //==========================================================================
    class MemoryPage : public EditPage
    {
    public:
        MemoryPage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            grid = panel (juce::String::fromUTF8 ("CHORD MEMORY  \xc2\xb7  8 BANKS x 16 CHORDS"), 0, 0, 1232, 470);
            choice ("memStep", "EACH CHORD LASTS", { 16, 482, 160, 44 });
            help = { 200, 486, 1000, 40 };
        }

        juce::Rectangle<float> cell (int bank, int step) const
        {
            return { (float) grid.getX() + 40.0f + step * 58.0f, (float) grid.getY() + bank * 52.0f + 8.0f, 54.0f, 40.0f };
        }
        juce::Rectangle<float> rec (int bank) const   { return { (float) grid.getX() + 40.0f + 16 * 58.0f + 6.0f, (float) grid.getY() + bank * 52.0f + 10.0f, 46.0f, 36.0f }; }
        juce::Rectangle<float> drag (int bank) const  { return { (float) grid.getX() + 40.0f + 16 * 58.0f + 58.0f, (float) grid.getY() + bank * 52.0f + 10.0f, 66.0f, 36.0f }; }

        void tick() override
        {
            juce::String sig;
            sig << processor.memBank.load() << processor.memPlayStep.load() << (processor.memRecord.load() ? "r" : "");
            for (auto& m : processor.memory) sig << m.load() << ",";
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }

        void paintExtras (juce::Graphics& g) override
        {
            text (g, "Choose a bank, press REC and click chords: they fill the steps. Switch the strum mode to MEMO and the sequence plays in time with FL Studio. Drag DRAG onto the FL piano roll or playlist to get the chords as MIDI.", help);
            const int cur = processor.memBank.load();
            const int playing = processor.memPlayStep.load();
            for (int b = 0; b < Hamamatsu108Processor::memBanks; ++b)
            {
                const int len = processor.memLength[(size_t) b].load();
                auto letter = juce::Rectangle<float> ((float) grid.getX() + 8.0f, cell (b, 0).getY(), 26.0f, 40.0f);
                drawDisplay (g, letter, 5.0f);
                g.setFont (LookAndFeel::monoFont (16.0f));
                if (b == cur) glowText (g, juce::String::charToString ((juce::juce_wchar) ('A' + b)), letter, juce::Justification::centred);
                else { g.setColour (juce::Colour (0xff6e737a)); g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + b)), letter, juce::Justification::centred, false); }
                for (int s = 0; s < Hamamatsu108Processor::memSteps; ++s)
                {
                    auto c = cell (b, s);
                    const int ch = s < len ? processor.memChord (b, s) : -1;
                    const bool on = b == cur && s == playing;
                    if (on) { g.setColour (th().accent.withAlpha (0.3f)); g.fillRoundedRectangle (c.expanded (2.0f), 6.0f); }
                    drawDisplay (g, c, 5.0f);
                    if (on) { g.setColour (th().accent); g.drawRoundedRectangle (c, 5.0f, 1.5f); }
                    g.setFont (LookAndFeel::monoFont (ch >= 0 ? 13.0f : 10.0f));
                    if (ch >= 0) glowText (g, chords::shortName (ch), c, juce::Justification::centred, b == cur ? 1.0f : 0.6f);
                    else { g.setColour (juce::Colour (0xff33363b)); g.drawText (juce::String (s + 1), c, juce::Justification::centred, false); }
                }
                drawHardButton (g, rec (b), "REC", b == cur && processor.memRecord.load(), true, false);
                drawDisplay (g, drag (b), 5.0f);
                g.setFont (LookAndFeel::monoFont (11.0f));
                glowText (g, juce::String::fromUTF8 ("DRAG \xe2\x87\xa2"), drag (b), juce::Justification::centred, len > 0 ? 1.0f : 0.3f);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto p = e.position;
            for (int b = 0; b < Hamamatsu108Processor::memBanks; ++b)
            {
                if (rec (b).contains (p))
                {
                    const bool on = ! (processor.memRecord.load() && processor.memBank.load() == b);
                    processor.memBank.store (b);
                    if (on && processor.memLength[(size_t) b].load() >= Hamamatsu108Processor::memSteps) processor.memClear (b);
                    processor.memRecord.store (on);
                    return;
                }
                if (p.y >= cell (b, 0).getY() && p.y <= cell (b, 0).getBottom() && p.x < cell (b, 0).getX()) { processor.memBank.store (b); return; }
                for (int s = 0; s < Hamamatsu108Processor::memSteps; ++s)
                    if (cell (b, s).contains (p))
                    {
                        processor.memBank.store (b);
                        const int ch = processor.memChord (b, s);
                        if (e.mods.isPopupMenu())
                        {
                            juce::PopupMenu m;
                            m.addItem (1, "Delete step " + juce::String (s + 1), ch >= 0);
                            m.addItem (2, "Clear bank " + juce::String::charToString ((juce::juce_wchar) ('A' + b)));
                            juce::Component::SafePointer<MemoryPage> safe (this);
                            m.showMenuAsync (juce::PopupMenu::Options(), [safe, b, s] (int r)
                            {
                                if (safe == nullptr) return;
                                if (r == 1) safe->processor.memDelete (b, s);
                                if (r == 2) safe->processor.memClear (b);
                            });
                        }
                        else if (ch >= 0) { processor.clickChord (ch); audition = ch; }
                        return;
                    }
            }
        }

        void mouseUp (const juce::MouseEvent&) override { if (audition >= 0) processor.releaseChord (audition); audition = -1; dragging = false; }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragging || e.getDistanceFromDragStart() < 6) return;
            for (int b = 0; b < Hamamatsu108Processor::memBanks; ++b)
                if (drag (b).contains (e.mouseDownPosition) && processor.memLength[(size_t) b].load() > 0)
                {
                    dragging = true;
                    const auto f = writeMidi (b);
                    if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
                    return;
                }
        }

        juce::File writeMidi (int bank) const
        {
            const int len = processor.memLength[(size_t) bank].load();
            const int ppq = 960;
            const double stepTicks = Hamamatsu108Processor::memStepQuarters (juce::roundToInt (plain (processor.apvts, "memStep"))) * ppq;
            const int tr = juce::roundToInt (plain (processor.apvts, "transpose"));
            const int inv = juce::roundToInt (plain (processor.apvts, "voicing"));
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / processor.hostBpm.load())), 0.0);
            std::vector<int> prev;
            for (int i = 0; i < len; ++i)
            {
                const int c = processor.memChord (bank, i);
                if (c < 0) continue;
                prev = chords::voicing (c, tr, inv, prev);
                for (int n : prev)
                {
                    seq.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 96), i * stepTicks);
                    seq.addEvent (juce::MidiMessage::noteOff (1, n), (i + 1) * stepTicks - 10.0);
                }
            }
            seq.updateMatchedPairs();
            seq.sort();
            juce::MidiFile mf;
            mf.setTicksPerQuarterNote (ppq);
            mf.addTrack (seq);
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Kuprell Hamamatsu 108");
            dir.createDirectory();
            auto file = dir.getChildFile ("Hamamatsu 108 memory " + juce::String::charToString ((juce::juce_wchar) ('A' + bank)) + ".mid");
            file.deleteFile();
            juce::FileOutputStream out (file);
            if (out.openedOk()) mf.writeTo (out);
            return file;
        }

    private:
        juce::Rectangle<int> grid, help;
        juce::String lastSig;
        int audition = -1;
        bool dragging = false;
    };

    //==========================================================================
    // Editable function curve: 8 points, drag them anywhere (time and level).
    // First and last point stay on the left and right wall. Shift = fine, double-click = reset point, right-click = menu.
    class FunctionEditor : public juce::Component, private juce::Timer
    {
    public:
        explicit FunctionEditor (Hamamatsu108Processor& p) : processor (p) { startTimerHz (30); }
        ~FunctionEditor() override { stopTimer(); }

        static constexpr int n = mod::funcPoints;
        juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (10.0f); }
        float yOf (int i) const { return plain (processor.apvts, ("funcP" + juce::String (i + 1)).toRawUTF8()); }
        float xOf (int i) const
        {
            std::array<float, n> xs {}, ys {};
            processor.funcShape (xs, ys);
            return xs[(size_t) i];
        }
        juce::Point<float> point (int i) const
        {
            auto r = area();
            return { r.getX() + r.getWidth() * xOf (i), r.getBottom() - r.getHeight() * yOf (i) };
        }
        static float defaultY (int i) { static const float d[] = { 0.0f, 0.85f, 1.0f, 0.7f, 0.55f, 0.4f, 0.2f, 0.0f }; return d[i]; }
        static float defaultX (int i) { return (float) i / (float) (n - 1); }

        void setPoint (int i, float x, float y)
        {
            setParam (processor.apvts, "funcP" + juce::String (i + 1), juce::jlimit (0.0f, 1.0f, y));
            if (i > 0 && i < n - 1)
            {
                const float lo = xOf (i - 1), hi = xOf (i + 1);
                setParam (processor.apvts, "funcT" + juce::String (i + 1), juce::jlimit (lo, hi, x));
            }
        }
        void setShape (std::array<float, n> xs, std::array<float, n> ys)
        {
            // set times from left to right so the ordering clamp never blocks a move
            for (int i = 1; i < n - 1; ++i) setParam (processor.apvts, "funcT" + juce::String (i + 1), 0.0f);
            for (int i = n - 2; i >= 1; --i) setParam (processor.apvts, "funcT" + juce::String (i + 1), xs[(size_t) i]);
            for (int i = 0; i < n; ++i) setParam (processor.apvts, "funcP" + juce::String (i + 1), ys[(size_t) i]);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            drawDisplay (g, r, 6.0f);
            auto in = area();
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            for (int k = 1; k < 4; ++k)
            {
                g.drawVerticalLine ((int) (in.getX() + in.getWidth() * k / 4.0f), in.getY(), in.getBottom());
                g.drawHorizontalLine ((int) (in.getY() + in.getHeight() * k / 4.0f), in.getX(), in.getRight());
            }
            juce::Path path;
            for (int i = 0; i < n; ++i)
                i == 0 ? path.startNewSubPath (point (i)) : path.lineTo (point (i));
            juce::Path fill (path);
            fill.lineTo (in.getRight(), in.getBottom()); fill.lineTo (in.getX(), in.getBottom()); fill.closeSubPath();
            g.setColour (th().accent.withAlpha (0.10f));
            g.fillPath (fill);
            g.setColour (th().accent.withAlpha (0.3f));
            g.strokePath (path, juce::PathStrokeType (4.0f));
            g.setColour (th().accent);
            g.strokePath (path, juce::PathStrokeType (1.6f));
            for (int i = 0; i < n; ++i)
            {
                const bool hot = i == dragging || i == hover;
                g.setColour (hot ? juce::Colours::white : th().soft);
                g.fillEllipse (juce::Rectangle<float> (hot ? 11.0f : 8.0f, hot ? 11.0f : 8.0f).withCentre (point (i)));
            }
            // playhead: only while something sounds
            if (processor.modActive.load())
            {
                const float x = in.getX() + in.getWidth() * processor.funcPhase.load();
                const float v = processor.modSources[mod::function].load();
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.drawVerticalLine ((int) x, in.getY(), in.getBottom());
                g.setColour (juce::Colours::white);
                g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ x, in.getBottom() - in.getHeight() * v }));
            }
            // exact values of the point under the mouse
            const int shown = dragging >= 0 ? dragging : hover;
            if (shown >= 0)
            {
                const juce::String txt = "P" + juce::String (shown + 1) + "  time " + juce::String (juce::roundToInt (xOf (shown) * 100.0f)) + " %  level "
                                       + juce::String (juce::roundToInt (yOf (shown) * 100.0f)) + " %";
                g.setFont (kuprell::LookAndFeel::monoFont (11.0f));
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.drawText (txt, in.reduced (4.0f, 2.0f), juce::Justification::topRight);
            }
        }

        int nearest (juce::Point<float> p) const
        {
            int best = 0;
            for (int i = 1; i < n; ++i)
                if (point (i).getDistanceFrom (p) < point (best).getDistanceFrom (p)) best = i;
            return best;
        }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int i = nearest (e.position);
            const int h = point (i).getDistanceFrom (e.position) < 14.0f ? i : -1;
            if (h != hover) { hover = h; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = nearest (e.position);
            if (e.mods.isPopupMenu()) { showMenu (point (i).getDistanceFrom (e.position) < 18.0f ? i : -1); return; }
            dragging = i;
            grabOffset = point (i) - e.position;
            startX = xOf (i); startY = yOf (i); startPos = e.position;
            processor.apvts.undoManager->beginNewTransaction();
            repaint();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragging < 0) return;
            auto r = area();
            float x, y;
            if (e.mods.isShiftDown())   // fine adjustment: 10x slower
            {
                const auto d = (e.position - startPos) * 0.1f;
                x = startX + d.x / r.getWidth();
                y = startY - d.y / r.getHeight();
            }
            else
            {
                const auto p = e.position + grabOffset;
                x = (p.x - r.getX()) / r.getWidth();
                y = (r.getBottom() - p.y) / r.getHeight();
            }
            // snap onto the walls and the top / bottom when close
            auto snap = [] (float v) { return v < 0.015f ? 0.0f : v > 0.985f ? 1.0f : v; };
            setPoint (dragging, snap (juce::jlimit (0.0f, 1.0f, x)), snap (juce::jlimit (0.0f, 1.0f, y)));
            repaint();
        }
        void mouseUp (const juce::MouseEvent&) override { dragging = -1; repaint(); }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int i = nearest (e.position);
            if (point (i).getDistanceFrom (e.position) < 18.0f) setPoint (i, defaultX (i), defaultY (i));
            repaint();
        }

        void showMenu (int i)
        {
            juce::PopupMenu m;
            if (i >= 0)
            {
                m.addSectionHeader ("Point " + juce::String (i + 1));
                m.addItem (1, "Reset point");
                m.addItem (2, "Level 0 %");
                m.addItem (3, "Level 100 %");
                if (i > 0 && i < n - 1)
                {
                    m.addItem (4, "Move to previous point (jump)");
                    m.addItem (5, "Move to next point (jump)");
                }
                m.addSeparator();
            }
            m.addSectionHeader ("Whole curve");
            m.addItem (10, "Reset curve");
            m.addItem (11, "Decay (no attack)");
            m.addItem (12, "Ramp up");
            m.addItem (13, "Triangle");
            m.addItem (14, "Gate (square)");
            m.addItem (15, "Flat 100 %");
            m.addItem (16, "Invert");
            juce::Component::SafePointer<FunctionEditor> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, i] (int r)
            {
                if (safe == nullptr || r == 0) return;
                safe->processor.apvts.undoManager->beginNewTransaction();
                safe->menuAction (i, r);
            });
        }
        void menuAction (int i, int r)
        {
            std::array<float, n> xs {}, ys {};
            processor.funcShape (xs, ys);
            auto even = [] { std::array<float, n> x {}; for (int k = 0; k < n; ++k) x[(size_t) k] = defaultX (k); return x; };
            switch (r)
            {
                case 1: setPoint (i, defaultX (i), defaultY (i)); break;
                case 2: setPoint (i, xs[(size_t) i], 0.0f); break;
                case 3: setPoint (i, xs[(size_t) i], 1.0f); break;
                case 4: setPoint (i, xs[(size_t) i - 1], ys[(size_t) i]); break;
                case 5: setPoint (i, xs[(size_t) i + 1], ys[(size_t) i]); break;
                case 10: { std::array<float, n> y {}; for (int k = 0; k < n; ++k) y[(size_t) k] = defaultY (k); setShape (even(), y); break; }
                case 11: { std::array<float, n> y {}; for (int k = 0; k < n; ++k) y[(size_t) k] = std::pow (1.0f - defaultX (k), 2.0f); setShape (even(), y); break; }
                case 12: { std::array<float, n> y {}; for (int k = 0; k < n; ++k) y[(size_t) k] = defaultX (k); setShape (even(), y); break; }
                case 13: { std::array<float, n> y {}; for (int k = 0; k < n; ++k) y[(size_t) k] = 1.0f - std::abs (2.0f * defaultX (k) - 1.0f); setShape (even(), y); break; }
                case 14: setShape ({ 0.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }); break;
                case 15: { std::array<float, n> y {}; y.fill (1.0f); setShape (even(), y); break; }
                case 16: { for (auto& v : ys) v = 1.0f - v; setShape (xs, ys); break; }
                default: break;
            }
            repaint();
        }

    private:
        void timerCallback() override
        {
            const bool act = processor.modActive.load();
            if (act || act != wasActive) repaint();
            wasActive = act;
        }
        Hamamatsu108Processor& processor;
        int dragging = -1, hover = -1;
        bool wasActive = false;
        juce::Point<float> grabOffset, startPos;
        float startX = 0.0f, startY = 0.0f;
    };

    class ModPage : public EditPage
    {
    public:
        ModPage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            for (int i = 0; i < 2; ++i)
            {
                const juce::String n (i + 1);
                auto l = panel ("LFO " + n, i * 312, 0, 300, 250);
                choice ("lfo" + n + "Shape", "SHAPE", { l.getX(), l.getY(), 128, 44 });
                choice ("lfo" + n + "Sync", "SYNC", { l.getX() + 140, l.getY(), 128, 44 });
                knobAt ("lfo" + n + "Rate", "Rate", 40, l.getX(), l.getY() + 60, 90);
                lfoDisplay[(size_t) i] = { l.getX() + 100, l.getY() + 64, l.getWidth() - 100, 86 };
            }
            auto f = panel ("FUNCTION", 624, 0, 360, 250);
            funcEditor = std::make_unique<FunctionEditor> (p);
            addAndMakeVisible (*funcEditor);
            funcEditor->setBounds (f.getX(), f.getY(), f.getWidth(), 120);
            choice ("funcMode", "MODE", { f.getX(), f.getY() + 132, 130, 44 });
            knobAt ("funcRate", "Rate", 36, f.getX() + 150, f.getY() + 126, 80);

            auto rnd = panel ("RANDOM", 996, 0, 236, 250);
            knobAt ("randRate", "Rate", 34, rnd.getX(), rnd.getY(), 68);
            knobAt ("randSmooth", "Smooth", 34, rnd.getX() + 70, rnd.getY(), 68);
            knobAt ("randChaos", "Chaos", 34, rnd.getX() + 140, rnd.getY(), 68);
            randDisplay = { rnd.getX(), rnd.getY() + 96, rnd.getWidth(), 92 };

            matrix = panel ("MOD MATRIX  \xc2\xb7  SOURCE  \xe2\x86\x92  TARGET  \xc2\xb7  AMOUNT", 0, 262, 1232, 268);
            for (int i = 0; i < mod::matrixSlots; ++i)
            {
                const juce::String n (i + 1);
                const int col = i / 4, row = i % 4;
                const int x = matrix.getX() + col * 600, y = matrix.getY() + row * 52;
                choice ("mod" + n + "Src", {}, { x + 30, y + 8, 150, 28 });
                choice ("mod" + n + "Dst", {}, { x + 230, y + 8, 180, 28 });
                Knob::Style st; st.showValue = false; st.labelHeight = 1.0f;
                auto* k = knobs.add (new Knob (processor.apvts, "mod" + n + "Amt", {}, 18, st));
                k->onInteract = [this] (const Knob& kn) { owner.status (kn.getDisplayName(), kn.getValueText()); };
                addAndMakeVisible (k);
                k->setBounds (x + 424, y + 2, 36, 40);
            }
        }

        void tick() override
        {
            for (int i = 0; i < 2; ++i) repaint (lfoDisplay[(size_t) i]);
            repaint (randDisplay);
            repaint (matrix);
            funcEditor->repaint();
            const float r = processor.modSources[mod::random].load();
            randHistory[(size_t) randPos] = r;
            randPos = (randPos + 1) % (int) randHistory.size();
        }

        void paintExtras (juce::Graphics& g) override
        {
            for (int i = 0; i < 2; ++i)
            {
                auto d = lfoDisplay[(size_t) i].toFloat();
                drawDisplay (g, d, 6.0f);
                const int shape = juce::roundToInt (plain (processor.apvts, ("lfo" + juce::String (i + 1) + "Shape").toRawUTF8()));
                juce::Path p;
                auto in = d.reduced (8.0f);
                for (int x = 0; x <= 64; ++x)
                {
                    const float ph = x / 64.0f;
                    float v;
                    switch (shape)
                    {
                        case 1: v = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
                        case 2: v = 2.0f * ph - 1.0f; break;
                        case 3: v = ph < 0.5f ? 1.0f : -1.0f; break;
                        case 4: v = std::sin ((float) (x / 8) * 2.7f); break;
                        default: v = std::sin (ph * juce::MathConstants<float>::twoPi); break;
                    }
                    const juce::Point<float> pt (in.getX() + in.getWidth() * ph, in.getCentreY() - v * in.getHeight() * 0.45f);
                    x == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
                }
                g.setColour (th().accent.withAlpha (0.8f));
                g.strokePath (p, juce::PathStrokeType (1.5f));
                const float cur = processor.modSources[(size_t) (mod::lfo1 + i)].load();
                g.setColour (juce::Colours::white);
                const float ph = processor.lfoPhase[(size_t) i].load();
                g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ in.getX() + in.getWidth() * ph, in.getCentreY() - cur * in.getHeight() * 0.45f }));
            }

            auto rd = randDisplay.toFloat();
            drawDisplay (g, rd, 6.0f);
            juce::Path rp;
            auto in = rd.reduced (8.0f);
            const int n = (int) randHistory.size();
            for (int i = 0; i < n; ++i)
            {
                const float v = randHistory[(size_t) ((randPos + i) % n)];
                const juce::Point<float> pt (in.getX() + in.getWidth() * i / (float) (n - 1), in.getCentreY() - v * in.getHeight() * 0.45f);
                i == 0 ? rp.startNewSubPath (pt) : rp.lineTo (pt);
            }
            g.setColour (th().accent);
            g.strokePath (rp, juce::PathStrokeType (1.5f));

            // matrix rows: numbers, arrows, live source meters, amounts
            for (int i = 0; i < mod::matrixSlots; ++i)
            {
                const int col = i / 4, row = i % 4;
                const int x = matrix.getX() + col * 600, y = matrix.getY() + row * 52;
                g.setColour (juce::Colour (0xff6e737a));
                g.setFont (LookAndFeel::uiFont (11.0f, true));
                g.drawText (juce::String (i + 1), x, y + 8, 24, 28, juce::Justification::centredLeft, false);
                g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92"), x + 186, y + 8, 40, 28, juce::Justification::centred, false);
                const juce::String n (i + 1);
                const int src = juce::roundToInt (plain (processor.apvts, ("mod" + n + "Src").toRawUTF8()));
                const float amt = plain (processor.apvts, ("mod" + n + "Amt").toRawUTF8());
                auto meter = juce::Rectangle<float> ((float) x + 470.0f, (float) y + 18.0f, 100.0f, 6.0f);
                g.setColour (juce::Colour (0xff22252a));
                g.fillRoundedRectangle (meter, 3.0f);
                if (src > 0)
                {
                    const float v = juce::jlimit (-1.0f, 1.0f, processor.modSources[(size_t) src].load() * amt);
                    const float cx = meter.getCentreX();
                    g.setColour (th().accent);
                    g.fillRoundedRectangle (v >= 0 ? meter.withX (cx).withWidth (meter.getWidth() * 0.5f * v) : meter.withX (cx + meter.getWidth() * 0.5f * v).withWidth (-meter.getWidth() * 0.5f * v), 3.0f);
                }
                g.setFont (LookAndFeel::monoFont (10.5f));
                g.setColour (th().soft);
                g.drawText (processor.apvts.getParameter ("mod" + n + "Amt")->getCurrentValueAsText(), (int) meter.getX(), y + 26, 100, 14, juce::Justification::centred, false);
            }
        }

    private:
        std::array<juce::Rectangle<int>, 2> lfoDisplay;
        juce::Rectangle<int> randDisplay, matrix;
        std::unique_ptr<FunctionEditor> funcEditor;
        std::array<float, 90> randHistory {};
        int randPos = 0;
    };

    //==========================================================================
    // FX: 8 slot cards. Drag a card by its header onto another slot to move it.
    class FxPage : public EditPage
    {
    public:
        static constexpr int cardW = 146, gap = 8;

        FxPage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            for (int i = 0; i < fx::numSlots; ++i)
            {
                const juce::String n (i + 1), id ("fx" + n);
                auto c = panel ("SLOT " + n, i * (cardW + gap), 0, cardW, 470);
                auto* t = choice (id + "Type", "EFFECT", { c.getX() - 6, c.getY(), c.getWidth() + 12, 44 });
                t->onChange = [this] { updateLabels(); };
                toggle (id + "On", "ON", { c.getX() + 14, c.getY() + 56, c.getWidth() - 28, 28 });
                aKnobs[(size_t) i] = knobAt (id + "A", "A", 44, c.getX() - 4, c.getY() + 100, c.getWidth() + 8);
                bKnobs[(size_t) i] = knobAt (id + "B", "B", 44, c.getX() - 4, c.getY() + 196, c.getWidth() + 8);
                knobAt (id + "Mix", "Mix", 44, c.getX() - 4, c.getY() + 292, c.getWidth() + 8);
            }
            updateLabels();
            help = { 0, 482, 1232, 40 };
        }

        void updateLabels()
        {
            for (int i = 0; i < fx::numSlots; ++i)
            {
                const int t = juce::roundToInt (plain (processor.apvts, ("fx" + juce::String (i + 1) + "Type").toRawUTF8()));
                const auto names = fx::controlNames (t);
                aKnobs[(size_t) i]->setLabel (names.first);
                bKnobs[(size_t) i]->setLabel (names.second);
                aKnobs[(size_t) i]->setEnabled (t != 0);
                bKnobs[(size_t) i]->setEnabled (t != 0);
            }
            repaint();
        }

        void tick() override
        {
            juce::String sig;
            for (int i = 0; i < fx::numSlots; ++i) sig << plain (processor.apvts, ("fx" + juce::String (i + 1) + "Type").toRawUTF8()) << ",";
            if (sig != lastSig) { lastSig = sig; updateLabels(); }
        }

        void paintExtras (juce::Graphics& g) override
        {
            text (g, juce::String::fromUTF8 ("Signal flows left to right: SLOT 1 \xe2\x86\x92 SLOT 8. Drag a slot by its title onto another slot to move it. "
                                             "The three FX knobs on the PLAY page are the Mix of slots 1-3. Delay times follow the FL Studio tempo."), help);
            for (int i = 0; i < fx::numSlots; ++i)
            {
                const int t = juce::roundToInt (plain (processor.apvts, ("fx" + juce::String (i + 1) + "Type").toRawUTF8()));
                if (t == 12 || t == 13)
                {
                    g.setFont (LookAndFeel::monoFont (10.5f));
                    g.setColour (th().soft);
                    const float a = plain (processor.apvts, ("fx" + juce::String (i + 1) + "A").toRawUTF8());
                    g.drawText (fx::delayName (a), i * (cardW + gap), 40 + 100 + 84, cardW, 14, juce::Justification::centred, false);
                }
                if (i < fx::numSlots - 1)
                {
                    g.setColour (juce::Colour (0xff4a4e54));
                    g.drawText (juce::String::fromUTF8 ("\xe2\x80\xba"), (i + 1) * (cardW + gap) - gap - 2, 10, gap + 4, 16, juce::Justification::centred, false);
                }
            }
            if (dragFrom >= 0 && dragTo >= 0 && dragTo != dragFrom)
            {
                g.setColour (th().accent);
                g.drawRoundedRectangle (juce::Rectangle<float> ((float) (dragTo * (cardW + gap)), 0.0f, (float) cardW, 470.0f), 14.0f, 2.0f);
            }
        }

        int slotAt (juce::Point<float> p) const { const int i = (int) (p.x / (cardW + gap)); return p.y < 470 && i >= 0 && i < fx::numSlots ? i : -1; }

        void mouseDown (const juce::MouseEvent& e) override { dragFrom = e.position.y < 34 ? slotAt (e.position) : -1; dragTo = -1; }
        void mouseDrag (const juce::MouseEvent& e) override { if (dragFrom >= 0) { dragTo = slotAt (e.position); repaint(); } }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragFrom >= 0 && dragTo >= 0 && dragTo != dragFrom) moveSlot (dragFrom, dragTo);
            dragFrom = dragTo = -1;
            repaint();
        }

        void moveSlot (int from, int to)
        {
            static const char* fields[] = { "Type", "On", "A", "B", "Mix" };
            auto read = [this] (int slot)
            {
                std::array<float, 5> v {};
                for (int f = 0; f < 5; ++f) v[(size_t) f] = processor.apvts.getParameter ("fx" + juce::String (slot + 1) + fields[f])->getValue();
                return v;
            };
            auto write = [this] (int slot, const std::array<float, 5>& v)
            {
                for (int f = 0; f < 5; ++f)
                {
                    auto* p = processor.apvts.getParameter ("fx" + juce::String (slot + 1) + fields[f]);
                    p->beginChangeGesture(); p->setValueNotifyingHost (v[(size_t) f]); p->endChangeGesture();
                }
            };
            std::vector<std::array<float, 5>> all;
            for (int i = 0; i < fx::numSlots; ++i) all.push_back (read (i));
            const auto moving = all[(size_t) from];
            all.erase (all.begin() + from);
            all.insert (all.begin() + to, moving);
            for (int i = 0; i < fx::numSlots; ++i) write (i, all[(size_t) i]);
            updateLabels();
        }

    private:
        std::array<Knob*, fx::numSlots> aKnobs {}, bKnobs {};
        juce::Rectangle<int> help;
        juce::String lastSig;
        int dragFrom = -1, dragTo = -1;
    };

    //==========================================================================
    class CharacterPage : public EditPage
    {
    public:
        CharacterPage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            auto m = panel ("MACROS", 0, 0, 1232, 250);
            const char* ids[] = { "morph", "macroAge", "macroDream", "macroWobble" };
            const char* labels[] = { "MORPH", "AGE", "DREAM", "WOBBLE" };
            for (int i = 0; i < 4; ++i)
            {
                knobAt (ids[i], labels[i], 70, m.getX() + i * 300, m.getY() + 4, 110);
                descs[(size_t) i] = { m.getX() + i * 300 + 118, m.getY() + 14, 160, 120 };
            }
            models = panel ("MODEL  \xc2\xb7  CLICK TO CHOOSE", 0, 262, 1232, 268);
        }

        juce::Rectangle<float> modelCard (int i) const
        {
            const float w = (models.getWidth() - 32 - 5 * 12.0f) / 6.0f;
            return { models.getX() + 16.0f + i * (w + 12.0f), models.getY() + 44.0f, w, 196.0f };
        }

        void tick() override
        {
            const int m = juce::roundToInt (plain (processor.apvts, "model"));
            if (m != shownModel) { shownModel = m; repaint(); }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < 6; ++i)
                if (modelCard (i).contains (e.position)) { setParam (processor.apvts, "model", (float) i); owner.status ("Model", Hamamatsu108Processor::modelNames()[i]); }
        }

        void paintExtras (juce::Graphics& g) override
        {
            static const char* d[] = {
                "Blends the two harp layers (turn DUAL on) and drives the 8 Morph targets on the Engine page.",
                "Time and wear: tape wow, hiss, a darker top, fewer bits. From fresh to well loved.",
                "Space: a huge soft reverb on top and a longer harp decay.",
                "Motion: slow pitch drift and wobble, like a warm old tape." };
            for (int i = 0; i < 4; ++i) text (g, d[i], descs[(size_t) i], 12.0f);

            static const char* era[] = { "1981", "1984", "1989", "1996", "1999", "today" };
            for (int i = 0; i < 6; ++i)
            {
                auto c = modelCard (i);
                const bool on = i == shownModel;
                if (on) { g.setColour (th().accent.withAlpha (0.25f)); g.fillRoundedRectangle (c.expanded (3.0f), 12.0f); }
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3f46), 0.0f, c.getY(), juce::Colour (0xff24272b), 0.0f, c.getBottom(), false));
                g.fillRoundedRectangle (c, 10.0f);
                g.setColour (on ? th().accent : juce::Colours::black);
                g.drawRoundedRectangle (c, 10.0f, on ? 2.0f : 1.0f);
                auto in = c.reduced (12.0f);
                g.setFont (spaced (9.0f, true, 0.2f));
                g.setColour (juce::Colour (0xff9aa0a7));
                g.drawText (era[i], in.removeFromTop (16.0f), juce::Justification::centredLeft, false);
                g.setFont (LookAndFeel::uiFont (16.0f, true));
                g.setColour (juce::Colours::white);
                g.drawText (Hamamatsu108Processor::modelNames()[i], in.removeFromTop (26.0f), juce::Justification::centredLeft, false);
                const auto& sp = Hamamatsu108Processor::modelSpec (i);
                const float vals[] = { sp.cutoff / 20000.0f, juce::jmin (1.0f, sp.width / 1.2f), juce::jmin (1.0f, sp.hiss / 0.0009f), (16.0f - sp.bits) / 8.0f };
                const char* names[] = { "BRIGHT", "WIDE", "NOISE", "GRIT" };
                in.removeFromTop (10.0f);
                for (int k = 0; k < 4; ++k)
                {
                    auto row = in.removeFromTop (22.0f);
                    g.setFont (spaced (8.0f, true, 0.12f));
                    g.setColour (juce::Colour (0xff9aa0a7));
                    g.drawText (names[k], row.removeFromLeft (52.0f), juce::Justification::centredLeft, false);
                    auto bar = row.withSizeKeepingCentre (row.getWidth(), 5.0f);
                    g.setColour (juce::Colour (0xff1a1c1f));
                    g.fillRoundedRectangle (bar, 2.5f);
                    g.setColour (on ? th().accent : th().deep);
                    g.fillRoundedRectangle (bar.withWidth (juce::jmax (3.0f, bar.getWidth() * vals[k])), 2.5f);
                }
            }
        }

    private:
        std::array<juce::Rectangle<int>, 4> descs;
        juce::Rectangle<int> models;
        int shownModel = -1;
    };

    //==========================================================================
    const juce::StringArray& EditView::tabNames()
    {
        static const juce::StringArray n { "ENGINE", "STRUM & ARP", "CHORDS", "MEMORY", "MOD", "FX", "CHARACTER" };
        return n;
    }

    EditView::EditView (Hamamatsu108Processor& p) : processor (p)
    {
        logo = juce::Drawable::createFromImageData (KuprellBrand::kuprelllogocream_svg, KuprellBrand::kuprelllogocream_svgSize);

        pages.add (new EnginePage (p, *this));
        pages.add (new StrumPage (p, *this));
        pages.add (new ChordsPage (p, *this));
        pages.add (new MemoryPage (p, *this));
        pages.add (new ModPage (p, *this));
        pages.add (new FxPage (p, *this));
        pages.add (new CharacterPage (p, *this));
        for (auto* pg : pages) addChildComponent (pg);

        Knob::Style st;
        st.labelHeight = 8.5f;
        st.showValue = false;
        const char* ids[] = { "morph", "macroAge", "macroDream", "macroWobble" };
        const char* labels[] = { "MORPH", "AGE", "DREAM", "WOBBLE" };
        for (int i = 0; i < 4; ++i)
        {
            auto* k = macroKnobs.add (new Knob (p.apvts, ids[i], labels[i], 28, st));
            k->onInteract = [this] (const Knob& kn) { status (kn.getDisplayName(), kn.getValueText()); };
            addAndMakeVisible (k);
        }
        showTab (0);
        startTimerHz (20);
    }

    EditView::~EditView() { stopTimer(); }

    void EditView::status (const juce::String& name, const juce::String& value)
    {
        vfdName = name;
        vfdValue = value;
        repaint (juce::Rectangle<int> (200, 14, 620, 60));
        if (onStatus) onStatus (name, value);
    }

    void EditView::showTab (int index)
    {
        tab = juce::jlimit (0, pages.size() - 1, index);
        for (int i = 0; i < pages.size(); ++i) pages[i]->setVisible (i == tab);
        repaint();
    }

    juce::Rectangle<int> EditView::tabArea (int i) const
    {
        const float w = 1232.0f / (float) tabNames().size();
        return juce::Rectangle<float> (24.0f + i * w, 98.0f, w - 6.0f, 34.0f).toNearestInt();
    }

    void EditView::resized()
    {
        for (auto* pg : pages) pg->setBounds (24, 142, 1232, 530);
        for (int i = 0; i < macroKnobs.size(); ++i) macroKnobs[i]->setBounds (840 + i * 96, 12, 90, 68);
    }

    void EditView::mouseDown (const juce::MouseEvent& e)
    {
        for (int i = 0; i < tabNames().size(); ++i)
            if (tabArea (i).contains (e.getPosition())) showTab (i);
    }

    void EditView::timerCallback()
    {
        if (auto* pg = pages[tab]) pg->tick();
    }

    void EditView::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff0b0c0d));

        // metal header plate (top of the instrument)
        auto plate = juce::Rectangle<float> (24.0f, 0.0f, 1232.0f, 86.0f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (plate.translated (0.0f, 8.0f), 26.0f);
        juce::Path pp;
        pp.addRoundedRectangle (plate.getX(), plate.getY(), plate.getWidth(), plate.getHeight(), 26.0f, 26.0f, false, false, true, true);
        juce::ColourGradient metal (juce::Colour (0xff555b63), 0.0f, plate.getY(), juce::Colour (0xff2c3036), 0.0f, plate.getBottom(), false);
        g.setGradientFill (metal);
        g.fillPath (pp);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.13f), plate.getX(), plate.getY(), juce::Colours::transparentWhite, plate.getX() + 400.0f, plate.getBottom(), false));
        g.fillPath (pp);

        if (logo != nullptr)
            logo->drawWithin (g, { 52.0f, 24.0f, 100.0f, 16.0f }, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yTop, 1.0f);
        g.setFont (spaced (10.0f, true, 0.4f));
        g.setColour (engrave);
        g.drawText ("HAMAMATSU 108", juce::Rectangle<float> (52.0f, 46.0f, 160.0f, 14.0f), juce::Justification::centredLeft, false);

        auto vfd = juce::Rectangle<float> (220.0f, 21.0f, 600.0f, 44.0f);
        drawDisplay (g, vfd, 8.0f);
        g.setFont (LookAndFeel::monoFont (15.0f));
        auto in = vfd.reduced (14.0f, 0.0f);
        glowText (g, vfdName.toUpperCase(), in, juce::Justification::centredLeft);
        glowText (g, vfdValue, in.withTrimmedRight (200.0f), juce::Justification::centredRight);
        glowText (g, "MODEL " + Hamamatsu108Processor::modelNames()[juce::roundToInt (plain (processor.apvts, "model"))].toUpperCase(), in, juce::Justification::centredRight, 0.6f);

        // tabs
        for (int i = 0; i < tabNames().size(); ++i)
        {
            auto t = tabArea (i).toFloat();
            const bool on = i == tab;
            g.setColour (on ? juce::Colour (0xff1c2a2e) : juce::Colour (0xff17191c));
            g.fillRoundedRectangle (t, 7.0f);
            g.setColour (on ? th().accent : juce::Colour (0xff2c2f34));
            g.drawRoundedRectangle (t.reduced (0.5f), 7.0f, on ? 1.5f : 1.0f);
            g.setFont (spaced (10.5f, true, 0.2f));
            g.setColour (on ? th().soft : juce::Colour (0xff9aa0a7));
            g.drawText (tabNames()[i], t, juce::Justification::centred, false);
            if (on) { g.setColour (th().accent); g.fillRoundedRectangle (t.withTop (t.getBottom() - 3.0f).reduced (20.0f, 0.0f), 1.5f); }
        }
    }
}
