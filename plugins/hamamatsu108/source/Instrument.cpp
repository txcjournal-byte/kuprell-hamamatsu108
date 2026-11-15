#include "Instrument.h"
#include "KuprellLookAndFeel.h"
#include "KuprellBrandAssets.h"

using namespace kuprell;

namespace hama
{
    namespace
    {
        const juce::Colour engrave { 0xffa7aeb6 };
        const juce::Colour wellTop { 0xff0d0f11 }, wellBottom { 0xff17191c };

        juce::Font spaced (float h, bool bold, float kerning)
        {
            return LookAndFeel::uiFont (h, bold).withExtraKerningFactor (kerning);
        }

        void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float plainValue)
        {
            if (auto* p = s.getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
                p->endChangeGesture();
            }
        }

        float plain (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue (id)->load(); }

        // recessed area with an inner shadow
        void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float radius)
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

        void drawLed (juce::Graphics& g, juce::Rectangle<float> r, bool on)
        {
            if (on)
            {
                g.setColour (colours::accent.withAlpha (0.35f));
                g.fillEllipse (r.expanded (r.getWidth() * 0.6f));
                juce::ColourGradient led (juce::Colours::white, r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.35f,
                                          colours::accentDeep, r.getRight(), r.getBottom(), true);
                led.addColour (0.55, colours::accent);
                g.setGradientFill (led);
            }
            else
                g.setColour (juce::Colour (0xff24272b));
            g.fillEllipse (r);
        }

        // 3D push button (raised cap with a dark side), optional LED strip or dot
        void drawHardButton (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool lit, bool enabled, bool stripLed)
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
                    g.setColour (colours::accent.withAlpha (0.4f));
                    g.fillRoundedRectangle (strip.expanded (3.0f, 2.0f), 3.0f);
                    g.setColour (colours::accent);
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

        void drawDisplay (juce::Graphics& g, juce::Rectangle<float> r, float radius)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff040607), r.getX(), r.getY(),
                                                     juce::Colour (0xff0a0e10), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, radius);
            g.setColour (juce::Colours::black);
            g.drawRoundedRectangle (r, radius, 1.0f);
        }

        void glowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j, float alpha = 1.0f)
        {
            g.setColour (colours::accent.withAlpha (0.22f * alpha));
            g.drawText (text, r.translated (0.0f, 0.6f), j, false);
            g.setColour (colours::accentSoft.withAlpha (alpha));
            g.drawText (text, r, j, false);
        }

        // deck layout shared by the backdrop (engravings) and the knobs
        struct DeckKnob { const char* param; const char* label; int size; const char* soon; };
        struct DeckGroup { const char* name; std::vector<DeckKnob> knobs; bool macro; };

        const std::vector<DeckGroup>& deckGroups()
        {
            static const std::vector<DeckGroup> g {
                { "HARP",    { { "harpTone", "Tone", 36, nullptr }, { "harpDecay", "Decay", 36, nullptr }, { "harpRange", "Range", 36, nullptr } }, false },
                { "CHORD",   { { nullptr, "Level", 36, "Chord layer arrives in step 3" }, { nullptr, "Filter", 36, "Chord layer arrives in step 3" } }, false },
                { "BASS",    { { nullptr, "Level", 36, "Bass arrives in step 3" }, { nullptr, "Tone", 36, "Bass arrives in step 3" } }, false },
                { "VIBRATO", { { "vibRate", "Rate", 36, nullptr }, { "vibDepth", "Depth", 36, nullptr } }, false },
                { "FX",      { { "fxChorus", "Chorus", 36, nullptr }, { "fxDelay", "Delay", 36, nullptr }, { "fxReverb", "Reverb", 36, nullptr } }, false },
                { "MACROS",  { { nullptr, "AGE", 44, "Macros arrive in step 4" }, { nullptr, "DREAM", 44, "Macros arrive in step 4" }, { nullptr, "WOBBLE", 44, "Macros arrive in step 4" } }, true },
                { "MASTER",  { { "master", "Volume", 48, nullptr } }, false },
            };
            return g;
        }

        int knobColumnWidth (int size) { return juce::jmax (size + 14, 48) + 4; }

        juce::Array<juce::Rectangle<int>> deckGroupBounds()
        {
            juce::Array<juce::Rectangle<int>> out;
            const auto deck = geo::deckArea();
            int total = 0;
            for (auto& gr : deckGroups())
                for (auto& k : gr.knobs) total += knobColumnWidth (k.size);
            const int groupPad = 8;
            const int spare = deck.getWidth() - 28 - total - (int) deckGroups().size() * groupPad;
            const int extra = spare / (int) deckGroups().size();
            int x = deck.getX() + 14;
            for (auto& gr : deckGroups())
            {
                int w = groupPad + extra;
                for (auto& k : gr.knobs) w += knobColumnWidth (k.size);
                out.add ({ x, deck.getY() + 8, w, deck.getHeight() - 14 });
                x += w;
            }
            return out;
        }
    }

    //==========================================================================
    struct InstrumentView::Backdrop : public juce::Component
    {
        Backdrop()
        {
            setBufferedToImage (true);
            setInterceptsMouseClicks (false, false);
            logo = juce::Drawable::createFromImageData (KuprellBrand::kuprelllogocream_svg, KuprellBrand::kuprelllogocream_svgSize);
        }

        void paint (juce::Graphics& g) override
        {
            using namespace geo;
            const auto bounds = getLocalBounds().toFloat();

            // studio desk
            juce::ColourGradient desk (juce::Colour (0xff3a4048), bounds.getCentreX(), bounds.getHeight() * 0.22f,
                                       juce::Colour (0xff0b0c0e), bounds.getCentreX(), bounds.getHeight() * 1.25f, true);
            desk.addColour (0.48, juce::Colour (0xff1c2025));
            g.setGradientFill (desk);
            g.fillAll();
            g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0.0f, bounds.getBottom() - 160.0f,
                                                     juce::Colours::black.withAlpha (0.55f), 0.0f, bounds.getBottom(), false));
            g.fillRect (bounds.withTop (bounds.getBottom() - 160.0f));

            const float bottom = (float) (bodyY + panelH + frontH);
            // floor shadow
            juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.85f), bounds.getCentreX(), bottom + 6.0f,
                                         juce::Colours::transparentBlack, bounds.getCentreX() + 560.0f, bottom + 6.0f, true);
            g.setGradientFill (shadow);
            g.fillEllipse ((float) bodyX + 20.0f, bottom - 30.0f, (float) bodyW - 40.0f, 80.0f);

            paintCheek (g, { (float) bodyX, (float) bodyY, (float) cheekW, (float) panelH }, true);
            paintCheek (g, { (float) (bodyX + bodyW - cheekW), (float) bodyY, (float) cheekW, (float) panelH }, false);
            paintPanel (g, { (float) (bodyX + cheekW), (float) bodyY, (float) (bodyW - 2 * cheekW), (float) panelH });
            paintFront (g, { (float) bodyX, (float) (bodyY + panelH), (float) bodyW, (float) frontH });

            paintRear (g);
            paintLabels (g);
            drawWell (g, padArea().toFloat(), 16.0f);
            drawWell (g, moduleArea().toFloat(), 16.0f);
            paintDeck (g);
        }

        void paintCheek (juce::Graphics& g, juce::Rectangle<float> r, bool left)
        {
            juce::Path p;
            p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 22.0f, 22.0f, left, ! left, left, ! left);
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (p);

            juce::ColourGradient wood (juce::Colour (0xff3e2412), r.getX(), 0.0f, juce::Colour (0xff3a2110), r.getRight(), 0.0f, false);
            wood.addColour (0.4, juce::Colour (0xff7a4b2a));
            wood.addColour (0.7, juce::Colour (0xff5e3a20));
            g.setGradientFill (wood);
            g.fillRect (r);

            // grain
            juce::Random rnd (left ? 81 : 108);
            for (float x = r.getX() + 1.0f; x < r.getRight(); x += 2.0f + rnd.nextFloat() * 4.0f)
            {
                g.setColour ((rnd.nextBool() ? juce::Colours::black.withAlpha (0.18f) : juce::Colours::white.withAlpha (0.06f)));
                juce::Path line;
                line.startNewSubPath (x, r.getY());
                const float wobble = rnd.nextFloat() * 3.0f - 1.5f;
                line.cubicTo (x + wobble, r.getY() + r.getHeight() * 0.33f, x - wobble, r.getY() + r.getHeight() * 0.66f, x + wobble * 0.5f, r.getBottom());
                g.strokePath (line, juce::PathStrokeType (0.6f + rnd.nextFloat() * 1.2f));
            }
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), 0.0f, r.getY(),
                                                     juce::Colours::transparentWhite, 0.0f, r.getY() + r.getHeight() * 0.18f, false));
            g.fillRect (r);
            // inner edge shadow towards the metal
            const float ex = left ? r.getRight() - 6.0f : r.getX();
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (left ? 0.0f : 0.45f), ex, 0.0f,
                                                     juce::Colours::black.withAlpha (left ? 0.45f : 0.0f), ex + 6.0f, 0.0f, false));
            g.fillRect (juce::Rectangle<float> (ex, r.getY(), 6.0f, r.getHeight()));
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            if (left) g.drawVerticalLine ((int) r.getX() + 1, r.getY() + 20.0f, r.getBottom());
        }

        void paintPanel (juce::Graphics& g, juce::Rectangle<float> r)
        {
            juce::ColourGradient metal (juce::Colour (0xff555b63), 0.0f, r.getY(), juce::Colour (0xff2c3036), 0.0f, r.getBottom(), false);
            metal.addColour (0.4, juce::Colour (0xff3a3f46));
            g.setGradientFill (metal);
            g.fillRect (r);

            // soft sheen across the metal
            juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.13f), r.getX(), r.getY(),
                                        juce::Colours::transparentWhite, r.getX() + r.getWidth() * 0.6f, r.getY() + r.getHeight() * 0.9f, false);
            sheen.addColour (0.45, juce::Colours::transparentWhite);
            sheen.addColour (0.75, juce::Colours::white.withAlpha (0.05f));
            g.setGradientFill (sheen);
            g.fillRect (r);

            // fine brushing
            juce::Random rnd (1108);
            for (float y = r.getY(); y < r.getBottom(); y += 2.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (rnd.nextFloat() * 0.025f));
                g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
            }

            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.fillRect (r.withHeight (2.0f));
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRect (r.withTop (r.getBottom() - 2.0f));

            // screws
            for (auto p : { juce::Point<float> (r.getX() + 9.0f, r.getY() + 9.0f), juce::Point<float> (r.getRight() - 9.0f, r.getY() + 9.0f),
                            juce::Point<float> (r.getX() + 9.0f, r.getBottom() - 9.0f), juce::Point<float> (r.getRight() - 9.0f, r.getBottom() - 9.0f) })
            {
                auto s = juce::Rectangle<float> (7.0f, 7.0f).withCentre (p);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9ced4), s.getX(), s.getY(), juce::Colour (0xff5a6068), s.getRight(), s.getBottom(), false));
                g.fillEllipse (s);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.drawLine (s.getX() + 1.5f, s.getCentreY(), s.getRight() - 1.5f, s.getCentreY(), 1.0f);
            }
        }

        void paintFront (juce::Graphics& g, juce::Rectangle<float> r)
        {
            auto woodL = r.removeFromLeft ((float) geo::cheekW);
            auto woodR = r.removeFromRight ((float) geo::cheekW);
            for (auto w : { woodL, woodR })
            {
                juce::Path p;
                const bool left = w == woodL;
                p.addRoundedRectangle (w.getX(), w.getY(), w.getWidth(), w.getHeight(), 16.0f, 16.0f, false, false, left, ! left);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a2c17), 0.0f, w.getY(), juce::Colour (0xff24140a), 0.0f, w.getBottom(), false));
                g.fillPath (p);
            }
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e3237), 0.0f, r.getY(), juce::Colour (0xff16181b), 0.0f, r.getBottom(), false));
            g.fillRect (r);
            g.setColour (juce::Colour (0xffc9ced4));
            g.fillRect (r.withHeight (2.0f));

            g.setFont (spaced (9.0f, true, 0.3f));
            g.setColour (juce::Colour (0xff7e858d));
            g.drawText ("ELECTRONIC STRUM HARP", r, juce::Justification::centred, false);
            g.drawText (juce::String::fromUTF8 ("MIDI \xc2\xb7 USB"), r.reduced (26.0f, 0.0f), juce::Justification::centredRight, false);
        }

        void paintRear (juce::Graphics& g)
        {
            using namespace geo;
            // speaker grille
            auto grille = juce::Rectangle<float> ((float) innerX, (float) rearY + 3.0f, 210.0f, 52.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3e44), 0.0f, grille.getY(), juce::Colour (0xff2a2d32), 0.0f, grille.getBottom(), false));
            g.fillRoundedRectangle (grille, 12.0f);
            {
                juce::Graphics::ScopedSaveState save (g);
                juce::Path clip;
                clip.addRoundedRectangle (grille.reduced (4.0f), 9.0f);
                g.reduceClipRegion (clip);
                g.setColour (juce::Colour (0xff040506));
                for (float y = grille.getY() + 6.0f; y < grille.getBottom() - 2.0f; y += 8.0f)
                    for (float x = grille.getX() + 6.0f; x < grille.getRight() - 2.0f; x += 8.0f)
                        g.fillEllipse (x - 1.9f, y - 1.9f, 3.8f, 3.8f);
            }
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), 0.0f, grille.getY(), juce::Colours::transparentBlack, 0.0f, grille.getY() + 10.0f, false));
            g.fillRoundedRectangle (grille, 12.0f);
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.drawHorizontalLine ((int) grille.getBottom(), grille.getX() + 12.0f, grille.getRight() - 12.0f);

            // logo (approved SVG, drawn as vector, never altered) and engraved model line
            const float tx = grille.getRight() + 20.0f;
            if (logo != nullptr)
            {
                const auto lb = logo->getDrawableBounds();
                const float h = 22.0f, w = lb.getHeight() > 0 ? lb.getWidth() * h / lb.getHeight() : 96.0f;
                logo->drawWithin (g, { tx, (float) rearY + 8.0f, w, h }, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yTop, 1.0f);
            }
            g.setFont (spaced (11.0f, false, 0.45f));
            g.setColour (engrave);
            g.drawText (juce::String::fromUTF8 ("HAMAMATSU 108  \xc2\xb7  ELECTRONIC STRUM HARP"),
                        juce::Rectangle<float> (tx, (float) rearY + 36.0f, 420.0f, 14.0f), juce::Justification::centredLeft, false);

            // model display (models arrive in step 4)
            auto disp = juce::Rectangle<float> ((float) (innerX + innerW - 230 - 46), (float) rearY + 6.0f, 230.0f, 46.0f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (disp.expanded (3.0f), 11.0f);
            g.setColour (juce::Colour (0xff1a1c1f));
            g.fillRoundedRectangle (disp.expanded (2.0f), 10.0f);
            drawDisplay (g, disp, 9.0f);
            g.setFont (LookAndFeel::monoFont (11.0f));
            glowText (g, "MODEL", disp.reduced (14.0f, 0.0f), juce::Justification::centredLeft, 0.7f);
            g.setFont (LookAndFeel::monoFont (17.0f));
            glowText (g, "SYSTEM 84", disp.reduced (14.0f, 0.0f), juce::Justification::centredRight);

            // power LED
            const float px = (float) (innerX + innerW - 18);
            drawLed (g, juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ px, (float) rearY + 22.0f }), true);
            g.setFont (spaced (8.5f, true, 0.25f));
            g.setColour (engrave);
            g.drawText ("POWER", juce::Rectangle<float> (px - 30.0f, (float) rearY + 34.0f, 60.0f, 12.0f), juce::Justification::centred, false);
        }

        void engravedLabel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& left, const juce::String& right = {})
        {
            g.setFont (spaced (9.0f, true, 0.28f));
            g.setColour (engrave);
            const float lw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), left) + 8.0f;
            g.drawText (left, r, juce::Justification::centredLeft, false);
            float rx = r.getRight();
            if (right.isNotEmpty())
            {
                const float rw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), right) + 8.0f;
                g.drawText (right, r, juce::Justification::centredRight, false);
                rx -= rw;
            }
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + lw, rx);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawHorizontalLine ((int) r.getCentreY() + 1, r.getX() + lw, rx);
        }

        void paintLabels (juce::Graphics& g)
        {
            using namespace geo;
            const float y = (float) labelY;
            engravedLabel (g, padArea().toFloat().withY (y).withHeight (12.0f), "CHORD BUTTONS", juce::String::fromUTF8 ("\xe2\x97\x8f IN KEY"));
            engravedLabel (g, plateArea().toFloat().withY (y).withHeight (12.0f), "STRUM PLATE");
            engravedLabel (g, moduleArea().toFloat().withY (y).withHeight (12.0f), juce::String::fromUTF8 ("VOICE \xc2\xb7 LIVE STRUM"));
        }

        void paintDeck (juce::Graphics& g)
        {
            const auto r = geo::deckArea().toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (r.translated (0.0f, 5.0f).expanded (1.0f), 14.0f);

            juce::ColourGradient silver (juce::Colour (0xffe4e7ea), 0.0f, r.getY(), juce::Colour (0xff9da3aa), 0.0f, r.getBottom(), false);
            silver.addColour (0.55, juce::Colour (0xffb9bec4));
            g.setGradientFill (silver);
            g.fillRoundedRectangle (r, 14.0f);

            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, 14.0f);
            g.reduceClipRegion (clip);
            // brushed metal
            for (float x = r.getX(); x < r.getRight(); x += 3.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.10f));
                g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
                g.setColour (juce::Colours::black.withAlpha (0.04f));
                g.drawVerticalLine ((int) x + 1, r.getY(), r.getBottom());
            }
            juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.35f), r.getX(), r.getY(),
                                        juce::Colours::transparentWhite, r.getX() + r.getWidth() * 0.5f, r.getBottom() + 200.0f, false);
            sheen.addColour (0.3, juce::Colours::transparentWhite);
            sheen.addColour (0.58, juce::Colours::white.withAlpha (0.18f));
            g.setGradientFill (sheen);
            g.fillRect (r);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 12.0f, r.getRight() - 12.0f);
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.fillRect (r.withTop (r.getBottom() - 2.0f));

            // group names and separators
            const auto groups = deckGroupBounds();
            for (int i = 0; i < groups.size(); ++i)
            {
                const auto& gr = deckGroups()[(size_t) i];
                auto b = groups[i].toFloat();
                g.setFont (spaced (9.0f, true, 0.25f));
                g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.drawText (gr.name, b.withHeight (12.0f).translated (0.0f, 1.0f), juce::Justification::centred, false);
                g.setColour (gr.macro ? colours::accentDeep : juce::Colour (0xff3a3f46));
                g.drawText (gr.name, b.withHeight (12.0f), juce::Justification::centred, false);
                if (i < groups.size() - 1)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.12f));
                    g.drawVerticalLine ((int) b.getRight(), b.getY() + 2.0f, b.getBottom() - 2.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.5f));
                    g.drawVerticalLine ((int) b.getRight() + 1, b.getY() + 2.0f, b.getBottom() - 2.0f);
                }
            }
        }

        std::unique_ptr<juce::Drawable> logo;
    };

    //==========================================================================
    class ChordPad : public juce::Component
    {
    public:
        explicit ChordPad (Hamamatsu108Processor& p) : processor (p) {}

        static constexpr float keyW = 42.0f, keyH = 27.0f, gapX = 4.9f, gapY = 4.0f;
        static constexpr float keysX = 14.0f + 38.0f, keysY = 9.0f + 16.0f + 4.0f;

        juce::Rectangle<float> keyBounds (int root, int type) const
        {
            return { keysX + root * (keyW + gapX), keysY + type * (keyH + gapY), keyW, keyH };
        }

        void update (int active, int keyRoot, bool minor)
        {
            if (active != activeChord || keyRoot != key || minor != keyMinor)
            {
                activeChord = active;
                key = keyRoot;
                keyMinor = minor;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            g.setFont (LookAndFeel::uiFont (10.0f, true));
            g.setColour (juce::Colour (0xff9aa0a7));
            for (int r = 0; r < chords::numRoots; ++r)
                g.drawText (chords::rootName (r), keyBounds (r, 0).withY (9.0f).withHeight (16.0f), juce::Justification::centred, false);

            g.setFont (spaced (8.5f, true, 0.05f));
            for (int t = 0; t < chords::numTypes; ++t)
                g.drawText (chords::types()[(size_t) t].rowLabel, juce::Rectangle<float> (12.0f, keyBounds (0, t).getY(), 35.0f, keyH),
                            juce::Justification::centredRight, false);

            for (int t = 0; t < chords::numTypes; ++t)
                for (int r = 0; r < chords::numRoots; ++r)
                    paintKey (g, r, t);
        }

        void paintKey (juce::Graphics& g, int root, int type)
        {
            const int idx = chords::index (root, type);
            const bool on = idx == activeChord || idx == pressed;
            auto b = keyBounds (root, type);

            if (! on)
            {
                g.setColour (juce::Colours::black.withAlpha (0.75f));
                g.fillRoundedRectangle (b.translated (0.0f, 4.0f).expanded (0.5f), 7.0f);
                g.setColour (juce::Colour (0xff121417));
                g.fillRoundedRectangle (b, 7.0f);
                auto cap = b.withTrimmedBottom (4.0f);
                juce::ColourGradient cg (juce::Colour (0xff4e535a), 0.0f, cap.getY(), juce::Colour (0xff2a2d32), 0.0f, cap.getBottom(), false);
                cg.addColour (0.62, juce::Colour (0xff34383e));
                g.setGradientFill (cg);
                g.fillRoundedRectangle (cap, 7.0f);
                g.setColour (juce::Colours::white.withAlpha (0.22f));
                g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + 6.0f, cap.getRight() - 6.0f);
                g.setColour (juce::Colour (0xffe6e7e9));
                g.setFont (LookAndFeel::uiFont (10.0f, true));
                g.drawText (chords::shortName (idx), cap, juce::Justification::centred, false);
                b = cap;
            }
            else
            {
                b = b.withTrimmedTop (3.0f);
                g.setColour (colours::accent.withAlpha (0.25f));
                g.fillRoundedRectangle (b.expanded (5.0f), 10.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e6f6a), 0.0f, b.getY(), juce::Colour (0xff1c4a47), 0.0f, b.getBottom(), false));
                g.fillRoundedRectangle (b, 7.0f);
                g.setColour (colours::accent);
                g.drawRoundedRectangle (b.reduced (0.5f), 7.0f, 2.0f);
                g.setColour (juce::Colours::white);
                g.setFont (LookAndFeel::uiFont (10.0f, true));
                g.drawText (chords::shortName (idx), b, juce::Justification::centred, false);
            }

            const bool inKey = chords::isInKey (idx, key, keyMinor);
            auto dot = juce::Rectangle<float> (4.0f, 4.0f).withPosition (b.getRight() - 8.0f, b.getY() + 3.0f);
            if (inKey)
            {
                g.setColour (colours::accent.withAlpha (0.4f));
                g.fillEllipse (dot.expanded (2.0f));
                g.setColour (colours::accent);
            }
            else
                g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillEllipse (dot);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int t = 0; t < chords::numTypes; ++t)
                for (int r = 0; r < chords::numRoots; ++r)
                    if (keyBounds (r, t).contains (e.position))
                    {
                        pressed = chords::index (r, t);
                        processor.clickChord (pressed);
                        repaint();
                        return;
                    }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            pressed = -1;
            repaint();
        }

    private:
        Hamamatsu108Processor& processor;
        int activeChord = -1, pressed = -1, key = 0;
        bool keyMinor = false;
    };

    //==========================================================================
    class StrumPlate : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit StrumPlate (Hamamatsu108Processor& p) : processor (p)
        {
            setTooltip (juce::String::fromUTF8 ("Strum plate \xc2\xb7 drag across the strings, or play C3-C6 on your keyboard"));
        }

        static constexpr int numStrings = Hamamatsu108Processor::numStrings;

        juce::Rectangle<float> glass() const { return getLocalBounds().toFloat().reduced (6.0f); }

        float stringY (int visual) const
        {
            const auto gl = glass();
            const float top = gl.getY() + 40.0f, bottom = gl.getBottom() - 12.0f;
            return bottom - (bottom - top) * (float) visual / (float) (numStrings - 1);   // low strings at the bottom
        }

        void update (std::uint32_t newFlashes, int chord)
        {
            bool changed = chord != chordIndex;
            chordIndex = chord;
            for (int i = 0; i < numStrings; ++i)
            {
                if (newFlashes & (1u << (unsigned) i)) { glow[(size_t) i] = 1.0f; changed = true; }
                else if (glow[(size_t) i] > 0.01f) { glow[(size_t) i] *= 0.86f; changed = true; }
                else glow[(size_t) i] = 0.0f;
            }
            if (changed) repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            // chrome bezel
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRoundedRectangle (r.translated (0.0f, 6.0f), 16.0f);
            juce::ColourGradient chrome (juce::Colour (0xfff2f4f6), 0.0f, r.getY(), juce::Colour (0xff7f868e), 0.0f, r.getBottom(), false);
            chrome.addColour (0.3, juce::Colour (0xff9aa1a9));
            chrome.addColour (0.55, juce::Colour (0xffd8dce0));
            g.setGradientFill (chrome);
            g.fillRoundedRectangle (r, 16.0f);

            const auto gl = glass();
            juce::ColourGradient glassFill (juce::Colour (0xff142027), gl.getX(), gl.getY(), juce::Colour (0xff101a20), gl.getRight(), gl.getBottom(), false);
            glassFill.addColour (0.6, juce::Colour (0xff070b0e));
            g.setGradientFill (glassFill);
            g.fillRoundedRectangle (gl, 11.0f);

            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (gl, 11.0f);
            g.reduceClipRegion (clip);

            // haze where the hand plays
            float sum = 0.0f;
            for (auto v : glow) sum += v;
            if (sum > 0.05f)
            {
                juce::ColourGradient haze (colours::accent.withAlpha (juce::jmin (0.3f, sum * 0.04f)), gl.getCentreX(), gl.getCentreY() + 20.0f,
                                           juce::Colours::transparentBlack, gl.getCentreX() + 90.0f, gl.getCentreY() + 20.0f, true);
                g.setGradientFill (haze);
                g.fillRect (gl);
            }

            // strings
            for (int i = 0; i < numStrings; ++i)
            {
                const float y = stringY (i);
                const float v = glow[(size_t) i];
                const float alpha = chordIndex >= 0 ? 0.32f + 0.68f * v : 0.16f + 0.84f * v;
                if (v > 0.05f)
                {
                    g.setColour (colours::accent.withAlpha (0.35f * v));
                    g.fillRect (gl.getX() + 6.0f, y - 2.5f, gl.getWidth() - 12.0f, 5.0f);
                }
                juce::ColourGradient s (colours::accent.withAlpha (0.0f), gl.getX(), y, colours::accent.withAlpha (0.0f), gl.getRight(), y, false);
                s.addColour (0.18, colours::accent.withAlpha (alpha));
                s.addColour (0.5, colours::accentSoft.withAlpha (alpha));
                s.addColour (0.82, colours::accent.withAlpha (alpha));
                g.setGradientFill (s);
                g.fillRect (gl.getX(), y - 1.0f, gl.getWidth(), 2.0f);
            }

            // chord name band
            auto band = gl.withHeight (28.0f);
            g.setColour (juce::Colour (0xff040607));
            g.fillRect (band);
            g.setColour (juce::Colours::black);
            g.drawHorizontalLine ((int) band.getBottom(), band.getX(), band.getRight());
            g.setFont (LookAndFeel::monoFont (16.0f));
            glowText (g, chordIndex >= 0 ? chords::shortName (chordIndex) : juce::String::fromUTF8 ("\xe2\x80\x94"), band, juce::Justification::centred,
                      chordIndex >= 0 ? 1.0f : 0.5f);

            // glass reflection
            juce::ColourGradient refl (juce::Colours::white.withAlpha (0.16f), gl.getX(), gl.getY(),
                                       juce::Colours::white.withAlpha (0.06f), gl.getRight(), gl.getBottom(), false);
            refl.addColour (0.32, juce::Colours::transparentWhite);
            refl.addColour (0.7, juce::Colours::transparentWhite);
            g.setGradientFill (refl);
            g.fillRect (gl);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawRoundedRectangle (gl, 11.0f, 1.5f);
        }

        int stringAt (float y) const
        {
            int best = -1;
            float bestD = 1.0e9f;
            for (int i = 0; i < numStrings; ++i)
            {
                const float d = std::abs (stringY (i) - y);
                if (d < bestD) { bestD = d; best = i; }
            }
            return bestD < 8.0f ? best : -1;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            lastY = e.position.y;
            if (const int s = stringAt (e.position.y); s >= 0)
                processor.pluckString (s);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float y0 = lastY, y1 = e.position.y;
            for (int i = 0; i < numStrings; ++i)
            {
                const float sy = stringY (i);
                if ((sy > juce::jmin (y0, y1) && sy <= juce::jmax (y0, y1)))
                    processor.pluckString (i);
            }
            lastY = y1;
        }

    private:
        Hamamatsu108Processor& processor;
        std::array<float, numStrings> glow {};
        int chordIndex = -1;
        float lastY = 0.0f;
    };

    //==========================================================================
    class VoiceModule : public juce::Component
    {
    public:
        explicit VoiceModule (Hamamatsu108Processor& p) : processor (p)
        {
            Knob::Style st;
            st.labelHeight = 8.5f;
            const char* ids[]    = { "harpVoice", "strumRate", "strumFeel", "strumSpeed" };
            const char* labels[] = { "VOICE", "RATE", "FEEL", "SPEED" };
            for (int i = 0; i < 4; ++i)
                addAndMakeVisible (knobs.add (new Knob (processor.apvts, ids[i], labels[i], 28, st)));
        }

        juce::OwnedArray<Knob> knobs;

        // button areas (module coordinates)
        juce::Rectangle<float> dualArea() const  { return { 12.0f, 130.0f, 93.0f, 30.0f }; }
        juce::Rectangle<float> holdArea() const  { return { 111.0f, 130.0f, 93.0f, 30.0f }; }
        juce::Rectangle<float> modeArea (int i) const { return { 12.0f + i * 49.0f, 166.0f, 45.0f, 34.0f }; }
        juce::Rectangle<float> keyArea() const   { return { 100.0f, 210.0f, 104.0f, 24.0f }; }
        juce::Rectangle<float> trDown() const    { return { 106.0f, 240.0f, 26.0f, 24.0f }; }
        juce::Rectangle<float> trValue() const   { return { 134.0f, 240.0f, 40.0f, 24.0f }; }
        juce::Rectangle<float> trUp() const      { return { 176.0f, 240.0f, 26.0f, 24.0f }; }
        juce::Rectangle<float> meterArea() const { return { 40.0f, 276.0f, 164.0f, 13.0f }; }

        void resized() override
        {
            for (int i = 0; i < knobs.size(); ++i)
                knobs[i]->setBounds (12 + i * 48, 60, 48, 66);
        }

        void update (float levelL, float levelR, int chord)
        {
            auto& s = processor.apvts;
            const int newVoice = juce::roundToInt (plain (s, "harpVoice"));
            const int newMode = juce::roundToInt (plain (s, "strumMode"));
            const int newRate = juce::roundToInt (plain (s, "strumRate"));
            const bool newHold = plain (s, "hold") > 0.5f;
            const int newKey = juce::roundToInt (plain (s, "keyRoot"));
            const bool newMinor = plain (s, "keyMode") > 0.5f;
            const int newTr = juce::roundToInt (plain (s, "transpose"));

            if (newVoice != voice || newMode != mode || newRate != rate || newHold != hold || newKey != key || newMinor != minor || newTr != transpose || chord != chordIndex)
            {
                voice = newVoice; mode = newMode; rate = newRate; hold = newHold; key = newKey; minor = newMinor; transpose = newTr; chordIndex = chord;
                repaint();
            }

            const int segL = segments (levelL), segR = segments (levelR);
            if (segL != meterL || segR != meterR)
            {
                meterL = segL; meterR = segR;
                repaint (meterArea().expanded (2.0f).toNearestInt());
            }
        }

        void paint (juce::Graphics& g) override
        {
            // VFD
            auto vfd = juce::Rectangle<float> (12.0f, 12.0f, 192.0f, 42.0f);
            drawDisplay (g, vfd, 8.0f);
            auto lines = vfd.reduced (10.0f, 5.0f);
            g.setFont (LookAndFeel::monoFont (13.0f));
            auto l1 = lines.removeFromTop (16.0f);
            glowText (g, "VOICE", l1, juce::Justification::centredLeft);
            glowText (g, harp::voiceNames()[voice].toUpperCase(), l1, juce::Justification::centredRight);
            auto l2 = lines.removeFromTop (16.0f);
            glowText (g, "STRUM", l2, juce::Justification::centredLeft, 0.75f);
            const juce::String strum = mode == 0 ? "LIVE " + Hamamatsu108Processor::strumRateNames()[rate]
                                                 : Hamamatsu108Processor::strumModeNames()[mode].toUpperCase();
            glowText (g, strum, l2, juce::Justification::centredRight, 0.75f);

            drawHardButton (g, dualArea(), "DUAL", false, false, false);
            drawHardButton (g, holdArea(), "HOLD", hold, true, false);
            for (int i = 0; i < 4; ++i)
                drawHardButton (g, modeArea (i), Hamamatsu108Processor::strumModeNames()[i].toUpperCase(), mode == i, i == 0 || i == 3, true);

            g.setFont (spaced (9.0f, true, 0.2f));
            g.setColour (juce::Colour (0xff9aa0a7));
            g.drawText ("KEY", juce::Rectangle<float> (12.0f, keyArea().getY(), 80.0f, 24.0f), juce::Justification::centredLeft, false);
            g.drawText ("TRANSPOSE", juce::Rectangle<float> (12.0f, trValue().getY(), 90.0f, 24.0f), juce::Justification::centredLeft, false);
            g.drawText ("OUT", juce::Rectangle<float> (12.0f, meterArea().getY(), 28.0f, 13.0f), juce::Justification::centredLeft, false);

            drawDisplay (g, keyArea(), 5.0f);
            g.setFont (LookAndFeel::monoFont (13.0f));
            glowText (g, juce::String (chords::rootName (key)) + (minor ? " MINOR" : " MAJOR"), keyArea(), juce::Justification::centred);

            drawHardButton (g, trDown(), {}, false, true, false);
            drawHardButton (g, trUp(), {}, false, true, false);
            g.setColour (juce::Colour (0xffecedef));
            g.setFont (LookAndFeel::uiFont (14.0f, true));
            g.drawText (juce::String::fromUTF8 ("\xe2\x88\x92"), trDown().withTrimmedBottom (4.0f), juce::Justification::centred, false);
            g.drawText ("+", trUp().withTrimmedBottom (4.0f), juce::Justification::centred, false);
            drawDisplay (g, trValue(), 5.0f);
            g.setFont (LookAndFeel::monoFont (14.0f));
            glowText (g, (transpose > 0 ? "+" : "") + juce::String (transpose), trValue(), juce::Justification::centred);

            // meter
            const auto m = meterArea();
            for (int row = 0; row < 2; ++row)
            {
                const int lit = row == 0 ? meterL : meterR;
                for (int i = 0; i < 16; ++i)
                {
                    const float w = (m.getWidth() - 15.0f * 2.0f) / 16.0f;
                    auto seg = juce::Rectangle<float> (m.getX() + i * (w + 2.0f), m.getY() + row * 8.0f, w, 5.0f);
                    if (i < lit)
                    {
                        const auto c = i >= 13 ? colours::accentSoft : colours::accent;
                        g.setColour (c.withAlpha (0.3f));
                        g.fillRoundedRectangle (seg.expanded (1.5f), 2.0f);
                        g.setColour (c);
                    }
                    else
                        g.setColour (juce::Colour (0xff22252a));
                    g.fillRoundedRectangle (seg, 1.0f);
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            auto& s = processor.apvts;
            const auto p = e.position;
            if (holdArea().contains (p))       setParam (s, "hold", hold ? 0.0f : 1.0f);
            else if (dualArea().contains (p))  message ("Dual (two harp voices at once) arrives in step 3.");
            else if (trDown().contains (p))    setParam (s, "transpose", (float) juce::jmax (-12, transpose - 1));
            else if (trUp().contains (p))      setParam (s, "transpose", (float) juce::jmin (12, transpose + 1));
            else if (keyArea().contains (p))   showKeyMenu();
            else
                for (int i = 0; i < 4; ++i)
                    if (modeArea (i).contains (p))
                    {
                        if (i == 1 || i == 2) message (i == 1 ? "Arpeggio modes arrive in step 3." : "Chord Memory arrives in step 3.");
                        else setParam (s, "strumMode", (float) i);
                    }
        }

        std::function<void (const juce::String&)> onMessage;

    private:
        void message (const juce::String& m) { if (onMessage) onMessage (m); }

        void showKeyMenu()
        {
            juce::PopupMenu menu;
            for (int mode2 = 0; mode2 < 2; ++mode2)
            {
                juce::PopupMenu sub;
                for (int r = 0; r < 12; ++r)
                    sub.addItem (1 + mode2 * 12 + r, chords::rootName (r), true, r == key && (mode2 == 1) == minor);
                menu.addSubMenu (mode2 == 0 ? "Major" : "Minor", sub);
            }
            juce::Component::SafePointer<VoiceModule> safe (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (keyArea().toNearestInt())), [safe] (int r)
            {
                if (safe == nullptr || r <= 0) return;
                setParam (safe->processor.apvts, "keyRoot", (float) ((r - 1) % 12));
                setParam (safe->processor.apvts, "keyMode", (float) ((r - 1) / 12));
            });
        }

        static int segments (float level)
        {
            const float db = juce::Decibels::gainToDecibels (level, -60.0f);
            return juce::jlimit (0, 16, (int) std::ceil ((db + 48.0f) / 3.0f));
        }

        Hamamatsu108Processor& processor;
        int voice = 0, mode = 0, rate = 2, key = 0, transpose = 0, chordIndex = -1, meterL = 0, meterR = 0;
        bool hold = false, minor = false;
    };

    //==========================================================================
    InstrumentView::InstrumentView (Hamamatsu108Processor& p) : processor (p)
    {
        backdrop = std::make_unique<Backdrop>();
        pad = std::make_unique<ChordPad> (p);
        plate = std::make_unique<StrumPlate> (p);
        module = std::make_unique<VoiceModule> (p);

        addAndMakeVisible (*backdrop);
        addAndMakeVisible (*pad);
        addAndMakeVisible (*plate);
        addAndMakeVisible (*module);

        module->onMessage = [this] (const juce::String& m) { if (onStatus) onStatus (m, {}); };
        for (auto* k : module->knobs)
            k->onInteract = [this] (const Knob& kn) { if (onStatus) onStatus (kn.getDisplayName(), kn.getValueText()); };

        Knob::Style deck;
        deck.ring = colours::accentDeep;
        deck.track = juce::Colours::black.withAlpha (0.18f);
        deck.pointer = colours::accentDeep;
        deck.label = juce::Colour (0xff22262b);
        deck.value = juce::Colour (0xff4a5058);
        deck.labelHeight = 10.0f;

        for (auto& gr : deckGroups())
            for (auto& k : gr.knobs)
            {
                auto st = deck;
                if (std::string (gr.name) == "MASTER") { st.ring = juce::Colour (0xff22262b); st.pointer = juce::Colour (0xff22262b); }
                if (k.param != nullptr)
                    addKnob (std::make_unique<Knob> (p.apvts, k.param, k.label, k.size, st));
                else
                    addKnob (std::make_unique<Knob> (k.label, k.size, st, k.soon));
            }

        startTimerHz (30);
    }

    InstrumentView::~InstrumentView()
    {
        stopTimer();
    }

    void InstrumentView::addKnob (std::unique_ptr<Knob> k)
    {
        k->onInteract = [this] (const Knob& kn) { if (onStatus) onStatus (kn.getDisplayName(), kn.getValueText()); };
        addAndMakeVisible (*k);
        deckKnobs.add (k.release());
    }

    void InstrumentView::resized()
    {
        backdrop->setBounds (getLocalBounds());
        pad->setBounds (geo::padArea());
        plate->setBounds (geo::plateArea());
        module->setBounds (geo::moduleArea());

        const auto groups = deckGroupBounds();
        int k = 0;
        for (int gi = 0; gi < groups.size(); ++gi)
        {
            const auto& gr = deckGroups()[(size_t) gi];
            int total = 0;
            for (auto& kn : gr.knobs) total += knobColumnWidth (kn.size);
            int x = groups[gi].getCentreX() - total / 2;
            for (auto& kn : gr.knobs)
            {
                const int w = knobColumnWidth (kn.size);
                const int box = kn.size + 14;
                const int y = groups[gi].getY() + 16 + (62 - box) / 2;
                deckKnobs[k++]->setBounds (x, y, w, box + 26);
                x += w;
            }
        }
    }

    void InstrumentView::timerCallback()
    {
        const int chord = processor.activeChord.load();
        pad->update (chord, juce::roundToInt (plain (processor.apvts, "keyRoot")), plain (processor.apvts, "keyMode") > 0.5f);
        plate->update (processor.stringFlashes.exchange (0), chord);
        module->update (processor.meterLeft.load(), processor.meterRight.load(), chord);
    }
}
