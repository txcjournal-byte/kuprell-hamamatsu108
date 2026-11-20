#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "KuprellKnob.h"

// The PLAY view: the instrument as one object on a studio desk.
// Static parts (desk, wood, metal, wells, engravings) are painted once into a cached image;
// only the live parts (keys, strings, displays, knobs, meter) repaint.
namespace hama
{
    // the brutalist skin's slab (editor coordinates) and the product wordmark, shared by both pages
    void paintConcreteSlab (juce::Graphics&, juce::Rectangle<float> editorBounds);
    void paintWordmark (juce::Graphics&, juce::Point<float> at, juce::Colour);

    // light colour of the instrument (one accent per skin)
    struct Theme
    {
        juce::Colour accent { 0xff3fe0d0 }, soft { 0xffa6f7ef }, deep { 0xff139488 };
        bool lcd = false;     // skin has backlit LCD screens (Graphite)
        bool lcdNow = false;  // the display being painted right now is one of those screens
        bool concrete = false; // Concrete skin: flat surfaces, hard edges, light instead of colour
    };
    inline Theme& th() { static Theme t; return t; }
    // paints the displays inside it as backlit LCD screens (only on skins that have them)
    struct LitScreen
    {
        LitScreen()  { th().lcdNow = th().lcd; }
        ~LitScreen() { th().lcdNow = false; }
    };
    // product shots: draw the instrument without the studio desk behind it
    inline bool& paintDesk() { static bool b = true; return b; }

    // geometry of the instrument in stage coordinates (stage = 1280 x 740 at 100 %)
    namespace geo
    {
        constexpr int bodyX = 18, bodyY = 14, bodyW = 1120, panelH = 588, frontH = 18, cheekW = 42;
        constexpr int innerX = bodyX + cheekW + 20, innerW = bodyW - 2 * cheekW - 40;
        constexpr int rearY = bodyY + 16, rearH = 58;
        constexpr int labelY = rearY + rearH + 14;
        constexpr int midY = labelY + 20, midH = 316;
        constexpr int padW = 624, plateW = 124, gap = 16;
        constexpr int deckY = midY + midH + 14, deckBottom = bodyY + panelH - 18;

        inline juce::Rectangle<int> padArea()    { return { innerX, midY, padW, midH }; }
        inline juce::Rectangle<int> plateArea()  { return { innerX + padW + gap, midY, plateW, midH }; }
        inline juce::Rectangle<int> moduleArea() { return { innerX + padW + plateW + 2 * gap, midY, innerW - padW - plateW - 2 * gap, midH }; }
        inline juce::Rectangle<int> deckArea()   { return { innerX, deckY, innerW, deckBottom - deckY }; }
    }

