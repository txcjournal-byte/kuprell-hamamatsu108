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
    enum class Model { chime, celesta, vibes, glass, pluck, flute, marimba, steel, epiano, strings,
                       analog, wavetable, fm, air,   // synthesis engines (layer B and modern presets)
                       harp };                       // the physical harp string: exciter, two strings, a resonating body
    constexpr int numModels = 15;
    inline bool isSynthEngine (int m) { return m >= 10 && m <= 13; }

    inline const juce::StringArray& voiceNames()
    {
        static const juce::StringArray names { "Classic Chime", "Celesta", "Vibes", "Glass", "Pluck",
                                               "Flute", "Marimba", "Steel", "E-Piano", "Strings",
                                               "Analog", "Wavetable", "Digital FM", "Air", "Harp String" };
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
        float attack = 0.0015f;  // seconds: fade-in of every note (long = soft swell)
        float sustain = 0.0f;    // 0..1: how much longer the string holds on (up to 6x the decay)
        float shape = 0.4f;      // 0..1: engine character (analog detune, wavetable position, FM ratio, air breath)
        float pitchMul = 1.0f;   // layer octave / detune
        float level = 1.0f;      // layer level
        float stereo = 1.0f;     // 0 = mono voice, 1 = the engine's own stereo image
        float bend = 0.0f;       // semitones the string starts away from its pitch (slides into tune)
        float bendTime = 0.1f;   // seconds of that slide
        float pick = -1.0f;      // pick position along the string (0..0.5), -1 = the engine's own
        // HARP STRING: what touches the string, where, and the body it sits on
        int exciter = 1;         // 0 finger, 1 nail, 2 pick, 3 felt, 4 bow
        float hard = 0.5f;       // 0 soft .. 1 hard touch
        float pos = 0.3f;        // 0 at the end (thin) .. 1 the middle (round)
        int bodyMat = 0;         // 0 wood, 1 glass, 2 steel, 3 stone
        float bodySize = 0.5f, bodyDamp = 0.3f, bodyRing = 0.4f, bodyMix = 0.35f;
        // CONSOLE: the pluck envelope (a filter that opens and closes with the note), hold, buzz, harmonics
        std::array<float, 8> penv { 0.0f, 1.0f, 0.3f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f };   // T1 L1 T2 L2 T3 L3 T4 Tail (0..1)
        float penvVelo = 0.0f;   // -1..1
        float hold = 0.0f;       // seconds at full level before the decay
        float buzz = 0.0f;       // a buzzing bridge 0..1
        float harmonic = 0.0f;   // 0..1: the octave flageolet rises over the fundamental
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
        int key = -1;   // the key that started this string (layers and switch voices share it)
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
                side = 0.0f;
                // analog drift: every voice wanders by a few cents, slowly, like real oscillators and strings
                driftPhase += driftInc;
                if (driftPhase >= 1.0f) driftPhase -= 1.0f;
                const float drift = 1.0f + driftDepth * sine() (driftPhase);
                float s = next (pitchMul[n] * drift * bendMul, rng);
                if (bendMul != 1.0f) { bendMul = 1.0f + (bendMul - 1.0f) * bendCoef; if (std::abs (bendMul - 1.0f) < 1.0e-5f) bendMul = 1.0f; }
                if (penvOn)   // the pluck envelope: a lowpass that follows T1..T4 / L1..L3 / Tail
                {
                    if (penvStage < 4)
                    {
                        const float u = (float) ++penvPos / (float) penvTimes[(size_t) penvStage];
                        penvValue = penvFrom + (penvLevels[(size_t) penvStage] - penvFrom) * juce::jmin (1.0f, u);
                        if (u >= 1.0f) { penvFrom = penvLevels[(size_t) penvStage]; ++penvStage; penvPos = 0; }
                    }
                    const float fc = 120.0f * std::exp2 (penvValue * 7.8f);
                    const float k = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (fc, 0.45f * sr) / sr);
                    penvLp += (s - penvLp) * k;
                    penvLpR += (side - penvLpR) * k;
                    s = penvLp; side = penvLpR;
                }
                if (buzzAmt > 0.001f)   // the bridge buzzes: an uneven clip that grows with the level
                {
                    const float x = s * (2.0f + 10.0f * buzzAmt);
                    s += buzzAmt * 0.25f * (std::tanh (x + 0.6f) - std::tanh (0.6f) - std::tanh (x) * 0.9f);
                }
                if (attack < 1.0f) attack = juce::jmin (1.0f, attack + attackInc);
                if (holdLeft > 0) --holdLeft; else env *= envMul;
                const float amp = attack * env * attackGain;
                s *= amp;
                float d = side * amp * stereo;

                if (fadeOutLeft > 0)
                {
                    s *= (float) fadeOutLeft / (float) fadeOut;
                    d *= (float) fadeOutLeft / (float) fadeOut;
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
                const float dd = d * gain;
                left[n]  += (out + dd) * panL;
                right[n] += (out - dd) * panR;
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
            freq = a.hz * a.s.pitchMul;
            bendMul = a.s.bend != 0.0f ? std::exp2 (a.s.bend / 12.0f) : 1.0f;
            bendCoef = std::exp (-1.0f / (juce::jmax (0.005f, a.s.bendTime) * 0.3f * sr));
            vel = a.velocity;
            const float tone = juce::jlimit (0.0f, 1.0f, a.s.tone);
            const float bright = juce::jlimit (0.0f, 1.0f, tone * (0.55f + 0.45f * vel));
            const float noteScale = juce::jlimit (0.3f, 1.6f, std::pow (2.0f, (60.0f - (float) note) / 30.0f));
            // a slow attack keeps the string alive while it swells in (otherwise it would decay before it is heard)
            const float t60 = (juce::jmax (0.08f, a.s.decay) * noteScale + 3.0f * juce::jmax (0.0f, a.s.attack - 0.01f)) * (1.0f + 5.0f * juce::jlimit (0.0f, 1.0f, a.s.sustain));

            panL = std::cos ((a.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            panR = std::sin ((a.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            attack = 0.0f;
            attackInc = 1.0f / (juce::jmax (0.0015f, a.s.attack) * sr);
            attackGain = 1.0f + 3.0f * juce::jlimit (0.0f, 1.0f, a.s.attack);   // a swelled note is as loud as a plucked one
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
                    detune = 1.0012f;   // two oscillators 2 cents apart: alive, still in tune
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

                case Model::harp:
                {
                    shape = juce::jlimit (0.0f, 1.0f, a.s.shape);
                    // two strings a hair apart; STIFF (shape) pulls them further and makes the tone metallic
                    const float spreadC = 0.0004f + 0.006f * shape * shape;
                    const float brightS = juce::jlimit (0.0f, 1.0f, bright * (0.7f + 0.5f * a.s.hard) * (1.0f - 0.35f * a.s.bodyDamp));
                    const int ex = juce::jlimit (0, 4, a.s.exciter);
                    // the touch: length of the contact and how much grit is in it
                    static const float exMs[] = { 3.2f, 0.9f, 0.28f, 7.0f, 1.0f };
                    static const float exNoise[] = { 0.15f, 0.45f, 0.75f, 0.0f, 0.9f };
                    const float lenMs = exMs[ex] * (1.4f - 0.8f * a.s.hard);
                    const float noiseMix = juce::jlimit (0.0f, 1.0f, exNoise[ex] * (0.5f + 0.8f * a.s.hard));
                    const float posF = a.s.pick >= 0.0f ? a.s.pick : 0.04f + 0.42f * a.s.pos;
                    for (int st = 0; st < 2; ++st)
                    {
                        auto& str = strings[(size_t) st];
                        const float hmul = (st == 1 && a.s.harmonic > 0.01f) ? 2.0f : 1.0f;   // HARMONIC: the second string sounds the octave flageolet
                        str.tune (freq * hmul * (st == 0 ? 1.0f - spreadC : 1.0f + spreadC), t60 * (st == 0 ? 0.95f : 0.8f), brightS, sr);
                        const int len = str.len;
                        const int pick = juce::jlimit (1, len - 1, (int) ((float) len * posF));
                        const int exLen = juce::jlimit (2, len, (int) (lenMs * 0.001f * sr));
                        float lp = 0.0f;
                        const float lpc = 0.05f + 0.6f * a.s.hard * brightS;
                        for (int i = 0; i < len; ++i)
                        {
                            float v = 0.0f;
                            if (i < exLen)
                            {
                                const float u = (float) i / (float) exLen;
                                const float pulse = std::sin (u * juce::MathConstants<float>::pi);
                                lp += ((rng.nextFloat() * 2.0f - 1.0f) - lp) * lpc;
                                v = pulse * pulse * (1.0f - noiseMix) + lp * noiseMix * 1.6f;
                            }
                            burst[(size_t) i] = v;
                        }
                        const float amp = (ex == 4 ? 0.25f : 1.0f) * (st == 0 ? 0.8f : 0.6f) * (0.4f + 0.6f * vel);
                        for (int i = 0; i < len; ++i)
                            str.excite (i, amp * (burst[(size_t) i] - (i >= pick ? burst[(size_t) (i - pick)] : 0.0f)));
                    }
                    // the body: eight resonances of a material, fixed in pitch like a real instrument
                    static const float ratios[4][8] = { { 1.0f, 1.9f, 2.7f, 3.9f, 5.6f, 8.1f, 11.5f, 16.0f },        // wood: warm, dense
                                                        { 1.0f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f, 24.8f, 31.9f }, // glass: a free bar
                                                        { 1.0f, 1.506f, 2.0f, 2.44f, 3.17f, 3.9f, 4.8f, 5.9f },      // steel: a plate
                                                        { 1.0f, 1.6f, 2.3f, 3.1f, 4.4f, 5.2f, 6.9f, 8.8f } };        // stone: low, heavy
                    static const float baseHz[4] = { 95.0f, 420.0f, 180.0f, 60.0f };
                    static const float matT60[4] = { 0.09f, 1.6f, 1.1f, 0.05f };
                    static const float matTilt[4] = { 0.75f, 0.9f, 0.85f, 0.6f };
                    const int m = juce::jlimit (0, 3, a.s.bodyMat);
                    const float base = baseHz[m] * std::pow (2.0f, (0.5f - a.s.bodySize) * 2.5f);
                    const float ringT = matT60[m] * (0.25f + 2.2f * a.s.bodyRing) * (1.0f - 0.85f * a.s.bodyDamp);
                    numModes = 8;
                    for (int k = 0; k < 8; ++k)
                    {
                        const float f = juce::jmin (0.45f * sr, base * ratios[m][k]);
                        modes[(size_t) k].set (f, ringT * std::pow (0.8f, (float) k), std::pow (matTilt[m], (float) k) * 0.06f, sr);
                    }
                    bodyMixV = juce::jlimit (0.0f, 1.0f, a.s.bodyMix);
                    harmonicMix = juce::jlimit (0.0f, 1.0f, a.s.harmonic);
                    bowLeft = ex == 4 ? (int) ((0.35f + 2.5f * juce::jlimit (0.0f, 1.0f, a.s.sustain)) * sr) : 0;
                    bowAmp = 0.035f * (0.4f + 0.6f * vel);
                    bowLp = 0.0f;
                    envMul = 1.0f;
                    gain = 0.55f * velGain;
                    break;
                }
                case Model::pluck:
                    for (int st = 0; st < 2; ++st)
                    {
                        auto& str = strings[(size_t) st];
                        str.tune (freq * (st == 0 ? 0.9991f : 1.0009f), t60 * 0.7f, bright, sr);   // two strings, gentle chorus
                        const int len = str.len;
                        const int pick = juce::jmax (1, a.s.pick >= 0.0f ? (int) ((float) len * a.s.pick) : len / 7);   // pick position: 1/7 of the string, or set per string
                        float lp = 0.0f;
                        for (int i = 0; i < len; ++i)
                        {
                            lp += (rng.nextFloat() * 2.0f - 1.0f - lp) * (0.12f + 0.4f * bright);
                            burst[(size_t) i] = lp;
                        }
                        for (int i = 0; i < len; ++i)
                            str.excite (i, 0.7f * (burst[(size_t) i] - (i >= pick ? burst[(size_t) (i - pick)] : 0.0f)));
                    }
                    bodyLp = 0.0f; pluckLp = pluckLp2 = 0.0f; sideLp = 0.0f;
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
                    attackInc = juce::jmin (attackInc, 1.0f / (0.045f * sr));
                    envMul = std::exp (-6.9078f / (t60 * 2.0f * sr));
                    svf[0].reset();
                    svf[0].set (freq, 6.0f, sr);
                    gain = 0.26f * velGain;
                    break;

                case Model::analog:
                    shape = juce::jlimit (0.0f, 1.0f, a.s.shape);
                    detune = 0.001f + 0.007f * shape;
                    filterBase = 180.0f + freq * 1.3f + 2500.0f * bright * bright;
                    filterSweep = (900.0f + 7000.0f * bright) * (0.45f + 0.55f * vel);
                    filterEnv = 1.0f;
                    filterEnvMul = std::exp (-1.0f / ((0.07f + 0.22f * t60) * sr));
                    envMul = std::exp (-6.9078f / (t60 * 1.6f * sr));
                    svf[0].reset(); svf[1].reset(); svf[2].reset();
                    gain = 0.24f * velGain;
                    break;

                case Model::wavetable:
                {
                    shape = juce::jlimit (0.0f, 1.0f, a.s.shape);
                    numHarm = juce::jlimit (1, maxHarm, (int) (0.45f * sr / juce::jmax (20.0f, freq)));
                    wtSweep = 0.22f * (0.3f + bright) * (0.5f + 0.5f * vel);   // the table opens on the attack and settles
                    filterEnv = 1.0f;
                    filterEnvMul = std::exp (-1.0f / ((0.12f + 0.3f * t60) * sr));
                    envMul = std::exp (-6.9078f / (t60 * 1.6f * sr));
                    bright_ = bright;
                    for (int h = 0; h < maxHarm; ++h) wtOffset[(size_t) h] = h == 0 ? 0.0f : 0.08f + 0.17f * rng.nextFloat();   // fixed per note
                    updateTable();
                    gain = 0.18f * velGain;
                    break;
                }

                case Model::fm:
                {
                    shape = juce::jlimit (0.0f, 1.0f, a.s.shape);
                    static const float ratios[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 3.5f, 4.0f, 5.0f, 7.0f, 9.0f };
                    fmRatio = ratios[juce::jlimit (0, 9, (int) (shape * 9.99f))];
                    const float limit = juce::jlimit (0.08f, 1.0f, 2600.0f / (freq * fmRatio));   // no fold-over up high
                    fmIndex = (0.6f + 4.5f * bright * (0.4f + 0.6f * vel)) * limit;
                    fmFloor = 0.25f * bright * limit;
                    fmIndexMul = std::exp (-1.0f / ((0.08f + 0.18f * t60) * sr));
                    envMul = std::exp (-6.9078f / (t60 * 1.3f * sr));
                    gain = 0.22f * velGain;
                    break;
                }

                case Model::air:
                {
                    shape = juce::jlimit (0.0f, 1.0f, a.s.shape);
                    attackInc = juce::jmin (attackInc, 1.0f / (0.03f * sr));
                    envMul = std::exp (-6.9078f / (t60 * 2.0f * sr));
                    airQ = 14.0f + 70.0f * (1.0f - shape);             // low shape = clear tone, high = soft breath (never hiss)
                    for (int i = 0; i < 3; ++i)
                    {
                        const float f = juce::jmin (freq * (float) (i + 1), 0.42f * sr);
                        svf[(size_t) i].reset(); svf[(size_t) i].set (f, airQ, sr);
                        svf[(size_t) i + 3].reset(); svf[(size_t) i + 3].set (f, airQ, sr);
                        airNorm[(size_t) i] = std::sqrt (sr / (airQ * f));   // equal loudness at any pitch and breath
                    }
                    airLp = 0.0f;
                    filterCounter = 0;
                    gain = 0.09f * velGain;
                    break;
                }

                case Model::strings:
                    attackInc = juce::jmin (attackInc, 1.0f / (0.07f * sr));
                    envMul = std::exp (-6.9078f / (t60 * 2.2f * sr));
                    svf[0].reset(); svf[1].reset();
                    svf[2].reset();
                    svf[0].set (600.0f + freq * 2.0f + 5000.0f * bright, 0.7f, sr);
                    svf[1].set (600.0f + freq * 2.0f + 5000.0f * bright, 0.7f, sr);
                    svf[2].set (600.0f + freq * 2.0f + 5000.0f * bright, 0.7f, sr);
                    gain = 0.3f * velGain;
                    break;
            }
            gain *= a.s.level;
            // CONSOLE pluck envelope: levels to a cutoff, times to samples
            {
                const float va = juce::jlimit (0.0f, 2.0f, 1.0f + a.s.penvVelo * (vel * 2.0f - 1.0f));
                auto lvl = [va] (float l) { return 1.0f - (1.0f - juce::jlimit (0.0f, 1.0f, l)) * va; };
                auto smp = [this] (float t) { return juce::jmax (1, (int) ((0.002f + 2.5f * t * t) * sr)); };
                penvLevels = { lvl (a.s.penv[1]), lvl (a.s.penv[3]), lvl (a.s.penv[5]), lvl (a.s.penv[7]) };
                penvTimes = { smp (a.s.penv[0]), smp (a.s.penv[2]), smp (a.s.penv[4]), smp (a.s.penv[6]) };
                penvOn = penvLevels[0] < 0.995f || penvLevels[1] < 0.995f || penvLevels[2] < 0.995f || penvLevels[3] < 0.995f;
                penvStage = 0; penvPos = 0; penvFrom = penvLevels[0] * 0.3f + 0.7f; penvValue = penvFrom; penvLp = 0.0f; penvLpR = 0.0f;
                holdLeft = (int) (juce::jmax (0.0f, a.s.hold) * sr);
                buzzAmt = juce::jlimit (0.0f, 1.0f, a.s.buzz);
            }
            driftPhase = rng.nextFloat();
            driftInc = (0.12f + 0.3f * rng.nextFloat()) / sr;
            // synths and the chime drift more, struck bars hardly (they are tuned metal)
            // under a cent: alive, never out of tune (struck bars do not drift at all)
            driftDepth = (isSynthEngine ((int) model) || model == Model::chime || model == Model::strings ? 0.0004f : 0.0f) * (0.6f + 0.8f * rng.nextFloat());
            stereo = juce::jlimit (0.0f, 1.5f, a.s.stereo);
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
                        const float y = svf[(size_t) o].lp (0.55f * pulse + 0.45f * tri, bp);
                        out += y;
                        side += (o == 0 ? 0.22f : -0.22f) * y;   // the two detuned oscillators sit slightly apart
                    }
                    filterEnv *= filterEnvMul;
                    const float bell = sn (phase[2]) * 0.18f * filterEnv;   // octave sparkle on the attack
                    phase[2] = wrap (phase[2] + 2.0f * freq * pm / sr);
                    side *= 0.5f;
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
                case Model::harp:
                {
                    if (bowLeft > 0)   // BOW: the hair keeps feeding the strings while the note lasts
                    {
                        bowLp += ((rng.nextFloat() * 2.0f - 1.0f) - bowLp) * 0.25f;
                        const float fade = juce::jmin (1.0f, (float) bowLeft / (0.2f * sr));
                        strings[0].excite (0, bowLp * bowAmp * fade);
                        strings[1].excite (0, bowLp * bowAmp * 0.7f * fade);
                        --bowLeft;
                    }
                    const float s0 = strings[0].process(), s1 = strings[1].process();
                    const float y = harmonicMix > 0.01f ? s0 * (1.0f - 0.85f * harmonicMix) + s1 * (0.5f + 1.1f * harmonicMix) : s0 + 0.8f * s1;
                    float b = 0.0f;
                    for (int i = 0; i < numModes; ++i) b += modes[(size_t) i].process (y);
                    side = 0.35f * (s0 - s1);
                    return y * (1.0f - 0.55f * bodyMixV) + b * bodyMixV * 2.2f;
                }
                case Model::pluck:
                {
                    const float s0 = strings[0].process(), s1 = strings[1].process();
                    const float y = s0 + s1;
                    pluckLp += (y - pluckLp) * pluckCoef;           // 2-pole body low-pass: warm, no fizz
                    pluckLp2 += (pluckLp - pluckLp2) * pluckCoef;
                    sideLp += ((s0 - s1) - sideLp) * pluckCoef;       // the two strings sit apart
                    side = 0.3f * sideLp;
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
                case Model::analog:
                {
                    if (filterCounter-- <= 0)
                    {
                        filterCounter = 16;
                        const float cutoff = filterBase + filterSweep * filterEnv;
                        svf[0].set (cutoff, 0.75f, sr);
                        svf[1].set (cutoff, 0.95f, sr);
                    }
                    float saw = 0.0f, diff = 0.0f;
                    for (int v = 0; v < 3; ++v)
                    {
                        const float dt = freq * pm * (1.0f + (float) (v - 1) * detune) / sr;
                        float& p = phase[(size_t) v];
                        const float x = 2.0f * p - 1.0f - polyBlep (p, dt);
                        saw += x;
                        diff += v == 0 ? x : (v == 2 ? -x : 0.0f);   // outer saws left and right: a real stereo unison
                        p = wrap (p + dt);
                    }
                    const float sub = sn (phase[3]) * (0.35f - 0.25f * shape);
                    phase[3] = wrap (phase[3] + 0.5f * freq * pm / sr);
                    filterEnv *= filterEnvMul;
                    float bp;
                    if (filterCounter == 16) svf[2].set (filterBase + filterSweep * filterEnv, 0.75f, sr);
                    side = svf[2].lp (diff * 0.36f * (0.4f + 0.6f * shape), bp);
                    return svf[1].lp (svf[0].lp (saw * 0.36f + sub, bp), bp);
                }
                case Model::wavetable:
                {
                    if (filterCounter-- <= 0) { filterCounter = 32; updateTable(); }
                    filterEnv *= filterEnvMul;
                    float y = 0.0f, yR = 0.0f;
                    const float p = phase[0];
                    for (int h = 0; h < numHarm; ++h)
                    {
                        const float ph = p * (float) (h + 1);
                        y  += wtAmp[(size_t) h] * sn (wrap (ph));
                        yR += wtAmp[(size_t) h] * sn (wrap (ph + wtOffset[(size_t) h]));   // same spectrum, other phases: wide, mono-safe fundamental
                    }
                    phase[0] = wrap (p + freq * pm / sr);
                    side = 0.5f * (y - yR);
                    return 0.5f * (y + yR);
                }
                case Model::fm:
                {
                    const float dt = freq * pm / sr;
                    const float idx = fmFloor + fmIndex;
                    const float m = sn (phase[1]);
                    const float c1 = sn (wrap (phase[0] + m * idx * 0.159f));
                    const float c2 = sn (wrap (phase[2] + m * idx * 0.11f));
                    phase[0] = wrap (phase[0] + dt);
                    phase[1] = wrap (phase[1] + dt * fmRatio);
                    phase[2] = wrap (phase[2] + dt * 1.0035f);
                    fmIndex *= fmIndexMul;
                    side = 0.25f * (c1 - c2);
                    return 0.6f * c1 + 0.4f * c2;
                }
                case Model::air:
                {
                    if (filterCounter-- <= 0)
                    {
                        filterCounter = 16;
                        for (int i = 0; i < 3; ++i) { svf[(size_t) i].set (juce::jmin (freq * pm * (float) (i + 1), 0.42f * sr), airQ, sr); svf[(size_t) i + 3] = svf[(size_t) i]; }
                    }
                    const float n = rng.nextFloat() * 2.0f - 1.0f, n2 = rng.nextFloat() * 2.0f - 1.0f;
                    float y = 0.0f, y2 = 0.0f, bp;
                    static const float partial[] = { 1.0f, 0.45f, 0.22f };
                    for (int i = 0; i < 3; ++i)
                    {
                        svf[(size_t) i].lp (n, bp); y += bp * partial[i] * airNorm[(size_t) i];
                        svf[(size_t) i + 3].lp (n2, bp); y2 += bp * partial[i] * airNorm[(size_t) i];   // own noise per side: a wide breath
                    }
                    side = 0.5f * (y - y2);
                    y = 0.5f * (y + y2) * 1.41f;
                    airLp += (n - airLp) * 0.05f;
                    const float tone = sn (phase[0]) * 0.05f * (1.0f - shape);
                    phase[0] = wrap (phase[0] + freq * pm / sr);
                    return y + airLp * 0.02f * shape + tone;
                }
                case Model::strings:
                {
                    float saw = 0.0f, diff = 0.0f;
                    for (int v = 0; v < 3; ++v)
                    {
                        const float dt = freq * pm * (1.0f + (v - 1) * 0.0045f) / sr;
                        float& p = phase[(size_t) v];
                        const float x = 2.0f * p - 1.0f - polyBlep (p, dt);
                        saw += x;
                        diff += v == 0 ? x : (v == 2 ? -x : 0.0f);   // ensemble spread left and right
                        p = wrap (p + dt);
                    }
                    float bp;
                    side = svf[2].lp (diff * 0.2f, bp);
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
        float attack = 1.0f, attackInc = 1.0f, attackGain = 1.0f, env = 1.0f, envMul = 1.0f;
        int fadeOut = 1, fadeOutLeft = 0, filterCounter = 0;
        std::array<float, 4> phase {};

        float detune = 1.0f, filterBase = 1000.0f, filterSweep = 0.0f, filterEnv = 0.0f, filterEnvMul = 1.0f;
        std::array<Svf, 6> svf;
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
        // synthesis engines
        static constexpr int maxHarm = 24;
        std::array<float, maxHarm> wtAmp {}, wtOffset {};
        float bendMul = 1.0f, bendCoef = 0.0f;
        float bodyMixV = 0.0f, bowAmp = 0.0f, bowLp = 0.0f, harmonicMix = 0.0f;
        std::array<float, 4> penvLevels {}; std::array<int, 4> penvTimes {};
        bool penvOn = false; int penvStage = 0, penvPos = 0, holdLeft = 0;
        float penvFrom = 1.0f, penvValue = 1.0f, penvLp = 0.0f, penvLpR = 0.0f, buzzAmt = 0.0f;
        int bowLeft = 0;
        float side = 0.0f, sideLp = 0.0f, stereo = 1.0f, driftPhase = 0.0f, driftInc = 0.0f, driftDepth = 0.0f;
        int numHarm = 1;
        float shape = 0.4f, wtSweep = 0.0f, bright_ = 0.5f, fmRatio = 1.0f, fmFloor = 0.0f, airQ = 20.0f, airLp = 0.0f;
        std::array<float, 3> airNorm {};

        // wavetable: five spectra (sine, warm, saw, hollow square, vocal formant), scanned by SHAPE
        void updateTable() noexcept
        {
            const float pos = juce::jlimit (0.0f, 1.0f, shape + wtSweep * filterEnv) * 4.0f;
            const int f0 = juce::jmin (3, (int) pos);
            const float fr = pos - (float) f0;
            auto amp = [this] (int frame, int h) -> float   // h = 1..
            {
                const float hf = (float) h;
                switch (frame)
                {
                    case 0: return h == 1 ? 1.0f : (h == 2 ? 0.12f : 0.0f);
                    case 1: return 1.0f / (hf * hf);
                    case 2: return 1.0f / hf;
                    case 3: return (h % 2) == 1 ? 1.0f / hf : 0.06f / hf;
                    default:
                    {
                        const float f1 = std::exp (-0.5f * (hf - 3.0f) * (hf - 3.0f)), f2 = 0.6f * std::exp (-0.3f * (hf - 9.0f) * (hf - 9.0f));
                        return (0.5f / hf + f1 + f2);
                    }
                }
            };
            float sum = 0.0f;
            const float tilt = 0.55f + 0.9f * bright_;
            for (int h = 1; h <= numHarm; ++h)
            {
                float a = (1.0f - fr) * amp (f0, h) + fr * amp (f0 + 1, h);
                a *= std::pow (tilt, std::log2 ((float) h) * 0.6f);
                wtAmp[(size_t) h - 1] = a;
                sum += a * a;
            }
            const float norm = 0.75f / std::sqrt (juce::jmax (1.0e-6f, sum));
            for (int h = 0; h < numHarm; ++h) wtAmp[(size_t) h] *= norm;
        }
    };
}
