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
    : AudioProcessorEditor (&p), audioProcessor (p), instrument (p), editView (p), browser (p), settings (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    content.addAndMakeVisible (topBar);
    content.addAndMakeVisible (instrument);
    content.addChildComponent (editView);
    content.addChildComponent (browser);
    content.addChildComponent (settings);
    content.addChildComponent (help);
    editView.onStatus = [this] (const juce::String& n, const juce::String& v) { status.setText (n, v); };
    browser.onStatus = editView.onStatus;
    browser.onClose = [this] { showPage (Page::play); };
    settings.onClose = browser.onClose;
    settings.onZoom = [this] (double z) { setZoom (z); };
    settings.onFit = [this] { audioProcessor.editorWidth.store (0); setZoom ((double) initialWidth() / baseWidth); };
    settings.currentZoom = [this] { return juce::roundToInt (100.0 * getWidth() / baseWidth); };
    content.addAndMakeVisible (status);
    content.addChildComponent (about);

    topBar.setPreset (audioProcessor.presetManager->currentCategory(), audioProcessor.presetManager->currentName(), audioProcessor.presetManager->isModified());
    instrument.onStatus = [this] (const juce::String& n, const juce::String& v) { status.setText (n, v); };

    topBar.onPrevPreset    = [this] { browser.refresh(); audioProcessor.presetManager->loadNext (-1, browser.filtered()); say ("Preset: " + audioProcessor.presetManager->currentName()); };
    topBar.onNextPreset    = [this] { browser.refresh(); audioProcessor.presetManager->loadNext (1, browser.filtered()); say ("Preset: " + audioProcessor.presetManager->currentName()); };
    topBar.onPresetClicked = [this] { showPage (page == Page::presets ? Page::play : Page::presets); };
    topBar.onPrevPreset    = [this] { browser.refresh(); audioProcessor.presetManager->loadNext (-1, browser.filtered()); audioProcessor.previewPreset(); say ("Preset: " + audioProcessor.presetManager->currentName()); };
    topBar.onNextPreset    = [this] { browser.refresh(); audioProcessor.presetManager->loadNext (1, browser.filtered()); audioProcessor.previewPreset(); say ("Preset: " + audioProcessor.presetManager->currentName()); };
    topBar.onSave          = [this] { browser.saveCurrent(); };
    topBar.onCompare       = [this] { compareAB(); };
    topBar.onRandomize     = [this] { showRandomMenu(); };
    topBar.onUndo          = [this] { audioProcessor.undoManager.beginNewTransaction(); audioProcessor.undoManager.undo(); say ("Undo"); };
    topBar.onRedo          = [this] { audioProcessor.undoManager.redo(); say ("Redo"); };
    topBar.onHelp          = [this] { showPage (Page::play); help.setVisible (! help.isVisible()); help.toFront (false); };
    topBar.onSettings      = [this] { showPage (page == Page::settings ? Page::play : Page::settings); };
    topBar.onEdit          = [this] { showPage (page == Page::edit ? Page::play : Page::edit); };
    topBar.onLogoClicked   = [this] { about.setVisible (true); about.toFront (false); };

    // resizable 50 % - 200 %, fixed aspect ratio; opens at the last used size, or fitted to the screen
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (baseWidth * minScale), juce::roundToInt (baseHeight * minScale),
                     juce::roundToInt (baseWidth * maxScale), juce::roundToInt (baseHeight * maxScale));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);

    // the plugin never keeps the keyboard: space, transport and shortcuts stay with FL Studio
    std::function<void (juce::Component&)> noFocus = [&noFocus] (juce::Component& c)
    {
        if (dynamic_cast<juce::TextEditor*> (&c) == nullptr)
        {
            c.setWantsKeyboardFocus (false);
            c.setMouseClickGrabsKeyboardFocus (false);
        }
        for (auto* child : c.getChildren()) noFocus (*child);
    };
    noFocus (*this);

    const int w = initialWidth();
    intendedWidth = w;
    setSize (w, juce::roundToInt (w * (double) baseHeight / (double) baseWidth));
    startTimerHz (30);   // fast while the window opens, so a host resize is undone within a frame
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

void Hamamatsu108Editor::showPage (Page p)
{
    page = p;
    instrument.setVisible (p == Page::play);
    editView.setVisible (p == Page::edit);
    browser.setVisible (p == Page::presets);
    settings.setVisible (p == Page::settings);
    if (p != Page::play) help.setVisible (false);
    topBar.setEditActive (p == Page::edit);
}

void Hamamatsu108Editor::compareAB()
{
    auto& s = audioProcessor.apvts;
    if (! abReady)
    {
        abStates[0] = s.copyState();
        abStates[1] = abStates[0].createCopy();
        abReady = true;
    }
    abStates[abActive] = s.copyState();
    abActive = 1 - abActive;
    s.replaceState (abStates[abActive].createCopy());
    say (abActive == 0 ? "Comparing: A (click A | B to hear B)" : "Comparing: B (click A | B to hear A)");
}

void Hamamatsu108Editor::showRandomMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Randomize (locked controls stay)");
    m.addItem (1, "Sound: voices, tone, decay, layers");
    m.addItem (2, "Strum and arpeggio");
    m.addItem (3, "Effects");
    m.addItem (4, "Modulation");
    m.addSeparator();
    m.addItem (5, "Everything");
    juce::Component::SafePointer<Hamamatsu108Editor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe] (int r) { if (safe != nullptr && r > 0) safe->randomize (r); });
}

