#pragma once

#include "Controls.h"

// PRESETS: browse, filter, favourite, save and delete presets.
namespace hama
{
    class BrowserView : public juce::Component, private juce::ListBoxModel, private juce::Timer
    {
    public:
        explicit BrowserView (Hamamatsu108Processor&);
        ~BrowserView() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void visibilityChanged() override;

        void refresh();
        const std::vector<int>& filtered() const noexcept { return shown; }
        void saveAs();
        void saveCurrent();

        std::function<void (const juce::String&, const juce::String&)> onStatus;
        std::function<void()> onClose;

    private:
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        void timerCallback() override;

        juce::Rectangle<int> categoryRow (int i) const;
        juce::Rectangle<int> modelRow (int i) const;
        juce::Rectangle<int> button (int i) const;

        Hamamatsu108Processor& processor;
        juce::ListBox list;
        juce::TextEditor search;
        std::vector<int> shown;
        int category = 0;   // 0 all, 1..8 categories, 9 favourites, 10 user
        juce::String bankFilter, lastBank;   // "My presets": one of the user's banks (empty = all)
        void showBankMenu();
        void showRowMenu (int idx);
        void askName (const juce::String& title, const juce::String& name, const juce::String& bank, std::function<void (juce::String, juce::String)> done);
        int model = -1;     // -1 all
        int harp = -1;      // HARP TYPE filter: -1 all
        juce::Rectangle<int> harpRow (int i) const;   // 0 = all, 1.. = the harp types
        int harpOf (int presetIndex) const;
        int lastCurrent = -2;
    };
}
