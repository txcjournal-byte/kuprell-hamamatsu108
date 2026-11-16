#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Layers.h"
#include <array>
#include <cmath>

// 16 effects for the 8 FX slots. Every slot owns one FxUnit that can become any effect.
// Each effect has two controls (A, B) and Mix.
namespace fx
{
    constexpr int numSlots = 8;

    inline const juce::StringArray& names()
    {
        static const juce::StringArray n { "Empty", "EQ", "Compressor", "Drive", "Bitcrush", "Filter", "Vibrato", "Chorus", "Flanger",
                                           "Phaser", "Speaker", "Tape", "Delay", "Ping-Pong", "Reverb", "Shimmer", "Limiter" };
        return n;
    }

    // labels of A and B for each effect
    inline std::pair<juce::String, juce::String> controlNames (int type)
    {
        switch (type)
        {
            case 1:  return { "Low", "High" };
            case 2:  return { "Amount", "Release" };
            case 3:  return { "Drive", "Tone" };
            case 4:  return { "Bits", "Rate" };
            case 5:  return { "Cutoff", "Reso" };
            case 6:  return { "Rate", "Depth" };
            case 7:  return { "Rate", "Depth" };
            case 8:  return { "Rate", "Feedback" };
            case 9:  return { "Rate", "Feedback" };
            case 10: return { "Size", "Drive" };
            case 11: return { "Saturate", "Wow" };
            case 12: return { "Time", "Feedback" };
            case 13: return { "Time", "Feedback" };
            case 14: return { "Size", "Damping" };
            case 15: return { "Size", "Shimmer" };
            case 16: return { "Ceiling", "Release" };
            default: return { "A", "B" };
        }
    }

    // musical delay times for A (Delay, Ping-Pong)
    inline double delayQuarters (float a)
    {
        static const double q[] = { 0.25, 0.375, 0.5, 0.75, 1.0, 1.5, 2.0 };
        static const char* n[] = { "1/16", "1/16 D", "1/8", "1/8 D", "1/4", "1/4 D", "1/2" };
        juce::ignoreUnused (n);
        return q[juce::jlimit (0, 6, (int) (a * 6.999f))];
    }
    inline juce::String delayName (float a)
    {
        static const char* n[] = { "1/16", "1/16 D", "1/8", "1/8 D", "1/4", "1/4 D", "1/2" };
        return n[juce::jlimit (0, 6, (int) (a * 6.999f))];
    }

    struct DelayLine
    {
        std::vector<float> buf;
        int w = 0;
        void prepare (int size) { buf.assign ((size_t) juce::jmax (4, size), 0.0f); w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
        // delay in samples (>= 1), linear interpolation
        float read (float d) const
        {
            const int size = (int) buf.size();
            d = juce::jlimit (1.0f, (float) (size - 2), d);
            float rp = (float) w - d;
            while (rp < 0) rp += (float) size;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1) % size;
            const float f = rp - (float) i0;
            return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
        }
    };

    struct OnePole
    {
        float z = 0;
        float lp (float x, float c) { z += (x - z) * c; return z; }
    };

    // octave-up pitch shifter (two crossfaded taps on a delay line)
    struct OctaveUp
    {
        DelayLine line;
        float phase = 0.0f;
        int window = 2048;
        void prepare (double sr) { window = (int) (sr * 0.05); line.prepare (window * 3); }
        float process (float x)
        {
            line.push (x);
            phase += 1.0f / (float) window;   // ratio 2: read pointer moves twice as fast
            if (phase >= 1.0f) phase -= 1.0f;
            const float p2 = std::fmod (phase + 0.5f, 1.0f);
            const float d1 = (1.0f - phase) * (float) window + 2.0f;
            const float d2 = (1.0f - p2) * (float) window + 2.0f;
            const float g1 = std::sin (phase * juce::MathConstants<float>::pi);
            const float g2 = std::sin (p2 * juce::MathConstants<float>::pi);
            return line.read (d1) * g1 + line.read (d2) * g2;
        }
    };

