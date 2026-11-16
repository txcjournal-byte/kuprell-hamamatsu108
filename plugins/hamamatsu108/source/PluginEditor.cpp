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
    : AudioProcessorEditor (&p), audioProcessor (p), instrument (p), editView (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    content.addAndMakeVisible (topBar);
    content.addAndMakeVisible (instrument);
    content.addChildComponent (editView);
    editView.onStatus = [this] (const juce::String& n, const juce::String& v) { status.setText (n, v); };
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
    topBar.onSettings      = [this] { showSettingsMenu(); };
    topBar.onEdit          = [this] { showEdit (! editView.isVisible()); };
    topBar.onLogoClicked   = [this] { say ("About window arrives in step 6."); };

    // resizable 50 % - 200 %, fixed aspect ratio; opens at the last used size, or fitted to the screen
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (baseWidth * minScale), juce::roundToInt (baseHeight * minScale),
                     juce::roundToInt (baseWidth * maxScale), juce::roundToInt (baseHeight * maxScale));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);

    const int w = initialWidth();
    intendedWidth = w;
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
    // a size the user chose (ignore tiny values a host may have forced during setup)
    const int saved = audioProcessor.editorWidth.load();
    if (saved >= juce::roundToInt (baseWidth * 0.6))
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

void Hamamatsu108Editor::setZoom (double scale)
{
    const int w = juce::roundToInt (baseWidth * scale);
    intendedWidth = w;
    setSize (w, juce::roundToInt (w * (double) baseHeight / (double) baseWidth));
    audioProcessor.editorWidth.store (w);
}

void Hamamatsu108Editor::showSettingsMenu()
{
    juce::PopupMenu zoom;
    const int current = juce::roundToInt (100.0 * getWidth() / baseWidth);
    for (int pct : { 50, 60, 70, 75, 80, 85, 90, 100, 110, 125, 150, 175, 200 })
        zoom.addItem (pct, juce::String (pct) + " %", true, std::abs (pct - current) <= 1);

    juce::PopupMenu m;
    m.addSectionHeader ("Settings");
    m.addSubMenu ("Window size", zoom);
    m.addItem (1000, "Fit to screen");
    m.addSeparator();
    m.addItem (2000, "Skins, MIDI zones, tuning: step 6", false);

    juce::Component::SafePointer<Hamamatsu108Editor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r == 1000)
        {
            safe->audioProcessor.editorWidth.store (0);
            const int w = safe->initialWidth();
            safe->setZoom ((double) w / baseWidth);
        }
        else if (r <= 200)
            safe->setZoom (r / 100.0);
    });
}

void Hamamatsu108Editor::showEdit (bool shouldShow)
{
    editView.setVisible (shouldShow);
    instrument.setVisible (! shouldShow);
    topBar.setEditActive (shouldShow);
}

void Hamamatsu108Editor::paint (juce::Graphics& g)
{
    g.fillAll (colours::window);
}

void Hamamatsu108Editor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;
    // remember only sizes the user picks, not the host's sizing while the window opens
    if (juce::Time::getMillisecondCounter() - openedAt > 1500)
        audioProcessor.editorWidth.store (getWidth());

    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, baseWidth, baseHeight);

    topBar.setBounds (0, 0, baseWidth, TopBar::height);
    instrument.setBounds (0, TopBar::height, baseWidth, baseHeight - TopBar::height - StatusBar::height);
    editView.setBounds (instrument.getBounds());
    status.setBounds (0, baseHeight - StatusBar::height, baseWidth, StatusBar::height);
}

void Hamamatsu108Editor::timerCallback()
{
    // some hosts (FL Studio) shrink the window while opening it: put our size back once it is on screen
    if (sizeFixes < 4 && intendedWidth > 0 && isShowing()
        && juce::Time::getMillisecondCounter() - openedAt < 4000 && std::abs (getWidth() - intendedWidth) > 4)
    {
        ++sizeFixes;
        setSize (intendedWidth, juce::roundToInt (intendedWidth * (double) baseHeight / (double) baseWidth));
    }

    status.setInfo (audioProcessor.voicesInUse.load(), juce::roundToInt (100.0 * getWidth() / baseWidth));
}

void Hamamatsu108Editor::say (const juce::String& text)
{
    status.setText (text, {});
}
