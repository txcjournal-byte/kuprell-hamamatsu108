#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "KuprellColours.h"

namespace kuprell
{
    // Small square toolbar button whose icon is drawn with a Path (sharp at every GUI scale).
    class IconButton : public juce::Button
    {
    public:
        using Painter = std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)>;

        IconButton (const juce::String& name, Painter painterToUse);
        void paintButton (juce::Graphics&, bool isMouseOverButton, bool isButtonDown) override;

    private:
        Painter painter;
    };

    // The Kuprell frame's top bar, shared by every plugin:
    // logo + plugin name on the left, preset browser in the middle, tools and EDIT on the right.
    // Layout follows claude/kuprell-logo.md: logo 12 px high, 16 px from the left edge,
    // 12 px gap and a thin divider before the plugin name (plain UI text, not part of the logo).
    class TopBar : public juce::Component
    {
    public:
        static constexpr int height = 52;

        explicit TopBar (const juce::String& pluginName);

        void setPreset (const juce::String& category, const juce::String& name, bool modified);
        void setEditActive (bool shouldBeActive);
        // embedded: the bar sits inside the plugin's own panel (no background, no logo; the panel shows the logo once)
        void setEmbedded (bool e) { embedded = e; resized(); repaint(); }
        // minimal: only a small "Edit" and the settings (the main page shows sounds itself; save, A/B, random live in EDIT)
        void setMinimal (bool m) { if (minimal != m) { minimal = m; setInterceptsMouseClicks (! m, true); resized(); repaint(); } }

        static inline bool concrete = false;   // brutalist skin: the preset name sits on a phosphor screen
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void()> onPrevPreset, onNextPreset, onPresetClicked, onSave, onCompare,
                              onRandomize, onUndo, onRedo, onHelp, onSettings, onEdit, onLogoClicked;

    private:
        void paintPresetDisplay (juce::Graphics&);

        juce::String pluginName;
        juce::String presetCategory { "INIT" }, presetName { "Init" };
        bool presetModified = false;
        bool embedded = false, minimal = false;

        std::unique_ptr<juce::Drawable> logo;
        juce::Rectangle<int> logoArea, nameArea, presetArea;

        IconButton prevButton, nextButton, randomButton, undoButton, redoButton, helpButton, settingsButton;
        juce::TextButton saveButton { "Save" }, compareButton { "A | B" }, editButton { "EDIT" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
    };
}
