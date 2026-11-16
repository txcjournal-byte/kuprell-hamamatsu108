#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <cmath>

// Smooth stereo reverb: input diffusion + 8-line feedback delay network with damping and slow modulation.
// Produces the wet signal only. Stable for any setting (all loop gains < 1).
namespace hama
{
    class FdnReverb
    {
    public:
        void prepare (double sampleRate)
        {
            sr = (float) sampleRate;
            const float scale = sr / 44100.0f;
            static const int apLen[] = { 142, 107, 379, 277, 211, 163 };
            for (int i = 0; i < 6; ++i) ap[(size_t) i].init ((int) (apLen[i] * scale));
            for (auto& l : lines) l.buf.assign ((size_t) (sr * 0.2f) + 16, 0.0f);
            pre.assign ((size_t) (sr * 0.06f) + 4, 0.0f);
            reset();
        }

        void reset()
        {
            for (auto& a : ap) std::fill (a.buf.begin(), a.buf.end(), 0.0f);
            for (auto& l : lines) { std::fill (l.buf.begin(), l.buf.end(), 0.0f); l.w = 0; l.lp = 0.0f; }
            std::fill (pre.begin(), pre.end(), 0.0f);
            preW = 0;
        }

        // size 0..1 (room size and decay), damping 0..1 (dark), returns wet in L/R (in place)
        void process (float* L, float* R, int n, float size, float damping)
        {
            static const float baseMs[8] = { 37.3f, 41.9f, 47.7f, 53.1f, 59.3f, 67.7f, 73.1f, 79.9f };
            const float roomScale = 0.55f + size * 1.1f;
            const float rt60 = 0.6f + size * size * 8.0f;
            const float cutoff = 1500.0f + (1.0f - damping) * 12500.0f;
            const float dampCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / sr);
            std::array<float, 8> len {}, gain {};
            for (int i = 0; i < 8; ++i)
            {
                len[(size_t) i] = juce::jmin ((float) lines[(size_t) i].buf.size() - 12.0f, baseMs[i] * 0.001f * sr * roomScale);
                gain[(size_t) i] = std::pow (10.0f, -3.0f * len[(size_t) i] / (rt60 * sr));
            }
            const int preDelay = (int) (0.012f * sr * (0.5f + size));
            const float modInc = 0.37f / sr;

            for (int s = 0; s < n; ++s)
            {
                // pre-delay and diffusion
                pre[(size_t) preW] = 0.5f * (L[s] + R[s]);
                int rp = preW - preDelay; if (rp < 0) rp += (int) pre.size();
                float x = pre[(size_t) rp];
                if (++preW >= (int) pre.size()) preW = 0;
                for (auto& a : ap) x = a.process (x, 0.62f);

                // read the 8 lines (two of them slowly modulated)
                std::array<float, 8> y {};
                modPhase += modInc; if (modPhase >= 1.0f) modPhase -= 1.0f;
                for (int i = 0; i < 8; ++i)
                {
                    float d = len[(size_t) i];
                    if (i == 1) d += 6.0f * std::sin (modPhase * juce::MathConstants<float>::twoPi);
                    if (i == 6) d += 6.0f * std::cos (modPhase * juce::MathConstants<float>::twoPi * 1.3f);
                    auto& l = lines[(size_t) i];
                    y[(size_t) i] = l.read (d);
                }

                // Householder mixing, damping, feedback
                float sum = 0.0f;
                for (auto v : y) sum += v;
                const float k = sum * 0.25f;   // 2 / N
                for (int i = 0; i < 8; ++i)
                {
                    auto& l = lines[(size_t) i];
                    float fb = (y[(size_t) i] - k) * gain[(size_t) i];
                    l.lp += (fb - l.lp) * dampCoef;
                    l.write (l.lp + x * (i % 2 == 0 ? 0.5f : -0.5f));
                }

                L[s] = (y[0] - y[1] + y[2] + y[4] - y[5] + y[6]) * 0.3f;
                R[s] = (y[1] + y[3] - y[4] + y[5] - y[6] + y[7]) * 0.3f;
            }
        }

