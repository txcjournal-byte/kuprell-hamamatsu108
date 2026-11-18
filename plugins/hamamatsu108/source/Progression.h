#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <array>
#include "Chords.h"

// PROGRESSION: a row of up to 8 chords (one bar each) and a generator that turns them into notes.
// The same generator plays inside the plugin (with the song) and writes the MIDI clip you drag out,
// so what you hear is exactly what lands in the piano roll.
namespace prog
{
    constexpr int numSlots = 8;
    enum Mode { strum = 0, arp, melody, numModes };
    inline const juce::StringArray& modeNames() { static const juce::StringArray n { "STRUM", "ARP", "MELODY" }; return n; }

    struct Note { double beat; int pitch; float velocity; double length; };

    struct Settings
    {
        int mode = strum, seed = 1;
        int keyRoot = 0; bool minor = false;
        int transpose = 0, range = 2, octave = 0;   // octave: applied when the clip is written (the plugin shifts what it plays itself)
        // strum
        juce::String strumSteps { "D...D.U...U.D.U." };   // 16ths, letters as the strum patterns
        float strumSpeedMs = 18.0f;
        // arp
        double arpStepBeats = 0.5;
        int arpOctaves = 1, arpPattern = 0;
        float arpLength = 0.8f;
        std::array<float, 16> stepVel {};
        int stepLen = 16;
        double bpm = 120.0;
    };

    // ready-made progressions in the key, as scale degrees (0 = I) and minor flags
    struct Ready { const char* name; std::array<int, 4> deg; };
    inline const std::array<Ready, 6>& readyMade()
    {
        static const std::array<Ready, 6> r { {
            { "I - V - vi - IV   (pop)",        { 0, 4, 5, 3 } },
            { "vi - IV - I - V   (emotional)",  { 5, 3, 0, 4 } },
            { "I - vi - IV - V   (50s)",        { 0, 5, 3, 4 } },
            { "ii - V - I - I    (jazz)",       { 1, 4, 0, 0 } },
            { "I - IV - vi - V   (bright)",     { 0, 3, 5, 4 } },
            { "vi - V - IV - V   (dark pop)",   { 5, 4, 3, 4 } } } };
        return r;
    }

    // the chord on a scale degree of the key (major: I ii iii IV V vi vii°; minor: i ii° III iv v VI VII)
    inline int chordOnDegree (int keyRoot, bool minor, int degree)
    {
        static const int majSteps[] = { 0, 2, 4, 5, 7, 9, 11 }, minSteps[] = { 0, 2, 3, 5, 7, 8, 10 };
        static const int majType[] = { 0, 1, 1, 0, 0, 1, 1 }, minType[] = { 1, 1, 0, 1, 1, 0, 0 };
        const int d = ((degree % 7) + 7) % 7;
        const int root = (keyRoot + (minor ? minSteps[d] : majSteps[d])) % 12;
        return chords::index (root, minor ? minType[d] : majType[d]);
    }

    inline std::vector<int> chordTones (int chord, int transpose, int range)
    {
        std::vector<int> t;
        if (chord < 0) return t;
        const int mask = chords::pitchClassMask (chord, transpose);
        static const int lows[] = { 60, 60, 54, 48, 48 };
        const int low = lows[juce::jlimit (1, 4, range)];
        for (int n = low; n <= low + 12 * juce::jlimit (1, 4, range); ++n)
            if (mask & (1 << (n % 12))) t.push_back (n);
        return t;
    }

