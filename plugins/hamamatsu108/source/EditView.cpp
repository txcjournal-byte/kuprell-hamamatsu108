#include "EditView.h"
#include "Skins.h"
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
                if (th().concrete)
                {
                    g.setFont (LookAndFeel::uiFont (11.0f));
                    g.setColour (page::ink());
                    g.drawText (quiet (pn.title), pn.area.toFloat().reduced (14.0f, 0.0f).withHeight (28.0f), juce::Justification::centredLeft, false);
                    g.setColour (juce::Colour (0xff2e2d2a));
                    g.fillRect ((float) pn.area.getX() + 14.0f, (float) pn.area.getY() + 28.0f, (float) pn.area.getWidth() - 28.0f, 1.0f);
                    continue;
                }
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
            // LAYER A: the harp (or any engine), LAYER B: a second engine, MORPH between them (like a hybrid instrument)
            auto a = panel ("LAYER A  \xc2\xb7  ENGINE", 0, 0, 470, 250);
            choice ("harpVoice", "ENGINE", { a.getX(), a.getY(), 200, 44 });
            choice ("keysPlay", "YOUR KEYS / PIANO ROLL", { a.getX() + 214, a.getY(), 224, 44 });
            knobAt ("harpTone", "Tone", 46, a.getX(), a.getY() + 64, 96);
            knobAt ("harpShape", "Shape", 46, a.getX() + 108, a.getY() + 64, 96);
            knobAt ("harpRange", "Range", 46, a.getX() + 216, a.getY() + 64, 96);
            choice ("harpOctave", "OCTAVE", { a.getX() + 326, a.getY() + 74, 112, 44 });

            auto m = panel ("MORPH", 482, 0, 232, 250);
            knobAt ("morph", "A  /  B", 92, m.getX() + 6, m.getY() + 8, 188);
            toggle ("dual", "LAYER B ON", { m.getX() + 34, m.getY() + 160, 132, 30 });

            auto b = panel ("LAYER B  \xc2\xb7  ENGINE", 726, 0, 506, 250);
            choice ("harpVoice2", "ENGINE", { b.getX(), b.getY(), 200, 44 });
            choice ("bOctave", "OCTAVE", { b.getX() + 214, b.getY(), 112, 44 });
            const char* bIds[] = { "bTone", "bShape", "bDetune", "bAttack", "bDecay", "bLevel" };
            const char* bNames[] = { "Tone", "Shape", "Detune", "Attack", "Decay", "Level" };
            for (int i = 0; i < 6; ++i) knobAt (bIds[i], bNames[i], 42, b.getX() + i * 78, b.getY() + 64, 76);

            auto env = panel ("LAYER A ENVELOPE", 0, 262, 380, 268);
            knobAt ("harpAttack", "Attack", 60, env.getX() - 6, env.getY() + 50, 90);
            knobAt ("harpDecay", "Decay", 60, env.getX() + 82, env.getY() + 50, 90);
            knobAt ("harpSustain", "Sustain", 60, env.getX() + 170, env.getY() + 50, 90);
            knobAt ("harpRelease", "Release", 60, env.getX() + 258, env.getY() + 50, 90);

            auto st = panel (juce::String::fromUTF8 ("STEREO  \xc2\xb7  POLISH"), 392, 262, 212, 268);
            knobAt ("spread", "Spread", 46, st.getX() - 10, st.getY() + 6, 66);
            knobAt ("width", "Width", 46, st.getX() + 56, st.getY() + 6, 66);
            knobAt ("polish", "Polish", 46, st.getX() + 122, st.getY() + 6, 66);

            auto v = panel ("VIBRATO", 616, 262, 188, 268);
            knobAt ("vibRate", "Rate", 46, v.getX() - 8, v.getY() + 20, 80);
            knobAt ("vibDepth", "Depth", 46, v.getX() + 72, v.getY() + 20, 80);

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

        void tick() override
        {
            repaint (amountArea());
            const int a = juce::roundToInt (plain (processor.apvts, "harpVoice")), b = juce::roundToInt (plain (processor.apvts, "harpVoice2"));
            if (a != shownA || b != shownB) { shownA = a; shownB = b; repaint(); }
        }
        juce::Rectangle<int> amountArea() const { return { 816 + 316, 262 + 40, 100, 220 }; }

        static juce::String engineText (int m)
        {
            static const char* t[] = {
                "Classic chime: two detuned pulse/triangle oscillators, envelope-swept filter. SHAPE: no effect.",
                "Celesta: struck metal bars (modal). Soft, music-box like.",
                "Vibes: metal bars with motor tremolo.",
                "Glass: glass partials, long and pure.",
                "Pluck: physical string model, two strings, pick position.",
                "Flute: breath and tone, soft attack.",
                "Marimba: wooden bars, short and warm.",
                "Steel: steel pan, bright overtones.",
                "E-Piano: FM tine model, bell on hard velocity.",
                "Strings: three detuned saws, ensemble.",
                "Analog: three saws + sub, 24 dB low-pass with envelope. SHAPE: detune / width.",
                "Wavetable: scans sine > warm > saw > hollow > vocal. SHAPE: table position.",
                "Digital FM: 80s digital bell and keys. SHAPE: modulator ratio (0.5 .. 9).",
                "Air: tuned noise resonators, breathy and glassy. SHAPE: clear tone > breath." };
            return t[juce::jlimit (0, harp::numModels - 1, m)];
        }

        void paintExtras (juce::Graphics& g) override
        {
            text (g, engineText (shownA < 0 ? juce::roundToInt (plain (processor.apvts, "harpVoice")) : shownA), { 16, 204, 438, 34 }, 10.5f);
            text (g, engineText (shownB < 0 ? juce::roundToInt (plain (processor.apvts, "harpVoice2")) : shownB), { 742, 204, 474, 34 }, 10.5f);
            text (g, "SPREAD fans the strings across the stereo field, layer B mirrors layer A. WIDTH opens the mix, the bass stays mono. POLISH: bus glue and air.", { 408, 420, 182, 90 }, 10.5f);
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
        int shownA = -1, shownB = -1;
    };

    //==========================================================================
    //==========================================================================
    // ARP step editor: NOTE / VELOCITY / OCT / RATCHET / CHANCE / TIE for 16 steps
    class SeqEditor : public juce::Component
    {
    public:
        explicit SeqEditor (Hamamatsu108Processor& p) : processor (p) {}
        bool simple = true;   // only the strength bars (like a step sequencer); the detailed lanes are gone from the GUI

        void tick()
        {
            juce::String sig;
            sig << processor.seqLen.load() << "|" << processor.arpStepGui.load() << "|" << plain (processor.apvts, "arpPattern") << "|";
            for (int i = 0; i < Hamamatsu108Processor::seqSteps; ++i)
                sig << processor.seqDeg[(size_t) i].load() << processor.seqOct[(size_t) i].load() << processor.seqRatchet[(size_t) i].load()
                    << processor.seqTie[(size_t) i].load() << juce::roundToInt (processor.seqVel[(size_t) i].load() * 50) << juce::roundToInt (processor.seqProb[(size_t) i].load() * 4) << ",";
            if (sig != last) { last = sig; repaint(); }
        }

        enum Lane { note, vel, oct, rat, prob, tie, numLanes };
        juce::Rectangle<float> lane (int l) const
        {
            if (simple) return l == vel ? juce::Rectangle<float> (0.0f, 24.0f, (float) getWidth(), (float) getHeight() - 26.0f) : juce::Rectangle<float> (0.0f, -1000.0f, 0.0f, 0.0f);
            static const float hs[] = { 66.0f, 46.0f, 17.0f, 17.0f, 17.0f, 15.0f };
            float y = 22.0f;
            for (int i = 0; i < l; ++i) y += hs[i] + 4.0f;
            return { 56.0f, y, (float) getWidth() - 56.0f, hs[l] };
        }
        juce::Rectangle<float> cell (int l, int i) const
        {
            auto r = lane (l);
            const float w = r.getWidth() / 16.0f;
            return { r.getX() + i * w + 1.5f, r.getY(), w - 3.0f, r.getHeight() };
        }
        juce::Rectangle<float> menuButton() const { return { (float) getWidth() - 110.0f, 0.0f, 110.0f, 18.0f }; }

        void paint (juce::Graphics& g) override
        {
            const int len = processor.seqLen.load();
            const int playing = processor.arpStepGui.load();
            const bool stepsMode = juce::roundToInt (plain (processor.apvts, "arpPattern")) == 8;
            if (simple)
            {
                g.setFont (spaced (8.0f, true, 0.12f));
                g.setColour (juce::Colour (0xff9aa0a7));
                g.drawText ("DRAG = STEP STRENGTH  /  RIGHT-CLICK = REST", juce::Rectangle<float> (0.0f, 0.0f, 360.0f, 18.0f), juce::Justification::centredLeft, false);
                drawDisplay (g, menuButton(), 5.0f);
                g.setFont (LookAndFeel::monoFont (11.0f));
                glowText (g, juce::String::fromUTF8 ("STEPS \xe2\x96\xbe  ") + juce::String (len), menuButton(), juce::Justification::centred);
                for (int i = 0; i < 16; ++i)
                {
                    auto c = cell (vel, i);
                    const bool inLen = i < len;
                    g.setColour (juce::Colour (0xff15171a)); g.fillRoundedRectangle (c, 3.0f);
                    const float v = processor.seqVel[(size_t) i].load();
                    const bool rest = v <= 0.06f;
                    if (! rest)
                    {
                        auto bar = c.withTop (c.getBottom() - c.getHeight() * v);
                        g.setGradientFill (juce::ColourGradient (th().accent.withAlpha (inLen ? 0.95f : 0.2f), 0.0f, bar.getY(), th().accent.withAlpha (inLen ? 0.5f : 0.08f), 0.0f, bar.getBottom(), false));   // same as the steps on the main page
                        g.fillRoundedRectangle (bar, 3.0f);
                        g.setColour (th().accent.withAlpha (inLen ? 1.0f : 0.25f));
                        g.fillRoundedRectangle (bar.withHeight (3.0f), 1.5f);
                    }
                    if (i == playing && inLen) { g.setColour (juce::Colours::white.withAlpha (0.6f)); g.drawRoundedRectangle (c.expanded (1.0f), 3.0f, 1.5f); }
                }
                return;
            }
            static const char* names[] = { "NOTE", "VELOCITY", "OCT", "RATCHET", "CHANCE", "TIE" };
            g.setFont (spaced (8.0f, true, 0.12f));
            for (int l = 0; l < numLanes; ++l)
            {
                g.setColour (juce::Colour (0xff9aa0a7).withAlpha ((l == note || l == oct) && ! stepsMode ? 0.35f : 0.9f));
                g.drawText (names[l], lane (l).withX (0.0f).withWidth (54.0f), juce::Justification::centredLeft, false);
            }
            g.setColour (juce::Colour (0xff9aa0a7));
            g.drawText (stepsMode ? juce::String ("STEPS PATTERN: NOTE + OCT PLAY") : juce::String ("NOTE / OCT: PICK THE STEPS PATTERN"),
                        juce::Rectangle<float> (0.0f, 0.0f, 300.0f, 18.0f), juce::Justification::centredLeft, false);
            drawDisplay (g, menuButton(), 5.0f);
            g.setFont (LookAndFeel::monoFont (11.0f));
            glowText (g, juce::String::fromUTF8 ("STEPS \xe2\x96\xbe  ") + juce::String (len), menuButton(), juce::Justification::centred);

            for (int i = 0; i < 16; ++i)
            {
                const bool inLen = i < len;
                const float a = inLen ? 1.0f : 0.25f;
                const auto acc = th().accent;
                // note
                {
                    auto c = cell (note, i);
                    g.setColour (juce::Colour (0xff15171a)); g.fillRoundedRectangle (c, 3.0f);
                    const int d = processor.seqDeg[(size_t) i].load();
                    if (d >= 0)
                    {
                        const float h = c.getHeight() * (0.18f + 0.82f * (float) d / 7.0f);
                        g.setColour (acc.withAlpha ((stepsMode ? 0.85f : 0.3f) * a));
                        g.fillRoundedRectangle (c.withTop (c.getBottom() - h), 3.0f);
                        g.setColour (juce::Colours::black.withAlpha (0.7f));
                        g.setFont (LookAndFeel::monoFont (9.0f));
                        g.drawText (juce::String (d + 1), c.withTop (c.getBottom() - 13.0f), juce::Justification::centred, false);
                    }
                    else
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.25f * a));
                        g.drawText (juce::String::fromUTF8 ("\xe2\x80\x93"), c, juce::Justification::centred, false);
                    }
                    if (i == playing && inLen) { g.setColour (juce::Colours::white.withAlpha (0.6f)); g.drawRoundedRectangle (c.expanded (1.0f), 3.0f, 1.5f); }
                }
                // velocity
                {
                    auto c = cell (vel, i);
                    g.setColour (juce::Colour (0xff15171a)); g.fillRoundedRectangle (c, 3.0f);
                    const float v = processor.seqVel[(size_t) i].load();
                    g.setColour (th().soft.withAlpha (0.85f * a));
                    g.fillRoundedRectangle (c.withTop (c.getBottom() - c.getHeight() * v), 3.0f);
                }
                auto small = [&] (int l, const juce::String& t, bool lit)
                {
                    auto c = cell (l, i);
                    g.setColour (lit ? acc.withAlpha (0.35f * a) : juce::Colour (0xff15171a));
                    g.fillRoundedRectangle (c, 3.0f);
                    g.setColour (juce::Colours::white.withAlpha ((lit ? 0.95f : 0.4f) * a));
                    g.setFont (LookAndFeel::monoFont (9.5f));
                    g.drawText (t, c, juce::Justification::centred, false);
                };
                const int o = processor.seqOct[(size_t) i].load();
                small (oct, o == 0 ? juce::String ("0") : (o > 0 ? "+" + juce::String (o) : juce::String (o)), o != 0);
                const int r = processor.seqRatchet[(size_t) i].load();
                small (rat, r == 1 ? juce::String ("1") : "x" + juce::String (r), r > 1);
                const float pr = processor.seqProb[(size_t) i].load();
                small (prob, juce::String (juce::roundToInt (pr * 100)), pr < 0.99f);
                const bool t = processor.seqTie[(size_t) i].load() != 0;
                small (tie, t ? juce::String::fromUTF8 ("\xe2\x80\x94") : juce::String(), t);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto p = e.position;
            if (menuButton().contains (p)) { showMenu(); return; }
            for (int l = 0; l < numLanes; ++l)
                for (int i = 0; i < 16; ++i)
                    if (cell (l, i).contains (p))
                    {
                        dragLane = l;
                        if (simple && l == vel && e.mods.isPopupMenu())
                        {
                            auto& v = processor.seqVel[(size_t) i];
                            v.store (v.load() <= 0.06f ? 0.8f : 0.0f);
                            dragLane = -1;
                        }
                        else if (l == note && e.mods.isPopupMenu())
                        {
                            auto& d = processor.seqDeg[(size_t) i];
                            d.store (d.load() < 0 ? 0 : -1);
                            dragLane = -1;
                        }
                        else if (l == oct)  { auto& v = processor.seqOct[(size_t) i]; v.store (v.load() >= 2 ? -1 : v.load() + 1); }
                        else if (l == rat)  { auto& v = processor.seqRatchet[(size_t) i]; v.store (v.load() >= 4 ? 1 : v.load() + 1); }
                        else if (l == prob) { auto& v = processor.seqProb[(size_t) i]; const float x = v.load(); v.store (x > 0.9f ? 0.75f : x > 0.6f ? 0.5f : x > 0.4f ? 0.25f : 1.0f); }
                        else if (l == tie)  { auto& v = processor.seqTie[(size_t) i]; v.store (v.load() ? 0 : 1); }
                        else setFromY (l, i, p.y);
                        tick();
                        return;
                    }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragLane != note && dragLane != vel) return;
            auto r = lane (dragLane);
            const int i = juce::jlimit (0, 15, (int) ((e.position.x - r.getX()) / (r.getWidth() / 16.0f)));
            setFromY (dragLane, i, e.position.y);
            tick();
        }
        void mouseUp (const juce::MouseEvent&) override { dragLane = -1; }

    private:
        void setFromY (int l, int i, float y)
        {
            auto c = cell (l, i);
            const float f = juce::jlimit (0.0f, 1.0f, (c.getBottom() - y) / c.getHeight());
            if (l == note) processor.seqDeg[(size_t) i].store (juce::jlimit (0, 7, juce::roundToInt (f * 7.0f)));
            if (l == vel)  processor.seqVel[(size_t) i].store (juce::jmax (0.05f, f));
        }

        void showMenu()
        {
            juce::PopupMenu m;
            juce::PopupMenu pre;
            for (int i = 0; i < Hamamatsu108Processor::seqPresetNames().size(); ++i) pre.addItem (100 + i, Hamamatsu108Processor::seqPresetNames()[i]);
            m.addSubMenu ("Load steps", pre);
            juce::PopupMenu lenM;
            for (int l = 1; l <= 16; ++l) lenM.addItem (200 + l, juce::String (l) + " steps", true, processor.seqLen.load() == l);
            m.addSubMenu ("Length", lenM);
            m.addItem (1, "Randomise (new idea)");
            m.addItem (2, "Reset");
            m.addSeparator();
            m.addItem (3, "Use the STEPS pattern (NOTE + OCT play)", true, juce::roundToInt (plain (processor.apvts, "arpPattern")) == 8);
            juce::Component::SafePointer<SeqEditor> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
            {
                if (safe == nullptr || r <= 0) return;
                auto& pr = safe->processor;
                if (r >= 200) pr.seqLen.store (r - 200);
                else if (r >= 100) { pr.seqLoadPreset (r - 100); setParam (pr.apvts, "arpPattern", 8.0f); }
                else if (r == 1) { pr.seqRandomise(); setParam (pr.apvts, "arpPattern", 8.0f); }
                else if (r == 2) pr.seqLoadPreset (0);
                else if (r == 3) setParam (pr.apvts, "arpPattern", 8.0f);
                safe->tick();
            });
        }

        Hamamatsu108Processor& processor;
        juce::String last;
        int dragLane = -1;
    };

    class StrumPage : public EditPage
    {
    public:
        StrumPage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            auto s = panel ("LIVE STRUM  \xc2\xb7  HOW THE HAND PLAYS", 0, 0, 600, 250);
            choice ("strumMode", "MODE", { s.getX(), s.getY(), 110, 44 });
            choice ("strumPattern", "PATTERN", { s.getX() + 122, s.getY(), 150, 44 });
            choice ("keysPlay", "YOUR KEYS", { s.getX() + 284, s.getY(), 120, 44 });
            toggle ("hold", "HOLD", { s.getX() + 418, s.getY() + 14, 90, 30 });
            knobAt ("strumRate", "Rate", 44, s.getX(), s.getY() + 58, 106);
            knobAt ("strumFeel", "Feel", 44, s.getX() + 110, s.getY() + 58, 106);
            knobAt ("strumSpeed", "Speed", 44, s.getX() + 220, s.getY() + 58, 106);
            knobAt ("strumSwing", "Swing", 44, s.getX() + 330, s.getY() + 58, 106);
            knobAt ("strumVary", "Variation", 44, s.getX() + 440, s.getY() + 58, 106);
            helpS = { s.getX(), s.getY() + 160, s.getWidth(), 40 };

            preview = panel ("ONE BAR OF THE HAND  \xc2\xb7  CLICK A STEP TO EDIT (CUSTOM PATTERN)", 612, 0, 620, 250);
            choice ("strumShape", "STROKE SHAPE", { preview.getX(), preview.getY(), 140, 44 });
            choice ("strumSpeedMul", "SPEED", { preview.getX() + 150, preview.getY(), 110, 44 });
            choice ("strumRestart", "PATTERN START", { preview.getX() + 270, preview.getY(), 160, 44 });
            preview = preview.withTrimmedTop (54).withTrimmedBottom (16);

            auto a = panel ("ARPEGGIO", 0, 262, 600, 268);
            choice ("arpPattern", "PATTERN", { a.getX(), a.getY(), 150, 44 });
            choice ("arpRegen", "REGEN", { a.getX() + 162, a.getY(), 120, 44 });
            knobAt ("strumRate", "Rate", 40, a.getX(), a.getY() + 58, 92);
            knobAt ("arpOctaves", "Octaves", 40, a.getX() + 92, a.getY() + 58, 92);
            knobAt ("arpGate", "Gate", 40, a.getX() + 184, a.getY() + 58, 92);
            knobAt ("arpSwing", "Swing", 40, a.getX() + 276, a.getY() + 58, 92);
            knobAt ("arpVelScale", "Dynamics", 40, a.getX() + 368, a.getY() + 58, 92);
            knobAt ("arpChance", "Chance", 40, a.getX() + 460, a.getY() + 58, 92);
            helpA = { a.getX(), a.getY() + 160, a.getWidth(), 50 };

            pattern = panel ("ARP STEPS  \xc2\xb7  DRAG NOTE / VELOCITY, CLICK THE SMALL ROWS", 612, 262, 620, 268);
            seq = std::make_unique<SeqEditor> (processor);
            addAndMakeVisible (*seq);
            seq->setBounds (pattern);
        }

        void tick() override
        {
            juce::String sig;
            for (auto* id : { "strumFeel", "strumSpeed", "strumRate", "arpPattern", "arpChance", "arpRegen", "harpRange", "strumPattern", "strumSwing", "strumVary", "keysPlay" })
                sig << processor.apvts.getRawParameterValue (id)->load() << ",";
            sig << processor.patternString (13);
            if (seq != nullptr) seq->tick();
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }

        void paintExtras (juce::Graphics& g) override
        {
            text (g, "PATTERN = how a player strums (Even = plain down/up at RATE). FEEL loosens timing and force, SPEED = how fast a stroke crosses the strings, SWING delays every 2nd 1/16, VARIATION adds ghost strokes, leaves notes out and plays a fill every 4th bar. YOUR KEYS: Notes = exact pitch, Strum = each key press strums the keys you hold, Pattern = the pattern plays them.", helpS);
            text (g, "ARP plays the chord tones one by one (As Played = the order of your keys, Chord = the whole chord every step). OCTAVES = how wide it climbs, GATE = note length (100 % lets the strings ring), SWING delays every 2nd step, DYNAMICS = how much the VELOCITY row is followed (0 = all notes equal), CHANCE drops notes, REGEN rolls a new variation every 1, 2 or 4 bars. The STEPS row on the right adds accents, ratchets, ties and chance to every pattern; the STEPS pattern also plays its own NOTE / OCT line.", helpA);

            // hand preview: one bar of the pattern (or two beats of the plain clock), low strings at the bottom
            auto r = preview.toFloat().reduced (0.0f, 6.0f);
            const float feel = plain (processor.apvts, "strumFeel"), speed = plain (processor.apvts, "strumSpeed");
            const int patIdx = juce::roundToInt (plain (processor.apvts, "strumPattern"));
            juce::Random rnd (42);
            if (patIdx > 0)
            {
                const juce::String steps (processor.patternString (patIdx));
                const int len = steps.length();
                const float swing = plain (processor.apvts, "strumSwing");
                const float colW = r.getWidth() / (float) len;
                for (int i = 0; i <= len; ++i)
                {
                    g.setColour (juce::Colours::white.withAlpha (i % 4 == 0 ? 0.12f : 0.04f));
                    g.drawVerticalLine ((int) (r.getX() + i * colW), r.getY(), r.getBottom());
                }
                auto yOf = [&] (float f) { return r.getBottom() - 6.0f - (r.getHeight() - 12.0f) * f; };
                bool sweepDn = false;
                for (int i = 0; i < len; ++i)
                {
                    const char c = (char) steps[i];
                    if (c == '.') continue;
                    const float x = r.getX() + (i + 0.5f + (i % 2 == 1 ? swing * 0.33f : 0.0f)) * colW;
                    const float accent = i == 0 ? 1.0f : (i % 4 == 0 ? 0.9f : 0.75f);
                    float lo = 0.0f, hi = 1.0f, a = 0.9f * accent; bool up = false;
                    float w = juce::jmin (colW * 0.9f, speed * 0.25f);
                    switch (c)
                    {
                        case 'D': break;
                        case 'U': up = true; lo = 0.3f; a *= 0.75f; break;
                        case 'd': lo = 0.35f; a *= 0.65f; break;
                        case 'u': up = true; lo = 0.65f; a *= 0.5f; break;
                        case 'x': lo = 0.4f; hi = 0.6f; a *= 0.25f; w *= 0.5f; break;
                        case 'B': lo = 0.0f; hi = 0.08f; w = 0.0f; break;
                        case 'H': lo = 0.9f; hi = 1.0f; w = 0.0f; a *= 0.65f; break;
                        case 'T': lo = 0.3f + 0.4f * rnd.nextFloat(); hi = lo + 0.15f; w = 2.0f; a *= 0.7f; break;
                        case 'S': sweepDn = ! sweepDn; up = ! sweepDn; w = colW * 3.0f; break;
                        default: break;
                    }
                    const float y0 = yOf (up ? hi : lo), y1 = yOf (up ? lo : hi);
                    g.setColour (th().accent.withAlpha (0.2f + 0.8f * a));
                    if (w < 1.0f) g.fillEllipse (x - 4.0f, (y0 + y1) * 0.5f - 4.0f, 8.0f, 8.0f);
                    else
                    {
                        juce::Path p;
                        p.startNewSubPath (x, y0);
                        p.lineTo (x + w, y1);
                        g.strokePath (p, juce::PathStrokeType (2.0f + 2.0f * a, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                        juce::Path head;
                        head.addArrow ({ x + w * 0.6f, y0 + (y1 - y0) * 0.6f, x + w, y1 }, 0.0f, 9.0f, 8.0f);
                        g.fillPath (head);
                    }
                }
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.setFont (spaced (8.5f, true, 0.15f));
                for (int b = 0; b < len / 4; ++b)
                    g.drawText (juce::String (b + 1), juce::Rectangle<float> (r.getX() + b * 4 * colW + 4.0f, r.getY(), 20.0f, 12.0f), juce::Justification::left, false);
                // the stroke letter of every step (click to change it)
                static const char* names[] = { "D", "U", "d", "u", "B", "H", "T", "x", "S" };
                juce::ignoreUnused (names);
                g.setFont (LookAndFeel::monoFont (10.0f));
                for (int i = 0; i < len; ++i)
                {
                    const char c = (char) steps[i];
                    g.setColour (c == '.' ? juce::Colours::white.withAlpha (0.2f) : th().soft);
                    g.drawText (c == '.' ? juce::String::fromUTF8 ("\xc2\xb7") : juce::String::charToString ((juce::juce_wchar) c),
                                juce::Rectangle<float> (r.getX() + i * colW, r.getBottom() + 1.0f, colW, 14.0f), juce::Justification::centred, false);
                }
            }
            else
            {
            const int rateIdx = juce::roundToInt (plain (processor.apvts, "strumRate"));
            static const double rq[] = { 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 2.0 / 3.0 };
            const double stepQ = rq[juce::jlimit (0, 8, rateIdx)];
            const int strokes = juce::jlimit (1, 16, (int) std::round (2.0 / stepQ));
            const float beatW = r.getWidth() / 2.0f;
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            for (int b = 0; b <= 2; ++b) g.drawVerticalLine ((int) (r.getX() + b * beatW), r.getY(), r.getBottom());
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
            }

        }

        void resized() override {}

        // click a step of the hand: it becomes the CUSTOM pattern (a copy of the shown one) and the step changes
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! preview.expanded (0, 16).contains (e.getPosition())) return;
            auto& pr = processor;
            int pat = juce::roundToInt (plain (pr.apvts, "strumPattern"));
            if (pat == 0) { owner.status ("Pattern", "Pick a pattern first (Even has no steps)"); return; }
            if (pat != 13)
            {
                const auto src = pr.patternString (pat);
                const int len = juce::jmin (16, src.length());
                pr.customLen.store (len);
                for (int i = 0; i < 16; ++i) pr.customSteps[(size_t) i].store (i < len ? (int) src[i] : '.');
                setParam (pr.apvts, "strumPattern", 13.0f);
                pat = 13;
            }
            const int len = juce::jlimit (1, 16, pr.customLen.load());
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m, lenM, from;
                for (int l : { 4, 6, 8, 12, 16 }) lenM.addItem (100 + l, juce::String (l) + " steps" + (l == 12 ? juce::String (" (3/4)") : juce::String()), true, len == l);
                for (int i = 1; i <= 12; ++i) from.addItem (200 + i, Hamamatsu108Processor::strumPatternNames()[i]);
                m.addSubMenu ("Length", lenM);
                m.addSubMenu ("Start from a pattern", from);
                m.addItem (1, "Clear all steps");
                m.addSectionHeader ("Strokes: D down, U up, d light down, u light up, B bass (thumb), H high string, T tap, x ghost, S slow sweep");
                juce::Component::SafePointer<StrumPage> safe (this);
                m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
                {
                    if (safe == nullptr || r <= 0) return;
                    auto& p2 = safe->processor;
                    if (r == 1) for (auto& c : p2.customSteps) c.store ('.');
                    if (r >= 100 && r < 200) p2.customLen.store (r - 100);
                    if (r >= 200)
                    {
                        const juce::String src (Hamamatsu108Processor::strumPatternSteps (r - 200));
                        p2.customLen.store (src.length());
                        for (int i = 0; i < 16; ++i) p2.customSteps[(size_t) i].store (i < src.length() ? (int) src[i] : '.');
                    }
                    safe->repaint();
                });
                return;
            }
            const int i = juce::jlimit (0, len - 1, (int) ((e.position.x - (float) preview.getX()) / ((float) preview.getWidth() / (float) len)));
            static const juce::String cycle (".DdUuBHTxS");
            const int cur = juce::jmax (0, cycle.indexOfChar ((juce::juce_wchar) pr.customSteps[(size_t) i].load()));
            pr.customSteps[(size_t) i].store ((int) cycle[(cur + 1) % cycle.length()]);
            owner.status ("Step " + juce::String (i + 1), juce::String::charToString ((juce::juce_wchar) pr.customSteps[(size_t) i].load()));
            repaint();
        }

    private:
        juce::Rectangle<int> preview, pattern, helpS, helpA;
        std::unique_ptr<SeqEditor> seq;
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

            inKey = panel ("CHORDS IN KEY  \xc2\xb7  CLICK = ON / OFF", 624, 0, 608, 250);
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
                if (diatonicButton (i).contains (e.position)) { processor.latchChord (diatonicChord (i)); return; }   // same as the chord buttons: click on, click again off
        }

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
            text (g, "These seven chords always sound good together in this key. A click selects the chord like the chord buttons (click again = off); strum the plate or play keys to hear it.", { inKey.getX(), inKey.getY() + 130, inKey.getWidth(), 40 });

            paintKeyboard (g, keyboard.toFloat().withTrimmedTop (34.0f), voiced, active);
        }

        void paintKeyboard (juce::Graphics& g, juce::Rectangle<float> r, const std::vector<int>& voiced, int active)
        {
            const int first = 12, last = 96;   // FL: C1 .. C8 (the keyswitches sit at C1 - B1)
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
            if (processor.keyswitches.load() && (! processor.chordZones.load() || processor.rootZoneLow() >= 24))
                zone (12, 23, "PATTERN KEYS", juce::Colour (0xffe0b25a));
            if (processor.chordZones.load())
            {
                zone (processor.rootZoneLow(), processor.rootZoneHigh(), "CHORD ROOT", th().accent);
                zone (processor.typeZoneLow(), processor.typeZoneHigh(), "CHORD TYPE", th().soft);
                zone (juce::jmin (last, processor.typeZoneHigh() + 1), last, "HARP NOTES (EXACT PITCH)", juce::Colour (0xff9aa0a7));
            }
            else if (juce::roundToInt (plain (processor.apvts, "keysPlay")) == 6)
            {
                const int ksTop = processor.keyswitches.load() ? 24 : first;
                zone (ksTop, 59, "LEFT HAND: CHORD (ONE KEY = CHORD IN KEY)", th().accent);
                zone (60, last, "RIGHT HAND: WHITE = STRINGS, BLACK = STRUM", th().soft);
            }
            else if (juce::roundToInt (plain (processor.apvts, "keysPlay")) >= 4)
            {
                // FLOURISH: every octave is one chord type
                static const char* tn[] = { "MAJOR RUN", "MINOR RUN", "7 RUN", "MAJ7 RUN", "M7 RUN", "DIM RUN", "AUG RUN" };
                const int ksTop = processor.keyswitches.load() ? 24 : first;
                for (int o = ksTop; o + 11 <= last; o += 12)
                    zone (o, o + 11, tn[juce::jlimit (0, 6, processor.flourishType (o))], o % 24 == 0 ? th().accent : th().soft);
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
            text (g, "Right-click a step: set it to a chord of the key, or load a ready-made progression. Or press REC and click chord buttons. MEMO mode plays the bank with the LIVE strum pattern, in time with FL Studio. DRAG = the chords as MIDI (long stacked notes).", help);
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
                        if (e.mods.isPopupMenu() || ch < 0)
                        {
                            juce::PopupMenu m;
                            const int keyR = juce::roundToInt (plain (processor.apvts, "keyRoot"));
                            const bool minorK = plain (processor.apvts, "keyMode") > 0.5f;
                            juce::PopupMenu setIt;
                            static const int majS[] = { 0, 2, 4, 5, 7, 9, 11 }, minS[] = { 0, 2, 3, 5, 7, 8, 10 };
                            static const int majT[] = { 0, 1, 1, 0, 0, 1, 5 }, minT[] = { 1, 5, 0, 1, 1, 0, 0 };
                            static const int majT7[] = { 3, 4, 4, 3, 2, 4, 4 }, minT7[] = { 4, 4, 3, 4, 4, 3, 2 };
                            for (int d = 0; d < 7; ++d)
                            {
                                const int root = (keyR + (minorK ? minS[d] : majS[d])) % 12;
                                setIt.addItem (1000 + chords::index (root, minorK ? minT[d] : majT[d]), chords::shortName (chords::index (root, minorK ? minT[d] : majT[d])));
                            }
                            setIt.addSeparator();
                            for (int d = 0; d < 7; ++d)
                            {
                                const int root = (keyR + (minorK ? minS[d] : majS[d])) % 12;
                                setIt.addItem (1000 + chords::index (root, minorK ? minT7[d] : majT7[d]), chords::shortName (chords::index (root, minorK ? minT7[d] : majT7[d])));
                            }
                            m.addSubMenu ("Set step " + juce::String (s + 1) + " to (chords of the key)", setIt);
                            m.addItem (1, "Delete step " + juce::String (s + 1), ch >= 0);
                            juce::PopupMenu prog;
                            const auto& all = Hamamatsu108Processor::progressions();
                            for (int i = 0; i < (int) all.size(); ++i) prog.addItem (200 + i, all[(size_t) i].name);
                            m.addSubMenu ("Load a progression into bank " + juce::String::charToString ((juce::juce_wchar) ('A' + b)), prog);
                            m.addItem (2, "Clear bank " + juce::String::charToString ((juce::juce_wchar) ('A' + b)));
                            juce::Component::SafePointer<MemoryPage> safe (this);
                            m.showMenuAsync (juce::PopupMenu::Options(), [safe, b, s, keyR] (int r)
                            {
                                if (safe == nullptr) return;
                                if (r == 1) safe->processor.memDelete (b, s);
                                if (r == 2) safe->processor.memClear (b);
                                if (r >= 200 && r < 1000) safe->processor.memLoad (b, r - 200, keyR);
                                if (r >= 1000) safe->processor.memSet (b, s, r - 1000);
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
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Kuprell Hamamatsu 144");
            dir.createDirectory();
            auto file = dir.getChildFile ("Hamamatsu 144 memory " + juce::String::charToString ((juce::juce_wchar) ('A' + bank)) + ".mid");
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
                knobAt ("lfo" + n + "Rate", "Rate", 34, l.getX(), l.getY() + 52, 80);
                lfoDisplay[(size_t) i] = { l.getX() + 90, l.getY() + 56, l.getWidth() - 90, 70 };
                choice ("lfo" + n + "QDst", juce::String::fromUTF8 ("\xe2\x86\x92 MODULATES"), { l.getX(), l.getY() + 142, 170, 44 });
                knobAt ("lfo" + n + "QAmt", "Depth", 30, l.getX() + 180, l.getY() + 132, 88);
            }
            auto f = panel ("FUNCTION", 624, 0, 360, 250);
            funcEditor = std::make_unique<FunctionEditor> (p);
            addAndMakeVisible (*funcEditor);
            funcEditor->setBounds (f.getX(), f.getY(), f.getWidth(), 110);
            choice ("funcMode", "MODE", { f.getX(), f.getY() + 122, 90, 44 });
            choice ("funcQDst", juce::String::fromUTF8 ("\xe2\x86\x92 MODULATES"), { f.getX() + 98, f.getY() + 122, 120, 44 });
            knobAt ("funcRate", "Rate", 28, f.getX() + 222, f.getY() + 116, 52);
            knobAt ("funcQAmt", "Depth", 28, f.getX() + 276, f.getY() + 116, 52);

            auto rnd = panel ("RANDOM", 996, 0, 236, 250);
            knobAt ("randRate", "Rate", 34, rnd.getX(), rnd.getY(), 68);
            knobAt ("randSmooth", "Smooth", 34, rnd.getX() + 70, rnd.getY(), 68);
            knobAt ("randChaos", "Chaos", 34, rnd.getX() + 140, rnd.getY(), 68);
            randDisplay = { rnd.getX(), rnd.getY() + 84, rnd.getWidth(), 50 };
            choice ("randQDst", juce::String::fromUTF8 ("\xe2\x86\x92 MODULATES"), { rnd.getX(), rnd.getY() + 142, 140, 44 });
            knobAt ("randQAmt", "Depth", 28, rnd.getX() + 146, rnd.getY() + 136, 58);

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
    //==========================================================================
    // GATE grid: 32 steps, click / drag to open or close a step
    class GateEditor : public juce::Component
    {
    public:
        explicit GateEditor (Hamamatsu108Processor& p) : processor (p) {}

        void tick()
        {
            const juce::String sig = juce::String ((juce::int64) processor.gateMask.load()) + "|" + juce::String (processor.gateLen.load()) + "|" + juce::String (processor.gateStepGui.load());
            if (sig != last) { last = sig; repaint(); }
        }
        juce::Rectangle<float> menuButton() const { return { (float) getWidth() - 110.0f, 0.0f, 110.0f, 20.0f }; }
        juce::Rectangle<float> cell (int i) const
        {
            const float top = 28.0f, h = (float) getHeight() - top - 4.0f;
            const float w = (float) getWidth() / 16.0f;
            const int row = i / 16, col = i % 16;
            return { col * w + 1.5f, top + row * (h * 0.5f) + 1.5f, w - 3.0f, h * 0.5f - 3.0f };
        }
        void paint (juce::Graphics& g) override
        {
            const auto mask = processor.gateMask.load();
            const int len = processor.gateLen.load(), playing = processor.gateStepGui.load();
            g.setFont (spaced (8.0f, true, 0.12f));
            g.setColour (juce::Colour (0xff9aa0a7));
            g.drawText ("STEPS: LIT = SOUND, DARK = CLOSED", juce::Rectangle<float> (0.0f, 0.0f, 320.0f, 20.0f), juce::Justification::centredLeft, false);
            drawDisplay (g, menuButton(), 5.0f);
            g.setFont (LookAndFeel::monoFont (11.0f));
            glowText (g, juce::String::fromUTF8 ("STEPS \xe2\x96\xbe  ") + juce::String (len), menuButton(), juce::Justification::centred);
            for (int i = 0; i < Hamamatsu108Processor::gateSteps; ++i)
            {
                auto c = cell (i);
                const bool on = (mask >> (unsigned) i) & 1u, in = i < len;
                g.setColour (on ? th().accent.withAlpha (in ? 0.85f : 0.2f) : juce::Colour (in ? 0xff1b1e22 : 0xff121416));
                g.fillRoundedRectangle (c, 3.0f);
                if (i % 4 == 0) { g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillRect (c.withHeight (2.0f)); }
                if (i == playing) { g.setColour (juce::Colours::white.withAlpha (0.8f)); g.drawRoundedRectangle (c.expanded (1.0f), 3.0f, 1.5f); }
            }
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (menuButton().contains (e.position)) { showMenu(); return; }
            const int i = stepAt (e.position);
            if (i < 0) return;
            paintOn = ! ((processor.gateMask.load() >> (unsigned) i) & 1u);
            setStep (i);
        }
        void mouseDrag (const juce::MouseEvent& e) override { const int i = stepAt (e.position); if (i >= 0) setStep (i); }

    private:
        int stepAt (juce::Point<float> p) const { for (int i = 0; i < Hamamatsu108Processor::gateSteps; ++i) if (cell (i).expanded (1.5f).contains (p)) return i; return -1; }
        void setStep (int i)
        {
            auto m = processor.gateMask.load();
            m = paintOn ? (m | (1u << (unsigned) i)) : (m & ~(1u << (unsigned) i));
            processor.gateMask.store (m);
            if (paintOn) setParam (processor.apvts, "gateOn", 1.0f);
            tick();
        }
        void showMenu()
        {
            juce::PopupMenu m, pre, lenM;
            for (int i = 0; i < Hamamatsu108Processor::gatePresetNames().size(); ++i) pre.addItem (100 + i, Hamamatsu108Processor::gatePresetNames()[i]);
            m.addSubMenu ("Load pattern", pre);
            for (int l : { 4, 8, 12, 16, 24, 32 }) lenM.addItem (200 + l, juce::String (l) + " steps", true, processor.gateLen.load() == l);
            m.addSubMenu ("Length", lenM);
            m.addItem (1, "All open");
            m.addItem (2, "Invert");
            m.addItem (3, "Randomise");
            juce::Component::SafePointer<GateEditor> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
            {
                if (safe == nullptr || r <= 0) return;
                auto& pr = safe->processor;
                if (r >= 200) pr.gateLen.store (r - 200);
                else if (r >= 100) { pr.gateLoadPreset (r - 100); setParam (pr.apvts, "gateOn", 1.0f); }
                else if (r == 1) pr.gateMask.store (0xFFFFFFFFu);
                else if (r == 2) pr.gateMask.store (~pr.gateMask.load());
                else if (r == 3) { juce::Random rr; std::uint32_t mm = 1; for (int i = 1; i < 32; ++i) if (rr.nextFloat() < 0.6f) mm |= 1u << (unsigned) i; pr.gateMask.store (mm); setParam (pr.apvts, "gateOn", 1.0f); }
                safe->tick();
            });
        }
        Hamamatsu108Processor& processor;
        juce::String last;
        bool paintOn = true;
    };


    //==========================================================================
    // ARP & GATE: two simple step rows, like a classic step sequencer
    class ArpGatePage : public EditPage
    {
    public:
        ArpGatePage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            auto a = panel ("ARPEGGIO  \xc2\xb7  PLAYS THE CHORD TONES ONE BY ONE", 0, 0, 600, 250);
            toggle ("strumMode", "ARP", { a.getX(), a.getY() + 14, 90, 30 })->setValues (1.0f, 3.0f);
            choice ("arpPattern", "PATTERN", { a.getX() + 104, a.getY(), 150, 44 });
            choice ("arpRegen", "NEW IDEA EVERY", { a.getX() + 266, a.getY(), 150, 44 });
            knobAt ("strumRate", "Rate", 40, a.getX(), a.getY() + 62, 98);
            knobAt ("arpOctaves", "Octaves", 40, a.getX() + 98, a.getY() + 62, 98);
            knobAt ("arpGate", "Length", 40, a.getX() + 196, a.getY() + 62, 98);
            knobAt ("arpSwing", "Swing", 40, a.getX() + 294, a.getY() + 62, 98);
            knobAt ("arpVelScale", "Dynamics", 40, a.getX() + 392, a.getY() + 62, 98);
            knobAt ("arpChance", "Chance", 40, a.getX() + 490, a.getY() + 62, 98);
            helpA = { a.getX(), a.getY() + 164, a.getWidth(), 30 };

            auto st = panel ("ARP STEPS", 612, 0, 620, 250);
            seq = std::make_unique<SeqEditor> (processor);
            addAndMakeVisible (*seq);
            seq->setBounds (st);

            auto gp = panel ("GATE  \xc2\xb7  CHOPS THE SOUND IN TIME WITH THE SONG", 0, 262, 600, 268);
            toggle ("gateOn", "GATE", { gp.getX(), gp.getY() + 14, 90, 30 });
            choice ("gateRate", "STEP", { gp.getX() + 104, gp.getY(), 110, 44 });
            choice ("gateTarget", "CHOPS", { gp.getX() + 226, gp.getY(), 140, 44 });
            knobAt ("gateDepth", "Depth", 40, gp.getX(), gp.getY() + 62, 110);
            knobAt ("gateSmooth", "Smooth", 40, gp.getX() + 110, gp.getY() + 62, 110);
            helpG = { gp.getX(), gp.getY() + 168, gp.getWidth(), 40 };

            auto gs = panel ("GATE STEPS", 612, 262, 620, 268);
            gate = std::make_unique<GateEditor> (processor);
            addAndMakeVisible (*gate);
            gate->setBounds (gs);
        }

        void tick() override { if (seq != nullptr) seq->tick(); if (gate != nullptr) gate->tick(); }

        void paintExtras (juce::Graphics& g) override
        {
            text (g, "Drag the bars for the strength of each step, right-click = rest. CHANCE leaves notes out, NEW IDEA rolls a variation.", helpA);
            text (g, "Click / drag the steps: lit = sound, dark = silence. DEPTH = how far it closes, SMOOTH = soft or hard edges.", helpG);
        }

    private:
        juce::Rectangle<int> helpA, helpG;
        std::unique_ptr<SeqEditor> seq;
        std::unique_ptr<GateEditor> gate;
    };

    class FxPage : public EditPage
    {
    public:
        static constexpr int cardW = 146, gap = 8, slotH = 400;

        FxPage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            for (int i = 0; i < fx::numSlots; ++i)
            {
                const juce::String n (i + 1), id ("fx" + n);
                auto c = panel ("SLOT " + n, i * (cardW + gap), 0, cardW, slotH);
                auto* t = choice (id + "Type", "EFFECT", { c.getX() - 6, c.getY(), c.getWidth() + 12, 44 });
                t->onChange = [this] { updateLabels(); };
                toggle (id + "On", "ON", { c.getX() + 14, c.getY() + 52, c.getWidth() - 28, 26 });
                aKnobs[(size_t) i] = knobAt (id + "A", "A", 34, c.getX() - 8, c.getY() + 88, (c.getWidth() + 16) / 2);
                bKnobs[(size_t) i] = knobAt (id + "B", "B", 34, c.getX() - 8 + (c.getWidth() + 16) / 2, c.getY() + 88, (c.getWidth() + 16) / 2);
                knobAt (id + "Mix", "Mix", 38, c.getX() - 4, c.getY() + 184, c.getWidth() + 8);
            }
            updateLabels();
            help = { 16, slotH + 24, 1200, 40 };

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
            text (g, juce::String::fromUTF8 ("Slots run left to right, drag a slot by its title to move it. New: PRISM glows on the chord, DISPERSE adds a laser attack, "
                                             "SHAPE morphs from warm to broken, WIDE opens the stereo. Right-click any knob to modulate it."), help);
            for (int i = 0; i < fx::numSlots; ++i)
            {
                const int t = juce::roundToInt (plain (processor.apvts, ("fx" + juce::String (i + 1) + "Type").toRawUTF8()));
                if (t == 12 || t == 13)
                {
                    g.setFont (LookAndFeel::monoFont (10.5f));
                    g.setColour (th().soft);
                    const float a = plain (processor.apvts, ("fx" + juce::String (i + 1) + "A").toRawUTF8());
                    g.drawText (fx::delayName (a), i * (cardW + gap), 40 + 88 + 76, cardW, 14, juce::Justification::centred, false);
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
                g.drawRoundedRectangle (juce::Rectangle<float> ((float) (dragTo * (cardW + gap)), 0.0f, (float) cardW, (float) slotH), 14.0f, 2.0f);
            }
        }

        int slotAt (juce::Point<float> p) const { const int i = (int) (p.x / (cardW + gap)); return p.y < slotH && i >= 0 && i < fx::numSlots ? i : -1; }

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
            auto f = panel ("FILTER & SATURATION  \xc2\xb7  ON THE WHOLE INSTRUMENT", 0, 0, 520, 250);
            knobAt ("filterCutoff", "Cutoff", 60, f.getX(), f.getY() + 4, 160);
            knobAt ("filterReso", "Resonance", 60, f.getX() + 170, f.getY() + 4, 160);
            knobAt ("drive", "Saturation", 60, f.getX() + 340, f.getY() + 4, 160);
            filterHelp = { f.getX(), f.getY() + 140, f.getWidth(), 40 };

            auto m = panel ("MACROS", 532, 0, 700, 250);
            const char* ids[] = { "morph", "macroAge", "macroDream", "macroWobble" };
            const char* labels[] = { "Morph", "Age", "Dream", "Wobble" };
            for (int i = 0; i < 4; ++i)
            {
                knobAt (ids[i], labels[i], 52, m.getX() + i * 175, m.getY() + 4, 150);
                descs[(size_t) i] = { m.getX() + i * 175, m.getY() + 120, 158, 70 };
            }
            models = panel ("CHARACTER  \xc2\xb7  THE COLOUR OF THE SOUND, CLICK TO CHOOSE", 0, 262, 1232, 268);
        }

        juce::Rectangle<float> modelCard (int i) const
        {
            const float w = (models.getWidth() - 32 - 5 * 12.0f) / 6.0f;
            return { models.getX() + 16.0f + i * (w + 12.0f), models.getY() + 36.0f, w, 176.0f };
        }

        void tick() override
        {
            const int m = juce::roundToInt (plain (processor.apvts, "model"));
            if (m != shownModel) { shownModel = m; repaint(); }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < 6; ++i)
                if (modelCard (i).contains (e.position)) { setParam (processor.apvts, "model", (float) i); owner.status ("Character", Hamamatsu108Processor::modelNames()[i]); }
        }

        void paintExtras (juce::Graphics& g) override
        {
            static const char* d[] = {
                "Blends layer A and B (DUAL) and the morph targets.",
                "Time and wear: tape wow, hiss, darker, fewer bits.",
                "Space: a huge soft reverb and longer strings.",
                "Motion: slow pitch drift, like a warm old tape." };
            for (int i = 0; i < 4; ++i) text (g, d[i], descs[(size_t) i], 11.0f);
            text (g, "CUTOFF closes the top like a classic low-pass, RESONANCE adds a peak at the cutoff, SATURATION warms and thickens (tape-like). All three can be modulated: right-click a knob.", filterHelp);

            static const char* era[] = { "1981", "1984", "1989", "1996", "1999", "today" };
            for (int i = 0; i < 6; ++i)
            {
                auto c = modelCard (i);
                const bool on = i == shownModel;
                if (th().concrete)
                {
                    // a phosphor screen per character, the chosen one in the light
                    g.setColour (on ? page::lit() : page::screen());
                    g.fillRect (c);
                    auto in = c.reduced (12.0f);
                    const auto ink = on ? page::litInk() : page::phosphor();
                    g.setFont (LookAndFeel::monoFont (10.0f));
                    g.setColour (on ? page::dim() : page::phosphorDim());
                    g.drawText (era[i], in.removeFromTop (16.0f), juce::Justification::centredLeft, false);
                    g.setFont (LookAndFeel::uiFont (16.0f));
                    g.setColour (ink);
                    g.drawText (Hamamatsu108Processor::modelNames()[i], in.removeFromTop (26.0f), juce::Justification::centredLeft, false);
                    const auto& sp = Hamamatsu108Processor::modelSpec (i);
                    const float vals[] = { sp.cutoff / 20000.0f, juce::jmin (1.0f, sp.width / 1.2f), juce::jmin (1.0f, sp.hiss / 0.0009f), (16.0f - sp.bits) / 8.0f };
                    const char* names[] = { "Bright", "Wide", "Noise", "Grit" };
                    in.removeFromTop (10.0f);
                    for (int k = 0; k < 4; ++k)
                    {
                        auto row = in.removeFromTop (22.0f);
                        g.setFont (LookAndFeel::uiFont (10.0f));
                        g.setColour (on ? page::litInk().withAlpha (0.7f) : page::phosphorDim());
                        g.drawText (names[k], row.removeFromLeft (52.0f), juce::Justification::centredLeft, false);
                        auto bar = row.withSizeKeepingCentre (row.getWidth(), 2.0f);
                        g.setColour (on ? juce::Colour (0x33000000) : page::phosphor().withAlpha (0.12f));
                        g.fillRect (bar);
                        g.setColour (ink);
                        g.fillRect (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * vals[k])));
                    }
                    continue;
                }
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
        juce::Rectangle<int> models, filterHelp;
        int shownModel = -1;
    };

    //==========================================================================
    // PLAY: everything the hand and the switches do, packed on one page
    class PlayPage : public EditPage
    {
    public:
        PlayPage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            // only what the main page does not have (the switches, strokes, envelope and mix live there)
            auto h = panel ("Hand", 0, 0, 470, 276);
            choice ("motionType", "Motion", { h.getX(), h.getY(), 140, 40 });
            choice ("gridRate", "Rate", { h.getX() + 148, h.getY(), 140, 40 });
            choice ("motionGate", "Note length", { h.getX() + 296, h.getY(), 142, 40 });
            choice ("strumZone", "Strum keys", { h.getX(), h.getY() + 50, 140, 40 });
            choice ("keysPlay", "Your keys", { h.getX() + 148, h.getY() + 50, 140, 40 });
            toggle ("gridOn", "Motion", { h.getX() + 296, h.getY() + 64, 142, 26 });
            const char* hk[] = { "gridLen", "strumRate", "arpSwing", "arpChance", "arpVelScale", "arpGate" };
            const char* hn[] = { "Steps", "Speed", "Swing", "Chance", "Dynamics", "Length" };
            for (int i = 0; i < 6; ++i) knobAt (hk[i], hn[i], 34, h.getX() + i * 73, h.getY() + 112, 73);

            auto f = panel ("Stroke feel", 480, 0, 360, 276);
            choice ("strumShape", "Stroke shape", { f.getX(), f.getY(), 160, 40 });
            choice ("strumPattern", "Pattern", { f.getX() + 168, f.getY(), 160, 40 });
            const char* fk[] = { "strumFeel", "strumSpeed", "strumSwing", "strumVary" };
            const char* fn[] = { "Feel", "Speed", "Swing", "Variation" };
            for (int i = 0; i < 4; ++i) knobAt (fk[i], fn[i], 34, f.getX() + i * 82, f.getY() + 62, 82);

            auto k = panel ("Key", 850, 0, 382, 276);
            choice ("keyRoot", "Key", { k.getX(), k.getY(), 170, 40 });
            choice ("keyMode", "Scale", { k.getX() + 180, k.getY(), 170, 40 });
            knobAt ("harpRange", "Range", 34, k.getX(), k.getY() + 62, 88);
            knobAt ("bOctave", "B octave", 34, k.getX() + 88, k.getY() + 62, 88);
            knobAt ("bDetune", "B detune", 34, k.getX() + 176, k.getY() + 62, 88);
            knobAt ("bLevel", "B level", 34, k.getX() + 264, k.getY() + 62, 88);

            auto c = panel ("Chord layer", 0, 286, 610, 244);
            choice ("chordType", "Chord", { c.getX(), c.getY(), 180, 40 });
            knobAt ("chordLevel", "Level", 40, c.getX() + 200, c.getY() - 6, 100);
            knobAt ("chordFilter", "Filter", 40, c.getX() + 300, c.getY() - 6, 100);
            chordHelp = { c.getX(), c.getY() + 110, c.getWidth(), 40 };

            auto bs = panel ("Bass", 622, 286, 610, 244);
            choice ("bassType", "Bass", { bs.getX(), bs.getY(), 180, 40 });
            choice ("bassMode", "Pattern", { bs.getX(), bs.getY() + 50, 180, 40 });
            knobAt ("bassLevel", "Level", 40, bs.getX() + 200, bs.getY() - 6, 100);
            knobAt ("bassTone", "Tone", 40, bs.getX() + 300, bs.getY() - 6, 100);
            bassHelp = { bs.getX(), bs.getY() + 110, bs.getWidth(), 40 };
        }

        void paintExtras (juce::Graphics& g) override
        {
            text (g, "A soft pad of the same chord under the harp, follows every chord you play.", chordHelp);
            text (g, "A bass note under the harp, on the chord root, in the pattern you pick.", bassHelp);
        }

    private:
        juce::Rectangle<int> chordHelp, bassHelp;
    };

    //==========================================================================
    // EDIT: one page, everything the main page does not show, packed tight
    class AllPage : public EditPage
    {
    public:
        AllPage (Hamamatsu108Processor& p, EditView& o) : EditPage (p, o)
        {
            const int rh = 125, g = 8;
            auto cp = [this] (const char* t, int x, int y, int w, int h) { panel (t, x, y, w, h); return juce::Rectangle<int> (x + 10, y + 32, w - 20, h - 38); };
            auto small = [this] (const juce::String& id, const juce::String& label, int x, int y) { return knobAt (id, label, 26, x, y, 56); };

            // row 1: the two engines and the character
            int y = 0;
            auto a = cp ("Layer A", 0, y, 330, rh);
            choice ("harpVoice", "Engine", { a.getX(), a.getY(), 124, 36 });
            choice ("harpOctave", "Octave", { a.getX(), a.getY() + 44, 124, 36 });
            small ("harpTone", "Tone", a.getX() + 130, a.getY());
            small ("harpShape", "Shape", a.getX() + 186, a.getY());
            small ("harpRange", "Range", a.getX() + 242, a.getY());

            auto b = cp ("Layer B", 338, y, 486, rh);
            choice ("harpVoice2", "Engine", { b.getX(), b.getY(), 120, 36 });
            choice ("bOctave", "Octave", { b.getX(), b.getY() + 44, 58, 36 });
            toggle ("dual", "On", { b.getX() + 62, b.getY() + 58, 58, 22 });
            const char* bIds[] = { "bTone", "bShape", "bDetune", "bAttack", "bDecay", "bLevel" };
            const char* bNames[] = { "Tone", "Shape", "Detune", "Attack", "Decay", "Level" };
            for (int i = 0; i < 6; ++i) small (bIds[i], bNames[i], b.getX() + 126 + i * 56, b.getY());

            auto c = cp ("Character", 832, y, 400, rh);
            cards = { c.getX(), c.getY() + 2, 170, 80 };
            small ("filterReso", "Reso", c.getX() + 176, c.getY());
            small ("drive", "Drive", c.getX() + 232, c.getY());
            small ("vibRate", "Vib rate", c.getX() + 288, c.getY());
            small ("vibDepth", "Vib depth", c.getX() + 344 - 6, c.getY());

            // row 2: how the strum feels, the key, the chord layer and the bass
            y += rh + g;
            auto f = cp ("Stroke feel", 0, y, 404, rh);
            choice ("strumShape", "Shape", { f.getX(), f.getY(), 140, 36 });
            choice ("strumPattern", "Pattern", { f.getX(), f.getY() + 44, 140, 36 });
            const char* fk[] = { "strumFeel", "strumSpeed", "strumSwing", "strumVary" };
            const char* fn[] = { "Feel", "Speed", "Swing", "Vary" };
            for (int i = 0; i < 4; ++i) small (fk[i], fn[i], f.getX() + 150 + i * 58, f.getY());

            auto k = cp ("Key and motion", 412, y, 270, rh);
            choice ("keyRoot", "Key", { k.getX(), k.getY(), 78, 36 });
            choice ("keyMode", "Scale", { k.getX() + 84, k.getY(), 78, 36 });
            choice ("motionType", "Motion", { k.getX() + 168, k.getY(), 82, 36 });
            choice ("gridRate", "Rate", { k.getX(), k.getY() + 44, 78, 36 });
            choice ("motionGate", "Notes", { k.getX() + 84, k.getY() + 44, 78, 36 });
            toggle ("gridOn", "Motion", { k.getX() + 168, k.getY() + 58, 82, 22 });

            auto ch = cp ("Chord layer", 690, y, 266, rh);
            choice ("chordType", "Chord", { ch.getX(), ch.getY(), 124, 36 });
            small ("chordLevel", "Level", ch.getX() + 130, ch.getY());
            small ("chordFilter", "Filter", ch.getX() + 186, ch.getY());

            auto bs = cp ("Bass", 964, y, 268, rh);
            choice ("bassType", "Bass", { bs.getX(), bs.getY(), 124, 36 });
            choice ("bassMode", "Pattern", { bs.getX(), bs.getY() + 44, 124, 36 });
            small ("bassLevel", "Level", bs.getX() + 130, bs.getY());
            small ("bassTone", "Tone", bs.getX() + 186, bs.getY());

            // row 3: the steps, the morph targets and the modulation matrix
            y += rh + g;
            auto as = cp ("Arp steps", 0, y, 296, rh);
            seq = std::make_unique<SeqEditor> (processor);
            addAndMakeVisible (*seq);
            seq->setBounds (as);
            auto gs = cp ("Gate steps", 304, y, 236, rh);
            gate = std::make_unique<GateEditor> (processor);
            addAndMakeVisible (*gate);
            gate->setBounds (gs);
            auto mt = cp ("Morph targets", 548, y, 300, rh);
            for (int i = 0; i < mod::morphSlots; ++i)
            {
                const juce::String n (i + 1);
                const int cx = mt.getX() + (i / 4) * 142, cy = mt.getY() + (i % 4) * 20;
                choice ("morph" + n + "Dst", {}, { cx, cy, 108, 18 });
                tiny ("morph" + n + "Amt", cx + 108, cy - 5);
            }
            auto mx = cp ("Modulation", 856, y, 376, rh);
            for (int i = 0; i < mod::matrixSlots; ++i)
            {
                const juce::String n (i + 1);
                const int cx = mx.getX() + (i / 4) * 180, cy = mx.getY() + (i % 4) * 20;
                choice ("mod" + n + "Src", {}, { cx, cy, 60, 18 });
                choice ("mod" + n + "Dst", {}, { cx + 62, cy, 84, 18 });
                tiny ("mod" + n + "Amt", cx + 146, cy - 5);
            }

            // row 4: the eight effect slots, left to right
            y += rh + g;
            auto fxp = cp ("Effects", 0, y, 1232, rh);
            for (int i = 0; i < fx::numSlots; ++i)
            {
                const juce::String n (i + 1), id ("fx" + n);
                const int cx = fxp.getX() + i * 152;
                auto* t = choice (id + "Type", {}, { cx, fxp.getY(), 98, 20 });
                t->onChange = [this] { updateFx(); };
                toggle (id + "On", "On", { cx + 100, fxp.getY(), 42, 20 });
                fxA[(size_t) i] = knobAt (id + "A", "A", 22, cx - 4, fxp.getY() + 24, 48);
                fxB[(size_t) i] = knobAt (id + "B", "B", 22, cx + 44, fxp.getY() + 24, 48);
                knobAt (id + "Mix", "Mix", 22, cx + 92, fxp.getY() + 24, 48);
            }
            updateFx();
        }

        void tick() override
        {
            seq->tick(); gate->tick();
            const int m = juce::roundToInt (plain (processor.apvts, "model"));
            if (m != shownModel) { shownModel = m; repaint (cards); }
            juce::String sig;
            for (int i = 0; i < fx::numSlots; ++i) sig << plain (processor.apvts, ("fx" + juce::String (i + 1) + "Type").toRawUTF8()) << ",";
            if (sig != fxSig) { fxSig = sig; updateFx(); }
        }

        juce::Rectangle<float> card (int i) const
        {
            const float w = (cards.getWidth() - 8.0f) / 2.0f, h = (cards.getHeight() - 8.0f) / 3.0f;
            return { cards.getX() + (i % 2) * (w + 8.0f), cards.getY() + (i / 2) * (h + 4.0f), w, h };
        }

        void paintExtras (juce::Graphics& g) override
        {
            for (int i = 0; i < 6; ++i)
            {
                auto r = card (i);
                const bool on = i == shownModel;
                g.setColour (on ? page::phosphor() : page::screen());
                g.fillRect (r);
                g.setFont (LookAndFeel::monoFont (10.5f));
                g.setColour (on ? page::screen() : page::phosphor().withAlpha (0.8f));
                g.drawText (Hamamatsu108Processor::modelNames()[i], r.reduced (6.0f, 0.0f), juce::Justification::centredLeft, false);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < 6; ++i)
                if (card (i).contains (e.position)) { setParam (processor.apvts, "model", (float) i); owner.status ("Character", Hamamatsu108Processor::modelNames()[i]); }
        }

    private:
        void tiny (const juce::String& id, int x, int y)
        {
            Knob::Style st; st.showValue = false; st.labelHeight = 1.0f;
            auto* k = knobs.add (new Knob (processor.apvts, id, {}, 14, st));
            k->onInteract = [this] (const Knob& kn) { owner.status (kn.getDisplayName(), kn.getValueText()); };
            addAndMakeVisible (k);
            k->setBounds (x, y + 2, 32, 24);
        }
        void updateFx()
        {
            for (int i = 0; i < fx::numSlots; ++i)
            {
                const int t = juce::roundToInt (plain (processor.apvts, ("fx" + juce::String (i + 1) + "Type").toRawUTF8()));
                const auto names = fx::controlNames (t);
                fxA[(size_t) i]->setLabel (names.first);
                fxB[(size_t) i]->setLabel (names.second);
                fxA[(size_t) i]->setEnabled (t != 0);
                fxB[(size_t) i]->setEnabled (t != 0);
            }
        }

        std::unique_ptr<SeqEditor> seq;
        std::unique_ptr<GateEditor> gate;
        std::array<Knob*, fx::numSlots> fxA {}, fxB {};
        juce::Rectangle<int> cards;
        juce::String fxSig;
        int shownModel = -1;
    };

    //==========================================================================
    const juce::StringArray& EditView::tabNames()
    {
        static const juce::StringArray n { "LAYERS", "PLAY", "ARP & GATE", "FILTER", "MOD", "FX" };
        return n;
    }

    EditView::EditView (Hamamatsu108Processor& p) : processor (p)
    {
        logo = juce::Drawable::createFromImageData (KuprellBrand::kuprelllogocream_svg, KuprellBrand::kuprelllogocream_svgSize);

        pages.add (new AllPage (p, *this));   // HARPOLOGY: one EDIT page
        pages.add (new EnginePage (p, *this));
        pages.add (new PlayPage (p, *this));
        pages.add (new ArpGatePage (p, *this));
        pages.add (new CharacterPage (p, *this));   // FILTER: character, filter, saturation, macros
        pages.add (new ModPage (p, *this));
        pages.add (new FxPage (p, *this));
        for (auto* pg : pages) addChildComponent (pg);

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

    juce::Rectangle<float> EditView::toolTile (int i) const { return { 24.0f + (float) i * 84.0f, 14.0f, 78.0f, 26.0f }; }

    juce::Rectangle<int> EditView::tabArea (int i) const
    {
        if (th().concrete)
        {
            // narrow flat tabs, left aligned under the wordmark
            return juce::Rectangle<int> (24 + i * 112, 104, 108, 26);
        }
        const float w = 1232.0f / (float) tabNames().size();
        return juce::Rectangle<float> (24.0f + i * w, 98.0f, w - 6.0f, 34.0f).toNearestInt();
    }

    void EditView::resized()
    {
        for (auto* pg : pages) pg->setBounds (24, 142, 1232, 530);
        pages[0]->setBounds (24, 44, 1232, 524);   // one look: the single EDIT page
    }

    void EditView::mouseDown (const juce::MouseEvent& e)
    {
        for (int i = 0; i < tabNames().size(); ++i)
            if (! th().concrete && tabArea (i).contains (e.getPosition())) showTab (i);
        if (th().concrete)
        {
            const std::function<void()>* acts[] = { &onSave, &onCompare, &onRandom, &onUndo, &onRedo };
            for (int i = 0; i < 5; ++i) if (toolTile (i).contains (e.position) && *acts[i]) (*acts[i])();
        }
    }

    void EditView::timerCallback()
    {
        if (auto* pg = pages[tab]) pg->tick();
    }

    void EditView::paint (juce::Graphics& g)
    {
        if (th().concrete)
        {
            // one cast block under the main header, like settings: dark frame, slab, a row of tools
            g.fillAll (page::theme().hall);
            auto slab = getLocalBounds().toFloat().reduced (8.0f);
            g.setColour (page::theme().slab);
            g.fillRect (slab);
            g.setColour (page::theme().slab.brighter (0.45f));
            g.fillRect (slab.withHeight (2.0f));
            for (int i = 0; i < 5; ++i)
            {
                auto t = toolTile (i);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillRect (t.translated (1.5f, 2.0f));
                g.setColour (juce::Colour (0xff4b4a46));
                g.fillRect (t);
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.fillRect (t.withHeight (1.0f));
                g.setFont (LookAndFeel::uiFont (12.0f));
                g.setColour (page::ink());
                static const char* names[] = { "Save", "A | B", "Random", "Undo", "Redo" };
                g.drawText (names[i], t, juce::Justification::centred, false);
            }
            g.setFont (LookAndFeel::uiFont (12.0f));
            g.setColour (page::ink().withAlpha (0.7f));
            g.drawText (vfdName.isEmpty() ? juce::String() : vfdName + "   " + vfdValue, juce::Rectangle<float> (500.0f, 14.0f, 756.0f, 26.0f), juce::Justification::centredRight, false);
            return;
        }
        g.fillAll (juce::Colour (0xff0b0c0d));

        // metal header plate (top of the instrument)
        auto plate = juce::Rectangle<float> (24.0f, 0.0f, 1232.0f, 86.0f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (plate.translated (0.0f, 8.0f), 26.0f);
        juce::Path pp;
        pp.addRoundedRectangle (plate.getX(), plate.getY(), plate.getWidth(), plate.getHeight(), 26.0f, 26.0f, false, false, true, true);
        const auto& sk = skins::get (skins::currentIndex());
        const bool lightPanel = sk.panelMid.getPerceivedBrightness() > 0.6f;
        juce::ColourGradient metal (lightPanel ? juce::Colour (0xff555b63) : sk.panelTop, 0.0f, plate.getY(),
                                    lightPanel ? juce::Colour (0xff2c3036) : sk.panelBottom, 0.0f, plate.getBottom(), false);
        g.setGradientFill (metal);
        g.fillPath (pp);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (lightPanel ? 0.13f : 0.04f), plate.getX(), plate.getY(), juce::Colours::transparentWhite, plate.getX() + 400.0f, plate.getBottom(), false));
        g.fillPath (pp);

        if (logo != nullptr)
            logo->drawWithin (g, { 52.0f, 24.0f, 100.0f, 16.0f }, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yTop, 1.0f);
        g.setFont (spaced (10.0f, true, 0.4f));
        g.setColour (engrave);
        g.drawText ("HAMAMATSU 144", juce::Rectangle<float> (52.0f, 46.0f, 160.0f, 14.0f), juce::Justification::centredLeft, false);

        // tabs
        for (int i = 0; i < tabNames().size(); ++i)
        {
            auto t = tabArea (i).toFloat();
            const bool on = i == tab;
            g.setColour (on ? juce::Colour (0xff22252a) : juce::Colour (0xff17191c));
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
