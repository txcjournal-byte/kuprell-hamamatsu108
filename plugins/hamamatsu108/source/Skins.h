#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>

// Themed editions: same layout, own chassis colour, side cheeks, panel pattern, grille motif,
// edition badge, front lettering and one light colour. The Kuprell top bar never changes.
namespace skins
{
    enum class Cheek { walnut, flat, tiles, lacquerGold, dots, carbon, neon, ice, pixels };
    enum class Pattern { none, waves, water, leaves, artDeco, dots, stripes, grid, frost, rainbow };
    enum class Grille { holes, bubbles, moss, vinyl, candy, sunset, snow, checker };

    struct Accent { juce::Colour main, soft, deep; };
    inline Accent turquoise() { return { juce::Colour (0xff3fe0d0), juce::Colour (0xffa6f7ef), juce::Colour (0xff139488) }; }
    inline Accent iceBlue()   { return { juce::Colour (0xff6fb8ff), juce::Colour (0xffc4e2ff), juce::Colour (0xff2f73c9) }; }
    inline Accent pink()      { return { juce::Colour (0xffff6fb4), juce::Colour (0xffffc2df), juce::Colour (0xffc93a7f) }; }

    struct Skin
    {
        const char* name;
        juce::Colour panelTop, panelMid, panelBottom;
        Cheek cheek;
        juce::Colour cheekA, cheekB;          // flat cheeks / pattern colours
        Pattern pattern;
        Grille grille;
        juce::Colour grilleA, grilleB;
        juce::Colour deckTop, deckMid, deckBottom;
        juce::Colour frontTop, frontBottom;
        juce::Colour engrave;
        const char* edition;                  // badge text (empty for Classic)
        juce::Colour badgeBg, badgeFg;
        const char* frontText;
        Accent accent;
        float panelAlpha = 1.0f;              // Frozen: see-through acrylic
    };

