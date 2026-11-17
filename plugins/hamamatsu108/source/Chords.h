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

    // Stacked chords: several chord buttons pressed together. Encoded as customBase + root * 4096 + mask,
    // mask = the pitch classes relative to the root (bit 0 = root).
    constexpr int customBase = 10000;
    inline bool isCustom (int chordIndex)   { return chordIndex >= customBase; }
    inline int customOf (int root, int relMask) { return customBase + (((root % 12) + 12) % 12) * 4096 + (relMask & 4095); }
    inline bool isValid (int chordIndex)    { return (chordIndex >= 0 && chordIndex < numChords) || (chordIndex >= customBase && chordIndex < customBase + 12 * 4096); }

    inline int index (int root, int type)   { return type * numRoots + root; }
    inline int rootOf (int chordIndex)      { return isCustom (chordIndex) ? (chordIndex - customBase) / 4096 : chordIndex % numRoots; }
    inline int typeOf (int chordIndex)      { return isCustom (chordIndex) ? -1 : chordIndex / numRoots; }

    // intervals above the root (0 first, ascending; a 9th of add9 is kept as 14)
    inline std::vector<int> intervalsOf (int chordIndex)
    {
        std::vector<int> v;
        if (chordIndex < 0) return v;
        if (! isCustom (chordIndex))
        {
            const auto& t = types()[(size_t) typeOf (chordIndex)];
            for (int i = 0; i < t.numTones; ++i) v.push_back (t.intervals[(size_t) i]);
            return v;
        }
        const int rel = (chordIndex - customBase) % 4096;
        v.push_back (0);
        for (int i = 1; i < 12; ++i)
            if (rel & (1 << i)) v.push_back ((i == 1 || i == 2) && (rel & (0x18)) ? i + 12 : i);   // b9 / 9 above a third
        std::sort (v.begin(), v.end());
        return v;
    }

    // pitch classes (relative to C) used by the chord
    inline int pitchClassMask (int chordIndex, int transpose = 0)
    {
        int mask = 0;
        for (int iv : intervalsOf (chordIndex))
            mask |= 1 << (((rootOf (chordIndex) + transpose + iv) % 12 + 12) % 12);
        return mask;
    }

    // a stack of chords as one chord: the first chord's root, every tone of all of them
    inline int stack (const std::vector<int>& parts)
    {
        if (parts.empty()) return -1;
        if (parts.size() == 1) return parts.front();
        const int root = rootOf (parts.front());
        int abs = 0;
        for (int p : parts) abs |= pitchClassMask (p);
        int rel = 0;
        for (int i = 0; i < 12; ++i) if (abs & (1 << ((root + i) % 12))) rel |= 1 << i;
        // the stack may be a plain chord after all (C + Em = Cmaj7)
        for (int t = 0; t < numTypes; ++t)
            if (pitchClassMask (index (root, t)) == abs) return index (root, t);
        return customOf (root, rel);
    }

    inline juce::String shortName (int chordIndex)
    {
        if (chordIndex < 0) return "-";
        if (! isCustom (chordIndex))
            return juce::String (rootName (rootOf (chordIndex))) + juce::String::fromUTF8 (types()[(size_t) typeOf (chordIndex)].suffix);
        // a readable name for a stack: base triad + extensions
        const int rel = (chordIndex - customBase) % 4096;
        auto has = [rel] (int i) { return (rel & (1 << i)) != 0; };
        juce::String n (rootName (rootOf (chordIndex)));
        const bool maj3 = has (4), min3 = has (3) && ! has (4);
        if (min3) n << "m";
        else if (! maj3 && has (5)) n << "sus4";
        else if (! maj3 && has (2)) n << "sus2";
        if (has (10)) n << "7"; else if (has (11)) n << "maj7"; else if (has (9) && (maj3 || min3)) n << "6";
        if (has (2) && (maj3 || min3)) n << (has (10) || has (11) ? "(9)" : "add9");
        if (has (1)) n << "b9";
        if (has (3) && maj3) n << "#9";
        if (has (5) && (maj3 || min3)) n << "(11)";
        if (has (6)) n << (has (7) ? "#11" : "b5");
        if (has (8)) n << (has (7) ? "b13" : "#5");
        if (has (11) && has (10)) n << "+M7";
        return n;
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

        const int root = rootOf (chordIndex) + transpose;
        std::vector<int> rel = intervalsOf (chordIndex);

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
