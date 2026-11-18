#include "KuprellTopBar.h"
#include "KuprellLookAndFeel.h"
#include "KuprellBrandAssets.h"

namespace kuprell
{
    namespace
    {
        const juce::PathStrokeType iconStroke (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        void chevron (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool left)
        {
            const auto cx = r.getCentreX(), cy = r.getCentreY();
            const float dx = left ? 2.5f : -2.5f;
            juce::Path p;
            p.startNewSubPath (cx + dx, cy - 5.0f);
            p.lineTo (cx - dx, cy);
            p.lineTo (cx + dx, cy + 5.0f);
            g.setColour (c);
            g.strokePath (p, iconStroke);
        }

        void undoArrow (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool redo)
        {
            const auto cx = r.getCentreX(), cy = r.getCentreY() + 1.0f;
            const float s = redo ? -1.0f : 1.0f;
            juce::Path p;
            p.startNewSubPath (cx - 5.0f * s, cy - 3.0f);
            p.lineTo (cx + 2.0f * s, cy - 3.0f);
            p.quadraticTo (cx + 6.0f * s, cy - 3.0f, cx + 6.0f * s, cy + 1.0f);
            p.quadraticTo (cx + 6.0f * s, cy + 5.0f, cx + 2.0f * s, cy + 5.0f);
            p.lineTo (cx - 1.0f * s, cy + 5.0f);
            p.startNewSubPath (cx - 2.0f * s, cy - 6.0f);
            p.lineTo (cx - 5.0f * s, cy - 3.0f);
            p.lineTo (cx - 2.0f * s, cy);
            g.setColour (c);
            g.strokePath (p, iconStroke);
        }

        void dice (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            auto box = r.withSizeKeepingCentre (14.0f, 14.0f);
            g.setColour (c);
            g.drawRoundedRectangle (box, 3.0f, 1.8f);
            for (auto pt : { box.getTopLeft().translated (4.0f, 4.0f), box.getCentre(), box.getBottomRight().translated (-4.0f, -4.0f) })
                g.fillEllipse (juce::Rectangle<float> (2.6f, 2.6f).withCentre (pt));
        }

        void gear (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            const auto centre = r.getCentre();
            g.setColour (c);
            g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (centre), 1.8f);
            juce::Path spokes;
            for (int i = 0; i < 8; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
                spokes.startNewSubPath (centre.getPointOnCircumference (5.5f, a));
                spokes.lineTo (centre.getPointOnCircumference (7.5f, a));
            }
            g.strokePath (spokes, iconStroke);
        }

        void question (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            g.setColour (c);
            g.setFont (LookAndFeel::uiFont (15.0f, true));
            g.drawText ("?", r, juce::Justification::centred);
        }
    }

    //==============================================================================
    IconButton::IconButton (const juce::String& name, Painter painterToUse)
        : juce::Button (name), painter (std::move (painterToUse))
    {
        setTooltip (name);
    }

    void IconButton::paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
    {
        if (auto* lnf = dynamic_cast<juce::LookAndFeel_V4*> (&getLookAndFeel()))
            lnf->drawButtonBackground (g, *this, colours::control, isMouseOverButton, isButtonDown);

        const auto colour = isMouseOverButton ? colours::accentSoft : colours::textDim;
        if (painter)
            painter (g, getLocalBounds().toFloat(), colour);
    }

    //==============================================================================
    TopBar::TopBar (const juce::String& name)
        : pluginName (name),
          prevButton     ("Previous preset", [] (auto& g, auto r, auto c) { chevron (g, r, c, true); }),
          nextButton     ("Next preset",     [] (auto& g, auto r, auto c) { chevron (g, r, c, false); }),
          randomButton   ("Randomize",       [] (auto& g, auto r, auto c) { dice (g, r, c); }),
          undoButton     ("Undo",            [] (auto& g, auto r, auto c) { undoArrow (g, r, c, false); }),
          redoButton     ("Redo",            [] (auto& g, auto r, auto c) { undoArrow (g, r, c, true); }),
          helpButton     ("Help mode",       [] (auto& g, auto r, auto c) { question (g, r, c); }),
          settingsButton ("Settings",        [] (auto& g, auto r, auto c) { gear (g, r, c); })
    {
        logo = juce::Drawable::createFromImageData (KuprellBrand::kuprelllogocream_svg,
                                                    KuprellBrand::kuprelllogocream_svgSize);

        auto wire = [] (juce::Button& b, std::function<void()>& target)
        {
            b.onClick = [&target] { if (target) target(); };
        };

        for (auto* b : std::initializer_list<juce::Button*> { &prevButton, &nextButton, &saveButton, &compareButton, &randomButton,
                                                               &undoButton, &redoButton, &helpButton, &settingsButton, &editButton })
            addAndMakeVisible (b);

        wire (prevButton, onPrevPreset);
        wire (nextButton, onNextPreset);
        wire (saveButton, onSave);
        wire (compareButton, onCompare);
        wire (randomButton, onRandomize);
        wire (undoButton, onUndo);
        wire (redoButton, onRedo);
        wire (helpButton, onHelp);
        wire (settingsButton, onSettings);
        wire (editButton, onEdit);

        saveButton.setTooltip ("Save preset");
        compareButton.setTooltip ("Compare two versions of the sound (A / B)");
        editButton.setTooltip ("Open deep editing");
        editButton.setComponentID ("accent");
        editButton.setColour (juce::TextButton::textColourOffId, colours::onAccent);
    }

