#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cmath>

// Harp voices, pure synthesis:
//  - Classic Chime: two detuned band-limited pulse/triangle oscillators through an envelope-swept low-pass (the classic strum-harp tone)
//  - Celesta, Vibes, Glass, Marimba, Steel: modal synthesis (resonant modes of bars, glass and steel pans, excited by a mallet)
//  - Pluck: physical string model (Karplus-Strong with fractional tuning, pick position, two strings)
//  - E-Piano: two-operator FM tine model
//  - Flute, Strings: subtractive with breath / ensemble
// Every voice is click-free: soft attacks, and a stolen voice fades out for 3 ms before the new note starts.
namespace harp
{
    enum class Model { chime, celesta, vibes, glass, pluck, flute, marimba, steel, epiano, strings };

    inline const juce::StringArray& voiceNames()
    {
        static const juce::StringArray names { "Classic Chime", "Celesta", "Vibes", "Glass", "Pluck",
                                               "Flute", "Marimba", "Steel", "E-Piano", "Strings" };
        return names;
    }

    struct SineTable
    {
        static constexpr int size = 4096;
        std::array<float, size + 1> data {};
        SineTable()
        {
            for (int i = 0; i <= size; ++i)
                data[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * i / size);
        }
        inline float operator() (float phase) const noexcept
        {
            const float p = phase * (float) size;
            const int i = juce::jlimit (0, size - 1, (int) p);
            const float f = p - (float) i;
            return data[(size_t) i] + f * (data[(size_t) i + 1] - data[(size_t) i]);
        }
    };

    inline const SineTable& sine() { static const SineTable table; return table; }
    inline float wrap (float p) noexcept { return p - std::floor (p); }

    inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    struct Settings
    {
        Model model = Model::chime;
        float tone = 0.6f;     // 0..1 brightness
        float decay = 1.8f;    // seconds
    };

    // 2-pole resonator: one mode of a vibrating body
    struct Mode
    {
        float b1 = 0, b2 = 0, y1 = 0, y2 = 0, amp = 0;
        void set (float freq, float t60, float a, float sr)
        {
            const float r = std::exp (-6.9078f / (juce::jmax (0.01f, t60) * sr));
            const float w = juce::MathConstants<float>::twoPi * freq / sr;
            b1 = 2.0f * r * std::cos (w);
            b2 = -r * r;
            amp = a * std::sin (w);   // impulse response peak = a at every pitch
            y1 = y2 = 0.0f;
        }
        inline float process (float x) noexcept
        {
            const float y = b1 * y1 + b2 * y2 + x * amp;
            y2 = y1; y1 = y;
            return y;
        }
    };

