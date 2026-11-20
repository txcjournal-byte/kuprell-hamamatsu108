#pragma once

#include "Look.h"
#include "PluginProcessor.h"

// Small parameter controls for the EDIT pages: a display-style dropdown and a lit push button.
namespace hama
{
    // Dropdown for a choice / int / bool parameter, drawn as a small display. Label above it.
    class ChoiceBox : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ChoiceBox (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& labelText)
            : state (s), param (s.getParameter (paramID)), label (labelText)
        {
            jassert (param != nullptr);
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); if (onChange) onChange(); });
            attachment->sendInitialUpdate();
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            setTooltip (param->getName (64));
        }

        std::function<void()> onChange;
        std::function<void (const juce::String&, const juce::String&)> onInteract;

        int getIndex() const { return juce::roundToInt (param->convertFrom0to1 (param->getValue())); }

        void paint (juce::Graphics& g) override
        {
            using namespace look;
            auto r = getLocalBounds().toFloat();
            if (label.isNotEmpty())
            {
                if (th().concrete) { g.setFont (LookAndFeel::uiFont (10.0f)); g.setColour (juce::Colour (0xffb3b0a9)); }
                else { g.setFont (spaced (8.5f, true, 0.18f)); g.setColour (juce::Colour (0xff9aa0a7)); }
                g.drawText (th().concrete ? quiet (label) : label, r.removeFromTop (14.0f), juce::Justification::centredLeft, false);
            }
            drawDisplay (g, r, 5.0f);
            if (hover) { g.setColour (th().concrete ? page::phosphorDim() : th().accent.withAlpha (0.6f)); if (th().concrete) g.drawRect (r, 1.0f); else g.drawRoundedRectangle (r, 5.0f, 1.0f); }
            g.setFont (kuprell::LookAndFeel::monoFont (12.0f));
            glowText (g, param->getCurrentValueAsText(), r.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
            g.setColour (juce::Colour (0xff7e8187));
            g.drawText (juce::String::fromUTF8 ("\xe2\x96\xbe"), r.reduced (8.0f, 0.0f), juce::Justification::centredRight, false);
        }

        void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); if (onInteract) onInteract (param->getName (64), param->getCurrentValueAsText()); }
        void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

        void mouseDown (const juce::MouseEvent&) override
        {
            juce::PopupMenu m;
            const int n = param->getNumSteps();
            const int current = getIndex();
            const auto& range = param->getNormalisableRange();
            const int lo = juce::roundToInt (range.start), hi = juce::roundToInt (range.end);
            juce::ignoreUnused (n);
            for (int v = lo; v <= hi; ++v)
                m.addItem (v - lo + 1, param->getText (param->convertTo0to1 ((float) v), 64), true, v == current);
            juce::Component::SafePointer<ChoiceBox> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()), [safe, lo] (int r)
            {
                if (safe == nullptr || r <= 0) return;
                safe->attachment->setValueAsCompleteGesture ((float) (r - 1 + lo));
                if (safe->onInteract) safe->onInteract (safe->param->getName (64), safe->param->getCurrentValueAsText());
            });
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            const auto& range = param->getNormalisableRange();
            const int v = juce::jlimit (juce::roundToInt (range.start), juce::roundToInt (range.end), getIndex() + (w.deltaY < 0 ? 1 : -1));
            attachment->setValueAsCompleteGesture ((float) v);
        }

    private:
        juce::AudioProcessorValueTreeState& state;
        juce::RangedAudioParameter* param;
        juce::String label;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        bool hover = false;
    };

    // Lit push button for a bool parameter.
    class ToggleKey : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ToggleKey (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& text)
            : param (s.getParameter (paramID)), label (text)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { on = std::abs (v - onValue) < 0.5f; repaint(); });
            attachment->sendInitialUpdate();
            setTooltip (param->getName (64));
        }
        void paint (juce::Graphics& g) override { look::drawHardButton (g, getLocalBounds().toFloat(), label, on, true, false); }
        void mouseDown (const juce::MouseEvent&) override { attachment->setValueAsCompleteGesture (on ? offValue : onValue); }
        // for a choice parameter: which values mean on / off (ARP: strum mode 1 = on, 3 = off)
        void setValues (float onV, float offV) { onValue = onV; offValue = offV; attachment->sendInitialUpdate(); }

    private:
        juce::RangedAudioParameter* param;
        juce::String label;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        bool on = false;
        float onValue = 1.0f, offValue = 0.0f;
    };
}
