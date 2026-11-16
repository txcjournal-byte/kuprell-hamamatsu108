#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "KuprellKnob.h"

// The PLAY view: the instrument as one object on a studio desk.
// Static parts (desk, wood, metal, wells, engravings) are painted once into a cached image;
// only the live parts (keys, strings, displays, knobs, meter) repaint.
namespace hama
{
    // light colour of the instrument (one accent per skin)
    struct Theme
    {
        juce::Colour accent { 0xff3fe0d0 }, soft { 0xffa6f7ef }, deep { 0xff139488 };
    };
    inline Theme& th() { static Theme t; return t; }
    // product shots: draw the instrument without the studio desk behind it
    inline bool& paintDesk() { static bool b = true; return b; }

    // geometry of the instrument in stage coordinates (stage = 1280 x 740 at 100 %)
    namespace geo
    {
        constexpr int bodyX = 80, bodyY = 16, bodyW = 1120, panelH = 588, frontH = 40, cheekW = 42;
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

    class ChordPad;
    class StrumPlate;
    class VoiceModule;
    class MidiCapture;
    class MemoryRail;
    class GroupSelector;
    class ModelDisplay;

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
        std::array<kuprell::Knob*, 3> fxKnobs {};
        juce::OwnedArray<kuprell::Knob> deckKnobs;
        juce::Array<juce::Rectangle<int>> groupBounds;
        int currentSkin = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentView)
    };
}
