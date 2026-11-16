#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <algorithm>

// 108 chords = 12 roots x 9 chord types.
namespace chords
{
    constexpr int numRoots = 12;
    constexpr int numTypes = 9;
    constexpr int numChords = numRoots * numTypes;

    struct Type
    {
        const char* rowLabel;   // label at the left of the keypad row
        const char* suffix;     // short suffix printed on the key (UTF-8)
        const char* longName;   // display name
        int numTones;
        std::array<int, 4> intervals;
    };

    inline const std::array<Type, numTypes>& types()
    {
        static const std::array<Type, numTypes> t {{
            { "MAJ",  "",            "major",   3, { 0, 4, 7, 0 } },
            { "MIN",  "m",           "minor",   3, { 0, 3, 7, 0 } },
            { "7",    "7",           "7",       4, { 0, 4, 7, 10 } },
            { "MAJ7", "\xce\x94" "7", "maj7",   4, { 0, 4, 7, 11 } },
            { "m7",   "m7",          "m7",      4, { 0, 3, 7, 10 } },
            { "DIM",  "\xc2\xb0",    "dim",     3, { 0, 3, 6, 0 } },
            { "AUG",  "+",           "aug",     3, { 0, 4, 8, 0 } },
            { "SUS4", "s4",          "sus4",    3, { 0, 5, 7, 0 } },
            { "ADD9", "a9",          "add9",    4, { 0, 4, 7, 14 } },
        }};
        return t;
    }

    inline const char* rootName (int root)
    {
        static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        return names[((root % 12) + 12) % 12];
    }

    inline int index (int root, int type)   { return type * numRoots + root; }
    inline int rootOf (int chordIndex)      { return chordIndex % numRoots; }
    inline int typeOf (int chordIndex)      { return chordIndex / numRoots; }

    inline juce::String shortName (int chordIndex)
    {
        return juce::String (rootName (rootOf (chordIndex))) + juce::String::fromUTF8 (types()[(size_t) typeOf (chordIndex)].suffix);
    }

    // pitch classes (relative to C) used by the chord
    inline int pitchClassMask (int chordIndex, int transpose = 0)
    {
        const auto& t = types()[(size_t) typeOf (chordIndex)];
        int mask = 0;
        for (int i = 0; i < t.numTones; ++i)
            mask |= 1 << (((rootOf (chordIndex) + transpose + t.intervals[(size_t) i]) % 12 + 12) % 12);
        return mask;
    }

    // scale masks for the key display (major / natural minor)
    inline int scaleMask (int keyRoot, bool minor)
    {
        static const int majorSteps[] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int minorSteps[] = { 0, 2, 3, 5, 7, 8, 10 };
        int mask = 0;
        for (int i = 0; i < 7; ++i)
            mask |= 1 << ((keyRoot + (minor ? minorSteps[i] : majorSteps[i])) % 12);
        return mask;
    }

    inline bool isInKey (int chordIndex, int keyRoot, bool minor)
    {
        const int s = scaleMask (keyRoot, minor);
        const int c = pitchClassMask (chordIndex);
        return (c & ~s) == 0;
    }

    // Voicing for the chord layer and for MIDI export.
    // inversion: 0 = auto (voice leading: closest to the previous voicing), 1 = root position, 2 = 1st, 3 = 2nd, 4 = 3rd
    inline std::vector<int> voicing (int chordIndex, int transpose, int inversion, const std::vector<int>& previous)
    {
        std::vector<int> best;
        if (chordIndex < 0) return best;

        const auto& t = types()[(size_t) typeOf (chordIndex)];
        const int root = rootOf (chordIndex) + transpose;
        std::vector<int> rel;
        for (int i = 0; i < t.numTones; ++i) rel.push_back (t.intervals[(size_t) i]);

        auto build = [&] (int inv)
        {
            std::vector<int> v;
            for (int i = 0; i < (int) rel.size(); ++i)
            {
                const int idx = (i + inv) % (int) rel.size();
                const int octaveUp = (i + inv) >= (int) rel.size() ? 12 : 0;
                v.push_back (root + rel[(size_t) idx] + octaveUp);
            }
            // keep the bottom note between E3 and D#4
            while (v.front() < 52) for (auto& n : v) n += 12;
            while (v.front() >= 64) for (auto& n : v) n -= 12;
            std::sort (v.begin(), v.end());
            return v;
        };

        const int n = (int) rel.size();
        if (inversion >= 1)
            return build (juce::jlimit (0, n - 1, inversion - 1));

        int bestCost = 1 << 30;
        for (int inv = 0; inv < n; ++inv)
        {
            auto v = build (inv);
            int cost = 0;
            if (! previous.empty())
                for (int note : v)
                {
                    int d = 1 << 20;
                    for (int p : previous) d = std::min (d, std::abs (note - p));
                    cost += d;
                }
            else
                cost = std::abs (v.front() - 55);   // first chord: sit around G3
            if (cost < bestCost) { bestCost = cost; best = v; }
        }
        return best;
    }
}
