#include "BrowserView.h"
#include "ConsoleStyle.h"

using namespace kuprell;
using namespace hama::look;

namespace hama
{
    namespace
    {
        const char* const catLabels[] = { "All presets", "Harp", "Pluck", "Bell", "Keys", "Pad", "Texture", "Hybrid", "Cinematic", "\xe2\x99\xa5  Favourites", "My presets" };
        constexpr int NC = 8, FAV = 9, USER = 10, ROWS = 11;
        const char* const buttons[] = { "Save", "Save as", "Delete", "Show folder", "Back to play" };
    }

    BrowserView::BrowserView (Hamamatsu108Processor& p) : processor (p)
    {
        list.setModel (this);
        list.setRowHeight (28);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        list.setOutlineThickness (0);
        addAndMakeVisible (list);
        list.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, cs::mint.withAlpha (0.35f));

        search.setTextToShowWhenEmpty ("SEARCH PRESETS", cs::mint.withAlpha (0.35f));
        search.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        search.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        search.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        search.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        search.setColour (juce::TextEditor::textColourId, cs::mint);
        search.setColour (juce::CaretComponent::caretColourId, cs::mint);
        search.setColour (juce::TextEditor::highlightColourId, cs::mint.withAlpha (0.3f));
        search.setIndents (10, 7);
        search.onTextChange = [this] { refresh(); };
        search.onReturnKey = [this] { search.giveAwayKeyboardFocus(); };
        search.onEscapeKey = [this] { search.giveAwayKeyboardFocus(); };
        addAndMakeVisible (search);

