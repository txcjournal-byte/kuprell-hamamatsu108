#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Instrument.h"
#include "EditView.h"
#include "BrowserView.h"
#include "Overlays.h"
#include "KuprellLookAndFeel.h"
#include "KuprellTopBar.h"

// Status bar: what you are touching, plus voices and zoom.
class StatusBar : public juce::Component
{
public:
    static constexpr int height = 28;
    void setText (const juce::String& name, const juce::String& value);
    void setInfo (int voices, int zoomPercent);
    void paint (juce::Graphics&) override;

private:
    juce::String name { "Click a chord button to start" }, value;
    int voices = 0, zoom = 100;
};

class Hamamatsu108Editor : public juce::AudioProcessorEditor, public kuprell::KnobMenuProvider, private juce::Timer
{
public:
    static constexpr int baseWidth = 1280, baseHeight = 760;
    static constexpr double minScale = 0.5, maxScale = 2.0;

    explicit Hamamatsu108Editor (Hamamatsu108Processor&);
    ~Hamamatsu108Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;   // PC keyboard plays (Settings: PC KEYBOARD), otherwise the host gets every key
    void mouseDown (const juce::MouseEvent&) override;   // with PC KEYBOARD on, a click gives the plugin the keyboard
    void updateKeyboardFocus();

private:
    void timerCallback() override;
    int initialWidth() const;
    void showSettingsMenu();
    void setZoom (double scale);

    juce::uint32 openedAt = juce::Time::getMillisecondCounter();
    int intendedWidth = 0, sizeFixes = 0;
    bool fitChecked = false, userSized = false;
    void say (const juce::String& text);

    Hamamatsu108Processor& audioProcessor;

    kuprell::LookAndFeel lookAndFeel;   // declared first, destroyed last
    juce::Component content;            // drawn at 1280 x 760 and scaled as a whole (50-200 %)
    kuprell::TopBar topBar { "Hamamatsu 144" };
    hama::InstrumentView instrument;
    hama::EditView editView;
    hama::BrowserView browser;
    hama::SettingsView settings;
    hama::HelpView help;
    hama::AboutView about;

    enum class Page { play, edit, presets, settings };
    void showPage (Page);
    Page page = Page::play;

    void compareAB();
    void randomize (int section);
    void showRandomMenu();
    void addKnobMenuItems (juce::PopupMenu&, const juce::String& paramID) override;
    void knobMenuResult (int result, const juce::String& paramID) override;

    juce::ValueTree abStates[2];
    int abActive = 0;
    bool abReady = false;
    int lastUndoCount = 0, undoIdleTicks = 0;
    StatusBar status;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Hamamatsu108Editor)
};
