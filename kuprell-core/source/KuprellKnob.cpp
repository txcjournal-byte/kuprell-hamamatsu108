#include "KuprellKnob.h"
#include "KuprellLookAndFeel.h"

namespace kuprell
{
    namespace
    {
        constexpr float startAngle = -0.75f * juce::MathConstants<float>::pi;
        constexpr float endAngle   =  0.75f * juce::MathConstants<float>::pi;
    }

    class Knob::Dial : public juce::Slider
    {
    public:
        explicit Dial (Knob& o) : owner (o)
        {
            setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            setRotaryParameters (startAngle + juce::MathConstants<float>::twoPi, endAngle + juce::MathConstants<float>::twoPi, true);
            setMouseDragSensitivity (220);
            setVelocityModeParameters (0.6, 1, 0.0, true, juce::ModifierKeys::ctrlModifier);
            setScrollWheelEnabled (true);
        }

        void paint (juce::Graphics& g) override
        {
            const auto& st = owner.style;
            auto box = getLocalBounds().toFloat();
            const float size = juce::jmin (box.getWidth(), box.getHeight());
            auto area = box.withSizeKeepingCentre (size, size);
            const auto c = area.getCentre();
            const bool enabled = isEnabled();
            const float prop = enabled ? (float) valueToProportionOfLength (getValue()) : 0.0f;
            const float angle = startAngle + prop * (endAngle - startAngle);

            // value ring
            const float ringR = size * 0.5f - 2.0f;
            juce::Path track;
            track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
            g.setColour (st.track);
            g.strokePath (track, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (enabled && prop > 0.001f)
            {
                juce::Path arc;
                arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, angle, true);
                g.setColour (st.ring.withAlpha (0.35f));
                g.strokePath (arc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (st.ring);
                g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // body with drop shadow
            auto body = area.reduced (size * 0.16f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillEllipse (body.translated (0.0f, size * 0.07f).expanded (1.0f));

            g.setColour (juce::Colour (0xff1e2124));
            g.fillEllipse (body);
            // knurling
            const int teeth = 36;
            const float rOut = body.getWidth() * 0.5f, rIn = rOut * 0.78f;
            juce::Path knurl;
            for (int i = 0; i < teeth; i += 2)
            {
                const float a0 = (float) i / teeth * juce::MathConstants<float>::twoPi;
                const float a1 = (float) (i + 1) / teeth * juce::MathConstants<float>::twoPi;
                knurl.startNewSubPath (c.x + rIn * std::sin (a0), c.y - rIn * std::cos (a0));
                knurl.lineTo (c.x + rOut * std::sin (a0), c.y - rOut * std::cos (a0));
                knurl.lineTo (c.x + rOut * std::sin (a1), c.y - rOut * std::cos (a1));
                knurl.lineTo (c.x + rIn * std::sin (a1), c.y - rIn * std::cos (a1));
                knurl.closeSubPath();
            }
            g.setColour (juce::Colour (0xff3a3e43));
            g.fillPath (knurl);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (body, 1.0f);

            // chrome cap
            auto cap = body.reduced (body.getWidth() * 0.19f);
            juce::ColourGradient chrome (juce::Colour (0xfff6f8fa), cap.getX(), cap.getY(),
                                         juce::Colour (0xff7a8189), cap.getRight(), cap.getBottom(), false);
            chrome.addColour (0.35, juce::Colour (0xffaeb5bc));
            chrome.addColour (0.55, juce::Colour (0xffeef1f4));
            chrome.addColour (0.8, juce::Colour (0xff8e959d));
            g.setGradientFill (chrome);
            g.fillEllipse (cap);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.drawEllipse (cap.reduced (0.5f), 0.8f);
            if (! enabled)
            {
                g.setColour (juce::Colour (0x8c1a1c1f));
                g.fillEllipse (body);
            }

            // pointer
            const float pr0 = rOut * 0.42f, pr1 = rOut * 0.93f;
            juce::Line<float> pointer (c.x + pr0 * std::sin (angle), c.y - pr0 * std::cos (angle),
                                       c.x + pr1 * std::sin (angle), c.y - pr1 * std::cos (angle));
            const auto pc = enabled ? st.pointer : juce::Colour (0xff5a5e64);
            if (enabled)
            {
                g.setColour (pc.withAlpha (0.35f));
                g.drawLine (pointer, size > 50 ? 5.0f : 4.0f);
            }
            g.setColour (pc);
            g.drawLine (pointer, size > 50 ? 2.6f : 2.0f);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                owner.showMenu();
                return;
            }
            juce::Slider::mouseDown (e);
        }

        void mouseEnter (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseEnter (e);
            if (owner.onInteract) owner.onInteract (owner);
        }

    private:
        Knob& owner;
    };

    //==========================================================================
    Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& labelText, int size, Style s)
        : parameter (state.getParameter (paramID)), label (labelText), knobSize (size), style (s)
    {
        dial = std::make_unique<Dial> (*this);
        addAndMakeVisible (*dial);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, *dial);
        if (parameter != nullptr)
        {
            dial->setDoubleClickReturnValue (true, (double) parameter->convertFrom0to1 (parameter->getDefaultValue()));
            dial->setTooltip (parameter->getName (64) + juce::String::fromUTF8 ("  \xc2\xb7  double-click: reset  \xc2\xb7  right-click: menu"));
        }
        dial->addListener (this);
    }

