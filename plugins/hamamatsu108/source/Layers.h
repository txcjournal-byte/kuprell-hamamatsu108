#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "HarpVoice.h"
#include <array>
#include <cmath>

// Chord layer (Organ, Pad, Strings, Choir) and bass (Sub, Organ, Synth): sustained voices, pure synthesis.
namespace layers
{
    enum class Kind { off, organ, pad, strings, choir, bassSub, bassOrgan, bassSynth };

    inline const juce::StringArray& chordTypeNames() { static const juce::StringArray n { "Off", "Organ", "Pad", "Strings", "Choir" }; return n; }
    inline const juce::StringArray& bassTypeNames()  { static const juce::StringArray n { "Off", "Sub", "Organ", "Synth" }; return n; }
    inline const juce::StringArray& bassModeNames()  { static const juce::StringArray n { "Root", "Root + 5th", "Walking" }; return n; }
    inline const juce::StringArray& voicingNames()   { static const juce::StringArray n { "Auto (voice leading)", "Root position", "1st inversion", "2nd inversion", "3rd inversion" }; return n; }

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
        // returns lowpass; band in bp
        float process (float x, float& bp)
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            bp = v1;
            return v2;
        }
        void reset() { ic1 = ic2 = 0; }
    };

    inline float polyBlep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    class SustainVoice
    {
    public:
        bool isActive() const noexcept { return active; }
        bool isReleasing() const noexcept { return releasing; }
        int getNote() const noexcept { return note; }

        void prepare (double newSr) { sr = (float) newSr; }

        void start (int midiNote, float velocity, Kind k, juce::Random& rng)
        {
            kind = k;
            note = midiNote;
            freq = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
            vel = 0.4f + 0.6f * velocity;
            active = true;
            releasing = false;
            if (env < 0.0001f) { for (auto& p : ph) p = rng.nextFloat(); lp.reset(); f1.reset(); f2.reset(); }

            float attackS = 0.01f, releaseS = 0.1f;
            switch (kind)
            {
                case Kind::organ:     attackS = 0.008f; releaseS = 0.08f; break;
                case Kind::pad:       attackS = 0.7f;   releaseS = 1.4f;  break;
                case Kind::strings:   attackS = 0.28f;  releaseS = 0.8f;  break;
                case Kind::choir:     attackS = 0.45f;  releaseS = 1.0f;  break;
                case Kind::bassSub:   attackS = 0.004f; releaseS = 0.12f; break;
                case Kind::bassOrgan: attackS = 0.006f; releaseS = 0.08f; break;
                case Kind::bassSynth: attackS = 0.003f; releaseS = 0.1f;  break;
                case Kind::off: break;
            }
            attackInc = 1.0f / (attackS * sr);
            releaseMul = std::exp (-6.9f / (releaseS * sr));
            synthEnv = 1.0f;
            synthEnvMul = std::exp (-1.0f / (0.18f * sr));
        }

        void release() { releasing = true; }
        void kill()    { active = false; env = 0.0f; }

        // tone: 0..1 (filter / brightness)
        void render (float* left, float* right, int n, float tone, float level, const float* pitchMul)
        {
            if (! active) return;
            const auto& sine = harp::sine();

            float cutoff = 0.0f, q = 0.7f;
            switch (kind)
            {
                case Kind::pad:       cutoff = 300.0f * std::pow (40.0f, tone); q = 0.9f; break;
                case Kind::strings:   cutoff = 500.0f * std::pow (24.0f, tone); q = 0.7f; break;
                case Kind::choir:     cutoff = 900.0f * std::pow (10.0f, tone); q = 0.6f; break;
                case Kind::bassSynth: cutoff = 120.0f * std::pow (25.0f, tone); q = 1.6f; break;
                default: break;
            }
            if (cutoff > 0.0f && kind != Kind::bassSynth) lp.set (cutoff, q, sr);
            if (kind == Kind::choir) { f1.set (freq < 300 ? 650.0f : 800.0f, 6.0f, sr); f2.set (1150.0f, 7.0f, sr); }

            const float dt = freq / sr;
            for (int i = 0; i < n; ++i)
            {
                if (releasing) env *= releaseMul;
                else env = juce::jmin (1.0f, env + attackInc);

                const float pm = pitchMul[i];
                float s = 0.0f;
                switch (kind)
                {
                    case Kind::organ:
                    {
                        // drawbars 8' 4' 2 2/3' 2' + a touch of 16'
                        s = sine (ph[0]) * 0.8f + sine (ph[1]) * 0.6f + sine (ph[2]) * 0.4f * tone + sine (ph[3]) * 0.35f * tone + sine (ph[4]) * 0.25f;
                        ph[0] = harp::wrap (ph[0] + dt * pm);
                        ph[1] = harp::wrap (ph[1] + dt * 2.0f * pm);
                        ph[2] = harp::wrap (ph[2] + dt * 3.0f * pm);
                        ph[3] = harp::wrap (ph[3] + dt * 4.0f * pm);
                        ph[4] = harp::wrap (ph[4] + dt * 0.5f * pm);
                        s *= 0.45f;
                        break;
                    }
                    case Kind::pad:
                    case Kind::strings:
                    case Kind::choir:
                    {
                        const float det = kind == Kind::pad ? 0.0045f : kind == Kind::strings ? 0.006f : 0.003f;
                        float saw = 0.0f;
                        for (int v = 0; v < 3; ++v)
                        {
                            const float d = dt * pm * (1.0f + (v - 1) * det);
                            saw += 2.0f * ph[(size_t) v] - 1.0f - polyBlep (ph[(size_t) v], d);
                            ph[(size_t) v] = harp::wrap (ph[(size_t) v] + d);
                        }
                        saw *= 0.33f;
                        float bp;
                        s = lp.process (saw, bp);
                        if (kind == Kind::choir)
                        {
                            float b1, b2;
                            f1.process (s, b1);
                            f2.process (s, b2);
                            s = b1 * 1.4f + b2 * 0.9f + s * 0.15f;
                        }
                        s *= 0.6f;
                        break;
                    }
                    case Kind::bassSub:
                        s = sine (ph[0]) + 0.18f * sine (ph[1]);
                        ph[0] = harp::wrap (ph[0] + dt);
                        ph[1] = harp::wrap (ph[1] + dt * 2.0f);
                        s *= 0.8f;
                        break;
                    case Kind::bassOrgan:
                        s = 0.7f * sine (ph[0]) + 0.5f * sine (ph[1]) + 0.3f * sine (ph[2]);
                        ph[0] = harp::wrap (ph[0] + dt);
                        ph[1] = harp::wrap (ph[1] + dt * 2.0f);
                        ph[2] = harp::wrap (ph[2] + dt * 3.0f);
                        s *= 0.7f;
                        break;
                    case Kind::bassSynth:
                    {
                        const float saw = 2.0f * ph[0] - 1.0f - polyBlep (ph[0], dt);
                        ph[0] = harp::wrap (ph[0] + dt);
                        if ((i & 15) == 0) lp.set (cutoff * (1.0f + 4.0f * synthEnv), q, sr);
                        float bp;
                        s = lp.process (saw, bp) * 0.8f;
                        synthEnv *= synthEnvMul;
                        break;
                    }
                    case Kind::off: break;
                }

                const float out = s * env * vel * level;
                left[i] += out;
                right[i] += out;
            }

            if (releasing && env < 1.0e-4f) { active = false; env = 0.0f; }
        }

    private:
        float sr = 44100.0f;
        Kind kind = Kind::off;
        bool active = false, releasing = false;
        int note = -1;
        float freq = 100.0f, vel = 1.0f, env = 0.0f, attackInc = 0.01f, releaseMul = 0.99f, synthEnv = 0.0f, synthEnvMul = 1.0f;
        std::array<float, 5> ph {};
        Svf lp, f1, f2;
    };
}
