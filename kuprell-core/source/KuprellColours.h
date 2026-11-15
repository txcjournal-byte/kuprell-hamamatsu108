#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Kuprell frame palette (shared by all plugins). Plugin-specific colours live in each plugin.
namespace kuprell::colours
{
    // brand (see claude/kuprell-logo.md)
    inline const juce::Colour ink       { 0xff0d0d0e };
    inline const juce::Colour cream     { 0xfff2efea };

    // frame
    inline const juce::Colour window    { 0xff0b0c0d };
    inline const juce::Colour barTop    { 0xff202225 };
    inline const juce::Colour barBottom { 0xff141517 };
    inline const juce::Colour control   { 0xff2b2d30 };
    inline const juce::Colour controlLo { 0xff1e2023 };
    inline const juce::Colour border    { 0xff3a3d41 };
    inline const juce::Colour display   { 0xff07090a };
    inline const juce::Colour text      { 0xffecedef };
    inline const juce::Colour textSoft  { 0xffd9dadc };
    inline const juce::Colour textDim   { 0xffa7aaaf };
    inline const juce::Colour textFaint { 0xff7e8187 };

    // single accent (turquoise)
    inline const juce::Colour accent     { 0xff3fe0d0 };
    inline const juce::Colour accentSoft { 0xffa6f7ef };
    inline const juce::Colour accentDeep { 0xff139488 };
    inline const juce::Colour onAccent   { 0xff06201d };
}
