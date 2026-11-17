#include "Instrument.h"
#include "Look.h"
#include "Skins.h"
#include "KuprellLookAndFeel.h"
#include "KuprellBrandAssets.h"

using namespace kuprell;
using namespace hama::look;

namespace hama
{
    namespace
    {
        // deck layout shared by the backdrop (engravings) and the knobs
        struct DeckKnob { const char* param; const char* label; int size; const char* soon; };
        struct DeckGroup { const char* name; std::vector<DeckKnob> knobs; bool macro; int selector = 0; };   // selector: 1 chord layer, 2 bass

        const std::vector<DeckGroup>& deckGroups()
        {
            static const std::vector<DeckGroup> g {
                { "HARP",    { { "harpTone", "Tone", 36, nullptr }, { "harpDecay", "Decay", 36, nullptr }, { "harpRange", "Range", 36, nullptr } }, false },
                { "CHORD",   { { "chordLevel", "Level", 36, nullptr }, { "chordFilter", "Filter", 36, nullptr } }, false, 1 },
                { "BASS",    { { "bassLevel", "Level", 36, nullptr }, { "bassTone", "Tone", 36, nullptr } }, false, 2 },
                { "VIBRATO", { { "vibRate", "Rate", 36, nullptr }, { "vibDepth", "Depth", 36, nullptr } }, false },
                { "FX",      { { "fx1Mix", "FX 1", 36, nullptr }, { "fx2Mix", "FX 2", 36, nullptr }, { "fx3Mix", "FX 3", 36, nullptr } }, false },
                { "MACROS",  { { "macroAge", "AGE", 44, nullptr }, { "macroDream", "DREAM", 44, nullptr }, { "macroWobble", "WOBBLE", 44, nullptr } }, true },
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
            if (paintDesk())
            {
                g.setGradientFill (desk);
                g.fillAll();
                g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0.0f, bounds.getBottom() - 160.0f,
                                                         juce::Colours::black.withAlpha (0.55f), 0.0f, bounds.getBottom(), false));
                g.fillRect (bounds.withTop (bounds.getBottom() - 160.0f));
            }