    void TopBar::setPreset (const juce::String& category, const juce::String& name, bool modified)
    {
        const auto cat = category.toUpperCase();
        if (cat == presetCategory && name == presetName && modified == presetModified) return;   // nothing changed
        presetCategory = cat;
        presetName = name;
        presetModified = modified;
        repaint (presetArea);
    }

    void TopBar::setEditActive (bool shouldBeActive)
    {
        editButton.setToggleState (shouldBeActive, juce::dontSendNotification);
        editButton.repaint();
    }

    void TopBar::resized()
    {
        constexpr int button = 32, gap = 6;
        const int margin = embedded ? 0 : 16;
        const int y = (getHeight() - button) / 2;

        // left: logo 12 px high (about 52 px wide at 100 %), then divider and plugin name
        logoArea = embedded ? juce::Rectangle<int>() : juce::Rectangle<int> { margin, (height - 12) / 2, 52, 12 };
        nameArea = embedded ? juce::Rectangle<int>() : juce::Rectangle<int> { logoArea.getRight() + 24, 0, 160, height };

        // right: undo, redo, help, settings, EDIT
        int x = getWidth() - margin;
        auto placeRight = [&] (juce::Component& c, int w) { x -= w; c.setBounds (x, y, w, button); x -= gap; };
        placeRight (editButton, 76);
        placeRight (settingsButton, button);
        placeRight (helpButton, button);
        placeRight (redoButton, button);
        placeRight (undoButton, button);

        // centre: < preset > Save A|B dice
        const int presetWidth = embedded ? 300 : 340;
        const int groupWidth = button + gap + presetWidth + gap + button + gap + 56 + gap + 60 + gap + button;
        int cx = embedded ? 0 : (getWidth() - groupWidth) / 2;
        auto placeLeft = [&] (juce::Component& c, int w) { c.setBounds (cx, y, w, button); cx += w + gap; };
        placeLeft (prevButton, button);
        presetArea = { cx, y, presetWidth, button };
        cx += presetWidth + gap;
        placeLeft (nextButton, button);
        placeLeft (saveButton, 56);
        placeLeft (compareButton, 60);
        placeLeft (randomButton, button);
    }

    void TopBar::paint (juce::Graphics& g)
    {
        using namespace colours;
        auto r = getLocalBounds().toFloat();
        if (embedded) { paintPresetDisplay (g); return; }

        g.setGradientFill (juce::ColourGradient (barTop, 0.0f, 0.0f, barBottom, 0.0f, r.getBottom(), false));
        g.fillRect (r);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (0, 0.0f, r.getRight());
        g.setColour (juce::Colours::black);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, r.getRight());

        // logo: approved SVG, drawn as vector, never set in a font and never altered
        if (logo != nullptr)
            logo->drawWithin (g, logoArea.toFloat(),
                              juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid, 1.0f);

        // divider + plugin name (plain UI text)
        const float dividerX = (float) logoArea.getRight() + 12.0f;
        g.setColour (juce::Colour (0xff4a4e54));
        g.fillRect (juce::Rectangle<float> (dividerX, (float) height * 0.5f - 8.0f, 1.0f, 16.0f));
        g.setColour (textSoft);
        g.setFont (LookAndFeel::uiFont (13.0f, true));
        g.drawText (pluginName, nameArea, juce::Justification::centredLeft);

        paintPresetDisplay (g);
    }

    void TopBar::paintPresetDisplay (juce::Graphics& g)
    {
        using namespace colours;
        auto r = presetArea.toFloat();

        g.setColour (display);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);

        auto inner = r.reduced (12.0f, 0.0f);

        // category tag
        g.setFont (LookAndFeel::uiFont (10.0f, true));
        const float tagWidth = g.getCurrentFont().getStringWidthFloat (presetCategory) + 12.0f;
        auto tag = inner.removeFromLeft (tagWidth).withSizeKeepingCentre (tagWidth, 16.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (tag, 3.0f);
        g.setColour (onAccent);
        g.drawText (presetCategory, tag, juce::Justification::centred);

        inner.removeFromLeft (10.0f);

        // dropdown chevron
        auto arrow = inner.removeFromRight (12.0f);
        juce::Path p;
        p.startNewSubPath (arrow.getCentreX() - 4.0f, arrow.getCentreY() - 2.0f);
        p.lineTo (arrow.getCentreX(), arrow.getCentreY() + 2.0f);
        p.lineTo (arrow.getCentreX() + 4.0f, arrow.getCentreY() - 2.0f);
        g.setColour (textDim);
        g.strokePath (p, iconStroke);

        // preset name, glowing like the hardware display
        g.setFont (LookAndFeel::monoFont (15.0f));
        const auto label = presetName + (presetModified ? " *" : "");
        g.setColour (accent.withAlpha (0.25f));
        g.drawText (label, inner.translated (0.0f, 1.0f), juce::Justification::centredLeft);
        g.setColour (accentSoft);
        g.drawText (label, inner, juce::Justification::centredLeft);
    }

    void TopBar::mouseUp (const juce::MouseEvent& e)
    {
        const auto pos = e.getPosition();

        if (presetArea.contains (pos))
        {
            if (onPresetClicked) onPresetClicked();
        }
        else if (logoArea.getUnion (nameArea).contains (pos))
        {
            if (onLogoClicked) onLogoClicked();
        }
    }
}
