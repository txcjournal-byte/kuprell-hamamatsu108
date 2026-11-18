#pragma once

#include <juce_core/juce_core.h>
#include "HarpVoice.h"
#include <array>
#include <cmath>

// Modulation sources and targets (block rate).
namespace mod
{
    constexpr int matrixSlots = 8, morphSlots = 8, funcPoints = 8;
    // continuous harp-bus targets (no parameter of their own): sweep the sound while notes ring
    enum BusTarget { tFilter = 21, tVolume, tPan, tPitch };

    enum Source { none, lfo1, lfo2, function, random, velocity, modWheel, aftertouch, morph, age, dream, wobble, numSources };

    inline const juce::StringArray& sourceNames()
    {
        static const juce::StringArray n { "None", "LFO 1", "LFO 2", "Function", "Random", "Velocity", "Mod Wheel", "Aftertouch", "Morph", "Age", "Dream", "Wobble" };
        return n;
    }

    // parameter IDs that can be modulated (index 0 = none)
    inline const juce::StringArray& targetIds()
    {
        static const juce::StringArray n { "", "harpTone", "harpDecay", "vibRate", "vibDepth", "chordLevel", "chordFilter", "bassLevel", "bassTone",
                                           "strumFeel", "strumSpeed", "arpChance", "fx1A", "fx1Mix", "fx2A", "fx2Mix", "fx3A", "fx3Mix", "fx4A", "fx4Mix", "master",
                                           "@filter", "@volume", "@pan", "@pitch", "filterCutoff", "filterReso", "drive", "morph" };
        return n;
    }

    inline const juce::StringArray& targetNames()
    {
        static const juce::StringArray n { "None", "Harp Tone", "Harp Decay", "Vibrato Rate", "Vibrato Depth", "Chord Level", "Chord Filter", "Bass Level", "Bass Tone",
                                           "Strum Feel", "Strum Speed", "Arp Chance", "FX 1 A", "FX 1 Mix", "FX 2 A", "FX 2 Mix", "FX 3 A", "FX 3 Mix", "FX 4 A", "FX 4 Mix", "Master",
                                           "Harp Filter", "Harp Volume", "Harp Pan", "Harp Pitch", "Filter Cutoff", "Filter Resonance", "Saturation", "Morph" };
        return n;
    }

    inline const juce::StringArray& lfoShapeNames() { static const juce::StringArray n { "Sine", "Triangle", "Saw", "Square", "S&H" }; return n; }
    inline const juce::StringArray& lfoSyncNames()  { static const juce::StringArray n { "Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" }; return n; }
    inline double syncQuarters (int i) { static const double q[] = { 0, 16, 8, 4, 2, 1, 0.5, 0.25 }; return q[juce::jlimit (0, 7, i)]; }
    inline const juce::StringArray& funcModeNames() { static const juce::StringArray n { "Loop", "One-shot", "Key trig" }; return n; }

    // bipolar LFO, -1..1
    struct Lfo
    {
        double phase = 0.0;
        float held = 0.0f;
        juce::Random rnd { 7 };

        float advance (double cycles, int shape)
        {
            const double before = phase;
            phase += cycles;
            if (phase >= 1.0) { phase -= std::floor (phase); held = rnd.nextFloat() * 2.0f - 1.0f; }
            juce::ignoreUnused (before);
            const float p = (float) phase;
            switch (shape)
            {
                case 1: return 1.0f - 4.0f * std::abs (p - 0.5f);
                case 2: return 2.0f * p - 1.0f;
                case 3: return p < 0.5f ? 1.0f : -1.0f;
                case 4: return held;
                default: return harp::sine() (p);
            }
        }
    };

    // drawn function: 8 points with free positions in time (first at 0, last at 1), linear between them; unipolar 0..1
    inline float functionValue (float t, const std::array<float, funcPoints>& xs, const std::array<float, funcPoints>& ys)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        for (int i = 0; i < funcPoints - 1; ++i)
        {
            const float x0 = xs[(size_t) i], x1 = xs[(size_t) i + 1];
            if (t <= x1 || i == funcPoints - 2)
            {
                if (x1 - x0 < 1.0e-5f) return t < x1 ? ys[(size_t) i] : ys[(size_t) i + 1];   // vertical edge = instant jump
                const float f = juce::jlimit (0.0f, 1.0f, (t - x0) / (x1 - x0));
                return ys[(size_t) i] + f * (ys[(size_t) i + 1] - ys[(size_t) i]);
            }
        }
        return ys.back();
    }

    struct Function
    {
        double phase = 0.0;
        bool running = true;
        void trigger() { phase = 0.0; running = true; }
        float advance (double cycles, int mode, const std::array<float, funcPoints>& xs, const std::array<float, funcPoints>& ys)
        {
            if (running) phase += cycles;
            if (phase >= 1.0)
            {
                if (mode != 1) phase -= std::floor (phase);   // loop and key trig repeat
                else { phase = 1.0; running = false; }        // one-shot holds the end
            }
            return functionValue ((float) phase, xs, ys);
        }
    };

    // random walk: new target at Rate, Smooth glides, Chaos sets the jump size; -1..1
    struct Random
    {
        double phase = 0.0;
        float value = 0.0f, target = 0.0f;
        void reset() { phase = 0.0; value = target = 0.0f; }
        juce::Random rnd { 108 };
        float advance (double cycles, float smooth, float chaos, float blockSeconds)
        {
            phase += cycles;
            if (phase >= 1.0)
            {
                phase -= std::floor (phase);
                const float jump = (rnd.nextFloat() * 2.0f - 1.0f) * (0.25f + 0.75f * chaos) * 2.0f;
                target = juce::jlimit (-1.0f, 1.0f, chaos > 0.95f ? jump * 0.5f : target + jump);
            }
            const float t = 0.002f + smooth * 0.5f;
            value += (target - value) * juce::jmin (1.0f, blockSeconds / t);
            return value;
        }
    };
}