    class FxUnit
    {
    public:
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate;
            const int maxDelay = (int) (sr * 2.6);
            for (auto* d : { &dl, &dr }) d->prepare (maxDelay);
            for (auto* d : { &ml, &mr }) d->prepare ((int) (sr * 0.06));
            reverb.setSampleRate (sr);
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlock), 2 };
            comp.prepare (spec);
            limiter.prepare (spec);
            shL.prepare (sr); shR.prepare (sr);
            wetL.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
            wetR.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
            shimmerFbL.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
            shimmerFbR.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
            reset();
        }

        void reset()
        {
            dl.clear(); dr.clear(); ml.clear(); mr.clear();
            reverb.reset(); comp.reset(); limiter.reset();
            for (auto& a : ap) a = {};
            phFb = {};
            lfo = 0; crushL = crushR = 0; crushCount = 0;
            for (auto* f : { &svfL, &svfR, &spkL, &spkR, &spk2L, &spk2R }) f->reset();
            std::fill (shimmerFbL.begin(), shimmerFbL.end(), 0.0f);
            std::fill (shimmerFbR.begin(), shimmerFbR.end(), 0.0f);
        }

        void process (int type, float a, float b, float mix, float* L, float* R, int n, double bpm)
        {
            if (type != lastType) { reset(); lastType = type; }
            if (type == 0 || mix <= 0.0001f) return;
            if ((int) wetL.size() < n) { wetL.resize ((size_t) n); wetR.resize ((size_t) n); shimmerFbL.resize ((size_t) n, 0.0f); shimmerFbR.resize ((size_t) n, 0.0f); }

            float* wl = wetL.data();
            float* wr = wetR.data();
            std::copy (L, L + n, wl);
            std::copy (R, R + n, wr);
            const float fsr = (float) sr;

            switch (type)
            {
                case 1: // EQ: low and high shelves (+-12 dB), A/B centre = flat
                {
                    const float lowG = juce::Decibels::decibelsToGain ((a - 0.5f) * 24.0f);
                    const float highG = juce::Decibels::decibelsToGain ((b - 0.5f) * 24.0f);
                    const float cl = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 250.0f / fsr);
                    const float ch = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 4000.0f / fsr);
                    for (int i = 0; i < n; ++i)
                    {
                        const float lowL = eqLowL.lp (wl[i], cl), lowR = eqLowR.lp (wr[i], cl);
                        const float hiL = wl[i] - eqHiL.lp (wl[i], ch), hiR = wr[i] - eqHiR.lp (wr[i], ch);
                        wl[i] += lowL * (lowG - 1.0f) + hiL * (highG - 1.0f);
                        wr[i] += lowR * (lowG - 1.0f) + hiR * (highG - 1.0f);
                    }
                    break;
                }
                case 2: // compressor
                {
                    const float thr = -6.0f - a * 30.0f, ratio = 2.0f + a * 6.0f;
                    comp.setThreshold (thr);
                    comp.setRatio (ratio);
                    comp.setAttack (5.0f);
                    comp.setRelease (40.0f + b * 400.0f);
                    float* chans[] = { wl, wr };
                    juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
                    comp.process (juce::dsp::ProcessContextReplacing<float> (blk));
                    const float makeup = juce::Decibels::decibelsToGain (-thr * (1.0f - 1.0f / ratio) * 0.75f);   // automatic make-up gain
                    for (int i = 0; i < n; ++i) { wl[i] *= makeup; wr[i] *= makeup; }
                    break;
                }
                case 3: // drive
                {
                    const float drive = 1.0f + a * 20.0f;
                    const float c = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * (800.0f + b * 12000.0f) / fsr);
                    const float comp2 = 1.0f / std::tanh (drive);
                    for (int i = 0; i < n; ++i)
                    {
                        wl[i] = toneL.lp (std::tanh (wl[i] * drive) * comp2 * 0.8f, c);
                        wr[i] = toneR.lp (std::tanh (wr[i] * drive) * comp2 * 0.8f, c);
                    }
                    break;
                }
                case 4: // bitcrush
                {
                    const float bits = 16.0f - a * 13.0f;
                    const float steps = std::pow (2.0f, bits - 1.0f);
                    const int hold = 1 + (int) (b * 24.0f);
                    for (int i = 0; i < n; ++i)
                    {
                        if (crushCount <= 0) { crushL = std::round (wl[i] * steps) / steps; crushR = std::round (wr[i] * steps) / steps; crushCount = hold; }
                        --crushCount;
                        wl[i] = crushL; wr[i] = crushR;
                    }
                    break;
                }
                case 5: // filter (low-pass)
                {
                    const float cutoff = 60.0f * std::pow (300.0f, a);
                    svfL.set (cutoff, 0.6f + b * 8.0f, fsr); svfR.set (cutoff, 0.6f + b * 8.0f, fsr);
                    float bp;
                    for (int i = 0; i < n; ++i) { wl[i] = svfL.process (wl[i], bp); wr[i] = svfR.process (wr[i], bp); }
                    break;
                }
                case 6: case 7: case 8: // vibrato, chorus, flanger: modulated delay
                {
                    const float rate = type == 8 ? 0.05f + a * 2.0f : 0.1f + a * 7.0f;
                    const float base = type == 6 ? 0.006f : type == 7 ? 0.012f : 0.0025f;
                    const float depth = (type == 6 ? 0.004f : type == 7 ? 0.006f : 0.002f) * (type == 8 ? 1.0f : b) * fsr;
                    const float fb = type == 8 ? b * 0.85f : 0.0f;
                    const float inc = rate / fsr;
                    for (int i = 0; i < n; ++i)
                    {
                        const float sL = harp::sine() (lfo), sR = harp::sine() (harp::wrap (lfo + 0.25f));
                        const float yl = ml.read (base * fsr + depth * (0.5f + 0.5f * sL));
                        const float yr = mr.read (base * fsr + depth * (0.5f + 0.5f * sR));
                        ml.push (wl[i] + yl * fb);
                        mr.push (wr[i] + yr * fb);
                        wl[i] = yl; wr[i] = yr;
                        lfo = harp::wrap (lfo + inc);
                    }
                    if (type == 6) mix = juce::jmin (1.0f, mix * 2.0f);   // vibrato is fully wet at half mix
                    break;
                }
                case 9: // phaser: 6 all-pass stages
                {
                    const float inc = (0.05f + a * 4.0f) / fsr;
                    const float fb = b * 0.85f;
                    for (int i = 0; i < n; ++i)
                    {
                        const float m = 0.5f + 0.5f * harp::sine() (lfo);
                        const float f = 200.0f * std::pow (20.0f, m);
                        const float t = std::tan (juce::MathConstants<float>::pi * f / fsr);
                        const float coef = (t - 1.0f) / (t + 1.0f);
                        for (int ch = 0; ch < 2; ++ch)
                        {
                            float x = (ch == 0 ? wl[i] : wr[i]) + phFb[(size_t) ch] * fb;
                            for (int s = 0; s < 6; ++s)
                            {
                                auto& st = ap[(size_t) (ch * 6 + s)];
                                const float y = coef * x + st.z;
                                st.z = x - coef * y;
                                x = y;
                            }
                            phFb[(size_t) ch] = x;
                            (ch == 0 ? wl[i] : wr[i]) = x;
                        }
                        lfo = harp::wrap (lfo + inc);
                    }
                    break;
                }
                case 10: // speaker: small box resonance + drive
                {
                    const float centre = 2500.0f - a * 1900.0f;
                    spkL.set (centre, 0.9f, fsr); spkR.set (centre, 0.9f, fsr);
                    spk2L.set (centre * 3.2f, 0.7f, fsr); spk2R.set (centre * 3.2f, 0.7f, fsr);
                    const float drive = 1.0f + b * 8.0f;
                    for (int i = 0; i < n; ++i)
                    {
                        float bpL, bpR, b2L, b2R;
                        spkL.process (std::tanh (wl[i] * drive), bpL); spkR.process (std::tanh (wr[i] * drive), bpR);
                        spk2L.process (bpL, b2L); spk2R.process (bpR, b2R);
                        wl[i] = (bpL * 1.6f + b2L * 0.5f); wr[i] = (bpR * 1.6f + b2R * 0.5f);
                    }
                    break;
                }
                case 11: // tape: saturation, wow and flutter, soft top end
                {
                    const float sat = 1.0f + a * 4.0f;
                    const float wowDepth = b * 0.0025f * fsr;
                    const float c = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 9000.0f / fsr);
                    for (int i = 0; i < n; ++i)
                    {
                        const float w = 0.7f * harp::sine() (lfo) + 0.3f * harp::sine() (lfo2);
                        ml.push (std::tanh (wl[i] * sat) / std::tanh (sat));
                        mr.push (std::tanh (wr[i] * sat) / std::tanh (sat));
                        wl[i] = toneL.lp (ml.read (0.004f * fsr + wowDepth * (0.5f + 0.5f * w)), c);
                        wr[i] = toneR.lp (mr.read (0.004f * fsr + wowDepth * (0.5f + 0.5f * w)), c);
                        lfo = harp::wrap (lfo + 0.6f / fsr);
                        lfo2 = harp::wrap (lfo2 + 7.0f / fsr);
                    }
                    break;
                }
                case 12: case 13: // delay, ping-pong
                {
                    const float d = (float) (delayQuarters (a) * 60.0 / bpm * sr);
                    const float fb = b * 0.78f;
                    const float c = 0.4f;
                    for (int i = 0; i < n; ++i)
                    {
                        const float yl = dl.read (d), yr = dr.read (d);
                        const float fl = toneL.lp (yl, c), fr = toneR.lp (yr, c);
                        if (type == 12) { dl.push (wl[i] + fl * fb); dr.push (wr[i] + fr * fb); }
                        else            { dl.push (0.5f * (wl[i] + wr[i]) + fr * fb); dr.push (fl * fb); }
                        wl[i] = yl; wr[i] = yr;
                    }
                    break;
                }
                case 14: case 15: // reverb, shimmer
                {
                    juce::Reverb::Parameters p;
                    p.roomSize = type == 15 ? 0.5f + a * 0.32f : 0.5f + a * 0.48f;
                    p.damping = type == 15 ? 0.2f : b;
                    p.width = 1.0f; p.wetLevel = 1.0f; p.dryLevel = 0.0f;
                    reverb.setParameters (p);
                    if (type == 15)
                        for (int i = 0; i < n; ++i)
                        {
                            // octave-up copy of the input feeds the reverb (no feedback loop: always stable)
                            wl[i] += shLp.lp (shL.process (wl[i]), 0.3f) * b * 0.8f;
                            wr[i] += shRp.lp (shR.process (wr[i]), 0.3f) * b * 0.8f;
                        }
                    reverb.processStereo (wl, wr, n);
                    for (int i = 0; i < n; ++i) { wl[i] *= 0.6f; wr[i] *= 0.6f; }
                    break;
                }
                case 16: // limiter
                {
                    limiter.setThreshold (-a * 12.0f);
                    limiter.setRelease (20.0f + b * 400.0f);
                    float* chans[] = { wl, wr };
                    juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
                    limiter.process (juce::dsp::ProcessContextReplacing<float> (blk));
                    break;
                }
                default: return;
            }

            for (int i = 0; i < n; ++i)
            {
                L[i] += (wl[i] - L[i]) * mix;
                R[i] += (wr[i] - R[i]) * mix;
            }
        }

    private:
        struct Ap { float z = 0; };
        double sr = 44100.0;
        int lastType = -1;
        DelayLine dl, dr, ml, mr;
        juce::Reverb reverb;
        juce::dsp::Compressor<float> comp;
        juce::dsp::Limiter<float> limiter;
        layers::Svf svfL, svfR, spkL, spkR, spk2L, spk2R;
        OnePole toneL, toneR, eqLowL, eqLowR, eqHiL, eqHiR, shLp, shRp;
        OctaveUp shL, shR;
        std::array<Ap, 12> ap {};
        std::array<float, 2> phFb {};
        float lfo = 0, lfo2 = 0, crushL = 0, crushR = 0;
        int crushCount = 0;
        std::vector<float> wetL, wetR, shimmerFbL, shimmerFbR;
    };
}
