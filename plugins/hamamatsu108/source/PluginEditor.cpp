#include "PluginEditor.h"

using namespace kuprell;

//==============================================================================
void SkeletonStage::setActivity (int lastNote, int heldNotes, float level)
{
    if (lastNote != note || heldNotes != held || std::abs (level - meter) > 0.005f)
    {
        note = lastNote;
        held = heldNotes;
        meter = level;
        repaint();
    }
}

void SkeletonStage::setMessage (const juce::String& text)
{
    message = text;
    repaint();
}

void SkeletonStage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto status = r.removeFromBottom (28.0f);

    // studio backdrop
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4048), r.getCentreX(), r.getY() + r.getHeight() * 0.2f,
                                             juce::Colour (0xff0b0c0e), r.getCentreX(), r.getBottom() + 120.0f, true));
    g.fillRect (r);

    // centre plate (gunmetal, like the instrument body)
    auto plate = r.withSizeKeepingCentre (760.0f, 330.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (plate.translated (0.0f, 18.0f).expanded (8.0f), 30.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff555b63), plate.getX(), plate.getY(),
                                             juce::Colour (0xff2c3036), plate.getX(), plate.getBottom(), false));
    g.fillRoundedRectangle (plate, 26.0f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawHorizontalLine ((int) plate.getY() + 1, plate.getX() + 26.0f, plate.getRight() - 26.0f);

    auto text = plate.reduced (40.0f, 34.0f);

    g.setColour (colours::text);
    g.setFont (LookAndFeel::uiFont (40.0f, true));
    g.drawText ("Hamamatsu 108", text.removeFromTop (52.0f), juce::Justification::centred);

    g.setColour (colours::textDim);
    g.setFont (LookAndFeel::uiFont (14.0f, true));
    g.drawText ("ELECTRONIC STRUM HARP", text.removeFromTop (24.0f), juce::Justification::centred);

    text.removeFromTop (18.0f);
    g.setColour (colours::accent);
    g.setFont (LookAndFeel::uiFont (13.0f, true));
    g.drawText (juce::String::fromUTF8 ("STEP 1 \xc2\xb7 SKELETON BUILD"), text.removeFromTop (22.0f), juce::Justification::centred);

    // MIDI display
    text.removeFromTop (16.0f);
    auto display = text.removeFromTop (54.0f).withSizeKeepingCentre (420.0f, 54.0f);
    g.setColour (colours::display);
    g.fillRoundedRectangle (display, 9.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (display, 9.0f, 1.0f);

    auto led = display.removeFromLeft (44.0f).withSizeKeepingCentre (12.0f, 12.0f);
    if (held > 0)
    {
        g.setColour (colours::accent.withAlpha (0.35f));
        g.fillEllipse (led.expanded (5.0f));
        g.setColour (colours::accent);
    }
    else
    {
        g.setColour (juce::Colour (0xff24272b));
    }
    g.fillEllipse (led);

    const juce::String noteText = note < 0 ? "MIDI  waiting for notes"
                                           : "MIDI  last note " + juce::MidiMessage::getMidiNoteName (note, true, true, 4)
                                               + "   held " + juce::String (held);
    g.setColour (colours::accentSoft);
    g.setFont (LookAndFeel::monoFont (16.0f));
    g.drawText (noteText, display.reduced (4.0f, 0.0f), juce::Justification::centredLeft);

    text.removeFromTop (14.0f);
    g.setColour (colours::textDim);
    g.setFont (LookAndFeel::uiFont (13.0f));
    g.drawText (message, text.removeFromTop (22.0f), juce::Justification::centred);

    // status bar
    g.setColour (juce::Colour (0xff121315));
    g.fillRect (status);
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine ((int) status.getY(), status.getX(), status.getRight());

    auto s = status.reduced (16.0f, 0.0f);
    g.setFont (LookAndFeel::uiFont (11.0f));
    g.setColour (colours::textFaint);
    g.drawText (juce::String::fromUTF8 ("Kuprell Hamamatsu 108  \xc2\xb7  v") + JucePlugin_VersionString + juce::String::fromUTF8 ("  \xc2\xb7  test tone only"),
                s, juce::Justification::centredLeft);

    // output meter
    auto meterArea = s.removeFromRight (160.0f).withSizeKeepingCentre (160.0f, 6.0f);
    g.setColour (juce::Colour (0xff1c1e21));
    g.fillRoundedRectangle (meterArea, 3.0f);
    g.setColour (colours::accent);
    g.fillRoundedRectangle (meterArea.withWidth (meterArea.getWidth() * juce::jlimit (0.0f, 1.0f, meter * 1.6f)), 3.0f);
    g.setColour (colours::textFaint);
    g.drawText ("OUT", s.removeFromRight (40.0f), juce::Justification::centredRight);
}

//==============================================================================
Hamamatsu108Editor::Hamamatsu108Editor (Hamamatsu108Processor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    content.addAndMakeVisible (topBar);
    content.addAndMakeVisible (stage);

    topBar.setPreset ("Init", "Init", false);

    topBar.onPrevPreset    = [this] { say ("Presets arrive in step 5 (144 factory sounds)."); };
    topBar.onNextPreset    = topBar.onPrevPreset;
    topBar.onPresetClicked = [this] { say ("The preset browser arrives in step 5."); };
    topBar.onSave          = [this] { say ("Saving presets arrives in step 5."); };
    topBar.onCompare       = [this] { say ("A / B compare arrives in step 6."); };
    topBar.onRandomize     = [this] { say ("Randomize arrives in step 6."); };
    topBar.onUndo          = [this] { say ("Undo / Redo arrives in step 6."); };
    topBar.onRedo          = topBar.onUndo;
    topBar.onHelp          = [this] { say ("Help mode arrives in step 6."); };
    topBar.onSettings      = [this] { say ("Settings (skins, window size, MIDI) arrive in step 6."); };
    topBar.onEdit          = [this] { say ("The EDIT pages arrive with the sound engine (steps 2 to 4)."); };
    topBar.onLogoClicked   = [this] { say ("About window arrives in step 6."); };

    // resizable 75 % - 200 %, fixed aspect ratio
    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);

    setSize (baseWidth, baseHeight);
    startTimerHz (30);
}

Hamamatsu108Editor::~Hamamatsu108Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Hamamatsu108Editor::paint (juce::Graphics& g)
{
    g.fillAll (colours::window);
}

void Hamamatsu108Editor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;

    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, baseWidth, baseHeight);

    topBar.setBounds (0, 0, baseWidth, TopBar::height);
    stage.setBounds (0, TopBar::height, baseWidth, baseHeight - TopBar::height);
}

void Hamamatsu108Editor::timerCallback()
{
    stage.setActivity (audioProcessor.lastNote.load(), audioProcessor.heldNotes.load(), audioProcessor.outputLevel.load());
}

void Hamamatsu108Editor::say (const juce::String& text)
{
    stage.setMessage (text);
}
