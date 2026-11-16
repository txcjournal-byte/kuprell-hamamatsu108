#pragma once

#include "Controls.h"

// SETTINGS page, ABOUT window and HELP mode.
namespace hama
{
    class SettingsView : public juce::Component, private juce::Timer
    {
    public:
        explicit SettingsView (Hamamatsu108Processor&);
        ~SettingsView() override;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        std::function<void (double)> onZoom;
        std::function<void()> onFit, onClose;
        std::function<int()> currentZoom;

    private:
        void timerCallback() override;
        juce::Rectangle<float> swatch (int i) const;
        juce::Rectangle<float> zoomButton (int i) const;
        juce::Rectangle<float> rowButton (int row, int i) const;
        Hamamatsu108Processor& processor;
    };

    class AboutView : public juce::Component
    {
    public:
        AboutView();
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
    private:
        std::unique_ptr<juce::Drawable> logo;
    };

    class HelpView : public juce::Component
    {
    public:
        HelpView() = default;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
    };
}
