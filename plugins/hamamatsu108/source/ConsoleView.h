#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// HARPOLOGY: the one-page console (design v15). A rendered 3D panel with the controls laid over it:
// faders, knobs, keys with LEDs, LED lists, screens, and the strings with a handle at each end.
namespace hama
{
    class ConsoleView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        explicit ConsoleView (Hamamatsu108Processor&);
        ~ConsoleView() override;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void mouseMove (const juce::MouseEvent&) override;

        void flash (std::uint32_t bits);   // strings plucked since the last look

        std::function<void()> onMenu, onSetup, onPrev, onNext, onEffects;
        std::function<void (juce::Component*)> onDragTake, onTakeMenu;
        std::function<bool()> takeLive, hasTake, takeArmed;
        std::function<void (bool)> onArmTake;
        std::function<int()> takeCount;
        std::function<void (const juce::String&, const juce::String&)> onStatus;
        bool effectsOpen = false;

        struct Control;   // one control of the console and what it is tied to

    private:
        void timerCallback() override;
        juce::Point<float> toDesign (juce::Point<float>) const;
        int hitControl (juce::Point<float> design) const;
        void bindAll();

        // strings
        juce::Rectangle<float> bed() const;
        float stringX (int s) const;
        float handleY (int s, bool top) const;
        std::atomic<float>& handleRef (int s, bool top) const;
        int stringAt (float x) const;
        int handleAt (juce::Point<float> p) const;
        void paintStrings (juce::Graphics&);
        void paintKeys (juce::Graphics&);
        int keyAt (juce::Point<float>) const;
        juce::Rectangle<float> keyRect (int note) const;

        Hamamatsu108Processor& processor;
        juce::Image background, knobStrip, capLight, capDark;
        juce::Image bgScaled;   // the panel at the size it is shown: drawn once, then only copied
        std::vector<std::unique_ptr<Control>> controls;
        int dragging = -1;
        float dragStartValue = 0.0f, dragStartY = 0.0f;
        std::array<float, Hamamatsu108Processor::numStrings> glow {};
        float phase = 0.0f;
        int bedLayer = 0;          // 0 = TONE (bottom) / BEND (top), 1 = TIME (bottom) / LEVEL (top)
        bool keysView = false;
        int dragHandle = -1, heldKey = -1;
        float lastStrumX = -1.0f;
        juce::int64 lastSig = 0;
        juce::Rectangle<float> resetKey() const;
        void resetMenu();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConsoleView)
    };
}