    Knob::Knob (const juce::String& labelText, int size, Style s, const juce::String& comingSoonText)
        : label (labelText), comingSoon (comingSoonText), knobSize (size), style (s)
    {
        dial = std::make_unique<Dial> (*this);
        dial->setRange (0.0, 1.0);
        dial->setEnabled (false);
        dial->setTooltip (comingSoonText);
        addAndMakeVisible (*dial);
    }

    Knob::~Knob()
    {
        dial->removeListener (this);
        attachment.reset();
    }

    juce::String Knob::getDisplayName() const
    {
        return parameter != nullptr ? parameter->getName (64) : label;
    }

    juce::String Knob::getValueText() const
    {
        if (parameter == nullptr) return comingSoon;
        return parameter->getCurrentValueAsText();
    }

    void Knob::sliderValueChanged (juce::Slider*)
    {
        repaint();
        if (onInteract) onInteract (*this);
    }

    void Knob::showMenu()
    {
        if (parameter == nullptr) return;

        juce::PopupMenu m;
        m.addSectionHeader (parameter->getName (64) + ":  " + getValueText());
        m.addItem (1, "Reset to default");
        m.addItem (2, "Type value...");
        juce::Component::SafePointer<Knob> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (dial.get()), [safe] (int r)
        {
            if (safe == nullptr || safe->parameter == nullptr) return;
            auto* p = safe->parameter;
            if (r == 1)
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getDefaultValue());
                p->endChangeGesture();
            }
            else if (r == 2)
            {
                auto* w = new juce::AlertWindow (p->getName (64), "Type a new value:", juce::MessageBoxIconType::NoIcon, safe.getComponent());
                w->addTextEditor ("v", safe->getValueText());
                w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
                {
                    if (result == 1 && safe != nullptr && safe->parameter != nullptr)
                    {
                        auto* q = safe->parameter;
                        const auto text = w->getTextEditorContents ("v");
                        q->beginChangeGesture();
                        q->setValueNotifyingHost (q->getValueForText (text.trim()));
                        q->endChangeGesture();
                    }
                }), true);
            }
        });
    }

    void Knob::resized()
    {
        const int box = knobSize + 14;
        dial->setBounds ((getWidth() - box) / 2, 0, box, box);
    }

    void Knob::paint (juce::Graphics& g)
    {
        const int box = knobSize + 14;
        auto r = getLocalBounds().withTrimmedTop (box + 1);
        const bool enabled = parameter != nullptr;

        g.setFont (LookAndFeel::uiFont (style.labelHeight, true));
        g.setColour (enabled ? style.label : style.label.withAlpha (0.45f));
        g.drawText (label, r.removeFromTop (12), juce::Justification::centredTop, false);

        if (style.showValue)
        {
            g.setFont (LookAndFeel::monoFont (10.5f));
            g.setColour (enabled ? style.value : style.value.withAlpha (0.4f));
            g.drawText (enabled ? getValueText() : juce::String::fromUTF8 ("\xe2\x80\x94"), r.removeFromTop (12), juce::Justification::centredTop, false);
        }
    }
}