    inline const std::array<Skin, 10>& all()
    {
        using C = juce::Colour;
        static const std::array<Skin, 10> s {{
            { "Classic", C (0xff555b63), C (0xff3a3f46), C (0xff2c3036), Cheek::walnut, C (0xff7a4b2a), C (0xff3e2412), Pattern::none, Grille::holes,
              C (0xff3a3e44), C (0xff2a2d32), C (0xffe4e7ea), C (0xffb9bec4), C (0xff9da3aa), C (0xff2e3237), C (0xff16181b), C (0xffa7aeb6),
              "", C (0x00000000), C (0x00000000), "ELECTRONIC STRUM HARP", turquoise() },
            { "Deep Sea", C (0xff4a86f0), C (0xff2c63d6), C (0xff1a45a8), Cheek::flat, C (0xff2453c0), C (0xff12306e), Pattern::waves, Grille::bubbles,
              C (0xff1a4fb0), C (0xff0a2160), C (0xffe8eef8), C (0xffc4d2ea), C (0xffa7b9d9), C (0xff1a45a8), C (0xff12306e), C (0xc8ffffff),
              "\xe2\x89\x88 DEEP SEA", C (0xff0a1c45), C (0xff7fe8ff), "\xe2\x89\x88 DEEP SEA EDITION \xe2\x89\x88", turquoise() },
            { "Pool Party", C (0xff2fd0de), C (0xff14a9b9), C (0xff0b7f8c), Cheek::tiles, C (0xff36d3e3), C (0xff0e8fa0), Pattern::water, Grille::holes,
              C (0xff0b7f8c), C (0xff075860), C (0xffe6f6f8), C (0xffc2e3e8), C (0xffa3cdd3), C (0xfff2f8fa), C (0xffff6fb4), C (0xc8ffffff),
              "\xe2\x97\x8c POOL PARTY", C (0xfff2f8fa), C (0xff0b6f7c), "POOL PARTY EDITION", pink() },
            { "Greenhouse", C (0xff3fa36a), C (0xff278050), C (0xff175c38), Cheek::walnut, C (0xff7a4b2a), C (0xff3e2412), Pattern::leaves, Grille::moss,
              C (0xff1e5a36), C (0xff0f3a22), C (0xffe8f2ec), C (0xffc6ddcf), C (0xffa8c8b4), C (0xff175c38), C (0xff0f3c24), C (0xc8ffffff),
              "\xe2\x9c\xbf GREENHOUSE", C (0xffe8f2ec), C (0xff175c38), "\xe2\x9c\xbf GREENHOUSE EDITION \xe2\x9c\xbf", pink() },
            { "Gold Record", C (0xffe3c88c), C (0xffc2a060), C (0xff97783c), Cheek::lacquerGold, C (0xff2a2a2d), C (0xff0a0a0b), Pattern::artDeco, Grille::vinyl,
              C (0xff2c2c2c), C (0xff151515), C (0xfff4eedf), C (0xffded2b6), C (0xffc6b690), C (0xff1a1a1c), C (0xff050505), C (0xbf281c08),
              "\xe2\x97\x89 GOLD RECORD", C (0xff111111), C (0xffe3c88c), "\xe2\x97\x89 GOLD RECORD EDITION", iceBlue() },
            { "Bubblegum", C (0xfff27bb5), C (0xffd9529a), C (0xffaa3474), Cheek::dots, C (0xfff27bb5), C (0xffc9468a), Pattern::dots, Grille::candy,
              C (0xffffc2df), C (0xfff27bb5), C (0xfff8e8f0), C (0xffeacbdb), C (0xffd9aec4), C (0xffaa3474), C (0xff7a1f50), C (0xc8ffffff),
              "\xe2\x99\xa5 BUBBLEGUM", C (0xffffffff), C (0xffd9529a), "\xe2\x99\xa5 BUBBLEGUM EDITION \xe2\x99\xa5", turquoise() },
            { "Grand Prix", C (0xffe04a52), C (0xffc02e38), C (0xff8e1d26), Cheek::carbon, C (0xff2e2e32), C (0xff19191b), Pattern::stripes, Grille::checker,
              C (0xff8e1d26), C (0xff5e1218), C (0xfff6e6e7), C (0xffe4c5c8), C (0xffcfa6aa), C (0xff1c1c1e), C (0xff060606), C (0xc8ffffff),
              "\xe2\x9a\x91 GRAND PRIX", C (0xfff2f2f2), C (0xff8e1d26), "GRAND PRIX EDITION", iceBlue() },
            { "Midnight Drive", C (0xff8f72f0), C (0xff6e4fd6), C (0xff4d33a8), Cheek::neon, C (0xff3a2690), C (0xff1e1450), Pattern::grid, Grille::sunset,
              C (0xff4d2a8a), C (0xff1e1450), C (0xffeeeaf9), C (0xffd3caef), C (0xffb9ade0), C (0xff2a1c6a), C (0xff120a36), C (0xc8ffffff),
              "\xe2\x96\xb2 MIDNIGHT DRIVE", C (0xff160c3e), C (0xffff9fcb), "MIDNIGHT DRIVE EDITION", turquoise() },
            { "Frozen", C (0xffbedcf6), C (0xff9cc4e8), C (0xff7eaad6), Cheek::ice, C (0xffdcefff), C (0xffa0cdf5), Pattern::frost, Grille::snow,
              C (0xff1d3a55), C (0xff0f2234), C (0xfff2f8fd), C (0xffd3e4f2), C (0xffb8d0e4), C (0xff8fbde6), C (0xff4e7ea8), C (0xe6ffffff),
              "\xe2\x9c\xb1 FROZEN", C (0xe0ffffff), C (0xff2f73c9), "\xe2\x9c\xb1 FROZEN EDITION \xe2\x9c\xb1", iceBlue(), 0.85f },
            { "Arcade", C (0xffff6fb4), C (0xff6fb8ff), C (0xff3fe0d0), Cheek::pixels, C (0xff6fb8ff), C (0xffff6fb4), Pattern::rainbow, Grille::checker,
              C (0xff2a2d32), C (0xff14161a), C (0xffeceef1), C (0xffc8cdd3), C (0xffaeb4bb), C (0xff2a2d32), C (0xff0e1013), C (0xd9ffffff),
              "\xe2\x99\xa5 ARCADE", C (0xff0e1013), C (0xff9df2b5), "INSERT COIN \xc2\xb7 ARCADE EDITION", pink() },
        }};
        return s;
    }

    inline const Skin& get (int i) { return all()[(size_t) juce::jlimit (0, 9, i)]; }

