#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// Harp voices: additive / FM / plucked-string synthesis, no samples.
namespace harp
{
    enum class Model { chime, celesta, vibes, glass, pluck, flute, marimba, steel, epiano, strings };

    inline const juce::StringArray& voiceNames()
    {
        static const juce::StringArray names { "Classic Chime", "Celesta", "Vibes", "Glass", "Pluck",
                                               "Flute", "Marimba", "Steel", "E-Piano", "Strings" };
        return names;
    }

    // shared sine table
    struct SineTable
    {
        static constexpr int size = 4096;
        std::array<float, size + 1> data {};
        SineTable()
        {
            for (int i = 0; i <= size; ++i)
                data[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * i / size);
        }
        // phase in cycles [0, 1)
        inline float operator() (float phase) const noexcept
        {
            const float p = phase * (float) size;
            const int i = (int) p;
            const float f = p - (float) i;
            return data[(size_t) i] + f * (data[(size_t) i + 1] - data[(size_t) i]);
        }
    };

    inline const SineTable& sine()
    {
        static const SineTable table;
        return table;
    }

    inline float wrap (float p) noexcept { return p - std::floor (p); }

    struct Partial { float ratio, amp, decay; };

    struct Timbre
    {
        int numPartials;
        std::array<Partial, 8> partials;
        float attackMs;
        float decayScale;   // multiplies the Decay knob
        float noise;        // attack click / breath
        float sustainNoise; // continuous breath (flute)
        float gain;
    };

    inline const Timbre& timbre (Model m)
    {
        static const std::array<Timbre, 10> t {{
            // chime: bright bell-like harp, the classic sound
            { 5, {{ { 1.0f, 1.0f, 1.0f }, { 2.0f, 0.42f, 0.55f }, { 3.0f, 0.16f, 0.38f }, { 4.2f, 0.07f, 0.22f }, { 6.0f, 0.03f, 0.12f } }}, 1.5f, 1.0f, 0.05f, 0.0f, 0.9f },
            // celesta
            { 4, {{ { 1.0f, 1.0f, 1.0f }, { 2.0f, 0.12f, 0.6f }, { 4.0f, 0.32f, 0.28f }, { 10.0f, 0.08f, 0.1f } }}, 0.8f, 0.8f, 0.04f, 0.0f, 0.9f },
            // vibes
            { 3, {{ { 1.0f, 1.0f, 1.0f }, { 4.0f, 0.22f, 0.32f }, { 10.0f, 0.05f, 0.1f } }}, 2.5f, 1.3f, 0.02f, 0.0f, 1.0f },
            // glass
            { 4, {{ { 1.0f, 1.0f, 1.0f }, { 2.76f, 0.45f, 0.6f }, { 5.4f, 0.26f, 0.42f }, { 8.93f, 0.12f, 0.28f } }}, 4.0f, 1.2f, 0.03f, 0.0f, 0.8f },
            // pluck (Karplus-Strong, partials unused)
            { 0, {{}}, 0.5f, 0.6f, 0.0f, 0.0f, 1.1f },
            // flute
            { 3, {{ { 1.0f, 1.0f, 1.0f }, { 2.0f, 0.22f, 1.0f }, { 3.0f, 0.1f, 1.0f } }}, 70.0f, 2.2f, 0.0f, 0.06f, 0.85f },
            // marimba
            { 3, {{ { 1.0f, 1.0f, 1.0f }, { 4.0f, 0.38f, 0.16f }, { 9.9f, 0.1f, 0.06f } }}, 1.0f, 0.35f, 0.06f, 0.0f, 1.1f },
            // steel drum
            { 4, {{ { 1.0f, 1.0f, 1.0f }, { 2.0f, 0.55f, 0.7f }, { 3.0f, 0.28f, 0.5f }, { 4.95f, 0.18f, 0.3f } }}, 2.0f, 0.75f, 0.03f, 0.0f, 0.8f },
            // e-piano (FM, partials unused)
            { 0, {{}}, 1.5f, 1.2f, 0.0f, 0.0f, 0.9f },
            // strings
            { 8, {{ { 1.0f, 1.0f, 1.0f }, { 1.004f, 0.7f, 1.0f }, { 2.0f, 0.5f, 1.0f }, { 3.0f, 0.33f, 1.0f },
                    { 4.0f, 0.25f, 1.0f }, { 5.0f, 0.2f, 1.0f }, { 6.0f, 0.16f, 1.0f }, { 7.0f, 0.12f, 1.0f } }}, 140.0f, 2.6f, 0.0f, 0.0f, 0.55f },
        }};
        return t[(size_t) m];
    }

