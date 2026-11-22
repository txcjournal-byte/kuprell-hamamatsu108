#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <array>
#include <cmath>

// HARPOLOGY console effects on the whole instrument: SPACE (rooms), TILT, WOOD, SYMPATHETIC strings
namespace hama::consolefx
{
    struct OnePole
    {
        float z = 0.0f, a = 0.0f;
        void set (float hz, float sr) { a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (5.0f, sr * 0.45f, hz) / sr); }
        float lp (float x) { z += (x - z) * a; return z; }
        float hp (float x) { return x - lp (x); }
    };

    // SPACE: a room after everything. Hall, Plate, Cave, Void; size, tone, pre-delay, mix, width, duck, modulation, low cut, reverse, tail
    struct Space
    {
        struct Settings { int room = 0; float size = 0.5f, tone = 0.5f, pre = 0.1f, mix = 0.0f, width = 0.8f, duck = 0.0f, mod = 0.2f, lowCut = 0.2f; bool freeze = false, reverse = false, tail = true; };

        void prepare (double sampleRate, int)
        {
            sr = (float) sampleRate;
            reverb.setSampleRate (sampleRate);
            pre.assign ((size_t) (sr * 0.6f) + 4, { 0.0f, 0.0f });
            rev.assign ((size_t) (sr * 0.35f) + 4, { 0.0f, 0.0f });
            mod.assign ((size_t) (sr * 0.05f) + 4, { 0.0f, 0.0f });
            reverb.reset();
            prePos = revPos = modPos = 0; duckEnv = 0.0f; silent = 0.0f; tailGain = 1.0f;
        }

        void process (float* L, float* R, int n, const Settings& s)
        {
            if (s.mix < 0.001f && ! s.freeze) { idle = true; return; }
            if (idle) { reverb.reset(); idle = false; }
            // the room
            static const float baseSize[4] = { 0.82f, 0.62f, 0.95f, 0.99f };
            static const float baseDamp[4] = { 0.35f, 0.08f, 0.72f, 0.18f };
            static const float basePre[4] = { 0.0f, 0.0f, 0.045f, 0.02f };
            static const float baseMod[4] = { 0.0f, 0.0f, 0.1f, 0.5f };
            const int room = juce::jlimit (0, 3, s.room);
            juce::Reverb::Parameters p;
            p.roomSize = juce::jlimit (0.0f, 1.0f, baseSize[room] * (0.55f + 0.5f * s.size));
            p.damping = juce::jlimit (0.0f, 1.0f, baseDamp[room] + (0.5f - s.tone) * 0.6f);
            p.wetLevel = 1.0f; p.dryLevel = 0.0f;
            p.width = juce::jlimit (0.0f, 1.0f, s.width);
            p.freezeMode = s.freeze ? 1.0f : 0.0f;
            reverb.setParameters (p);
            const int preLen = juce::jlimit (1, (int) pre.size() - 2, (int) ((basePre[room] + 0.25f * s.pre * s.pre * 2.0f) * sr) + 1);
            const float modDepth = juce::jlimit (0.0f, 1.0f, s.mod + baseMod[room]);
            lowCut.set (20.0f + 780.0f * s.lowCut * s.lowCut, sr); lowCutR.set (20.0f + 780.0f * s.lowCut * s.lowCut, sr);
            toneL.set (1200.0f * std::pow (15.0f, s.tone), sr); toneR.set (1200.0f * std::pow (15.0f, s.tone), sr);
            const float duckRel = std::exp (-1.0f / (0.25f * sr));
            if ((int) wetL.size() < n) { wetL.resize ((size_t) n); wetR.resize ((size_t) n); }
            float level = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float l = lowCut.hp (L[i]), r = lowCutR.hp (R[i]);
                level = juce::jmax (level, std::abs (L[i]) + std::abs (R[i]));
                // pre-delay
                pre[(size_t) prePos] = { l, r };
                const auto& d = pre[(size_t) ((prePos - preLen + (int) pre.size()) % (int) pre.size())];
                prePos = (prePos + 1) % (int) pre.size();
                l = d[0]; r = d[1];
                // REVERSE: the sound fed to the room runs backwards in short pieces, so it swells in
                if (s.reverse)
                {
                    const int len = (int) rev.size() - 2;
                    rev[(size_t) revPos] = { l, r };
                    const int back = (len - 1 - revPos) * 2 % len;
                    const auto& b = rev[(size_t) ((revPos - back + len * 4) % len)];
                    const float w = std::sin (juce::MathConstants<float>::pi * (float) revPos / (float) len);
                    l = b[0] * w; r = b[1] * w;
                    revPos = (revPos + 1) % len;
                }
                // MOD: a slowly moving short delay, the room shimmers
                if (modDepth > 0.001f)
                {
                    modPhase += 0.35f / sr; if (modPhase >= 1.0f) modPhase -= 1.0f;
                    mod[(size_t) modPos] = { l, r };
                    const float dl = (2.0f + 10.0f * modDepth * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * modPhase))) * 0.001f * sr;
                    const int mlen = (int) mod.size();
                    const int i0 = (modPos - (int) dl + mlen * 2) % mlen;
                    l = l * (1.0f - modDepth * 0.5f) + mod[(size_t) i0][0] * modDepth * 0.5f;
                    r = r * (1.0f - modDepth * 0.5f) + mod[(size_t) i0][1] * modDepth * 0.5f;
                    modPos = (modPos + 1) % mlen;
                }
                wetL[(size_t) i] = l; wetR[(size_t) i] = r;
            }
            reverb.processStereo (wetL.data(), wetR.data(), n);
            // TAIL off: the room stops soon after the playing stops
            silent = level < 1.0e-4f ? silent + (float) n / sr : 0.0f;
            const float tailTarget = (s.tail || silent < 0.25f) ? 1.0f : 0.0f;
            for (int i = 0; i < n; ++i)
            {
                tailGain += (tailTarget - tailGain) * 0.0008f;
                const float a = std::abs (L[i]) + std::abs (R[i]);
                duckEnv = a > duckEnv ? a : duckEnv * duckRel;
                const float duckGain = 1.0f - s.duck * juce::jmin (1.0f, duckEnv * 3.0f) * 0.9f;
                const float wl = toneL.lp (wetL[(size_t) i]) * duckGain * tailGain, wr = toneR.lp (wetR[(size_t) i]) * duckGain * tailGain;
                const float m = juce::jlimit (0.0f, 1.0f, s.mix);
                L[i] = L[i] * (1.0f - 0.5f * m) + wl * m * 0.9f;
                R[i] = R[i] * (1.0f - 0.5f * m) + wr * m * 0.9f;
            }
        }

        juce::Reverb reverb;
        std::vector<std::array<float, 2>> pre, rev, mod;
        std::vector<float> wetL, wetR;
        OnePole lowCut, lowCutR, toneL, toneR;
        float sr = 48000.0f, duckEnv = 0.0f, modPhase = 0.0f, silent = 0.0f, tailGain = 1.0f;
        int prePos = 0, revPos = 0, modPos = 0;
        bool idle = true;
    };

    // TILT (bright <> dark around 800 Hz) and WOOD (a warm low body and soft saturation)
    struct Colour
    {
        OnePole tl, tr, wl, wr;
        void process (float* L, float* R, int n, float tilt, float wood, float sr)
        {
            if (std::abs (tilt) < 0.01f && wood < 0.01f) return;
            tl.set (800.0f, sr); tr.set (800.0f, sr); wl.set (180.0f, sr); wr.set (180.0f, sr);
            const float drive = 1.0f + 3.0f * wood, makeup = 1.0f / (0.7f + 0.3f * drive);
            for (int i = 0; i < n; ++i)
            {
                float l = L[i], r = R[i];
                if (std::abs (tilt) >= 0.01f)
                {
                    const float ll = tl.lp (l), lr = tr.lp (r);
                    l = ll * (1.0f - tilt) + (l - ll) * (1.0f + tilt);
                    r = lr * (1.0f - tilt) + (r - lr) * (1.0f + tilt);
                }
                if (wood >= 0.01f)
                {
                    l += wl.lp (l) * wood * 1.2f; r += wr.lp (r) * wood * 1.2f;
                    l = std::tanh (l * drive) * makeup; r = std::tanh (r * drive) * makeup;
                }
                L[i] = l; R[i] = r;
            }
        }
    };

    // SYMPATHETIC strings: the other strings of the harp ring along with what is played
    struct Sympathetic
    {
        struct Res { float ic1 = 0, ic2 = 0, g = 0, k = 0.01f, a1 = 0, a2 = 0, a3 = 0; };
        std::array<Res, 12> res;
        std::array<int, 12> notes {};
        int num = 0;
        void tune (const std::vector<int>& tones, float sr)
        {
            num = 0;
            for (int t : tones)
            {
                if (num >= 12) break;
                const int note = 48 + ((t % 12) + 12) % 12 + 12 * (num % 3);
                if (num < 12 && notes[(size_t) num] == note) { ++num; continue; }
                notes[(size_t) num] = note;
                auto& r = res[(size_t) num];
                const float hz = 440.0f * std::exp2 ((float) (note - 69) / 12.0f);
                r.g = std::tan (juce::MathConstants<float>::pi * hz / sr);
                r.k = 1.0f / 120.0f;
                r.a1 = 1.0f / (1.0f + r.g * (r.g + r.k)); r.a2 = r.g * r.a1; r.a3 = r.g * r.a2;
                ++num;
            }
        }
        void process (float* L, float* R, int n, float amount)
        {
            if (amount < 0.005f || num == 0) return;
            for (int i = 0; i < n; ++i)
            {
                const float x = 0.5f * (L[i] + R[i]) * 0.2f;
                float sum = 0.0f;
                for (int k = 0; k < num; ++k)
                {
                    auto& r = res[(size_t) k];
                    const float v3 = x - r.ic2, v1 = r.a1 * r.ic1 + r.a2 * v3, v2 = r.ic2 + r.a2 * r.ic1 + r.a3 * v3;
                    r.ic1 = 2.0f * v1 - r.ic1; r.ic2 = 2.0f * v2 - r.ic2;
                    sum += v1 * ((k & 1) ? 1.0f : -1.0f);
                }
                L[i] += sum * amount * 0.12f; R[i] += sum * amount * 0.12f;
            }
        }
    };
}
