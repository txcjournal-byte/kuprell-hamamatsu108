#include "Instrument.h"
#include "KuprellMidiClip.h"
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
                { "HARP",    { { "harpTone", "Tone", 36, nullptr }, { "harpRange", "Range", 36, nullptr } }, false },
                { "ENVELOPE", { { "harpAttack", "A", 0, nullptr }, { "harpDecay", "D", 0, nullptr }, { "harpSustain", "S", 0, nullptr }, { "harpRelease", "R", 0, nullptr } }, false },
                { "FILTER",  { { "filterCutoff", "Cutoff", 36, nullptr }, { "filterReso", "Reso", 36, nullptr }, { "drive", "Saturation", 36, nullptr } }, false },
                { "FX",      { { "fx1Mix", "FX 1", 36, nullptr }, { "fx2Mix", "FX 2", 36, nullptr }, { "fx3Mix", "FX 3", 36, nullptr } }, false },
                { "MACROS",  { { "macroAge", "AGE", 44, nullptr }, { "macroDream", "DREAM", 44, nullptr }, { "macroWobble", "WOBBLE", 44, nullptr } }, true },
                { "MASTER",  { { "master", "Volume", 48, nullptr } }, false },
            };
            return g;
        }

        int knobColumnWidth (int size) { return size == 0 ? 30 : juce::jmax (size + 14, 48) + 4; }   // size 0 = vertical slider

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
            logoInk = juce::Drawable::createFromImageData (KuprellBrand::kuprelllogoink_svg, KuprellBrand::kuprelllogoink_svgSize);   // on light panels
        }

        void paint (juce::Graphics& g) override
        {
            using namespace geo;
            const auto bounds = getLocalBounds().toFloat();

            // studio desk
            juce::ColourGradient desk (juce::Colour (0xff3a4048), bounds.getCentreX(), bounds.getHeight() * 0.22f,
                                       juce::Colour (0xff0b0c0e), bounds.getCentreX(), bounds.getHeight() * 1.25f, true);
            desk.addColour (0.48, juce::Colour (0xff1c2025));
            if (juce::String (skins::get (skin).name) == "Dust" && false)   // the instrument stands in orange haze
            {
                desk = juce::ColourGradient (juce::Colour (0xffb8622a), bounds.getCentreX(), bounds.getHeight() * 0.1f,
                                             juce::Colour (0xff1a0e07), bounds.getCentreX(), bounds.getHeight() * 1.2f, true);
                desk.addColour (0.5, juce::Colour (0xff5a3018));
            }
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

            if (juce::String (skins::get (skin).name) == "Concrete")
            {
                paintConcrete (g, bounds);
                paintRear (g);
                drawWell (g, page::strumArea().toFloat(), 0.0f);
                drawWell (g, page::deckArea2().toFloat().expanded (0.0f, 2.0f), 0.0f);
                return;
            }
            paintCheek (g, { (float) bodyX, (float) bodyY, (float) cheekW, (float) panelH }, true);
            paintCheek (g, { (float) (bodyX + bodyW - cheekW), (float) bodyY, (float) cheekW, (float) panelH }, false);
            paintPanel (g, { (float) (bodyX + cheekW), (float) bodyY, (float) (bodyW - 2 * cheekW), (float) panelH });
            paintFront (g, { (float) bodyX, (float) (bodyY + panelH), (float) bodyW, (float) frontH });

            paintRear (g);
            // HARPOLOGY main page: quiet wells, small engraved titles
            drawWell (g, page::strumArea().toFloat(), 14.0f);
            drawWell (g, page::deckArea2().toFloat().expanded (0.0f, 2.0f), 12.0f);

        }

        // CONCRETE: one cast slab in a dark hall, cut by a single hard shaft of light
        void paintConcrete (juce::Graphics& g, juce::Rectangle<float> bounds)
        {
            using namespace geo;
            g.fillAll (juce::Colour (0xff141413));
            // the shaft in the hall behind the slab (a hard-edged wedge, no blur)
            {
                juce::Path wedge;
                wedge.startNewSubPath (bounds.getWidth() * 0.58f, 0.0f);
                wedge.lineTo (bounds.getWidth() * 0.80f, 0.0f);
                wedge.lineTo (bounds.getWidth() * 0.46f, bounds.getBottom());
                wedge.lineTo (bounds.getWidth() * 0.12f, bounds.getBottom());
                wedge.closeSubPath();
                g.setColour (juce::Colour (0xff2a2a28));
                g.fillPath (wedge);
            }
            const juce::Rectangle<float> slab ((float) bodyX, (float) bodyY, (float) bodyW, (float) (panelH + frontH));
            // drop shadow: one hard offset block, like a heavy object on the floor
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRect (slab.translated (10.0f, 12.0f));
            g.setColour (juce::Colour (0xff595854));
            g.fillRect (slab);
            // cast concrete: fine grain and the odd pore, fixed seed so it never changes
            {
                juce::Random rnd (2049);
                for (int i = 0; i < 26000; ++i)
                {
                    const float x = slab.getX() + rnd.nextFloat() * slab.getWidth(), y = slab.getY() + rnd.nextFloat() * slab.getHeight();
                    const bool dark = rnd.nextBool();
                    g.setColour ((dark ? juce::Colours::black : juce::Colours::white).withAlpha (0.02f + rnd.nextFloat() * 0.05f));
                    g.fillRect (x, y, 1.0f, 1.0f);
                }
                for (int i = 0; i < 260; ++i)
                {
                    const float x = slab.getX() + rnd.nextFloat() * slab.getWidth(), y = slab.getY() + rnd.nextFloat() * slab.getHeight();
                    const float d = 1.0f + rnd.nextFloat() * 2.2f;
                    g.setColour (juce::Colours::black.withAlpha (0.18f + rnd.nextFloat() * 0.2f));
                    g.fillEllipse (x, y, d, d);
                }
                // formwork joints: the slab was cast in panels
                g.setColour (juce::Colours::black.withAlpha (0.22f));
                for (float x = slab.getX() + 280.0f; x < slab.getRight() - 40.0f; x += 280.0f) g.fillRect (x, slab.getY(), 1.0f, slab.getHeight());
                g.fillRect (slab.getX(), slab.getY() + 300.0f, slab.getWidth(), 1.0f);
            }
            // the light falls across the slab: the same wedge, lighter where it hits
            {
                juce::Graphics::ScopedSaveState save (g);
                g.reduceClipRegion (slab.toNearestInt());
                juce::Path lit;
                lit.startNewSubPath (bounds.getWidth() * 0.58f, 0.0f);
                lit.lineTo (bounds.getWidth() * 0.80f, 0.0f);
                lit.lineTo (bounds.getWidth() * 0.46f, bounds.getBottom());
                lit.lineTo (bounds.getWidth() * 0.12f, bounds.getBottom());
                lit.closeSubPath();
                g.setColour (juce::Colour (0xffe8e6df).withAlpha (0.13f));
                g.fillPath (lit);
            }
            // hard edges: light on top, shadow at the bottom
            g.setColour (juce::Colour (0xff8a8883));
            g.fillRect (slab.withHeight (2.0f));
            g.setColour (juce::Colour (0xff2c2b29));
            g.fillRect (slab.withTop (slab.getBottom() - 3.0f));
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
            g.setColour (sk.frontTop.getPerceivedBrightness() < 0.3f ? juce::Colour (0xff3a3d42) : juce::Colour (0xffc9ced4));
            g.fillRect (r.withHeight (2.0f));
        }

        void paintRear (juce::Graphics& g)
        {
            using namespace geo;
            // no speaker grille (it is a plugin): the logo sits at the left edge
            const auto& sk = skins::get (skin);
            // logo (approved SVG, drawn as vector, never altered) and engraved model line
            const float tx = (float) innerX + 6.0f;
            // product wordmark (the company mark joins when the brand is settled)
            g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)).withExtraKerningFactor (0.06f));
            g.setColour (th().concrete ? juce::Colour (0xffeceae4) : sk.panelMid.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff1c1d22) : juce::Colour (0xfff1e2cc));
            g.drawText ("Harpology", juce::Rectangle<float> (tx - 1.0f, (float) rearY + 4.0f, 200.0f, 30.0f), juce::Justification::centredLeft, false);
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
            if (th().concrete) { g.setFont (LookAndFeel::uiFont (11.0f, false)); g.setColour (juce::Colour (0xffcfccc4)); }
            g.drawText (th().concrete ? "Strum harp" : "STRUM HARP", juce::Rectangle<float> (tx, (float) rearY + 36.0f, 180.0f, 14.0f), juce::Justification::centredLeft, false);



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
            engravedLabel (g, padArea().toFloat().withY (y).withHeight (12.0f), "CHORD BUTTONS", juce::String::fromUTF8 ("RIGHT-CLICK = MORE CHORDS \xc2\xb7 CTRL+CLICK = COMBINE   \xe2\x97\x8f IN KEY"));
            engravedLabel (g, plateArea().toFloat().withY (y).withHeight (12.0f), "MOTION");
            engravedLabel (g, moduleArea().toFloat().withY (y).withHeight (12.0f), juce::String::fromUTF8 ("VOICE \xc2\xb7 PLAY"));
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
            // brushed metal (dark decks stay smooth and matte)
            const bool matte = sk.deckMid.getPerceivedBrightness() < 0.45f;
            for (float x = r.getX(); x < r.getRight() && ! matte; x += 3.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.10f));
                g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
                g.setColour (juce::Colours::black.withAlpha (0.04f));
                g.drawVerticalLine ((int) x + 1, r.getY(), r.getBottom());
            }
            juce::ColourGradient sheen (juce::Colours::white.withAlpha (matte ? 0.06f : 0.35f), r.getX(), r.getY(),
                                        juce::Colours::transparentWhite, r.getX() + r.getWidth() * 0.5f, r.getBottom() + 200.0f, false);
            sheen.addColour (0.3, juce::Colours::transparentWhite);
            sheen.addColour (0.58, juce::Colours::white.withAlpha (0.18f));
            g.setGradientFill (sheen);
            g.fillRect (r);
            g.setColour (juce::Colours::white.withAlpha (matte ? 0.12f : 0.9f));   // edge light: subtle on dark decks
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
                const bool darkDeck = skins::get (skin).deckMid.getPerceivedBrightness() < 0.45f;
                g.setColour (darkDeck ? juce::Colours::black.withAlpha (0.6f) : juce::Colours::white.withAlpha (0.6f));
                g.drawText (gr.name, b.withHeight (12.0f).translated (0.0f, 1.0f), juce::Justification::centred, false);
                g.setColour (gr.macro && ! th().lcd ? th().deep : (darkDeck ? juce::Colour (0xffb9bec5) : juce::Colour (0xff3a3f46)));
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

        std::unique_ptr<juce::Drawable> logo, logoInk;
        int skin = 0;
    };

    //==========================================================================
    class ChordPad : public juce::Component
    {
    public:
        explicit ChordPad (Hamamatsu108Processor& p) : processor (p) {}

        static constexpr float keyW = 42.0f, keyH = 27.0f, gapX = 4.9f, gapY = 4.0f;
        static constexpr float keysX = 14.0f + 38.0f, keysY = 9.0f + 16.0f + 4.0f;
        static constexpr int shownTypes = 3;   // major, minor, 7 - the rest by combination or right-click

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
            for (int t = 0; t < shownTypes; ++t)
                g.drawText (chords::types()[(size_t) t].rowLabel, juce::Rectangle<float> (12.0f, keyBounds (0, t).getY(), 35.0f, keyH),
                            juce::Justification::centredRight, false);

            for (int t = 0; t < shownTypes; ++t)
                for (int r = 0; r < chords::numRoots; ++r)
                    paintKey (g, r, t);
        }

        void paintKey (juce::Graphics& g, int root, int type)
        {
            const int idx = chords::index (root, type);
            bool on = idx == activeChord || idx == pressed || std::find (stack.begin(), stack.end(), idx) != stack.end();
            if (! on && activeChord >= 0 && ! chords::isCustom (activeChord) && chords::typeOf (activeChord) >= shownTypes && chords::rootOf (activeChord) == root)
            {
                // a combination chord lights the buttons it is made of
                static const int parts[12] = { 0, 0, 0, 0b101, 0b110, 0b011, 0b111, 0b001, 0b001, 0b001, 0b010, 0b001 };
                on = (parts[chords::typeOf (activeChord)] >> type) & 1;
            }
            auto b = keyBounds (root, type);
            const bool inKeyNow = chords::isInKey (idx, key, keyMinor);

            if (th().lcd)
            {
                // matte key with a backlit bar: white = in key, aqua = playing
                g.setColour (juce::Colours::black.withAlpha (0.7f));
                g.fillRoundedRectangle (b.translated (0.0f, 3.0f), 7.0f);
                g.setColour (juce::Colour (0xff0c0d0f));
                g.fillRoundedRectangle (b, 7.0f);
                auto cap = b.withTrimmedBottom (on ? 2.0f : 3.0f).withTrimmedTop (on ? 1.0f : 0.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (on ? 0xff2c2f33 : 0xff292b2f), 0.0f, cap.getY(), juce::Colour (0xff1c1d20), 0.0f, cap.getBottom(), false));
                g.fillRoundedRectangle (cap, 7.0f);
                g.setColour (juce::Colours::white.withAlpha (0.1f));
                g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + 6.0f, cap.getRight() - 6.0f);
                auto bar = juce::Rectangle<float> (cap.getWidth() - 16.0f, 3.0f).withCentre ({ cap.getCentreX(), cap.getBottom() - 5.0f });
                if (on)
                {
                    g.setColour (th().accent.withAlpha (0.28f));
                    g.fillRoundedRectangle (bar.expanded (5.0f, 4.0f), 5.0f);
                    g.setColour (th().accent.withAlpha (0.5f));
                    g.fillRoundedRectangle (bar.expanded (2.0f, 1.5f), 3.0f);
                    g.setColour (th().soft.brighter (0.3f));
                }
                else
                    g.setColour (inKeyNow ? juce::Colour (0xffd9dde2).withAlpha (0.75f) : juce::Colour (0xff3a3d42));
                g.fillRoundedRectangle (bar, 1.5f);
                g.setColour (on ? juce::Colours::white : juce::Colour (inKeyNow ? 0xffe6e8eb : 0xff9a9fa6));
                g.setFont (LookAndFeel::uiFont (10.0f, true));
                g.drawText (chords::shortName (idx), cap.withTrimmedBottom (8.0f), juce::Justification::centred, false);
                return;
            }

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
                g.setGradientFill (juce::ColourGradient (th().deep.darker (0.9f).withSaturation (0.55f), 0.0f, b.getY(), th().deep.darker (1.6f).withSaturation (0.5f), 0.0f, b.getBottom(), false));   // the lit key takes the skin's light
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
            for (int t = 0; t < shownTypes; ++t)
                for (int r = 0; r < chords::numRoots; ++r)
                    if (keyBounds (r, t).contains (e.position))
                    {
                        pressed = chords::index (r, t);
                        if (e.mods.isPopupMenu())
                        {
                            // every chord type of this root
                            juce::PopupMenu m;
                            m.addSectionHeader (juce::String (chords::rootName (r)) + " chords (Ctrl+click two buttons: MAJ+7 = maj7, MIN+7 = m7, MAJ+MIN = dim, all three = aug)");
                            for (int k = 0; k < chords::numTypes; ++k)
                                m.addItem (1 + chords::index (r, k), chords::shortName (chords::index (r, k)) + "   " + chords::types()[(size_t) k].longName, true, activeChord == chords::index (r, k));
                            juce::Component::SafePointer<ChordPad> safe (this);
                            m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (keyBounds (r, t).toNearestInt())),
                                             [safe] (int res) { if (safe != nullptr && res > 0) safe->processor.latchChord (res - 1); });
                            pressed = -1;
                            return;
                        }
                        if (e.mods.isCtrlDown() || e.mods.isShiftDown() || e.mods.isCommandDown())
                            processor.addChord (pressed);     // stack it on the chord that is on
                        else
                        {
                            processor.latchChord (pressed);   // stays on until clicked again or another chord is chosen
                            if (! processor.gridPlay.load()) processor.audition();   // hear it once: one bar of the pattern (MOTION) or one stroke
                            if (const int ed = processor.progEdit.load(); ed >= 0)
                            {
                                processor.progSlots[(size_t) ed].store (pressed);   // PROGRESSION: fill the slot, go to the next
                                processor.progEdit.store (ed + 1 < prog::numSlots ? ed + 1 : -1);
                            }
                        }
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
            setTooltip (juce::String::fromUTF8 ("Strum plate \xc2\xb7 try it with the mouse; play it with your keys (right hand from C5), the wheels or - / = on the PC keyboard"));
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
            if (th().lcd)
            {
                chrome = juce::ColourGradient (juce::Colour (0xff3a3c41), 0.0f, r.getY(), juce::Colour (0xff1a1b1e), 0.0f, r.getBottom(), false);
                chrome.addColour (0.5, juce::Colour (0xff26282c));
            }
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
                // screen skins: plain white light, brighter only where a string sounds
                const auto sc = th().accent, sm = th().soft;
                // screen skins: the strings stay dark and light up only where a string sounds
                const float a2 = th().lcd ? 0.1f + 0.9f * v : alpha;
                juce::ColourGradient s (sc.withAlpha (0.0f), gl.getX(), y, sc.withAlpha (0.0f), gl.getRight(), y, false);
                s.addColour (0.18, sc.withAlpha (a2));
                s.addColour (0.5, sm.withAlpha (a2));
                s.addColour (0.82, sc.withAlpha (a2));
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
        juce::Rectangle<float> dualArea() const  { return { 12.0f, 130.0f, 58.0f, 30.0f }; }
        juce::Rectangle<float> holdArea() const  { return { 74.0f, 130.0f, 58.0f, 30.0f }; }
        juce::Rectangle<float> flourArea() const { return { 136.0f, 130.0f, 68.0f, 30.0f }; }   // FLOURISH: one key = a whole melodic run
        juce::Rectangle<float> modeArea (int i) const { return { 12.0f + i * 65.0f, 166.0f, 61.0f, 34.0f }; }
        static int modeOf (int button) { return button == 2 ? 3 : button; }   // LIVE, ARP, OFF
        juce::Rectangle<float> keyArea() const   { return { 100.0f, 220.0f, 104.0f, 24.0f }; }
        juce::Rectangle<float> trDown() const    { return { 106.0f, 249.0f, 26.0f, 24.0f }; }
        juce::Rectangle<float> trValue() const   { return { 134.0f, 249.0f, 40.0f, 24.0f }; }
        juce::Rectangle<float> trUp() const      { return { 176.0f, 249.0f, 26.0f, 24.0f }; }
        juce::Rectangle<float> charArea() const  { return { 100.0f, 278.0f, 104.0f, 24.0f }; }   // CHARACTER: the era / colour of the whole instrument
        juce::Rectangle<float> meterArea() const { return { 40.0f, 400.0f, 164.0f, 13.0f }; }   // (meter retired: CHARACTER sits there)

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
            auto lit = std::make_unique<LitScreen>();   // the main screen is backlit
            drawDisplay (g, vfd, 8.0f);
            auto lines = vfd.reduced (10.0f, 5.0f);
            g.setFont (LookAndFeel::monoFont (13.0f));
            auto l1 = lines.removeFromTop (16.0f);
            glowText (g, "VOICE", l1, juce::Justification::centredLeft);
            glowText (g, dual ? shortVoice (voice) + "+" + shortVoice (voice2) : harp::voiceNames()[voice].toUpperCase(), l1, juce::Justification::centredRight);
            auto l2 = lines.removeFromTop (16.0f);
            const auto rateText = Hamamatsu108Processor::strumRateNames()[rate];
            static const char* keysShort[] = { "NOTES", "STRUM", "PATTERN", "PLATE ONLY", "FLOURISH UP", "FLOURISH DN", "SPLIT" };
            juce::String left = "KEYS", right = keysShort[juce::jlimit (0, 6, keysPlay)];
            if (mode == 1) { left = "ARP"; right = Hamamatsu108Processor::arpPatternNames()[pattern].toUpperCase().removeCharacters (" ") + " " + rateText; }
            glowText (g, left, l2, juce::Justification::centredLeft, 0.75f);
            glowText (g, right, l2, juce::Justification::centredRight, 0.75f);
            lit.reset();

            drawHardButton (g, dualArea(), "DUAL", dual, true, false);
            drawHardButton (g, holdArea(), "HOLD", hold, true, false);
            drawHardButton (g, flourArea(), "FLOURISH", keysPlay == 4 || keysPlay == 5, true, false);

            g.setFont (spaced (9.0f, true, 0.2f));
            g.setColour (juce::Colour (0xff9aa0a7));
            g.drawText ("KEY", juce::Rectangle<float> (12.0f, keyArea().getY(), 80.0f, 24.0f), juce::Justification::centredLeft, false);
            g.drawText ("TRANSPOSE", juce::Rectangle<float> (12.0f, trValue().getY(), 90.0f, 24.0f), juce::Justification::centredLeft, false);
            g.drawText ("CHARACTER", juce::Rectangle<float> (12.0f, charArea().getY(), 90.0f, 24.0f), juce::Justification::centredLeft, false);
            drawDisplay (g, charArea(), 5.0f);
            g.setFont (LookAndFeel::monoFont (12.0f));
            glowText (g, Hamamatsu108Processor::modelNames()[juce::roundToInt (plain (processor.apvts, "model"))].toUpperCase() + juce::String::fromUTF8 (" \xe2\x96\xbe"), charArea(), juce::Justification::centred);

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
            if (juce::Rectangle<float> (12.0f, 12.0f, 192.0f, 21.0f).contains (p)) { showPresetMenu(); return; }
            if (juce::Rectangle<float> (12.0f, 33.0f, 192.0f, 21.0f).contains (p)) { showKeysMenu(); return; }
            if (flourArea().contains (p))
            {
                const bool on = keysPlay == 4 || keysPlay == 5;
                if (e.mods.isPopupMenu()) { showFlourishMenu(); return; }
                if (on) setParam (s, "keysPlay", (float) (keysBeforeFlourish >= 0 && keysBeforeFlourish != 4 && keysBeforeFlourish != 5 ? keysBeforeFlourish : 6));
                else { keysBeforeFlourish = keysPlay; setParam (s, "keysPlay", 4.0f); }
                message (on ? juce::String ("Flourish off") : juce::String ("Flourish: one key = a whole run of a chord (key = root, octave = chord type). Right-click: up / down"));
                return;
            }
            if (holdArea().contains (p))       setParam (s, "hold", hold ? 0.0f : 1.0f);
            else if (dualArea().contains (p))  showDualMenu();
            else if (trDown().contains (p))    setParam (s, "transpose", (float) juce::jmax (-12, transpose - 1));
            else if (trUp().contains (p))      setParam (s, "transpose", (float) juce::jmin (12, transpose + 1));
            else if (keyArea().contains (p))   showKeyMenu();
            else if (charArea().contains (p))
            {
                juce::PopupMenu m;
                m.addSectionHeader ("CHARACTER: the colour of the whole instrument");
                static const char* d[] = { "Warm: soft, round, a little dark", "Classic: the chime of the 80s", "Bright: glassy and clear",
                                           "Clean: modern and precise", "Lo-fi: worn tape, fewer bits", "Hi-fi: wide and polished" };
                const int cur = juce::roundToInt (plain (processor.apvts, "model"));
                for (int i = 0; i < 6; ++i) m.addItem (100 + i, d[i], true, cur == i);
                popup (m, charArea(), [this] (int r) { setParam (processor.apvts, "model", (float) (r - 100)); message ("Character: " + Hamamatsu108Processor::modelNames()[r - 100]); });
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

        // what your keyboard / piano-roll notes do (click the KEYS line of the display)
        void showKeysMenu()
        {
            juce::PopupMenu m;
            m.addSectionHeader ("YOUR KEYS / PIANO ROLL: what the notes do");
            static const char* keyText[] = { "Notes: the harp plays the exact pitch (melodies in the piano roll)",
                                             "Strum: each key press strums the keys you hold",
                                             "Pattern: the strum pattern plays the keys you hold",
                                             "Plate only: hold keys, strum them on the plate / wheels",
                                             "Flourish up: one key = a whole harp run of a chord (octave = chord type)",
                                             "Flourish down: the same run falling",
                                             "Split: left hand (below C5 = middle C) = chord, right hand = strings, black keys = strum" };
            for (int i = 0; i < 7; ++i) m.addItem (100 + i, keyText[i], true, keysPlay == i);
            // STRUM KEYS: which octave plays the hand gestures, and what each key does
            {
                juce::PopupMenu z;
                const int zone = juce::roundToInt (plain (processor.apvts, "strumZone"));
                for (int i = 0; i < Hamamatsu108Processor::strumZoneNames().size(); ++i)
                    z.addItem (300 + i, Hamamatsu108Processor::strumZoneNames()[i], true, zone == i);
                z.addSeparator();
                z.addSectionHeader ("In that octave:");
                static const char* keyNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                for (int k = 0; k < 12; ++k)
                    z.addItem (400 + k, juce::String (keyNames[k]) + "   " + Hamamatsu108Processor::strumGestureNames()[k], false, false);
                m.addSeparator();
                m.addSubMenu ("STRUM KEYS (one octave = 12 hand gestures)", z);
            }
            popup (m, { 12.0f, 33.0f, 192.0f, 21.0f }, [this] (int r)
            {
                if (r >= 300 && r < 400)
                {
                    setParam (processor.apvts, "strumZone", (float) (r - 300));
                    message ("Strum keys: " + Hamamatsu108Processor::strumZoneNames()[r - 300]);
                    return;
                }
                setParam (processor.apvts, "keysPlay", (float) (r - 100));
                static const char* names[] = { "Notes", "Strum", "Pattern", "Plate only", "Flourish up", "Flourish down", "Split" };
                message (juce::String ("Your keys: ") + names[r - 100]);
            });
        }

        void showFlourishMenu()
        {
            juce::PopupMenu m;
            m.addSectionHeader ("FLOURISH: one key = a whole run (key = root, octave = chord type)");
            m.addItem (1, "Flourish up", true, keysPlay == 4);
            m.addItem (2, "Flourish down", true, keysPlay == 5);
            m.addItem (3, "Off", true, keysPlay != 4 && keysPlay != 5);
            popup (m, flourArea(), [this] (int r)
            {
                if (r == 3) setParam (processor.apvts, "keysPlay", (float) (keysBeforeFlourish >= 0 && keysBeforeFlourish < 4 ? keysBeforeFlourish : 6));
                else { if (keysPlay != 4 && keysPlay != 5) keysBeforeFlourish = keysPlay; setParam (processor.apvts, "keysPlay", r == 1 ? 4.0f : 5.0f); }
            });
        }
        int keysBeforeFlourish = 6;

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
                                             "Plate only: hold the keys, strum them on the plate / pitch wheel",
                                             "Flourish up: one key = a harp run (octave = chord type, harder = faster)",
                                             "Flourish down: the same run falling",
                                             "Split: left hand = chord, right hand (from C5) = strings, black keys = whole strum" };
            for (int i = 0; i < 7; ++i) m.addItem (100 + i, keyText[i], true, keysPlay == i);
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

        static juce::Rectangle<int> area() { return { geo::moduleArea().getX() + 12, geo::midY + 166, 192, 46 }; }   // in the voice module, under DUAL / HOLD

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
                // keep a rolling take of the last 16 bars: endless loops never pile up thousands of notes
                if (! notes.empty())
                {
                    const double keep = 16.0 * 4.0 * 60.0 / juce::jmax (40.0, processor.hostBpm.load());
                    const double from = notes.back().seconds - keep;
                    size_t cut = 0;
                    while (cut < notes.size() && notes[cut].seconds < from) ++cut;
                    // never start inside a chord: cut only before a note-on
                    while (cut > 0 && cut < notes.size() && ! notes[cut].on) ++cut;
                    if (cut > 0) notes.erase (notes.begin(), notes.begin() + (long) cut);
                }
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
            static const char* modeNames[] = { "EXACT", "CLEAN", "CLEAN" };
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
                for (auto& n : notes) if (n.on && n.layer < 3) ++ons;
                int blocks = 0;
                for (auto& n : notes) if (n.on && n.layer == 3) ++blocks;
                const int mode = processor.captureMode.load();
                juce::ignoreUnused (blocks, mode);
                const juce::String what = juce::String (ons) + " notes";
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
            const int mode = processor.captureMode.load();
            juce::PopupMenu m;
            m.addSectionHeader ("Drag the take as...");
            m.addItem (2, "Clean: every stroke = one tidy chord on the grid (easy to edit)", true, mode != 0);
            m.addItem (1, "Exact: the timing as played (humanized strums)", true, mode == 0);
            m.addSeparator();
            m.addItem (20, "Reset (clear the take)", ! notes.empty());
            juce::Component::SafePointer<MidiCapture> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
            {
                if (safe == nullptr || r <= 0) return;
                if (r == 1) safe->processor.captureMode.store (0);
                if (r == 2) safe->processor.captureMode.store (1);
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
            const double t0 = notes.front().seconds;
            std::vector<kuprell::midiclip::Note> clip;
            for (size_t i = 0; i < notes.size(); ++i)
            {
                const auto& n = notes[i];
                if (n.layer != 0 || ! n.on) continue;
                const double beat = (n.seconds - t0) * bpm / 60.0;
                double next = beat + 2.0;   // rings until the same string sounds again (at most 2 beats)
                for (size_t j = i + 1; j < notes.size(); ++j)
                    if (notes[j].layer == 0 && notes[j].on && notes[j].note == n.note) { next = (notes[j].seconds - t0) * bpm / 60.0; break; }
                clip.push_back ({ beat, n.note, n.velocity, juce::jmin (2.0, next - beat) });
            }
            return kuprell::midiclip::write (clip, "Hamamatsu 144 take " + juce::Time::getCurrentTime().formatted ("%H%M%S"), processor.captureMode.load() != 0);
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
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Kuprell Hamamatsu 144");
            dir.createDirectory();
            auto file = dir.getChildFile ("Hamamatsu 144 memory " + juce::String::charToString ((juce::juce_wchar) ('A' + bank)) + ".mid");
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
            setTooltip ("Character: six sound characters, from Warm to Hi-fi. Click to choose.");
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
            glowText (g, "CHARACTER", r, juce::Justification::centredLeft, 0.7f);
            g.setFont (LookAndFeel::monoFont (17.0f));
            glowText (g, Hamamatsu108Processor::modelNames()[juce::jmax (0, model)].toUpperCase(), r, juce::Justification::centredRight);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            juce::PopupMenu m;
            m.addSectionHeader ("Character");
            static const char* desc[] = { "dark, warm, a little noisy", "the classic chime", "brighter, wider", "clean and smooth", "lo-fi grit", "hi-fi and wide" };
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
    // Vertical slider for the deck (ADSR)
    class DeckSlider : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        DeckSlider (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& labelText)
            : param (s.getParameter (paramID)), label (labelText)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
            attachment->sendInitialUpdate();
            setTooltip (param->getName (64));
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        }
        std::function<void (const juce::String&, const juce::String&)> onInteract;
        juce::Colour capColour { juce::Colours::transparentBlack };

        juce::Rectangle<float> track() const { return getLocalBounds().toFloat().withTrimmedTop (4.0f).withTrimmedBottom (26.0f).withSizeKeepingCentre (6.0f, (float) getHeight() - 30.0f); }

        void paint (juce::Graphics& g) override
        {
            auto t = track();
            const float v = param->getValue();
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (t, 3.0f);
            g.setColour (th().lcd ? juce::Colour (0xff8d959e) : th().deep);
            g.fillRoundedRectangle (t.withTop (t.getBottom() - t.getHeight() * v), 3.0f);
            auto cap = juce::Rectangle<float> (20.0f, 9.0f).withCentre ({ t.getCentreX(), t.getBottom() - t.getHeight() * v });
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 2.5f);
            if (capColour.isTransparent())
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f4f6), 0.0f, cap.getY(), juce::Colour (0xff9aa1a9), 0.0f, cap.getBottom(), false));
            else   // colour-coded cap, like the lever sliders of an 80s polysynth
                g.setGradientFill (juce::ColourGradient (capColour.brighter (0.25f), 0.0f, cap.getY(), capColour.darker (0.35f), 0.0f, cap.getBottom(), false));
            g.fillRoundedRectangle (cap, 2.5f);
            g.setColour (th().lcd ? juce::Colour (0xff3a4048) : th().deep);
            g.fillRect (cap.withSizeKeepingCentre (14.0f, 1.5f));
            g.setColour (skins::deckInk (false));
            g.setFont (LookAndFeel::uiFont (10.0f, true));
            g.drawText (label, getLocalBounds().removeFromBottom (24).removeFromTop (12), juce::Justification::centred, false);
            g.setColour (skins::deckInk (true));
            g.setFont (LookAndFeel::monoFont (8.5f));
            g.drawText (param->getCurrentValueAsText(), getLocalBounds().removeFromBottom (12).expanded (8, 0), juce::Justification::centred, false);
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() || e.getNumberOfClicks() > 1) { attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue())); return; }
            attachment->beginGesture();
            startValue = param->getValue();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float sens = e.mods.isShiftDown() || e.mods.isCtrlDown() ? 0.25f : 1.0f;
            const float nv = juce::jlimit (0.0f, 1.0f, startValue - sens * (float) e.getDistanceFromDragStartY() / track().getHeight());
            attachment->setValueAsPartOfGesture (param->convertFrom0to1 (nv));
            if (onInteract) onInteract (param->getName (32), param->getCurrentValueAsText());
        }
        void mouseUp (const juce::MouseEvent&) override { attachment->endGesture(); }
    private:
        juce::RangedAudioParameter* param;
        juce::String label;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        float startValue = 0.0f;
    };

    //==========================================================================
    // Display with a menu on the rear of the instrument (PATTERN, YOUR KEYS)
    class RearChoice : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        RearChoice (Hamamatsu108Processor& p, const char* paramID, const char* labelText, const char* tip) : processor (p), id (paramID), label (labelText)
        {
            setTooltip (tip);
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        void update()
        {
            const int v = juce::roundToInt (plain (processor.apvts, id.toRawUTF8()));
            if (v != value) { value = v; repaint(); }
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (r.expanded (1.0f), 10.0f);
            drawDisplay (g, r, 9.0f);
            if (hover) { g.setColour (th().accent.withAlpha (0.6f)); g.drawRoundedRectangle (r, 9.0f, 1.5f); }
            auto t = r.reduced (12.0f, 5.0f);
            g.setFont (LookAndFeel::monoFont (11.0f));
            glowText (g, label, t.removeFromTop (17.0f), juce::Justification::centredLeft, 0.7f);
            glowText (g, juce::String::fromUTF8 ("\xe2\x96\xbe"), r.reduced (12.0f, 5.0f).removeFromTop (17.0f), juce::Justification::centredRight, 0.5f);
            g.setFont (LookAndFeel::monoFont (14.0f));
            auto* prm = processor.apvts.getParameter (id);
            glowText (g, prm != nullptr ? prm->getCurrentValueAsText().toUpperCase() : juce::String(), t, juce::Justification::centredLeft);
        }
        void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
        void mouseDown (const juce::MouseEvent&) override
        {
            auto* prm = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id));
            if (prm == nullptr) return;
            juce::PopupMenu m;
            m.addSectionHeader (getTooltip());
            for (int i = 0; i < prm->choices.size(); ++i) m.addItem (1 + i, prm->choices[i], true, i == value);
            juce::Component::SafePointer<RearChoice> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
            {
                if (safe != nullptr && r > 0) { setParam (safe->processor.apvts, safe->id.toRawUTF8(), (float) (r - 1)); safe->update(); }
            });
        }
    private:
        Hamamatsu108Processor& processor;
        juce::String id, label;
        int value = -1;
        bool hover = false;
    };

    //==========================================================================
    // Under the chord buttons: the hand (swing, variation) and ARP / GATE with 16 steps, like a small sequencer
    class PerformPanel : public juce::Component
    {
    public:
        explicit PerformPanel (Hamamatsu108Processor& p) : processor (p)
        {
            Knob::Style st;
            st.labelHeight = 8.0f;
            auto add = [&] (const char* pid, const char* lab) { auto* k = knobs.add (new Knob (processor.apvts, pid, lab, 26, st)); addAndMakeVisible (k); return k; };
            arpKnobs = { add ("strumRate", "RATE"), add ("arpOctaves", "OCTAVES"), add ("arpGate", "LENGTH"), add ("arpSwing", "SWING") };
            gateKnobs = { add ("gateRate", "RATE"), add ("gateDepth", "DEPTH"), add ("gateSmooth", "SMOOTH"), add ("arpPattern", "ARP MODE") };
            gateKnobs[3]->setVisible (false);
            showView (0);
        }

        std::function<void (const juce::String&, const juce::String&)> onStatus;

        // screen skins: neutral knobs here, colour stays on the steps
        void restyle (bool neutral)
        {
            Knob::Style st;
            st.labelHeight = 8.0f;
            if (neutral) { st.ring = juce::Colour (0xffaeb5bd); st.themed = 0; st.pointer = juce::Colour (0xffd9dde2); st.value = juce::Colour (0xffc9ced4); }
            for (auto* k : knobs) k->setStyle (st);
        }

        juce::Rectangle<float> arpButton() const  { return { 14.0f, 8.0f, 128.0f, 26.0f }; }
        juce::Rectangle<float> gateButton() const { return { 150.0f, 8.0f, 128.0f, 26.0f }; }
        juce::Rectangle<float> tab (int i) const  { return { 300.0f + i * 86.0f, 2.0f, 82.0f, 18.0f }; }
        juce::Rectangle<float> bars() const       { return { 300.0f, 26.0f, (float) getWidth() - 314.0f, (float) getHeight() - 34.0f }; }
        juce::Rectangle<float> bar (int i) const  { auto b = bars(); const float w = b.getWidth() / 16.0f; return { b.getX() + i * w + 1.5f, b.getY(), w - 3.0f, b.getHeight() }; }

        void resized() override
        {
            for (int i = 0; i < 4; ++i)
            {
                arpKnobs[(size_t) i]->setBounds (10 + i * 70, 40, 70, 66);
                gateKnobs[(size_t) i]->setBounds (10 + i * 70, 40, 70, 66);
            }
        }

        void showView (int v)
        {
            view = v;
            for (auto* k : arpKnobs) k->setVisible (v == 0);
            for (int i = 0; i < 3; ++i) gateKnobs[(size_t) i]->setVisible (v == 1);
            repaint();
        }

        void update()
        {
            juce::String sig;
            sig << plain (processor.apvts, "strumMode") << plain (processor.apvts, "gateOn") << "|" << processor.arpStepGui.load() << "|" << processor.gateStepGui.load() << "|"
                << (juce::int64) processor.gateMask.load() << "|" << processor.seqLen.load();
            for (auto& v : processor.seqVel) sig << juce::roundToInt (v.load() * 40) << ",";
            if (sig != last) { last = sig; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawHorizontalLine (0, 10.0f, (float) getWidth() - 10.0f);
            g.setFont (spaced (8.5f, true, 0.2f));
            g.setColour (juce::Colour (0xff9aa0a7));

            const bool arpOn = juce::roundToInt (plain (processor.apvts, "strumMode")) == 1;
            const bool gateOn = plain (processor.apvts, "gateOn") > 0.5f;
            drawHardButton (g, arpButton(), "ARP", arpOn, true, true);
            drawHardButton (g, gateButton(), "GATE", gateOn, true, true);

            // tabs: which steps are shown
            for (int i = 0; i < 2; ++i)
            {
                const bool on = i == view;
                g.setColour (on ? th().accent.withAlpha (0.2f) : juce::Colour (0xff1b1e22));
                g.fillRoundedRectangle (tab (i), 4.0f);
                g.setColour (on ? th().accent : juce::Colour (0xff9aa0a7));
                g.setFont (spaced (8.5f, true, 0.15f));
                g.drawText (i == 0 ? juce::String::fromUTF8 ("ARP STEPS") : juce::String::fromUTF8 ("GATE STEPS"), tab (i), juce::Justification::centred, false);
            }
            g.setFont (LookAndFeel::uiFont (10.0f));
            g.setColour (juce::Colour (0xff6e737a));
            g.drawText (view == 0 ? "drag = step strength" : "click = sound on / off", juce::Rectangle<float> (480.0f, 2.0f, (float) getWidth() - 492.0f, 18.0f), juce::Justification::centredRight, false);

            auto b = bars();
            g.setColour (juce::Colour (0xff0c0e10));
            g.fillRoundedRectangle (b.expanded (4.0f), 6.0f);
            const int playing = view == 0 ? (arpOn ? processor.arpStepGui.load() : -1) : processor.gateStepGui.load();
            const auto mask = processor.gateMask.load();
            const int len = view == 0 ? processor.seqLen.load() : processor.gateLen.load();
            for (int i = 0; i < 16; ++i)
            {
                auto c = bar (i);
                const bool in = i < len;
                g.setColour (juce::Colour (0xff1b1e22));
                g.fillRoundedRectangle (c, 3.0f);
                const bool active = view == 0 ? arpOn : gateOn;
                const auto col = th().accent.withAlpha ((in ? 0.95f : 0.3f) * (active ? 1.0f : 0.7f));
                if (view == 0)
                {
                    const float v = processor.seqVel[(size_t) i].load();
                    auto fill = c.withTop (c.getBottom() - c.getHeight() * v);
                    g.setGradientFill (juce::ColourGradient (th().accent.withAlpha (col.getFloatAlpha() * 0.95f), 0.0f, fill.getY(), th().accent.withAlpha (col.getFloatAlpha() * 0.5f), 0.0f, fill.getBottom(), false));
                    g.fillRoundedRectangle (fill, 3.0f);
                    g.setColour (th().accent.withAlpha (col.getFloatAlpha()));
                    g.fillRoundedRectangle (fill.withHeight (3.0f), 1.5f);   // bright top edge = the level
                }
                else if ((mask >> (unsigned) i) & 1u)
                {
                    g.setColour (col);
                    g.fillRoundedRectangle (c, 3.0f);
                }
                if (i % 4 == 0) { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillRect (c.withHeight (2.0f)); }
                if (i == playing) { g.setColour (juce::Colours::white.withAlpha (0.85f)); g.drawRoundedRectangle (c.expanded (1.0f), 3.0f, 1.5f); }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto p = e.position;
            if (arpButton().contains (p))
            {
                const bool on = juce::roundToInt (plain (processor.apvts, "strumMode")) == 1;
                setParam (processor.apvts, "strumMode", on ? 3.0f : 1.0f);
                showView (0);
                if (onStatus) onStatus ("ARP", on ? "off" : "on: the chord is played note by note");
                return;
            }
            if (gateButton().contains (p))
            {
                const bool on = plain (processor.apvts, "gateOn") > 0.5f;
                setParam (processor.apvts, "gateOn", on ? 0.0f : 1.0f);
                showView (1);
                if (onStatus) onStatus ("GATE", on ? "off" : "on: the sound is chopped in time");
                return;
            }
            for (int i = 0; i < 2; ++i) if (tab (i).contains (p)) { showView (i); return; }
            if (bars().expanded (2.0f).contains (p))
            {
                const int i = stepAt (p);
                if (view == 1) paintOn = ! ((processor.gateMask.load() >> (unsigned) i) & 1u);
                edit (i, p.y);
            }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! bars().expanded (2.0f).contains (e.mouseDownPosition)) return;
            edit (stepAt (e.position), e.position.y);
        }

    private:
        int stepAt (juce::Point<float> p) const { auto b = bars(); return juce::jlimit (0, 15, (int) ((p.x - b.getX()) / (b.getWidth() / 16.0f))); }
        void edit (int i, float y)
        {
            if (view == 0)
            {
                auto b = bars();
                processor.seqVel[(size_t) i].store (juce::jlimit (0.05f, 1.0f, (b.getBottom() - y) / b.getHeight()));
            }
            else
            {
                auto m = processor.gateMask.load();
                processor.gateMask.store (paintOn ? (m | (1u << (unsigned) i)) : (m & ~(1u << (unsigned) i)));
                if (processor.gateLen.load() > 16) processor.gateLen.store (16);
            }
            update();
        }

        Hamamatsu108Processor& processor;
        juce::OwnedArray<Knob> knobs;
        std::array<Knob*, 4> arpKnobs {}, gateKnobs {};
        juce::String last;
        int view = 0;
        bool paintOn = true;
    };


    //==========================================================================
    // PROGRESSION: up to 8 chords, one bar each. Click a slot, then chord buttons fill it.
    // PLAY follows the song; DRAG 4 / 8 drops the finished MIDI clip into the piano roll.
    class ProgressionStrip : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit ProgressionStrip (Hamamatsu108Processor& p) : processor (p)
        {
            setTooltip ("PROGRESSION: click a slot, then chord buttons. PLAY = plays right away in the song tempo (follows the bars when FL plays), DRAG 4 / 8 = drag the MIDI into the piano roll.");
        }
        std::function<void (const juce::String&, const juce::String&)> onStatus;

        juce::Rectangle<float> playButton() const { return { 0.0f, 6.0f, 58.0f, 30.0f }; }
        juce::Rectangle<float> slot (int i) const { return { 64.0f + i * 39.0f, 6.0f, 36.0f, 30.0f }; }
        juce::Rectangle<float> modeButton() const { return { 382.0f, 6.0f, 78.0f, 30.0f }; }
        juce::Rectangle<float> diceButton() const { return { 464.0f, 6.0f, 30.0f, 30.0f }; }
        juce::Rectangle<float> dragButton (int i) const { return { 498.0f + i * 49.0f, 6.0f, 46.0f, 30.0f }; }

        void update()
        {
            juce::String sig;
            for (auto& sl : processor.progSlots) sig << sl.load() << ",";
            sig << (processor.progOn.load() ? 1 : 0) << processor.progMode.load() << processor.progEdit.load() << "|" << processor.progSlotGui.load()
                << "|" << plain (processor.apvts, "strumPattern");
            if (sig != last) { last = sig; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            drawHardButton (g, playButton(), "PLAY", processor.progOn.load(), true, true);
            const int edit = processor.progEdit.load(), playing = processor.progSlotGui.load();
            for (int i = 0; i < prog::numSlots; ++i)
            {
                auto r = slot (i);
                const int c = processor.progSlots[(size_t) i].load();
                drawDisplay (g, r, 5.0f);
                if (i == playing)
                {
                    g.setColour (th().accent.withAlpha (0.22f));
                    g.fillRoundedRectangle (r.reduced (2.0f), 4.0f);
                }
                if (i == edit)
                {
                    g.setColour (th().accent);
                    g.drawRoundedRectangle (r.reduced (1.0f), 5.0f, 1.6f);
                }
                g.setFont (LookAndFeel::monoFont (c >= 0 && chords::shortName (c).length() > 4 ? 10.0f : 12.0f));
                if (c >= 0) glowText (g, chords::shortName (c), r, juce::Justification::centred, i == playing ? 1.0f : 0.8f);
                else
                {
                    g.setColour (juce::Colours::white.withAlpha (i == edit ? 0.6f : 0.22f));
                    g.drawText (i == edit ? "?" : juce::String (i + 1), r, juce::Justification::centred, false);
                }
            }
            const int mode = processor.progMode.load();
            juce::String modeText = prog::modeNames()[mode];
            drawHardButton (g, modeButton(), modeText + juce::String::fromUTF8 (" \xe2\x96\xbe"), false, true, false);
            drawHardButton (g, diceButton(), {}, false, true, false);
            {
                // dice icon
                auto d = diceButton().withTrimmedBottom (4.0f).withSizeKeepingCentre (14.0f, 14.0f);
                g.setColour (juce::Colour (0xffecedef));
                g.drawRoundedRectangle (d, 3.0f, 1.4f);
                for (auto pt : { juce::Point<float> (0.3f, 0.3f), juce::Point<float> (0.7f, 0.7f), juce::Point<float> (0.5f, 0.5f) })
                    g.fillEllipse (d.getX() + d.getWidth() * pt.x - 1.5f, d.getY() + d.getHeight() * pt.y - 1.5f, 3.0f, 3.0f);
            }
            for (int i = 0; i < 2; ++i)
            {
                drawHardButton (g, dragButton (i), {}, false, true, false);
                auto t = dragButton (i).withTrimmedBottom (4.0f);
                g.setFont (spaced (8.0f, true, 0.1f));
                g.setColour (juce::Colour (0xff9aa0a7));
                g.drawText ("DRAG", t.removeFromTop (t.getHeight() * 0.5f).translated (0.0f, 2.0f), juce::Justification::centred, false);
                g.setFont (LookAndFeel::uiFont (11.0f, true));
                g.setColour (th().accent);
                g.drawText (i == 0 ? "4 BARS" : "8 BARS", t.translated (0.0f, -1.0f), juce::Justification::centred, false);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto p = e.position;
            dragFrom = -1;
            if (playButton().contains (p))
            {
                processor.progOn.store (! processor.progOn.load());
                status ("PROGRESSION", processor.progOn.load() ? "playing (FL tempo); with FL playing it follows the song bars" : "stopped");
            }
            for (int i = 0; i < prog::numSlots; ++i)
                if (slot (i).contains (p))
                {
                    if (e.mods.isPopupMenu()) { slotMenu (i); return; }
                    processor.progEdit.store (processor.progEdit.load() == i ? -1 : i);
                    status ("Slot " + juce::String (i + 1), processor.progEdit.load() >= 0 ? "now click a chord button" : "done");
                }
            if (modeButton().contains (p)) modeMenu();
            if (diceButton().contains (p))
            {
                processor.progSeed.store (juce::Random::getSystemRandom().nextInt (100000) + 1);
                status ("New idea", processor.progMode.load() == prog::melody ? "a new melody" : "a new variation");
            }
            for (int i = 0; i < 2; ++i) if (dragButton (i).contains (p)) dragFrom = i;
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragFrom < 0 || dragging || e.getDistanceFromDragStart() < 5) return;
            const auto file = writeClip (dragFrom == 0 ? 4 : 8);
            if (! file.existsAsFile()) { status ("PROGRESSION", "empty: click a slot, then chord buttons"); return; }
            dragging = true;
            juce::Component::SafePointer<ProgressionStrip> safe (this);
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                       [safe] { if (safe != nullptr) safe->dragging = false; });
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (dragFrom >= 0 && ! dragging && e.getDistanceFromDragStart() < 5)
                status ("DRAG", "hold the mouse and drag this button into the piano roll or a track");
            dragFrom = -1;
        }

    private:
        void status (const juce::String& a, const juce::String& b) { if (onStatus) onStatus (a, b); }

        void slotMenu (int i)
        {
            juce::PopupMenu m, ready;
            const int key = juce::roundToInt (plain (processor.apvts, "keyRoot"));
            const bool minor = plain (processor.apvts, "keyMode") > 0.5f;
            for (int r = 0; r < (int) prog::readyMade().size(); ++r) ready.addItem (100 + r, prog::readyMade()[(size_t) r].name);
            m.addSubMenu ("Ready-made in " + juce::String (chords::rootName (key)) + (minor ? " minor" : " major"), ready);
            m.addSeparator();
            m.addItem (1, "Clear this slot", processor.progSlots[(size_t) i].load() >= 0);
            m.addItem (2, "Clear all");
            juce::Component::SafePointer<ProgressionStrip> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (slot (i).toNearestInt())), [safe, i, key, minor] (int r)
            {
                if (safe == nullptr || r <= 0) return;
                auto& pr = safe->processor;
                if (r == 1) pr.progSlots[(size_t) i].store (-1);
                if (r == 2) for (auto& sl : pr.progSlots) sl.store (-1);
                if (r >= 100)
                {
                    const auto& rm = prog::readyMade()[(size_t) (r - 100)];
                    for (int k = 0; k < prog::numSlots; ++k) pr.progSlots[(size_t) k].store (k < 4 ? prog::chordOnDegree (key, minor, rm.deg[(size_t) k]) : -1);
                    pr.progOn.store (true);
                    safe->status ("PROGRESSION", juce::String (rm.name) + ": playing, drag 4 / 8 bars into the piano roll");
                }
                safe->repaint();
            });
        }

        void modeMenu()
        {
            juce::PopupMenu m, pats;
            const int mode = processor.progMode.load();
            const int pat = juce::roundToInt (plain (processor.apvts, "strumPattern"));
            for (int i = 1; i < Hamamatsu108Processor::strumPatternNames().size() - 1; ++i)
                pats.addItem (200 + i, Hamamatsu108Processor::strumPatternNames()[i], true, mode == prog::strum && pat == i);
            m.addSubMenu ("STRUM: a strum rhythm", pats, true, nullptr, mode == prog::strum);
            m.addItem (2, "ARP: note by note (settings: ARP panel / EDIT > ARP & GATE)", true, mode == prog::arp);
            m.addItem (3, "MELODY: a tune over the chords (dice = new tune)", true, mode == prog::melody);
            juce::Component::SafePointer<ProgressionStrip> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (modeButton().toNearestInt())), [safe] (int r)
            {
                if (safe == nullptr || r <= 0) return;
                auto& pr = safe->processor;
                if (r >= 200) { pr.progMode.store (prog::strum); setParam (pr.apvts, "strumPattern", (float) (r - 200)); }
                if (r == 2) pr.progMode.store (prog::arp);
                if (r == 3) pr.progMode.store (prog::melody);
                safe->repaint();
            });
        }

        juce::File writeClip (int bars)
        {
            std::array<int, prog::numSlots> slots {};
            for (int i = 0; i < prog::numSlots; ++i) slots[(size_t) i] = processor.progSlots[(size_t) i].load();
            const auto st = processor.progSettings();
            // never cut the progression short: 6 chords in "4 bars" become 6 bars
            int used = 0;
            for (int c : slots) if (c >= 0) ++used;
            if (used > bars) bars = used;
            const auto notes = prog::clip (slots, bars, st);
            if (notes.empty()) return {};
            const int ppqTicks = 960;
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / st.bpm)), 0.0);
            seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Hamamatsu 144"), 0.0);
            for (auto& n : notes)
            {
                const double on = n.beat * ppqTicks, off = (n.beat + n.length) * ppqTicks;
                const int pitch = juce::jlimit (0, 127, n.pitch + 12 * st.octave);
                seq.addEvent (juce::MidiMessage::noteOn (1, pitch, juce::jlimit (0.05f, 1.0f, n.velocity)), on);
                seq.addEvent (juce::MidiMessage::noteOff (1, pitch), juce::jmax (on + 10.0, off));
            }
            seq.addEvent (juce::MidiMessage::endOfTrack(), 4.0 * bars * ppqTicks);
            seq.updateMatchedPairs();
            juce::MidiFile mf;
            mf.setTicksPerQuarterNote (ppqTicks);
            mf.addTrack (seq);
            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Hamamatsu " + prog::modeNames()[st.mode].toLowerCase() + " " + juce::String (bars) + " bars.mid");
            f.deleteFile();
            juce::FileOutputStream os (f);
            if (! os.openedOk()) return {};
            mf.writeTo (os);
            os.flush();
            return f;
        }

        Hamamatsu108Processor& processor;
        juce::String last;
        int dragFrom = -1;
        bool dragging = false;
    };

    //==========================================================================
    // MOTION: a grid of 8 rows (the strings of the chord, low to high) x 16 steps. Light cells, hold a chord
    // (keys, chord buttons or PLAY) and it loops in time with the song.
    class MotionGrid : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        bool plateOnly = false;   // main page: the window is only the strum plate
        float phase = 0.0f;       // the scope trace moves
        int curType() const { return plateOnly ? 4 : juce::roundToInt (plain (processor.apvts, "motionType")); }
        explicit MotionGrid (Hamamatsu108Processor& p) : processor (p)
        {
            setTooltip ("MOTION: click or drag to light cells. Rows = strings of the chord (low to high), columns = steps. Hold a chord and it plays.");
        }

        juce::Rectangle<float> gridArea() const
        {
            if (plateOnly) return getLocalBounds().toFloat().reduced (18.0f, 10.0f).withTrimmedTop (14.0f);
            return getLocalBounds().toFloat().reduced (12.0f, 8.0f).withTrimmedLeft (34.0f).withTrimmedTop (16.0f);
        }
        juce::Rectangle<float> cell (int row, int step) const
        {
            auto g = gridArea();
            const float cw = g.getWidth() / prog::gridSteps, ch = g.getHeight() / prog::gridRows;
            return { g.getX() + step * cw + 1.5f, g.getBottom() - (row + 1) * ch + 1.5f, cw - 3.0f, ch - 3.0f };
        }

        void update()
        {
            juce::String sig;
            for (auto& c : processor.gridCells) sig << c.load() << ",";
            sig << plain (processor.apvts, "motionType") << plain (processor.apvts, "strumPattern");
            for (int r = 0; r < prog::gridRows; ++r) sig << processor.strRate[(size_t) r].load() << processor.strOffset[(size_t) r].load() << processor.strLevel[(size_t) r].load();
            for (auto& c : processor.customSteps) sig << c.load();
            sig << processor.gridStepGui.load() << "|" << plain (processor.apvts, "gridLen") << "|" << processor.activeChord.load() << (plain (processor.apvts, "gridOn") > 0.5f ? "on" : "off");
            if (sig != last) { last = sig; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            if (plateOnly && th().concrete) page::paintScreen (g, r); else drawWell (g, r, 14.0f);
            g.setFont (spaced (8.5f, true, 0.18f));
            g.setColour (plateOnly && th().concrete ? page::phosphorDim() : juce::Colour (0xff9aa0a7));
            const int type = curType();
            g.drawText (type == 0 ? "CLICK / DRAG = LIGHT CELLS  /  HOLD A CHORD OR KEY G0 TO PLAY"
                        : type == 1 ? "DRAW A LINE = A FINGER OVER THE STRINGS  /  RIGHT-DRAG = ERASE"
                        : type == 2 ? "CLICK A STEP = CHANGE THE STROKE  /  IDEAS = 3-3-2, HALF-TIME ..."
                        : type == 3 ? "CLICK = START, DRAG = LEVEL  /  RIGHT-CLICK = OCTAVE, CHANCE, REPEATS"
                                    : "DRAG ACROSS THE STRINGS = YOUR HAND  /  LEFT HAND = CHORD + STROKE  /  KEYS C0..B0 = 12 STROKES",
                        r.reduced (14.0f, 6.0f).withHeight (12.0f).withTrimmedLeft (plateOnly ? 0.0f : 346.0f), juce::Justification::centredLeft, false);
            for (int t = 0; t < (plateOnly ? 0 : 5); ++t)
            {
                auto tb = typeTab (t);
                g.setColour (t == type ? th().accent.withAlpha (0.18f) : juce::Colour (0xff202226));
                g.fillRoundedRectangle (tb, 4.0f);
                g.setColour (t == type ? th().accent : juce::Colour (0xff8a9097));
                g.setFont (spaced (8.5f, true, 0.15f));
                g.drawText (t == 0 ? "GRID" : t == 1 ? "LINES" : t == 2 ? "RHYTHM" : t == 3 ? "STRINGS" : "PLATE", tb, juce::Justification::centred, false);
            }
            const bool on = plain (processor.apvts, "gridOn") > 0.5f;
            const int len = juce::roundToInt (plain (processor.apvts, "gridLen"));
            const int playing = processor.gridStepGui.load();
            // row labels: the notes of the chord that is on
            const int chord = processor.activeChord.load();
            const auto tones = prog::chordTones (chord >= 0 ? chord : chords::index (juce::roundToInt (plain (processor.apvts, "keyRoot")), plain (processor.apvts, "keyMode") > 0.5f ? 1 : 0),
                                                 juce::roundToInt (plain (processor.apvts, "transpose")), juce::roundToInt (plain (processor.apvts, "harpRange")));
            static const char* nn[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
            if (type == 1) { paintLines (g, tones, on, playing); return; }
            if (type == 2) { paintRhythm (g, on, playing); return; }
            if (type == 3) { paintStrings (g, tones, on, playing); return; }
            if (type == 4) { paintPlate (g); return; }
            for (int row = 0; row < prog::gridRows; ++row)
            {
                auto c0 = cell (row, 0);
                juce::String lab;
                if (! tones.empty())
                {
                    const int t = tones[(size_t) juce::roundToInt ((float) row * (float) ((int) tones.size() - 1) / (float) (prog::gridRows - 1))];
                    lab = juce::String (nn[t % 12]) + juce::String (t / 12 - 1);
                }
                g.setFont (LookAndFeel::monoFont (9.5f));
                g.setColour (juce::Colour (0xff8a9097));
                g.drawText (lab, juce::Rectangle<float> (r.getX() + 8.0f, c0.getY(), 34.0f, c0.getHeight()), juce::Justification::centredLeft, false);
                for (int st = 0; st < prog::gridSteps; ++st)
                {
                    auto c = cell (row, st);
                    const bool lit = (processor.gridCells[(size_t) st].load() >> row) & 1;
                    const bool inLen = st < len;
                    g.setColour (st % 4 == 0 ? juce::Colour (0xff23262a) : juce::Colour (0xff1b1d20));
                    g.fillRoundedRectangle (c, 3.0f);
                    if (lit)
                    {
                        const float a = (inLen ? 1.0f : 0.25f) * (on ? 1.0f : 0.45f);
                        if (st == playing && on)
                        {
                            g.setColour (th().accent.withAlpha (0.35f * a));
                            g.fillRoundedRectangle (c.expanded (2.0f), 4.0f);
                        }
                        g.setColour (th().accent.withAlpha (a * (st == playing ? 1.0f : 0.82f)));
                        g.fillRoundedRectangle (c, 3.0f);
                        g.setColour (juce::Colours::white.withAlpha (0.25f * a));
                        g.fillRoundedRectangle (c.withHeight (2.0f), 1.0f);
                    }
                }
            }
            if (playing >= 0 && on)
            {
                auto top = cell (prog::gridRows - 1, playing), bottom = cell (0, playing);
                g.setColour (juce::Colours::white.withAlpha (0.18f));
                g.drawRoundedRectangle (top.getUnion (bottom).expanded (1.5f), 3.0f, 1.0f);
            }
            if (len < prog::gridSteps)
            {
                auto edge = cell (0, len).getX() - 1.5f;
                g.setColour (juce::Colours::white.withAlpha (0.25f));
                g.drawVerticalLine ((int) edge, gridArea().getY(), gridArea().getBottom());
            }
        }

        juce::Rectangle<float> typeTab (int t) const { return { 12.0f + t * 66.0f, 4.0f, 62.0f, 16.0f }; }

        // STRINGS: every string a line with its own pulse; dots = where it plays in this bar, the line glows when it sounds
        juce::Rectangle<float> rateChip (int row) const { const float y = rowY (row); return { gridArea().getX(), y - 8.0f, 44.0f, 16.0f }; }
        juce::Rectangle<float> stringLane() const { return gridArea().withTrimmedLeft (52.0f); }
        void paintStrings (juce::Graphics& g, const std::vector<int>& tones, bool on, int playing)
        {
            static const char* nn[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
            auto lane = stringLane();
            const double pos = playing >= 2000 ? (playing - 2000) / 100.0 : -1.0;   // beats into the bar
            for (int b = 1; b < 4; ++b)
            {
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.drawVerticalLine ((int) (lane.getX() + lane.getWidth() * b / 4.0f), lane.getY() - 6.0f, lane.getBottom() + 6.0f);
            }
            for (int row = 0; row < prog::gridRows; ++row)
            {
                const float y = rowY (row);
                const int rate = processor.strRate[(size_t) row].load();
                const float lvl = processor.strLevel[(size_t) row].load();
                const double period = prog::stringRates()[(size_t) juce::jlimit (0, 9, rate)];
                const double t0 = 0.25 * processor.strOffset[(size_t) row].load();
                if (! tones.empty())
                {
                    const int t = tones[(size_t) juce::roundToInt ((float) row * (float) ((int) tones.size() - 1) / (float) (prog::gridRows - 1))];
                    g.setFont (LookAndFeel::monoFont (9.5f));
                    g.setColour (juce::Colour (0xff8a9097));
                    g.drawText (juce::String (nn[t % 12]) + juce::String (t / 12 - 1), juce::Rectangle<float> (getLocalBounds().toFloat().getX() + 8.0f, y - 7.0f, 34.0f, 14.0f), juce::Justification::centredLeft, false);
                }
                // rate chip
                auto chip = rateChip (row);
                g.setColour (rate > 0 ? th().accent.withAlpha (0.16f) : juce::Colour (0xff202226));
                g.fillRoundedRectangle (chip, 4.0f);
                g.setFont (LookAndFeel::monoFont (9.5f));
                g.setColour (rate > 0 ? th().accent : juce::Colour (0xff6e737a));
                g.drawText (prog::stringRateNames()[rate], chip, juce::Justification::centred, false);
                // the string: steady, thicker = louder; a hit only breathes a soft glow over it (alive, never flashing)
                const bool muted = processor.strMute[(size_t) row].load();
                float glow = 0.0f;
                if (period > 0.0 && pos >= 0.0 && on && ! muted)
                {
                    const double since = std::fmod (pos - t0 + 64.0 * period, period);
                    glow = (float) std::exp (-since / 0.35) * 0.14f;   // slow, faint
                }
                const float thick = 1.2f + 3.2f * lvl;
                g.setColour (rate == 0 || muted ? juce::Colour (0xff2a2d32) : th().accent.withAlpha (on ? 0.32f + 0.3f * lvl : 0.2f));
                g.fillRoundedRectangle (lane.getX(), y - thick * 0.5f, lane.getWidth(), thick, 2.0f);
                if (glow > 0.005f)
                {
                    g.setColour (th().accent.withAlpha (glow));
                    g.fillRoundedRectangle (lane.getX(), y - thick * 0.5f - 3.0f, lane.getWidth(), thick + 6.0f, 4.0f);
                }
                if (period > 0.0 && ! muted)
                    for (double t = t0; t < 4.0 - 1.0e-9; t += period)
                    {
                        const float x = lane.getX() + lane.getWidth() * (float) (t / 4.0);
                        g.setColour (th().accent.withAlpha (on ? 0.85f : 0.4f));
                        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ x, y }));
                        for (int q = 1; q < processor.strRatchet[(size_t) row].load(); ++q)   // repeats: small dots after the hit
                            g.fillEllipse (juce::Rectangle<float> (3.5f, 3.5f).withCentre ({ x + 6.0f * (float) q, y }));
                    }
                {   // what else is set on this string (octave, chance, repeats)
                    juce::StringArray tags;
                    const int oc = processor.strOct[(size_t) row].load(), ch = processor.strChance[(size_t) row].load();
                    if (muted) tags.add ("MUTE");
                    if (oc != 0) tags.add (oc > 0 ? "+1 OCT" : "-1 OCT");
                    if (ch < 100) tags.add (juce::String (ch) + " %");
                    if (processor.strRatchet[(size_t) row].load() > 1) tags.add ("x" + juce::String (processor.strRatchet[(size_t) row].load()));
                    if (! tags.isEmpty())
                    {
                        g.setFont (LookAndFeel::monoFont (8.5f));
                        g.setColour (juce::Colour (0xff9aa0a7));
                        g.drawText (tags.joinIntoString ("  "), juce::Rectangle<float> (lane.getRight() - 150.0f, y - 13.0f, 150.0f, 10.0f), juce::Justification::centredRight, false);
                    }
                }
            }
            if (pos >= 0.0 && on)
            {
                const float x = lane.getX() + lane.getWidth() * (float) (pos / 4.0);
                g.setColour (juce::Colours::white.withAlpha (0.3f));
                g.drawVerticalLine ((int) x, lane.getY() - 6.0f, lane.getBottom() + 6.0f);
            }
        }
        int stringAt (juce::Point<float> p) const
        {
            int best = -1; float bd = 1.0e9f;
            for (int r = 0; r < prog::gridRows; ++r) { const float d = std::abs (p.y - rowY (r)); if (d < bd) { bd = d; best = r; } }
            return bd < 14.0f ? best : -1;
        }
        void stringsMouseDown (const juce::MouseEvent& e)
        {
            const int r = stringAt (e.position);
            if (r < 0) return;
            if (rateChip (r).contains (e.position))
            {
                juce::PopupMenu m;
                m.addSectionHeader ("STRING " + juce::String (r + 1) + ": HOW OFTEN IT PLAYS");
                for (int i = 0; i < prog::stringRateNames().size(); ++i) m.addItem (100 + i, prog::stringRateNames()[i], true, processor.strRate[(size_t) r].load() == i);
                juce::Component::SafePointer<MotionGrid> safe (this);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (rateChip (r).toNearestInt())), [safe, r] (int res)
                {
                    if (safe == nullptr || res < 100) return;
                    safe->processor.strRate[(size_t) r].store (res - 100);
                    safe->repaint();
                });
                return;
            }
            auto lane = stringLane();
            if (e.mods.isPopupMenu() && lane.expanded (0.0f, 14.0f).contains (e.position)) { stringMenu (r); return; }
            if (lane.expanded (0.0f, 14.0f).contains (e.position))
            {
                // click on the string = it starts there (snapped to 16ths); then drag up / down for the level
                processor.strOffset[(size_t) r].store (juce::jlimit (0, 15, (int) ((e.position.x - lane.getX()) / lane.getWidth() * 16.0f)));
                if (processor.strRate[(size_t) r].load() == 0) processor.strRate[(size_t) r].store (5);
                levelRow = r; levelStart = processor.strLevel[(size_t) r].load(); levelY = e.position.y;
                repaint();
            }
        }
        int levelRow = -1; float levelStart = 0.0f, levelY = 0.0f;

        // right-click a string: everything about it
        void stringMenu (int r)
        {
            juce::PopupMenu m, rate, oct, chance, rep;
            const size_t i = (size_t) r;
            m.addSectionHeader ("STRING " + juce::String (r + 1));
            for (int k = 0; k < prog::stringRateNames().size(); ++k) rate.addItem (100 + k, prog::stringRateNames()[k], true, processor.strRate[i].load() == k);
            m.addSubMenu ("How often (rate)", rate);
            const int oc = processor.strOct[i].load();
            oct.addItem (201, "+1 octave", true, oc == 1); oct.addItem (200, "As the chord", true, oc == 0); oct.addItem (199, "-1 octave", true, oc == -1);
            m.addSubMenu ("Octave", oct);
            for (int c : { 100, 85, 70, 50, 30, 15 }) chance.addItem (300 + c, juce::String (c) + " %", true, processor.strChance[i].load() == c);
            m.addSubMenu ("Chance (every hit)", chance);
            rep.addItem (501, "Single hit", true, processor.strRatchet[i].load() <= 1);
            rep.addItem (502, "Double (flam)", true, processor.strRatchet[i].load() == 2);
            rep.addItem (503, "Triple (roll)", true, processor.strRatchet[i].load() == 3);
            m.addSubMenu ("Repeats", rep);
            m.addItem (600, "Mute", true, processor.strMute[i].load());
            m.addSeparator();
            m.addItem (700, "Copy this string to all strings");
            m.addItem (701, "Reset this string");
            juce::Component::SafePointer<MotionGrid> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (rateChip (r).toNearestInt())), [safe, i] (int res)
            {
                if (safe == nullptr || res <= 0) return;
                auto& pr = safe->processor;
                if (res >= 100 && res < 199) pr.strRate[i].store (res - 100);
                else if (res >= 199 && res <= 201) pr.strOct[i].store (res - 200);
                else if (res >= 300 && res <= 400) pr.strChance[i].store (res - 300);
                else if (res >= 501 && res <= 503) pr.strRatchet[i].store (res - 500);
                else if (res == 600) pr.strMute[i].store (! pr.strMute[i].load());
                else if (res == 700)
                    for (size_t k = 0; k < (size_t) prog::gridRows; ++k)
                    {
                        pr.strRate[k].store (pr.strRate[i].load()); pr.strOffset[k].store (pr.strOffset[i].load()); pr.strLevel[k].store (pr.strLevel[i].load());
                        pr.strOct[k].store (pr.strOct[i].load()); pr.strChance[k].store (pr.strChance[i].load()); pr.strRatchet[k].store (pr.strRatchet[i].load());
                    }
                else if (res == 701) { pr.strOct[i].store (0); pr.strChance[i].store (100); pr.strRatchet[i].store (1); pr.strMute[i].store (false); pr.strLevel[i].store (0.75f); }
                safe->repaint();
            });
        }

        // PLATE: the classic strum plate, lying down: drag up or down across the strings
        std::array<float, Hamamatsu108Processor::numStrings> glow {};
        float plateLast = -1.0f;
        float plateY (int s) const { auto a = gridArea(); return a.getBottom() - a.getHeight() * (float) s / (Hamamatsu108Processor::numStrings - 1); }
        void paintPlate (juce::Graphics& g)
        {
            auto a = gridArea();
            for (int s = 0; s < Hamamatsu108Processor::numStrings; ++s)
            {
                const float y = plateY (s), v = glow[(size_t) s];
                const float th0 = 1.0f + 1.6f * (1.0f - (float) s / (float) (Hamamatsu108Processor::numStrings - 1));   // low strings are thicker
                if (th().concrete)
                {
                    // the trace of a string on the scope: flat at rest, a decaying wave when it is plucked
                    juce::Path trace;
                    const float amp = v * v * juce::jmin (9.0f, a.getHeight() / (float) Hamamatsu108Processor::numStrings * 0.9f);
                    const float k = juce::MathConstants<float>::twoPi * (2.0f + (float) (s % 5)) / a.getWidth();
                    trace.startNewSubPath (a.getX(), y);
                    for (float x = a.getX(); x <= a.getRight(); x += 3.0f)
                    {
                        const float u = (x - a.getX()) / a.getWidth();
                        trace.lineTo (x, y + amp * std::sin (u * juce::MathConstants<float>::pi) * std::sin (x * k + phase * (3.0f + (float) s)));
                    }
                    if (v > 0.02f)
                    {
                        g.setColour (page::phosphor().withAlpha (0.3f * v));
                        g.strokePath (trace, juce::PathStrokeType (th0 + 3.0f));
                    }
                    g.setColour (page::phosphor().withAlpha (0.16f + 0.84f * v));
                    g.strokePath (trace, juce::PathStrokeType (v > 0.02f ? juce::jmax (1.0f, th0 * 0.7f) : 0.8f));
                    continue;
                }
                g.setColour (juce::Colour (0xffd9c3a3).withAlpha (0.22f).interpolatedWith (th().accent, v));
                g.fillRect (juce::Rectangle<float> (a.getX(), y - th0 * 0.5f - v, a.getWidth(), th0 + 2.0f * v));
                if (v > 0.05f)
                {
                    g.setColour (th().accent.withAlpha (0.25f * v));
                    g.fillRect (juce::Rectangle<float> (a.getX(), y - 4.0f, a.getWidth(), 8.0f));
                }
            }
        }
    public:
        void flash (std::uint32_t bits)
        {
            bool any = false;
            for (int s = 0; s < Hamamatsu108Processor::numStrings; ++s)
            {
                if (bits & (1u << s)) glow[(size_t) s] = 1.0f;
                if (glow[(size_t) s] > 0.0f) { glow[(size_t) s] = juce::jmax (0.0f, glow[(size_t) s] - 0.035f); any = true; }
            }
            phase += 0.9f;
            if (any && curType() == 4) repaint();
        }
    private:
        void plateMove (juce::Point<float> p)
        {
            auto a = gridArea();
            const float pos = juce::jlimit (0.0f, 1.0f, (a.getBottom() - p.y) / a.getHeight()) * (Hamamatsu108Processor::numStrings - 1);
            if (plateLast >= 0.0f)
            {
                const int s0 = (int) std::floor (juce::jmin (plateLast, pos)), s1 = (int) std::floor (juce::jmax (plateLast, pos));
                for (int s = s0 + 1; s <= s1; ++s) { processor.pluckString (s); glow[(size_t) s] = 1.0f; }
            }
            plateLast = pos;
            repaint();
        }

        // LINES: strings as thin lines, the drawn path glowing, the playhead running across
        void paintLines (juce::Graphics& g, const std::vector<int>& tones, bool on, int playing)
        {
            static const char* nn[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
            auto a = gridArea();
            for (int row = 0; row < prog::gridRows; ++row)
            {
                const float y = rowY (row);
                g.setColour (juce::Colour (0xff2a2d32));
                g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
                if (! tones.empty())
                {
                    const int t = tones[(size_t) juce::roundToInt ((float) row * (float) ((int) tones.size() - 1) / (float) (prog::gridRows - 1))];
                    g.setFont (LookAndFeel::monoFont (9.5f));
                    g.setColour (juce::Colour (0xff8a9097));
                    g.drawText (juce::String (nn[t % 12]) + juce::String (t / 12 - 1), juce::Rectangle<float> (getLocalBounds().toFloat().getX() + 8.0f, y - 7.0f, 34.0f, 14.0f), juce::Justification::centredLeft, false);
                }
            }
            for (int b = 0; b <= 4; ++b)
            {
                g.setColour (juce::Colours::white.withAlpha (b == 0 || b == 4 ? 0.0f : 0.06f));
                g.drawVerticalLine ((int) (a.getX() + a.getWidth() * b / 4.0f), a.getY(), a.getBottom());
            }
            // the path, in segments (the finger can lift)
            juce::Path p;
            bool down = false;
            for (int i = 0; i < prog::linePoints; ++i)
            {
                const float v = processor.motionLine[(size_t) i].load();
                if (v < 0.0f) { down = false; continue; }
                const juce::Point<float> pt (a.getX() + a.getWidth() * (i + 0.5f) / prog::linePoints, a.getBottom() - v * a.getHeight());
                if (! down) { p.startNewSubPath (pt); down = true; } else p.lineTo (pt);
            }
            g.setColour (th().accent.withAlpha (on ? 0.25f : 0.1f));
            g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (th().accent.withAlpha (on ? 1.0f : 0.45f));
            g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (playing >= 1000 && on)
            {
                const int i = juce::jlimit (0, prog::linePoints - 1, playing - 1000);
                const float x = a.getX() + a.getWidth() * (i + 0.5f) / prog::linePoints;
                g.setColour (juce::Colours::white.withAlpha (0.25f));
                g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
                const float v = processor.motionLine[(size_t) i].load();
                if (v >= 0.0f)
                {
                    g.setColour (juce::Colours::white);
                    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ x, a.getBottom() - v * a.getHeight() }));
                }
            }
        }
        // RHYTHM: the strum patterns as strokes; a step's arrow length = how many strings it crosses
        juce::String rhythm() const { return processor.patternString (juce::roundToInt (plain (processor.apvts, "strumPattern"))); }
        juce::Rectangle<float> rhythmStep (int i) const
        {
            auto a = gridArea();
            const float w = a.getWidth() / 16.0f;
            return { a.getX() + i * w + 2.0f, a.getY(), w - 4.0f, a.getHeight() };
        }
        void paintRhythm (juce::Graphics& g, bool on, int playing)
        {
            const auto steps = rhythm();
            const int pat = juce::roundToInt (plain (processor.apvts, "strumPattern"));
            g.setFont (LookAndFeel::monoFont (10.0f));
            g.setColour (juce::Colour (0xff8a9097));
            g.drawText (Hamamatsu108Processor::strumPatternNames()[pat].toUpperCase(), juce::Rectangle<float> (getLocalBounds().toFloat().getX() + 8.0f, gridArea().getY(), 40.0f, 40.0f), juce::Justification::topLeft, true);
            for (int i = 0; i < 16; ++i)
            {
                auto c = rhythmStep (i);
                const bool inLen = i < steps.length();
                g.setColour (i % 4 == 0 ? juce::Colour (0xff23262a) : juce::Colour (0xff1b1d20));
                g.fillRoundedRectangle (c, 4.0f);
                if (! inLen) continue;
                const auto ch = (char) steps[i];
                if (ch == '.') continue;
                const bool up = ch == 'U' || ch == 'u' || ch == 'H';
                // how much of the height the stroke covers: full stroke, half stroke, bass / high only, a tap
                float from = 0.0f, to = 1.0f;
                switch (ch)
                {
                    case 'd': to = 0.6f; break;
                    case 'U': from = 0.3f; break;
                    case 'u': from = 0.6f; break;
                    case 'B': to = 0.25f; break;
                    case 'H': from = 0.75f; break;
                    case 'T': from = 0.45f; to = 0.55f; break;
                    case 'x': from = 0.48f; to = 0.52f; break;
                    default: break;
                }
                auto bar = c.reduced (c.getWidth() * 0.28f, 6.0f);
                const float y0 = bar.getBottom() - bar.getHeight() * from, y1 = bar.getBottom() - bar.getHeight() * to;
                const float a = (on ? 1.0f : 0.45f) * (ch == 'x' ? 0.4f : 1.0f);
                juce::Path arrow;
                const float cx = bar.getCentreX();
                arrow.addArrow (up ? juce::Line<float> (cx, y0, cx, y1) : juce::Line<float> (cx, y1, cx, y0), ch == 'S' ? 5.0f : 3.0f, 11.0f, 9.0f);
                if (i == playing && on) { g.setColour (th().accent.withAlpha (0.25f)); g.fillRoundedRectangle (c, 4.0f); }
                g.setColour (th().accent.withAlpha (a));
                if (ch == 'T' || ch == 'x') g.fillEllipse (juce::Rectangle<float> (ch == 'T' ? 10.0f : 6.0f, ch == 'T' ? 10.0f : 6.0f).withCentre (bar.getCentre()));   // a tap
                else g.fillPath (arrow);
            }
        }
        void rhythmClick (juce::Point<float> p)
        {
            for (int i = 0; i < 16; ++i)
                if (rhythmStep (i).contains (p))
                {
                    // editing makes it your own (CUSTOM): copy the pattern you start from
                    auto steps = rhythm();
                    while (steps.length() < 16) steps += ".";
                    static const juce::String cycle (".DUduBH");
                    const int k = cycle.indexOfChar (steps[i]);
                    const auto next = cycle[(k + 1) % cycle.length()];
                    for (int j = 0; j < 16; ++j) processor.customSteps[(size_t) j].store (j == i ? (int) next : (int) steps[j]);
                    processor.customLen.store (16);
                    setParam (processor.apvts, "strumPattern", 13.0f);
                    repaint();
                    return;
                }
        }
        float rowY (int row) const { auto a = gridArea(); return a.getBottom() - a.getHeight() * (float) row / (prog::gridRows - 1); }

        void drawLine (juce::Point<float> p, bool erase)
        {
            auto a = gridArea();
            const int i = juce::jlimit (0, prog::linePoints - 1, (int) ((p.x - a.getX()) / a.getWidth() * prog::linePoints));
            const float v = erase ? -1.0f : juce::jlimit (0.0f, 1.0f, (a.getBottom() - p.y) / a.getHeight());
            // fill the gap to the last point so a fast stroke stays one line
            if (lastLineIndex >= 0 && std::abs (i - lastLineIndex) >= 1)
            {
                const int d = i > lastLineIndex ? 1 : -1;
                const float v0 = processor.motionLine[(size_t) lastLineIndex].load();
                for (int k = lastLineIndex + d; k != i; k += d)
                    processor.motionLine[(size_t) k].store (erase || v0 < 0.0f ? v : v0 + (v - v0) * (float) (k - lastLineIndex) / (float) (i - lastLineIndex));
            }
            processor.motionLine[(size_t) i].store (v);
            lastLineIndex = i;
            repaint();
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int t = 0; t < 5; ++t)
                if (! plateOnly && typeTab (t).contains (e.position)) { setParam (processor.apvts, "motionType", (float) t); drawValue = -1; repaint(); return; }
            if (curType() == 2) { rhythmClick (e.position); drawValue = -1; return; }
            if (curType() == 3) { levelRow = -1; stringsMouseDown (e); drawValue = 3; return; }
            if (curType() == 4)
            {
                plateLast = -1.0f;
                plateMove (e.position);
                const int s = juce::roundToInt (juce::jlimit (0.0f, 1.0f, (gridArea().getBottom() - e.position.y) / gridArea().getHeight()) * (Hamamatsu108Processor::numStrings - 1));
                processor.pluckString (s);   // a tap plucks one string
                drawValue = 4;
                return;
            }
            if (curType() == 1)
            {
                lastLineIndex = -1;
                lineErase = e.mods.isPopupMenu();
                lastMouse = { -1.0f, -1.0f };
                if (gridArea().contains (e.position)) { drawLine (e.position, lineErase); lastMouse = e.position; }
                drawValue = 2;
                return;
            }
            int row, st;
            if (! hit (e.position, row, st)) { drawValue = -1; return; }
            drawValue = ((processor.gridCells[(size_t) st].load() >> row) & 1) ? 0 : 1;
            set (row, st);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (drawValue == 4) { plateMove (e.position); return; }
            if (drawValue == 3)
            {
                if (levelRow >= 0) { processor.strLevel[(size_t) levelRow].store (juce::jlimit (0.1f, 1.0f, levelStart + (levelY - e.position.y) / 60.0f)); repaint(); }
                return;
            }
            if (drawValue == 2)
            {
                // walk from the last mouse position to this one in small steps: a fast stroke stays one unbroken line
                auto a = gridArea();
                auto clampPt = [a] (juce::Point<float> q) { return juce::Point<float> (juce::jlimit (a.getX(), a.getRight() - 0.01f, q.x), juce::jlimit (a.getY(), a.getBottom(), q.y)); };
                const auto to = clampPt (e.position);
                const auto from = lastMouse.x < 0.0f ? to : lastMouse;
                const int n = juce::jmax (1, (int) (from.getDistanceFrom (to) / 2.0f));
                for (int k = 1; k <= n; ++k) drawLine (from + (to - from) * ((float) k / (float) n), lineErase);
                lastMouse = to;
                return;
            }
            int row, st;
            if (drawValue >= 0 && hit (e.position, row, st)) set (row, st);
        }

    private:
        int lastLineIndex = -1;
        bool lineErase = false;
        juce::Point<float> lastMouse { -1.0f, -1.0f };
        bool hit (juce::Point<float> p, int& row, int& st) const
        {
            auto g = gridArea();
            if (! g.contains (p)) return false;
            st = juce::jlimit (0, prog::gridSteps - 1, (int) ((p.x - g.getX()) / (g.getWidth() / prog::gridSteps)));
            row = juce::jlimit (0, prog::gridRows - 1, prog::gridRows - 1 - (int) ((p.y - g.getY()) / (g.getHeight() / prog::gridRows)));
            return true;
        }
        void set (int row, int st)
        {
            auto& c = processor.gridCells[(size_t) st];
            const int v = drawValue ? (c.load() | (1 << row)) : (c.load() & ~(1 << row));
            if (v != c.load()) { c.store (v); if (drawValue) processor.pluckString (juce::jlimit (0, 15, row * 2)); repaint(); }
        }

        Hamamatsu108Processor& processor;
        juce::String last;
        int drawValue = -1;
    };

    //==========================================================================
    // the MOTION controls in the column right of the chord buttons
    class MotionControls : public juce::Component
    {
    public:
        explicit MotionControls (Hamamatsu108Processor& p) : processor (p) {}
        std::function<void (const juce::String&, const juce::String&)> onStatus;

        juce::Rectangle<float> btn (int col, int row) const { return { 8.0f + col * 56.0f, 6.0f + row * 30.0f, 52.0f, 26.0f }; }

        void update()
        {
            juce::String sig;
            sig << (plain (processor.apvts, "gridOn") > 0.5f ? 1 : 0) << (processor.gridPlay.load() ? 1 : 0) << plain (processor.apvts, "gridRate") << plain (processor.apvts, "gridLen");
            if (sig != last) { last = sig; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            drawHardButton (g, btn (0, 0), "PLAY", processor.gridPlay.load(), true, true);
            drawHardButton (g, btn (1, 0), "MOTION", plain (processor.apvts, "gridOn") > 0.5f, true, true);
            const int rate = juce::roundToInt (plain (processor.apvts, "gridRate"));
            drawHardButton (g, btn (0, 1), Hamamatsu108Processor::strumRateNames()[rate] + juce::String::fromUTF8 (" \xe2\x96\xbe"), false, true, false);
            drawHardButton (g, btn (1, 1), juce::String (juce::roundToInt (plain (processor.apvts, "gridLen"))) + juce::String::fromUTF8 (" st \xe2\x96\xbe"), false, true, false);
            drawHardButton (g, btn (0, 2), juce::String::fromUTF8 ("IDEAS \xe2\x96\xbe"), false, true, false);
            drawHardButton (g, btn (1, 2), "CLEAR", false, true, false);
            drawHardButton (g, btn (0, 3).withWidth (108.0f), {}, false, true, false);
            auto d = btn (0, 3).withWidth (108.0f).withTrimmedBottom (4.0f);
            g.setFont (LookAndFeel::uiFont (11.0f, true));
            g.setColour (th().accent);
            g.drawText (juce::String::fromUTF8 ("DRAG MIDI \xe2\x86\x92"), d, juce::Justification::centred, false);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto p = e.position;
            dragFrom = btn (0, 3).withWidth (108.0f).contains (p);
            if (btn (0, 0).contains (p))
            {
                processor.gridPlay.store (! processor.gridPlay.load());
                status ("PLAY", processor.gridPlay.load() ? "the chord loops (pick chords with the buttons or keys)" : "stopped");
            }
            if (btn (1, 0).contains (p))
            {
                const bool on = plain (processor.apvts, "gridOn") > 0.5f;
                setParam (processor.apvts, "gridOn", on ? 0.0f : 1.0f);
                status ("MOTION", on ? "off: chords sound once, keys play as set" : "on: held chords play the grid");
            }
            if (btn (0, 1).contains (p))
            {
                juce::PopupMenu m;
                const int cur = juce::roundToInt (plain (processor.apvts, "gridRate"));
                for (int i : { 1, 2, 3, 4, 5, 6 }) m.addItem (100 + i, "Step " + Hamamatsu108Processor::strumRateNames()[i], true, cur == i);
                m.addSeparator();
                m.addSectionHeader ("NOTES: how long every string rings");
                const int gate = juce::roundToInt (plain (processor.apvts, "motionGate"));
                static const char* gn[] = { "Ring (the harp decays by itself)", "Long", "Medium (clear pattern)", "Tight (punchy, staccato)" };
                for (int i = 0; i < 4; ++i) m.addItem (150 + i, gn[i], true, gate == i);
                menu (m, btn (0, 1), [this] (int r)
                {
                    if (r >= 150) { setParam (processor.apvts, "motionGate", (float) (r - 150)); return; }
                    setParam (processor.apvts, "gridRate", (float) (r - 100));
                });
            }
            if (btn (1, 1).contains (p))
            {
                juce::PopupMenu m;
                const int cur = juce::roundToInt (plain (processor.apvts, "gridLen"));
                for (int i : { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 }) m.addItem (100 + i, juce::String (i) + " steps", true, cur == i);
                menu (m, btn (1, 1), [this] (int r) { setParam (processor.apvts, "gridLen", (float) (r - 100)); });
            }
            if (btn (0, 2).contains (p))
            {
                juce::PopupMenu m;
                for (int i = 0; i < (int) prog::gridPresets().size(); ++i) m.addItem (100 + i, prog::gridPresets()[(size_t) i].name);
                m.addSeparator();
                {
                    juce::PopupMenu rh;
                    for (int i = 0; i < Hamamatsu108Processor::strumPatternNames().size(); ++i) rh.addItem (400 + i, Hamamatsu108Processor::strumPatternNames()[i]);
                    m.addSubMenu ("RHYTHM patterns (3-3-2, half-time ...)", rh);
                }
                m.addSectionHeader ("STRINGS");
                m.addItem (500, "Web (mixed pulses)");
                m.addItem (501, "Polyrhythm 3 against 4");
                m.addItem (502, "Steady eighths");
                m.addItem (503, "Slow bells");
                m.addSectionHeader ("LINES");
                m.addItem (300, "Wave");
                m.addItem (301, "Zig-zag");
                m.addItem (302, "Climb and fall");
                m.addItem (303, "Two gestures");
                m.addItem (304, "Erase the line");
                m.addSeparator();
                m.addItem (200, "Surprise me (random)");
                m.addItem (201, "Shift right");
                m.addItem (202, "Shift left");
                m.addItem (203, "Flip (low <-> high)");
                menu (m, btn (0, 2), [this] (int r)
                {
                    auto& c = processor.gridCells;
                    if (r >= 500)
                    {
                        static const int rates[4][8] = { { 2, 0, 3, 0, 4, 5, 0, 7 }, { 2, 0, 0, 6, 0, 2, 0, 6 }, { 5, 0, 5, 0, 5, 0, 5, 0 }, { 1, 0, 3, 0, 9, 0, 4, 0 } };
                        static const int offs[4][8] = { { 0, 0, 2, 0, 1, 2, 0, 1 }, { 0, 0, 0, 0, 0, 2, 0, 1 }, { 0, 0, 1, 0, 2, 0, 3, 0 }, { 0, 0, 2, 0, 0, 0, 1, 0 } };
                        for (int k = 0; k < prog::gridRows; ++k)
                        {
                            processor.strRate[(size_t) k].store (rates[r - 500][k]);
                            processor.strOffset[(size_t) k].store (offs[r - 500][k]);
                            processor.strLevel[(size_t) k].store (0.85f - 0.04f * k);
                        }
                        setParam (processor.apvts, "motionType", 3.0f);
                        return;
                    }
                    if (r >= 400) { setParam (processor.apvts, "strumPattern", (float) (r - 400)); setParam (processor.apvts, "motionType", 2.0f); return; }
                    if (r >= 300)
                    {
                        auto& ln = processor.motionLine;
                        for (int i = 0; i < prog::linePoints; ++i)
                        {
                            const float x = i / (float) prog::linePoints;
                            float v = -1.0f;
                            switch (r)
                            {
                                case 300: v = 0.5f + 0.45f * std::sin (x * juce::MathConstants<float>::twoPi * 2.0f); break;
                                case 301: v = std::abs (std::fmod (x * 8.0f, 2.0f) - 1.0f); break;
                                case 302: v = x < 0.5f ? x * 2.0f : 2.0f - x * 2.0f; break;
                                case 303: v = (x < 0.2f) ? x * 5.0f : (x > 0.5f && x < 0.7f) ? 1.0f - (x - 0.5f) * 5.0f : -1.0f; break;
                                default: break;
                            }
                            ln[(size_t) i].store (v < 0.0f ? -1.0f : juce::jlimit (0.0f, 1.0f, v));
                        }
                        setParam (processor.apvts, "motionType", 1.0f);
                        return;
                    }
                    if (r < 200) { setParam (processor.apvts, "motionType", 0.0f); processor.setGridPreset (r - 100); status ("MOTION", prog::gridPresets()[(size_t) (r - 100)].name); return; }
                    if (r == 200)
                    {
                        auto& rnd = juce::Random::getSystemRandom();
                        int prev = rnd.nextInt (8);
                        for (int i = 0; i < prog::gridSteps; ++i)
                        {
                            int v = 0;
                            if (rnd.nextFloat() < (i % 4 == 0 ? 0.95f : i % 2 == 0 ? 0.6f : 0.35f))
                            {
                                prev = juce::jlimit (0, 7, prev + rnd.nextInt (5) - 2);   // moves like a melody, not noise
                                v |= 1 << prev;
                                if (i % 8 == 0) v |= 1 << juce::jmax (0, prev - 4);
                            }
                            c[(size_t) i].store (v);
                        }
                        status ("MOTION", "a new idea");
                    }
                    std::array<int, prog::gridSteps> cp {};
                    for (int i = 0; i < prog::gridSteps; ++i) cp[(size_t) i] = c[(size_t) i].load();
                    if (r == 201) for (int i = 0; i < prog::gridSteps; ++i) c[(size_t) i].store (cp[(size_t) ((i + prog::gridSteps - 1) % prog::gridSteps)]);
                    if (r == 202) for (int i = 0; i < prog::gridSteps; ++i) c[(size_t) i].store (cp[(size_t) ((i + 1) % prog::gridSteps)]);
                    if (r == 203)
                        for (int i = 0; i < prog::gridSteps; ++i)
                        {
                            int v = 0;
                            for (int b = 0; b < 8; ++b) if (cp[(size_t) i] & (1 << b)) v |= 1 << (7 - b);
                            c[(size_t) i].store (v);
                        }
                });
            }
            if (btn (1, 2).contains (p)) { for (auto& c : processor.gridCells) c.store (0); status ("MOTION", "cleared: click cells to draw"); }
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragFrom || dragging || e.getDistanceFromDragStart() < 5) return;
            const auto file = writeClip();
            if (! file.existsAsFile()) { status ("DRAG", "light some cells first"); return; }
            dragging = true;
            juce::Component::SafePointer<MotionControls> safe (this);
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this, [safe] { if (safe != nullptr) safe->dragging = false; });
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (dragFrom && ! dragging && e.getDistanceFromDragStart() < 5)
                status ("DRAG MIDI", "drag into the piano roll (hold SHIFT while dropping: FL skips its import window)");
            dragFrom = false;
        }

    private:
        void status (const juce::String& a, const juce::String& b) { if (onStatus) onStatus (a, b); }
        void menu (juce::PopupMenu& m, juce::Rectangle<float> r, std::function<void (int)> f)
        {
            juce::Component::SafePointer<MotionControls> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (r.toNearestInt())), [safe, f] (int res) { if (safe != nullptr && res > 0) { f (res); safe->repaint(); } });
        }

        juce::File writeClip()
        {
            auto st = processor.progSettings();
            st.mode = st.motionType == 2 ? prog::strum : prog::grid;
            int chord = processor.activeChord.load();
            if (chord < 0) chord = chords::index (juce::roundToInt (plain (processor.apvts, "keyRoot")), plain (processor.apvts, "keyMode") > 0.5f ? 1 : 0);
            std::vector<prog::Note> notes;
            for (int b = 0; b < 4; ++b)
            {
                const size_t s0 = notes.size();
                prog::bar (chord, b, chord, st, notes);
                for (size_t i = s0; i < notes.size(); ++i) notes[i].beat += 4.0 * b;
            }
            if (notes.empty()) return {};
            std::vector<kuprell::midiclip::Note> clip;
            for (auto& n : notes) clip.push_back ({ n.beat, juce::jlimit (0, 127, n.pitch + 12 * st.octave), n.velocity, n.length });
            // with a gate (Long / Medium / Tight) the clip shows what you hear
            static const double gateOf[] = { 1.0, 1.0, 0.6, 0.3 };
            const double g = gateOf[juce::jlimit (0, 3, juce::roundToInt (plain (processor.apvts, "motionGate")))];
            if (st.motionType != 2) for (auto& n : clip) n.length *= g;
            return kuprell::midiclip::write (clip, "Hamamatsu motion " + chords::shortName (chord), processor.captureMode.load() != 0, 16, 16.0);
        }

        Hamamatsu108Processor& processor;
        juce::String last;
        bool dragFrom = false, dragging = false;
    };

    //==========================================================================