    // TPT state-variable filter
    struct Svf
    {
        float ic1 = 0, ic2 = 0, g = 0, k = 1.4f, a1 = 0, a2 = 0, a3 = 0;
        void set (float cutoff, float q, float sr)
        {
            g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, sr * 0.45f, cutoff) / sr);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        inline float lp (float x, float& bp) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            bp = v1;
            return v2;
        }
        void reset() { ic1 = ic2 = 0; }
    };

    // string with exact tuning: delay line + loss filter + Thiran fractional all-pass
    struct String
    {
        std::vector<float> buf;
        int w = 0, len = 2;
        float apCoef = 0, apX = 0, apY = 0, loss = 0.99f, damp = 0.5f, prev = 0;
        void prepare (int maxLen) { buf.assign ((size_t) juce::jmax (8, maxLen), 0.0f); }
        void tune (float freq, float t60, float bright, float sr)
        {
            const float period = sr / freq;
            damp = 0.5f * (1.0f - bright * 0.6f);
            const float total = period - damp;                // the loss filter delays by 'damp' samples
            len = juce::jlimit (2, (int) buf.size() - 2, (int) std::floor (total - 0.1f));
            float frac = total - (float) len;                  // kept in 0.1 .. 1.1: stable and flat
            if (frac < 0.1f && len > 2) { --len; frac += 1.0f; }
            apCoef = (1.0f - frac) / (1.0f + frac);
            loss = std::pow (0.001f, 1.0f / (juce::jmax (0.05f, t60) * freq));
            apX = apY = prev = 0.0f;
            std::fill (buf.begin(), buf.end(), 0.0f);
            w = 0;
        }
        void excite (int i, float v) { buf[(size_t) ((w + i) % len)] += v; }
        inline float process() noexcept
        {
            const float x = buf[(size_t) w];
            const float lpOut = (1.0f - damp) * x + damp * prev;
            prev = x;
            const float y = apCoef * (lpOut - apY) + apX;
            apX = lpOut; apY = y;
            buf[(size_t) w] = y * loss;
            if (++w >= len) w = 0;
            return x;
        }
    };

    class Voice
    {
    public:
        bool isActive() const noexcept { return active || pending; }
        bool isBusy() const noexcept   { return pending || fadeOutLeft > 0; }   // fading out or waiting to restart
        int getNote() const noexcept   { return pending ? pendingArgs.note : note; }
        juce::int64 getAge() const noexcept { return age; }
        float getLevel() const noexcept { return active ? level : 0.0f; }

        void prepare (double newSampleRate)
        {
            sr = (float) newSampleRate;
            for (auto& s : strings) s.prepare ((int) (sr / 20.0f) + 8);
            burst.assign ((size_t) (sr / 20.0f) + 8, 0.0f);
            active = pending = false;
        }

        void start (int midiNote, float velocity, float pan, const Settings& s, juce::Random& rng, float hz)
        {
            const Args args { midiNote, velocity, pan, s, hz };
            if (active && level > 1.0e-4f)
            {
                // steal smoothly: fade the old note for 3 ms, then start the new one
                pendingArgs = args;
                if (pending || fadeOutLeft > 0)   // already fading: keep going from where it is (never jump back up)
                {
                    pending = true;
                    return;
                }
                pending = true;
                fadeOut = juce::jmax (1, (int) (0.003f * sr));
                fadeOutLeft = fadeOut;
                return;
            }
            begin (args, rng);
        }

        void release (float seconds = 0.006f)
        {
            if (! active) return;
            pending = false;
            const int len = juce::jmax (1, (int) (seconds * sr));
            if (fadeOutLeft > 0)
            {
                // continue the running fade, stretched to the new length, from the current gain
                const float g = (float) fadeOutLeft / (float) fadeOut;
                fadeOut = len; fadeOutLeft = juce::jmax (1, (int) (g * (float) len));
                return;
            }
            fadeOut = len;
            fadeOutLeft = fadeOut;
        }

        void render (float* left, float* right, int numSamples, const float* pitchMul, juce::Random& rng)
        {
            if (! active && ! pending) return;
            if (! active && pending) { pending = false; begin (pendingArgs, rng); }

            float peak = 0.0f;
            for (int n = 0; n < numSamples; ++n)
            {
                float s = next (pitchMul[n], rng);
                if (attack < 1.0f) attack = juce::jmin (1.0f, attack + attackInc);
                env *= envMul;
                s *= attack * env;

                if (fadeOutLeft > 0)
                {
                    s *= (float) fadeOutLeft / (float) fadeOut;
                    if (--fadeOutLeft == 0)
                    {
                        if (pending) { pending = false; begin (pendingArgs, rng); continue; }
                        active = false;
                        break;
                    }
                }

                const float out = s * gain;
               #ifdef HAMA_PROBE
                {
                    const float d2 = out - 2.0f * pr1 + pr2; pr2 = pr1; pr1 = out;
                    prEnv = 0.995f * prEnv + 0.005f * d2 * d2;
                    if (std::abs (d2) > 0.006f && d2 * d2 > 100.0f * prEnv)
                        printf ("PROBE note %d model %d age %lld n %d fadeLeft %d pending %d attack %.3f env %.3f d2 %.4f out %.4f excite %d\n",
                                note, (int) model, (long long) age, n, fadeOutLeft, (int) pending, attack, env, d2, out, exciteLeft);
                }
               #endif
                left[n]  += out * panL;
                right[n] += out * panR;
                peak = juce::jmax (peak, std::abs (out));
            }
            level = peak;
            age += numSamples;
            if (active && fadeOutLeft == 0 && ! pending && exciteLeft == 0 && peak < 3.0e-6f && age > (juce::int64) (0.05f * sr))
                active = false;
        }

    private:
        struct Args { int note = 60; float velocity = 1.0f, pan = 0.0f; Settings s; float hz = 440.0f; };

        void begin (const Args& a, juce::Random& rng)
        {
            note = a.note;
            model = a.s.model;
            active = true;
            age = 0;
            fadeOutLeft = 0;
            freq = a.hz;
            vel = a.velocity;
            const float tone = juce::jlimit (0.0f, 1.0f, a.s.tone);
            const float bright = juce::jlimit (0.0f, 1.0f, tone * (0.55f + 0.45f * vel));
            const float noteScale = juce::jlimit (0.3f, 1.6f, std::pow (2.0f, (60.0f - (float) note) / 30.0f));
            const float t60 = juce::jmax (0.08f, a.s.decay) * noteScale;

            panL = std::cos ((a.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            panR = std::sin ((a.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            attack = 0.0f;
            attackInc = 1.0f / (0.0015f * sr);
            env = 1.0f;
            envMul = 1.0f;
            exciteLeft = 0;
            numModes = 0;
            tremolo = 0.0f;
            filterCounter = 0;
            for (auto& p : phase) p = rng.nextFloat();
            const float velGain = 0.35f + 0.65f * vel;

            switch (model)
            {
                case Model::chime:
                    detune = 1.0024f;
                    filterBase = 250.0f + freq * 1.5f;
                    filterSweep = (1500.0f + 9000.0f * bright) * (0.6f + 0.4f * vel);
                    filterEnv = 1.0f;
                    filterEnvMul = std::exp (-1.0f / ((0.06f + 0.09f * t60) * sr));
                    envMul = std::exp (-6.9078f / (t60 * sr));
                    svf[0].reset(); svf[1].reset();
                    gain = 0.33f * velGain;
                    break;

                case Model::celesta: case Model::vibes: case Model::glass: case Model::marimba: case Model::steel:
                {
                    struct M { float r, a, d; };
                    static const M celesta[] = { { 1.0f, 1.0f, 1.0f }, { 2.0f, 0.06f, 0.5f }, { 4.0f, 0.22f, 0.25f }, { 9.93f, 0.05f, 0.08f } };
                    static const M vibes[]   = { { 1.0f, 1.0f, 1.0f }, { 4.0f, 0.32f, 0.3f }, { 10.0f, 0.07f, 0.1f }, { 2.0f, 0.03f, 0.6f } };
                    static const M glass[]   = { { 1.0f, 1.0f, 1.0f }, { 2.32f, 0.22f, 0.6f }, { 4.25f, 0.1f, 0.45f }, { 6.63f, 0.05f, 0.3f }, { 9.38f, 0.02f, 0.2f } };
                    static const M marimba[] = { { 1.0f, 1.0f, 1.0f }, { 3.93f, 0.3f, 0.2f }, { 9.2f, 0.08f, 0.07f } };
                    static const M steel[]   = { { 1.0f, 1.0f, 1.0f }, { 2.0f, 0.6f, 0.8f }, { 3.0f, 0.32f, 0.55f }, { 4.03f, 0.18f, 0.4f }, { 5.08f, 0.08f, 0.28f }, { 2.98f, 0.1f, 0.5f } };
                    const M* table = celesta; int count = 4; float decayScale = 0.8f, malletMs = 1.0f;
                    if (model == Model::vibes)   { table = vibes;   count = 4; decayScale = 1.4f; malletMs = 1.5f; tremolo = 0.22f; }
                    if (model == Model::glass)   { table = glass;   count = 5; decayScale = 1.3f; malletMs = 4.0f; }
                    if (model == Model::marimba) { table = marimba; count = 3; decayScale = 0.3f; malletMs = 2.0f; }
                    if (model == Model::steel)   { table = steel;   count = 6; decayScale = 0.8f; malletMs = 1.2f; }
                    for (int i = 0; i < count; ++i)
                    {
                        const float f = freq * table[i].r;
                        if (f > juce::jmin (sr * 0.42f, 16000.0f)) continue;
                        const float octaves = std::log2 (juce::jmax (1.0f, table[i].r));
                        float amp = table[i].a * std::pow (0.35f + 1.1f * bright, octaves);
                        if (f > 7000.0f) amp *= 7000.0f / f * 7000.0f / f;   // real bars lose their top quickly
                        modes[(size_t) numModes++].set (f, t60 * decayScale * table[i].d, amp, sr);
                    }
                    // mallet contact time: soft felt ~0.6 ms, hard mallet ~0.15 ms (a Hann pulse, never a raw impulse)
                    exciteLen = juce::jmax (4, (int) ((0.15f + 0.45f * (1.0f - bright)) * (0.6f + 0.4f * malletMs) * 0.001f * sr));
                    exciteLeft = exciteLen;
                    malletNoise = model == Model::glass ? 0.0f : 0.15f * bright;
                    gain = 0.16f * velGain;
                    break;
                }

                case Model::pluck:
                    for (int st = 0; st < 2; ++st)
                    {
                        auto& str = strings[(size_t) st];
                        str.tune (freq * (st == 0 ? 0.9991f : 1.0009f), t60 * 0.7f, bright, sr);   // two strings, gentle chorus
                        const int len = str.len;
                        const int pick = juce::jmax (1, len / 7);   // pick position: 1/7 of the string
                        float lp = 0.0f;
                        for (int i = 0; i < len; ++i)
                        {
                            lp += (rng.nextFloat() * 2.0f - 1.0f - lp) * (0.12f + 0.4f * bright);
                            burst[(size_t) i] = lp;
                        }
                        for (int i = 0; i < len; ++i)
                            str.excite (i, 0.7f * (burst[(size_t) i] - (i >= pick ? burst[(size_t) (i - pick)] : 0.0f)));
                    }
                    bodyLp = 0.0f; pluckLp = pluckLp2 = 0.0f;
                    {
                        const float fc = juce::jmin (0.42f * sr, (1800.0f + 5000.0f * bright) * (1.0f + freq / 1500.0f));
                        pluckCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * fc / sr);
                    }
                    gain = 0.28f * velGain;
                    break;

                case Model::epiano:
                    // less FM and a softer tine up high, so nothing folds back above 20 kHz
                    fmIndex = (0.8f + 2.6f * vel * vel) * (0.5f + bright) * juce::jlimit (0.15f, 1.0f, 700.0f / freq);
                    fmIndexMul = std::exp (-1.0f / (0.35f * sr));
                    tineEnv = 0.35f * vel * (0.4f + bright) * juce::jlimit (0.0f, 1.0f, (6000.0f - freq * 4.0f) / 4000.0f);
                    tineInc = juce::jmin (freq * 14.0f, 7000.0f) / sr;
                    tineMul = std::exp (-1.0f / (0.025f * sr));
                    envMul = std::exp (-6.9078f / (t60 * 1.2f * sr));
                    gain = 0.2f * velGain;
                    break;

                case Model::flute:
                    attackInc = 1.0f / (0.045f * sr);
                    envMul = std::exp (-6.9078f / (t60 * 2.0f * sr));
                    svf[0].reset();
                    svf[0].set (freq, 6.0f, sr);
                    gain = 0.26f * velGain;
                    break;

                case Model::strings:
                    attackInc = 1.0f / (0.07f * sr);
                    envMul = std::exp (-6.9078f / (t60 * 2.2f * sr));
                    svf[0].reset(); svf[1].reset();
                    svf[0].set (600.0f + freq * 2.0f + 5000.0f * bright, 0.7f, sr);
                    svf[1].set (600.0f + freq * 2.0f + 5000.0f * bright, 0.7f, sr);
                    gain = 0.3f * velGain;
                    break;
            }
        }

        inline float next (float pm, juce::Random& rng) noexcept
        {
            const auto& sn = sine();
            switch (model)
            {
                case Model::chime:
                {
                    if (filterCounter-- <= 0)
                    {
                        filterCounter = 16;
                        const float cutoff = filterBase + filterSweep * filterEnv;
                        svf[0].set (cutoff, 0.9f, sr);
                        svf[1].set (cutoff * 1.02f, 0.9f, sr);
                    }
                    float out = 0.0f;
                    for (int o = 0; o < 2; ++o)
                    {
                        const float dt = freq * pm * (o == 0 ? 1.0f : detune) / sr;
                        float& p = phase[(size_t) o];
                        const float pulse = (p < 0.4f ? 1.0f : -1.0f) + 0.2f + polyBlep (p, dt) - polyBlep (wrap (p + 0.6f), dt);   // +0.2 removes the DC of a 40 % pulse
                        const float tri = 1.0f - 4.0f * std::abs (p - 0.5f);
                        p = wrap (p + dt);
                        float bp;
                        out += svf[(size_t) o].lp (0.55f * pulse + 0.45f * tri, bp);
                    }
                    filterEnv *= filterEnvMul;
                    const float bell = sn (phase[2]) * 0.18f * filterEnv;   // octave sparkle on the attack
                    phase[2] = wrap (phase[2] + 2.0f * freq * pm / sr);
                    return out * 0.5f + bell;
                }
                case Model::celesta: case Model::vibes: case Model::glass: case Model::marimba: case Model::steel:
                {
                    float x = 0.0f;
                    if (exciteLeft > 0)
                    {
                        // impulse strike plus a very short mallet contact noise
                        {
                            // Hann-shaped contact force, area 1: low modes keep their level, the top is rolled off
                            const int k = exciteLen - exciteLeft;
                            const float ph = ((float) k + 0.5f) / (float) exciteLen;
                            x = (1.0f - std::cos (juce::MathConstants<float>::twoPi * ph)) / (float) exciteLen;
                        }
                        malletLp += ((rng.nextFloat() * 2.0f - 1.0f) - malletLp) * 0.3f;
                        x += malletLp * malletNoise * 0.25f * (float) exciteLeft / ((float) exciteLen * (float) exciteLen);
                        --exciteLeft;
                    }
                    float y = 0.0f;
                    for (int i = 0; i < numModes; ++i) y += modes[(size_t) i].process (x);
                    if (tremolo > 0.0f)
                    {
                        y *= 1.0f - tremolo * (0.5f + 0.5f * sn (phase[3]));
                        phase[3] = wrap (phase[3] + 5.5f / sr);
                    }
                    return y;
                }
                case Model::pluck:
                {
                    const float y = strings[0].process() + strings[1].process();
                    pluckLp += (y - pluckLp) * pluckCoef;           // 2-pole body low-pass: warm, no fizz
                    pluckLp2 += (pluckLp - pluckLp2) * pluckCoef;
                    return pluckLp2;
                }
                case Model::epiano:
                {
                    const float dt = freq * pm / sr;
                    const float car = sn (wrap (phase[0] + sn (phase[1]) * fmIndex * 0.159f));
                    const float tine = sn (phase[2]) * tineEnv;
                    phase[0] = wrap (phase[0] + dt);
                    phase[1] = wrap (phase[1] + dt);
                    phase[2] = wrap (phase[2] + tineInc * pm);
                    fmIndex *= fmIndexMul;
                    tineEnv *= tineMul;
                    return car + tine;
                }
                case Model::flute:
                {
                    const float dt = freq * pm / sr;
                    const float tone = sn (phase[0]) + 0.16f * sn (phase[1]) + 0.05f * sn (phase[2]);
                    phase[0] = wrap (phase[0] + dt);
                    phase[1] = wrap (phase[1] + 2.0f * dt);
                    phase[2] = wrap (phase[2] + 3.0f * dt);
                    float bp;
                    svf[0].lp (rng.nextFloat() * 2.0f - 1.0f, bp);
                    return tone * 0.8f + bp * (0.2f * (1.0f - attack) + 0.025f);
                }
                case Model::strings:
                {
                    float saw = 0.0f;
                    for (int v = 0; v < 3; ++v)
                    {
                        const float dt = freq * pm * (1.0f + (v - 1) * 0.0045f) / sr;
                        float& p = phase[(size_t) v];
                        saw += 2.0f * p - 1.0f - polyBlep (p, dt);
                        p = wrap (p + dt);
                    }
                    float bp;
                    return svf[1].lp (svf[0].lp (saw * 0.33f, bp), bp);
                }
            }
            return 0.0f;
        }

        float sr = 44100.0f;
        bool active = false, pending = false;
        Args pendingArgs {};
        int note = -1;
        juce::int64 age = 0;
        Model model = Model::chime;
        float freq = 440.0f, vel = 1.0f, gain = 0.0f, panL = 0.7f, panR = 0.7f, level = 0.0f;
        float attack = 1.0f, attackInc = 1.0f, env = 1.0f, envMul = 1.0f;
        int fadeOut = 1, fadeOutLeft = 0, filterCounter = 0;
        std::array<float, 4> phase {};

        float detune = 1.0f, filterBase = 1000.0f, filterSweep = 0.0f, filterEnv = 0.0f, filterEnvMul = 1.0f;
        std::array<Svf, 2> svf;
        std::array<Mode, 8> modes;
        int numModes = 0, exciteLeft = 0, exciteLen = 1;
        float malletNoise = 0.0f, tremolo = 0.0f, malletLp = 0.0f, tineInc = 0.0f, pluckLp = 0.0f, pluckLp2 = 0.0f, pluckCoef = 0.5f;
        std::array<String, 2> strings;
        std::vector<float> burst;
        float bodyLp = 0.0f;
       #ifdef HAMA_PROBE
        float pr1 = 0, pr2 = 0, prEnv = 1e-6f;
       #endif
        float fmIndex = 0, fmIndexMul = 1, tineEnv = 0, tineMul = 1;
    };
}