    // one bar (4 beats) of notes for a chord
    inline void bar (int chord, int barIndex, int prevChord, const Settings& s, std::vector<Note>& out)
    {
        const auto tones = chordTones (chord, s.transpose, s.range);
        if (tones.empty()) return;
        const int n = (int) tones.size();
        const size_t first = out.size();

        if (s.mode == strum)
        {
            const int len = juce::jmax (1, s.strumSteps.length());
            const double stepBeats = 0.25;   // the steps are 16ths
            const double perString = s.strumSpeedMs / 1000.0 * s.bpm / 60.0;
            std::vector<double> strokeStarts;
            for (int i = 0; i < len; ++i)
            {
                const auto c = (char) s.strumSteps[i];
                if (c == '.' || c == 'x') continue;
                const double b = i * stepBeats;
                strokeStarts.push_back (b);
                int lo = 0, hi = n - 1; bool up = false; float v = 0.9f; double speed = 1.0;
                switch (c)
                {
                    case 'D': break;
                    case 'U': up = true; lo = n * 3 / 10; v = 0.7f; break;
                    case 'd': hi = juce::jmax (1, n * 6 / 10); v = 0.6f; break;
                    case 'u': up = true; lo = n * 6 / 10; v = 0.5f; break;
                    case 'B': hi = juce::jmin (1, n - 1); v = 0.8f; break;
                    case 'H': lo = juce::jmax (0, n - 2); v = 0.6f; break;
                    case 'T': lo = hi = n / 2; v = 0.55f; break;
                    case 'S': speed = 3.0; v = 0.75f; break;
                    default: break;
                }
                const float accent = (i % 4 == 0) ? 1.0f : 0.9f;
                for (int k = 0; k <= hi - lo; ++k)
                {
                    const int idx = up ? hi - k : lo + k;
                    out.push_back ({ b + k * perString * speed, tones[(size_t) idx], juce::jlimit (0.1f, 1.0f, v * accent * (1.0f - 0.2f * (float) k / (float) n)), 1.0 });
                }
            }
            // every string rings until the next stroke (at least an 1/8, at most 2 beats)
            for (size_t i = first; i < out.size(); ++i)
            {
                double next = 4.0;
                for (double st : strokeStarts) if (st > out[i].beat + 1.0e-6) { next = st; break; }
                out[i].length = juce::jlimit (0.5, 2.0, next - out[i].beat);
            }
        }
        else if (s.mode == arp)
        {
            std::vector<int> run;
            const int octs = juce::jlimit (1, 4, s.arpOctaves);
            const int base = tones.front();
            std::vector<int> one;
            for (int t : tones) if (t < base + 12) one.push_back (t);
            for (int o = 0; o < octs; ++o) for (int t : one) run.push_back (t + 12 * o);
            const int m = (int) run.size();
            const int steps = juce::jmax (1, (int) std::round (4.0 / s.arpStepBeats));
            juce::Random r (s.seed * 7919 + barIndex * 131 + chord);
            for (int i = 0; i < steps; ++i)
            {
                const int si = i % juce::jlimit (1, 16, s.stepLen);
                const float sv = s.stepVel[(size_t) si];
                if (sv <= 0.06f) continue;   // a rest
                int idx;
                switch (s.arpPattern)
                {
                    case 1:  idx = m - 1 - (i % m); break;                                            // down
                    case 2:  { const int span = juce::jmax (1, 2 * m - 2); const int p = i % span; idx = p < m ? p : span - p; break; }   // up / down
                    case 5:  idx = r.nextInt (m); break;                                              // random
                    default: idx = i % m; break;                                                      // up
                }
                out.push_back ({ i * s.arpStepBeats, run[(size_t) idx], juce::jlimit (0.1f, 1.0f, 0.35f + 0.65f * sv), s.arpStepBeats * juce::jlimit (0.1f, 1.0f, s.arpLength) });
            }
        }
        else   // melody: chord tones on the strong 8ths, scale steps in between, a few rests; the same seed = the same tune
        {
            const int scale = chords::scaleMask (s.keyRoot, s.minor);
            std::vector<int> pool;   // C4 .. C6 in the key
            for (int p = 60 + s.transpose; p <= 84 + s.transpose; ++p) if (scale & (1 << (((p % 12) + 12) % 12))) pool.push_back (p);
            const int cmask = chords::pitchClassMask (chord, s.transpose);
            juce::Random r (s.seed * 104729 + barIndex * 977 + chord * 13);
            // a fixed rhythm per seed, shared by every bar (that is what makes it a hook)
            juce::Random rr (s.seed * 31337);
            std::array<bool, 8> hit {};
            for (int i = 0; i < 8; ++i) hit[(size_t) i] = i == 0 || rr.nextFloat() < (i % 2 == 0 ? 0.75f : 0.45f);
            int cur = (int) pool.size() / 3;
            // start near a chord tone
            for (int k = 0; k < (int) pool.size(); ++k) if (cmask & (1 << (pool[(size_t) k] % 12))) { cur = k + (int) pool.size() / 4; break; }
            cur = juce::jlimit (0, (int) pool.size() - 1, cur);
            juce::ignoreUnused (prevChord);
            for (int i = 0; i < 8; ++i)
            {
                if (! hit[(size_t) i]) continue;
                if (i % 2 == 0)
                {
                    // strong: the nearest chord tone, a little up or down
                    int best = cur, bestD = 99;
                    const int dir = r.nextFloat() < 0.5f ? -1 : 1;
                    for (int k = 0; k < (int) pool.size(); ++k)
                        if (cmask & (1 << (pool[(size_t) k] % 12)))
                        {
                            const int d = std::abs (k - (cur + dir));
                            if (d < bestD) { bestD = d; best = k; }
                        }
                    cur = best;
                }
                else cur = juce::jlimit (0, (int) pool.size() - 1, cur + (r.nextFloat() < 0.5f ? -1 : 1) * (r.nextFloat() < 0.75f ? 1 : 2));
                out.push_back ({ i * 0.5, pool[(size_t) cur], i % 2 == 0 ? (i == 0 ? 0.9f : 0.8f) : 0.62f, 0.5 });
            }
            for (size_t i = first; i < out.size(); ++i)
                out[i].length = (i + 1 < out.size() ? out[i + 1].beat : 4.0) - out[i].beat - 0.02;
        }
    }

    // the whole clip: the filled slots in order, repeated to fill the bars
    inline std::vector<Note> clip (const std::array<int, numSlots>& slots, int bars, const Settings& s)
    {
        std::vector<int> used;
        for (int c : slots) if (c >= 0) used.push_back (c);
        std::vector<Note> out;
        if (used.empty()) return out;
        for (int b = 0; b < bars; ++b)
        {
            const size_t start = out.size();
            bar (used[(size_t) b % used.size()], b, used[(size_t) (b + used.size() - 1) % used.size()], s, out);
            for (size_t i = start; i < out.size(); ++i) out[i].beat += 4.0 * b;
        }
        return out;
    }
}
