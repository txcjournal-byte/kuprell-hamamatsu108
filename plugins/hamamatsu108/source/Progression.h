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
    enum Mode { strum = 0, arp, melody, grid, numModes };
    inline const juce::StringArray& modeNames() { static const juce::StringArray n { "STRUM", "ARP", "MELODY", "MOTION" }; return n; }

    // MOTION: your own pattern on a grid of 8 rows (the chord's strings, low to high) x up to 16 steps
    constexpr int gridRows = 8, gridSteps = 16, linePoints = 64;
    // STRINGS: the pulse of each string, in beats
    inline const std::array<double, 10>& stringRates() { static const std::array<double, 10> r { 0.0, 2.0, 1.0, 1.5, 0.75, 0.5, 2.0 / 3.0, 1.0 / 3.0, 0.25, 1.25 }; return r; }
    inline const juce::StringArray& stringRateNames() { static const juce::StringArray n { "off", "1/2", "1/4", "1/4.", "1/8.", "1/8", "1/4T", "1/8T", "1/16", "5/16" }; return n; }
    struct GridPreset { const char* name; std::array<int, 16> cells; };   // bit r = row r on
    inline const std::array<GridPreset, 8>& gridPresets()
    {
        static const std::array<GridPreset, 8> p { {
            { "Glide",    { 0b00010001, 0, 0b00100000, 0b01000000, 0b10000000, 0, 0b00100000, 0b00010000, 0b00010010, 0, 0b01000000, 0b10000000, 0b00100000, 0, 0b00001000, 0b01000000 } },
            { "Rise",     { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 } },
            { "Fall",     { 128, 64, 32, 16, 8, 4, 2, 1, 128, 64, 32, 16, 8, 4, 2, 1 } },
            { "Pendulum", { 1, 4, 16, 64, 128, 32, 8, 2, 1, 4, 16, 64, 128, 32, 8, 2 } },
            { "Pulse",    { 0b10010011, 0, 0, 0b01001000, 0, 0, 0b10010011, 0, 0, 0b01001000, 0, 0, 0b10010011, 0, 0b01000100, 0 } },
            { "Broken",   { 0b00000011, 0, 0b00110000, 0, 0b00001100, 0, 0b11000000, 0, 0b00000011, 0, 0b00110000, 0, 0b00001100, 0b01000000, 0b10000000, 0 } },
            { "Sparkle",  { 0b00000001, 0b10000000, 0, 0b01000000, 0b00000100, 0, 0b10100000, 0, 0b00000001, 0, 0b01000000, 0b10000000, 0b00010000, 0, 0b11000000, 0 } },
            { "Strum",    { 0b11111111, 0, 0, 0, 0, 0, 0b11111100, 0, 0b11111111, 0, 0b11110000, 0, 0, 0, 0b11111100, 0 } } } };
        return p;
    }

    struct Note { double beat; int pitch; float velocity; double length; };

    struct Settings
    {
        int mode = strum, seed = 1;
        int keyRoot = 0; bool minor = false;
        int transpose = 0, range = 2, octave = 0;   // octave: applied when the clip is written (the plugin shifts what it plays itself)
        // strum
        juce::String strumSteps { "D...D.U...U.D.U." };   // 16ths, letters as the strum patterns
        float strumSpeedMs = 18.0f;
        float feel = 0.5f;   // human looseness: timing, force and stroke speed vary a little
        // arp
        double arpStepBeats = 0.5;
        int arpOctaves = 1, arpPattern = 0;
        float arpLength = 0.8f;
        std::array<float, 16> stepVel {};
        int stepLen = 16;
        // motion grid
        std::array<int, 16> gridCells {};
        int gridLen = 16;
        double gridStepBeats = 0.25;
        int motionType = 0;                  // 0 = GRID (cells), 1 = LINES (a drawn finger path)
        std::array<float, 64> line {};       // LINES: height 0..1 at 64 points of the bar, < 0 = finger lifted
        std::array<int, 8> strRate {};       // STRINGS: every string its own pulse (index into stringRates, 0 = silent)
        std::array<int, 8> strOffset {};     //          where it starts, in 16ths
        std::array<float, 8> strLevel {};    //          how strong
        std::array<int, 8> strOct {};        //          octave shift (-1, 0, +1)
        std::array<int, 8> strChance {};     //          chance of every hit in %, 0 = 100
        std::array<int, 8> strRatchet {};    //          1 = single hit, 2 / 3 = quick repeats (a flam / roll)
        std::array<bool, 8> strMute {};
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
            const double msToBeats = s.bpm / 60.0 / 1000.0;
            juce::Random hr (s.seed * 6151 + barIndex * 389 + chord * 7);   // a human hand: never twice the same
            std::vector<double> strokeStarts;
            for (int i = 0; i < len; ++i)
            {
                const auto c = (char) s.strumSteps[i];
                if (c == '.' || c == 'x') continue;
                // late / early by a few ms, the downbeat stays tight
                const double drift = (i % 4 == 0 ? 0.3 : 1.0) * s.feel * (hr.nextFloat() * 2.0 - 0.8) * 14.0 * msToBeats;
                const double b = juce::jmax (0.0, i * stepBeats + drift);
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
                // beat 1 strongest, other beats a little less, off-beats lighter; force varies with FEEL
                const float accent = (i == 0 ? 1.0f : i % 4 == 0 ? 0.92f : i % 2 == 0 ? 0.82f : 0.74f) * (1.0f + s.feel * 0.18f * (hr.nextFloat() - 0.5f));
                const double strokeSpeed = speed * (1.0 + s.feel * 0.5 * (hr.nextFloat() - 0.5)) * (up ? 0.8 : 1.0);   // up strokes are quicker
                // a real stroke skips a string now and then (only with a loose hand, never on the downbeat)
                const int skip = (s.feel > 0.3f && i % 4 != 0 && hr.nextFloat() < 0.25f * s.feel) ? hr.nextInt (juce::jmax (1, hi - lo + 1)) : -1;
                for (int k = 0; k <= hi - lo; ++k)
                {
                    if (k == skip) continue;
                    const int idx = up ? hi - k : lo + k;
                    // the first strings get the most force, the hand loses some on the way
                    const float fall = 1.0f - 0.25f * (float) k / (float) juce::jmax (1, hi - lo);
                    out.push_back ({ b + k * perString * strokeSpeed, tones[(size_t) idx], juce::jlimit (0.1f, 1.0f, v * accent * fall), 1.0 });
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
        else if (s.mode == grid && s.motionType == 3)
        {
            // STRINGS: every string pulses at its own rate from its own start: the patterns drift against each other
            const double barStart = 4.0 * barIndex;
            for (int r = 0; r < gridRows; ++r)
            {
                const double period = stringRates()[(size_t) juce::jlimit (0, 9, s.strRate[(size_t) r])];
                if (period <= 0.0 || s.strMute[(size_t) r]) continue;
                const double t0 = 0.25 * s.strOffset[(size_t) r];
                const int idx = juce::roundToInt ((float) r * (float) (n - 1) / (float) (gridRows - 1));
                const int pitch = juce::jlimit (12, 115, tones[(size_t) idx] + 12 * juce::jlimit (-1, 1, s.strOct[(size_t) r]));
                const int chance = s.strChance[(size_t) r] <= 0 ? 100 : s.strChance[(size_t) r];
                const int ratchet = juce::jlimit (1, 3, s.strRatchet[(size_t) r] <= 0 ? 1 : s.strRatchet[(size_t) r]);
                // the first hit at or after the bar start (the pulse runs from song position 0)
                double k = std::ceil ((barStart - t0) / period - 1.0e-9);
                for (double t = t0 + k * period; t < barStart + 4.0 - 1.0e-9; t += period)
                    if (t >= barStart - 1.0e-9)
                    {
                        // CHANCE: the same bar always gives the same choice (seeded by the time and the string)
                        juce::Random cr ((juce::int64) (s.seed * 7919 + r * 104729 + (int) std::round (t * 96.0)));
                        if (cr.nextInt (100) >= chance) continue;
                        const float v = juce::jlimit (0.05f, 1.0f, s.strLevel[(size_t) r] * ((std::fmod (t, 1.0) < 1.0e-6) ? 1.0f : 0.85f));
                        const double gap = juce::jmin (period, 0.25) / (double) ratchet;
                        for (int q = 0; q < ratchet; ++q)
                            out.push_back ({ t - barStart + q * gap, pitch, v * (q == 0 ? 1.0f : 0.7f - 0.1f * (float) q), juce::jlimit (0.125, 2.0, q + 1 < ratchet ? gap : period) });
                    }
            }
        }
        else if (s.mode == grid && s.motionType == 1)
        {
            // LINES: the drawn path is a finger moving over the strings during the bar; every string it crosses sounds
            const double dt = 4.0 / linePoints;
            int prev = -1;
            auto rowTone = [&] (int row) { return tones[(size_t) juce::roundToInt ((float) row * (float) (n - 1) / (float) (gridRows - 1))]; };
            for (int i = 0; i < linePoints; ++i)
            {
                const float y = s.line[(size_t) i];
                if (y < 0.0f) { prev = -1; continue; }
                const int row = juce::jlimit (0, gridRows - 1, juce::roundToInt (y * (gridRows - 1)));
                if (prev < 0)
                    out.push_back ({ i * dt, rowTone (row), 0.85f, 0.5 });   // the finger lands
                else if (row != prev)
                {
                    const int dir = row > prev ? 1 : -1, count = std::abs (row - prev);
                    const float vel = juce::jlimit (0.35f, 1.0f, 0.55f + 0.12f * count);   // a faster move = a stronger stroke
                    for (int k = 1; k <= count; ++k)
                        out.push_back ({ i * dt - dt + dt * k / (double) count, rowTone (prev + dir * k), vel, 0.5 });
                }
                prev = row;
            }
            // every string rings until the finger crosses it again (or the bar ends)
            for (size_t i = first; i < out.size(); ++i)
            {
                double next = 4.0;
                for (size_t j = i + 1; j < out.size(); ++j) if (out[j].pitch == out[i].pitch) { next = out[j].beat; break; }
                out[i].length = juce::jlimit (0.0625, 2.0, next - out[i].beat);
            }
        }
        else if (s.mode == grid)
        {
            // every lit cell plays its row's string; several cells on one step roll quickly upwards (like a fast hand)
            const int steps = juce::jmax (1, (int) std::round (4.0 / s.gridStepBeats));
            const int len = juce::jlimit (1, gridSteps, s.gridLen);
            const double roll = s.strumSpeedMs * 0.5 / 1000.0 * s.bpm / 60.0;
            juce::Random hr (s.seed * 4099 + barIndex * 61 + chord);
            for (int i = 0; i < steps; ++i)
            {
                const int mask = s.gridCells[(size_t) (i % len)];
                if (mask == 0) continue;
                // strong contrast: the beat hits hard, the off-steps sit back, so a changed pattern is clearly heard
                const double beatPos = i * s.gridStepBeats;
                const float accent = beatPos == std::floor (beatPos) ? (std::fmod (beatPos, 2.0) == 0.0 ? 1.0f : 0.9f)
                                   : (std::fmod (beatPos * 2.0, 1.0) == 0.0 ? 0.68f : 0.52f);
                int k = 0;
                for (int r = 0; r < gridRows; ++r)
                    if (mask & (1 << r))
                    {
                        const int idx = juce::roundToInt ((float) r * (float) (n - 1) / (float) (gridRows - 1));
                        const double drift = s.feel * (hr.nextFloat() - 0.5) * 10.0 / 1000.0 * s.bpm / 60.0;
                        const float vel = juce::jlimit (0.1f, 1.0f, accent * (1.0f - 0.06f * k) * (1.0f + s.feel * 0.15f * (hr.nextFloat() - 0.5f)));
                        // the note rings until this row plays again (at most 2 beats)
                        int gapSteps = 1;
                        while (gapSteps < steps && (s.gridCells[(size_t) ((i + gapSteps) % len)] & (1 << r)) == 0) ++gapSteps;
                        out.push_back ({ juce::jmax (0.0, i * s.gridStepBeats + k * roll + drift), tones[(size_t) idx], vel,
                                         juce::jlimit (s.gridStepBeats * 0.9, 2.0, gapSteps * s.gridStepBeats) });
                        ++k;
                    }
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
