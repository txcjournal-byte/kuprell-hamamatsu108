#pragma once

#include <juce_core/juce_core.h>
#include <array>

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
}
