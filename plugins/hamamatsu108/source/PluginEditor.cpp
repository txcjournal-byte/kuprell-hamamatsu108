#include "PluginEditor.h"

using namespace kuprell;

//==============================================================================
namespace
{
    // layout shared by the backdrop and the live parts (base coordinates, 1280 x 768 stage)
    struct StageLayout
    {
        juce::Rectangle<float> backdrop, status, plate, title, subtitle, step, display, message, statusText, meter, meterLabel;

        explicit StageLayout (juce::Rectangle<float> r)
        {
            status = r.removeFromBottom (28.0f);
            backdrop = r;
            plate = r.withSizeKeepingCentre (760.0f, 330.0f);
            auto text = plate.reduced (40.0f, 34.0f);
            title = text.removeFromTop (52.0f);
            subtitle = text.removeFromTop (24.0f);
            text.removeFromTop (18.0f);
            step = text.removeFromTop (22.0f);
            text.removeFromTop (16.0f);
            display = text.removeFromTop (54.0f).withSizeKeepingCentre (420.0f, 54.0f);
            text.removeFromTop (14.0f);
            message = text.removeFromTop (22.0f);

            statusText = status.reduced (16.0f, 0.0f);
            auto s = statusText;
            meter = s.removeFromRight (160.0f).withSizeKeepingCentre (160.0f, 6.0f);
            meterLabel = s.removeFromRight (40.0f);
        }
    };
}

void SkeletonStage::Backdrop::paint (juce::Graphics& g)
{
    const StageLayout L (getLocalBounds().toFloat());
    auto r = L.backdrop;

    // studio backdrop
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4048), r.getCentreX(), r.getY() + r.getHeight() * 0.2f,
                                             juce::Colour (0xff0b0c0e), r.getCentreX(), r.getBottom() + 120.0f, true));
    g.fillRect (r);

    // centre plate (gunmetal, like the instrument body)
    auto plate = L.plate;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (plate.translated (0.0f, 18.0f).expanded (8.0f), 30.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff555b63), plate.getX(), plate.getY(),
                                             juce::Colour (0xff2c3036), plate.getX(), plate.getBottom(), false));
    g.fillRoundedRectangle (plate, 26.0f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawHorizontalLine ((int) plate.getY() + 1, plate.getX() + 26.0f, plate.getRight() - 26.0f);

    g.setColour (colours::text);
    g.setFont (LookAndFeel::uiFont (40.0f, true));
    g.drawText ("Hamamatsu 108", L.title, juce::Justification::centred);

    g.setColour (colours::textDim);
    g.setFont (LookAndFeel::uiFont (14.0f, true));
    g.drawText ("ELECTRONIC STRUM HARP", L.subtitle, juce::Justification::centred);

    g.setColour (colours::accent);
    g.setFont (LookAndFeel::uiFont (13.0f, true));
    g.drawText (juce::String::fromUTF8 ("STEP 1 \xc2\xb7 SKELETON BUILD"), L.step, juce::Justification::centred);

    // status bar
    g.setColour (juce::Colour (0xff121315));
    g.fillRect (L.status);
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine ((int) L.status.getY(), L.status.getX(), L.status.getRight());

    g.setFont (LookAndFeel::uiFont (11.0f));
    g.setColour (colours::textFaint);
    g.drawText (juce::String::fromUTF8 ("Kuprell Hamamatsu 108  \xc2\xb7  v") + JucePlugin_VersionString + juce::String::fromUTF8 ("  \xc2\xb7  test tone only"),
                L.statusText, juce::Justification::centredLeft);
    g.drawText ("OUT", L.meterLabel, juce::Justification::centredRight);
}

void SkeletonStage::resized()
{
    backdrop.setBounds (getLocalBounds());

    const StageLayout L (getLocalBounds().toFloat());
    displayArea = L.display.expanded (6.0f).getSmallestIntegerContainer();
    messageArea = L.message.getSmallestIntegerContainer();
    meterArea   = L.meter.expanded (2.0f).getSmallestIntegerContainer();
}

void SkeletonStage::setActivity (int lastNote, int heldNotes, float level)
{
    if (lastNote != note || heldNotes != held)
    {
        note = lastNote;
        held = heldNotes;
        repaint (displayArea);
    }

    // redraw the meter only when the bar visibly moves
    const auto px = [] (float v) { return juce::roundToInt (160.0f * juce::jlimit (0.0f, 1.0f, v * 1.6f)); };
    if (px (level) != px (meter))
    {
        meter = level;
        repaint (meterArea);
    }
}

void SkeletonStage::setMessage (const juce::String& text)
{
    message = text;
    repaint (messageArea);
}

void SkeletonStage::paint (juce::Graphics&) {}

void SkeletonStage::paintOverChildren (juce::Graphics& g)
{
    const StageLayout L (getLocalBounds().toFloat());

    // MIDI display
    auto display = L.display;
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

    g.setColour (colours::textDim);
    g.setFont (LookAndFeel::uiFont (13.0f));
    g.drawText (message, L.message, juce::Justification::centred);

    // output meter
    g.setColour (juce::Colour (0xff1c1e21));
    g.fillRoundedRectangle (L.meter, 3.0f);
    g.setColour (colours::accent);
    g.fillRoundedRectangle (L.meter.withWidth (L.meter.getWidth() * juce::jlimit (0.0f, 1.0f, meter * 1.6f)), 3.0f);
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

    // resizable 50 % - 200 %, fixed aspect ratio; opens at the last used size, or fitted to the screen
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (baseWidth * minScale), juce::roundToInt (baseHeight * minScale),
                     juce::roundToInt (baseWidth * maxScale), juce::roundToInt (baseHeight * maxScale));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);

    const int w = initialWidth();
    setSize (w, juce::roundToInt (w * (double) baseHeight / (double) baseWidth));
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

int Hamamatsu108Editor::initialWidth() const
{
    const int saved = audioProcessor.editorWidth.load();
    if (saved > 0)
        return juce::jlimit (juce::roundToInt (baseWidth * minScale), juce::roundToInt (baseWidth * maxScale), saved);

    // first open: 100 % if it fits, otherwise shrink so the window takes at most ~80 % of the screen height
    double scale = 1.0;
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userArea;
        scale = juce::jmin (1.0, area.getHeight() * 0.80 / baseHeight, area.getWidth() * 0.90 / baseWidth);
    }
    scale = juce::jmax (minScale, std::floor (scale * 20.0) / 20.0);   // round down to 5 % steps
    return juce::roundToInt (baseWidth * scale);
}

void Hamamatsu108Editor::resized()
{
    const float scale = (float) getWidth() / (float) baseWidth;
    audioProcessor.editorWidth.store (getWidth());

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
