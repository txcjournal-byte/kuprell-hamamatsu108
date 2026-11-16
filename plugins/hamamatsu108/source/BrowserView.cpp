#include "BrowserView.h"

using namespace kuprell;
using namespace hama::look;

namespace hama
{
    namespace
    {
        const char* const catLabels[] = { "All presets", "Harp", "Pluck", "Bell", "Keys", "Pad", "Texture", "\xe2\x99\xa5  Favourites", "My presets" };
        const char* const buttons[] = { "Save", "Save As...", "Delete", "Show folder", "Back to PLAY" };
    }

    BrowserView::BrowserView (Hamamatsu108Processor& p) : processor (p)
    {
        list.setModel (this);
        list.setRowHeight (30);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0d0f11));
        list.setColour (juce::ListBox::outlineColourId, juce::Colours::black);
        list.setOutlineThickness (1);
        addAndMakeVisible (list);

        search.setTextToShowWhenEmpty ("Search presets...", juce::Colour (0xff6e737a));
        search.setFont (LookAndFeel::uiFont (14.0f));
        search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff07090a));
        search.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff2c2f34));
        search.setColour (juce::TextEditor::focusedOutlineColourId, th().accent);
        search.setColour (juce::TextEditor::textColourId, th().soft);
        search.onTextChange = [this] { refresh(); };
        addAndMakeVisible (search);

        refresh();
        startTimerHz (5);
    }

    BrowserView::~BrowserView() { stopTimer(); list.setModel (nullptr); }

    void BrowserView::visibilityChanged()
    {
        if (isVisible()) { processor.presetManager->rescanUser(); refresh(); }
    }

    void BrowserView::refresh()
    {
        auto& pm = *processor.presetManager;
        shown.clear();
        const auto text = search.getText().trim().toLowerCase();
        for (int i = 0; i < (int) pm.all().size(); ++i)
        {
            const auto& pr = pm.all()[(size_t) i];
            if (category >= 1 && category <= 6 && pr.category != presets::categories()[category - 1]) continue;
            if (category == 7 && ! pm.isFavourite (i)) continue;
            if (category == 8 && pr.factory) continue;
            if (model >= 0 && pr.model != model) continue;
            if (text.isNotEmpty() && ! pr.name.toLowerCase().contains (text) && ! pr.category.toLowerCase().contains (text)) continue;
            shown.push_back (i);
        }
        list.updateContent();
        list.repaint();
        repaint();
    }

    void BrowserView::timerCallback()
    {
        const int cur = processor.presetManager->getCurrent();
        if (cur != lastCurrent) { lastCurrent = cur; list.repaint(); repaint(); }
    }

    juce::Rectangle<int> BrowserView::categoryRow (int i) const { return { 40, 70 + i * 34 + (i >= 7 ? 14 : 0), 210, 30 }; }
    juce::Rectangle<int> BrowserView::modelRow (int i) const    { return { 40, 420 + i * 30, 210, 26 }; }
    juce::Rectangle<int> BrowserView::button (int i) const      { return { 1036, 330 + i * 46, 204, 36 }; }

    void BrowserView::resized()
    {
        search.setBounds (280, 30, 520, 32);
        list.setBounds (280, 72, 730, 580);
    }

    void BrowserView::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff0b0c0d));
        drawWell (g, { 24.0f, 20.0f, 242.0f, 640.0f }, 14.0f);
        drawWell (g, { 1022.0f, 20.0f, 234.0f, 640.0f }, 14.0f);

        g.setFont (spaced (9.0f, true, 0.28f));
        g.setColour (engrave);
        g.drawText ("TYPE", 40, 34, 200, 24, juce::Justification::centredLeft, false);
        g.drawText ("MODEL", 40, 388, 200, 24, juce::Justification::centredLeft, false);

        auto& pm = *processor.presetManager;
        for (int i = 0; i < 9; ++i)
        {
            auto r = categoryRow (i).toFloat();
            const bool on = i == category;
            if (on) { g.setColour (th().accent.withAlpha (0.16f)); g.fillRoundedRectangle (r, 6.0f); g.setColour (th().accent); g.fillRoundedRectangle (r.withWidth (3.0f), 1.5f); }
            g.setFont (LookAndFeel::uiFont (13.0f, on));
            g.setColour (on ? th().soft : juce::Colour (0xffc9ccd1));
            g.drawText (juce::String::fromUTF8 (catLabels[i]), r.reduced (12.0f, 0.0f), juce::Justification::centredLeft, false);
            int count = 0;
            for (int k = 0; k < (int) pm.all().size(); ++k)
            {
                const auto& pr = pm.all()[(size_t) k];
                if (i == 0 || (i <= 6 && pr.category == presets::categories()[i - 1]) || (i == 7 && pm.isFavourite (k)) || (i == 8 && ! pr.factory)) ++count;
            }
            g.setColour (juce::Colour (0xff6e737a));
            g.setFont (LookAndFeel::monoFont (11.0f));
            g.drawText (juce::String (count), r.reduced (10.0f, 0.0f), juce::Justification::centredRight, false);
        }
        for (int i = 0; i < 7; ++i)
        {
            auto r = modelRow (i).toFloat();
            const bool on = (i == 0 && model < 0) || i - 1 == model;
            if (on) { g.setColour (th().accent.withAlpha (0.16f)); g.fillRoundedRectangle (r, 6.0f); }
            g.setFont (LookAndFeel::uiFont (12.5f, on));
            g.setColour (on ? th().soft : juce::Colour (0xffc9ccd1));
            g.drawText (i == 0 ? juce::String ("All models") : Hamamatsu108Processor::modelNames()[i - 1], r.reduced (12.0f, 0.0f), juce::Justification::centredLeft, false);
        }

        g.setFont (LookAndFeel::uiFont (12.0f));
        g.setColour (juce::Colour (0xff6e737a));
        g.drawText (juce::String ((int) shown.size()) + " presets", 820, 30, 190, 32, juce::Justification::centredRight, false);

        // current preset card
        const int cur = pm.getCurrent();
        auto card = juce::Rectangle<float> (1038.0f, 40.0f, 202.0f, 270.0f);
        g.setFont (spaced (9.0f, true, 0.28f));
        g.setColour (engrave);
        g.drawText ("CURRENT", card.withHeight (20.0f), juce::Justification::centredLeft, false);
        auto disp = card.withTrimmedTop (28.0f).withHeight (66.0f);
        drawDisplay (g, disp, 8.0f);
        g.setFont (LookAndFeel::monoFont (15.0f));
        glowText (g, pm.currentName() + (pm.isModified() ? " *" : ""), disp.reduced (10.0f, 6.0f).withHeight (24.0f), juce::Justification::centredLeft);
        g.setFont (LookAndFeel::monoFont (11.0f));
        juce::String info = pm.currentCategory().toUpperCase();
        if (cur >= 0) info << juce::String::fromUTF8 ("  \xc2\xb7  ") << Hamamatsu108Processor::modelNames()[pm.all()[(size_t) cur].model].toUpperCase();
        glowText (g, info, disp.reduced (10.0f, 6.0f).withTrimmedTop (30.0f), juce::Justification::centredLeft, 0.65f);
        g.setFont (LookAndFeel::uiFont (12.0f));
        g.setColour (juce::Colour (0xff9aa0a7));
        g.drawFittedText ("Click a preset to load it. The heart marks a favourite. Your own presets are saved in Documents / Kuprell / Hamamatsu 108.",
                          card.withTrimmedTop (110.0f).toNearestInt(), juce::Justification::topLeft, 6);

        for (int i = 0; i < 5; ++i)
        {
            const bool enabled = i != 2 || (cur >= 0 && ! pm.all()[(size_t) cur].factory);
            drawHardButton (g, button (i).toFloat(), buttons[i], i == 4, enabled, false);
        }
    }

    void BrowserView::mouseDown (const juce::MouseEvent& e)
    {
        const auto p = e.getPosition();
        for (int i = 0; i < 9; ++i) if (categoryRow (i).contains (p)) { category = i; refresh(); return; }
        for (int i = 0; i < 7; ++i) if (modelRow (i).contains (p)) { model = i - 1; refresh(); return; }
        auto& pm = *processor.presetManager;
        if (button (0).contains (p)) saveCurrent();
        if (button (1).contains (p)) saveAs();
        if (button (2).contains (p))
        {
            const int cur = pm.getCurrent();
            if (cur < 0 || pm.all()[(size_t) cur].factory) return;
            juce::Component::SafePointer<BrowserView> safe (this);
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete preset",
                                                "Delete \"" + pm.currentName() + "\"? This cannot be undone.", "Delete", "Cancel", this,
                                                juce::ModalCallbackFunction::create ([safe, cur] (int r)
                                                {
                                                    if (safe != nullptr && r == 1) { safe->processor.presetManager->deleteUser (cur); safe->refresh(); }
                                                }));
        }
        if (button (3).contains (p)) { auto f = presets::Manager::userFolder(); f.createDirectory(); f.startAsProcess(); }
        if (button (4).contains (p) && onClose) onClose();
    }

    void BrowserView::saveCurrent()
    {
        auto& pm = *processor.presetManager;
        const int cur = pm.getCurrent();
        if (cur >= 0 && ! pm.all()[(size_t) cur].factory)
        {
            pm.saveUser (pm.currentName(), pm.currentCategory(), juce::roundToInt (plain (processor.apvts, "model")));
            if (onStatus) onStatus ("Saved", pm.currentName());
            refresh();
        }
        else
            saveAs();   // factory presets are read-only: save a copy
    }

    void BrowserView::saveAs()
    {
        auto& pm = *processor.presetManager;
        auto* w = new juce::AlertWindow ("Save preset", "Name and type of your preset:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", pm.currentName() == "Init" ? juce::String ("My Preset") : pm.currentName() + " 2", "Name");
        w->addComboBox ("cat", presets::categories(), "Type");
        if (auto* cb = w->getComboBoxComponent ("cat"))
            cb->setSelectedItemIndex (juce::jmax (0, presets::categories().indexOf (pm.currentCategory())));
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<BrowserView> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
        {
            if (safe == nullptr || r != 1) return;
            const auto name = w->getTextEditorContents ("name").trim();
            const auto cat = w->getComboBoxComponent ("cat")->getText();
            if (name.isEmpty()) return;
            auto& m = *safe->processor.presetManager;
            const bool ok = m.saveUser (name, cat, juce::roundToInt (plain (safe->processor.apvts, "model")));
            if (safe->onStatus) safe->onStatus (ok ? "Saved" : "Could not save", name);
            safe->refresh();
        }), true);
    }

    int BrowserView::getNumRows() { return (int) shown.size(); }

    void BrowserView::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool)
    {
        if (row < 0 || row >= (int) shown.size()) return;
        auto& pm = *processor.presetManager;
        const int idx = shown[(size_t) row];
        const auto& pr = pm.all()[(size_t) idx];
        const bool cur = idx == pm.getCurrent();
        auto r = juce::Rectangle<int> (0, 0, width, height).toFloat();
        if (cur) { g.setColour (th().accent.withAlpha (0.18f)); g.fillRect (r); g.setColour (th().accent); g.fillRect (r.withWidth (3.0f)); }
        else if (row % 2) { g.setColour (juce::Colours::white.withAlpha (0.02f)); g.fillRect (r); }

        const bool fav = pm.isFavourite (idx);
        g.setColour (fav ? th().accent : juce::Colour (0xff3a3e44));
        g.setFont (LookAndFeel::uiFont (15.0f));
        g.drawText (juce::String::fromUTF8 (fav ? "\xe2\x99\xa5" : "\xe2\x99\xa1"), 10, 0, 24, height, juce::Justification::centred, false);
        g.setFont (LookAndFeel::uiFont (14.0f, cur));
        g.setColour (cur ? th().soft : juce::Colour (0xffe6e7e9));
        g.drawText (pr.name, 44, 0, 330, height, juce::Justification::centredLeft, false);

        auto tag = juce::Rectangle<float> (390.0f, 7.0f, 70.0f, (float) height - 14.0f);
        g.setColour (juce::Colour (0xff1e2124));
        g.fillRoundedRectangle (tag, 3.0f);
        g.setFont (spaced (9.0f, true, 0.12f));
        g.setColour (th().soft);
        g.drawText (pr.category.toUpperCase(), tag, juce::Justification::centred, false);
        g.setFont (LookAndFeel::uiFont (12.0f));
        g.setColour (juce::Colour (0xff9aa0a7));
        g.drawText (Hamamatsu108Processor::modelNames()[pr.model], 480, 0, 140, height, juce::Justification::centredLeft, false);
        g.drawText (pr.factory ? "Factory" : "User", width - 90, 0, 80, height, juce::Justification::centredRight, false);
    }

    void BrowserView::listBoxItemClicked (int row, const juce::MouseEvent& e)
    {
        if (row < 0 || row >= (int) shown.size()) return;
        const int idx = shown[(size_t) row];
        auto& pm = *processor.presetManager;
        if (e.x < 40) { pm.toggleFavourite (idx); if (category == 7) refresh(); list.repaint(); return; }
        pm.load (idx);
        if (onStatus) onStatus ("Preset", pm.currentName());
        list.repaint();
        repaint();
    }

    void BrowserView::listBoxItemDoubleClicked (int row, const juce::MouseEvent& e)
    {
        listBoxItemClicked (row, e);
        if (onClose) onClose();
    }
}