void Hamamatsu108Editor::randomize (int section)
{
    static const char* sound[] = { "harpVoice", "harpTone", "harpDecay", "harpRange", "dual", "harpVoice2", "morph", "chordType", "chordFilter", "voicing",
                                   "bassType", "bassMode", "bassTone", "vibRate", "vibDepth", "model", "macroAge", "macroDream", "macroWobble" };
    static const char* strum[] = { "strumRate", "strumFeel", "strumSpeed", "arpPattern", "arpChance", "arpRegen" };
    static const char* modIds[] = { "lfo1Rate", "lfo1Shape", "lfo2Rate", "lfo2Shape", "funcRate", "funcMode", "randRate", "randSmooth", "randChaos",
                                    "mod1Src", "mod1Dst", "mod1Amt", "mod2Src", "mod2Dst", "mod2Amt" };
    juce::StringArray ids;
    if (section == 1 || section == 5) for (auto* i : sound) ids.add (i);
    if (section == 2 || section == 5) for (auto* i : strum) ids.add (i);
    if (section == 3 || section == 5)
        for (int k = 1; k <= 4; ++k) for (auto* f : { "Type", "A", "B", "Mix" }) ids.add ("fx" + juce::String (k) + f);
    if (section == 4 || section == 5) for (auto* i : modIds) ids.add (i);

    juce::Random rnd;
    auto& s = audioProcessor.apvts;
    s.undoManager->beginNewTransaction ("Randomize");
    for (auto& id : ids)
    {
        if (audioProcessor.presetManager->locked.count (id) > 0) continue;
        auto* p = s.getParameter (id);
        if (p == nullptr) continue;
        float v = rnd.nextFloat();
        if (dynamic_cast<juce::AudioParameterFloat*> (p) != nullptr) v = 0.12f + 0.76f * v;   // stay in a musical middle range
        if (id.endsWith ("Mix")) v *= 0.6f;
        if (id == "chordType" || id == "bassType") v *= 0.9f;
        p->beginChangeGesture();
        p->setValueNotifyingHost (v);
        p->endChangeGesture();
    }
    say ("Randomized");
}

void Hamamatsu108Editor::addKnobMenuItems (juce::PopupMenu& m, const juce::String& paramID)
{
    const auto& params = audioProcessor.getParameters();
    int index = -1;
    for (int i = 0; i < params.size(); ++i)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (params[i])) if (rp->getParameterID() == paramID) index = i;
    const int cc = audioProcessor.ccForParam (index);
    m.addItem (100, audioProcessor.learnParam.load() == index ? "MIDI Learn: waiting for a controller..." : "MIDI Learn");
    if (cc >= 0) m.addItem (101, "Forget MIDI (CC " + juce::String (cc) + ")");
    const bool locked = audioProcessor.presetManager->locked.count (paramID) > 0;
    m.addItem (102, locked ? "Unlock (presets and dice change it again)" : "Lock (presets and dice keep it)", true, locked);
}

void Hamamatsu108Editor::knobMenuResult (int result, const juce::String& paramID)
{
    const auto& params = audioProcessor.getParameters();
    int index = -1;
    for (int i = 0; i < params.size(); ++i)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (params[i])) if (rp->getParameterID() == paramID) index = i;
    if (index < 0) return;
    if (result == 100) { audioProcessor.learnParam.store (index); say ("MIDI Learn: move a knob or slider on your controller"); }
    if (result == 101) { const int cc = audioProcessor.ccForParam (index); if (cc >= 0) audioProcessor.ccMap[(size_t) cc].store (-1); say ("MIDI mapping removed"); }
    if (result == 102)
    {
        auto& locked = audioProcessor.presetManager->locked;
        if (locked.count (paramID)) { locked.erase (paramID); say ("Unlocked"); }
        else { locked.insert (paramID); say ("Locked: presets and dice keep this value"); }
    }
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
    browser.setBounds (instrument.getBounds());
    settings.setBounds (instrument.getBounds());
    help.setBounds (0, 0, baseWidth, baseHeight - StatusBar::height);
    about.setBounds (0, 0, baseWidth, baseHeight);
    status.setBounds (0, baseHeight - StatusBar::height, baseWidth, StatusBar::height);
}

void Hamamatsu108Editor::timerCallback()
{
    // preset name in the top bar
    auto& pm = *audioProcessor.presetManager;
    topBar.setPreset (pm.currentCategory().toUpperCase(), pm.currentName() + (abReady ? (abActive == 0 ? "  [A]" : "  [B]") : juce::String()), pm.isModified());

    // group parameter changes into undo steps
    const int n = audioProcessor.undoManager.getNumActionsInCurrentTransaction();
    if (n != lastUndoCount) { lastUndoCount = n; undoIdleTicks = 0; }
    else if (n > 0 && ++undoIdleTicks > 4) { audioProcessor.undoManager.beginNewTransaction(); lastUndoCount = 0; }

    // some hosts (FL Studio) shrink the window while opening it: put our size back once it is on screen
    if (sizeFixes < 8 && intendedWidth > 0 && isShowing()
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