    struct Settings
    {
        Model model = Model::chime;
        float tone = 0.6f;     // 0..1 brightness
        float decay = 1.8f;    // seconds
    };

    class Voice
    {
    public:
        bool isActive() const noexcept { return active; }
        int getNote() const noexcept   { return note; }
        juce::int64 getAge() const noexcept { return age; }

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;
            ks.assign ((size_t) (sampleRate / 20.0) + 4, 0.0f);
        }

        void start (int midiNote, float velocity, float pan, const Settings& s, juce::Random& rng, float hz)
        {
            note = midiNote;
            model = s.model;
            active = true;
            releasing = false;
            age = 0;
            vel = velocity;
            freq = hz;

            const auto& t = timbre (model);
            const float brightness = juce::jmap (juce::jlimit (0.0f, 1.0f, s.tone * (0.6f + 0.4f * velocity)), 0.3f, 1.7f);
            const float sr = (float) sampleRate;
            const float baseDecay = juce::jmax (0.05f, s.decay * t.decayScale);

            numPartials = t.numPartials;
            for (int i = 0; i < numPartials; ++i)
            {
                const auto& p = t.partials[(size_t) i];
                const float f = freq * p.ratio;
                inc[(size_t) i] = f / sr;
                phase[(size_t) i] = rng.nextFloat();
                const float octaves = std::log2 (juce::jmax (1.0f, p.ratio));
                float amp = p.amp * std::pow (brightness, octaves);
                if (f > sr * 0.45f) amp = 0.0f;   // no aliasing
                env[(size_t) i] = amp;
                // higher notes ring a little shorter, like a real struck string
                const float noteScale = juce::jlimit (0.35f, 1.4f, std::pow (2.0f, (60.0f - (float) midiNote) / 36.0f));
                const float t60 = baseDecay * p.decay * noteScale;
                mul[(size_t) i] = std::exp (-6.9f / (t60 * sr));
            }

            attackInc = 1.0f / juce::jmax (1.0f, t.attackMs * 0.001f * sr);
            attack = 0.0f;
            noiseEnv = t.noise * (0.5f + velocity);
            noiseMul = std::exp (-1.0f / (0.004f * sr));
            breath = t.sustainNoise;
            gain = t.gain * (0.25f + 0.75f * velocity) * 0.22f;
            panL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            panR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            lpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmap (s.tone, 1500.0f, 16000.0f) / sr);
            lpState = 0.0f;
            noiseLp = 0.0f;
            releaseMul = 1.0f;