            const float bottom = (float) (bodyY + panelH + frontH);
            // floor shadow
            juce::ColourGradient shadow (juce::Colours::black.withAlpha (paintDesk() ? 0.85f : 0.35f), bounds.getCentreX(), bottom + 6.0f,
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

            const auto& sk = skins::get (skin);
            if (sk.cheek != skins::Cheek::walnut)
            {
                juce::ColourGradient flat (sk.cheekB, r.getX(), 0.0f, sk.cheekB, r.getRight(), 0.0f, false);
                flat.addColour (0.45, sk.cheekA);
                g.setGradientFill (flat);
                g.fillRect (r);
                skins::paintCheekPattern (g, r, sk);
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.3f), 0.0f, r.getY(), juce::Colours::transparentWhite, 0.0f, r.getY() + r.getHeight() * 0.2f, false));
                g.fillRect (r);
                return;
            }
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
            const auto& sk = skins::get (skin);
            juce::ColourGradient metal (sk.panelTop.withMultipliedAlpha (sk.panelAlpha), 0.0f, r.getY(), sk.panelBottom.withMultipliedAlpha (sk.panelAlpha), 0.0f, r.getBottom(), false);
            metal.addColour (0.4, sk.panelMid.withMultipliedAlpha (sk.panelAlpha));
            g.setGradientFill (metal);
            g.fillRect (r);
            skins::paintPattern (g, r, sk.pattern);

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
                const auto& skc = skins::get (skin);
                if (skc.cheek == skins::Cheek::walnut)
                    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a2c17), 0.0f, w.getY(), juce::Colour (0xff24140a), 0.0f, w.getBottom(), false));
                else
                    g.setGradientFill (juce::ColourGradient (skc.cheekA.darker (0.3f), 0.0f, w.getY(), skc.cheekB.darker (0.5f), 0.0f, w.getBottom(), false));
                g.fillPath (p);
            }
            const auto& sk = skins::get (skin);
            if (skin == 2)   // Pool Party: swimming lane rope
            {
                for (float x = r.getX(); x < r.getRight(); x += 32.0f)
                {
                    g.setColour (juce::Colour (0xfff2f8fa)); g.fillRect (x, r.getY(), 16.0f, r.getHeight());
                    g.setColour (juce::Colour (0xffff6fb4)); g.fillRect (x + 16.0f, r.getY(), 16.0f, r.getHeight());
                }
            }
            else if (skin == 6)   // Grand Prix: chequered flag
            {
                for (float y = r.getY(); y < r.getBottom(); y += 7.0f)
                    for (float x = r.getX(); x < r.getRight(); x += 7.0f)
                    { g.setColour ((((int) ((x - r.getX()) / 7.0f) + (int) ((y - r.getY()) / 7.0f)) % 2) ? juce::Colour (0xff111111) : juce::Colour (0xfff2f2f2)); g.fillRect (x, y, 7.0f, 7.0f); }
            }
            else
            {
                g.setGradientFill (juce::ColourGradient (sk.frontTop, 0.0f, r.getY(), sk.frontBottom, 0.0f, r.getBottom(), false));
                g.fillRect (r);
            }
            g.setColour (juce::Colour (0xffc9ced4));
            g.fillRect (r.withHeight (2.0f));
        }

        void paintRear (juce::Graphics& g)
        {
            using namespace geo;
            // speaker grille
            auto grille = juce::Rectangle<float> ((float) innerX, (float) rearY + 3.0f, 210.0f, 52.0f);
            const auto& sk = skins::get (skin);
            g.setGradientFill (juce::ColourGradient (sk.grilleA, 0.0f, grille.getY(), sk.grilleB, 0.0f, grille.getBottom(), false));
            g.fillRoundedRectangle (grille, 12.0f);
            skins::paintGrille (g, grille, sk);
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
            g.setColour (sk.engrave);
            if (juce::String (sk.edition).isNotEmpty())
            {
                // edition badge in place of the engraved name
                const auto badgeText = juce::String::fromUTF8 (sk.edition);
                g.setFont (spaced (9.5f, true, 0.2f));
                const float bw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), badgeText) + 24.0f;
                auto badge = juce::Rectangle<float> (tx, (float) rearY + 33.0f, bw, 20.0f);
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.fillRoundedRectangle (badge.translated (0.0f, 2.0f), 10.0f);
                g.setColour (sk.badgeBg);
                g.fillRoundedRectangle (badge, 10.0f);
                g.setColour (sk.badgeFg);
                g.drawText (badgeText, badge, juce::Justification::centred, false);
            }
            else
            g.drawText ("HAMAMATSU 108", juce::Rectangle<float> (tx, (float) rearY + 36.0f, 240.0f, 14.0f), juce::Justification::centredLeft, false);

            // model display (models arrive in step 4)
            auto disp = juce::Rectangle<float> ((float) (innerX + innerW - 230), (float) rearY + 6.0f, 230.0f, 46.0f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (disp.expanded (3.0f), 11.0f);
            g.setColour (juce::Colour (0xff1a1c1f));
            g.fillRoundedRectangle (disp.expanded (2.0f), 10.0f);
            drawDisplay (g, disp, 9.0f);


        }

        void engravedLabel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& left, const juce::String& right = {})
        {
            g.setFont (spaced (9.0f, true, 0.28f));
            g.setColour (skins::get (skin).engrave);
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
            engravedLabel (g, padArea().toFloat().withY (y).withHeight (12.0f), "CHORD BUTTONS", juce::String::fromUTF8 ("CTRL+CLICK = STACK CHORDS   \xe2\x97\x8f IN KEY"));
            engravedLabel (g, plateArea().toFloat().withY (y).withHeight (12.0f), "STRUM PLATE");
            engravedLabel (g, moduleArea().toFloat().withY (y).withHeight (12.0f), juce::String::fromUTF8 ("VOICE \xc2\xb7 LIVE STRUM"));
        }

        void paintDeck (juce::Graphics& g)
        {
            const auto r = geo::deckArea().toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (r.translated (0.0f, 5.0f).expanded (1.0f), 14.0f);

            const auto& sk = skins::get (skin);
            juce::ColourGradient silver (sk.deckTop, 0.0f, r.getY(), sk.deckBottom, 0.0f, r.getBottom(), false);
            silver.addColour (0.55, sk.deckMid);
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
                if (gr.selector != 0) { drawSeparator (g, groups, i); continue; }
                g.setFont (spaced (9.0f, true, 0.25f));
                g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.drawText (gr.name, b.withHeight (12.0f).translated (0.0f, 1.0f), juce::Justification::centred, false);
                g.setColour (gr.macro ? th().deep : juce::Colour (0xff3a3f46));
                g.drawText (gr.name, b.withHeight (12.0f), juce::Justification::centred, false);
                drawSeparator (g, groups, i);
            }
        }

        static void drawSeparator (juce::Graphics& g, const juce::Array<juce::Rectangle<int>>& groups, int i)
        {
            if (i >= groups.size() - 1) return;
            const auto b = groups[i].toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.drawVerticalLine ((int) b.getRight(), b.getY() + 2.0f, b.getBottom() - 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawVerticalLine ((int) b.getRight() + 1, b.getY() + 2.0f, b.getBottom() - 2.0f);
        }

        std::unique_ptr<juce::Drawable> logo;
        int skin = 0;
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
            std::array<int, 4> st;
            for (size_t i = 0; i < 4; ++i) st[i] = processor.stackGui[i].load();
            if (active != activeChord || keyRoot != key || minor != keyMinor || st != stack)
            {
                stack = st;
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
            const bool on = idx == activeChord || idx == pressed || std::find (stack.begin(), stack.end(), idx) != stack.end();
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
                g.setColour (th().accent.withAlpha (0.25f));
                g.fillRoundedRectangle (b.expanded (5.0f), 10.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e6f6a), 0.0f, b.getY(), juce::Colour (0xff1c4a47), 0.0f, b.getBottom(), false));
                g.fillRoundedRectangle (b, 7.0f);
                g.setColour (th().accent);
                g.drawRoundedRectangle (b.reduced (0.5f), 7.0f, 2.0f);
                g.setColour (juce::Colours::white);
                g.setFont (LookAndFeel::uiFont (10.0f, true));
                g.drawText (chords::shortName (idx), b, juce::Justification::centred, false);
            }

            const bool inKey = chords::isInKey (idx, key, keyMinor);
            auto dot = juce::Rectangle<float> (4.0f, 4.0f).withPosition (b.getRight() - 8.0f, b.getY() + 3.0f);
            if (inKey)
            {
                g.setColour (th().accent.withAlpha (0.4f));
                g.fillEllipse (dot.expanded (2.0f));
                g.setColour (th().accent);
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
                        if (e.mods.isCtrlDown() || e.mods.isShiftDown() || e.mods.isCommandDown())
                            processor.addChord (pressed);     // stack it on the chord that is on
                        else
                            processor.latchChord (pressed);   // stays on until clicked again or another chord is chosen
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
        std::array<int, 4> stack { -1, -1, -1, -1 };
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
                juce::ColourGradient haze (th().accent.withAlpha (juce::jmin (0.3f, sum * 0.04f)), gl.getCentreX(), gl.getCentreY() + 20.0f,
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
                    g.setColour (th().accent.withAlpha (0.35f * v));
                    g.fillRect (gl.getX() + 6.0f, y - 2.5f, gl.getWidth() - 12.0f, 5.0f);
                }
                juce::ColourGradient s (th().accent.withAlpha (0.0f), gl.getX(), y, th().accent.withAlpha (0.0f), gl.getRight(), y, false);
                s.addColour (0.18, th().accent.withAlpha (alpha));
                s.addColour (0.5, th().soft.withAlpha (alpha));
                s.addColour (0.82, th().accent.withAlpha (alpha));
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
            st.labelHeight = 8.0f;
            const char* ids[]    = { "harpVoice", "morph", "strumRate", "strumFeel", "strumSpeed", "arpPattern", "arpChance" };
            const char* labels[] = { "VOICE", "MORPH", "RATE", "FEEL", "SPEED", "PATTERN", "CHANCE" };
            for (int i = 0; i < 7; ++i)
                addChildComponent (knobs.add (new Knob (processor.apvts, ids[i], labels[i], 24, st)));
            for (int i = 0; i < 5; ++i) knobs[i]->setVisible (true);
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
            const float w = 192.0f / 5.0f;
            for (int i = 0; i < knobs.size(); ++i)
            {
                const int col = i < 5 ? i : i - 2;   // PATTERN / CHANCE take the places of FEEL / SPEED
                knobs[i]->setBounds (juce::roundToInt (12.0f + col * w), 60, juce::roundToInt (w), 66);
            }
        }

        void update (float levelL, float levelR, int chord)
        {
            auto& s = processor.apvts;
            const int newVoice = juce::roundToInt (plain (s, "harpVoice"));
            const int newVoice2 = juce::roundToInt (plain (s, "harpVoice2"));
            const bool newDual = plain (s, "dual") > 0.5f;
            const int newMode = juce::roundToInt (plain (s, "strumMode"));
            const int newRate = juce::roundToInt (plain (s, "strumRate"));
            const int newPattern = juce::roundToInt (plain (s, "arpPattern"));
            const int newStrumPat = juce::roundToInt (plain (s, "strumPattern"));
            const int newKeys = juce::roundToInt (plain (s, "keysPlay"));
            const int newBank = processor.memBank.load();
            const int newStep = processor.memPlayStep.load();
            const bool newHold = plain (s, "hold") > 0.5f;
            const int newKey = juce::roundToInt (plain (s, "keyRoot"));
            const bool newMinor = plain (s, "keyMode") > 0.5f;
            const int newTr = juce::roundToInt (plain (s, "transpose"));

            if (newVoice != voice || newVoice2 != voice2 || newDual != dual || newMode != mode || newRate != rate || newPattern != pattern
                || newStrumPat != strumPat || newKeys != keysPlay || newBank != bank || newStep != step || newHold != hold || newKey != key || newMinor != minor || newTr != transpose || chord != chordIndex)
            {
                if (newMode != mode)
                {
                    const bool arp = newMode == 1;
                    knobs[3]->setVisible (! arp); knobs[4]->setVisible (! arp);
                    knobs[5]->setVisible (arp);   knobs[6]->setVisible (arp);
                }
                voice = newVoice; voice2 = newVoice2; dual = newDual; mode = newMode; rate = newRate; pattern = newPattern; strumPat = newStrumPat; keysPlay = newKeys;
                bank = newBank; step = newStep; hold = newHold; key = newKey; minor = newMinor; transpose = newTr; chordIndex = chord;
                repaint();
            }

            const int segL = segments (levelL), segR = segments (levelR);
            if (segL != meterL || segR != meterR)
            {
                meterL = segL; meterR = segR;
                repaint (meterArea().expanded (2.0f).toNearestInt());
            }
        }

        static juce::String shortVoice (int v)
        {
            static const char* n[] = { "CHIME", "CELESTA", "VIBES", "GLASS", "PLUCK", "FLUTE", "MARIMBA", "STEEL", "E-PIANO", "STRINGS" };
            return n[juce::jlimit (0, 9, v)];
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
            glowText (g, dual ? shortVoice (voice) + "+" + shortVoice (voice2) : harp::voiceNames()[voice].toUpperCase(), l1, juce::Justification::centredRight);
            auto l2 = lines.removeFromTop (16.0f);
            const auto rateText = Hamamatsu108Processor::strumRateNames()[rate];
            juce::String left = "STRUM", right;
            switch (mode)
            {
                case 0: right = (keysPlay > 0 ? juce::String ("KEYS ") : juce::String ("LIVE "))
                              + (strumPat == 0 ? rateText : Hamamatsu108Processor::strumPatternNames()[strumPat].toUpperCase().removeCharacters (" /34")); break;
                case 1: left = "ARP"; right = Hamamatsu108Processor::arpPatternNames()[pattern].toUpperCase().removeCharacters (" ") + " " + rateText; break;
                case 2: left = "MEMO " + juce::String::charToString ((juce::juce_wchar) ('A' + bank));
                        right = step >= 0 ? "STEP " + juce::String (step + 1) : juce::String ("EMPTY"); break;
                default: right = "OFF"; break;
            }
            glowText (g, left, l2, juce::Justification::centredLeft, 0.75f);
            glowText (g, right, l2, juce::Justification::centredRight, 0.75f);

            drawHardButton (g, dualArea(), "DUAL", dual, true, false);
            drawHardButton (g, holdArea(), "HOLD", hold, true, false);
            for (int i = 0; i < 4; ++i)
                drawHardButton (g, modeArea (i), Hamamatsu108Processor::strumModeNames()[i].toUpperCase(), mode == i, true, true);

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
                        const auto c = i >= 13 ? th().soft : th().accent;
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
            if (juce::Rectangle<float> (12.0f, 12.0f, 192.0f, 42.0f).contains (p)) { showPresetMenu(); return; }
            if (holdArea().contains (p))       setParam (s, "hold", hold ? 0.0f : 1.0f);
            else if (dualArea().contains (p))  showDualMenu();
            else if (trDown().contains (p))    setParam (s, "transpose", (float) juce::jmax (-12, transpose - 1));
            else if (trUp().contains (p))      setParam (s, "transpose", (float) juce::jmin (12, transpose + 1));
            else if (keyArea().contains (p))   showKeyMenu();
            else
                for (int i = 0; i < 4; ++i)
                    if (modeArea (i).contains (p))
                    {
                        if (i == mode && i == 0)      showLiveMenu();
                        else if (i == mode && i == 1) showArpMenu();
                        else if (i == mode && i == 2) showMemoMenu();
                        else
                        {
                            setParam (s, "strumMode", (float) i);
                            if (i == 0) message ("LIVE: the hand plays the chord like a person. Click LIVE again to pick the strum pattern and how your keys play.");
                            if (i == 1) message ("ARP: arpeggio over the chord. PATTERN and CHANCE knobs, click ARP again for Regen.");
                            if (i == 2) message ("MEMO: plays the chord sequence with the LIVE pattern. Click MEMO again to load a ready-made progression, or press REC and click chords.");
                        }
                    }
        }

        std::function<void (const juce::String&)> onMessage;

    private:
        void message (const juce::String& m) { if (onMessage) onMessage (m); }

        void popup (juce::PopupMenu& m, juce::Rectangle<float> area, std::function<void (int)> f)
        {
            juce::Component::SafePointer<VoiceModule> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (area.toNearestInt())),
                             [safe, f] (int r) { if (safe != nullptr && r > 0) f (r); });
        }

        // quick preset choice while playing: click the VOICE display
        void showPresetMenu()
        {
            auto& pm = *processor.presetManager;
            juce::PopupMenu m;
            m.addSectionHeader ("Presets (click the display to change while playing)");
            for (int c = 0; c < presets::categories().size(); ++c)
            {
                juce::PopupMenu sub;
                for (int i = 0; i < (int) pm.all().size(); ++i)
                    if (pm.all()[(size_t) i].category == presets::categories()[c])
                        sub.addItem (1 + i, pm.all()[(size_t) i].name + (pm.all()[(size_t) i].factory ? juce::String() : juce::String (" (user)")), true, i == pm.getCurrent());
                m.addSubMenu (presets::categories()[c], sub);
            }
            {
                juce::PopupMenu mine;
                juce::StringArray banks { "" };
                banks.addArray (pm.userBanks());
                for (auto& bk : banks)
                {
                    juce::PopupMenu sub;
                    for (int i = 0; i < (int) pm.all().size(); ++i)
                        if (! pm.all()[(size_t) i].factory && pm.all()[(size_t) i].bank == bk)
                            sub.addItem (1 + i, pm.all()[(size_t) i].name, true, i == pm.getCurrent());
                    if (sub.getNumItems() == 0) continue;
                    if (bk.isEmpty()) mine.addSubMenu ("(no bank)", sub); else mine.addSubMenu (bk, sub);
                }
                if (mine.getNumItems() > 0) { m.addSeparator(); m.addSubMenu ("My presets", mine); }
            }
            popup (m, { 12.0f, 12.0f, 192.0f, 42.0f }, [this] (int r)
            {
                processor.presetManager->load (r - 1);
                processor.previewPreset();
                message ("Preset: " + processor.presetManager->currentName());
            });
        }

        void showDualMenu()
        {
            juce::PopupMenu m;
            m.addSectionHeader ("DUAL: a second harp voice (layer B). MORPH blends A and B.");
            m.addItem (1, "Dual off", true, ! dual);
            m.addSeparator();
            for (int i = 0; i < harp::voiceNames().size(); ++i)
                m.addItem (100 + i, "Layer B: " + harp::voiceNames()[i], true, dual && voice2 == i);
            popup (m, dualArea(), [this] (int r)
            {
                if (r == 1) setParam (processor.apvts, "dual", 0.0f);
                else { setParam (processor.apvts, "harpVoice2", (float) (r - 100)); setParam (processor.apvts, "dual", 1.0f); }
            });
        }

        void showLiveMenu()
        {
            juce::PopupMenu m;
            m.addSectionHeader ("LIVE: how the hand plays");
            for (int i = 0; i < Hamamatsu108Processor::strumPatternNames().size(); ++i)
                m.addItem (1 + i, Hamamatsu108Processor::strumPatternNames()[i] + (i == 0 ? juce::String (" (plain down / up at RATE)") : juce::String()), true, strumPat == i);
            m.addSeparator();
            m.addSectionHeader ("Your keys / piano roll notes (outside the chord zones)");
            static const char* keyText[] = { "Notes: play the exact pitch", "Strum: each key press strums the held notes", "Pattern: the pattern plays the held notes",
                                             "Plate only: hold the keys, strum them on the plate / pitch wheel" };
            for (int i = 0; i < 4; ++i) m.addItem (100 + i, keyText[i], true, keysPlay == i);
            m.addSeparator();
            juce::PopupMenu shape, speed, start;
            static const char* shapes[] = { "Linear", "Natural", "Fast start", "Fast finish" };
            const int sh = juce::roundToInt (plain (processor.apvts, "strumShape")), sp = juce::roundToInt (plain (processor.apvts, "strumSpeedMul")), st = juce::roundToInt (plain (processor.apvts, "strumRestart"));
            for (int i = 0; i < 4; ++i) shape.addItem (200 + i, shapes[i], true, sh == i);
            speed.addItem (210, "Half", true, sp == 0); speed.addItem (211, "Normal", true, sp == 1); speed.addItem (212, "Double", true, sp == 2);
            start.addItem (220, "Follow the bar", true, st == 0); start.addItem (221, "Restart on every chord", true, st == 1);
            m.addSubMenu ("Stroke shape", shape);
            m.addSubMenu ("Pattern speed", speed);
            m.addSubMenu ("Pattern start", start);
            popup (m, modeArea (0), [this] (int r)
            {
                if (r >= 220) setParam (processor.apvts, "strumRestart", (float) (r - 220));
                else if (r >= 210) setParam (processor.apvts, "strumSpeedMul", (float) (r - 210));
                else if (r >= 200) setParam (processor.apvts, "strumShape", (float) (r - 200));
                else if (r >= 100) setParam (processor.apvts, "keysPlay", (float) (r - 100));
                else setParam (processor.apvts, "strumPattern", (float) (r - 1));
            });
        }

        void showArpMenu()
        {
            juce::PopupMenu m;
            const int regen = juce::roundToInt (plain (processor.apvts, "arpRegen"));
            m.addSectionHeader ("Regen: the pattern changes itself every...");
            for (int i = 0; i < Hamamatsu108Processor::arpRegenNames().size(); ++i)
                m.addItem (1 + i, Hamamatsu108Processor::arpRegenNames()[i], true, regen == i);
            popup (m, modeArea (1), [this] (int r) { setParam (processor.apvts, "arpRegen", (float) (r - 1)); });
        }

        void showMemoMenu()
        {
            juce::PopupMenu m;
            const int len = juce::roundToInt (plain (processor.apvts, "memStep"));
            m.addSectionHeader ("MEMO plays the chord sequence with the LIVE pattern");
            juce::PopupMenu prog;
            const auto& all = Hamamatsu108Processor::progressions();
            for (int i = 0; i < (int) all.size(); ++i) prog.addItem (200 + i, all[(size_t) i].name);
            m.addSubMenu ("Load a progression (in the key) into bank " + juce::String::charToString ((juce::juce_wchar) ('A' + bank)), prog);
            juce::PopupMenu pat;
            for (int i = 0; i < Hamamatsu108Processor::strumPatternNames().size(); ++i)
                pat.addItem (300 + i, Hamamatsu108Processor::strumPatternNames()[i], true, strumPat == i);
            m.addSubMenu ("Strum pattern", pat);
            m.addSeparator();
            m.addSectionHeader ("Each memory chord lasts...");
            static const int order[] = { 3, 0, 1, 2, 4 };
            for (int o : order)
                m.addItem (1 + o, Hamamatsu108Processor::memStepNames()[o], true, len == o);
            popup (m, modeArea (2), [this] (int r)
            {
                if (r >= 300) setParam (processor.apvts, "strumPattern", (float) (r - 300));
                else if (r >= 200)
                {
                    processor.memLoad (processor.memBank.load(), r - 200, juce::roundToInt (plain (processor.apvts, "keyRoot")));
                    message ("Loaded: " + juce::String (Hamamatsu108Processor::progressions()[(size_t) (r - 200)].name) + ". MEMO plays it in time.");
                }
                else setParam (processor.apvts, "memStep", (float) (r - 1));
            });
        }

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
            popup (menu, keyArea(), [this] (int r)
            {
                setParam (processor.apvts, "keyRoot", (float) ((r - 1) % 12));
                setParam (processor.apvts, "keyMode", (float) ((r - 1) / 12));
            });
        }

        static int segments (float level)
        {
            const float db = juce::Decibels::gainToDecibels (level, -60.0f);
            return juce::jlimit (0, 16, (int) std::ceil ((db + 48.0f) / 3.0f));
        }

        Hamamatsu108Processor& processor;
        int strumPat = -1, keysPlay = -1;
        int voice = 0, voice2 = 9, mode = -1, rate = 2, pattern = 0, bank = 0, step = -1, key = 0, transpose = 0, chordIndex = -1, meterL = 0, meterR = 0;
        bool hold = false, minor = false, dual = false;
    };

    //==========================================================================
    // Everything the harp plays is captured; drag this pad into the DAW to get it as a MIDI clip.
    class MidiCapture : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit MidiCapture (Hamamatsu108Processor& p) : processor (p)
        {
            setTooltip (juce::String::fromUTF8 ("MIDI capture \xc2\xb7 what you play is recorded here. Drag it into the FL Studio playlist or piano roll. Right-click: CHORDS (long stacked notes) / DETAIL (every plucked note) / BOTH, chord grid, clear."));
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        }

        static juce::Rectangle<int> area() { return { geo::innerX + 500, geo::rearY + 6, 190, 46 }; }

        void update()
        {
            const int ready = processor.captureFifo.getNumReady();
            bool changed = false;
            if (ready > 0)
            {
                const auto scope = processor.captureFifo.read (ready);
                auto take = [this] (int start, int size)
                {
                    for (int i = 0; i < size; ++i)
                    {
                        const auto n = processor.captureBuffer[(size_t) (start + i)];
                        if (! notes.empty() && n.seconds - notes.back().seconds > 4.0)
                            notes.clear();   // a pause of 4 s starts a new take
                        if (notes.size() < 8000) notes.push_back (n);
                    }
                };
                take (scope.startIndex1, scope.blockSize1);
                take (scope.startIndex2, scope.blockSize2);
                lastActivity = juce::Time::getMillisecondCounterHiRes();
                changed = true;
            }
            const bool nowLive = juce::Time::getMillisecondCounterHiRes() - lastActivity < 250.0;
            if (changed || nowLive != live) { live = nowLive; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (r.expanded (1.0f), 10.0f);
            drawDisplay (g, r, 9.0f);
            if (hover || dragging)
            {
                g.setColour (th().accent.withAlpha (0.6f));
                g.drawRoundedRectangle (r, 9.0f, 1.5f);
            }

            drawLed (g, juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 14.0f, r.getY() + 14.0f }), live);
            auto text = r.reduced (26.0f, 5.0f).withTrimmedLeft (-2.0f);
            g.setFont (LookAndFeel::monoFont (11.0f));
            static const char* modeNames[] = { "DETAIL", "CHORDS", "BOTH" };
            auto top = text.removeFromTop (17.0f);
            glowText (g, "MIDI CAPTURE", top, juce::Justification::centredLeft, 0.7f);
            glowText (g, modeNames[juce::jlimit (0, 2, processor.captureMode.load())], top, juce::Justification::centredRight, 0.5f);
            g.setFont (LookAndFeel::monoFont (13.0f));
            if (notes.empty())
                glowText (g, "play, then drag", text, juce::Justification::centredLeft, 0.55f);
            else
            {
                const double secs = notes.back().seconds - notes.front().seconds;
                const double bars = secs * processor.hostBpm.load() / 240.0;
                int ons = 0;
                for (auto& n : notes) if (n.on) ++ons;
                int blocks = 0;
                for (auto& n : notes) if (n.on && n.layer == 3) ++blocks;
                const int mode = processor.captureMode.load();
                const juce::String what = mode == 1 ? juce::String (blocks) + " chord notes" : juce::String (ons) + " notes";
                glowText (g, juce::String::fromUTF8 ("DRAG \xe2\x87\xa2 ") + what + " " + juce::String (juce::jmax (1, (int) std::ceil (bars))) + " bar",
                          text, juce::Justification::centredLeft);
            }
        }

        void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() || (notes.empty() && ! e.mods.isAnyModifierKeyDown()))
                showMenu();
        }

        void showMenu()
        {
            const int mode = processor.captureMode.load(), q = processor.captureQuant.load();
            juce::PopupMenu m;
            m.addSectionHeader ("Drag the take as...");
            m.addItem (1, "Chords: long stacked notes (piano roll ready)", true, mode == 1);
            m.addItem (2, "Detail: every plucked note, as played", true, mode == 0);
            m.addItem (3, "Both: chords + detail on separate tracks", true, mode == 2);
            m.addSeparator();
            juce::PopupMenu grid;
            static const char* qn[] = { "Off (as played)", "1/16", "1/8", "1/4", "1 bar" };
            for (int i = 0; i < 5; ++i) grid.addItem (10 + i, qn[i], true, q == i);
            m.addSubMenu ("Chord grid (snap chord blocks)", grid);
            m.addSeparator();
            m.addItem (20, "Clear take", ! notes.empty());
            juce::Component::SafePointer<MidiCapture> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
            {
                if (safe == nullptr || r <= 0) return;
                if (r == 1) safe->processor.captureMode.store (1);
                if (r == 2) safe->processor.captureMode.store (0);
                if (r == 3) safe->processor.captureMode.store (2);
                if (r >= 10 && r < 15) safe->processor.captureQuant.store (r - 10);
                if (r == 20) safe->notes.clear();
                safe->repaint();
            });
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragging || notes.empty() || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 6) return;
            const auto file = writeMidiFile();
            if (! file.existsAsFile()) return;
            dragging = true;
            juce::Component::SafePointer<MidiCapture> safe (this);
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                       [safe] { if (safe != nullptr) { safe->dragging = false; safe->repaint(); } });
        }

        void mouseUp (const juce::MouseEvent&) override { dragging = false; repaint(); }

    private:
        juce::File writeMidiFile() const
        {
            const double bpm = processor.hostBpm.load();
            const int ppq = 960;
            const double t0 = notes.front().seconds;
            auto toTicks = [&] (double sec) { return (sec - t0) * bpm / 60.0 * ppq; };
            const int mode = processor.captureMode.load();
            static const double gridQ[] = { 0.0, 0.25, 0.5, 1.0, 4.0 };
            const double grid = gridQ[juce::jlimit (0, 4, processor.captureQuant.load())] * ppq;
            auto snap = [grid] (double t) { return grid > 0.0 ? std::round (t / grid) * grid : t; };

            // tracks: chord blocks / harp detail / chord layer / bass (FL Studio puts each on its own channel)
            juce::Array<int> layers;
            if (mode != 0) layers.add (3);
            if (mode != 1) layers.add (0);
            layers.add (1);
            layers.add (2);

            juce::MidiFile mf;
            mf.setTicksPerQuarterNote (ppq);
            const double endTicks = toTicks (notes.back().seconds) + ppq;
            bool first = true;
            for (int layer : layers)
            {
                juce::MidiMessageSequence seq;
                if (first) seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / bpm)), 0.0);
                static const char* trackNames[] = { "Hamamatsu harp (detail)", "Hamamatsu chord layer", "Hamamatsu bass", "Hamamatsu chords" };
                seq.addEvent (juce::MidiMessage::textMetaEvent (3, trackNames[layer]), 0.0);
                const int channel = layer == 3 ? 1 : (mode == 0 ? 1 + layer : 2 + layer);
                int count = 0;
                for (size_t i = 0; i < notes.size(); ++i)
                {
                    const auto& n = notes[i];
                    if (n.layer != layer || ! n.on) continue;
                    double on = toTicks (n.seconds);
                    double off = layer == 0 ? on + ppq / 2.0 : endTicks;
                    for (size_t j = i + 1; j < notes.size(); ++j)
                        if (notes[j].layer == layer && notes[j].note == n.note)
                        {
                            const double t = toTicks (notes[j].seconds) - 1.0;
                            if (layer == 0) { if (notes[j].on) off = juce::jmin (off, t); }
                            else { off = t; }
                            break;
                        }
                    if (layer == 3)
                    {
                        on = snap (on);
                        off = juce::jmax (on + (grid > 0.0 ? grid : ppq / 4.0), snap (off + 1.0) - 1.0);
                    }
                    const auto vel = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (n.velocity * 127.0f));
                    seq.addEvent (juce::MidiMessage::noteOn (channel, n.note, vel), on);
                    seq.addEvent (juce::MidiMessage::noteOff (channel, n.note), juce::jmax (on + 1.0, off));
                    ++count;
                }
                seq.updateMatchedPairs();
                seq.sort();
                if (count > 0 || first) { mf.addTrack (seq); first = false; }
            }

            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Kuprell Hamamatsu 108");
            dir.createDirectory();
            auto file = dir.getChildFile ("Hamamatsu 108 take " + juce::Time::getCurrentTime().formatted ("%H%M%S") + ".mid");
            file.deleteFile();
            juce::FileOutputStream out (file);
            if (out.openedOk())
                mf.writeTo (out);
            return file;
        }

        Hamamatsu108Processor& processor;
        std::vector<Hamamatsu108Processor::CapturedNote> notes;
        double lastActivity = 0.0;
        bool live = false, hover = false, dragging = false;
    };


    //==========================================================================
    // Pill above a deck group: shows and picks the chord layer / bass type.
    class GroupSelector : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        GroupSelector (Hamamatsu108Processor& p, int kindIn) : processor (p), kind (kindIn)
        {
            setTooltip (kind == 1 ? "Chord layer: sustained chord under the harp. Click to choose the sound and voicing."
                                  : "Bass: plays the chord root. Click to choose the sound and pattern.");
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        juce::String text() const
        {
            auto& s = processor.apvts;
            if (kind == 1) return "CHORD  " + layers::chordTypeNames()[juce::roundToInt (plain (s, "chordType"))].toUpperCase();
            const int t = juce::roundToInt (plain (s, "bassType"));
            if (t == 0) return "BASS  OFF";
            static const char* modes[] = { "ROOT", "R+5", "WALK" };
            return "BASS  " + layers::bassTypeNames()[t].toUpperCase() + " " + modes[juce::jlimit (0, 2, juce::roundToInt (plain (s, "bassMode")))];
        }

        void update() { const auto t = text(); if (t != shown) { shown = t; repaint(); } }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (juce::Colour (0xff1a1c1f));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (hover ? th().accent : juce::Colours::black);
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
            const bool off = shown.endsWith ("OFF");
            g.setFont (LookAndFeel::monoFont (10.5f));
            g.setColour (off ? juce::Colour (0xff7e8187) : th().soft);
            g.drawText (shown + juce::String::fromUTF8 (" \xe2\x96\xbe"), r, juce::Justification::centred, false);
        }

        void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

        void mouseDown (const juce::MouseEvent&) override
        {
            auto& s = processor.apvts;
            juce::PopupMenu m;
            if (kind == 1)
            {
                const int t = juce::roundToInt (plain (s, "chordType")), v = juce::roundToInt (plain (s, "voicing"));
                m.addSectionHeader ("Chord layer sound");
                for (int i = 0; i < layers::chordTypeNames().size(); ++i) m.addItem (1 + i, layers::chordTypeNames()[i], true, t == i);
                juce::PopupMenu inv;
                for (int i = 0; i < layers::voicingNames().size(); ++i) inv.addItem (100 + i, layers::voicingNames()[i], true, v == i);
                m.addSubMenu ("Voicing", inv);
            }
            else
            {
                const int t = juce::roundToInt (plain (s, "bassType")), md = juce::roundToInt (plain (s, "bassMode"));
                m.addSectionHeader ("Bass sound");
                for (int i = 0; i < layers::bassTypeNames().size(); ++i) m.addItem (1 + i, layers::bassTypeNames()[i], true, t == i);
                juce::PopupMenu pat;
                for (int i = 0; i < layers::bassModeNames().size(); ++i) pat.addItem (100 + i, layers::bassModeNames()[i], true, md == i);
                m.addSubMenu ("Pattern", pat);
            }
            juce::Component::SafePointer<GroupSelector> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
            {
                if (safe == nullptr || r <= 0) return;
                auto& st = safe->processor.apvts;
                if (safe->kind == 1) setParam (st, r >= 100 ? "voicing" : "chordType", (float) (r >= 100 ? r - 100 : r - 1));
                else                 setParam (st, r >= 100 ? "bassMode" : "bassType", (float) (r >= 100 ? r - 100 : r - 1));
                safe->update();
            });
        }

    private:
        Hamamatsu108Processor& processor;
        int kind;
        juce::String shown;
        bool hover = false;
    };

    //==========================================================================
    // Chord Memory on the front edge: banks A-H, 16 steps, REC, LEN, DRAG.
    class MemoryRail : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit MemoryRail (Hamamatsu108Processor& p) : processor (p) {}

        static juce::Rectangle<int> area() { return { geo::bodyX + geo::cheekW, geo::bodyY + geo::panelH, geo::bodyW - 2 * geo::cheekW, geo::frontH }; }

        juce::Rectangle<float> bankArea (int b) const { return { 16.0f + b * 22.0f, 9.0f, 20.0f, 24.0f }; }
        juce::Rectangle<float> stepArea (int i) const { return { 204.0f + i * 36.0f, 9.0f, 34.0f, 24.0f }; }
        juce::Rectangle<float> recArea() const  { return { 790.0f, 8.0f, 48.0f, 26.0f }; }
        juce::Rectangle<float> lenArea() const  { return { 844.0f, 9.0f, 60.0f, 24.0f }; }
        juce::Rectangle<float> dragArea() const { return { 910.0f, 9.0f, 108.0f, 24.0f }; }

        void update()
        {
            juce::String sig;
            const int bank = processor.memBank.load();
            sig << bank << ":" << processor.memLength[(size_t) bank].load() << ":" << processor.memPlayStep.load() << ":"
                << (processor.memRecord.load() ? 1 : 0) << ":" << juce::roundToInt (plain (processor.apvts, "memStep"));
            for (int i = 0; i < Hamamatsu108Processor::memSteps; ++i) sig << "," << processor.memChord (bank, i);
            const bool blink = processor.memRecord.load() && (juce::Time::getMillisecondCounter() / 400) % 2 == 0;
            sig << (blink ? "b" : "");
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            const int bank = processor.memBank.load();
            const int len = processor.memLength[(size_t) bank].load();
            const int playing = processor.memPlayStep.load();
            const bool rec = processor.memRecord.load();

            g.setFont (spaced (8.0f, true, 0.2f));
            for (int b = 0; b < Hamamatsu108Processor::memBanks; ++b)
            {
                auto r = bankArea (b);
                drawDisplay (g, r, 4.0f);
                g.setFont (LookAndFeel::monoFont (12.0f));
                const auto t = juce::String::charToString ((juce::juce_wchar) ('A' + b));
                if (b == bank) glowText (g, t, r, juce::Justification::centred);
                else { g.setColour (juce::Colour (processor.memLength[(size_t) b].load() > 0 ? 0xff9aa0a7 : 0xff4a4e54)); g.drawText (t, r, juce::Justification::centred, false); }
            }

            for (int i = 0; i < Hamamatsu108Processor::memSteps; ++i)
            {
                auto r = stepArea (i);
                const int c = i < len ? processor.memChord (bank, i) : -1;
                if (i == playing)
                {
                    g.setColour (th().accent.withAlpha (0.3f));
                    g.fillRoundedRectangle (r.expanded (2.0f), 5.0f);
                }
                drawDisplay (g, r, 4.0f);
                if (i == playing) { g.setColour (th().accent); g.drawRoundedRectangle (r, 4.0f, 1.5f); }
                if (rec && i == len && (juce::Time::getMillisecondCounter() / 400) % 2 == 0)
                { g.setColour (th().accent.withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 1.0f); }
                g.setFont (LookAndFeel::monoFont (c >= 0 ? 10.5f : 9.0f));
                if (c >= 0) glowText (g, chords::shortName (c), r, juce::Justification::centred, i == playing ? 1.0f : 0.8f);
                else { g.setColour (juce::Colour (0xff3a3e44)); g.drawText (juce::String (i + 1), r, juce::Justification::centred, false); }
            }

            drawHardButton (g, recArea(), "REC", rec, true, false);
            drawDisplay (g, lenArea(), 4.0f);
            g.setFont (LookAndFeel::monoFont (10.5f));
            glowText (g, Hamamatsu108Processor::memStepNames()[juce::roundToInt (plain (processor.apvts, "memStep"))].toUpperCase(), lenArea(), juce::Justification::centred, 0.8f);
            drawDisplay (g, dragArea(), 4.0f);
            glowText (g, juce::String::fromUTF8 ("DRAG MIDI \xe2\x87\xa2"), dragArea(), juce::Justification::centred, len > 0 ? 1.0f : 0.35f);
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const auto p = e.position;
            juce::String tip;
            if (p.x < 200) tip = "Memory banks A-H: 8 chord sequences, saved with the project.";
            else if (p.x < 784) tip = "Steps: click to hear a chord, right-click to delete. In MEMO mode they play in time.";
            else if (recArea().contains (p)) tip = "REC: click chords (buttons or keyboard) to add them to this bank.";
            else if (lenArea().contains (p)) tip = "How long each chord lasts (click to change).";
            else if (dragArea().contains (p)) tip = "Drag this into FL Studio: the sequence as MIDI chords.";
            setTooltip (tip);
            setMouseCursor (dragArea().contains (p) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto p = e.position;
            const int bank = processor.memBank.load();
            for (int b = 0; b < Hamamatsu108Processor::memBanks; ++b)
                if (bankArea (b).contains (p)) { processor.memBank.store (b); update(); return; }

            for (int i = 0; i < Hamamatsu108Processor::memSteps; ++i)
                if (stepArea (i).contains (p))
                {
                    const int c = processor.memChord (bank, i);
                    if (e.mods.isPopupMenu())
                    {
                        juce::PopupMenu m;
                        m.addItem (1, "Delete step " + juce::String (i + 1), c >= 0);
                        m.addItem (2, "Clear bank " + juce::String::charToString ((juce::juce_wchar) ('A' + bank)));
                        juce::Component::SafePointer<MemoryRail> safe (this);
                        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (stepArea (i).toNearestInt())), [safe, bank, i] (int r)
                        {
                            if (safe == nullptr) return;
                            if (r == 1) safe->processor.memDelete (bank, i);
                            if (r == 2) safe->processor.memClear (bank);
                            safe->update();
                        });
                    }
                    else if (c >= 0)
                    {
                        processor.clickChord (c);
                        auditioned = c;
                    }
                    return;
                }

            if (recArea().contains (p))
            {
                const bool on = ! processor.memRecord.load();
                if (on && processor.memLength[(size_t) bank].load() >= Hamamatsu108Processor::memSteps) processor.memClear (bank);
                processor.memRecord.store (on);
                update();
            }
            else if (lenArea().contains (p))
            {
                const int len = juce::roundToInt (plain (processor.apvts, "memStep"));
                setParam (processor.apvts, "memStep", (float) ((len + 1) % Hamamatsu108Processor::memStepNames().size()));
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (auditioned >= 0) { processor.releaseChord (auditioned); auditioned = -1; }
            dragging = false;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragging || ! dragArea().contains (e.mouseDownPosition) || e.getDistanceFromDragStart() < 6) return;
            const auto file = writeMidi();
            if (! file.existsAsFile()) return;
            dragging = true;
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
        }

    private:
        juce::File writeMidi() const
        {
            const int bank = processor.memBank.load();
            const int len = processor.memLength[(size_t) bank].load();
            if (len == 0) return {};
            const int ppq = 960;
            const double stepTicks = Hamamatsu108Processor::memStepQuarters (juce::roundToInt (plain (processor.apvts, "memStep"))) * ppq;
            const int transpose = juce::roundToInt (plain (processor.apvts, "transpose"));
            const int inv = juce::roundToInt (plain (processor.apvts, "voicing"));

            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / processor.hostBpm.load())), 0.0);
            std::vector<int> prev;
            for (int i = 0; i < len; ++i)
            {
                const int c = processor.memChord (bank, i);
                if (c < 0) continue;
                prev = chords::voicing (c, transpose, inv, prev);
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

        Hamamatsu108Processor& processor;
        juce::String lastSig;
        int auditioned = -1;
        bool dragging = false;
    };


    //==========================================================================
    // MODEL display on the rear of the instrument: click to choose one of the 6 models.
    class ModelDisplay : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit ModelDisplay (Hamamatsu108Processor& p) : processor (p)
        {
            setTooltip ("Model: six eras of the instrument, from the dark early 81 to the hi-fi Modern Strum. Click to choose.");
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        static juce::Rectangle<int> area() { return { geo::innerX + geo::innerW - 230, geo::rearY + 6, 230, 46 }; }

        void update()
        {
            const int m = juce::roundToInt (plain (processor.apvts, "model"));
            if (m != model) { model = m; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (14.0f, 0.0f);
            g.setFont (LookAndFeel::monoFont (11.0f));
            glowText (g, "MODEL", r, juce::Justification::centredLeft, 0.7f);
            g.setFont (LookAndFeel::monoFont (17.0f));
            glowText (g, Hamamatsu108Processor::modelNames()[juce::jmax (0, model)].toUpperCase(), r, juce::Justification::centredRight);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            juce::PopupMenu m;
            m.addSectionHeader ("Model");
            static const char* desc[] = { "dark, warm, a little noisy", "the classic chime", "brighter, wider", "clean and smooth", "lo-fi cartridge grit", "hi-fi and wide" };
            for (int i = 0; i < 6; ++i)
                m.addItem (1 + i, Hamamatsu108Processor::modelNames()[i] + juce::String::fromUTF8 ("  \xc2\xb7  ") + desc[i], true, i == model);
            juce::Component::SafePointer<ModelDisplay> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
            {
                if (safe != nullptr && r > 0) { setParam (safe->processor.apvts, "model", (float) (r - 1)); safe->update(); }
            });
        }

    private:
        Hamamatsu108Processor& processor;
        int model = -1;
    };

    //==========================================================================
    InstrumentView::InstrumentView (Hamamatsu108Processor& p) : processor (p)
    {
        backdrop = std::make_unique<Backdrop>();
        pad = std::make_unique<ChordPad> (p);
        plate = std::make_unique<StrumPlate> (p);
        module = std::make_unique<VoiceModule> (p);
        capture = std::make_unique<MidiCapture> (p);
        rail = std::make_unique<MemoryRail> (p);
        modelDisplay = std::make_unique<ModelDisplay> (p);
        chordSel = std::make_unique<GroupSelector> (p, 1);
        bassSel = std::make_unique<GroupSelector> (p, 2);

        addAndMakeVisible (*backdrop);
        addAndMakeVisible (*pad);
        addAndMakeVisible (*plate);
        addAndMakeVisible (*module);
        addAndMakeVisible (*capture);
        addAndMakeVisible (*rail);
        addAndMakeVisible (*modelDisplay);
        addAndMakeVisible (*chordSel);
        addAndMakeVisible (*bassSel);

        module->onMessage = [this] (const juce::String& m) { if (onStatus) onStatus (m, {}); };
        for (auto* k : module->knobs)
            k->onInteract = [this] (const Knob& kn) { if (onStatus) onStatus (kn.getDisplayName(), kn.getValueText()); };

        Knob::Style deck;
        deck.ring = th().deep;
        deck.themed = 2;
        deck.track = juce::Colours::black.withAlpha (0.18f);
        deck.pointer = th().deep;
        deck.label = juce::Colour (0xff22262b);
        deck.value = juce::Colour (0xff4a5058);
        deck.labelHeight = 10.0f;

        for (auto& gr : deckGroups())
            for (auto& k : gr.knobs)
            {
                auto st = deck;
                if (std::string (gr.name) == "MASTER") { st.ring = juce::Colour (0xff22262b); st.pointer = juce::Colour (0xff22262b); st.themed = 0; }
                if (k.param != nullptr)
                {
                    addKnob (std::make_unique<Knob> (p.apvts, k.param, k.label, k.size, st));
                    const juce::String pid (k.param);
                    if (pid.startsWith ("fx") && pid.endsWith ("Mix"))
                        fxKnobs[(size_t) (pid.substring (2, 3).getIntValue() - 1)] = deckKnobs.getLast();
                }
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
        capture->setBounds (MidiCapture::area());
        rail->setBounds (MemoryRail::area());
        modelDisplay->setBounds (ModelDisplay::area());

        const auto groups = deckGroupBounds();
        int k = 0;
        for (int gi = 0; gi < groups.size(); ++gi)
        {
            const auto& gr = deckGroups()[(size_t) gi];
            if (gr.selector == 1) chordSel->setBounds (groups[gi].getX() + 4, groups[gi].getY() - 3, groups[gi].getWidth() - 8, 17);
            if (gr.selector == 2) bassSel->setBounds (groups[gi].getX() + 4, groups[gi].getY() - 3, groups[gi].getWidth() - 8, 17);
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

    void InstrumentView::applySkin (int skinIndex)
    {
        const auto& sk = skins::get (skinIndex);
        currentSkin = skinIndex;
        th().accent = sk.accent.main;
        th().soft = sk.accent.soft;
        th().deep = sk.accent.deep;
        Knob::themeAccent = sk.accent.main;
        Knob::themeSoft = sk.accent.soft;
        Knob::themeDeep = sk.accent.deep;
        backdrop->skin = skinIndex;
        backdrop->repaint();
        for (auto* k : deckKnobs)
        {
            if (k->getParamID() == "master") continue;
            Knob::Style st;
            st.ring = sk.accent.deep; st.themed = 2;
            st.track = juce::Colours::black.withAlpha (0.18f);
            st.pointer = sk.accent.deep;
            st.label = juce::Colour (0xff22262b);
            st.value = juce::Colour (0xff4a5058);
            st.labelHeight = 10.0f;
            k->setStyle (st);
        }
        if (auto* top = getTopLevelComponent()) top->repaint();
        repaint();
    }

    void InstrumentView::timerCallback()
    {
        if (processor.skin.load() != currentSkin) applySkin (processor.skin.load());
        const int chord = processor.activeChord.load();
        pad->update (chord, juce::roundToInt (plain (processor.apvts, "keyRoot")), plain (processor.apvts, "keyMode") > 0.5f);
        plate->update (processor.stringFlashes.exchange (0), chord >= 0 ? chord : processor.detectedChord.load());   // chord detection: piano roll notes too
        module->update (processor.meterLeft.load(), processor.meterRight.load(), chord);
        capture->update();
        rail->update();
        modelDisplay->update();
        // FX knobs on the deck are named after the effect in slots 1-3
        for (int i = 0; i < 3; ++i)
            if (fxKnobs[(size_t) i] != nullptr)
            {
                const int t = juce::roundToInt (plain (processor.apvts, ("fx" + juce::String (i + 1) + "Type").toRawUTF8()));
                fxKnobs[(size_t) i]->setLabel (t == 0 ? "Empty" : fx::names()[t]);
            }
        chordSel->update();
        bassSel->update();
    }
}
