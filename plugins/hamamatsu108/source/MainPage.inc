// HARPOLOGY main page: BANK + presets, SWITCHES, the STRUM window bar, LFOs.
// Included into Instrument.cpp (namespace hama): uses its helpers (th(), spaced(), plain(), setParam()).

    // BANK tabs and the presets of that bank, right on the page: click = load and hear it
    class BankPresets : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit BankPresets (Hamamatsu108Processor& p) : processor (p) { fitColumns(); setTooltip ("BANK: pick a bank, click a sound to hear it. Right-click a sound = favourite"); }

        std::function<void (const juce::String&, const juce::String&)> onStatus;

        static const juce::StringArray& banks()
        {
            static const juce::StringArray b { "All", "Harp", "Pluck", "Bell", "Keys", "Pad", "Texture", "Hybrid", "Cinematic", "Favourites", "My sounds" };
            return b;
        }

        void update()
        {
            const int cur = processor.presetManager->getCurrent();
            if (cur != shownCurrent) { shownCurrent = cur; jumpToCurrent(); repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat();
            // tabs
            g.setFont (LookAndFeel::uiFont (11.0f, true));
            for (int i = 0; i < banks().size(); ++i)
            {
                const auto t = tab (i);
                const bool on = i == bank;
                if (on)   // the chosen bank is the one in the light
                {
                    g.setColour (page::lit());
                    g.fillRect (t);
                }
                g.setColour (on ? page::litInk() : page::onSlab());
                g.drawText (banks()[i], t, juce::Justification::centred, false);
            }
            // previous / next sound: two hairline chevrons after the tabs
            for (int k = 0; k < 2; ++k)
            {
                const auto a = stepArrow (k);
                juce::Path p;
                const float dx = k == 0 ? -3.0f : 3.0f;   // k 0 = back (‹), 1 = forward (›)
                p.startNewSubPath (a.getCentreX() - dx, a.getCentreY() - 6.0f);
                p.lineTo (a.getCentreX() + dx, a.getCentreY());
                p.lineTo (a.getCentreX() - dx, a.getCentreY() + 6.0f);
                g.setColour (k == hoverArrow ? page::lit() : page::onSlab());
                g.strokePath (p, juce::PathStrokeType (1.6f));
            }
            // pages
            const int pages = numPages();
            if (pages > 1)
            {
                g.setColour (page::phosphorDim());
                g.setFont (LookAndFeel::monoFont (9.5f));
                g.drawText (juce::String (pageIndex + 1) + " of " + juce::String (pages), pageText(), juce::Justification::centred, false);
                drawArrow (g, prevArrow(), true);
                drawArrow (g, nextArrow(), false);
            }
            // grid of sounds
            const auto grid = gridArea();
            page::paintScreen (g, grid);
            const auto shown = list();
            const int cur = processor.presetManager->getCurrent();
            for (int c = 0; c < cols * page::presetRows; ++c)
            {
                const int idx = pageIndex * cols * page::presetRows + c;
                if (idx >= (int) shown.size()) break;
                const int pi = shown[(size_t) idx];
                const auto cell = cellRect (c);
                const auto& pr = processor.presetManager->all()[(size_t) pi];
                const bool on = pi == cur, hov = c == hover;
                if (on)        { g.setColour (page::phosphor()); g.fillRect (cell); }                       // inverse video
                else if (hov)  { g.setColour (page::phosphor().withAlpha (0.08f)); g.fillRect (cell); }
                if (processor.presetManager->isFavourite (pi))
                {
                    g.setColour (page::led());
                    g.fillEllipse (cell.getX() + 3.0f, cell.getCentreY() - 2.0f, 4.0f, 4.0f);
                }
                g.setFont (LookAndFeel::monoFont (10.5f));
                if (on) { g.setColour (juce::Colour (0xff03100b)); g.drawText (pr.name, cell.withTrimmedLeft (10.0f).withTrimmedRight (30.0f), juce::Justification::centredLeft, true); }
                else page::phosphorText (g, pr.name, cell.withTrimmedLeft (10.0f).withTrimmedRight (30.0f), juce::Justification::centredLeft, page::phosphor());
                if (bank == 0 || bank >= 9)   // the type only where banks are mixed
                {
                    g.setFont (LookAndFeel::monoFont (8.5f));
                    g.setColour (on ? juce::Colour (0xff03100b).withAlpha (0.6f) : page::phosphorDim());
                    g.drawText (pr.category, cell.withTrimmedRight (6.0f), juce::Justification::centredRight, false);
                }
            }
            if (shown.empty())
            {
                g.setColour (page::dim());
                g.setFont (12.0f);
                g.drawText (bank == 9 ? "No favourites yet: right-click a sound" : "Nothing here yet", grid, juce::Justification::centred, false);
            }
            juce::ignoreUnused (r);
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const int h = cellAt (e.position);
            const int ha = stepArrow (0).contains (e.position) ? 0 : stepArrow (1).contains (e.position) ? 1 : -1;
            if (h != hover || ha != hoverArrow) { hover = h; hoverArrow = ha; repaint(); }
        }
        void mouseExit (const juce::MouseEvent&) override { hover = -1; hoverArrow = -1; repaint(); }

        // the next / previous sound of the bank (wraps around), heard right away
        void step (int delta)
        {
            const auto shown = list();
            if (shown.empty()) return;
            int k = 0;
            for (size_t i = 0; i < shown.size(); ++i) if (shown[i] == processor.presetManager->getCurrent()) k = (int) i;
            k = (k + delta + (int) shown.size()) % (int) shown.size();
            processor.presetManager->load (shown[(size_t) k]);
            processor.previewPreset();
            if (onStatus) onStatus ("Sound", processor.presetManager->currentName());
            repaint();
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < banks().size(); ++i)
                if (tab (i).contains (e.position)) { bank = i; pageIndex = 0; fitColumns(); repaint(); if (onStatus) onStatus ("Bank", banks()[i]); return; }
            for (int k = 0; k < 2; ++k)
                if (stepArrow (k).contains (e.position)) { step (k == 0 ? -1 : 1); return; }
            if (numPages() > 1)
            {
                if (prevArrow().expanded (4.0f).contains (e.position)) { pageIndex = (pageIndex + numPages() - 1) % numPages(); repaint(); return; }
                if (nextArrow().expanded (4.0f).contains (e.position)) { pageIndex = (pageIndex + 1) % numPages(); repaint(); return; }
            }
            const int c = cellAt (e.position);
            const auto shown = list();
            const int idx = pageIndex * cols * page::presetRows + c;
            if (c < 0 || idx >= (int) shown.size()) return;
            const int pi = shown[(size_t) idx];
            auto& pm = *processor.presetManager;
            if (e.mods.isPopupMenu()) { pm.toggleFavourite (pi); repaint(); return; }
            pm.load (pi);
            processor.previewPreset();   // hear it right away
            if (onStatus) onStatus ("Sound", pm.currentName());
            repaint();
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            if (numPages() < 2) return;
            pageIndex = juce::jlimit (0, numPages() - 1, pageIndex + (w.deltaY < 0 ? 1 : -1));
            repaint();
        }

    private:
        // columns follow the size of the bank, so the screen is always full: 40 sounds = 4 wide columns, ALL = 6 and pages
        int cols = 6;
        void fitColumns() { cols = juce::jlimit (3, 6, ((int) list().size() + page::presetRows - 1) / page::presetRows); }

        std::vector<int> list() const
        {
            std::vector<int> out;
            const auto& all = processor.presetManager->all();
            for (int i = 0; i < (int) all.size(); ++i)
            {
                const auto& p = all[(size_t) i];
                const bool ok = bank == 0 ? true
                              : bank <= 8 ? (p.factory && p.category.equalsIgnoreCase (presets::categories()[bank - 1]))
                              : bank == 9 ? processor.presetManager->isFavourite (i)
                              : ! p.factory;
                if (ok) out.push_back (i);
            }
            return out;
        }
        int numPages() const { return juce::jmax (1, ((int) list().size() + cols * page::presetRows - 1) / (cols * page::presetRows)); }

        void jumpToCurrent()
        {
            const auto shown = list();
            for (size_t k = 0; k < shown.size(); ++k)
                if (shown[k] == shownCurrent) { pageIndex = (int) k / (cols * page::presetRows); return; }
        }

        juce::Rectangle<float> tab (int i) const
        {
            float x = (float) page::tabsX;
            for (int k = 0; k < i; ++k) x += tabW (k) + 2.0f;
            return { x, 0.0f, tabW (i), (float) page::bankH };
        }
        static float tabW (int i) { return 12.0f + 6.2f * (float) banks()[i].length(); }
        // pages: small, at the bottom-right corner of the screen
        juce::Rectangle<float> pageText() const { const auto g = gridArea(); return { g.getRight() - 74.0f, g.getBottom() - 15.0f, 48.0f, 12.0f }; }
        juce::Rectangle<float> prevArrow() const { const auto t = pageText(); return { t.getX() - 10.0f, t.getY() + 2.0f, 8.0f, 8.0f }; }
        juce::Rectangle<float> nextArrow() const { const auto t = pageText(); return { t.getRight() + 2.0f, t.getY() + 2.0f, 8.0f, 8.0f }; }
        void drawArrow (juce::Graphics& g, juce::Rectangle<float> r, bool left) const
        {
            juce::Path p;
            if (left) p.addTriangle (r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getCentreY());
            else      p.addTriangle (r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY());
            g.setColour (page::phosphor());
            g.fillPath (p);
        }
        juce::Rectangle<float> stepArrow (int k) const
        {
            // between the name and the tabs: ‹ ›
            return { (float) page::tabsX - 56.0f + (float) k * 24.0f, 0.0f, 22.0f, (float) page::bankH };
        }
        juce::Rectangle<float> gridArea() const { return { 0.0f, (float) (page::presetY - page::bankY), (float) getWidth(), (float) page::presetH }; }
        juce::Rectangle<float> cellRect (int c) const
        {
            const auto gr = gridArea().reduced (8.0f, 4.0f);
            const float cw = gr.getWidth() / (float) cols;
            const int col = c % cols, row = c / cols;
            return { gr.getX() + cw * (float) col + 1.0f, gr.getY() + (float) (row * page::presetRowH), cw - 2.0f, (float) page::presetRowH };
        }
        int cellAt (juce::Point<float> p) const
        {
            for (int c = 0; c < cols * page::presetRows; ++c) if (cellRect (c).contains (p)) return c;
            return -1;
        }

        Hamamatsu108Processor& processor;
        int bank = 1, pageIndex = 0, hover = -1, hoverArrow = -1, shownCurrent = -2;
    };

    // SWITCHES: one touch changes the sound or the playing; the small knob sets how much
    class SwitchRow : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit SwitchRow (Hamamatsu108Processor& p) : processor (p) {}
        std::function<void (const juce::String&, const juce::String&)> onStatus;

        void update()
        {
            int sig = 0;
            for (int i = 0; i < Hamamatsu108Processor::numSwitches; ++i)
                sig = sig * 31 + (on (i) ? 1 : 0) + juce::roundToInt (amt (i) * 100.0f) * 2;
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            for (int i = 0; i < Hamamatsu108Processor::numSwitches; ++i)
            {
                const auto t = tile (i);
                const bool o = on (i);
                // a block of concrete; switched on = it stands in the light
                g.setColour (o ? page::lit() : juce::Colour (0xff4a4946));
                g.fillRect (t);
                g.setColour (juce::Colour (0xff2a2927));
                g.fillRect (t.withTop (t.getBottom() - 2.0f));
                g.fillRect (t.withLeft (t.getRight() - 1.0f));
                g.setColour (o ? juce::Colours::white : juce::Colour (0xff6c6a66));
                g.fillRect (t.withHeight (1.0f));
                // LED
                const auto ledR = juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ t.getCentreX(), t.getY() + 7.0f });
                g.setColour (o ? page::led() : juce::Colour (0xff2e2d2b));
                g.fillEllipse (ledR);
                // name
                g.setFont (LookAndFeel::uiFont (9.0f, true));
                g.setColour (o ? page::litInk() : page::onSlab());
                g.drawFittedText (niceName (i), t.withTrimmedTop (14.0f).withHeight (12.0f).toNearestInt(), juce::Justification::centred, 1, 0.75f);
                // amount knob
                const auto k = knobRect (i);
                const float a = amt (i);
                const float start = juce::MathConstants<float>::pi * 1.25f, span = juce::MathConstants<float>::pi * 1.5f;
                juce::Path track, val;
                track.addCentredArc (k.getCentreX(), k.getCentreY(), k.getWidth() * 0.5f, k.getWidth() * 0.5f, 0.0f, -start * 0.6f - 0.3f, start * 0.6f + 0.3f, true);
                g.setColour ((o ? page::litInk() : juce::Colours::black).withAlpha (0.18f));
                g.strokePath (track, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                const float a0 = -span * 0.5f, a1 = a0 + span * a;
                val.addCentredArc (k.getCentreX(), k.getCentreY(), k.getWidth() * 0.5f, k.getWidth() * 0.5f, 0.0f, a0, a1, true);
                g.setColour (o ? page::litInk() : page::onSlab());
                g.strokePath (val, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (o ? juce::Colour (0xff3a3936) : juce::Colour (0xff2c2b29));
                g.fillEllipse (k.reduced (4.0f));
                const float ang = a0 + span * a;
                g.setColour (o ? page::lit() : page::onSlab());
                g.drawLine (k.getCentreX(), k.getCentreY(), k.getCentreX() + std::sin (ang) * (k.getWidth() * 0.5f - 5.0f), k.getCentreY() - std::cos (ang) * (k.getWidth() * 0.5f - 5.0f), 1.6f);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragging = -1;
            for (int i = 0; i < Hamamatsu108Processor::numSwitches; ++i)
            {
                if (knobRect (i).expanded (6.0f).contains (e.position)) { dragging = i; dragStart = amt (i); return; }
                if (tile (i).contains (e.position))
                {
                    setParam (processor.apvts, "sw" + juce::String (i) + "On", on (i) ? 0.0f : 1.0f);
                    say (i);
                    repaint();
                    return;
                }
            }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragging < 0) return;
            const float v = juce::jlimit (0.0f, 1.0f, dragStart - (float) e.getDistanceFromDragStartY() / 120.0f);
            setParam (processor.apvts, "sw" + juce::String (dragging) + "Amt", v);
            if (! on (dragging)) setParam (processor.apvts, "sw" + juce::String (dragging) + "On", 1.0f);
            say (dragging);
            repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < Hamamatsu108Processor::numSwitches; ++i)
                if (knobRect (i).expanded (6.0f).contains (e.position)) { setParam (processor.apvts, "sw" + juce::String (i) + "Amt", 0.6f); repaint(); }
        }
        void mouseMove (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < Hamamatsu108Processor::numSwitches; ++i)
                if (tile (i).contains (e.position))
                {
                    setTooltip (Hamamatsu108Processor::switchNames()[i] + ": " + Hamamatsu108Processor::switchHelp()[i] + ". Click = on / off, knob = how much");
                    return;
                }
        }

    private:
        static juce::String niceName (int i) { auto n = Hamamatsu108Processor::switchNames()[i].toLowerCase(); return n.substring (0, 1).toUpperCase() + n.substring (1); }
        bool on (int i) const { return processor.apvts.getRawParameterValue ("sw" + juce::String (i) + "On")->load() > 0.5f; }
        float amt (int i) const { return processor.apvts.getRawParameterValue ("sw" + juce::String (i) + "Amt")->load(); }
        void say (int i) { if (onStatus) onStatus (Hamamatsu108Processor::switchNames()[i] + (on (i) ? juce::String ("  on") : juce::String ("  off")), juce::String (juce::roundToInt (amt (i) * 100.0f)) + " %  " + Hamamatsu108Processor::switchHelp()[i]); }
        juce::Rectangle<float> tile (int i) const
        {
            const int n = Hamamatsu108Processor::numSwitches;
            const float gap = 3.0f, w = ((float) getWidth() - gap * (float) (n - 1)) / (float) n;
            return { (float) i * (w + gap), 0.0f, w, (float) getHeight() };
        }
        juce::Rectangle<float> knobRect (int i) const { const auto t = tile (i); return juce::Rectangle<float> (22.0f, 22.0f).withCentre ({ t.getCentreX(), t.getBottom() - 20.0f }); }

        Hamamatsu108Processor& processor;
        int dragging = -1, lastSig = 0;
        float dragStart = 0.0f;
    };

    // the bar on top of the STRUM window: ARP, GATE, FLOURISH and quick MOVES for the strum
    class StrumBar : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit StrumBar (Hamamatsu108Processor& p) : processor (p) {}
        std::function<void (const juce::String&, const juce::String&)> onStatus;
        std::function<void (bool)> onNotes;
        bool notesOpen = false;

        void update()
        {
            int sig = (arpOn() ? 1 : 0) + (gateOn() ? 2 : 0) + (flourishOn() ? 4 : 0);
            for (int i = 0; i < numMoves; ++i) if (plain (processor.apvts, moveIds()[i]) > 0.5f) sig |= 8 << i;
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            g.setFont (LookAndFeel::uiFont (13.0f, true));
            g.setColour (page::ink());
            g.drawText ("Strum", juce::Rectangle<float> (0.0f, 0.0f, 66.0f, (float) getHeight()), juce::Justification::centredLeft, false);
            const char* names[] = { "Arp", "Gate", "Flourish" };
            const bool st[] = { arpOn(), gateOn(), flourishOn() };
            for (int i = 0; i < 3; ++i) chip (g, button (i), names[i], st[i], true);
            chip (g, notesChip(), "Notes", notesOpen, true);
            for (int i = 0; i < numMoves; ++i) chip (g, moveChip (i), moveNames()[i], plain (processor.apvts, moveIds()[i]) > 0.5f, true);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            auto& s = processor.apvts;
            if (notesChip().contains (e.position))
            {
                notesOpen = ! notesOpen;
                if (onNotes) onNotes (notesOpen);
                if (onStatus) onStatus ("Notes", notesOpen ? "click a note to add it or take it out, right-click = root" : "off");
            }
            else if (button (0).contains (e.position))
            {
                const bool on = ! arpOn();
                setParam (s, "strumMode", on ? 1.0f : 3.0f);
                if (on) setParam (s, "gridOn", 0.0f);   // ARP plays the held chord; MOTION would play over it
                if (onStatus) onStatus ("ARP", on ? "on: hold a chord, the harp plays it as an arpeggio (EDIT > ARP & GATE for more)" : "off");
            }
            else if (button (1).contains (e.position))
            {
                setParam (s, "gateOn", gateOn() ? 0.0f : 1.0f);
                if (onStatus) onStatus ("GATE", gateOn() ? "on: the sound is chopped in rhythm" : "off");
            }
            else if (button (2).contains (e.position))
            {
                const bool on = flourishOn();
                setParam (s, "keysPlay", on ? 6.0f : 4.0f);
                if (onStatus) onStatus ("FLOURISH", on ? "off" : "on: one key = a whole run of the chord in the key");
            }
            else
                for (int i = 0; i < numMoves; ++i)
                    if (moveChip (i).contains (e.position))
                    {
                        const bool now = plain (s, moveIds()[i]) < 0.5f;
                        setParam (s, moveIds()[i], now ? 1.0f : 0.0f);
                        static const char* help[] = { "the stroke goes from the top down", "only half of the strings", "every chord the other way: up, down, up ...",
                                                      "quick strokes up and down that fade (tremolo)", "from the middle out to both sides", "slow, wide strokes" };
                        if (onStatus) onStatus ("Strum " + moveNames()[i] + (now ? "  on" : "  off"), help[i]);
                    }
            repaint();
        }

        static constexpr int numMoves = 6;
        static const juce::StringArray& moveNames() { static const juce::StringArray n { "Down", "Half", "Bounce", "Waver", "Middle", "Half time" }; return n; }
        static const char* const* moveIds() { static const char* ids[] = { "strokeDown", "strokeHalf", "strokeBounce", "strokeWaver", "strokeMiddle", "strokeSlow" }; return ids; }

    private:
        // a MOVE draws a finger path into LINES and starts it
        void applyMove (int m)
        {
            auto& s = processor.apvts;
            for (int i = 0; i < prog::linePoints; ++i)
            {
                const float t = (float) i / (float) (prog::linePoints - 1);
                float v = 0.0f;
                switch (m)
                {
                    case 0: v = std::fmod (t * 4.0f, 1.0f); break;                                              // rise, every beat
                    case 1: v = 1.0f - std::fmod (t * 4.0f, 1.0f); break;                                       // fall
                    case 2: v = 0.5f * std::fmod (t * 8.0f, 1.0f) + (std::fmod (t * 2.0f, 1.0f) < 0.5f ? 0.0f : 0.5f); break;   // only half the strings, then the other half
                    case 3: v = std::fmod (t * 8.0f, 2.0f) < 1.0f ? 0.05f : 0.95f; break;                       // jump side to side
                    case 4: v = 0.5f + 0.42f * std::sin (t * juce::MathConstants<float>::twoPi * 2.0f); break;  // a slow wave
                    default: v = 0.5f + 0.45f * std::sin (t * juce::MathConstants<float>::twoPi * 6.0f) * (1.0f - t * 0.7f); break;   // spiral in
                }
                processor.motionLine[(size_t) i].store (juce::jlimit (0.0f, 1.0f, v));
            }
            setParam (s, "motionType", 1.0f);
            setParam (s, "gridOn", 1.0f);
        }

        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, bool led) const
        {
            g.setColour (on ? page::lit() : juce::Colour (0xff2a2928));
            g.fillRect (r);
            auto tr = r;
            if (led)
            {
                g.setColour (on ? page::led() : juce::Colour (0xff3d3c39));
                g.fillEllipse (r.getX() + 8.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
                tr = r.withTrimmedLeft (12.0f);
            }
            g.setFont (LookAndFeel::uiFont (10.5f, true));
            g.setColour (on ? page::litInk() : page::ink());
            g.drawText (text, tr, juce::Justification::centred, false);
        }
        juce::Rectangle<float> button (int i) const { const float w[] = { 52.0f, 56.0f, 78.0f }, x[] = { 62.0f, 118.0f, 178.0f }; return { x[i], 2.0f, w[i], (float) getHeight() - 4.0f }; }
        juce::Rectangle<float> notesChip() const { return { 270.0f, 2.0f, 64.0f, (float) getHeight() - 4.0f }; }
        juce::Rectangle<float> moveChip (int i) const
        {
            float x = (float) getWidth();
            for (int k = numMoves - 1; k >= i; --k) x -= (24.0f + 6.2f * (float) moveNames()[k].length()) + 4.0f;
            return { x, 2.0f, 24.0f + 6.2f * (float) moveNames()[i].length(), (float) getHeight() - 4.0f };
        }
        bool arpOn() const { return juce::roundToInt (plain (processor.apvts, "strumMode")) == 1; }
        bool gateOn() const { return plain (processor.apvts, "gateOn") > 0.5f; }
        bool flourishOn() const { const int k = juce::roundToInt (plain (processor.apvts, "keysPlay")); return k == 4 || k == 5; }

        Hamamatsu108Processor& processor;
        int lastSig = -1;
    };

    // one LFO on the main page: rate, depth, shape and what it moves
    class LfoPanel : public juce::Component
    {
    public:
        LfoPanel (Hamamatsu108Processor& p, int index) : processor (p), id ("lfo" + juce::String (index + 1)), title ("LFO " + juce::String (index + 1))
        {
            Knob::Style st;
            st.ring = page::lit(); st.themed = 0; st.pointer = page::lit();
            st.track = juce::Colours::white.withAlpha (0.08f);
            st.label = page::dim(); st.value = page::ink();
            st.labelHeight = 10.0f;
            rate = std::make_unique<Knob> (p.apvts, id + "Rate", "Rate", 28, st);
            depth = std::make_unique<Knob> (p.apvts, id + "QAmt", "Depth", 28, st);
            addAndMakeVisible (*rate);
            addAndMakeVisible (*depth);
        }

        void resized() override
        {
            rate->setBounds (4, 6, 60, 66);
            depth->setBounds (62, 6, 60, 66);
        }

        void update()
        {
            const int sig = juce::roundToInt (plain (processor.apvts, (id + "QDst").toRawUTF8())) * 16 + juce::roundToInt (plain (processor.apvts, (id + "Shape").toRawUTF8()));
            if (sig != lastSig) { lastSig = sig; repaint(); }
            // the lamp follows the wave, so you see it run
            const float v = processor.modSources[(size_t) (mod::lfo1 + (id == "lfo2" ? 1 : 0))].load();
            if (std::abs (v - lampValue) > 0.02f) { lampValue = v; repaint (lamp().toNearestInt().expanded (2)); }
        }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff262624));
            g.fillRect (r);
            g.setFont (LookAndFeel::uiFont (11.5f));
            g.setColour (page::dim());
            g.drawText (title, juce::Rectangle<float> (126.0f, (float) getHeight() - 16.0f, 80.0f, 12.0f), juce::Justification::centredLeft, false);
            g.setColour (page::led().withAlpha (0.15f + 0.85f * (0.5f + 0.5f * lampValue)));
            g.fillEllipse (lamp());
            chip (g, targetChip(), "To " + mod::targetNames()[juce::roundToInt (plain (processor.apvts, (id + "QDst").toRawUTF8()))]);
            chip (g, shapeChip(), mod::lfoShapeNames()[juce::roundToInt (plain (processor.apvts, (id + "Shape").toRawUTF8()))]);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (targetChip().contains (e.position))
            {
                juce::PopupMenu m;
                const int cur = juce::roundToInt (plain (processor.apvts, (id + "QDst").toRawUTF8()));
                for (int i = 0; i < mod::targetNames().size(); ++i) m.addItem (1 + i, mod::targetNames()[i], true, i == cur);
                showMenu (m, targetChip(), id + "QDst");
            }
            else if (shapeChip().contains (e.position))
            {
                juce::PopupMenu m;
                const int cur = juce::roundToInt (plain (processor.apvts, (id + "Shape").toRawUTF8()));
                for (int i = 0; i < mod::lfoShapeNames().size(); ++i) m.addItem (1 + i, mod::lfoShapeNames()[i], true, i == cur);
                showMenu (m, shapeChip(), id + "Shape");
            }
        }

    private:
        void showMenu (juce::PopupMenu& m, juce::Rectangle<float> r, const juce::String& pid)
        {
            juce::Component::SafePointer<LfoPanel> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (r.toNearestInt())),
                             [safe, pid] (int res)
                             {
                                 if (safe == nullptr || res <= 0) return;
                                 setParam (safe->processor.apvts, pid, (float) (res - 1));
                                 // a new target with no depth would do nothing: give it a clear amount
                                 if (pid.endsWith ("QDst") && res > 1)
                                 {
                                     const auto amt = pid.dropLastCharacters (3) + "Amt";
                                     if (std::abs (plain (safe->processor.apvts, amt.toRawUTF8())) < 0.05f) setParam (safe->processor.apvts, amt, 0.4f);
                                 }
                                 safe->repaint();
                             });
        }
        void chip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text) const
        {
            g.setColour (page::screen());
            g.fillRect (r);
            g.setFont (LookAndFeel::monoFont (11.0f));
            g.setColour (page::phosphor());
            g.drawFittedText (text + juce::String::fromUTF8 ("  \xe2\x96\xbe"), r.toNearestInt().reduced (4, 0), juce::Justification::centredLeft, 1, 0.8f);
        }
        juce::Rectangle<float> lamp() const { return { 172.0f, (float) getHeight() - 13.0f, 6.0f, 6.0f }; }
        float lampValue = 0.0f;
        juce::Rectangle<float> targetChip() const { return { 126.0f, 8.0f, (float) getWidth() - 134.0f, 20.0f }; }
        juce::Rectangle<float> shapeChip() const  { return { 126.0f, 32.0f, (float) getWidth() - 134.0f, 20.0f }; }

        Hamamatsu108Processor& processor;
        juce::String id, title;
        std::unique_ptr<Knob> rate, depth;
        int lastSig = -1;
    };

    //==========================================================================
    // NOTES: a strip at the bottom of the strum window. The chord on the strings, every note around it:
    // click to add or take out a note (the strum plays it at once), right-click = root.
    class NotesStrip : public juce::Component
    {
    public:
        explicit NotesStrip (Hamamatsu108Processor& p) : processor (p) {}
        std::function<void (const juce::String&, const juce::String&)> onStatus;

        void update()
        {
            const int sig = processor.stringsNow.load() * 2 + (processor.stringsPick.load() >= 0 ? 1 : 0);
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (page::screen().withAlpha (0.95f));
            g.fillRect (r);
            g.setColour (page::phosphorDim().withAlpha (0.6f));
            g.fillRect (r.withHeight (1.0f));
            const int chord = processor.stringsNow.load();
            const int mask = chord >= 0 ? chords::pitchClassMask (chord) : 0;
            const int root = chord >= 0 ? chords::rootOf (chord) : -1;
            const int key = chords::scaleMask (juce::roundToInt (plain (processor.apvts, "keyRoot")), plain (processor.apvts, "keyMode") > 0.5f);

            g.setFont (LookAndFeel::monoFont (20.0f));
            page::phosphorText (g, chords::shortName (chord), juce::Rectangle<float> (14.0f, 0.0f, 120.0f, r.getHeight() - 10.0f), juce::Justification::centredLeft, page::phosphor());
            g.setFont (LookAndFeel::monoFont (9.5f));
            g.setColour (page::phosphorDim());
            g.drawText (processor.stringsPick.load() >= 0 ? "your pick" : "from your keys", juce::Rectangle<float> (14.0f, r.getHeight() - 16.0f, 120.0f, 12.0f), juce::Justification::centredLeft, false);

            for (int i = 0; i < 12; ++i)
            {
                auto c = cell (i);
                const int pc = i;   // fixed C..B, so a note stays where it is while you edit
                const bool on = (mask >> pc) & 1, inKey = (key >> pc) & 1;
                if (on) { g.setColour (page::phosphor()); g.fillRect (c); }
                else    { g.setColour (inKey ? page::phosphor().withAlpha (0.12f) : page::phosphor().withAlpha (0.05f)); g.fillRect (c); }
                g.setFont (LookAndFeel::monoFont (13.0f));
                g.setColour (on ? page::screen() : inKey ? page::phosphor().withAlpha (0.75f) : page::phosphorDim());
                g.drawText (chords::rootName (pc), c, juce::Justification::centred, false);
                if (pc == root) { g.setColour (on ? page::screen() : page::phosphor()); g.fillRect (c.getX() + 8.0f, c.getBottom() - 4.0f, c.getWidth() - 16.0f, 2.0f); }
            }
            const char* acts[] = { "Strum", "Another", "Keys" };
            for (int i = 0; i < 3; ++i)
            {
                auto b = action (i);
                g.setColour (page::phosphorDim());
                g.drawRect (b, 1.0f);
                g.setFont (LookAndFeel::uiFont (11.0f));
                g.setColour (page::phosphor());
                g.drawText (acts[i], b, juce::Justification::centred, false);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int chord = processor.stringsNow.load();
            if (chord < 0) return;
            int mask = chords::pitchClassMask (chord), root = chords::rootOf (chord);
            for (int i = 0; i < 12; ++i)
                if (cell (i).contains (e.position))
                {
                    const int pc = i;
                    if (e.mods.isPopupMenu()) { mask |= 1 << pc; root = pc; }
                    else if ((mask >> pc) & 1)
                    {
                        if (juce::countNumberOfBits ((uint32_t) mask) <= 2) return;   // a strum needs two notes at least
                        mask &= ~(1 << pc);
                        if (pc == root) for (int k = 1; k < 12; ++k) if ((mask >> ((root + k) % 12)) & 1) { root = (root + k) % 12; break; }
                    }
                    else mask |= 1 << pc;
                    pick (root, mask);
                    return;
                }
            if (action (0).contains (e.position)) processor.strumNow (true);
            else if (action (1).contains (e.position))
            {
                // another colour: one note (not the root) moves to a free note of the key
                const int key = chords::scaleMask (juce::roundToInt (plain (processor.apvts, "keyRoot")), plain (processor.apvts, "keyMode") > 0.5f);
                std::vector<int> out, in;
                for (int pc = 0; pc < 12; ++pc)
                {
                    if (((mask >> pc) & 1) && pc != root) out.push_back (pc);
                    if (! ((mask >> pc) & 1) && ((key >> pc) & 1)) in.push_back (pc);
                }
                if (out.empty() || in.empty()) return;
                mask &= ~(1 << out[(size_t) rnd.nextInt ((int) out.size())]);
                mask |= 1 << in[(size_t) rnd.nextInt ((int) in.size())];
                pick (root, mask);
            }
            else if (action (2).contains (e.position))
            {
                processor.stringsPick.store (-1);
                if (onStatus) onStatus ("Notes", "the strings follow your keys again");
                repaint();
            }
        }

    private:
        void pick (int root, int mask)
        {
            int rel = 0;
            for (int i = 0; i < 12; ++i) if ((mask >> ((root + i) % 12)) & 1) rel |= 1 << i;
            int chord = chords::customOf (root, rel);
            for (int t = 0; t < chords::numTypes; ++t)
                if (chords::pitchClassMask (chords::index (root, t)) == mask) { chord = chords::index (root, t); break; }
            processor.stringsPick.store (chord);
            processor.stringsNow.store (chord);
            processor.strumNow (true);   // hear it at once
            if (onStatus) onStatus ("Notes", chords::shortName (chord));
            repaint();
        }
        juce::Rectangle<float> cell (int i) const { return { 150.0f + (float) i * 46.0f, 8.0f, 42.0f, (float) getHeight() - 16.0f }; }
        juce::Rectangle<float> action (int i) const { return { (float) getWidth() - 3.0f * 78.0f + (float) i * 78.0f - 8.0f, 10.0f, 72.0f, (float) getHeight() - 20.0f }; }

        Hamamatsu108Processor& processor;
        juce::Random rnd;
        int lastSig = -99;
    };

    //==========================================================================
    // MODE STRIP: when ARP, GATE or FLOURISH is on, its settings sit in a thin bar at the bottom of the strum window
    class ModeStrip : public juce::Component
    {
    public:
        explicit ModeStrip (Hamamatsu108Processor& p) : processor (p)
        {
            Knob::Style st;
            st.ring = page::lit(); st.themed = 0; st.pointer = page::lit();
            st.track = juce::Colours::white.withAlpha (0.08f);
            st.label = page::dim(); st.value = page::ink();
            st.labelHeight = 9.0f; st.showValue = false;
            auto knob = [&] (std::vector<Knob*>& into, const char* id, const char* label)
            {
                auto* k = knobs.add (new Knob (p.apvts, id, label, 22, st));
                k->onInteract = [this] (const Knob& kn) { if (onStatus) onStatus (kn.getDisplayName(), kn.getValueText()); };
                addChildComponent (k);
                into.push_back (k);
            };
            auto choice = [&] (std::vector<ChoiceBox*>& into, const char* id)
            {
                auto* c = choices.add (new ChoiceBox (p.apvts, id, {}));
                c->onInteract = [this] (const juce::String& n, const juce::String& v) { if (onStatus) onStatus (n, v); };
                addChildComponent (c);
                into.push_back (c);
            };
            choice (arpChoices, "arpPattern");
            choice (arpChoices, "arpRegen");
            knob (arpKnobs, "strumRate", "Rate");
            knob (arpKnobs, "arpOctaves", "Oct");
            knob (arpKnobs, "arpGate", "Length");
            knob (arpKnobs, "arpSwing", "Swing");
            knob (arpKnobs, "arpChance", "Chance");
            choice (gateChoices, "gateRate");
            choice (gateChoices, "gateTarget");
            knob (gateKnobs, "gateDepth", "Depth");
            knob (gateKnobs, "gateSmooth", "Smooth");
            knob (flourishKnobs, "strumRate", "Speed");
        }

        std::function<void (const juce::String&, const juce::String&)> onStatus;

        bool arp() const      { return juce::roundToInt (plain (processor.apvts, "strumMode")) == 1; }
        bool gate() const     { return plain (processor.apvts, "gateOn") > 0.5f; }
        bool flourish() const { const int k = juce::roundToInt (plain (processor.apvts, "keysPlay")); return k == 4 || k == 5; }
        bool anyOn() const    { return arp() || gate() || flourish(); }

        void update()
        {
            const int sig = (arp() ? 1 : 0) | (gate() ? 2 : 0) | (flourish() ? 4 : 0);
            if (sig != lastSig) { lastSig = sig; resized(); repaint(); }
            const auto g = juce::String ((juce::int64) processor.gateMask.load()) + "|" + juce::String (processor.gateStepGui.load());
            if (gate() && g != lastGate) { lastGate = g; repaint (gateCells.toNearestInt()); }
        }

        void resized() override
        {
            float x = 10.0f;
            const float h = (float) getHeight();
            auto place = [&] (bool on, const char*, std::vector<ChoiceBox*>& cs, std::vector<Knob*>& ks, float& start)
            {
                for (auto* c : cs) c->setVisible (on);
                for (auto* k : ks) k->setVisible (on);
                if (! on) return;
                start = x;
                x += 50.0f;   // the section name
                for (auto* c : cs) { c->setBounds ((int) x, (int) (h * 0.5f - 13.0f), 80, 26); x += 84.0f; }
                for (auto* k : ks) { k->setBounds ((int) x, 4, 38, (int) h - 8); x += 38.0f; }
                x += 8.0f;
            };
            place (arp(), "Arp", arpChoices, arpKnobs, arpX);
            place (gate(), "Gate", gateChoices, gateKnobs, gateX);
            gateCells = {};
            if (gate()) { gateCells = { x, 10.0f, 16.0f * 12.0f, h - 20.0f }; x += gateCells.getWidth() + 18.0f; }
            place (flourish(), "Flourish", noChoices, flourishKnobs, flourishX);
            flourishDir = {};
            if (flourish()) flourishDir = { flourishX + 50.0f + 40.0f, h * 0.5f - 13.0f, 96.0f, 26.0f };
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (page::screen().withAlpha (0.95f));
            g.fillRect (r);
            g.setColour (page::phosphorDim().withAlpha (0.6f));
            g.fillRect (r.withHeight (1.0f));
            g.setFont (LookAndFeel::uiFont (12.0f));
            auto title = [&] (bool on, float x, const char* t)
            {
                if (! on) return;
                g.setColour (page::phosphor());
                g.drawText (t, juce::Rectangle<float> (x, 0.0f, 48.0f, r.getHeight()), juce::Justification::centredLeft, false);
            };
            title (arp(), arpX, "Arp");
            title (gate(), gateX, "Gate");
            title (flourish(), flourishX, "Flourish");
            if (gate())
            {
                const auto mask = processor.gateMask.load();
                const int playing = processor.gateStepGui.load();
                for (int i = 0; i < 16; ++i)
                {
                    auto c = gateCell (i);
                    const bool on = (mask >> i) & 1;
                    g.setColour (on ? page::phosphor().withAlpha (i == playing ? 1.0f : 0.75f) : page::phosphor().withAlpha (i == playing ? 0.25f : 0.08f));
                    g.fillRect (c);
                }
            }
            if (flourish())
            {
                const bool up = juce::roundToInt (plain (processor.apvts, "keysPlay")) == 4;
                for (int i = 0; i < 2; ++i)
                {
                    auto b = flourishDir.withWidth (46.0f).translated (i * 50.0f, 0.0f);
                    const bool on = (i == 0) == up;
                    g.setColour (on ? page::phosphor() : page::phosphor().withAlpha (0.1f));
                    g.fillRect (b);
                    g.setColour (on ? page::screen() : page::phosphor());
                    g.drawText (i == 0 ? "Up" : "Down", b, juce::Justification::centred, false);
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < 16; ++i)
                if (gate() && gateCell (i).contains (e.position))
                {
                    paintOn = ! ((processor.gateMask.load() >> i) & 1);
                    setCell (i);
                    return;
                }
            if (flourish() && flourishDir.contains (e.position))
                setParam (processor.apvts, "keysPlay", e.position.x < flourishDir.getX() + 48.0f ? 4.0f : 5.0f);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < 16; ++i) if (gate() && gateCell (i).contains (e.position)) setCell (i);
        }

    private:
        juce::Rectangle<float> gateCell (int i) const { const float w = gateCells.getWidth() / 16.0f; return { gateCells.getX() + i * w + 1.0f, gateCells.getY(), w - 2.0f, gateCells.getHeight() }; }
        void setCell (int i)
        {
            // the bar repeats: the same step in both halves of the 32-step gate
            auto m = processor.gateMask.load();
            for (int k : { i, i + 16 }) m = paintOn ? (m | (1u << k)) : (m & ~(1u << k));
            processor.gateMask.store (m);
            repaint (gateCells.toNearestInt());
        }

        Hamamatsu108Processor& processor;
        juce::OwnedArray<Knob> knobs;
        juce::OwnedArray<ChoiceBox> choices;
        std::vector<Knob*> arpKnobs, gateKnobs, flourishKnobs;
        std::vector<ChoiceBox*> arpChoices, gateChoices, noChoices;
        juce::Rectangle<float> gateCells, flourishDir;
        float arpX = 0.0f, gateX = 0.0f, flourishX = 0.0f;
        int lastSig = -1;
        juce::String lastGate;
        bool paintOn = true;
    };