#include "MainPage.inc"

    InstrumentView::InstrumentView (Hamamatsu108Processor& p) : processor (p)
    {
        backdrop = std::make_unique<Backdrop>();
        pad = std::make_unique<ChordPad> (p);
        plate = std::make_unique<StrumPlate> (p);
        module = std::make_unique<VoiceModule> (p);
        capture = std::make_unique<MidiCapture> (p);
        rail = std::make_unique<MemoryRail> (p);
        perform = std::make_unique<PerformPanel> (p);
        progression = std::make_unique<ProgressionStrip> (p);
        motion = std::make_unique<MotionGrid> (p);
        motionControls = std::make_unique<MotionControls> (p);
        patternChoice = std::make_unique<RearChoice> (p, "strumPattern", "PATTERN", "How the hand strums (LIVE)");
        keysChoice = std::make_unique<RearChoice> (p, "keysPlay", "YOUR KEYS", "What your keyboard / piano-roll notes do");
        modelDisplay = std::make_unique<ModelDisplay> (p);
        chordSel = std::make_unique<GroupSelector> (p, 1);
        bassSel = std::make_unique<GroupSelector> (p, 2);

        addAndMakeVisible (*backdrop);
        addAndMakeVisible (*pad);
        addAndMakeVisible (*plate);
        addAndMakeVisible (*module);
        addAndMakeVisible (*capture);
        addChildComponent (*rail);   // chord memory is not on the PLAY page any more
        addAndMakeVisible (*perform);
        addChildComponent (*progression);   // retired: the MOTION grid replaces it
        addAndMakeVisible (*motion);
        addAndMakeVisible (*motionControls);
        motionControls->onStatus = [this] (const juce::String& a, const juce::String& b) { if (onStatus) onStatus (a, b); };
        progression->onStatus = [this] (const juce::String& a, const juce::String& b) { if (onStatus) onStatus (a, b); };
        addAndMakeVisible (*patternChoice);
        addAndMakeVisible (*keysChoice);
        perform->onStatus = [this] (const juce::String& a, const juce::String& b) { if (onStatus) onStatus (a, b); };
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
        skins::currentIndex() = processor.skin.load();
        deck.label = skins::deckInk (false);
        deck.value = skins::deckInk (true);
        deck.labelHeight = 10.0f;

        for (auto& gr : deckGroups())
            for (auto& k : gr.knobs)
            {
                auto st = deck;
                if (std::string (gr.name) == "MASTER") { st.ring = skins::deckInk (false); st.pointer = skins::deckInk (false); st.themed = 0; }
                if (k.param != nullptr && k.size == 0)
                {
                    auto* sl = deckSliders.add (new DeckSlider (p.apvts, k.param, k.label));
                    sl->onInteract = [this] (const juce::String& a, const juce::String& b) { if (onStatus) onStatus (a, b); };
                    addAndMakeVisible (sl);
                }
                else if (k.param != nullptr)
                {
                    addKnob (std::make_unique<Knob> (p.apvts, k.param, k.label, k.size, st));
                    const juce::String pid (k.param);
                    if (pid.startsWith ("fx") && pid.endsWith ("Mix"))
                        fxKnobs[(size_t) (pid.substring (2, 3).getIntValue() - 1)] = deckKnobs.getLast();
                }
                else
                    addKnob (std::make_unique<Knob> (k.label, k.size, st, k.soon));
            }

        // HARPOLOGY main page
        bankPresets = std::make_unique<BankPresets> (p);
        switches = std::make_unique<SwitchRow> (p);
        strumBar = std::make_unique<StrumBar> (p);
        lfo1 = std::make_unique<LfoPanel> (p, 0);
        lfo2 = std::make_unique<LfoPanel> (p, 1);
        auto status = [this] (const juce::String& a, const juce::String& b) { if (onStatus) onStatus (a, b); };
        bankPresets->onStatus = status; switches->onStatus = status; strumBar->onStatus = status;
        for (auto* c : std::initializer_list<juce::Component*> { bankPresets.get(), switches.get(), strumBar.get(), lfo1.get(), lfo2.get() }) addAndMakeVisible (c);
        const char* env[] = { "harpAttack", "harpDecay", "harpSustain", "harpRelease" };
        const char* envL[] = { "A", "D", "S", "R" };
        const juce::uint32 caps[] = { 0xffd8d4cc, 0xffe0a040, 0xffd8d4cc, 0xffd94a3a };   // ivory / amber / ivory / red, the old lever colours
        for (int i = 0; i < 4; ++i)
        {
            auto* sl = mainSliders.add (new DeckSlider (p.apvts, env[i], envL[i]));
            sl->capColour = juce::Colour (caps[i]);
            sl->onInteract = status;
            addAndMakeVisible (sl);
        }
        Knob::Style ks;
        ks.ring = page::lit(); ks.themed = 0; ks.pointer = page::lit();
        ks.track = juce::Colours::white.withAlpha (0.08f);
        ks.label = page::dim(); ks.value = page::ink();
        ks.labelHeight = 10.0f;
        const char* kid[] = { "harpTone", "filterCutoff", "master", "morph" };
        const char* kl[] = { "Tone", "Cutoff", "Volume", "MORPH" };
        for (int i = 0; i < 4; ++i)
        {
            auto* k = mainKnobs.add (new Knob (p.apvts, kid[i], kl[i], i == 3 ? 46 : 26, ks));
            k->onInteract = [this] (const Knob& kn) { if (onStatus) onStatus (kn.getDisplayName(), kn.getValueText()); };
            addAndMakeVisible (k);
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
        pad->setBounds (geo::padArea().withHeight (126));
        progression->setVisible (false);
        perform->setVisible (false);   // ARP and GATE live in EDIT > ARP & GATE
        // MOTION grid under the chord buttons, across the old plate column; its controls sit right of the chord buttons
        motion->setBounds (geo::padArea().getX(), geo::midY + 134, geo::plateArea().getRight() - geo::padArea().getX(), geo::midH - 134);
        motionControls->setBounds (geo::plateArea().getX(), geo::midY, geo::plateArea().getWidth(), 128);
        patternChoice->setVisible (false);   // LIVE strum retired
        keysChoice->setVisible (false);      // keys always play Split (Settings)
        modelDisplay->setVisible (false);    // character lives in EDIT > FILTER; the preset bar sits up here now
        chordSel->setVisible (false);
        bassSel->setVisible (false);
        plate->setVisible (false);   // MOTION replaces the plate (strums: black keys, wheels, PC keys)
        module->setBounds (geo::moduleArea());
        capture->setBounds (MidiCapture::area());
        rail->setBounds (MemoryRail::area());
        modelDisplay->setBounds (ModelDisplay::area());

        // HARPOLOGY main page: everything else is hidden (it lives in EDIT)
        pad->setVisible (false);
        module->setVisible (false);
        capture->setVisible (false);
        for (auto* c : deckKnobs) c->setVisible (false);
        for (auto* c : deckSliders) c->setVisible (false);
        bankPresets->setBounds (page::bankArea());
        switches->setBounds (page::switchArea());
        strumBar->setBounds (page::strumBar());
        motionControls->setVisible (false);   // MOTION patterns live in EDIT
        motion->plateOnly = true;
        motion->setBounds (page::strumArea().reduced (10, 0).withTrimmedTop (page::strumBarH + 8).withTrimmedBottom (8));
        {
            const auto d = page::deckArea2();
            for (int i = 0; i < 4; ++i) mainSliders[i]->setBounds (d.getX() + 8 + i * 42, d.getY() + 2, 40, d.getHeight() - 2);
            lfo1->setBounds (d.getX() + 184, d.getY() + 4, 280, d.getHeight() - 8);
            lfo2->setBounds (d.getX() + 470, d.getY() + 4, 280, d.getHeight() - 8);
            const int kx = d.getX() + 760;
            for (int i = 0; i < 3; ++i) mainKnobs[i]->setBounds (kx + i * 50, d.getY() + 10, 50, d.getHeight() - 10);
            mainKnobs[3]->setBounds (kx + 152, d.getY() - 2, d.getRight() - kx - 156, d.getHeight() + 2);   // MORPH: the big one, A to B
        }
        return;

        const auto groups = deckGroupBounds();
        int k = 0, sl = 0;
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
                if (kn.size == 0)
                {
                    deckSliders[sl++]->setBounds (x, groups[gi].getY() + 14, w, groups[gi].getHeight() - 16);
                    x += w;
                    continue;
                }
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
        skins::currentIndex() = skinIndex;
        th().lcd = sk.lcd;
        th().concrete = juce::String (sk.name) == "Concrete";
        th().accent = sk.accent.main;
        th().soft = sk.accent.soft;
        th().deep = sk.accent.deep;
        // the top bar (EDIT, preset tag) takes the instrument's light colour too
        kuprell::colours::accent = sk.accent.main;
        kuprell::colours::accentSoft = sk.accent.soft;
        kuprell::colours::accentDeep = sk.accent.deep;
        if (auto* top = getParentComponent()) top->repaint();
        Knob::themeAccent = sk.accent.main;
        Knob::themeSoft = sk.accent.soft;
        Knob::themeDeep = sk.accent.deep;
        backdrop->skin = skinIndex;
        backdrop->repaint();
        if (perform != nullptr) perform->restyle (sk.lcd);
        for (auto* k : deckKnobs)
        {
            Knob::Style st;
            if (k->getParamID() == "master")
            {
                st.ring = st.pointer = skins::deckInk (false); st.themed = 0;
                st.track = juce::Colours::black.withAlpha (0.18f);
                st.label = skins::deckInk (false); st.value = skins::deckInk (true);
                st.labelHeight = 10.0f;
                k->setStyle (st);
                continue;
            }
            st.ring = sk.accent.deep; st.themed = 2;
            st.track = juce::Colours::black.withAlpha (0.18f);
            st.pointer = sk.accent.deep;
            if (sk.lcd)
            {
                // screen skins: the deck stays calm and neutral, colour is kept for what plays
                st.ring = juce::Colour (0xffaeb5bd); st.themed = 0;
                st.pointer = juce::Colour (0xffd9dde2);
                st.track = juce::Colours::white.withAlpha (0.08f);
            }
            st.label = skins::deckInk (false);
            st.value = skins::deckInk (true);
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
        const auto flashes = processor.stringFlashes.exchange (0);
        plate->update (flashes, chord >= 0 ? chord : processor.detectedChord.load());
        motion->flash (flashes);   // chord detection: piano roll notes too
        module->update (processor.meterLeft.load(), processor.meterRight.load(), chord);
        capture->update();
        rail->update();
        perform->update();
        progression->update();
        motion->update();
        motionControls->update();
        bankPresets->update();
        switches->update();
        strumBar->update();
        lfo1->update();
        lfo2->update();
        patternChoice->update();
        keysChoice->update();
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
