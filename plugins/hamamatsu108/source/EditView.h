#pragma once

#include "Controls.h"
#include "KuprellKnob.h"

// EDIT: deep editing in 7 tabs (Engine, Strum & Arp, Chords, Memory, Mod, FX, Character).
namespace hama
{
    class EditPage;

    class EditView : public juce::Component, private juce::Timer
    {
    public:
        explicit EditView (Hamamatsu108Processor&);
        ~EditView() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

        void showTab (int index);
        int getTab() const noexcept { return tab; }
        static const juce::StringArray& tabNames();

        // text for the editor status bar
        std::function<void (const juce::String&, const juce::String&)> onStatus;
        void status (const juce::String& name, const juce::String& value);

    private:
        void timerCallback() override;
        juce::Rectangle<int> tabArea (int i) const;

        Hamamatsu108Processor& processor;
        juce::OwnedArray<EditPage> pages;
        juce::OwnedArray<kuprell::Knob> macroKnobs;
        std::unique_ptr<juce::Drawable> logo;
        juce::String vfdName { "EDIT" }, vfdValue;
        int tab = 0;
    };
}