    namespace page
    {
        // stage geometry of the main page
        constexpr int x0 = geo::innerX, w0 = geo::innerW;
        constexpr int bankY = 40, bankH = 22, tabsX = 300;   // the bank tabs sit up in the top row, next to the name
        constexpr int presetY = bankY + 32, presetRows = 10, presetRowH = 13, presetH = presetRows * presetRowH + 8;
        constexpr int switchY = presetY + presetH + 10, switchH = 68;
        constexpr int strumY = switchY + switchH + 10, strumBarH = 22, strumH = 184;
        constexpr int deckY2 = strumY + strumH + 8;
        inline juce::Rectangle<int> bankArea()   { return { x0, bankY, w0, presetY - bankY + presetH }; }
        inline juce::Rectangle<int> switchArea() { return { x0, switchY, w0, switchH }; }
        inline juce::Rectangle<int> strumArea()  { return { x0, strumY, w0, strumH }; }
        inline juce::Rectangle<int> strumBar()   { return { x0 + 10, strumY + 6, w0 - 20, strumBarH }; }
        inline juce::Rectangle<int> motionCtl()  { return { x0 + 10, strumY + 8 + strumBarH, 124, 128 }; }
        inline juce::Rectangle<int> motionWin()  { return { x0 + 142, strumY + 8 + strumBarH, w0 - 152, strumH - strumBarH - 14 }; }
        inline juce::Rectangle<int> deckArea2()  { return { x0, deckY2, w0, geo::deckBottom - deckY2 }; }
        inline juce::Colour led() { return juce::Colour (0xffc8321e); }
        inline juce::Colour lit() { return juce::Colour (0xffe8e6df); }      // something in the light: chosen, on
        inline juce::Colour litInk() { return juce::Colour (0xff1f1e1c); }   // text on a lit block
        inline juce::Colour onSlab() { return juce::Colour (0xffd8d5cd); }   // text straight on the concrete
        // phosphor screens set into the concrete (old green CRT)
        inline juce::Colour phosphor()    { return juce::Colour (0xff74f5b8); }
        inline juce::Colour phosphorDim() { return juce::Colour (0xff2c7a58); }
        inline void paintScreen (juce::Graphics& g, juce::Rectangle<float> r)
        {
            g.setColour (juce::Colour (0xff1d1d1c));             // the cut in the concrete
            g.fillRect (r);
            const auto s = r.reduced (4.0f);
            g.setColour (juce::Colour (0xff03100b));
            g.fillRect (s);
            g.setColour (juce::Colour (0xff0b2a1d));               // the graticule
            for (float x = s.getX() + 24.0f; x < s.getRight(); x += 24.0f) g.fillRect (x, s.getY(), 1.0f, s.getHeight());
            for (float y = s.getY() + 24.0f; y < s.getBottom(); y += 24.0f) g.fillRect (s.getX(), y, s.getWidth(), 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.22f)); // scan lines
            for (float y = s.getY(); y < s.getBottom(); y += 3.0f) g.fillRect (s.getX(), y, s.getWidth(), 1.0f);
            juce::ColourGradient tube (juce::Colours::transparentBlack, s.getCentreX(), s.getCentreY(), juce::Colours::black.withAlpha (0.45f), s.getX(), s.getY(), true);
            g.setGradientFill (tube);                               // the curved glass falls off at the edges
            g.fillRect (s);
        }
        // phosphor text: the trace, and its faint bloom
        inline void phosphorText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c)
        {
            g.setColour (c.withAlpha (0.22f));
            g.drawText (t, r.translated (0.0f, 1.0f).expanded (0.5f, 0.0f), j, true);
            g.setColour (c);
            g.drawText (t, r, j, true);
        }
        inline juce::Colour ink() { return juce::Colour (0xffd6d3cb); }
        inline juce::Colour dim() { return juce::Colour (0xff8b8882); }
    }


    class ChordPad;
    class StrumPlate;
    class VoiceModule;
    class MidiCapture;
    class MemoryRail;
    class GroupSelector;
    class ModelDisplay;
    class PerformPanel;
    class ProgressionStrip;
    class MotionGrid;
    class MotionControls;
    class DeckSlider;
    class RearChoice;
    class BankPresets;
    class SwitchRow;
    class StrumBar;
    class LfoPanel;

    class InstrumentView : public juce::Component, private juce::Timer
    {
    public:
        explicit InstrumentView (Hamamatsu108Processor&);
        ~InstrumentView() override;

        void resized() override;
        void applySkin (int skinIndex);
        int getSkin() const noexcept { return currentSkin; }

        // text shown in the editor's status bar
        std::function<void (const juce::String& name, const juce::String& value)> onStatus;

    private:
        struct Backdrop;
        void timerCallback() override;
        void addKnob (std::unique_ptr<kuprell::Knob>);

        Hamamatsu108Processor& processor;
        std::unique_ptr<Backdrop> backdrop;
        std::unique_ptr<ChordPad> pad;
        std::unique_ptr<StrumPlate> plate;
        std::unique_ptr<VoiceModule> module;
        std::unique_ptr<MidiCapture> capture;
        std::unique_ptr<MemoryRail> rail;
        std::unique_ptr<GroupSelector> chordSel, bassSel;
        std::unique_ptr<ModelDisplay> modelDisplay;
        std::unique_ptr<PerformPanel> perform;
        std::unique_ptr<ProgressionStrip> progression;
        std::unique_ptr<MotionGrid> motion;
        std::unique_ptr<MotionControls> motionControls;
        std::unique_ptr<RearChoice> patternChoice, keysChoice;
        std::array<kuprell::Knob*, 3> fxKnobs {};
        juce::OwnedArray<kuprell::Knob> deckKnobs;
        juce::OwnedArray<DeckSlider> deckSliders;
        juce::Array<juce::Rectangle<int>> groupBounds;
        int currentSkin = -1;
        std::unique_ptr<BankPresets> bankPresets;
        std::unique_ptr<SwitchRow> switches;
        std::unique_ptr<StrumBar> strumBar;
        std::unique_ptr<LfoPanel> lfo1, lfo2;
        juce::OwnedArray<DeckSlider> mainSliders;
        juce::OwnedArray<kuprell::Knob> mainKnobs;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentView)
    };
}