    private:
        struct Allpass
        {
            std::vector<float> buf; int p = 0;
            void init (int size) { buf.assign ((size_t) juce::jmax (2, size), 0.0f); p = 0; }
            float process (float x, float g)
            {
                const float d = buf[(size_t) p];
                const float y = -g * x + d;
                buf[(size_t) p] = x + g * y;
                if (++p >= (int) buf.size()) p = 0;
                return y;
            }
        };
        struct Line
        {
            std::vector<float> buf; int w = 0; float lp = 0.0f;
            void write (float v) { buf[(size_t) w] = v; if (++w >= (int) buf.size()) w = 0; }
            float read (float d) const
            {
                const int size = (int) buf.size();
                float r = (float) w - d;
                while (r < 0) r += (float) size;
                const int i0 = (int) r, i1 = (i0 + 1) % size;
                const float f = r - (float) i0;
                return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
            }
        };

        float sr = 44100.0f, modPhase = 0.0f;
        std::array<Allpass, 6> ap;
        std::array<Line, 8> lines;
        std::vector<float> pre;
        int preW = 0;
    };
}

namespace hama
{
    // The instrument's own body: a short 4-line feedback network that every string shares,
    // like the soundboard of a real harp. Adds depth and a stereo image to the harp only.
    class Soundboard
    {
    public:
        void prepare (double sampleRate)
        {
            sr = (float) sampleRate;
            for (auto& l : lines) { l.buf.assign ((size_t) (sr * 0.08f) + 4, 0.0f); l.w = 0; l.lp = 0.0f; }
            hpL = hpR = hpInL = hpInR = 0.0f;
        }
        // size 0.5..1.3, decay seconds, dark 0..1 (damping), returns wet only
        void configure (float size, float decay, float dark)
        {
            static const float base[4] = { 11.3f, 13.7f, 17.9f, 23.1f };
            for (int i = 0; i < 4; ++i)
            {
                auto& l = lines[(size_t) i];
                l.len = juce::jlimit (8, (int) l.buf.size() - 2, (int) (base[i] * size * 0.001f * sr));
                l.g = std::pow (10.0f, -3.0f * ((float) l.len / sr) / juce::jmax (0.05f, decay));
            }
            dampCoef = 0.15f + 0.7f * (1.0f - dark);
        }
        void process (const float* inL, const float* inR, float* outL, float* outR, int n, float mix)
        {
            const float hpC = 1.0f - juce::MathConstants<float>::twoPi * 160.0f / sr;   // keep the low end dry
            for (int i = 0; i < n; ++i)
            {
                const float xl = inL[i] - hpInL + hpC * hpL; hpInL = inL[i]; hpL = xl;
                const float xr = inR[i] - hpInR + hpC * hpR; hpInR = inR[i]; hpR = xr;
                float o[4];
                for (int k = 0; k < 4; ++k)
                {
                    auto& l = lines[(size_t) k];
                    int r = l.w - l.len; if (r < 0) r += (int) l.buf.size();
                    l.lp += (l.buf[(size_t) r] - l.lp) * dampCoef;
                    o[k] = l.lp * l.g;
                }
                // Householder-like 4x4 Hadamard mix (energy preserving)
                const float a = o[0] + o[1], b = o[0] - o[1], c = o[2] + o[3], d = o[2] - o[3];
                const float m0 = 0.5f * (a + c), m1 = 0.5f * (b + d), m2 = 0.5f * (a - c), m3 = 0.5f * (b - d);
                const float in[4] = { xl * 0.5f, xr * 0.5f, xl * 0.35f, xr * 0.35f };
                const float mm[4] = { m0, m1, m2, m3 };
                for (int k = 0; k < 4; ++k)
                {
                    auto& l = lines[(size_t) k];
                    l.buf[(size_t) l.w] = mm[k] + in[k];
                    if (++l.w >= (int) l.buf.size()) l.w = 0;
                }
                outL[i] += (o[0] + 0.6f * o[2]) * mix;
                outR[i] += (o[1] + 0.6f * o[3]) * mix;
            }
        }
    private:
        struct Line { std::vector<float> buf; int w = 0, len = 100; float g = 0.5f, lp = 0.0f; };
        std::array<Line, 4> lines;
        float sr = 48000.0f, dampCoef = 0.5f, hpL = 0, hpR = 0, hpInL = 0, hpInR = 0;
    };
}