    //==========================================================================
    inline void paintPattern (juce::Graphics& g, juce::Rectangle<float> r, Pattern p)
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (r.toNearestInt());
        juce::Random rnd (108);
        switch (p)
        {
            case Pattern::waves:
                g.setColour (juce::Colours::white.withAlpha (0.07f));
                for (float rad = 30.0f; rad < r.getWidth(); rad += 28.0f)
                    g.drawEllipse (r.getCentreX() - rad, r.getBottom() + r.getHeight() * 0.5f - rad, rad * 2.0f, rad * 2.0f, 2.0f);
                break;
            case Pattern::water:
                for (int i = 0; i < 9; ++i)
                {
                    const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
                    juce::ColourGradient cg (juce::Colours::white.withAlpha (0.16f), x, y, juce::Colours::transparentWhite, x + 90.0f, y, true);
                    g.setGradientFill (cg);
                    g.fillEllipse (x - 90.0f, y - 25.0f, 180.0f, 50.0f);
                }
                break;
            case Pattern::leaves:
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                for (float y = r.getY(); y < r.getBottom(); y += 36.0f)
                    for (float x = r.getX() + (((int) (y / 36.0f)) % 2) * 22.0f; x < r.getRight(); x += 44.0f)
                    {
                        juce::Path leaf;
                        leaf.startNewSubPath (x, y + 14.0f);
                        leaf.quadraticTo (x + 10.0f, y, x + 20.0f, y + 14.0f);
                        leaf.quadraticTo (x + 10.0f, y + 28.0f, x, y + 14.0f);
                        g.fillPath (leaf);
                    }
                break;
            case Pattern::artDeco:
                g.setColour (juce::Colours::white.withAlpha (0.09f));
                for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 14.0f)
                    g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
                break;
            case Pattern::dots:
                g.setColour (juce::Colours::white.withAlpha (0.1f));
                for (float y = r.getY() + 11.0f; y < r.getBottom(); y += 22.0f)
                    for (float x = r.getX() + 11.0f; x < r.getRight(); x += 22.0f)
                        g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
                break;
            case Pattern::stripes:
            {
                const float cx = r.getCentreX();
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.fillRect (juce::Rectangle<float> (cx - 60.0f, r.getY(), 26.0f, r.getHeight()));
                g.fillRect (juce::Rectangle<float> (cx + 34.0f, r.getY(), 26.0f, r.getHeight()));
                break;
            }
            case Pattern::grid:
                g.setColour (juce::Colour (0xffff6fb4).withAlpha (0.14f));
                for (float x = r.getX(); x < r.getRight(); x += 40.0f) g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
                for (float y = r.getY(); y < r.getBottom(); y += 40.0f) g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
                break;
            case Pattern::frost:
            {
                juce::ColourGradient c1 (juce::Colours::white.withAlpha (0.6f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX() + 220.0f, r.getY(), true);
                g.setGradientFill (c1); g.fillRect (r);
                juce::ColourGradient c2 (juce::Colours::white.withAlpha (0.5f), r.getRight(), r.getBottom(), juce::Colours::transparentWhite, r.getRight() - 220.0f, r.getBottom(), true);
                g.setGradientFill (c2); g.fillRect (r);
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                for (int i = 0; i < 18; ++i)
                {
                    const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight(), s = 4.0f + rnd.nextFloat() * 6.0f;
                    for (int k = 0; k < 3; ++k)
                    {
                        const float a = k * juce::MathConstants<float>::pi / 3.0f;
                        g.drawLine (x - s * std::cos (a), y - s * std::sin (a), x + s * std::cos (a), y + s * std::sin (a), 1.0f);
                    }
                }
                break;
            }
            case Pattern::rainbow:
            {
                juce::ColourGradient cg (juce::Colour (0xffff6fb4), r.getX(), r.getY(), juce::Colour (0xff9df2b5), r.getRight(), r.getY(), false);
                cg.addColour (0.25, juce::Colour (0xffb48cff));
                cg.addColour (0.5, juce::Colour (0xff6fb8ff));
                cg.addColour (0.75, juce::Colour (0xff3fe0d0));
                g.setGradientFill (cg);
                g.fillRect (r);
                g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0.0f, r.getCentreY(), juce::Colours::black.withAlpha (0.28f), 0.0f, r.getBottom(), false));
                g.fillRect (r);
                break;
            }
            case Pattern::none: break;
        }
    }

    inline void paintCheekPattern (juce::Graphics& g, juce::Rectangle<float> r, const Skin& s)
    {
        switch (s.cheek)
        {
            case Cheek::tiles:
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                for (float y = r.getY(); y < r.getBottom(); y += 14.0f) g.fillRect (r.getX(), y, r.getWidth(), 2.0f);
                for (float x = r.getX(); x < r.getRight(); x += 14.0f) g.fillRect (x, r.getY(), 2.0f, r.getHeight());
                break;
            case Cheek::lacquerGold:
                g.setColour (juce::Colour (0xffe3c88c));
                g.fillRect (r.withSizeKeepingCentre (r.getWidth() * 0.12f, r.getHeight()));
                break;
            case Cheek::dots:
                g.setColour (juce::Colours::white.withAlpha (0.92f));
                for (float y = r.getY() + 8.0f; y < r.getBottom(); y += 16.0f)
                    for (float x = r.getX() + 8.0f; x < r.getRight(); x += 16.0f) g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
                break;
            case Cheek::carbon:
                for (float y = r.getY(); y < r.getBottom(); y += 6.0f)
                    for (float x = r.getX(); x < r.getRight(); x += 6.0f)
                    {
                        g.setColour (((int) ((x + y) / 6.0f)) % 2 ? s.cheekA : s.cheekB);
                        g.fillRect (x, y, 3.0f, 6.0f);
                    }
                break;
            case Cheek::neon:
                g.setColour (juce::Colour (0xffff6fb4)); g.fillRect (r.withWidth (3.0f));
                g.setColour (juce::Colour (0xff3fe0d0)); g.fillRect (r.withX (r.getRight() - 3.0f).withWidth (3.0f));
                break;
            case Cheek::pixels:
            {
                const juce::Colour cols[] = { juce::Colour (0xffff6fb4), juce::Colour (0xff6fb8ff), juce::Colour (0xff3fe0d0), juce::Colour (0xffb48cff) };
                int k = 0;
                for (float y = r.getY(); y < r.getBottom(); y += 12.0f, ++k)
                    for (float x = r.getX(); x < r.getRight(); x += 12.0f, ++k)
                    { g.setColour (cols[k % 4]); g.fillRect (x, y, 12.0f, 12.0f); }
                break;
            }
            default: break;
        }
    }

    inline void paintGrille (juce::Graphics& g, juce::Rectangle<float> r, const Skin& s)
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r.reduced (4.0f), 9.0f);
        g.reduceClipRegion (clip);
        juce::Random rnd (81);
        switch (s.grille)
        {
            case Grille::bubbles:
                g.setColour (juce::Colour (0xbfaae6ff));
                for (int i = 0; i < 16; ++i)
                {
                    const float d = 3.0f + rnd.nextFloat() * 10.0f;
                    g.drawEllipse (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), d, d, 1.4f);
                }
                break;
            case Grille::moss:
                for (int i = 0; i < 160; ++i)
                {
                    g.setColour (rnd.nextBool() ? juce::Colour (0xff7cc892) : juce::Colour (0xff3e9a5e));
                    const float d = 2.0f + rnd.nextFloat() * 5.0f;
                    g.fillEllipse (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), d, d);
                }
                break;
            case Grille::vinyl:
                g.setColour (juce::Colour (0xff2c2c2c));
                for (float rad = 4.0f; rad < r.getWidth(); rad += 3.0f)
                    g.drawEllipse (r.getCentreX() - rad, r.getCentreY() - rad, rad * 2.0f, rad * 2.0f, 1.0f);
                g.setColour (juce::Colour (0xffe3c88c));
                g.fillEllipse (r.getCentreX() - 9.0f, r.getCentreY() - 9.0f, 18.0f, 18.0f);
                break;
            case Grille::candy:
                g.setColour (juce::Colour (0xfff27bb5));
                for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 16.0f)
                {
                    juce::Path stripe;
                    stripe.addQuadrilateral (x, r.getBottom(), x + 8.0f, r.getBottom(), x + 8.0f + r.getHeight(), r.getY(), x + r.getHeight(), r.getY());
                    g.fillPath (stripe);
                }
                break;
            case Grille::sunset:
            {
                g.setColour (juce::Colour (0xffff6fb4).withAlpha (0.55f));
                for (float y = r.getY() + 3.0f; y < r.getBottom(); y += 7.0f) g.fillRect (r.getX(), y, r.getWidth(), 3.0f);
                g.setColour (juce::Colour (0xffffd6ea));
                g.fillEllipse (r.getCentreX() - 22.0f, r.getBottom() - 18.0f, 44.0f, 44.0f);
                break;
            }
            case Grille::snow:
                g.setColour (juce::Colours::white.withAlpha (0.75f));
                for (float y = r.getY() + 5.0f; y < r.getBottom(); y += 11.0f)
                    for (float x = r.getX() + 6.0f; x < r.getRight(); x += 13.0f) g.fillEllipse (x - 1.5f, y - 1.5f, 3.0f, 3.0f);
                break;
            case Grille::checker:
                for (float y = r.getY(); y < r.getBottom(); y += 8.0f)
                    for (float x = r.getX(); x < r.getRight(); x += 8.0f)
                    {
                        g.setColour ((((int) ((x - r.getX()) / 8.0f) + (int) ((y - r.getY()) / 8.0f)) % 2) ? s.grilleA : s.grilleB);
                        g.fillRect (x, y, 8.0f, 8.0f);
                    }
                break;
            case Grille::holes:
                g.setColour (juce::Colour (0xff040506));
                for (float y = r.getY() + 6.0f; y < r.getBottom() - 2.0f; y += 8.0f)
                    for (float x = r.getX() + 6.0f; x < r.getRight() - 2.0f; x += 8.0f)
                        g.fillEllipse (x - 1.9f, y - 1.9f, 3.8f, 3.8f);
                break;
        }
    }
}