            if (model == Model::pluck)
            {
                ksLength = juce::jlimit (2, (int) ks.size() - 1, (int) std::round (sr / freq));
                ksPos = 0;
                const float bright = juce::jlimit (0.1f, 1.0f, s.tone);
                float lp = 0.0f;
                for (int i = 0; i < ksLength; ++i)
                {
                    const float n = rng.nextFloat() * 2.0f - 1.0f;
                    lp += (n - lp) * (0.2f + 0.8f * bright);
                    ks[(size_t) i] = lp;
                }
                const float t60 = juce::jmax (0.1f, s.decay * t.decayScale);
                ksFeedback = std::pow (0.001f, 1.0f / (t60 * freq));
                ksFeedback = juce::jmin (ksFeedback, 0.9995f);
                ksDamp = 0.5f - 0.35f * bright;
                ksLevel = 1.0f;
            }
            else if (model == Model::epiano)
            {
                fmPhaseC = fmPhaseM = fmPhaseT = 0.0f;
                fmIncC = freq / sr;
                fmIncT = juce::jmin (0.45f, freq * 14.0f / sr);
                fmIndex = (1.2f + 2.2f * velocity) * juce::jmap (s.tone, 0.4f, 1.2f);
                fmIndexMul = std::exp (-1.0f / (0.25f * sr));
                const float t60 = juce::jmax (0.1f, s.decay * t.decayScale) * juce::jlimit (0.35f, 1.4f, std::pow (2.0f, (60.0f - (float) midiNote) / 30.0f));
                fmEnv = 1.0f;
                fmEnvMul = std::exp (-6.9f / (t60 * sr));
                tineEnv = 0.25f * velocity;
                tineMul = std::exp (-1.0f / (0.03f * sr));
            }
        }

        // fade out quickly (voice stealing / panic)
        void release (float seconds = 0.006f)
        {
            if (! active) return;
            releasing = true;
            releaseMul = std::exp (-6.9f / (juce::jmax (0.001f, seconds) * (float) sampleRate));
        }

        // adds into the stereo buffer; pitchMul per sample (vibrato)
        void render (float* left, float* right, int numSamples, const float* pitchMul, juce::Random& rng)
        {
            if (! active) return;

            const auto& sine = harp::sine();
            float peak = 0.0f;

            for (int n = 0; n < numSamples; ++n)
            {
                const float pm = pitchMul[n];
                float s = 0.0f;

                if (model == Model::pluck)
                {
                    const int next = ksPos + 1 >= ksLength ? 0 : ksPos + 1;
                    const float a = ks[(size_t) ksPos], b = ks[(size_t) next];
                    const float y = (a * (1.0f - ksDamp) + b * ksDamp) * ksFeedback;
                    ks[(size_t) ksPos] = y;
                    ksPos = next;
                    s = a * 1.4f;
                    ksLevel *= 0.99999f;
                    peak = juce::jmax (peak, std::abs (a));
                }
                else if (model == Model::epiano)
                {
                    const float mod = sine (fmPhaseM) * fmIndex;
                    s = sine (wrap (fmPhaseC + mod * 0.159f)) * fmEnv + sine (fmPhaseT) * tineEnv;
                    fmPhaseC = wrap (fmPhaseC + fmIncC * pm);
                    fmPhaseM = wrap (fmPhaseM + fmIncC * pm);
                    fmPhaseT = wrap (fmPhaseT + fmIncT * pm);
                    fmIndex *= fmIndexMul;
                    fmEnv *= fmEnvMul;
                    tineEnv *= tineMul;
                    peak = juce::jmax (peak, fmEnv);
                }
                else
                {
                    for (int i = 0; i < numPartials; ++i)
                    {
                        s += sine (phase[(size_t) i]) * env[(size_t) i];
                        phase[(size_t) i] = wrap (phase[(size_t) i] + inc[(size_t) i] * pm);
                        env[(size_t) i] *= mul[(size_t) i];
                    }
                    peak = juce::jmax (peak, env[0] + (numPartials > 1 ? env[1] : 0.0f));
                }

                // attack click / breath noise
                if (noiseEnv > 1.0e-4f || breath > 0.0f)
                {
                    const float wn = rng.nextFloat() * 2.0f - 1.0f;
                    noiseLp += (wn - noiseLp) * 0.35f;
                    s += noiseLp * (noiseEnv + breath * (numPartials > 0 ? env[0] : 0.0f));
                    noiseEnv *= noiseMul;
                }

                if (attack < 1.0f) attack = juce::jmin (1.0f, attack + attackInc);
                lpState += (s - lpState) * lpCoef;

                float out = lpState * attack * gain;
                if (releasing) { gain *= releaseMul; }

                left[n]  += out * panL;
                right[n] += out * panR;
            }

            age += numSamples;
            if (peak < 1.0e-4f || (releasing && gain < 1.0e-5f))
                active = false;
        }

    private:
        double sampleRate = 44100.0;
        bool active = false, releasing = false;
        int note = -1;
        juce::int64 age = 0;
        Model model = Model::chime;
        float vel = 1.0f, freq = 440.0f;

        int numPartials = 0;
        std::array<float, 8> phase {}, inc {}, env {}, mul {};
        float attack = 1.0f, attackInc = 1.0f, noiseEnv = 0.0f, noiseMul = 0.0f, breath = 0.0f, noiseLp = 0.0f;
        float gain = 0.0f, panL = 0.7f, panR = 0.7f, lpCoef = 1.0f, lpState = 0.0f, releaseMul = 1.0f;

        // pluck
        std::vector<float> ks;
        int ksLength = 2, ksPos = 0;
        float ksFeedback = 0.99f, ksDamp = 0.5f, ksLevel = 1.0f;

        // e-piano
        float fmPhaseC = 0, fmPhaseM = 0, fmPhaseT = 0, fmIncC = 0, fmIncT = 0, fmIndex = 0, fmIndexMul = 1, fmEnv = 0, fmEnvMul = 1, tineEnv = 0, tineMul = 1;
    };
}