        refresh();
        startTimerHz (5);
    }

    BrowserView::~BrowserView() { stopTimer(); list.setModel (nullptr); }

    void BrowserView::visibilityChanged()
    {
        if (isVisible()) { processor.presetManager->rescanUser(); refresh(); }
        else search.giveAwayKeyboardFocus();
    }

    void BrowserView::refresh()
    {
        auto& pm = *processor.presetManager;
        shown.clear();
        const auto text = search.getText().trim().toLowerCase();
        for (int i = 0; i < (int) pm.all().size(); ++i)
        {
            const auto& pr = pm.all()[(size_t) i];
            if (category >= 1 && category <= NC && pr.category != presets::categories()[category - 1]) continue;
            if (category == FAV && ! pm.isFavourite (i)) continue;
            if (category == USER && pr.factory) continue;
            if (category == USER && bankFilter.isNotEmpty() && pr.bank != bankFilter) continue;
            if (harp >= 0 && harpOf (i) != harp) continue;
            // presets are sorted by type only (character is a filter on the sound, not a preset group)
            if (text.isNotEmpty() && ! pr.name.toLowerCase().contains (text) && ! pr.category.toLowerCase().contains (text) && ! pr.bank.toLowerCase().contains (text)) continue;
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

    juce::Rectangle<int> BrowserView::categoryRow (int i) const { return { 38, 58 + i * 27 + (i >= FAV ? 10 : 0), 214, 25 }; }
    juce::Rectangle<int> BrowserView::harpRow (int i) const     { return { 38, 422 + i * 23, 214, 21 }; }
    juce::Rectangle<int> BrowserView::modelRow (int i) const    { return { 40, 420 + i * 30, 210, 26 }; }
    juce::Rectangle<int> BrowserView::button (int i) const      { return { 1038, 350 + i * 50, 202, 38 }; }

    int BrowserView::harpOf (int idx) const
    {
        const auto& pr = processor.presetManager->all()[(size_t) idx];
        for (auto& v : pr.values) if (v.first == "harpType") return juce::roundToInt (v.second);
        return pr.factory ? 0 : -1;   // factory presets without the value are concert harps
    }

    void BrowserView::resized()
    {
        search.setBounds (286, 32, 520, 30);
        list.setBounds (286, 82, 716, 564);
    }

    void BrowserView::paint (juce::Graphics& g)
    {
        // HARPOLOGY console: graphite plates, red LEDs, the presets on green glass
        cs::pageBackground (g, getLocalBounds().toFloat());
        cs::plate (g, { 24.0f, 20.0f, 242.0f, 640.0f }, "Type");
        cs::plate (g, { 272.0f, 20.0f, 744.0f, 640.0f }, {});
        cs::plate (g, { 1022.0f, 20.0f, 234.0f, 640.0f }, "Current");
        cs::lcd (g, { 286.0f, 32.0f, 520.0f, 30.0f });          // search
        cs::lcd (g, { 286.0f, 78.0f, 716.0f, 570.0f });         // the list

        auto& pm = *processor.presetManager;
        auto ledRow = [&g] (juce::Rectangle<float> r, const juce::String& label, bool on, int count)
        {
            cs::led (g, { r.getX() + 10.0f, r.getCentreY() }, on, 7.0f);
            g.setFont (juce::Font (juce::FontOptions (12.0f, on ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (0.04f));
            g.setColour (on ? cs::bone : cs::ink.withAlpha (0.8f));
            g.drawText (label, r.withTrimmedLeft (24.0f), juce::Justification::centredLeft, false);
            if (count >= 0)
            {
                g.setFont (kuprell::LookAndFeel::monoFont (10.5f));
                g.setColour (on ? cs::mint : cs::dim);
                g.drawText (juce::String (count), r.withTrimmedRight (6.0f), juce::Justification::centredRight, false);
            }
        };
        for (int i = 0; i < ROWS; ++i)
        {
            juce::String label (juce::String::fromUTF8 (catLabels[i]));
            if (i == FAV) label = "Favourites";
            if (i == USER) label << (bankFilter.isNotEmpty() ? juce::String::fromUTF8 (" \xc2\xb7 ") + bankFilter : juce::String::fromUTF8 ("  \xe2\x96\xbe"));
            int count = 0;
            for (int k = 0; k < (int) pm.all().size(); ++k)
            {
                const auto& pr = pm.all()[(size_t) k];
                if (i == 0 || (i <= NC && pr.category == presets::categories()[i - 1]) || (i == FAV && pm.isFavourite (k))
                    || (i == USER && ! pr.factory && (bankFilter.isEmpty() || pr.bank == bankFilter))) ++count;
            }
            ledRow (categoryRow (i).toFloat(), label, i == category, count);
        }
        // HARP: the presets of one instrument
        auto hs = juce::Rectangle<float> (32.0f, 394.0f, 226.0f, 18.0f);
        g.setColour (juce::Colour (0xff141617));
        g.fillRoundedRectangle (hs, 2.0f);
        g.setColour (cs::mint);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.16f));
        g.drawText ("HARP", hs, juce::Justification::centred, false);
        for (int i = 0; i <= Hamamatsu108Processor::numHarpTypes; ++i)
        {
            int count = 0;
            for (int k = 0; k < (int) pm.all().size(); ++k) if (i == 0 || harpOf (k) == i - 1) ++count;
            const juce::String name = i == 0 ? juce::String ("Every harp") : juce::String (Hamamatsu108Processor::harpTypeName (i - 1)).toLowerCase().replaceSection (0, 1, juce::String (Hamamatsu108Processor::harpTypeName (i - 1)).substring (0, 1));
            ledRow (harpRow (i).toFloat(), name, harp == i - 1, count);
        }

        g.setFont (kuprell::LookAndFeel::monoFont (11.0f));
        g.setColour (cs::mint.withAlpha (0.7f));
        g.drawText (juce::String ((int) shown.size()) + " PRESETS", 812, 32, 190, 30, juce::Justification::centredRight, false);

        // current preset
        const int cur = pm.getCurrent();
        auto disp = juce::Rectangle<float> (1036.0f, 52.0f, 206.0f, 82.0f);
        cs::lcd (g, disp);
        g.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));
        cs::lcdText (g, pm.currentName() + (pm.isModified() ? " *" : ""), disp.reduced (10.0f, 8.0f).withHeight (24.0f), juce::Justification::centredLeft);
        g.setFont (kuprell::LookAndFeel::monoFont (11.0f));
        cs::lcdText (g, pm.currentCategory().toUpperCase() + "   " + Hamamatsu108Processor::harpTypeName (processor.harpType()), disp.reduced (10.0f, 8.0f).withTrimmedTop (34.0f).withHeight (16.0f), juce::Justification::centredLeft, 0.7f);
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.setColour (cs::ink.withAlpha (0.75f));
        g.drawFittedText ("Click a preset to load it, the heart marks a favourite. Save as: your own name and bank. Right-click a preset: rename, move to a bank, delete. HARP shows the presets of one instrument.",
                          juce::Rectangle<int> (1038, 150, 202, 150), juce::Justification::topLeft, 9);

        for (int i = 0; i < 5; ++i)
        {
            const bool enabled = i != 2 || (cur >= 0 && ! pm.all()[(size_t) cur].factory);
            cs::key (g, button (i).toFloat(), buttons[i], i == 4, enabled, 11.0f);
        }
    }

    void BrowserView::mouseDown (const juce::MouseEvent& e)
    {
        const auto p = e.getPosition();
        for (int i = 0; i < ROWS; ++i)
            if (categoryRow (i).contains (p))
            {
                if (i == USER && (category == USER || e.mods.isPopupMenu())) { category = USER; showBankMenu(); return; }
                category = i;
                if (i != USER) bankFilter.clear();
                refresh();
                return;
            }
        for (int i = 0; i <= Hamamatsu108Processor::numHarpTypes; ++i)
            if (harpRow (i).contains (p)) { harp = i - 1; refresh(); return; }
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
            pm.saveUser (pm.currentName(), pm.currentCategory(), juce::roundToInt (plain (processor.apvts, "model")), pm.currentBank());
            if (onStatus) onStatus ("Saved", pm.currentName());
            refresh();
        }
        else
            saveAs();   // factory presets are read-only: save a copy
    }

    void BrowserView::saveAs()
    {
        auto& pm = *processor.presetManager;
        auto* w = new juce::AlertWindow ("Save preset", "Name your sound, pick its type and put it in your own bank (your category, e.g. \"Lo-fi beats\").",
                                         juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", pm.currentName() == "Init" ? juce::String ("My Preset") : pm.currentName() + " 2", "Name");
        w->addComboBox ("cat", presets::categories(), "Type");
        if (auto* cb = w->getComboBoxComponent ("cat"))
            cb->setSelectedItemIndex (juce::jmax (0, presets::categories().indexOf (pm.currentCategory())));
        const auto banks = pm.userBanks();
        juce::StringArray bankChoices { "(new bank - type it below)" };
        bankChoices.addArray (banks);
        w->addComboBox ("bankPick", bankChoices, "Bank");
        const auto startBank = pm.currentBank().isNotEmpty() ? pm.currentBank() : lastBank;
        if (auto* cb = w->getComboBoxComponent ("bankPick"))
            cb->setSelectedItemIndex (juce::jmax (0, bankChoices.indexOf (startBank)));
        w->addTextEditor ("bank", banks.contains (startBank) ? juce::String() : startBank, "New bank name (empty = no bank)");
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<BrowserView> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
        {
            if (safe == nullptr || r != 1) return;
            const auto name = w->getTextEditorContents ("name").trim();
            const auto cat = w->getComboBoxComponent ("cat")->getText();
            auto bank = w->getTextEditorContents ("bank").trim();
            if (bank.isEmpty() && w->getComboBoxComponent ("bankPick")->getSelectedItemIndex() > 0)
                bank = w->getComboBoxComponent ("bankPick")->getText();
            if (name.isEmpty()) return;
            auto& m = *safe->processor.presetManager;
            const bool ok = m.saveUser (name, cat, juce::roundToInt (plain (safe->processor.apvts, "model")), bank);
            safe->lastBank = bank;
            if (safe->onStatus) safe->onStatus (ok ? "Saved" : "Could not save", bank.isNotEmpty() ? bank + " / " + name : name);
            safe->category = USER;
            safe->bankFilter = bank;
            safe->refresh();
        }), true);
    }

    void BrowserView::showBankMenu()
    {
        auto& pm = *processor.presetManager;
        const auto banks = pm.userBanks();
        juce::PopupMenu m;
        m.addSectionHeader ("My presets: show the bank...");
        m.addItem (1, "All my presets", true, bankFilter.isEmpty());
        for (int i = 0; i < banks.size(); ++i) m.addItem (10 + i, banks[i], true, bankFilter == banks[i]);
        juce::Component::SafePointer<BrowserView> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (categoryRow (8))), [safe, banks] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            safe->bankFilter = r == 1 ? juce::String() : banks[r - 10];
            safe->refresh();
        });
    }

    void BrowserView::askName (const juce::String& title, const juce::String& name, const juce::String& bank, std::function<void (juce::String, juce::String)> done)
    {
        auto* w = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", name, "Name");
        w->addTextEditor ("bank", bank, "Bank (empty = no bank)");
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<BrowserView> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, done] (int r)
        {
            if (safe == nullptr || r != 1) return;
            const auto n = w->getTextEditorContents ("name").trim();
            if (n.isNotEmpty()) done (n, w->getTextEditorContents ("bank").trim());
        }), true);
    }

    void BrowserView::showRowMenu (int idx)
    {
        auto& pm = *processor.presetManager;
        const auto& pr = pm.all()[(size_t) idx];
        juce::PopupMenu m;
        m.addSectionHeader (pr.name);
        if (pr.factory)
            m.addItem (1, "Save a copy in my presets...");
        else
        {
            m.addItem (2, "Rename / move to a bank...");
            juce::PopupMenu mv;
            const auto banks = pm.userBanks();
            mv.addItem (100, "(no bank)", true, pr.bank.isEmpty());
            for (int i = 0; i < banks.size(); ++i) mv.addItem (101 + i, banks[i], true, pr.bank == banks[i]);
            m.addSubMenu ("Move to bank", mv);
            m.addItem (3, "Delete");
        }
        m.addItem (4, pm.isFavourite (idx) ? "Remove from favourites" : "Add to favourites");
        juce::Component::SafePointer<BrowserView> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, idx] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            auto& pm2 = *safe->processor.presetManager;
            const auto pr2 = pm2.all()[(size_t) idx];
            if (r == 1) { pm2.load (idx); safe->saveAs(); }
            if (r == 2) safe->askName ("Rename preset", pr2.name, pr2.bank, [safe, idx] (juce::String n, juce::String b)
                                       { if (safe != nullptr) { safe->processor.presetManager->renameUser (idx, n, b); safe->refresh(); } });
            if (r == 3)
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete preset", "Delete \"" + pr2.name + "\"? This cannot be undone.", "Delete", "Cancel", safe.getComponent(),
                                                    juce::ModalCallbackFunction::create ([safe, idx] (int a) { if (safe != nullptr && a == 1) { safe->processor.presetManager->deleteUser (idx); safe->refresh(); } }));
            if (r == 4) pm2.toggleFavourite (idx);
            if (r >= 100)
            {
                const auto banks = pm2.userBanks();
                pm2.renameUser (idx, pr2.name, r == 100 ? juce::String() : banks[r - 101]);
            }
            safe->refresh();
        });
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
        if (cur)
        {
            g.setColour (cs::mint.withAlpha (0.16f)); g.fillRect (r.reduced (2.0f, 1.0f));
            g.setColour (cs::mint); g.fillRect (r.withWidth (3.0f).reduced (0.0f, 3.0f));
        }
        g.setColour (cs::mint.withAlpha (0.06f));
        g.fillRect (r.withTop (r.getBottom() - 1.0f).reduced (8.0f, 0.0f));

        const bool fav = pm.isFavourite (idx);
        g.setColour (fav ? cs::ledRed : cs::mint.withAlpha (0.25f));
        g.setFont (LookAndFeel::uiFont (14.0f));
        g.drawText (juce::String::fromUTF8 (fav ? "\xe2\x99\xa5" : "\xe2\x99\xa1"), 10, 0, 24, height, juce::Justification::centred, false);
        g.setFont (juce::Font (juce::FontOptions (14.0f, cur ? juce::Font::bold : juce::Font::plain)));
        cs::lcdText (g, pr.name, juce::Rectangle<float> (44.0f, 0.0f, 300.0f, (float) height), juce::Justification::centredLeft, cur ? 1.0f : 0.82f);

        auto tag = [&g, height] (float x, float w, const juce::String& t)
        {
            auto rr = juce::Rectangle<float> (x, 6.0f, w, (float) height - 12.0f);
            g.setColour (cs::mint.withAlpha (0.35f));
            g.drawRoundedRectangle (rr, 2.0f, 1.0f);
            g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)).withExtraKerningFactor (0.1f));
            g.setColour (cs::mint.withAlpha (0.8f));
            g.drawText (t, rr, juce::Justification::centred, false);
        };
        tag (360.0f, 78.0f, pr.category.toUpperCase());
        if (const int h = harpOf (idx); h >= 0) tag (446.0f, 78.0f, Hamamatsu108Processor::harpTypeName (h));
        g.setFont (kuprell::LookAndFeel::monoFont (10.5f));
        g.setColour (cs::mint.withAlpha (0.45f));
        g.drawText (pr.factory ? juce::String ("FACTORY") : (pr.bank.isNotEmpty() ? pr.bank.toUpperCase() : juce::String ("USER")), width - 150, 0, 136, height, juce::Justification::centredRight, false);
    }

    void BrowserView::listBoxItemClicked (int row, const juce::MouseEvent& e)
    {
        if (row < 0 || row >= (int) shown.size()) return;
        const int idx = shown[(size_t) row];
        auto& pm = *processor.presetManager;
        if (e.mods.isPopupMenu()) { showRowMenu (idx); return; }
        if (e.x < 40) { pm.toggleFavourite (idx); if (category == FAV) refresh(); list.repaint(); return; }
        pm.load (idx);
        processor.previewPreset();   // let it be heard right away
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
