#include "PluginEditor.h"

using namespace kuprell;

//==============================================================================
void StatusBar::setText (const juce::String& n, const juce::String& v)
{
    if (n != name || v != value)
    {
        name = n;
        value = v;
        repaint();
    }
}

void StatusBar::setInfo (int v, int z)
{
    if (v != voices || z != zoom)
    {
        voices = v;
        zoom = z;
        repaint();
    }
}

void StatusBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (juce::Colour (0xff141517));
    g.fillRect (r);
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());

    r = r.reduced (16, 0);
    g.setFont (LookAndFeel::uiFont (11.5f));
    g.setColour (colours::textSoft);
    const int nameW = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 14;
    g.drawText (name, r.removeFromLeft (nameW), juce::Justification::centredLeft, false);
    g.setFont (LookAndFeel::monoFont (12.0f));
    g.setColour (colours::accentSoft);
    g.drawText (value, r.removeFromLeft (200), juce::Justification::centredLeft, false);

    g.setFont (LookAndFeel::uiFont (11.0f));
    g.setColour (colours::textFaint);
    g.drawText ("Zoom " + juce::String (zoom) + " %", r.removeFromRight (80), juce::Justification::centredRight, false);
    g.drawText ("Voices " + juce::String (voices) + " / " + juce::String (Hamamatsu108Processor::numVoices), r.removeFromRight (110), juce::Justification::centredRight, false);
    g.drawText (juce::String::fromUTF8 ("Keys (FL): C2\xe2\x80\x93" "B2 root \xc2\xb7 C3\xe2\x80\x93G#3 type \xc2\xb7 rest plays the harp"),
                r.removeFromRight (340), juce::Justification::centredRight, false);
}

//==============================================================================
Hamamatsu108Editor::Hamamatsu108Editor (Hamamatsu108Processor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), instrument (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    content.addAndMakeVisible (topBar);
    content.addAndMakeVisible (instrument);
    content.addAndMakeVisible (status);

    topBar.setPreset ("Init", "Init", false);
    instrument.onStatus = [this] (const juce::String& n, const juce::String& v) { status.setText (n, v); };

    topBar.onPrevPreset    = [this] { say ("Presets arrive in step 5 (144 factory sounds)."); };
    topBar.onNextPreset    = topBar.onPrevPreset;
    topBar.onPresetClicked = [this] { say ("The preset browser arrives in step 5."); };
    topBar.onSave          = [this] { say ("Saving presets arrives in step 5."); };
    topBar.onCompare       = [this] { say ("A / B compare arrives in step 6."); };
    topBar.onRandomize     = [this] { say ("Randomize arrives in step 6."); };
    topBar.onUndo          = [this] { say ("Undo / Redo arrives in step 6."); };
    topBar.onRedo          = topBar.onUndo;
    topBar.onHelp          = [this] { say ("Help mode arrives in step 6."); };
    topBar.onSettings      = [this] { say ("Settings (skins, window size, MIDI zones) arrive in step 6."); };
    topBar.onEdit          = [this] { say ("The EDIT pages arrive in steps 3 and 4."); };
    topBar.onLogoClicked   = [this] { say ("About window arrives in step 6."); };

    // resizable 50 % - 200 %, fixed aspect ratio; opens at the last used size, or fitted to the screen
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (baseWidth * minScale), juce::roundToInt (baseHeight * minScale),
                     juce::roundToInt (baseWidth * maxScale), juce::roundToInt (baseHeight * maxScale));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);

    const int w = initialWidth();
    setSize (w, juce::roundToInt (w * (double) baseHeight / (double) baseWidth));
    startTimerHz (10);
}

Hamamatsu108Editor::~Hamamatsu108Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int Hamamatsu108Editor::initialWidth() const
{
    const int saved = audioProcessor.editorWidth.load();
    if (saved > 0)
        return juce::jlimit (juce::roundToInt (baseWidth * minScale), juce::roundToInt (baseWidth * maxScale), saved);

    // first open: 100 % if it fits, otherwise as big as fits on the screen
    double scale = 1.0;
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userArea;
        scale = juce::jmin (1.0, (area.getHeight() - 150) / (double) baseHeight, area.getWidth() * 0.9 / baseWidth);
    }
    scale = juce::jmax (0.6, std::floor (scale * 20.0) / 20.0);   // round down to 5 % steps
    return juce::roundToInt (baseWidth * scale);
}

void Hamamatsu108Editor::paint (juce::Graphics& g)
{
    g.fillAll (colours::window);
}

void Hamamatsu108Editor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;
    audioProcessor.editorWidth.store (getWidth());

    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, baseWidth, baseHeight);

    topBar.setBounds (0, 0, baseWidth, TopBar::height);
    instrument.setBounds (0, TopBar::height, baseWidth, baseHeight - TopBar::height - StatusBar::height);
    status.setBounds (0, baseHeight - StatusBar::height, baseWidth, StatusBar::height);
}

void Hamamatsu108Editor::timerCallback()
{
    status.setInfo (audioProcessor.voicesInUse.load(), juce::roundToInt (100.0 * getWidth() / baseWidth));
}

void Hamamatsu108Editor::say (const juce::String& text)
{
    status.setText (text, {});
}
