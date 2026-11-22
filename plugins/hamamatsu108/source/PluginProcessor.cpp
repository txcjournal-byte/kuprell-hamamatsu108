#include "PluginProcessor.h"
#include <algorithm>
#include "Skins.h"
#include "PluginEditor.h"
#if HAMA_MTS
 #include "libMTSClient.h"
#endif

namespace
{
    float param (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue (id)->load(); }

    double rateInQuarters (int rateIndex)
    {
        static const double r[] = { 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 2.0 / 3.0 };
        return r[juce::jlimit (0, 8, rateIndex)];
    }
}

const juce::StringArray& Hamamatsu108Processor::strumModeNames()
{
    static const juce::StringArray n { "Live", "Arp", "Memo", "Off" };
    return n;
}

// How the LIVE hand plays. "Even" is the plain clock (Rate knob, down / up alternating);
// the others are one bar of 1/16 steps played like a person would.
const juce::StringArray& Hamamatsu108Processor::strumPatternNames()
{
    static const juce::StringArray n { "Straight", "Sparse", "Syncopated", "8ths", "16ths", "3-3-2", "Half Notes", "Picked 8ths",
                                       "Bounce", "Backbeat", "3/4", "Half-Time", "Sweep + Pick", "Custom" };
    return n;
}

//  D full down  U full up  d light down (upper strings)  u light up (top strings)
//  B thumb on the bass strings  H one high string  T finger tap (2-3 strings)  x ghost brush  S slow full sweep  . rest
const char* Hamamatsu108Processor::strumPatternSteps (int index)
{
    static const char* steps[] = { "",
                                   "D.....u.d.....u.",   // Sparse
                                   "D...D.U...U.D.U.",   // Syncopated
                                   "D.u.D.U.d.u.D.U.",   // 8ths
                                   "D.dUx.dUD.dUx.dU",   // 16ths
                                   "T..TT.x.T..TT.x.",   // 3-3-2
                                   "S.......S.......",   // Half Notes
                                   "B.H.T.H.B.H.T.H.",   // Picked 8ths
                                   "B..dB.d.B..dB.d.",   // Bounce
                                   "....d.x.....d.x.",   // Backbeat
                                   "B...d.u.d.u.",       // 3/4 (12 steps)
                                   "D.......d...u...",   // Half-Time
                                   "S...H.u.S...H.u." }; // Sweep + Pick
    return steps[juce::jlimit (0, 12, index)];
}

juce::String Hamamatsu108Processor::patternString (int index) const
{
    if (index != 13) return strumPatternSteps (index);
    juce::String s;
    for (int i = 0; i < juce::jlimit (1, 16, customLen.load()); ++i) s += juce::String::charToString ((juce::juce_wchar) customSteps[(size_t) i].load());
    return s;
}

const juce::StringArray& Hamamatsu108Processor::keysPlayNames()
{
    static const juce::StringArray n { "Notes", "Strum", "Pattern", "Plate only", "Flourish up", "Flourish down", "Split" };
    return n;
}

const juce::StringArray& Hamamatsu108Processor::switchNames()
{
    static const juce::StringArray n { "SPARKLE", "SUB", "SWELL", "FREEZE", "DRIVE", "CRUSH", "FILTER", "TAPE",
                                       "DREAM", "WOBBLE", "WIDE", "DELAY", "REVERB", "SHIMMER", "HALF", "HUMAN",
                                       "CHORUS", "PHASE", "FLANGE", "RADIO", "FIFTH", "DETUNE", "STUTTER", "VIBRATO" };
    return n;
}

const juce::StringArray& Hamamatsu108Processor::switchHelp()
{
    static const juce::StringArray n { "an octave above every note", "an octave below every note", "the strings swell in slowly",
                                       "the strings ring on and on", "warm saturation", "lo-fi bits", "a low-pass that closes",
                                       "worn tape", "a huge soft reverb", "pitch wobble", "a wider stereo image", "ping-pong delay",
                                       "a big hall", "an octave-up shimmer tail", "MOTION plays at half speed", "loose timing and touch",
                                       "a wide chorus", "a slow phaser", "a jet flanger", "a small old speaker", "a fifth above every note",
                                       "a second string, slightly out of tune", "the sound chopped in rhythm", "a singing vibrato" };
    return n;
}

float Hamamatsu108Processor::sw (int i) const
{
    return *apvts.getRawParameterValue ("sw" + juce::String (i) + "On") > 0.5f ? apvts.getRawParameterValue ("sw" + juce::String (i) + "Amt")->load() : 0.0f;
}

const juce::StringArray& Hamamatsu108Processor::strumZoneNames()
{
    static const juce::StringArray n { "C0 (piano roll)", "C1", "C2", "C3", "Off" };
    return n;
}

const juce::StringArray& Hamamatsu108Processor::strumGestureNames()
{
    static const juce::StringArray n { "From the bottom", "From the bottom, slow", "From the top", "From the top, slow", "From the middle out",
                                       "Short, low strings", "Short, high strings", "MOTION while held", "MOTION, one bar", "Tremolo while held",
                                       "Glissando up and down", "Damp (stop the strings)" };
    return n;
}

const juce::StringArray& Hamamatsu108Processor::modelNames()
{
    static const juce::StringArray n { "Warm", "Classic", "Bright", "Clean", "Lo-fi", "Hi-fi" };
    return n;
}

const Hamamatsu108Processor::ModelSpec& Hamamatsu108Processor::modelSpec (int m)
{
    //                                   cutoff  width  hiss     bits   hold  drift  sat    tone   decay
    //                                   cutoff   width hiss     bits  hold drift sat    tone   decay  chorus
    static const ModelSpec specs[] = { { 3200.0f,  0.15f, 0.0003f, 16.0f, 1, 3.0f, 0.50f, 0.55f, 0.75f, 0.00f, 0.55f, 0.22f, 0.75f, 0.17f },   // Early 81: dark, nearly mono, warm drift
                                       { 6500.0f,  0.45f, 0.0f,    16.0f, 1, 0.8f, 0.25f, 0.80f, 1.00f, 0.25f, 0.80f, 0.32f, 0.55f, 0.08f },   // System 84: the classic, light chorus
                                       { 13000.0f, 0.85f, 0.0f,    16.0f, 1, 0.4f, 0.15f, 1.00f, 1.10f, 0.50f, 0.95f, 0.42f, 0.40f, 0.13f },   // Tone Bank 89: bright and lush
                                       { 17000.0f, 1.00f, 0.0f,    16.0f, 1, 0.0f, 0.08f, 1.05f, 1.15f, 0.15f, 1.00f, 0.30f, 0.35f, 0.09f },   // Late 90s: clean
                                       { 6000.0f,  0.60f, 0.0001f, 10.0f, 2, 1.0f, 0.35f, 0.90f, 0.85f, 0.00f, 0.50f, 0.18f, 0.60f, 0.10f },   // Cartridge 99: 8-bit, 16 kHz grit
                                       { 20000.0f, 1.30f, 0.0f,    16.0f, 1, 0.0f, 0.03f, 1.15f, 1.30f, 0.30f, 1.25f, 0.60f, 0.25f, 0.13f } }; // Modern Strum: hi-fi, extra wide
    return specs[juce::jlimit (0, 5, m)];
}

int Hamamatsu108Processor::targetIndex (const juce::String& id)
{
    return mod::targetIds().indexOf (id);
}

float Hamamatsu108Processor::mp (const char* id) const
{
    if (id[0] == '@') return 0.0f;
    auto* p = apvts.getParameter (id);
    const int t = targetIndex (id);
    if (p == nullptr) return 0.0f;
    if (t <= 0 || std::abs (modOffset[(size_t) t]) < 1.0e-5f) return apvts.getRawParameterValue (id)->load();
    return p->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, p->getValue() + modOffset[(size_t) t]));
}

// function points: times are kept in order, the first point sits at 0 and the last at 1
void Hamamatsu108Processor::funcShape (std::array<float, mod::funcPoints>& xs, std::array<float, mod::funcPoints>& ys) const
{
    float prev = 0.0f;
    for (int i = 0; i < mod::funcPoints; ++i)
    {
        const juce::String n (i + 1);
        ys[(size_t) i] = apvts.getRawParameterValue ("funcP" + n)->load();
        float x = i == 0 ? 0.0f : i == mod::funcPoints - 1 ? 1.0f : apvts.getRawParameterValue ("funcT" + n)->load();
        x = juce::jmax (prev, x);
        xs[(size_t) i] = x;
        prev = x;
    }
}

void Hamamatsu108Processor::updateModulation (int numSamples, bool hostPlaying, double ppq)
{
    const double secs = numSamples / sr;
    auto P = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };

    // the modulators rest while nothing sounds and start from the beginning with the next note
    bool active = activeChord.load() >= 0 || numEvents > 0 || pendingPlucks.load() != 0;
    for (auto& v : voices)      active = active || v.isActive();
    for (auto& v : chordVoices) active = active || v.isActive();
    for (auto& v : bassVoices)  active = active || v.isActive();
    if (! active)
    {
        func.trigger();
        randomSrc.reset();
    }
    modActive.store (active);

    std::array<float, mod::numSources> src {};
    for (int i = 0; i < 2; ++i)
    {
        const juce::String id ("lfo" + juce::String (i + 1));
        const int shape = juce::roundToInt (P (id + "Shape"));
        const int sync = juce::roundToInt (P (id + "Sync"));
        auto& l = lfos[(size_t) i];
        // the LFOs run free, whatever plays or not (a sound that starts lands somewhere in the wave, like hardware)
        if (sync > 0 && hostPlaying)
        {
            const double q = mod::syncQuarters (sync);
            l.phase = std::fmod (ppq / q, 1.0);
            src[(size_t) (mod::lfo1 + i)] = l.advance (0.0, shape);
        }
        else
        {
            const double hz = sync > 0 ? bpm / 60.0 / mod::syncQuarters (sync) : P (id + "Rate");
            src[(size_t) (mod::lfo1 + i)] = l.advance (hz * secs, shape);
        }
    }
    for (int i = 0; i < 2; ++i) lfoPhase[(size_t) i].store ((float) lfos[(size_t) i].phase);
    // CONSOLE LFO shaping: wait (DELAY), grow in (FADE) after each key, and sit above, around or below zero (OFFSET)
    keyTime += secs;
    for (int i = 0; i < 2; ++i)
    {
        const juce::String id ("lfo" + juce::String (i + 1));
        const double d = P (id + "Delay") * 3.0, f = P (id + "Fade") * 3.0;
        const float env = keyTime < d ? 0.0f : f <= 0.001 ? 1.0f : (float) juce::jmin (1.0, (keyTime - d) / f);
        float v = src[(size_t) (mod::lfo1 + i)];
        const int off = juce::roundToInt (P (id + "Offset"));
        if (off == 0) v = 0.5f * (v + 1.0f); else if (off == 2) v = 0.5f * (v - 1.0f);
        src[(size_t) (mod::lfo1 + i)] = v * env;
    }
    {
        // LFO 2: PHASE (against LFO 1), SMOOTH, INVERT, and a restart on every bar
        if (P ("lfo2Bar") > 0.5f && hostPlaying)
        {
            const int bar = (int) std::floor (ppq / 4.0);
            if (bar != lastLfoBar) { lfos[1].phase = 0.0; lastLfoBar = bar; }
        }
        float v = src[mod::lfo2];
        const float ph = P ("lfo2Phase");
        if (ph > 0.001f) v = v * std::cos (ph * juce::MathConstants<float>::pi) + src[mod::lfo1] * std::sin (ph * juce::MathConstants<float>::pi);
        if (P ("lfo2Invert") > 0.5f) v = -v;
        const float sm = P ("lfo2Smooth");
        lfo2Smoothed += (v - lfo2Smoothed) * (sm < 0.001f ? 1.0f : (float) juce::jmin (1.0, secs / (0.02 + 1.5 * sm * sm)));
        src[mod::lfo2] = lfo2Smoothed;
    }
    std::array<float, mod::funcPoints> pts {}, xs {};
    funcShape (xs, pts);
    src[mod::function] = func.advance (active ? P ("funcRate") * secs : 0.0, juce::roundToInt (P ("funcMode")), xs, pts);
    funcPhase.store ((float) func.phase);
    src[mod::random] = active ? randomSrc.advance (P ("randRate") * secs, P ("randSmooth"), P ("randChaos"), (float) secs) : 0.0f;
    src[mod::velocity] = lastVelocity;
    src[mod::modWheel] = modWheel;
    src[mod::aftertouch] = aftertouch;
    src[mod::morph] = P ("morph");
    src[mod::age] = juce::jmin (1.0f, P ("macroAge") + sw (swTape));
    src[mod::dream] = juce::jmin (1.0f, P ("macroDream") + sw (swDream));
    src[mod::wobble] = juce::jmin (1.0f, P ("macroWobble") + sw (swWobble));
    for (size_t i = 0; i < src.size(); ++i) modSources[i].store (src[i]);

    modOffset.fill (0.0f);
    modRouted.fill (false);
    for (int i = 1; i <= mod::matrixSlots; ++i)
    {
        const juce::String n (i);
        const int sIdx = juce::roundToInt (P ("mod" + n + "Src"));
        const int dIdx = juce::roundToInt (P ("mod" + n + "Dst"));
        if (sIdx > 0 && dIdx > 0)
        {
            modOffset[(size_t) dIdx] += P ("mod" + n + "Amt") * src[(size_t) sIdx];
            if (std::abs (P ("mod" + n + "Amt")) > 1.0e-4f) modRouted[(size_t) dIdx] = true;
        }
    }
    {
        const char* ids[] = { "lfo1", "lfo2", "func", "rand" };
        const int srcIdx[] = { mod::lfo1, mod::lfo2, mod::function, mod::random };
        for (int i = 0; i < 4; ++i)
        {
            const int dIdx = juce::roundToInt (P (juce::String (ids[i]) + "QDst"));
            const float amt = P (juce::String (ids[i]) + "QAmt");
            if (dIdx > 0 && std::abs (amt) > 1.0e-4f)
            {
                modOffset[(size_t) dIdx] += amt * src[(size_t) srcIdx[i]];
                modRouted[(size_t) dIdx] = true;
            }
        }
    }
    {
        // CONSOLE routes: STRING Mod + Amt to the pitch, FILTER LFO to the cutoff
        const int ps = juce::roundToInt (P ("pitchModSrc"));
        const float pa = P ("pitchModAmt");
        if (std::abs (pa) > 1.0e-4f)
        {
            const float sv = ps == 0 ? src[mod::lfo1] : ps == 1 ? src[mod::lfo2] : src[mod::function];
            modOffset[mod::tPitch] += pa * std::abs (pa) * 0.25f * sv;   // up to 3 semitones, fine near the middle
            modRouted[mod::tPitch] = true;
        }
        const float fa = P ("filtLfoAmt");
        if (std::abs (fa) > 1.0e-4f)
        {
            modOffset[mod::tFilter] += fa * 0.6f * (juce::roundToInt (P ("filtLfoSel")) == 0 ? src[mod::lfo1] : src[mod::lfo2]);
            modRouted[mod::tFilter] = true;
        }
    }
    const float morphValue = P ("morph");
    for (int i = 1; i <= mod::morphSlots; ++i)
    {
        const juce::String n (i);
        const int dIdx = juce::roundToInt (P ("morph" + n + "Dst"));
        if (dIdx > 0) { modOffset[(size_t) dIdx] += P ("morph" + n + "Amt") * morphValue; if (std::abs (P ("morph" + n + "Amt")) > 1.0e-4f) modRouted[(size_t) dIdx] = true; }
    }
}

const juce::StringArray& Hamamatsu108Processor::arpPatternNames()
{
    static const juce::StringArray n { "Up", "Down", "Up / Down", "Thumb", "Mirror", "Random", "Converge", "Pinky", "Steps", "As Played", "Down / Up", "Chord" };
    return n;
}

const juce::StringArray& Hamamatsu108Processor::arpRegenNames()
{
    static const juce::StringArray n { "Off", "1 bar", "2 bars", "4 bars" };
    return n;
}

const juce::StringArray& Hamamatsu108Processor::memStepNames()
{
    static const juce::StringArray n { "1/2 bar", "1 bar", "2 bars", "1 beat", "4 bars" };
    return n;
}

double Hamamatsu108Processor::memStepQuarters (int index)
{
    static const double q[] = { 2.0, 4.0, 8.0, 1.0, 16.0 };
    return q[juce::jlimit (0, 4, index)];
}

void Hamamatsu108Processor::memClear (int bank)
{
    for (int i = 0; i < memSteps; ++i) memory[(size_t) (bank * memSteps + i)].store (-1);
    memLength[(size_t) bank].store (0);
}

void Hamamatsu108Processor::memDelete (int bank, int step)
{
    const int len = memLength[(size_t) bank].load();
    if (step < 0 || step >= len) return;
    for (int i = step; i < len - 1; ++i)
        memory[(size_t) (bank * memSteps + i)].store (memory[(size_t) (bank * memSteps + i + 1)].load());
    memory[(size_t) (bank * memSteps + len - 1)].store (-1);
    memLength[(size_t) bank].store (len - 1);
}

// ready-made progressions, written as (semitones above the key, chord type)
const std::vector<Hamamatsu108Processor::Progression>& Hamamatsu108Processor::progressions()
{
    static const std::vector<Progression> p {
        { "Pop  I - V - vi - IV",            { { 0, 0 }, { 7, 0 }, { 9, 1 }, { 5, 0 } } },
        { "Ballad  vi - IV - I - V",         { { 9, 1 }, { 5, 0 }, { 0, 0 }, { 7, 0 } } },
        { "Fifties  I - vi - IV - V",        { { 0, 0 }, { 9, 1 }, { 5, 0 }, { 7, 0 } } },
        { "Canon  I - V - vi - iii - IV - I - IV - V", { { 0, 0 }, { 7, 0 }, { 9, 1 }, { 4, 1 }, { 5, 0 }, { 0, 0 }, { 5, 0 }, { 7, 0 } } },
        { "Jazz  ii7 - V7 - Imaj7",          { { 2, 4 }, { 7, 2 }, { 0, 3 }, { 0, 3 } } },
        { "Lo-fi  ii7 - V7 - iii7 - vi7",    { { 2, 4 }, { 7, 2 }, { 4, 4 }, { 9, 4 } } },
        { "City pop  IVmaj7 - V7 - iii7 - vi", { { 5, 3 }, { 7, 2 }, { 4, 4 }, { 9, 1 } } },
        { "Dreamy  Imaj7 - IVmaj7",          { { 0, 3 }, { 5, 3 } } },
        { "Soul  I - iii - IV - iv",         { { 0, 0 }, { 4, 1 }, { 5, 0 }, { 5, 1 } } },
        { "Gospel  I - I7 - IV - iv",        { { 0, 0 }, { 0, 2 }, { 5, 0 }, { 5, 1 } } },
        { "Folk  I - IV - I - V",            { { 0, 0 }, { 5, 0 }, { 0, 0 }, { 7, 0 } } },
        { "Sus  Isus4 - I - IVadd9 - V",     { { 0, 7 }, { 0, 0 }, { 5, 8 }, { 7, 0 } } },
        { "Blues  12 bar",                   { { 0, 2 }, { 0, 2 }, { 0, 2 }, { 0, 2 }, { 5, 2 }, { 5, 2 }, { 0, 2 }, { 0, 2 }, { 7, 2 }, { 5, 2 }, { 0, 2 }, { 7, 2 } } },
        { "Minor  i - VI - III - VII",       { { 0, 1 }, { 8, 0 }, { 3, 0 }, { 10, 0 } } },
        { "Minor  i - iv - VI - V",          { { 0, 1 }, { 5, 1 }, { 8, 0 }, { 7, 0 } } },
        { "Flamenco  i - VII - VI - V",      { { 0, 1 }, { 10, 0 }, { 8, 0 }, { 7, 0 } } },
        { "Dark  i - VI - iv - V7",          { { 0, 1 }, { 8, 0 }, { 5, 1 }, { 7, 2 } } },
        { "Minor jazz  iim7b5 - V7 - i",     { { 2, 5 }, { 7, 2 }, { 0, 4 }, { 0, 4 } } },
    };
    return p;
}

void Hamamatsu108Processor::memLoad (int bank, int progressionIndex, int keyRoot)
{
    const auto& all = progressions();
    if (progressionIndex < 0 || progressionIndex >= (int) all.size()) return;
    memClear (bank);
    const auto& steps = all[(size_t) progressionIndex].steps;
    int n = 0;
    for (auto [semi, type] : steps)
        if (n < memSteps) memory[(size_t) (bank * memSteps + n++)].store (chords::index ((keyRoot + semi) % 12, type));
    memLength[(size_t) bank].store (n);
}

void Hamamatsu108Processor::memSet (int bank, int step, int chord)
{
    const int len = memLength[(size_t) bank].load();
    if (step < 0 || step >= memSteps) return;
    if (step >= len)
    {
        for (int i = len; i < step; ++i) memory[(size_t) (bank * memSteps + i)].store (chord);
        memLength[(size_t) bank].store (step + 1);
    }
    memory[(size_t) (bank * memSteps + step)].store (chord);
}

void Hamamatsu108Processor::memRecordChord (int chord)
{
    if (chord < 0 || ! memRecord.load()) return;
    const int bank = memBank.load();
    const int len = memLength[(size_t) bank].load();
    if (len >= memSteps) { memRecord.store (false); return; }
    memory[(size_t) (bank * memSteps + len)].store (chord);
    memLength[(size_t) bank].store (len + 1);
    if (len + 1 >= memSteps) memRecord.store (false);
}

const juce::StringArray& Hamamatsu108Processor::strumRateNames()
{
    static const juce::StringArray n { "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/4T" };
    return n;
}

//==============================================================================
Hamamatsu108Processor::Hamamatsu108Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "Hamamatsu108", createParameterLayout())
{
    resetPlate();
    for (auto& v : consoleSpare) v.store (-1.0f);
    for (auto& sl : progSlots) sl.store (-1);
    progNotes.reserve (256);
    gridNotes.reserve (256);
    setGridPreset (0);
    {   // STRINGS default: a slow bass, a quarter, a dotted eighth, triplets on top: a moving web of pulses
        const int rate[] = { 2, 0, 3, 0, 4, 5, 0, 7 }, off[] = { 0, 0, 2, 0, 1, 2, 0, 1 };
        const float lvl[] = { 0.9f, 0.7f, 0.75f, 0.7f, 0.7f, 0.65f, 0.6f, 0.55f };
        for (int r = 0; r < prog::gridRows; ++r)
        {
            strRate[(size_t) r].store (rate[r]); strOffset[(size_t) r].store (off[r]); strLevel[(size_t) r].store (lvl[r]);
            strOct[(size_t) r].store (0); strChance[(size_t) r].store (100); strRatchet[(size_t) r].store (1); strMute[(size_t) r].store (false);
        }
    }
    for (int i = 0; i < prog::linePoints; ++i)   // a default wave: up and down twice per bar
        motionLine[(size_t) i].store (0.5f + 0.45f * (float) std::sin (i / (float) prog::linePoints * juce::MathConstants<float>::twoPi * 2.0f));
    for (auto& c : ccMap) c.store (-1);
    for (auto& c : ccPending) c.store (0.0f);
#if HAMA_MTS
    mtsClient = MTS_RegisterClient();
#endif
    loadUserDefaults();
    startTimerHz (30);
    presetManager = std::make_unique<presets::Manager> (apvts);
    presetManager->load (0);   // start with "Paper Lanterns"
    undoManager.clearUndoHistory();
    tones.reserve (64);
    for (auto& m : memory) m.store (-1);
    for (auto& l : memLength) l.store (0);
    seqLoadPreset (0);
    { const char* def = "D..dU.D.d.U.D.u."; for (int i = 0; i < 16; ++i) customSteps[(size_t) i].store (def[i]); }
    stackParts.reserve (8);
    blockNotes.reserve (16); prevBlock.reserve (16);
}

Hamamatsu108Processor::~Hamamatsu108Processor()
{
    stopTimer();
#if HAMA_MTS
    if (mtsClient != nullptr) MTS_DeregisterClient ((MTSClient*) mtsClient);
#endif
}

float Hamamatsu108Processor::noteHz (int midiNote) const
{
#if HAMA_MTS
    if (mtsEnabled.load() && mtsClient != nullptr && MTS_HasMaster ((MTSClient*) mtsClient))
        return (float) MTS_NoteToFrequency ((MTSClient*) mtsClient, (char) juce::jlimit (0, 127, midiNote), -1);
#endif
    // FINE (string): a few cents up or down
    return tuningA4.load() * std::pow (2.0f, ((float) (midiNote - 69) + apvts.getRawParameterValue ("strFine")->load() * 0.01f) / 12.0f);
}

bool Hamamatsu108Processor::mtsConnected() const
{
#if HAMA_MTS
    return mtsClient != nullptr && MTS_HasMaster ((MTSClient*) mtsClient);
#else
    return false;
#endif
}

juce::String Hamamatsu108Processor::ccName (int cc) const { return "CC " + juce::String (cc); }

int Hamamatsu108Processor::ccForParam (int paramIndex) const
{
    for (int i = 0; i < 128; ++i) if (ccMap[(size_t) i].load() == paramIndex) return i;
    return -1;
}

void Hamamatsu108Processor::timerCallback()
{
    if (const int pat = pendingPattern.exchange (-1); pat >= 0)
        if (auto* q = apvts.getParameter ("strumPattern")) q->setValueNotifyingHost (q->convertTo0to1 ((float) pat));
    if (const int md = pendingMode.exchange (-1); md >= 0)
        if (auto* q = apvts.getParameter ("strumMode")) q->setValueNotifyingHost (q->convertTo0to1 ((float) md));
    // MIDI learn: apply controller moves on the message thread
    const auto lo = ccDirtyLo.exchange (0), hi = ccDirtyHi.exchange (0);
    if (lo == 0 && hi == 0) return;
    const auto& params = getParameters();
    for (int cc = 0; cc < 128; ++cc)
    {
        const bool dirty = cc < 64 ? (lo >> cc) & 1 : (hi >> (cc - 64)) & 1;
        if (! dirty) continue;
        const int idx = ccMap[(size_t) cc].load();
        if (idx >= 0 && idx < params.size())
            params[idx]->setValueNotifyingHost (ccPending[(size_t) cc].load());
    }
}

static juce::File settingsFile()
{
    const auto kuprell = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Kuprell");
    const auto dir = kuprell.getChildFile ("Hamamatsu 144"), old = kuprell.getChildFile ("Hamamatsu 108");
    if (! dir.exists() && old.isDirectory()) old.copyDirectoryTo (dir);   // renamed from Hamamatsu 108: keep settings and presets
    return dir.getChildFile ("settings.xml");
}

void Hamamatsu108Processor::saveUserDefaults() const
{
    juce::XmlElement xml ("HAMAMATSU108SETTINGS");
    xml.setAttribute ("skin", skin.load());
    xml.setAttribute ("zoneOctave", zoneOctave.load());
    xml.setAttribute ("chordZones", chordZones.load());
    xml.setAttribute ("version", 8);
    xml.setAttribute ("tuning", tuningA4.load());
    xml.setAttribute ("voices", maxVoices.load());
    xml.setAttribute ("mts", mtsEnabled.load());
    xml.setAttribute ("keyswitches", keyswitches.load());
    xml.setAttribute ("pcKeys", pcKeys.load());
    xml.setAttribute ("midiOut", midiOut.load());
    settingsFile().getParentDirectory().createDirectory();
    xml.writeTo (settingsFile());
}

void Hamamatsu108Processor::loadUserDefaults()
{
    if (auto xml = juce::parseXML (settingsFile()))
    {
        // from settings version 3 the default look is Graphite
        // from settings version 5 the default look is Ivory 84 (the 80s instrument)
        skin.store (xml->getIntAttribute ("version", 1) >= 8 ? 14 : 14);   // v8: Concrete is the default
        zoneOctave.store (juce::jlimit (0, 3, xml->getIntAttribute ("zoneOctave", 1)));
        // from settings version 2 every key plays by default (chord zones are an option): piano-roll notes in
        // octave 2-3 used to fall into the zones and stay silent
        chordZones.store (xml->getIntAttribute ("version", 1) >= 2 ? xml->getBoolAttribute ("chordZones", false) : false);
        tuningA4.store ((float) juce::jlimit (400.0, 480.0, xml->getDoubleAttribute ("tuning", 440.0)));
        maxVoices.store (juce::jlimit (8, numVoices, xml->getIntAttribute ("voices", numVoices)));
        mtsEnabled.store (xml->getBoolAttribute ("mts", true));
        keyswitches.store (xml->getBoolAttribute ("keyswitches", true));
        pcKeys.store (xml->getBoolAttribute ("pcKeys", false));
        // MIDI OUT is off by default from settings version 4: FL Studio records the plugin's notes into the pattern otherwise
        midiOut.store (xml->getIntAttribute ("version", 1) >= 4 && xml->getBoolAttribute ("midiOut", false));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout Hamamatsu108Processor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto fl = [&] (const char* id, const char* name, float lo, float hi, float def, std::function<String (float, int)> text, float skewCentre = -1.0f)
    {
        NormalisableRange<float> r (lo, hi);
        if (skewCentre > 0.0f) r.setSkewForCentre (skewCentre);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (std::move (text))));
    };

    fl ("filterCutoff", "Filter Cutoff", 60.0f, 20000.0f, 20000.0f, [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (juce::roundToInt (v)) + " Hz"; }, 1200.0f);
    fl ("filterReso", "Filter Resonance", 0.0f, 1.0f, 0.1f, pct);
    fl ("drive", "Saturation", 0.0f, 1.0f, 0.0f, pct);
    fl ("polish", "Polish", 0.0f, 1.0f, 0.4f, pct);
    fl ("width", "Stereo Width", 0.0f, 2.0f, 1.0f, pct);
    fl ("spread", "Stereo Spread", 0.0f, 1.0f, 0.5f, pct);
    fl ("master", "Master Volume", -60.0f, 6.0f, 0.0f, [] (float v, int) { return String (v, 1) + " dB"; });
    fl ("presetTrim", "Preset Level", -12.0f, 24.0f, 0.0f, [] (float v, int) { return String (v, 1) + " dB"; });   // evens out the loudness of the presets

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "harpVoice", 1 }, "Harp Voice", harp::voiceNames(), 0));
    fl ("harpTone", "Harp Tone", 0.0f, 1.0f, 0.62f, pct);
    fl ("harpDecay", "Harp Decay", 0.15f, 8.0f, 1.8f, [] (float v, int) { return String (v, 2) + " s"; }, 1.5f);
    fl ("harpAttack", "Harp Attack", 0.0015f, 3.0f, 0.0015f, [] (float v, int) { return v < 0.01f ? String ("pluck") : String (roundToInt (v * 1000.0f)) + " ms"; }, 0.3f);
    fl ("harpSustain", "Harp Sustain", 0.0f, 1.0f, 0.0f, pct);
    fl ("harpShape", "Layer A Shape", 0.0f, 1.0f, 0.4f, pct);
    // HARP STRING engine: the touch and the body
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "strExciter", 1 }, "Touch", StringArray { "Finger", "Nail", "Pick", "Felt", "Bow" }, 1));
    fl ("strHard", "Hardness", 0.0f, 1.0f, 0.5f, pct);
    // CONSOLE LFOs: delay, fade-in, offset and key trigger; where they go (pitch, filter)
    for (int i = 1; i <= 2; ++i)
    {
        const String n ("lfo" + String (i));
        fl ((n + "Delay").toRawUTF8(), ("LFO " + String (i) + " Delay").toRawUTF8(), 0.0f, 1.0f, 0.0f, [] (float v, int) { return String (v * 3.0f, 2) + " s"; });
        fl ((n + "Fade").toRawUTF8(), ("LFO " + String (i) + " Fade").toRawUTF8(), 0.0f, 1.0f, 0.0f, [] (float v, int) { return String (v * 3.0f, 2) + " s"; });
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { n + "Offset", 1 }, "LFO " + String (i) + " Offset", StringArray { "+", "0", "-" }, 1));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { n + "Trig", 1 }, "LFO " + String (i) + " Key Trig", false));
    }
    // CONSOLE: pluck envelope, amp extras, string extras, filter modes, colour, SPACE, hand, LFO 2 extras, layer solo
    {
        const char* pe[] = { "pT1", "pL1", "pT2", "pL2", "pT3", "pL3", "pT4", "pTail" };
        const char* pn[] = { "Pluck T1", "Pluck L1", "Pluck T2", "Pluck L2", "Pluck T3", "Pluck L3", "Pluck T4", "Pluck Tail" };
        const float pd[] = { 0.05f, 1.0f, 0.3f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f };
        for (int i = 0; i < 8; ++i) fl (pe[i], pn[i], 0.0f, 1.0f, pd[i], pct);
    }
    fl ("pVelo", "Pluck Velocity", -1.0f, 1.0f, 0.0f, pct);
    fl ("ampLevel", "Layer A Level", -24.0f, 6.0f, 0.0f, [] (float v, int) { return String (v, 1) + " dB"; });
    fl ("ampVelo", "Velocity Sense", -1.0f, 1.0f, 0.0f, pct);
    fl ("ampHold", "Hold", 0.0f, 1.0f, 0.0f, [] (float v, int) { return String (v * 2.0f, 2) + " s"; });
    fl ("strFine", "Fine Tune", -50.0f, 50.0f, 0.0f, [] (float v, int) { return String (v, 1) + " ct"; });
    fl ("strSympath", "Sympathetic", 0.0f, 1.0f, 0.0f, pct);
    fl ("strBuzz", "Buzz", 0.0f, 1.0f, 0.0f, pct);
    fl ("strHarmonic", "Harmonic", 0.0f, 1.0f, 0.0f, pct);
    fl ("strRelTime", "Damper Release", 0.0f, 1.0f, 0.5f, pct);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "strDamper", 1 }, "Damper", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "strPedal", 1 }, "Pedal", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "filtMode", 1 }, "Filter Mode", StringArray { "HPF", "BPF", "LPF" }, 2));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "filt24", 1 }, "Filter 24 dB", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "filtTrack", 1 }, "Filter Key Track", false));
    fl ("filtKey", "Filter Key Follow", 0.0f, 1.0f, 0.5f, pct);
    fl ("filtVelo", "Filter Velocity", -1.0f, 1.0f, 0.0f, pct);
    fl ("bodyTilt", "Tilt", -1.0f, 1.0f, 0.0f, pct);
    fl ("morphWood", "Wood", 0.0f, 1.0f, 0.0f, pct);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "spRoom", 1 }, "Space Room", StringArray { "Hall", "Plate", "Cave", "Void" }, 0));
    fl ("spSize", "Space Size", 0.0f, 1.0f, 0.5f, pct);
    fl ("spTone", "Space Tone", 0.0f, 1.0f, 0.5f, pct);
    fl ("spPre", "Space Pre-delay", 0.0f, 1.0f, 0.1f, pct);
    fl ("spMix", "Space Mix", 0.0f, 1.0f, 0.0f, pct);
    fl ("spWidth", "Space Width", 0.0f, 1.0f, 0.8f, pct);
    fl ("spDuck", "Space Duck", 0.0f, 1.0f, 0.0f, pct);
    fl ("spMod", "Space Mod", 0.0f, 1.0f, 0.2f, pct);
    fl ("spLowcut", "Space Low Cut", 0.0f, 1.0f, 0.2f, pct);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "spReverse", 1 }, "Space Reverse", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "spTail", 1 }, "Space Tail", true));
    fl ("ratchet", "Ratchet", 0.0f, 1.0f, 0.0f, pct);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "mono", 1 }, "Mono", false));
    fl ("lfo2Phase", "LFO 2 Phase", 0.0f, 1.0f, 0.0f, pct);
    fl ("lfo2Smooth", "LFO 2 Smooth", 0.0f, 1.0f, 0.0f, pct);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "lfo2Bar", 1 }, "LFO 2 Bar Restart", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "lfo2Invert", 1 }, "LFO 2 Invert", false));
    for (auto* id : { "soloA", "soloB", "soloBass", "soloChord" })
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, String ("Solo ") + (String (id).substring (4)), false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "pitchModSrc", 1 }, "Pitch Mod Source", StringArray { "LFO 1", "LFO 2", "Env" }, 0));
    fl ("pitchModAmt", "Pitch Mod Depth", -1.0f, 1.0f, 0.0f, [] (float v, int) { return String (v * 3.0f, 2) + " st"; });
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "filtLfoSel", 1 }, "Filter LFO", StringArray { "LFO 1", "LFO 2" }, 0));
    fl ("filtLfoAmt", "Filter LFO Depth", -1.0f, 1.0f, 0.0f, [] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; });
    fl ("strPos", "Pluck Position", 0.0f, 1.0f, 0.3f, pct);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bodyMat", 1 }, "Body Material", StringArray { "Wood", "Glass", "Steel", "Stone" }, 0));
    fl ("bodySize", "Body Size", 0.0f, 1.0f, 0.5f, pct);
    fl ("bodyDamp", "Body Damping", 0.0f, 1.0f, 0.3f, pct);
    fl ("bodyRing", "Body Ring", 0.0f, 1.0f, 0.4f, pct);
    fl ("bodyMix", "Body Mix", 0.0f, 1.0f, 0.35f, pct);
    // layer B: its own engine settings (DUAL)
    fl ("bTone", "Layer B Tone", 0.0f, 1.0f, 0.55f, pct);
    fl ("bShape", "Layer B Shape", 0.0f, 1.0f, 0.4f, pct);
    fl ("bAttack", "Layer B Attack", 0.0015f, 3.0f, 0.0015f, [] (float v, int) { return v < 0.01f ? String ("pluck") : String (roundToInt (v * 1000.0f)) + " ms"; }, 0.3f);
    fl ("bDecay", "Layer B Decay", 0.15f, 8.0f, 2.4f, [] (float v, int) { return String (v, 2) + " s"; }, 1.5f);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "bOctave", 1 }, "Layer B Octave", -2, 2, 0,
                                                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return (v > 0 ? "+" : "") + String (v) + " oct"; })));
    fl ("bLevel", "Layer B Level", -24.0f, 6.0f, 0.0f, [] (float v, int) { return String (v, 1) + " dB"; });
    fl ("bDetune", "Layer B Detune", -50.0f, 50.0f, 0.0f, [] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v)) + " ct"; });
    fl ("harpRelease", "Harp Release", 0.02f, 8.0f, 8.0f, [] (float v, int) { return v > 7.9f ? String ("ring") : String (v, 2) + " s"; }, 0.4f);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "harpRange", 1 }, "Harp Range", 1, 4, 3,
                                                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + " oct"; })));

    fl ("vibRate", "Vibrato Rate", 0.5f, 9.0f, 5.2f, [] (float v, int) { return String (v, 1) + " Hz"; });
    fl ("vibDepth", "Vibrato Depth", 0.0f, 1.0f, 0.0f, pct);

    // step 4: model, FX slots, modulation
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "model", 1 }, "Model", modelNames(), 1));
    {
        const int defType[] = { 7, 12, 14, 0, 0, 0, 0, 0 };
        const float defA[] = { 0.3f, 0.45f, 0.8f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        const float defB[] = { 0.45f, 0.4f, 0.45f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        const float defMix[] = { 0.45f, 0.22f, 0.55f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        for (int i = 0; i < fx::numSlots; ++i)
        {
            const String n (i + 1), id ("fx" + n);
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id + "Type", 1 }, "FX " + n + " Type", fx::names(), defType[i]));
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { id + "On", 1 }, "FX " + n + " On", true));
            fl ((id + "A").toRawUTF8(), ("FX " + n + " A").toRawUTF8(), 0.0f, 1.0f, defA[i], pct);
            fl ((id + "B").toRawUTF8(), ("FX " + n + " B").toRawUTF8(), 0.0f, 1.0f, defB[i], pct);
            fl ((id + "Mix").toRawUTF8(), ("FX " + n + " Mix").toRawUTF8(), 0.0f, 1.0f, defMix[i], pct);
        }
    }
    for (int i = 1; i <= 2; ++i)
    {
        const String id ("lfo" + String (i));
        fl ((id + "Rate").toRawUTF8(), ("LFO " + String (i) + " Rate").toRawUTF8(), 0.02f, 20.0f, i == 1 ? 0.8f : 3.0f, [] (float v, int) { return String (v, 2) + " Hz"; }, 1.0f);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id + "Shape", 1 }, "LFO " + String (i) + " Shape", mod::lfoShapeNames(), 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id + "Sync", 1 }, "LFO " + String (i) + " Sync", mod::lfoSyncNames(), 0));
    }
    fl ("funcRate", "Function Rate", 0.02f, 10.0f, 0.5f, [] (float v, int) { return String (v, 2) + " Hz"; }, 0.8f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "funcMode", 1 }, "Function Mode", mod::funcModeNames(), 0));
    {
        const float pts[] = { 0.0f, 0.85f, 1.0f, 0.7f, 0.55f, 0.4f, 0.2f, 0.0f };
        for (int i = 0; i < mod::funcPoints; ++i)
            fl (("funcP" + String (i + 1)).toRawUTF8(), ("Function Point " + String (i + 1)).toRawUTF8(), 0.0f, 1.0f, pts[i], pct);
        for (int i = 1; i < mod::funcPoints - 1; ++i)   // time of the inner points (first = 0, last = 1)
            fl (("funcT" + String (i + 1)).toRawUTF8(), ("Function Time " + String (i + 1)).toRawUTF8(), 0.0f, 1.0f, (float) i / (mod::funcPoints - 1), pct);
    }
    fl ("randRate", "Random Rate", 0.05f, 20.0f, 2.0f, [] (float v, int) { return String (v, 2) + " Hz"; }, 1.5f);
    fl ("randSmooth", "Random Smooth", 0.0f, 1.0f, 0.5f, pct);
    fl ("randChaos", "Random Chaos", 0.0f, 1.0f, 0.3f, pct);
    // quick routing in every modulator panel: one target + depth, no matrix needed
    {
        const char* ids[] = { "lfo1", "lfo2", "func", "rand" };
        const char* names[] = { "LFO 1", "LFO 2", "Function", "Random" };
        const int defDst[] = { 21, 23, 22, 24 };   // Harp Filter, Harp Pan, Harp Volume, Harp Pitch
        for (int i = 0; i < 4; ++i)
        {
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { String (ids[i]) + "QDst", 1 }, String (names[i]) + " Target", mod::targetNames(), defDst[i]));
            fl ((String (ids[i]) + "QAmt").toRawUTF8(), (String (names[i]) + " Depth").toRawUTF8(), -1.0f, 1.0f, 0.0f,
                [] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; });
        }
    }
    for (int i = 1; i <= mod::matrixSlots; ++i)
    {
        const String n (i);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mod" + n + "Src", 1 }, "Mod " + n + " Source", mod::sourceNames(), 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mod" + n + "Dst", 1 }, "Mod " + n + " Target", mod::targetNames(), 0));
        fl (("mod" + n + "Amt").toRawUTF8(), ("Mod " + n + " Amount").toRawUTF8(), -1.0f, 1.0f, 0.0f, [] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; });
    }
    for (int i = 1; i <= mod::morphSlots; ++i)
    {
        const String n (i);
        const int defDst = i == 1 ? 17 : i == 2 ? 2 : 0;   // morph: more reverb, longer decay
        const float defAmt = i == 1 ? 0.3f : i == 2 ? 0.25f : 0.0f;
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "morph" + n + "Dst", 1 }, "Morph " + n + " Target", mod::targetNames(), defDst));
        fl (("morph" + n + "Amt").toRawUTF8(), ("Morph " + n + " Amount").toRawUTF8(), -1.0f, 1.0f, defAmt, [] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; });
    }

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "strumMode", 1 }, "Strum Mode", strumModeNames(), 3));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "strumRate", 1 }, "Strum Rate", strumRateNames(), 2));
    fl ("strumFeel", "Strum Feel", 0.0f, 1.0f, 0.62f, pct);
    fl ("strumSpeed", "Strum Speed", 4.0f, 80.0f, 22.0f, [] (float v, int) { return String (roundToInt (v)) + " ms"; }, 20.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "strumPattern", 1 }, "Strum Pattern", strumPatternNames(), 2));
    fl ("strumSwing", "Strum Swing", 0.0f, 1.0f, 0.0f, pct);
    fl ("strumVary", "Strum Variation", 0.0f, 1.0f, 0.35f, pct);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "strumShape", 1 }, "Stroke Shape", StringArray { "Linear", "Natural", "Fast start", "Fast finish" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "strumSpeedMul", 1 }, "Pattern Speed", StringArray { "Half", "Normal", "Double" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "strumRestart", 1 }, "Pattern Start", StringArray { "Follow the bar", "Restart on chord" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "keysPlay", 1 }, "Keys Play", keysPlayNames(), 0));   // Notes: every key is exactly one note

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "hold", 1 }, "Hold", true));
    // STRUM KEYS: one octave of the keyboard plays hand gestures (any keyboard size: pick the octave you can reach)
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "strumZone", 1 }, "Strum Keys", strumZoneNames(), 4));   // Off: every key plays its own note; turn on in settings
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "motionGate", 1 }, "Motion Notes", StringArray { "Ring", "Long", "Medium", "Tight" }, 2));
    // SWITCHES: 16 one-touch moves on the main page, each with its own amount
    for (int i = 0; i < numSwitches; ++i)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "sw" + String (i) + "On", 1 }, switchNames()[i] + " On", false));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sw" + String (i) + "Amt", 1 }, switchNames()[i] + " Amount",
                                                           NormalisableRange<float> (0.0f, 1.0f), 0.6f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    }

    // step 3: arpeggio, dual, chord layer, bass, memory
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "arpPattern", 1 }, "Arp Pattern", arpPatternNames(), 0));
    fl ("arpChance", "Arp Chance", 0.0f, 1.0f, 1.0f, pct);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "arpOctaves", 1 }, "Arp Octaves", 1, 4, 2));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "harpOctave", 1 }, "Octave", -3, 2, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "gridOn", 1 }, "Motion", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "gridRate", 1 }, "Motion Rate", strumRateNames(), 4));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "gridLen", 1 }, "Motion Steps", 2, 16, 16));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "motionType", 1 }, "Motion Type", juce::StringArray { "Grid", "Lines", "Rhythm", "Strings", "Plate" }, 4));
    // STROKE switches (main page, combine freely): how a chord is strummed when you play it
    for (auto* id : { "strokeDown", "strokeHalf", "strokeBounce", "strokeWaver", "strokeMiddle", "strokeSlow" })
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, String (id).fromFirstOccurrenceOf ("stroke", false, false), false));   // the whole harp an octave down (bass) or up
    fl ("arpGate", "Arp Gate", 0.05f, 1.0f, 1.0f, pct);
    fl ("arpSwing", "Arp Swing", 0.0f, 1.0f, 0.0f, pct);
    fl ("arpVelScale", "Arp Dynamics", 0.0f, 1.0f, 1.0f, pct);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "gateOn", 1 }, "Gate On", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "gateRate", 1 }, "Gate Rate", StringArray { "1/8", "1/16", "1/32", "1/8T", "1/16T", "1/2", "1/4", "1/2T", "1/4T", "1/32T" }, 1));
    fl ("gateDepth", "Gate Depth", 0.0f, 1.0f, 0.85f, pct);
    fl ("gateSmooth", "Gate Smooth", 1.0f, 60.0f, 6.0f, [] (float v, int) { return String (roundToInt (v)) + " ms"; }, 30.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "gateTarget", 1 }, "Gate Target", StringArray { "Harp", "Everything" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "arpRegen", 1 }, "Arp Regen", arpRegenNames(), 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "dual", 1 }, "Dual", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "harpVoice2", 1 }, "Dual Voice", harp::voiceNames(), 9));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "chordType", 1 }, "Chord Layer", layers::chordTypeNames(), 0));
    fl ("chordLevel", "Chord Level", -48.0f, 0.0f, -14.0f, [] (float v, int) { return String (v, 1) + " dB"; });
    fl ("chordFilter", "Chord Filter", 0.0f, 1.0f, 0.55f, pct);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "voicing", 1 }, "Voicing", layers::voicingNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bassType", 1 }, "Bass", layers::bassTypeNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bassMode", 1 }, "Bass Pattern", layers::bassModeNames(), 0));
    fl ("bassLevel", "Bass Level", -48.0f, 0.0f, -10.0f, [] (float v, int) { return String (v, 1) + " dB"; });
    fl ("bassTone", "Bass Tone", 0.0f, 1.0f, 0.4f, pct);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "memStep", 1 }, "Memory Step", memStepNames(), 1));
    fl ("morph", "Morph", 0.0f, 1.0f, 0.35f, pct);
    fl ("macroAge", "Age", 0.0f, 1.0f, 0.0f, pct);
    fl ("macroDream", "Dream", 0.0f, 1.0f, 0.0f, pct);
    fl ("macroWobble", "Wobble", 0.0f, 1.0f, 0.0f, pct);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "keyRoot", 1 }, "Key", 0, 11, 0,
                                                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (chords::rootName (v)); })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "keyMode", 1 }, "Key Mode", StringArray { "Major", "Minor" }, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "transpose", 1 }, "Transpose", -12, 12, 0,
                                                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return (v > 0 ? "+" : "") + String (v); })));
    return layout;
}

void Hamamatsu108Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& v : voices) v.prepare (sampleRate);
    for (auto& v : chordVoices) { v.prepare (sampleRate); v.kill(); }
    for (auto& v : bassVoices) { v.prepare (sampleRate); v.kill(); }
    layerChord = bassChord = -2;
    numEvents = 0;
    numReleases = 0;
    pitchMul.assign ((size_t) juce::jmax (32, samplesPerBlock), 1.0f);
    gateBuf.assign ((size_t) juce::jmax (32, samplesPerBlock), 1.0f);
    midiOutBuffer.ensureSize (4096);
    numPendingOffs = 0;
    for (auto& ch : outOn) ch.fill (0);
    limLook = juce::jlimit (16, limLookMax, (int) std::round (sampleRate * 0.0015));
    soundboard.prepare (sampleRate); boardModel = -1;
    limDelL.fill (0.0f); limDelR.fill (0.0f); limNeed.fill (1.0f); limPos = 0; limGain = 1.0f;
    setLatencySamples (limLook);

    for (auto& u : fxUnits) u.prepare (sampleRate, juce::jmax (32, samplesPerBlock));
    for (auto& u : switchFx) u.prepare (sampleRate, juce::jmax (32, samplesPerBlock));
    modelChorusL.prepare ((int) (sampleRate * 0.03)); modelChorusR.prepare ((int) (sampleRate * 0.03));
    dreamReverb.prepare (sampleRate);
    space.prepare (sampleRate, samplesPerBlock);
    dreamL.assign ((size_t) juce::jmax (32, samplesPerBlock), 0.0f);
    dreamR.assign ((size_t) juce::jmax (32, samplesPerBlock), 0.0f);
    work.setSize (2, juce::jmax (32, samplesPerBlock));
    for (auto* s : { &masterGain })
        s->reset (sampleRate, 0.05);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param (apvts, "master") + param (apvts, "presetTrim")));
}

bool Hamamatsu108Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

//==============================================================================
int Hamamatsu108Processor::resolveChord() const
{
    if (rootHeldMask != 0 && lastRoot >= 0)
    {
        const int type = numTypesHeld > 0 ? typeHeldStack[numTypesHeld - 1] : 0;
        // several root keys held = a stacked chord (C + E keys = C and E chords together)
        std::vector<int> parts { chords::index (lastRoot, type) };
        for (int r = 0; r < 12; ++r)
            if (r != lastRoot && (rootHeldMask & (1 << r))) parts.push_back (chords::index (r, type));
        return chords::stack (parts);
    }
    if (holdChord >= 0) return holdChord;
    return latchedChord;
}

// the chord that best explains the held piano-roll notes, preferring chords of the current key
int Hamamatsu108Processor::chordFromHeldNotes()
{
    int mask = 0, lowest = 128;
    for (int n = 0; n < 128; ++n)
        if (heldMelodic[(size_t) n]) { mask |= 1 << (n % 12); lowest = juce::jmin (lowest, n); }
    return chordFromMask (mask, lowest);
}

int Hamamatsu108Processor::chordFromMask (int mask, int lowest)
{
    if (mask == 0) return -1;
    const int keyRoot = juce::roundToInt (param (apvts, "keyRoot"));
    const bool minorKey = param (apvts, "keyMode") > 0.5f;
    static const int majDeg[7][2] = { { 0, 0 }, { 2, 1 }, { 4, 1 }, { 5, 0 }, { 7, 0 }, { 9, 1 }, { 11, 5 } };
    static const int minDeg[7][2] = { { 0, 1 }, { 2, 5 }, { 3, 0 }, { 5, 1 }, { 7, 1 }, { 8, 0 }, { 10, 0 } };
    const auto& deg = minorKey ? minDeg : majDeg;
    auto pcs = [] (int m) { int c = 0; for (int i = 0; i < 12; ++i) c += (m >> i) & 1; return c; };
    if (pcs (mask) == 1)
    {
        // one key = the chord of the key on that degree (C in C major = C, D = Dm, E = Em ...), outside the key = major
        const int pc = lowest % 12;
        for (const auto& d : deg)
            if ((keyRoot + d[0]) % 12 == pc) return chords::index (pc, d[1]);
        return chords::index (pc, 0);
    }
    int best = -1, bestScore = -1000;
    auto consider = [&] (int idx, int bonus)
    {
        const int cm = chords::pitchClassMask (idx);
        const int hit = pcs (cm & mask), miss = pcs (mask & ~cm);
        int score = hit * 10 - miss * 12 + bonus;
        if (chords::rootOf (idx) == lowest % 12) score += 6;   // the bass note is usually the root
        if (score > bestScore) { bestScore = score; best = idx; }
    };
    for (const auto& d : deg) consider (chords::index ((keyRoot + d[0]) % 12, d[1]), 3);
    for (int t = 0; t < chords::numTypes; ++t)
        for (int r = 0; r < 12; ++r) consider (chords::index (r, t), t <= 1 ? 0 : -4);
    return best;
}

void Hamamatsu108Processor::handleMidi (const juce::MidiMessage& m, int samplePos)
{
    // EXACT NOTES: MIDI channel 16 always plays the written pitch, whatever KEYS / MOTION / Flourish do.
    // DRAG MIDI writes its clips on channel 16 (in FL: colour group 16), so a dragged clip plays back exactly as you heard it.
    if ((m.isNoteOn() || m.isNoteOff()) && m.getChannel() == 16)
    {
        const int n = m.getNoteNumber();
        if (m.isNoteOn()) schedulePitch (samplePos, n, m.getFloatVelocity());
        else if (const float rel = noteRelease(); rel > 0.0f && numReleases < (int) releases.size())
            releases[(size_t) numReleases++] = { samplePos, n, rel };
        return;
    }

    // STRUM KEYS (one octave, default C0 = the piano roll's lowest): every key is a hand gesture on the chord that is on,
    // like drums in the piano roll. Velocity = force. See strumGestureNames() for the 12 keys.
    const int zone = juce::roundToInt (param (apvts, "strumZone"));
    const int zoneBase = zone * 12;
    if ((m.isNoteOn() || m.isNoteOff()) && keyswitches.load() && zone < 4 && m.getNoteNumber() >= zoneBase && m.getNoteNumber() < zoneBase + 12)
    {
        const int n = m.getNoteNumber() - zoneBase;
        const float vel = m.getFloatVelocity();
        if (m.isNoteOn())
        {
            static const int dir[] = { 1, 1, -1, -1, 1, 1, -1 }, kind[] = { 0, 1, 0, 1, 4, 2, 3 };   // kind: 0 full, 1 slow, 2 low half, 3 high half, 4 middle out, 5 gliss
            if (n <= 6 && numSplitHits < (int) splitHits.size()) splitHits[(size_t) numSplitHits++] = { samplePos, kind[n], vel, dir[n] };
            else if (n == 7) { ++motionKeysHeld; motionRestart = true; }
            else if (n == 8) { motionOneShot = (int) (4.0 * 60.0 / bpm * getSampleRate()); motionRestart = true; }
            else if (n == 9) { tremoloHeld = true; tremoloVel = vel; tremoloLeft = 0; tremoloUp = true; }
            else if (n == 10 && numSplitHits + 1 < (int) splitHits.size()) splitHits[(size_t) numSplitHits++] = { samplePos, 5, vel, 1 };
            else if (n == 11) dampNow = true;
            lastVelocity = vel;
        }
        else
        {
            if (n == 7) motionKeysHeld = juce::jmax (0, motionKeysHeld - 1);
            if (n == 9) tremoloHeld = false;
        }
        return;   // the strum octave never sounds as notes
    }
    if (m.isPitchWheel())
    {
        // pitch wheel strums the strings (like dragging across the plate)
        wheelStrum (samplePos, (float) m.getPitchWheelValue() / 16383.0f, bendPos);
        return;
    }
    const bool splitMode = juce::roundToInt (param (apvts, "keysPlay")) == 6 && ! chordZones.load();
    if (splitMode && (m.isNoteOn() || m.isNoteOff()))
    {
        // SPLIT: left hand = chord, right hand = strings (white keys) and whole strums (black keys)
        const int n = m.getNoteNumber();
        if (n < splitPoint)
        {
            if (m.isNoteOn())
            {
                if (! splitLeft[(size_t) n]) { splitLeft[(size_t) n] = true; ++splitHeld; }
                int mask = 0, lowest = 128;
                for (int k = 0; k < splitPoint; ++k)
                    if (splitLeft[(size_t) k]) { mask |= 1 << (k % 12); lowest = juce::jmin (lowest, k); }
                splitChord = chordFromMask (mask, lowest);
                latchedChord = -1; holdChord = -1; stackParts.clear();
                lastVelocity = m.getFloatVelocity();
                // the left hand sounds too: one soft down stroke of the new chord (unless the progression plays)
                if (numSplitHits < (int) splitHits.size()) splitHits[(size_t) numSplitHits++] = { samplePos, 0, m.getFloatVelocity() * 0.85f, 2 };
            }
            else if (splitLeft[(size_t) n]) { splitLeft[(size_t) n] = false; --splitHeld; }
        }
        else if (m.isNoteOn())
        {
            // right hand: the exact written note (a clean melody, like any instrument). Strokes: the STRUM KEYS octave.
            schedulePitch (samplePos, n, m.getFloatVelocity());
        }
        else if (m.isNoteOff())
        {
            if (const float rel = noteRelease(); rel > 0.0f && numReleases < (int) releases.size())
                releases[(size_t) numReleases++] = { samplePos, n, rel };
        }
        return;
    }
    if (m.isNoteOn())
    {
        const int n = m.getNoteNumber();
        lastVelocity = m.getFloatVelocity();
        if (chordZones.load() && n >= rootZoneLow() && n <= rootZoneHigh())
        {
            lastRoot = n - rootZoneLow();
            rootHeldMask |= 1 << lastRoot;
            latchedChord = -1;
        }
        else if (chordZones.load() && n >= typeZoneLow() && n <= typeZoneHigh())
        {
            if (numTypesHeld < 16) typeHeldStack[numTypesHeld++] = n - typeZoneLow();
        }
        else
        {
            // melodic note: in ARP mode held notes are arpeggiated (piano roll or keyboard),
            // otherwise it plays exactly the written pitch
            if (! heldMelodic[(size_t) n])
            {
                heldMelodic[(size_t) n] = true; ++heldMelodicCount;
                if (pressCount < (int) pressOrder.size()) pressOrder[(size_t) pressCount++] = n;
            }
            const int strumModeNow = juce::roundToInt (param (apvts, "strumMode"));
            const int keysPlay = juce::roundToInt (param (apvts, "keysPlay"));
            if ((strumModeNow == 0 || strumModeNow == 3) && keysPlay >= 4)
            {
                // FLOURISH: one key = a whole harp run of the chord on that key (the octave picks the chord type)
                flourish (samplePos, n, m.getFloatVelocity(), keysPlay == 4);
            }
            else if (strumModeNow == 0 && keysPlay > 0)
            {
                // KEYS: the held keys are the strings. Strum = every key press strums them (a chord pressed
                // together = one stroke, harder = stronger and faster); Pattern = the LIVE pattern plays them
                keyNotes[(size_t) n] = true;
                if (keysPlay == 1)
                {
                    if (keyStrumAt < 0) { keyStrumAt = samplePos + (int) (0.012 * sr); keyStrumVel = 0.0f; }
                    keyStrumVel = juce::jmax (keyStrumVel, m.getFloatVelocity());
                }
            }
            else if (strumModeNow != 1)
                schedulePitch (samplePos, n, m.getFloatVelocity());
        }
    }
    else if (m.isNoteOff())
    {
        const int n = m.getNoteNumber();
        if (const float rel = noteRelease(); rel > 0.0f && numReleases < (int) releases.size())
        {
            const int kp = juce::roundToInt (param (apvts, "keysPlay"));
            // RELEASE: the sound ends when the key goes up (Notes: that note, Flourish: the whole run)
            if (kp == 0 && ! (chordZones.load() && n <= typeZoneHigh()))
                releases[(size_t) numReleases++] = { samplePos, n, rel };
            else if (kp >= 4)
                releases[(size_t) numReleases++] = { samplePos, -1, rel };
        }
        if (heldMelodic[(size_t) n])
        {
            heldMelodic[(size_t) n] = false; --heldMelodicCount;
            for (int i = 0; i < pressCount; ++i)
                if (pressOrder[(size_t) i] == n) { for (int j = i; j < pressCount - 1; ++j) pressOrder[(size_t) j] = pressOrder[(size_t) j + 1]; --pressCount; break; }
        }
        if (chordZones.load() && n >= rootZoneLow() && n <= rootZoneHigh())
        {
            const int r = n - rootZoneLow();
            if (rootHeldMask == (1 << r) && param (apvts, "hold") > 0.5f)
                holdChord = resolveChord();   // keep the chord after release
            rootHeldMask &= ~(1 << r);
            if (r == lastRoot && rootHeldMask != 0)
                for (int i = 0; i < 12; ++i)
                    if (rootHeldMask & (1 << i)) { lastRoot = i; break; }
            if (rootHeldMask == 0 && param (apvts, "hold") <= 0.5f)
                holdChord = -1;
        }
        else if (chordZones.load() && n >= typeZoneLow() && n <= typeZoneHigh())
        {
            const int t = n - typeZoneLow();
            for (int i = 0; i < numTypesHeld; ++i)
                if (typeHeldStack[i] == t)
                {
                    for (int j = i; j < numTypesHeld - 1; ++j) typeHeldStack[j] = typeHeldStack[j + 1];
                    --numTypesHeld;
                    break;
                }
        }
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 64) sustainPedal = m.getControllerValue() >= 64;
        if (cc == 1)
        {
            modWheel = m.getControllerValue() / 127.0f;
            wheelStrum (samplePos, modWheel, wheelPos);   // mod wheel = a finger across the strings
        }
        const int learn = learnParam.load();
        if (learn >= 0 && cc != 120 && cc != 123)
        {
            for (auto& c : ccMap) if (c.load() == learn) c.store (-1);
            ccMap[(size_t) cc].store (learn);
            learnParam.store (-1);
        }
        if (ccMap[(size_t) cc].load() >= 0)
        {
            ccPending[(size_t) cc].store (m.getControllerValue() / 127.0f);
            if (cc < 64) ccDirtyLo.fetch_or (1ull << cc); else ccDirtyHi.fetch_or (1ull << (cc - 64));
        }
    }
    else if (m.isChannelPressure())
        aftertouch = m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch())
        aftertouch = m.getAfterTouchValue() / 127.0f;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        rootHeldMask = 0;
        numTypesHeld = 0;
        holdChord = -1;
        heldMelodic.fill (false);
        heldMelodicCount = 0;
        pressCount = 0;
        if (m.isAllSoundOff())
            for (auto& v : voices) v.release();
    }
}

prog::Settings Hamamatsu108Processor::progSettings()
{
    prog::Settings st;
    st.mode = progMode.load();
    st.seed = progSeed.load();
    st.keyRoot = juce::roundToInt (param (apvts, "keyRoot"));
    st.minor = param (apvts, "keyMode") > 0.5f;
    st.transpose = juce::roundToInt (param (apvts, "transpose"));
    st.range = juce::roundToInt (param (apvts, "harpRange"));
    st.octave = juce::roundToInt (param (apvts, "harpOctave"));
    const int pat = juce::roundToInt (param (apvts, "strumPattern"));
    st.strumSteps = pat > 0 ? patternString (pat) : juce::String ("D.......D.......");
    st.strumSpeedMs = param (apvts, "strumSpeed");
    st.feel = param (apvts, "strumFeel");
    st.arpStepBeats = rateInQuarters (juce::roundToInt (param (apvts, "strumRate")));
    st.arpOctaves = juce::roundToInt (param (apvts, "arpOctaves"));
    st.arpPattern = juce::roundToInt (param (apvts, "arpPattern"));
    st.arpLength = param (apvts, "arpGate");
    for (int i = 0; i < 16; ++i) st.stepVel[(size_t) i] = seqVel[(size_t) i].load();
    st.stepLen = seqLen.load();
    st.bpm = hostBpm.load() > 1.0 ? hostBpm.load() : 120.0;
    for (int i = 0; i < prog::gridSteps; ++i) st.gridCells[(size_t) i] = gridCells[(size_t) i].load();
    st.gridLen = juce::roundToInt (param (apvts, "gridLen"));
    st.gridStepBeats = rateInQuarters (juce::roundToInt (param (apvts, "gridRate"))) * (sw (swHalftime) > 0.0f ? 2.0 : 1.0);
    st.motionType = juce::roundToInt (param (apvts, "motionType"));
    for (int i = 0; i < prog::linePoints; ++i) st.line[(size_t) i] = motionLine[(size_t) i].load();
    for (int r = 0; r < prog::gridRows; ++r)
    {
        st.strRate[(size_t) r] = strRate[(size_t) r].load(); st.strOffset[(size_t) r] = strOffset[(size_t) r].load(); st.strLevel[(size_t) r] = strLevel[(size_t) r].load();
        st.strOct[(size_t) r] = strOct[(size_t) r].load(); st.strChance[(size_t) r] = strChance[(size_t) r].load();
        st.strRatchet[(size_t) r] = strRatchet[(size_t) r].load(); st.strMute[(size_t) r] = strMute[(size_t) r].load();
    }
    return st;
}

void Hamamatsu108Processor::wheelStrum (int samplePos, float pos, float& last)
{
    const int n = (int) tones.size();
    if (n > 0 && last >= 0.0f)
    {
        const int a = juce::roundToInt (last * (float) (n - 1)), b = juce::roundToInt (pos * (float) (n - 1));
        const float vel = juce::jlimit (0.35f, 1.0f, 0.5f + 4.0f * std::abs (pos - last));
        if (a != b)
            for (int i = a + (b > a ? 1 : -1);; i += (b > a ? 1 : -1))
            {
                scheduleNote (samplePos, i, vel);
                if (i == b) break;
            }
    }
    last = pos;
}

void Hamamatsu108Processor::updateTones (int chordIndex, int transpose, int rangeOctaves)
{
    const int smode = stringsMode.load();
    const int scaleKey = juce::roundToInt (param (apvts, "keyRoot")) * 2 + (param (apvts, "keyMode") > 0.5f ? 1 : 0);
    int heldMask = 0;
    if (smode == 2) for (int n = 0; n < 128; ++n) if (heldMelodic[(size_t) n]) heldMask |= 1 << (n % 12);
    if (chordIndex == tonesChord && transpose == tonesTranspose && rangeOctaves == tonesRange && smode == tonesMode && (smode == 0 || scaleKey == tonesScaleKey) && heldMask == tonesHeld)
        return;
    tonesHeld = heldMask;
    tonesMode = smode;
    tonesScaleKey = scaleKey;

    tonesChord = chordIndex;
    tonesTranspose = transpose;
    tonesRange = rangeOctaves;
    tones.clear();

    if (chordIndex < 0) return;

    int mask = chords::pitchClassMask (chordIndex, transpose);
    if (smode == 2 && heldMask != 0) mask = heldMask;   // NOTES: only the notes you hold
    if (smode == 1)   // SCALE: every note of the key on the strings
    {
        static const int major[] = { 0, 2, 4, 5, 7, 9, 11 }, minor[] = { 0, 2, 3, 5, 7, 8, 10 };
        const int root = scaleKey / 2 + transpose;
        mask = 0;
        for (int k = 0; k < 7; ++k) mask |= 1 << ((((scaleKey & 1) ? minor[k] : major[k]) + root + 120) % 12);
    }
    // never below C3 (MIDI 48), so recorded strums never land in the chord zones
    static const int lows[] = { 60, 60, 54, 48, 48 };
    const int low = lows[juce::jlimit (1, 4, rangeOctaves)];
    const int high = low + 12 * rangeOctaves;
    for (int n = low; n <= high; ++n)
        if (mask & (1 << (n % 12)))
            tones.push_back (n);
}

void Hamamatsu108Processor::scheduleNote (int delay, int toneIndex, float velocity, int visualOverride)
{
    if (numEvents >= (int) events.size() || tones.empty()) return;
    toneIndex = juce::jlimit (0, (int) tones.size() - 1, toneIndex);
    const int visual = visualOverride >= 0 ? juce::jlimit (0, numStrings - 1, visualOverride)
                     : tones.size() > 1 ? juce::roundToInt ((float) toneIndex * (numStrings - 1) / (float) (tones.size() - 1)) : numStrings / 2;
    // PLATE tuning: a string switched off stays silent; its own timing and level
    if ((plMute.load() >> (unsigned) visual) & 1u) return;
    delay += (int) (plTime[(size_t) visual].load() * 0.12f * (float) sr);
    velocity *= 1.0f - 0.85f * plLevel[(size_t) visual].load();
    events[(size_t) numEvents++] = { juce::jmax (0, delay), juce::jlimit (0, 127, tones[(size_t) toneIndex] + 12 * juce::roundToInt (param (apvts, "harpOctave"))), juce::jlimit (0.05f, 1.0f, velocity), visual, ! eventsFromHost };
    // RATCHET: the string is struck again, fast, softer each time
    if (const int reps = juce::roundToInt (param (apvts, "ratchet") * 3.0f); reps > 0)
        for (int r = 1; r <= reps && numEvents < (int) events.size(); ++r)
        {
            auto e = events[(size_t) (numEvents - 1)];
            e.delay = juce::jmax (0, delay) + (int) (0.055f * (float) r * (float) sr);
            e.velocity = juce::jlimit (0.05f, 1.0f, velocity * std::pow (0.72f, (float) r));
            events[(size_t) numEvents++] = e;
        }
}

void Hamamatsu108Processor::schedulePitch (int delay, int note, float velocity, bool record)
{
    if (numEvents >= (int) events.size()) return;
    int visual = numStrings / 2;
    if (! tones.empty())
    {
        int bestIdx = 0;
        for (int i = 0; i < (int) tones.size(); ++i)
            if (std::abs (tones[(size_t) i] - note) < std::abs (tones[(size_t) bestIdx] - note)) bestIdx = i;
        visual = tones.size() > 1 ? juce::roundToInt ((float) bestIdx * (numStrings - 1) / (float) (tones.size() - 1)) : numStrings / 2;
    }
    else
        visual = juce::jlimit (0, numStrings - 1, (note - 48) * (numStrings - 1) / 48);
    delay += (int) (plTime[(size_t) visual].load() * 0.12f * (float) sr);
    velocity *= 1.0f - 0.85f * plLevel[(size_t) visual].load();
    // HUMAN: a little looseness in time and touch
    if (const float h = sw (swHuman); h > 0.0f)
    {
        delay += humanRng.nextInt (juce::jmax (1, (int) (h * 0.025 * getSampleRate())));
        velocity *= 1.0f - h * 0.35f * humanRng.nextFloat();
    }
    events[(size_t) numEvents++] = { juce::jmax (0, delay), juce::jlimit (0, 127, note + 12 * juce::roundToInt (param (apvts, "harpOctave"))), juce::jlimit (0.05f, 1.0f, velocity), visual, record && ! eventsFromHost };
    if (const int reps = juce::roundToInt (param (apvts, "ratchet") * 3.0f); reps > 0)   // RATCHET on single notes too
        for (int r = 1; r <= reps && numEvents < (int) events.size(); ++r)
        {
            auto e = events[(size_t) (numEvents - 1)];
            e.delay = juce::jmax (0, delay) + (int) (0.055f * (float) r * (float) sr);
            e.velocity = juce::jlimit (0.05f, 1.0f, velocity * std::pow (0.72f, (float) r));
            e.capture = false;
            events[(size_t) numEvents++] = e;
        }
}

// one hand gesture across the strings, with human variation driven by Feel
void Hamamatsu108Processor::scheduleStrum (int offset, bool down, float velScale, float speedMul)
{
    const int n = (int) tones.size();
    if (n == 0) return;

    const float feel = mp ("strumFeel");
    auto rnd = [this] { return rng.nextFloat(); };

    // a loose hand sometimes skips a light up-stroke
    if (! down && rnd() < 0.3f * feel) return;

    const float accent = down ? 0.85f : 0.62f;
    const float baseVel = juce::jlimit (0.15f, 1.0f, (accent + (rnd() - 0.5f) * 0.4f * feel) * velScale);

    int count, first;
    if (down)
    {
        count = juce::jmax (2, juce::roundToInt ((float) n * (0.62f + 0.38f * rnd())));
        first = juce::roundToInt ((float) (n - count) * rnd() * 0.5f);
    }
    else
    {
        count = juce::jmax (2, juce::roundToInt ((float) n * (0.35f + 0.35f * rnd())));
        first = n - 1 - juce::roundToInt ((float) (n - count) * rnd() * 0.3f);
    }
    sweep (offset, down, first, count, baseVel, speedMul);
}

// strings first .. first+count (down) or first .. first-count (up), one after another
void Hamamatsu108Processor::sweep (int offset, bool down, int first, int count, float vel, float speedMul)
{
    const int n = (int) tones.size();
    if (n == 0 || count <= 0) return;
    // the STROKE switches shape every stroke, whatever plays it (keys, strum keys, mouse, motion, left hand)
    if (false && ! inStroke && count >= 3)   // stroke toggles retired: every strum plays clean
    {
        auto on = [this] (const char* id) { return param (apvts, id) > 0.5f; };
        const bool sDown = on ("strokeDown"), sHalf = on ("strokeHalf"), sBounce = on ("strokeBounce"),
                   sWaver = on ("strokeWaver"), sMiddle = on ("strokeMiddle"), sSlow = on ("strokeSlow");
        if (sDown || sHalf || sBounce || sWaver || sMiddle || sSlow)
        {
            inStroke = true;
            // the strings this stroke covers
            const int lo = juce::jlimit (0, n - 1, down ? first : first - count + 1);
            const int hi = juce::jlimit (0, n - 1, down ? first + count - 1 : first);
            int len = hi - lo + 1;
            bool up = down;                                     // up = from the low strings to the high ones
            if (sDown) up = false;                              // DOWN: from the top
            if (sBounce) { up = sideToSideUp; sideToSideUp = ! sideToSideUp; }   // BOUNCE: every stroke the other way
            if (sHalf) len = (len + 1) / 2;                     // HALF: half of the strings
            const float speed = speedMul * (sSlow ? 2.2f : 1.0f);   // HALF TIME: slow and wide
            const int repeats = sWaver ? 6 : 1;                 // WAVER: quick strokes up and down that fade
            const double bpmNow = juce::jmax (40.0, lastBpm);
            const int every = juce::jmax (64, (int) (60.0 / bpmNow / (sSlow ? 3.0 : 6.0) * sr));
            for (int k = 0; k < repeats; ++k)
            {
                const bool u = (k % 2) == 0 ? up : ! up;
                const float vk = vel * (1.0f - 0.12f * (float) k);
                const int at = offset + k * every;
                const float sp = repeats > 1 ? speed * 0.4f : speed;
                if (sMiddle)
                {
                    const int mid = lo + (hi - lo + 1) / 2;
                    const int half = juce::jmax (1, len / 2 + (len & 1));
                    sweep (at, true, mid, juce::jmin (half, hi - mid + 1), vk, sp);
                    sweep (at, false, mid - 1, juce::jmin (half, mid - lo), vk * 0.9f, sp);
                }
                else if (u) sweep (at, true, lo, len, vk, sp);
                else        sweep (at, false, hi, len, vk, sp);
            }
            inStroke = false;
            return;
        }
    }
    const float feel = mp ("strumFeel");
    const float speedMs = mp ("strumSpeed") * speedMul;
    auto rnd = [this] { return rng.nextFloat(); };

    const int lead = juce::roundToInt ((rnd() - 0.3f) * feel * 0.012f * (float) sr);
    const float stepMs = speedMs * (1.0f + (rnd() - 0.5f) * 0.6f * feel);
    const int shape = juce::roundToInt (param (apvts, "strumShape"));
    float t = (float) juce::jmax (0, offset + lead);
    for (int i = 0; i < count; ++i)
    {
        const int tone = down ? first + i : first - i;
        if (tone < 0 || tone >= n) break;
        const float v = vel * (1.0f - 0.25f * (float) i / (float) count) * (1.0f + (rnd() - 0.5f) * 0.25f * feel);
        scheduleNote (juce::roundToInt (t), tone, v);
        const float jitter = 1.0f + (rnd() - 0.5f) * 0.5f * feel;
        // stroke shape: how the hand moves across the strings (even, eased, accelerating, braking)
        const float f = count > 1 ? (float) i / (float) (count - 1) : 0.0f;
        float shapeMul = 1.0f;
        if (shape == 1) shapeMul = 0.55f + 1.8f * (f - 0.5f) * (f - 0.5f) * 2.0f;
        else if (shape == 2) shapeMul = 0.4f + 1.2f * f;
        else if (shape == 3) shapeMul = 1.6f - 1.2f * f;
        t += stepMs * 0.001f * (float) sr * jitter * shapeMul;
    }
}

// FLOURISH: the key is the root, its octave the chord type (from the first harp octave up: major, minor, 7, maj7, m7),
// harder = faster run. The run climbs (or falls) through the chord over the harp RANGE.
int Hamamatsu108Processor::flourishType (int key) const
{
    static const int byOctave[] = { 5, 0, 1, 2, 3, 4, 6 };   // dim, major, minor, 7, maj7, m7, aug
    const int base = chordZones.load() ? juce::jmax (36, typeZoneHigh() + 1) - 12 : 24;
    return byOctave[juce::jlimit (0, 6, (key - base) / 12)];
}

void Hamamatsu108Processor::flourish (int samplePos, int key, float velocity, bool up)
{
    // octaves (FL names): C2 dim, C3 major, C4 minor, C5 7, C6 maj7, C7 m7, C8 aug (zones on: the same, above the zones)
    // the chord of the key on that note (C major: C = C, D = Dm, E = Em ...), so every run is in tune with the song.
    // A note outside the key keeps the old octave rule (C2 dim, C3 major, C4 minor ...).
    const int keyRoot = juce::roundToInt (param (apvts, "keyRoot"));
    const bool keyMinor = param (apvts, "keyMode") > 0.5f;
    const int pcRel = ((key % 12) - keyRoot + 12) % 12;
    static const int majSteps[] = { 0, 2, 4, 5, 7, 9, 11 }, minSteps[] = { 0, 2, 3, 5, 7, 8, 10 };
    int degree = -1;
    for (int d = 0; d < 7; ++d) if ((keyMinor ? minSteps[d] : majSteps[d]) == pcRel) degree = d;
    int chord = degree >= 0 ? prog::chordOnDegree (keyRoot, keyMinor, degree) : chords::index (key % 12, flourishType (key));
    // NOTES strip: the notes you picked are the run, whatever key starts it
    if (const int pick = stringsPick.load(); chords::isValid (pick)) chord = pick;
    flourishChord = chord;
    if (motionActive()) return;   // MOTION is on (GRID / LINES / RHYTHM / STRINGS): the key only picks the chord, the window plays it
    const int range = juce::jlimit (1, 4, juce::roundToInt (param (apvts, "harpRange")));
    const int mask = chords::pitchClassMask (chord, juce::roundToInt (param (apvts, "transpose")));
    const int low = juce::jlimit (36, 84, key);   // the run starts on the key you press
    std::vector<int> run;
    for (int n = low; n <= juce::jmin (108, low + 12 * range); ++n)
        if (mask & (1 << (n % 12))) run.push_back (n);
    if (! up) std::reverse (run.begin(), run.end());
    const float v = juce::jlimit (0.05f, 1.0f, velocity);
    const float stepMs = 60.0f - 46.0f * v;   // soft = slow, hard = fast
    const float feel = mp ("strumFeel");
    float t = (float) samplePos;
    for (size_t i = 0; i < run.size(); ++i)
    {
        const float f = run.size() > 1 ? (float) i / (float) (run.size() - 1) : 0.0f;
        const float vel = (0.45f + 0.5f * v) * (0.85f + 0.3f * std::sin (f * juce::MathConstants<float>::pi)) * (1.0f + (rng.nextFloat() - 0.5f) * 0.2f * feel);
        schedulePitch (juce::roundToInt (t), run[i], vel, true);
        t += stepMs * 0.001f * (float) sr * (1.0f + (rng.nextFloat() - 0.5f) * 0.3f * feel);
    }
}

// one stroke of a pattern (see strumPatternSteps)
void Hamamatsu108Processor::stroke (int offset, char type, float accent)
{
    const int n = (int) tones.size();
    if (n == 0) return;
    const float feel = mp ("strumFeel");
    auto rnd = [this] { return rng.nextFloat(); };
    auto human = [&] (float v) { return juce::jlimit (0.06f, 1.0f, v * accent * (1.0f + (rnd() - 0.5f) * 0.35f * feel)); };

    switch (type)
    {
        case 'D': scheduleStrum (offset, true, accent); break;
        case 'U': scheduleStrum (offset, false, accent); break;
        case 'd':
        {
            const int first = juce::roundToInt ((float) n * (0.3f + 0.15f * rnd()));
            sweep (offset, true, first, juce::jmax (2, n - first), human (0.6f), 0.85f);
            break;
        }
        case 'u':
            sweep (offset, false, n - 1, juce::jlimit (2, 5, juce::roundToInt ((float) n * 0.35f)), human (0.46f), 0.8f);
            break;
        case 'x':
        {
            const int first = juce::roundToInt ((float) juce::jmax (0, n - 3) * (0.3f + 0.4f * rnd()));
            sweep (offset, true, first, juce::jmin (3, n), human (0.22f), 0.55f);
            break;
        }
        case 'B':
        {
            // thumb: root, then the next chord tone up (alternating bass)
            const int idx = (bassAlt++ % 2 == 0) ? 0 : juce::jmin (n - 1, 2);
            scheduleNote (juce::jmax (0, offset), idx, human (0.8f));
            break;
        }
        case 'H':
            scheduleNote (juce::jmax (0, offset), n - 1 - (int) (rnd() * (float) juce::jmin (3, n)), human (0.56f));
            break;
        case 'T':
        {
            const int k = rnd() < 0.5f ? 2 : 3;
            const int first = (int) (rnd() * (float) juce::jmax (1, n - k));
            sweep (offset, rnd() < 0.7f, first + (rnd() < 0.7f ? 0 : k - 1), juce::jmin (k, n), human (0.62f), 0.15f);
            break;
        }
        case 'S':
            sweepDown = ! sweepDown;
            sweep (offset, ! sweepDown, sweepDown ? n - 1 : 0, n, human (0.7f), 3.5f);
            break;
        default: break;
    }
}

// pattern step k (counted in 1/16 from the bar start): accents, variation and fills like a player
void Hamamatsu108Processor::patternStep (int offset, juce::int64 k)
{
    const juce::String steps (patternString (juce::roundToInt (param (apvts, "strumPattern"))));
    const int len = steps.length();
    if (len == 0) return;
    const int i = (int) (k % len);
    const juce::int64 bar = k / len;
    const float vary = mp ("strumVary");
    auto rnd = [this] { return rng.nextFloat(); };

    char c = (char) steps[i];
    // every 4th bar the last beat turns into a small fill
    if (vary > 0.05f && bar % 4 == 3 && i >= len - 4 && rnd() < vary * 0.8f)
        c = "dudu"[i - (len - 4)];
    else if (c == '.')
    {
        if (i % 2 == 1 && rnd() < 0.14f * vary) c = 'x';                 // ghost brush between strokes
    }
    else if (i != 0 && (c == 'd' || c == 'u' || c == 'x' || c == 'H') && rnd() < 0.22f * vary)
        c = '.';                                                          // a light stroke left out
    else if (c == 'U' && rnd() < 0.2f * vary)
        c = 'u';
    if (c == '.') return;

    const float accent = i == 0 ? 1.0f : (i % 4 == 0 ? 0.9f : 0.78f);
    stroke (offset, c, accent);
}

// RELEASE when a key goes up: the knob, but "ring" lets go over 3 s, so a held key decides how long a note lasts
float Hamamatsu108Processor::noteRelease()
{
    // PEDAL (or the MIDI sustain pedal), or DAMPER off: the strings ring on after the key; -1 = no release
    if (param (apvts, "strPedal") > 0.5f || sustainPedal || param (apvts, "strDamper") < 0.5f) return -1.0f;
    return juce::jmin (param (apvts, "harpRelease"), 3.0f) * (0.2f + 1.6f * param (apvts, "strRelTime"));
}

void Hamamatsu108Processor::startVoice (int note, float velocity)
{
    keyTime = 0.0;   // console LFOs: DELAY and FADE count from here
    for (int i = 0; i < 2; ++i)
        if (param (apvts, (juce::String ("lfo") + juce::String (i + 1) + "Trig").toRawUTF8()) > 0.5f) lfos[(size_t) i].phase = 0.0;
    if (param (apvts, "mono") > 0.5f)   // MONO: one string at a time
        for (auto& v : voices) if (v.isActive()) v.release (0.04f);
    {
        // VELO (amp): + = more touch sensitive, - = more even
        const float vs = param (apvts, "ampVelo");
        velocity = vs >= 0.0f ? std::pow (juce::jlimit (0.0f, 1.0f, velocity), 1.0f + 2.0f * vs) : velocity + (0.85f - velocity) * -vs;
    }
    lastNotePlayed = note;
    keyNow = note;   // every string this key starts (layers, SPARKLE, SUB...) belongs to it, so its release finds them all
    if (! dualOn) { startHarp (voiceSettings, note, velocity); startSwitchVoices (note, velocity); return; }
    // MORPH blends layer A and layer B (equal power)
    const float m = juce::jlimit (0.0f, 1.0f, morphNow);
    const float gA = std::cos (m * juce::MathConstants<float>::halfPi), gB = std::sin (m * juce::MathConstants<float>::halfPi);
    if (gA > 0.05f) startHarp (voiceSettings, note, velocity * gA);
    if (gB > 0.05f) startHarp (voiceSettings2, note, velocity * gB, true);
    startSwitchVoices (note, velocity);
}

// SPARKLE / SUB: the same string an octave up / down, softer
void Hamamatsu108Processor::startSwitchVoices (int note, float velocity)
{
    if (const float a = sw (swSparkle); a > 0.0f && note + 12 <= 120) startHarp (voiceSettings, note + 12, velocity * (0.25f + 0.45f * a));
    if (const float a = sw (swSub); a > 0.0f && note - 12 >= 12) startHarp (voiceSettings, note - 12, velocity * (0.3f + 0.5f * a));
    if (const float a = sw (swFifth); a > 0.0f && note + 7 <= 120) startHarp (voiceSettings, note + 7, velocity * (0.25f + 0.45f * a));
    if (const float a = sw (swDetune); a > 0.0f)
    {
        auto s2 = voiceSettings;
        s2.pitchMul *= std::exp2 ((4.0f + 14.0f * a) / 1200.0f);   // 4 .. 18 cents: a second string beating against the first
        startHarp (s2, note, velocity * 0.7f);
    }
}

void Hamamatsu108Processor::startHarp (const harp::Settings& settings, int note, float velocity, bool layerB)
{
    harp::Voice* target = nullptr;

    const int limit = juce::jlimit (1, numVoices, maxVoices.load());
    for (int i = 0; i < limit; ++i)
        if (! voices[(size_t) i].isActive()) { target = &voices[(size_t) i]; break; }

    if (target == nullptr)   // steal the quietest voice (it fades out over 3 ms, no click)
    {
        // prefer a voice that is not already being faded out for another note
        target = nullptr;
        for (int i = 0; i < limit; ++i)
            if (! voices[(size_t) i].isBusy() && (target == nullptr || voices[(size_t) i].getLevel() < target->getLevel()))
                target = &voices[(size_t) i];
        if (target == nullptr)
        {
            target = &voices[0];
            for (int i = 0; i < limit; ++i)
                if (voices[(size_t) i].getLevel() < target->getLevel()) target = &voices[(size_t) i];
        }
    }

    // spread around the centre of what is playing, so the harp never leans to one ear
    float centre = 72.0f;
    if (tones.size() > 1) centre = 0.5f * (float) (tones.front() + tones.back());
    // SPREAD: how far the strings fan out across the stereo field; layer B mirrors layer A, so the two engines interleave
    const float spread = param (apvts, "spread");
    const float panLimit = 0.15f + 0.75f * spread;
    float pan = juce::jlimit (-panLimit, panLimit, ((float) note - centre) / 30.0f * (0.4f + 1.4f * spread));
    if (layerB) pan = juce::jlimit (-panLimit, panLimit, -pan * 1.15f);
    if (curString >= 0 && curString < numStrings)
    {
        // PLATE tuning: where this string is plucked (near the end = bright and thin, towards the middle = round) and how slack it is
        auto s = settings;
        const float t = plTone[(size_t) curString].load(), b = plBend[(size_t) curString].load();
        s.tone = juce::jlimit (0.02f, 1.0f, s.tone + (0.35f - t) * 0.9f);
        const float basePick = s.model == harp::Model::harp ? 0.04f + 0.42f * s.pos : 1.0f / 7.0f;
        s.pick = juce::jlimit (0.02f, 0.5f, basePick + (t - 0.35f) * 0.5f);
        if (b > 0.001f) { s.bend = -2.0f * b; s.bendTime = 0.06f + 0.3f * b; }
        target->start (note, velocity, pan, s, rng, noteHz (note));
    }
    else
        target->start (note, velocity, pan, settings, rng, noteHz (note));
    target->key = keyNow;
}

// MIDI OUT: harp on channel 1, chord layer on 2, bass on 3 (FL Studio: set the plugin's MIDI output port)
void Hamamatsu108Processor::midiNote (int samplePos, int channel, int note, float velocity, bool on)
{
    if (! midiOut.load() || note < 0 || note > 127 || channel < 0 || channel > 2) return;
    const int pos = juce::jlimit (0, juce::jmax (0, blockSamples - 1), samplePos);
    auto& count = outOn[(size_t) channel][(size_t) note];
    if (on)
    {
        if (count > 0) midiOutBuffer.addEvent (juce::MidiMessage::noteOff (channel + 1, note), pos);   // retrigger: close the old one first
        midiOutBuffer.addEvent (juce::MidiMessage::noteOn (channel + 1, note, (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f))), pos);
        count = 1;
    }
    else if (count > 0)
    {
        midiOutBuffer.addEvent (juce::MidiMessage::noteOff (channel + 1, note), pos);
        count = 0;
    }
}

void Hamamatsu108Processor::capture (double seconds, int note, float velocity, int layer, bool on)
{
    if ((layer == 1 || layer == 2) && ! previewMute)
        midiNote ((int) (seconds * sr - (double) sampleClock), layer, note, velocity, on);
    if (previewMute) return;   // preset previews never end up in the MIDI capture
    if (note < 0 || note > 127 || layer < 0 || layer > 3) return;
    auto& wasOn = capturedOn[(size_t) layer][(size_t) note];
    if (! on && ! wasOn) return;  // the matching note-on was never recorded
    if (on && hostDriven) return; // piano roll playback is not recorded again
    wasOn = on;
    if ((layer == 1 || layer == 2) && layersFromHost) return;   // layers that only follow the piano roll are not recorded
    const auto scope = captureFifo.write (1);
    const CapturedNote cn { seconds, note, velocity, layer, on };
    if (scope.blockSize1 > 0) captureBuffer[(size_t) scope.startIndex1] = cn;
    else if (scope.blockSize2 > 0) captureBuffer[(size_t) scope.startIndex2] = cn;
}

// rhythmic gate patterns (x = open, . = closed), 16 or 32 steps
const juce::StringArray& Hamamatsu108Processor::gatePresetNames()
{
    static const juce::StringArray n { "Pulse 16ths", "Trance", "Offbeat", "Dotted", "Stutter", "Half open", "Syncopated", "Glitch 32", "Swell" };
    return n;
}

void Hamamatsu108Processor::gateLoadPreset (int index)
{
    static const char* p[] = { "xxxxxxxxxxxxxxxx", "x.xxx.xxx.xxx.xx", "..x...x...x...x.", "x..x..x..x..x.x.", "xxxx.x.xxxxx.x.x",
                               "xxxxxxxx........", "x..x..x...x..x..", "x.x.xx..x.xxx...x.x.xx.x..x.xxx.", "x.......x.xx.xxx" };
    const juce::String s (p[juce::jlimit (0, 8, index)]);
    std::uint32_t mask = 0;
    for (int i = 0; i < s.length(); ++i) if (s[i] == 'x') mask |= 1u << (unsigned) i;
    gateMask.store (mask);
    gateLen.store (s.length());
}

// ARP step sequencer presets: NOTE / OCT / VELOCITY / RATCHET / CHANCE / TIE for 16 steps
const juce::StringArray& Hamamatsu108Processor::seqPresetNames()
{
    static const juce::StringArray n { "Basic", "Accent groove", "Harp ripple", "Octave bounce", "Broken chord", "Ratchet roll",
                                       "Sparse dream", "Gallop", "Pluck 3+3+2", "Music box" };
    return n;
}

void Hamamatsu108Processor::seqLoadPreset (int index)
{
    struct P { int len; const char* deg; const char* oct; const char* vel; const char* rat; const char* prob; const char* tie; };
    // deg: 1-8 = chord tone, . = rest ; oct: - 0 + 2 ; vel: 1-9 ; rat: 1-4 ; prob: 1-9 (x10 %), 0 = always ; tie: t
    static const P presets[] = {
        { 16, "1234123412341234", "0000000000000000", "9575957595759575", "1111111111111111", "0000000000000000", "................" },
        { 16, "1324132413241324", "0000000000000000", "9363936393639363", "1111111111111111", "0000000600000006", "................" },
        { 16, "1234567887654321", "0000000000000000", "8566677888776655", "1111111111111111", "0000000000000000", "................" },
        { 16, "1111222233334444", "0+0+0+0+0+0+0+0+", "9696969696969696", "1111111111111111", "0000000000000000", "................" },
        { 16, "1325132513251325", "000+000+000+000+", "9566956695669566", "1111111111111111", "0000000000000000", "................" },
        { 16, "1.3.2.4.1.3.5555", "000000000000++++", "9.7.8.7.9.7.6789", "1111111111112234", "0000000000000000", "................" },
        { 16, "1...3...5...4...", "0000000+0000000+", "8...6...7...5...", "1111111111111111", "0007000600070005", ".t.t.t.t.t.t.t.t" },
        { 16, "1.121.121.121.12", "0000000000000000", "9.659.659.659.65", "1111111111111111", "0000000000000000", "................" },
        { 16, "1..3..5.1..3..5.", "000000+000000+0+", "9..7..8.9..7..8.", "1111111111111111", "0000000000000000", "................" },
        { 16, "5354535453546545", "++++++++++++++++", "7565756575657565", "1111111111111111", "0000000000000000", "................" },
    };
    const auto& p = presets[juce::jlimit (0, (int) (sizeof (presets) / sizeof (presets[0])) - 1, index)];
    seqLen.store (p.len);
    for (int i = 0; i < seqSteps; ++i)
    {
        const char d = p.deg[i], o = p.oct[i], v = p.vel[i], r = p.rat[i], c = p.prob[i];
        seqDeg[(size_t) i].store (d == '.' ? -1 : d - '1');
        seqOct[(size_t) i].store (o == '-' ? -1 : o == '+' ? 1 : o == '2' ? 2 : 0);
        seqVel[(size_t) i].store (v == '.' ? 0.6f : (float) (v - '0') / 9.0f);
        seqRatchet[(size_t) i].store (juce::jlimit (1, 4, r - '0'));
        seqProb[(size_t) i].store (c == '0' ? 1.0f : (float) (c - '0') / 10.0f);
        seqTie[(size_t) i].store (p.tie[i] == 't' ? 1 : 0);
    }
}

void Hamamatsu108Processor::seqRandomise()
{
    juce::Random r;
    for (int i = 0; i < seqSteps; ++i)
    {
        seqDeg[(size_t) i].store (r.nextFloat() < 0.12f ? -1 : r.nextInt (6));
        seqOct[(size_t) i].store (r.nextFloat() < 0.2f ? 1 : 0);
        seqVel[(size_t) i].store (i % 4 == 0 ? 0.95f : 0.45f + 0.4f * r.nextFloat());
        seqRatchet[(size_t) i].store (r.nextFloat() < 0.1f ? 2 : 1);
        seqProb[(size_t) i].store (i % 4 == 0 ? 1.0f : (r.nextFloat() < 0.25f ? 0.6f : 1.0f));
        seqTie[(size_t) i].store (0);
    }
}

// one arpeggio step: the pattern picks the chord tone, the step row adds velocity, chance, ratchets and ties;
// OCTAVES limits the span, GATE shortens the notes, Regen re-rolls the variation
void Hamamatsu108Processor::scheduleArpStep (int offset)
{
    if (tones.empty()) return;

    // the arp plays the chord tones from the bottom of the harp over OCTAVES (not the whole harp)
    const int octaves = juce::roundToInt (param (apvts, "arpOctaves"));
    std::vector<int> at;
    for (int t : tones) if (t < tones.front() + 12 * octaves) at.push_back (t);
    const int n = (int) at.size();
    if (n == 0) return;

    const int pattern = juce::roundToInt (param (apvts, "arpPattern"));
    const float chance = mp ("arpChance");
    const int regen = juce::roundToInt (param (apvts, "arpRegen"));
    const double stepQ = rateInQuarters (juce::roundToInt (param (apvts, "strumRate")));
    static const int regenBars[] = { 1, 1, 2, 4 };
    const int cycle = juce::jmax (1, (int) std::round (regenBars[juce::jlimit (0, 3, regen)] * 4.0 / stepQ));

    const juce::int64 c = arpCounter++;
    if (regen > 0 && c > 0 && c % cycle == 0) ++arpSeed;   // new variation every N bars
    const int inCycle = (int) (c % cycle);
    const int len = juce::jlimit (1, seqSteps, seqLen.load());
    const int s = (int) (c % len);
    arpStepGui.store (s);

    auto hash = [this] (int i)
    {
        std::uint32_t h = (std::uint32_t) i * 2654435761u ^ (arpSeed * 40503u + 0x9e3779b9u);
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        return (float) (h & 0xffff) / 65535.0f;
    };

    int idx = 0, octShift = 0;
    const int span = juce::jmax (1, 2 * n - 2);
    switch (pattern)
    {
        case 0: idx = (int) (c % n); break;                                   // up
        case 1: idx = n - 1 - (int) (c % n); break;                           // down
        case 2: { const int p = (int) (c % span); idx = p < n ? p : span - p; break; }       // up / down
        case 3: idx = (c % 2 == 0) ? 0 : 1 + (int) ((c / 2) % juce::jmax (1, n - 1)); break; // thumb
        case 4: { const int p = (int) (c % (2 * n)); idx = p < n ? p : 2 * n - 1 - p; break; } // mirror (ends repeat)
        case 5: idx = juce::jlimit (0, n - 1, (int) (hash (inCycle + 7919) * n)); break;    // random
        case 6: { const int p = (int) (c % n); idx = (p % 2 == 0) ? p / 2 : n - 1 - p / 2; break; }   // converge (outside in)
        case 7: idx = (c % 2 == 1) ? n - 1 : (int) ((c / 2) % juce::jmax (1, n - 1)); break;       // pinky (top note pedal)
        case 9:                                                                                     // as played (order of the keys)
        {
            if (pressCount > 0 && heldMelodicCount > 0)
            {
                std::vector<int> order;
                for (int o = 0; o < octaves; ++o)
                    for (int i = 0; i < pressCount; ++i) order.push_back (pressOrder[(size_t) i] + 12 * o);
                const int note = order[(size_t) (c % (juce::int64) order.size())];
                auto it = std::find (at.begin(), at.end(), note);
                if (it == at.end()) { at.push_back (note); idx = (int) at.size() - 1; } else idx = (int) (it - at.begin());
            }
            else idx = (int) (c % n);
            break;
        }
        case 10: { const int p = (int) (c % span); idx = p < n ? n - 1 - p : p - (n - 2); break; }   // down / up
        case 11: idx = -1; break;                                                                   // chord: all tones at once
        case 8:
        default:                                                                                    // steps
        {
            const int d = seqDeg[(size_t) s].load();
            if (d < 0) return;
            idx = d % n;
            octShift = seqOct[(size_t) s].load() + d / n;
            break;
        }
    }

    if (seqTie[(size_t) s].load() != 0 && c > 0) return;   // tie: the previous note keeps ringing
    if (seqVel[(size_t) s].load() <= 0.06f) return;          // a step drawn down to nothing is a rest
    if (inCycle > 0 && hash (inCycle) > chance) return;     // the first step of a cycle always plays
    if (inCycle > 0 && hash (inCycle * 31 + 5) > seqProb[(size_t) s].load()) return;

    const bool onBeat = std::fmod ((double) c * stepQ, 1.0) < 1.0e-6;
    // DYNAMICS: 0 = every note the same, 100 % = the VELOCITY row as drawn
    const float dyn = param (apvts, "arpVelScale");
    const float vel = juce::jlimit (0.06f, 1.0f, (0.78f + dyn * (seqVel[(size_t) s].load() - 0.78f)) * (onBeat ? 1.0f : 0.92f));
    if (idx < 0)
    {
        // CHORD mode: the whole chord on every step (a tiny roll keeps it natural)
        const double stepSamples0 = stepQ * 60.0 / bpm * sr;
        const float gate0 = param (apvts, "arpGate");
        for (int i = 0; i < n; ++i)
        {
            const int d = offset + i * (int) (0.003 * sr);
            schedulePitch (d, juce::jlimit (0, 127, at[(size_t) i]), vel * 0.8f, true);
            if (gate0 < 0.98f && numReleases < (int) releases.size())
                releases[(size_t) numReleases++] = { d + juce::jmax (64, (int) (gate0 * stepSamples0)), at[(size_t) i] };
        }
        return;
    }
    const int note = juce::jlimit (0, 127, at[(size_t) idx] + 12 * octShift);
    const int ratchet = juce::jlimit (1, 4, seqRatchet[(size_t) s].load());
    const double stepSamples = stepQ * 60.0 / bpm * sr;
    const float gate = param (apvts, "arpGate");
    for (int r = 0; r < ratchet; ++r)
    {
        const int d = offset + (int) (r * stepSamples / ratchet);
        schedulePitch (d, note, vel * std::pow (0.85f, (float) r), true);
        if (gate < 0.98f && numReleases < (int) releases.size())
            releases[(size_t) numReleases++] = { d + juce::jmax (64, (int) (gate * stepSamples / ratchet)), note };
    }
}

void Hamamatsu108Processor::updateChordLayer (int chord, int transpose)
{
    const int type = juce::roundToInt (param (apvts, "chordType"));
    const int inv = juce::roundToInt (param (apvts, "voicing"));
    if (chord == layerChord && type == layerType && transpose == layerTranspose && inv == layerVoicing)
        return;

    const bool typeChanged = type != layerType;
    layerChord = chord; layerType = type; layerTranspose = transpose; layerVoicing = inv;

    if (chord < 0 || type == 0)
    {
        for (auto& v : chordVoices)
            if (v.isActive() && ! v.isReleasing()) { v.release(); capture ((double) sampleClock / sr, v.getNote(), 0.0f, 1, false); }
        return;
    }

    voicingNotes = chords::voicing (chord, transpose, inv, voicingNotes);
    const auto kind = (layers::Kind) type;   // organ .. choir map 1..4

    // keep common tones ringing, release the rest, start the new ones
    const double now = (double) sampleClock / sr;
    for (auto& v : chordVoices)
        if (v.isActive() && ! v.isReleasing())
            if (typeChanged || std::find (voicingNotes.begin(), voicingNotes.end(), v.getNote()) == voicingNotes.end())
            {
                v.release();
                capture (now, v.getNote(), 0.0f, 1, false);
            }

    for (int note : voicingNotes)
    {
        bool held = false;
        for (auto& v : chordVoices)
            if (v.isActive() && ! v.isReleasing() && v.getNote() == note) held = true;
        if (held) continue;
        layers::SustainVoice* target = nullptr;
        for (auto& v : chordVoices) if (! v.isActive()) { target = &v; break; }
        if (target == nullptr) for (auto& v : chordVoices) if (v.isReleasing()) { target = &v; break; }
        if (target != nullptr) { target->start (note, 0.8f, kind, rng, noteHz (note)); capture (now, note, 0.8f, 1, true); }
    }
}

void Hamamatsu108Processor::bassBeat (int offset, int beatIndex)
{
    const int chord = bassChord >= 0 ? bassChord : sounding;
    const int c = chord >= 0 ? chord : resolveChord();
    if (c < 0) return;

    const auto iv = chords::intervalsOf (c);
    int fifth = iv[(size_t) juce::jmin (2, (int) iv.size() - 1)];
    for (int x : iv) if (x == 7) fifth = 7;
    const int mode = juce::roundToInt (param (apvts, "bassMode"));
    int interval = 0;
    if (mode == 1)      interval = (beatIndex % 2 == 0) ? 0 : fifth;                   // root + 5th
    else if (mode == 2) { static const int step[] = { 0, 1, 2, 1 }; interval = iv[(size_t) juce::jmin ((int) iv.size() - 1, step[beatIndex % 4])] % 12; }  // walking

    int note = 36 + ((chords::rootOf (c) + juce::roundToInt (param (apvts, "transpose")) + interval) % 12 + 12) % 12;
    if (note < 36) note += 12;
    bassNext = note;
    bassPendingOffset = offset;
}

void Hamamatsu108Processor::startBassNote()
{
    const int type = juce::roundToInt (param (apvts, "bassType"));
    if (type == 0) return;
    const auto kind = type == 1 ? layers::Kind::bassSub : type == 2 ? layers::Kind::bassOrgan : layers::Kind::bassSynth;
    const double now = (double) sampleClock / sr;
    if (bassVoices[(size_t) bassVoiceIndex].isActive() && ! bassVoices[(size_t) bassVoiceIndex].isReleasing())
        capture (now, bassVoices[(size_t) bassVoiceIndex].getNote(), 0.0f, 2, false);
    bassVoices[(size_t) bassVoiceIndex].release();
    bassVoiceIndex = 1 - bassVoiceIndex;
    bassVoices[(size_t) bassVoiceIndex].start (bassNext, 0.85f, kind, rng, noteHz (bassNext));
    capture (now, bassNext, 0.85f, 2, true);
}

//==============================================================================

#ifdef HAMA_PROBE
static void probeStage (int stage, const float* x, int n, juce::int64 clock)
{
    static float p1[16] {}, p2[16] {}, e[16] = { 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f };
    for (int i = 0; i < n; ++i)
    {
        const float d2 = x[i] - 2 * p1[stage] + p2[stage]; p2[stage] = p1[stage]; p1[stage] = x[i];
        e[stage] = 0.999f * e[stage] + 0.001f * d2 * d2;
        if (clock > 2000 && d2 * d2 > 400.0f * e[stage] && std::abs (d2) > 0.015f) printf ("STAGE %d spike at sample %lld (pos %d of %d) d2 %.4f\n", stage, (long long) (clock + i), i, n, d2);
    }
}
#define PROBE(st, buf) probeStage (st, buf, numSamples, sampleClock - numSamples)
#else
#define PROBE(st, buf)
#endif

void Hamamatsu108Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    blockSamples = numSamples;
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);   // KEYS on the strum window
    if ((int) pitchMul.size() < numSamples) pitchMul.resize ((size_t) numSamples, 1.0f);

    // parameters
    // modulation first: everything below reads modulated values
    {
        bool playing = false; double pq = 0.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (pos->getIsPlaying())
                    if (auto p = pos->getPpqPosition()) { playing = true; pq = *p; }
        updateModulation (numSamples, playing, pq);
    }
    const auto& model = modelSpec (juce::roundToInt (param (apvts, "model")));
    const float age = juce::jmin (1.0f, param (apvts, "macroAge") + sw (swTape)), dream = juce::jmin (1.0f, param (apvts, "macroDream") + sw (swDream)),
                wobble = juce::jmin (1.0f, param (apvts, "macroWobble") + sw (swWobble));
    morphNow = mp ("morph");
    voiceSettings.model = (harp::Model) juce::roundToInt (param (apvts, "harpVoice"));
    voiceSettings.tone = juce::jlimit (0.0f, 1.0f, mp ("harpTone") * model.tone * (1.0f - 0.35f * age));
    voiceSettings.decay = mp ("harpDecay") * model.decay * (1.0f + 1.2f * dream);
    voiceSettings.attack = juce::jmin (3.0f, param (apvts, "harpAttack") + 1.6f * sw (swSwell) * sw (swSwell) + (sw (swSwell) > 0.0f ? 0.03f : 0.0f));
    voiceSettings.sustain = juce::jmax (param (apvts, "harpSustain"), sw (swFreeze));
    {
        // FREEZE off: what it held lets go now, it does not ring on
        const bool frozen = sw (swFreeze) > 0.0f;
        if (wasFrozen && ! frozen) for (auto& v : voices) if (v.isActive()) v.release (0.8f);
        wasFrozen = frozen;
    }
    if (sw (swFreeze) > 0.0f) voiceSettings.decay *= 1.0f + 3.0f * sw (swFreeze);
    voiceSettings.shape = mp ("harpShape");
    voiceSettings.exciter = juce::roundToInt (param (apvts, "strExciter"));
    voiceSettings.hard = param (apvts, "strHard");
    voiceSettings.pos = param (apvts, "strPos");
    voiceSettings.bodyMat = juce::roundToInt (param (apvts, "bodyMat"));
    voiceSettings.bodySize = param (apvts, "bodySize");
    voiceSettings.bodyDamp = param (apvts, "bodyDamp");
    voiceSettings.bodyRing = param (apvts, "bodyRing");
    voiceSettings.bodyMix = param (apvts, "bodyMix");
    {
        const char* pe[] = { "pT1", "pL1", "pT2", "pL2", "pT3", "pL3", "pT4", "pTail" };
        for (int i = 0; i < 8; ++i) voiceSettings.penv[(size_t) i] = param (apvts, pe[i]);
        voiceSettings.penvVelo = param (apvts, "pVelo");
        voiceSettings.hold = param (apvts, "ampHold") * 2.0f;
        voiceSettings.buzz = param (apvts, "strBuzz");
        voiceSettings.harmonic = param (apvts, "strHarmonic");
        soloMask = (param (apvts, "soloA") > 0.5f ? 1 : 0) | (param (apvts, "soloB") > 0.5f ? 2 : 0) | (param (apvts, "soloBass") > 0.5f ? 4 : 0) | (param (apvts, "soloChord") > 0.5f ? 8 : 0);
        voiceSettings.level = juce::Decibels::decibelsToGain (param (apvts, "ampLevel")) * ((soloMask == 0 || (soloMask & 1)) ? 1.0f : 0.0f);
    }
    voiceSettings.stereo = juce::jmin (1.5f, 2.0f * param (apvts, "spread"));
    // long ringing strings pile up under dense strum patterns: trim the harp bus a little for long decays
    const float decayComp = 1.0f / (1.0f + 0.09f * juce::jmax (0.0f, voiceSettings.decay - 2.0f));
    const int range = juce::roundToInt (param (apvts, "harpRange"));
    const int transpose = juce::roundToInt (param (apvts, "transpose"));
    // LIVE (the auto hand) and MEMO are retired: the plugin plays what you play, ARP is the only automatic mode
    // while the progression plays, its own generator plays the arp (the engine's ARP stays quiet)
    // PLATE is played by hand (mouse, keys, wheels): no automatic motion then
    const bool motionOn = param (apvts, "gridOn") > 0.5f && juce::roundToInt (param (apvts, "motionType")) != 4;
    const int mode = (juce::roundToInt (param (apvts, "strumMode")) == 1 && ! lastProgPlaying && ! motionOn) ? 1 : 3;
    const int rateIndex = juce::roundToInt (param (apvts, "strumRate"));

    // host tempo / position
    bool hostPlaying = false;
    double ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (30.0, 300.0, *b);
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition()) { hostPlaying = true; ppq = *p; }
        }
    lastBpm = bpm;

    // dual voice
    dualOn = param (apvts, "dual") > 0.5f;
    voiceSettings2 = voiceSettings;
    voiceSettings2.model = (harp::Model) juce::roundToInt (param (apvts, "harpVoice2"));
    voiceSettings2.tone = juce::jlimit (0.0f, 1.0f, mp ("bTone") * model.tone * (1.0f - 0.35f * age) * (mp ("harpTone") / juce::jmax (0.05f, param (apvts, "harpTone"))));
    voiceSettings2.decay = param (apvts, "bDecay") * model.decay * (1.0f + 1.2f * dream);
    voiceSettings2.attack = param (apvts, "bAttack");
    voiceSettings2.shape = mp ("bShape");
    voiceSettings2.level = juce::Decibels::decibelsToGain (param (apvts, "bLevel")) * ((soloMask == 0 || (soloMask & 2)) ? 1.0f : 0.0f);
    voiceSettings2.pitchMul = std::exp2 ((float) juce::roundToInt (param (apvts, "bOctave")) + param (apvts, "bDetune") / 1200.0f);

    // GUI requests
    // HOLD (MIDI keys): on = the chord keeps sounding after the keys are released; off = only while held
    const bool holdOn = param (apvts, "hold") > 0.5f;
    if (lastHoldOn && ! holdOn)
    {
        // HOLD switched off: release a chord held by MIDI keys. A chord switched on by mouse / PC keys
        // stays on (HOLD only decides what MIDI keys do), so ARP / LIVE keep playing.
        if (rootHeldMask == 0) holdChord = -1;
    }
    lastHoldOn = holdOn;

    // a release is applied one block later, so even a very short click strums once
    if (deferredRelease >= 0 && ! holdOn && deferredRelease == latchedChord)
        latchedChord = -1;
    deferredRelease = -1;
    const int click = pendingChordClick.exchange (-1);
    if (click >= 0)
    {
        holdChord = -1;
        latchedChord = (holdOn && click == latchedChord && rootHeldMask == 0) ? -1 : click;
        stackParts.clear();
        if (latchedChord >= 0 && ! chords::isCustom (latchedChord)) stackParts.push_back (latchedChord);
    }
    deferredRelease = pendingChordRelease.exchange (-1);
    // a mouse click on a chord button always latches (a mouse cannot hold a chord and strum at once);
    // HOLD only decides what MIDI keys do
    if (const int latch = pendingChordLatch.exchange (-1); latch >= 0)
    {
        holdChord = -1;
        latchedChord = (latch == latchedChord && rootHeldMask == 0) ? -1 : latch;
        stackParts.clear();
        if (latchedChord >= 0 && ! chords::isCustom (latchedChord)) stackParts.push_back (latchedChord);
    }
    // Ctrl / Shift + click: stack another chord on top of the one that is on (or take it away again)
    if (const int add = pendingChordAdd.exchange (-1); add >= 0)
    {
        holdChord = -1;
        if (latchedChord < 0 || stackParts.empty()) stackParts = { add };
        else
        {
            auto it = std::find (stackParts.begin(), stackParts.end(), add);
            if (it != stackParts.end()) stackParts.erase (it);
            else if (stackParts.size() < 4) stackParts.push_back (add);
        }
        latchedChord = chords::stack (stackParts);
        // the instrument's button combinations on one root: major + 7 = maj7, minor + 7 = m7,
        // major + minor = dim, all three = aug
        if (stackParts.size() >= 2)
        {
            const int root = chords::rootOf (stackParts.front());
            int typesMask = 0;
            bool sameRoot = true;
            for (int c : stackParts)
            {
                if (chords::isCustom (c) || chords::rootOf (c) != root || chords::typeOf (c) > 2) { sameRoot = false; break; }
                typesMask |= 1 << chords::typeOf (c);
            }
            if (sameRoot)
            {
                const int t = typesMask == 0b101 ? 3 : typesMask == 0b110 ? 4 : typesMask == 0b011 ? 5 : typesMask == 0b111 ? 6 : -1;
                if (t >= 0) latchedChord = chords::index (root, t);
            }
        }
    }
    if (latchedChord < 0) stackParts.clear();
    for (size_t i = 0; i < stackGui.size(); ++i) stackGui[i].store (i < stackParts.size() && stackParts.size() > 1 ? stackParts[i] : -1);

    for (const auto meta : midi)
        handleMidi (meta.getMessage(), meta.samplePosition);

    // PROGRESSION follows the song: bar N plays slot N (the filled slots in a loop)
    std::array<int, prog::numSlots> usedSlots {};
    int numUsed = 0;
    for (int i = 0; i < prog::numSlots; ++i) if (progSlots[(size_t) i].load() >= 0) usedSlots[(size_t) numUsed++] = i;
    // PLAY in the plugin is enough: with FL stopped it runs on its own clock (in the song tempo),
    // with FL playing it follows the song
    const bool progPlaying = false && progOn.load() && numUsed > 0;   // the progression row is retired (MOTION grid instead)
    lastProgPlaying = progPlaying;
    if (progPlaying && ! hostPlaying)
    {
        if (! progWasPlaying || progOnHost) progOwnPpq = 0.0;
    }
    const bool restartProg = progPlaying && (! progWasPlaying || progOnHost != hostPlaying);
    progOnHost = hostPlaying;
    const double progPpq = hostPlaying ? ppq : progOwnPpq;
    int progBar = 0;
    if (progPlaying)
    {
        // FL Studio loops a short pattern (1 bar when it is empty): the bars are counted here, so the
        // progression keeps moving on even when the song position jumps back to the start of the loop
        const int hostBar = (int) std::floor (progPpq / 4.0 + 1.0e-9);
        if (restartProg) { progBarOffset = -hostBar; progLastHostBar = hostBar; progBarCached = -1000; }
        else if (hostBar < progLastHostBar) progBarOffset += progLastHostBar - hostBar + 1;   // the loop jumped back
        progLastHostBar = hostBar;
        progBar = hostBar + progBarOffset;
        const int slot = usedSlots[(size_t) (((progBar % numUsed) + numUsed) % numUsed)];
        progSlotGui.store (slot);
        const int c = progSlots[(size_t) slot].load();
        if (c != latchedChord) { latchedChord = c; holdChord = -1; stackParts.clear(); stackParts.push_back (c); }
    }
    else progSlotGui.store (-1);
    progWasPlaying = progPlaying;

    int chord = resolveChord();
    if (gridPlay.load() && chord < 0)
        chord = splitChord >= 0 ? splitChord : chords::index (juce::roundToInt (param (apvts, "keyRoot")), param (apvts, "keyMode") > 0.5f ? 1 : 0);
    const bool splitOn = juce::roundToInt (param (apvts, "keysPlay")) == 6 && ! chordZones.load();
    if (! splitOn) { splitChord = -1; splitHeld = 0; splitLeft.fill (false); }
    if (latchedChord >= 0 || rootHeldMask != 0) splitChord = -1;   // a chord button / zone wins
    // the left-hand chord sounds (chord layer, bass, LIVE pattern) while held, or kept with HOLD
    if (chord < 0 && splitChord >= 0 && (splitHeld > 0 || param (apvts, "hold") > 0.5f)) chord = splitChord;
    // notes coming from the host while its transport runs (piano roll, playlist) are already in the
    // project: the MIDI capture only records what is played by hand on the plugin
    hostDriven = hostPlaying && (rootHeldMask != 0 || heldMelodicCount > 0 || numTypesHeld > 0 || splitHeld > 0 || numSplitHits > 0);

    // preset preview: ONE stroke of the key chord (plus its chord layer and bass) so the sound can be heard.
    // It never changes the play mode, never repeats, never records, and a real chord always wins.
    // picking a sound: ONE note (the key's root, around middle C), never a strum
    if (previewRequest.exchange (false))
    {
        const int n = 60 + juce::roundToInt (param (apvts, "keyRoot"));
        schedulePitch (0, n, 0.75f);
        if (numReleases < (int) releases.size())
            releases[(size_t) numReleases++] = { (int) (1.4 * sr), n + 12 * juce::roundToInt (param (apvts, "harpOctave")), 0.4f };
    }
    bool previewing = false;
    if (chord >= 0) previewSamplesLeft = 0;
    if (previewSamplesLeft > 0)
    {
        chord = chords::index (juce::roundToInt (param (apvts, "keyRoot")), param (apvts, "keyMode") > 0.5f ? 1 : 0);
        previewSamplesLeft -= numSamples;
        previewing = true;
    }

    // Chord Memory: record new chords while REC is on
    if (! previewing && chord >= 0 && chord != memRecordedLast) memRecordChord (chord);
    if (! previewing) memRecordedLast = chord;

    // MEMO mode plays the stored sequence (unless recording)
    if (mode != lastMode) { memSamples = 0.0; arpCounter = 0; }
    lastMode = mode;
    const int bank = memBank.load();
    const int memLen = memLength[(size_t) bank].load();
    if (! previewing && mode == 2 && memLen > 0 && ! memRecord.load())
    {
        const double stepQ = memStepQuarters (juce::roundToInt (param (apvts, "memStep")));
        int step;
        if (hostPlaying)
            step = (int) (juce::jmax (0.0, std::floor (ppq / stepQ))) % memLen;
        else
        {
            const double samplesPerMemStep = stepQ * 60.0 / bpm * sr;
            step = (int) (memSamples / samplesPerMemStep) % memLen;
            memSamples += numSamples;
        }
        memPlayStep.store (step);
        chord = memChord (bank, step);
    }
    else
        memPlayStep.store (-1);

    {
        // the strings: the playing chord; in SPLIT the last left-hand chord stays on the strings, else the key chord
        int strings = chord;
        if (strings < 0 && splitOn)
            strings = splitChord >= 0 ? splitChord : chords::index (juce::roundToInt (param (apvts, "keyRoot")), param (apvts, "keyMode") > 0.5f ? 1 : 0);
        // NOTES: the strings follow the notes you hold; let go and the last chord stays (the key chord at the start),
        // so the strum keys and the mouse always have something to strum
        if (strings < 0)
        {
            if (heldMelodicCount > 0) { const int c = chordFromHeldNotes(); if (c >= 0) lastHeldChord = c; }
            strings = lastHeldChord >= 0 ? lastHeldChord : chords::index (juce::roundToInt (param (apvts, "keyRoot")), param (apvts, "keyMode") > 0.5f ? 1 : 0);
            // NOTES panel: the player picked the notes for the strings
            if (const int pick = stringsPick.load(); chords::isValid (pick)) strings = pick;
        }
        stringsNow.store (strings);
        updateTones (strings, transpose, range);
    }
    activeChord.store (previewing ? -1 : chord);   // a preview does not light a chord button

    const int eventsBefore = numEvents;
    if (const auto plucks = pendingPlucks.exchange (0); plucks != 0 && ! tones.empty())
        for (int i = 0; i < numStrings; ++i)
            if (plucks & (1u << (unsigned) i))
                scheduleNote (0, juce::roundToInt ((float) i * (float) (tones.size() - 1) / (float) (numStrings - 1)), 0.8f, i);

    if (const int st = pendingStrum.exchange (0); st != 0 && ! tones.empty())
        sweep (0, st > 0, st > 0 ? 0 : (int) tones.size() - 1, (int) tones.size(), 0.8f, 0.8f);

    if (progPlaying)
    {
        const double beatsPerSample = bpm / 60.0 / getSampleRate();
        // work in the counted bars: shift the host position by the loop offset
        const double vppq = progPpq + 4.0 * progBarOffset;
        const double endPpq = vppq + numSamples * beatsPerSample;
        const auto settings = progSettings();
        for (int b = progBar; b <= (int) std::floor (endPpq / 4.0); ++b)
        {
            if (b != progBarCached)
            {
                progNotes.clear();
                const int slot = usedSlots[(size_t) (((b % numUsed) + numUsed) % numUsed)];
                const int prevSlot = usedSlots[(size_t) ((((b - 1) % numUsed) + numUsed) % numUsed)];
                prog::bar (progSlots[(size_t) slot].load(), b, progSlots[(size_t) prevSlot].load(), settings, progNotes);
                progBarCached = b;
            }
            for (const auto& nt : progNotes)
            {
                const double at = 4.0 * b + nt.beat;
                if (at >= vppq - 1.0e-9 && at < endPpq)
                    schedulePitch ((int) ((at - vppq) / beatsPerSample), nt.pitch, nt.velocity, true);
            }
        }
    }
    else progBarCached = -1000;
    if (progPlaying && ! hostPlaying) progOwnPpq += numSamples * bpm / 60.0 / getSampleRate();

    // SPLIT right hand: white keys pluck one string each (C4 = the lowest), black keys strum the whole chord
    for (int i = 0; i < numSplitHits && ! tones.empty(); ++i)
    {
        const auto& h = splitHits[(size_t) i];
        const int nt = (int) tones.size();
        const float v = juce::jlimit (0.1f, 1.0f, h.vel);
        if (h.strum == 2)
        {
            // left-hand chord: a soft down stroke so you hear what you picked
            if (! lastProgPlaying && (param (apvts, "gridOn") < 0.5f || juce::roundToInt (param (apvts, "motionType")) == 4))
            {
                const float vv = juce::jlimit (0.2f, 0.95f, 0.3f + 0.65f * v);
                sweep (h.pos, true, 0, nt, vv, 1.2f);   // the STROKE switches shape it inside sweep()
            }
            continue;
        }
        if (h.strum == 0)
        {
            if (h.string < nt) scheduleNote (h.pos, h.string, v);
            else
            {
                // above the plate: keep climbing through the chord tones (every key plays, up to the top of the keyboard)
                int mask = 0;
                for (int t : tones) mask |= 1 << (t % 12);
                int pitch = tones.back();
                for (int k = nt - 1; k < h.string && pitch < 120; ++k)
                    do ++pitch; while (pitch < 127 && ! (mask & (1 << (pitch % 12))));
                schedulePitch (h.pos, juce::jmin (127, pitch), v, true);
            }
        }
        else
        {
            // whole stroke (black keys, strum keys); h.string = kind: 0 full, 1 slow, 2 low strings, 3 high strings, 4 middle out, 5 gliss
            if (h.string == 4)
            {
                const int mid = nt / 2;
                const float vv = juce::jlimit (0.2f, 1.0f, 0.35f + 0.65f * v), sp = 1.5f - 0.9f * v;
                sweep (h.pos, true, mid, nt - mid, vv, sp);
                sweep (h.pos, false, mid - 1, mid, vv * 0.9f, sp);
                continue;
            }
            if (h.string == 5)
            {
                const float vv = juce::jlimit (0.2f, 1.0f, 0.35f + 0.65f * v);
                const float stepMs = mp ("strumSpeed") * 0.6f;
                sweep (h.pos, true, 0, nt, vv, 0.6f);
                sweep (h.pos + (int) ((float) nt * stepMs * 0.001f * (float) sr), false, nt - 2, nt - 1, vv * 0.85f, 0.6f);
                continue;
            }
            const int lo = h.string == 3 ? nt / 2 : 0, hi = h.string == 2 ? juce::jmax (1, nt / 2) : nt;
            const float speed = (h.string == 1 ? 3.2f : 1.0f) * (1.5f - 0.9f * v);
            sweep (h.pos, h.strum > 0, h.strum > 0 ? lo : hi - 1, hi - lo, juce::jlimit (0.2f, 1.0f, 0.35f + 0.65f * v), speed);
        }
    }
    numSplitHits = 0;

    // TREMOLO key: fast up / down strokes while the key is held (1/32 at the song tempo)
    if (tremoloHeld && ! tones.empty())
    {
        const int every = juce::jmax (64, (int) (60.0 / bpm / 8.0 * sr));
        const int nt = (int) tones.size();
        while (tremoloLeft < numSamples)
        {
            const float v = juce::jlimit (0.2f, 1.0f, 0.3f + 0.55f * tremoloVel);
            inStroke = true;   // a tremolo is its own stroke
            sweep (juce::jmax (0, tremoloLeft), tremoloUp, tremoloUp ? 0 : nt - 1, nt, v, 0.35f);
            inStroke = false;
            tremoloUp = ! tremoloUp;
            tremoloLeft += every;
        }
        tremoloLeft -= numSamples;
    }
    // DAMP key: the hand lies on the strings
    if (dampNow) { dampNow = false; for (auto& v : voices) if (v.isActive()) v.release (0.06f); }

    // MOTION: the grid plays the chord that is on (keys, chord buttons, PLAY), in time with the song
    {
        // MOTION plays only when asked: PLAY is on, or keys are held (MIDI keyboard / piano roll).
        // Picking a preset or clicking a chord button only lets you hear the sound (one stroke), it never starts the loop.
        if (auditionBar.exchange (false)) { motionOneShot = (int) (4.0 * 60.0 / bpm * sr); motionRestart = true; }
        if (motionRestart) { motionRestart = false; if (! hostPlaying) gridOwnPpq = 0.0; gridBarCached = -1000; }
        const bool keysHeld = splitHeld > 0 || rootHeldMask != 0 || numTypesHeld > 0 || motionKeysHeld > 0 || motionOneShot > 0
                              || (heldMelodicCount > 0 && juce::roundToInt (param (apvts, "keysPlay")) != 0);   // Notes mode = a melody, not a chord to move
        const int motionChord = (previewing || ! (gridPlay.load() || keysHeld)) ? -1 : (chord >= 0 ? chord : -1);
        const bool motionByKey = (motionKeysHeld > 0 || motionOneShot > 0) && juce::roundToInt (param (apvts, "motionType")) != 4;   // the strum keys play the window even with MOTION off
        if ((motionOn || motionByKey) && motionChord >= 0 && ! tones.empty())
        {
            const double bps = bpm / 60.0 / getSampleRate();
            if (gridLastChord < 0 && ! hostPlaying) gridOwnPpq = 0.0;   // a new chord without the song: start at step 1
            const double start = hostPlaying ? ppq : gridOwnPpq;
            const double end = start + numSamples * bps;
            auto st = progSettings();
            st.mode = st.motionType == 2 ? prog::strum : prog::grid;   // RHYTHM = the strum patterns (3-3-2, half-time ...)
            for (int b = (int) std::floor (start / 4.0); b <= (int) std::floor (end / 4.0); ++b)
            {
                if (b != gridBarCached || motionChord != gridCachedChord)
                {
                    gridNotes.clear();
                    prog::bar (motionChord, b, motionChord, st, gridNotes);
                    gridBarCached = b; gridCachedChord = motionChord;
                }
                for (const auto& nt : gridNotes)
                {
                    const double at = 4.0 * b + nt.beat;
                    if (at >= start - 1.0e-9 && at < end)
                    {
                        const int when = (int) ((at - start) / bps);
                        schedulePitch (when, nt.pitch, nt.velocity, true);
                        // NOTES (GRID / LINES / STRINGS): how long a string rings. Ring = the harp decays by itself;
                        // Long / Medium / Tight damp it after the note, so the pattern is heard clearly and changes hit hard
                        static const float gateOf[] = { 0.0f, 1.0f, 0.6f, 0.3f };
                        const int gate = juce::roundToInt (param (apvts, "motionGate"));
                        if (st.motionType != 2 && gate > 0 && numReleases < (int) releases.size())
                        {
                            const double len = juce::jmax (0.06, nt.length * gateOf[gate] * 0.95);
                            releases[(size_t) numReleases++] = { when + (int) (len / bps), nt.pitch, gate == 3 ? 0.06f : 0.12f };
                        }
                    }
                }
            }
            gridStepGui.store (st.motionType == 3 ? 2000 + (int) (std::fmod (start, 4.0) * 100.0)
                             : st.motionType == 2 ? (int) std::floor (std::fmod (start, 4.0) / 0.25) % juce::jmax (1, st.strumSteps.length())
                             : st.motionType == 1 ? 1000 + (int) (std::fmod (start, 4.0) / 4.0 * prog::linePoints)
                                                  : (int) std::floor (std::fmod (start, 4.0) / st.gridStepBeats) % juce::jmax (1, st.gridLen));
            if (! hostPlaying) gridOwnPpq = end;
        }
        else gridStepGui.store (-1);
        gridLastChord = motionOn ? motionChord : -1;
        if (motionOneShot > 0) motionOneShot = juce::jmax (0, motionOneShot - numSamples);
    }

    // ARP over held MIDI notes (piano roll melody / chords) when no chord button is active
    const bool arpHeld = ! previewing && mode == 1 && chord < 0 && heldMelodicCount > 0;
    eventsFromHost = arpHeld;
    if (arpHeld)
    {
        tones.clear();
        const int octaves = juce::jmax (1, range - 1);
        for (int o = 0; o < octaves; ++o)
            for (int n = 0; n < 128; ++n)
                if (heldMelodic[(size_t) n] && n + 12 * o < 128) tones.push_back (n + 12 * o);
        std::sort (tones.begin(), tones.end());
        tones.erase (std::unique (tones.begin(), tones.end()), tones.end());
        tonesChord = -3;   // rebuild chord tones next time a chord is used
    }

    // KEYS (LIVE): the held keys become the strings, spread over the RANGE like a harp
    const int keysPlay = juce::roundToInt (param (apvts, "keysPlay"));
    const bool keysMode = ! previewing && mode == 0 && keysPlay > 0 && keysPlay < 4;
    bool anyKeyNote = false;
    for (int n = 0; n < 128 && keysMode; ++n) anyKeyNote = anyKeyNote || keyNotes[(size_t) n] || heldMelodic[(size_t) n];
    int keysSig = -1;
    if (keysMode && anyKeyNote)
    {
        tones.clear();
        std::uint32_t h = 2166136261u;
        const int copies = juce::jmax (1, range - 1);
        for (int n = 0; n < 128; ++n)
            if (heldMelodic[(size_t) n] || keyNotes[(size_t) n])
            {
                h = (h ^ (std::uint32_t) n) * 16777619u;
                for (int o = 0; o < copies && n + 12 * o < 128; ++o) tones.push_back (n + 12 * o);
            }
        std::sort (tones.begin(), tones.end());
        tones.erase (std::unique (tones.begin(), tones.end()), tones.end());
        tonesChord = -3;
        keysSig = 1000 + (int) (h & 0xfffff);
    }
    if (keyStrumAt >= 0)
    {
        if (! keysMode) { keyStrumAt = -1; keyNotes.fill (false); }
        else if (keyStrumAt < numSamples)
        {
            // harder keys = stronger and faster stroke
            sweep (keyStrumAt, true, 0, (int) tones.size(), juce::jlimit (0.2f, 1.0f, 0.35f + 0.65f * keyStrumVel), 1.5f - 0.9f * keyStrumVel);
            keyStrumAt = -1;
            keyNotes.fill (false);
        }
        else keyStrumAt -= numSamples;
    }
    else if (keysMode && keysPlay >= 2)
        keyNotes.fill (false);
    const bool keysPattern = keysMode && keysPlay == 2 && heldMelodicCount > 0;

    // Live Strum / Arp clock (MEMO uses the Live Strum hand)
    const bool arp = (chord >= 0 && mode == 1) || arpHeld;
    const bool strumming = ! previewing && ((chord >= 0 && (mode == 0 || mode == 2 || mode == 1)) || arpHeld || keysPattern);
    if (previewing && previewStroke)
    {
        scheduleStrum (0, true);
        previewStroke = false;
    }
    const int pattern = juce::roundToInt (param (apvts, "strumPattern"));
    const bool usePattern = ! arp && pattern > 0;
    static const double speedMuls[] = { 2.0, 1.0, 0.5 };
    const double stepQuarters = usePattern ? 0.25 * speedMuls[juce::jlimit (0, 2, juce::roundToInt (param (apvts, "strumSpeedMul")))] : rateInQuarters (rateIndex);
    const bool restartOnChord = usePattern && juce::roundToInt (param (apvts, "strumRestart")) == 1;
    const double samplesPerStep = stepQuarters * 60.0 / bpm * sr;
    const float swing = usePattern ? mp ("strumSwing") : (arp ? param (apvts, "arpSwing") : 0.0f);
    auto hit = [&] (int offset, juce::int64 k)
    {
        if (arp) scheduleArpStep (offset + ((k % 2 == 1) ? (int) (swing * 0.33 * samplesPerStep) : 0));
        else if (usePattern)
        {
            // swing: every second 1/16 comes late (up to a triplet feel)
            const juce::int64 kk = restartOnChord ? k - patOrigin : k;
            const int late = (kk % 2 == 1) ? (int) (swing * 0.33 * samplesPerStep) : 0;
            patternStep (offset + late, kk);
        }
        else scheduleStrum (offset, (k % 2) == 0);
    };
    const int strumId = keysPattern && chord < 0 ? keysSig : chord;

    if (strumming)
    {
        if (strumId != lastStrumId || ! wasStrumming)
        {
            if (! arp || ! wasStrumming) arpCounter = 0;
            if (hostPlaying) patOrigin = (juce::int64) std::floor (ppq / stepQuarters);
            if (usePattern && rootHeldMask != 0)
            {
                // a chord key on the MIDI keyboard: harder = stronger and faster first stroke
                scheduleStrum (0, true, 0.5f + 0.6f * lastVelocity, 1.45f - 0.85f * lastVelocity);
            }
            else if (usePattern)
            {
                // a new chord: the hand hits it at once (a thumb pattern starts on the bass)
                const auto ps = patternString (pattern);
                const char first = ps.isNotEmpty() ? (char) ps[0] : 'D';
                stroke (0, first == 'B' ? 'B' : (first == 'T' ? 'T' : (first == 'H' ? 'H' : 'D')), 1.0f);
            }
            else hit (0, 0);   // instant stroke on a new chord
            samplesToNextStrum = samplesPerStep - numSamples;
            strumCount = 1;
            samplesSinceTrigger = 0;
        }
        else if (hostPlaying)
        {
            const double quartersPerSample = bpm / (60.0 * sr);
            const double endPpq = ppq + numSamples * quartersPerSample;
            for (double k = std::ceil (ppq / stepQuarters); k * stepQuarters < endPpq; k += 1.0)
            {
                const int offset = (int) ((k * stepQuarters - ppq) / quartersPerSample);
                if (samplesSinceTrigger + offset > (int) (0.05 * sr))
                    hit (offset, (juce::int64) k);
            }
        }
        else
        {
            double pos = samplesToNextStrum;
            while (pos < numSamples)
            {
                hit (juce::jmax (0, (int) pos), strumCount);
                ++strumCount;
                pos += samplesPerStep;
            }
            samplesToNextStrum = pos - numSamples;
        }
    }

    eventsFromHost = false;

    // chord layer and bass ring while the harp is played (strum, arp, plucks, MIDI keys held),
    // and release when the playing stops - a latched chord button alone never drones forever
    const int playedChord = chord;
    {
        const double stepQ = rateInQuarters (rateIndex);
        int gateLen = juce::jmax ((int) (0.6 * sr), (int) (stepQ * 60.0 / bpm * sr * 1.15));
        if (usePattern) gateLen = juce::jmax (gateLen, (int) (4.0 * 60.0 / bpm * sr * 0.75));   // patterns leave longer gaps
        if (numEvents > eventsBefore) layerGateLeft = gateLen;
        else layerGateLeft = juce::jmax (0, layerGateLeft - numSamples);
        if (chord >= 0 && rootHeldMask == 0 && layerGateLeft <= 0) chord = -1;
    }

    // MIDI capture, "chords" view: what is being played as one long block of stacked notes
    // (what a producer would write in the piano roll), voiced like the chord layer
    {
        int id = previewing ? -1 : chord;
        std::vector<int> notesNow;
        if (id >= 0)
            notesNow = chords::voicing (id, transpose, juce::roundToInt (param (apvts, "voicing")), prevBlock);
        else if (keysPlay >= 4 && flourishChord >= 0 && heldMelodicCount > 0)
        {
            id = flourishChord;
            notesNow = chords::voicing (id, transpose, juce::roundToInt (param (apvts, "voicing")), prevBlock);
        }
        else if (keysMode && heldMelodicCount > 0 && layerGateLeft > 0)
        {
            id = keysSig;
            for (int n = 0; n < 128; ++n) if (heldMelodic[(size_t) n]) notesNow.push_back (n);
        }
        if (id != blockId)
        {
            const double now = (double) sampleClock / sr;
            for (int n : blockNotes) capture (now, n, 0.0f, 3, false);
            blockNotes = notesNow;
            for (int n : blockNotes) capture (now, n, 0.8f, 3, true);
            if (! blockNotes.empty()) prevBlock = blockNotes;
            blockId = id;
        }
    }
    layersFromHost = false;
    if (heldMelodicCount == 0) { detectedChord.store (-1); flourishChord = -1; }
    if (chord < 0 && ! previewing && heldMelodicCount > 0)
    {
        chord = (keysPlay >= 4 && flourishChord >= 0) ? flourishChord : chordFromHeldNotes();
        layersFromHost = chord >= 0;
        detectedChord.store (chord);
    }
    updateChordLayer (chord, transpose);
    {
        const int newBassType = juce::roundToInt (param (apvts, "bassType"));
        if (newBassType != bassType || chord != bassChord)
        {
            bassType = newBassType;
            bassChord = chord;
            if (chord < 0 || newBassType == 0)
            {
                for (auto& v : bassVoices)
                    if (v.isActive() && ! v.isReleasing()) { v.release(); capture ((double) sampleClock / sr, v.getNote(), 0.0f, 2, false); }
            }
            else
            {
                bassBeat (0, 0);   // root on every chord change
                bassSamplesToNext = 60.0 / bpm * sr - numSamples;
                bassCount = 1;
            }
        }
        else if (chord >= 0 && bassType != 0 && juce::roundToInt (param (apvts, "bassMode")) != 0)
        {
            if (hostPlaying)
            {
                const double quartersPerSample = bpm / (60.0 * sr);
                const double endPpq = ppq + numSamples * quartersPerSample;
                for (double k = std::ceil (ppq); k < endPpq; k += 1.0)
                {
                    const int offset = (int) ((k - ppq) / quartersPerSample);
                    if (samplesSinceTrigger + offset > (int) (0.05 * sr))
                        bassBeat (offset, (int) ((juce::int64) k % 4));
                }
            }
            else
            {
                double pos = bassSamplesToNext;
                while (pos < numSamples)
                {
                    bassBeat (juce::jmax (0, (int) pos), (int) (bassCount % 4));
                    ++bassCount;
                    pos += 60.0 / bpm * sr;
                }
                bassSamplesToNext = pos - numSamples;
            }
        }
    }

    chord = playedChord;
    if (chord != sounding && chord >= 0 && juce::roundToInt (param (apvts, "funcMode")) != 0)
        func.trigger();   // one-shot and key trig restart on every new chord
    sounding = previewing ? -1 : chord;
    wasStrumming = strumming;
    lastStrumId = strumming ? strumId : -1;
    if (! previewing) previewMute = false;
    samplesSinceTrigger = juce::jmin (samplesSinceTrigger + numSamples, 1 << 29);

    // vibrato
    // pitch: vibrato + model drift + AGE wow/flutter + WOBBLE
    const float vibRate = mp ("vibRate");
    const float vibDepth = juce::jlimit (0.0f, 1.0f, mp ("vibDepth")) * 0.45f / 12.0f;   // up to ~0.45 semitone
    const float vibInc = vibRate / (float) sr;
    const float driftDepth = (model.drift + age * 14.0f) / 1200.0f;
    const float wobbleDepth = wobble * 0.3f / 12.0f;
    const float pitchTarget = modRouted[mod::tPitch] ? juce::jlimit (-1.0f, 1.0f, modOffset[mod::tPitch]) : 0.0f;   // up to +-1 octave
    for (int i = 0; i < numSamples; ++i)
    {
        const float drift = driftDepth * (0.7f * harp::sine() (wowPhase) + 0.3f * harp::sine() (flutterPhase) * age);
        const float wob = wobbleDepth * (harp::sine() (wobblePhase) + 0.3f * harp::sine() (vibPhase));
        busPitch += (pitchTarget - busPitch) * 0.002f;
        pitchMul[(size_t) i] = std::exp2 (vibDepth * harp::sine() (vibPhase) + drift + wob + busPitch);
        vibPhase = harp::wrap (vibPhase + vibInc);
        wowPhase = harp::wrap (wowPhase + 0.45f / (float) sr);
        flutterPhase = harp::wrap (flutterPhase + 6.5f / (float) sr);
        wobblePhase = harp::wrap (wobblePhase + 0.8f / (float) sr);
    }

    const float chordTone = mp ("chordFilter") * (1.0f - 0.4f * age);
    const float chordGain = (param (apvts, "chordType") > 0.5f && (soloMask == 0 || (soloMask & 8))) ? juce::Decibels::decibelsToGain (mp ("chordLevel")) : 0.0f;   // VOICE: Chord
    const float bassTone = mp ("bassTone");
    const float bassGain = (param (apvts, "bassType") > 0.5f && (soloMask == 0 || (soloMask & 4))) ? juce::Decibels::decibelsToGain (mp ("bassLevel")) : 0.0f;    // VOICE: Bass

    // harp bus modulation: Harp Filter (Harp Tone also sweeps it, so it is heard on ringing notes), Volume, Pan
    const bool filterOn = modRouted[mod::tFilter] || modRouted[(size_t) targetIndex ("harpTone")];
    const float cutTarget = ! filterOn ? 1.0f
        : juce::jlimit (0.0f, 1.0f, (modRouted[mod::tFilter] ? 0.62f : 1.0f) + 0.62f * modOffset[mod::tFilter] + 0.8f * modOffset[(size_t) targetIndex ("harpTone")]);
    const float volTarget = modRouted[mod::tVolume] ? juce::jlimit (0.0f, 1.5f, 1.0f + modOffset[mod::tVolume]) : 1.0f;
    const float panTarget = modRouted[mod::tPan] ? juce::jlimit (-1.0f, 1.0f, modOffset[mod::tPan]) : 0.0f;
    if (! filterOn) { busIc1L = busIc2L = busIc1R = busIc2R = 0.0f; busCut = 1.0f; }
    const bool busActive = true;   // the harp always goes through its own bus (soundboard)
    {
        const int mIdx = juce::roundToInt (param (apvts, "model"));
        if (mIdx != boardModel) { soundboard.configure (model.boardSize, model.boardDecay, model.boardDark); boardModel = mIdx; }
    }

    // render in small slices so strum notes start close to their exact time
    if (work.getNumSamples() < numSamples) work.setSize (2, numSamples, false, false, true);
    work.clear();
    auto* left = work.getWritePointer (0);
    auto* right = work.getWritePointer (1);
    std::uint32_t flashes = 0;

    // rhythmic gate: one gain per sample, smoothed so the steps never click
    const bool gateOn = param (apvts, "gateOn") > 0.5f || sw (swStutter) > 0.0f;   // STUTTER switch = the gate
    const bool gateAll = juce::roundToInt (param (apvts, "gateTarget")) == 1;
    if ((int) gateBuf.size() < numSamples) gateBuf.resize ((size_t) numSamples, 1.0f);
    {
        static const double gq[] = { 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0, 2.0, 1.0, 4.0 / 3.0, 2.0 / 3.0, 1.0 / 12.0 };
        const double stepQ = gq[juce::jlimit (0, 9, juce::roundToInt (param (apvts, "gateRate")))];
        const int glen = juce::jlimit (1, gateSteps, gateLen.load());
        const std::uint32_t mask = gateMask.load();
        const float depth = gateOn ? juce::jmax (param (apvts, "gateOn") > 0.5f ? param (apvts, "gateDepth") : 0.0f, sw (swStutter)) : 0.0f;
        const float coef = 1.0f - std::exp (-1.0f / (0.001f * param (apvts, "gateSmooth") * (float) sr));
        const double stepsPerSample = bpm / 60.0 / stepQ / sr;
        const double startStep = hostPlaying ? ppq / stepQ : gateClock;
        int shown = -1;
        for (int i = 0; i < numSamples; ++i)
        {
            const int step = (int) ((juce::int64) std::floor (startStep + i * stepsPerSample) % glen);
            if (i == 0) shown = step;
            const float target = (mask >> (unsigned) step) & 1u ? 1.0f : 1.0f - depth;
            gateGain += (target - gateGain) * coef;
            gateBuf[(size_t) i] = gateGain;
        }
        gateClock = hostPlaying ? 0.0 : gateClock + numSamples * stepsPerSample;
        gateStepGui.store (gateOn ? shown : -1);
    }

    for (int start = 0; start < numSamples; start += 32)
    {
        const int len = juce::jmin (32, numSamples - start);

        // ARP gate: shorten notes (release the voices playing that pitch)
        for (int e = 0; e < numReleases;)
        {
            if (releases[(size_t) e].delay < start + len)
            {
                for (auto& v : voices)
                    if (v.isActive() && (releases[(size_t) e].note < 0 || v.getNote() == releases[(size_t) e].note || v.key == releases[(size_t) e].note)) v.release (releases[(size_t) e].secs);
                releases[(size_t) e] = releases[(size_t) --numReleases];
            }
            else ++e;
        }

        for (int e = 0; e < numEvents;)
        {
            if (events[(size_t) e].delay < start + len)
            {
                curString = events[(size_t) e].visual;
                startVoice (events[(size_t) e].note, events[(size_t) e].velocity);
                curString = -1;
                if (midiOut.load() && ! previewMute)
                {
                    const int pos = juce::jmax (start, events[(size_t) e].delay);
                    midiNote (pos, 0, events[(size_t) e].note, events[(size_t) e].velocity, true);
                    // the note lasts about as long as it rings
                    const int len = (int) (juce::jlimit (0.05f, 4.0f, voiceSettings.decay * 0.5f) * (float) sr);
                    for (int q = 0; q < numPendingOffs; ++q)
                        if (pendingOffs[(size_t) q].note == events[(size_t) e].note && pendingOffs[(size_t) q].channel == 0) { pendingOffs[(size_t) q] = pendingOffs[(size_t) --numPendingOffs]; break; }
                    if (numPendingOffs < (int) pendingOffs.size()) pendingOffs[(size_t) numPendingOffs++] = { pos + len, 0, events[(size_t) e].note };
                }
                if (events[(size_t) e].capture)
                    capture ((double) (sampleClock + start) / sr, events[(size_t) e].note, events[(size_t) e].velocity, 0, true);
                flashes |= 1u << (unsigned) events[(size_t) e].visual;
                events[(size_t) e] = events[(size_t) --numEvents];
            }
            else ++e;
        }

        float* r = right + start;
        if (busActive)
        {
            float hl[32] = {}, hr[32] = {};
            for (auto& v : voices)
                v.render (hl, hr, len, pitchMul.data() + start, rng);
            for (int i = 0; i < len; ++i)
            {
                busCut += (cutTarget - busCut) * 0.003f;
                busVol += (volTarget - busVol) * 0.003f;
                busPan += (panTarget - busPan) * 0.003f;
                const float gg = gateOn && ! gateAll ? gateBuf[(size_t) (start + i)] : 1.0f;
                float x = hl[i] * decayComp * gg, y = hr[i] * decayComp * gg;
                if (filterOn)
                {
                    // TPT state-variable low-pass, 120 Hz .. 20 kHz
                    const float fc = 120.0f * std::exp2 (busCut * 7.37f);
                    const float g = std::tan (juce::MathConstants<float>::pi * juce::jmin (fc, 0.45f * (float) sr) / (float) sr);
                    const float k = 1.0f / 1.4f, a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
                    auto svf = [&] (float in, float& ic1, float& ic2)
                    {
                        const float v3 = in - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
                        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
                        return v2;
                    };
                    x = svf (x, busIc1L, busIc2L); y = svf (y, busIc1R, busIc2R);
                }
                const float pl = std::cos ((busPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.41421356f;
                const float pr = std::sin ((busPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.41421356f;
                hl[i] = x * busVol * juce::jmin (1.0f, pl);
                hr[i] = y * busVol * juce::jmin (1.0f, pr);
                left[start + i] += hl[i];
                r[i] += hr[i];
            }
            soundboard.process (hl, hr, left + start, r, len, model.boardMix);
        }
        else
            for (auto& v : voices)
                v.render (left + start, r, len, pitchMul.data() + start, rng);

        for (auto& v : chordVoices)
            v.render (left + start, r, len, chordTone, chordGain, pitchMul.data() + start);

        if (bassPendingOffset >= 0 && bassPendingOffset < start + len)
        {
            startBassNote();
            bassPendingOffset = -1;
        }
        for (auto& v : bassVoices)
            v.render (left + start, r, len, bassTone, bassGain, pitchMul.data() + start);
    }
    if (bassPendingOffset >= 0) bassPendingOffset -= numSamples;

    // MIDI OUT: note-offs that fall in this block, then hand the notes to the host
    for (int q = 0; q < numPendingOffs;)
    {
        if (pendingOffs[(size_t) q].samples < numSamples)
        {
            midiNote (pendingOffs[(size_t) q].samples, pendingOffs[(size_t) q].channel, pendingOffs[(size_t) q].note, 0.0f, false);
            pendingOffs[(size_t) q] = pendingOffs[(size_t) --numPendingOffs];
        }
        else { pendingOffs[(size_t) q].samples -= numSamples; ++q; }
    }
    midi.clear();
    if (! midiOutBuffer.isEmpty()) { midi.addEvents (midiOutBuffer, 0, numSamples, 0); midiOutBuffer.clear(); }
    for (int e = 0; e < numEvents; ++e) events[(size_t) e].delay -= numSamples;
    for (int e = 0; e < numReleases; ++e) releases[(size_t) e].delay -= numSamples;
    sampleClock += numSamples;
    hostBpm.store (bpm);
    if (flashes != 0) stringFlashes.fetch_or (flashes);

    int inUse = 0;
    for (auto& v : voices) inUse += v.isActive() ? 1 : 0;
    voicesInUse.store (inUse);

    if (gateOn && gateAll)
        for (int i = 0; i < numSamples; ++i) { left[i] *= gateBuf[(size_t) i]; right[i] *= gateBuf[(size_t) i]; }
    PROBE (0, left);
    // model character: tone, width, saturation, chorus, bits (Cartridge), hiss only while playing (+ AGE)
    {
        const float cutoff = model.cutoff * (1.0f - 0.65f * age);
        const float c = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / (float) sr);
        const float width = model.width;
        const float sat = model.sat + age * 0.4f;
        const float drive = 1.0f + sat * 2.5f;
        const float satNorm = 1.0f / std::tanh (drive);
        const float bits = model.bits - age * 3.0f;
        const bool crush = bits < 15.5f;
        const float steps = std::pow (2.0f, bits - 1.0f);
        const float hiss = model.hiss + age * 0.002f;
        const float chorusAmt = model.chorus;
        const float relHiss = std::exp (-1.0f / (0.3f * (float) sr));
        const float fsr = (float) sr;
        for (int i = 0; i < numSamples; ++i)
        {
            float l = left[i], r = right[i];
            const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * width;
            l = mid + side; r = mid - side;
            mlp1L += (l - mlp1L) * c; mlp2L += (mlp1L - mlp2L) * c;
            mlp1R += (r - mlp1R) * c; mlp2R += (mlp1R - mlp2R) * c;
            l = std::tanh (mlp2L * drive) * satNorm;
            r = std::tanh (mlp2R * drive) * satNorm;
            if (chorusAmt > 0.0f)
            {
                // built-in ensemble of the model (two slow taps, opposite phase left / right)
                modelChorusL.push (l); modelChorusR.push (r);
                const float m1 = harp::sine() (chorusPhase), m2 = harp::sine() (harp::wrap (chorusPhase + 0.5f));
                const float wl = modelChorusL.read (0.007f * fsr + 0.0025f * fsr * (0.5f + 0.5f * m1));
                const float wr = modelChorusR.read (0.007f * fsr + 0.0025f * fsr * (0.5f + 0.5f * m2));
                l = l * (1.0f - 0.4f * chorusAmt) + wl * 0.6f * chorusAmt;
                r = r * (1.0f - 0.4f * chorusAmt) + wr * 0.6f * chorusAmt;
                chorusPhase = harp::wrap (chorusPhase + 0.6f / fsr);
            }
            if (crush)
            {
                // lo-fi converter: sample-and-hold + coarse steps, no dither (dither = constant hiss).
                // The step size follows the signal level, so the grit stays on the notes and the
                // decaying tail fades out cleanly instead of turning into noise.
                if (--holdCount <= 0)
                {
                    crushEnv = juce::jmax (std::abs (l) + std::abs (r), crushEnv * 0.995f);
                    const float st = steps / juce::jlimit (0.05f, 1.0f, crushEnv * 1.5f);
                    heldL = std::round (l * st) / st;
                    heldR = std::round (r * st) / st;
                    holdCount = model.hold;
                }
                l = heldL; r = heldR;
            }
            // hiss follows the music, never in silence
            hissEnv = juce::jmax (std::abs (mid), hissEnv * relHiss);
            if (hiss > 0.0f)
            {
                const float h = hiss * juce::jmin (1.0f, hissEnv * 20.0f);
                l += (rng.nextFloat() - 0.5f) * h; r += (rng.nextFloat() - 0.5f) * h;
            }
            left[i] = l; right[i] = r;
        }
    }

    // FILTER (low-pass with resonance) and SATURATION on the whole instrument, before the FX chain
    {
        const float swF = sw (swFilter);
        const float cut = mp ("filterCutoff") * std::pow (0.04f, swF), reso = juce::jmin (1.0f, mp ("filterReso") + 0.35f * swF),
                    drv = juce::jmin (1.0f, mp ("drive") + 0.7f * sw (swDrive));
        const int fMode = juce::roundToInt (param (apvts, "filtMode"));
        const bool f24 = param (apvts, "filt24") > 0.5f;
        // KEY follow (with TRACK) and VELO move the cutoff with the last note played
        const float keyOct = param (apvts, "filtTrack") > 0.5f ? (float) (lastNotePlayed - 60) / 12.0f * param (apvts, "filtKey") : 0.0f;
        const float velOct = param (apvts, "filtVelo") * (lastVelocity - 0.6f) * 4.0f;
        const float cutK = juce::jlimit (30.0f, 20000.0f, cut * std::exp2 (keyOct + velOct));
        const bool filt = fMode != 2 || cutK < 19500.0f || f24;
        if (filt || drv > 0.001f)
        {
            const float fsr = (float) sr;
            const float q = 0.6f + reso * reso * 9.0f;
            const float dGain = 1.0f + drv * drv * 14.0f, makeup = 1.0f / (0.6f + 0.4f * std::sqrt (dGain));
            float* L = left;    // the instrument is in the work buffer here (the host buffer is filled at the end)
            float* R = right;
            for (int i = 0; i < numSamples; ++i)
            {
                if ((i & 15) == 0)
                {
                    cutSmooth += (cutK - cutSmooth) * 0.25f;
                    masterFiltL.set (juce::jmin (cutSmooth, fsr * 0.45f), q, fsr);
                    masterFiltR.set (juce::jmin (cutSmooth, fsr * 0.45f), q, fsr);
                    masterFilt2L.set (juce::jmin (cutSmooth, fsr * 0.45f), 0.7f, fsr);
                    masterFilt2R.set (juce::jmin (cutSmooth, fsr * 0.45f), 0.7f, fsr);
                }
                float bp;
                float l = L[i], r = R != nullptr ? R[i] : l;
                if (filt || cutSmooth < 19500.0f)
                {
                    // MODE: HPF / BPF / LPF from one state-variable filter; 24 dB = a second stage
                    auto run = [fMode] (layers::Svf& f, float x)
                    {
                        float b; const float lp = f.process (x, b);
                        return fMode == 2 ? lp : fMode == 1 ? b * f.k : x - f.k * b - lp;
                    };
                    l = run (masterFiltL, l); r = run (masterFiltR, r);
                    if (f24) { l = run (masterFilt2L, l); r = run (masterFilt2R, r); }
                }
                if (drv > 0.001f)
                {
                    // warm tape-like saturation: soft, slightly asymmetric
                    l = std::tanh (l * dGain + 0.08f * drv) - std::tanh (0.08f * drv);
                    r = std::tanh (r * dGain + 0.08f * drv) - std::tanh (0.08f * drv);
                    l *= makeup; r *= makeup;
                }
                L[i] = l; if (R != nullptr) R[i] = r;
            }
        }
        else cutSmooth = 20000.0f;
    }

    // PRISM peaks: the pitch classes on the strings (the playing chord), spread from C3 to B6; no chord = the key's tonic
    {
        int mask = 0;
        for (int t : tones) mask |= 1 << (((t % 12) + 12) % 12);
        if (mask == 0) mask = (1 << (juce::roundToInt (param (apvts, "keyRoot")) % 12)) | (1 << ((juce::roundToInt (param (apvts, "keyRoot")) + 7) % 12));
        std::array<float, 12> hz {};
        int np = 0;
        std::array<int, 64> cand {};
        int nc = 0;
        for (int note = 48; note < 96 && nc < 64; ++note)
            if (mask & (1 << (note % 12))) cand[(size_t) nc++] = note;
        for (int k = 0; k < 12 && nc > 0; ++k)   // up to 12 peaks, evenly picked across the candidates
        {
            const int note = cand[(size_t) juce::jmin (nc - 1, (k * nc) / juce::jmin (12, nc))];
            if (np > 0 && hz[(size_t) np - 1] >= 440.0f * std::pow (2.0f, (float) (note - 69) / 12.0f)) continue;
            hz[(size_t) np++] = 440.0f * std::pow (2.0f, (float) (note - 69) / 12.0f);
            if (np >= juce::jmin (12, nc)) break;
        }
        for (auto& u : fxUnits) { u.prismHz = hz; u.prismPeaks = np; }
    }

    // FX chain: 8 slots in order
    for (int i = 0; i < fx::numSlots; ++i)
    {
        const juce::String id ("fx" + juce::String (i + 1));
        const int type = juce::roundToInt (param (apvts, (id + "Type").toRawUTF8()));
        if (param (apvts, (id + "On").toRawUTF8()) < 0.5f) continue;
        const float a = i < 4 ? mp ((id + "A").toRawUTF8()) : param (apvts, (id + "A").toRawUTF8());
        const float b = param (apvts, (id + "B").toRawUTF8());
        const float mixv = i < 4 ? mp ((id + "Mix").toRawUTF8()) : param (apvts, (id + "Mix").toRawUTF8());
        PROBE (1 + i, left);
        fxUnits[(size_t) i].process (type, a, b, mixv, left, right, numSamples, bpm);
    }

    // SWITCH effects after the chain: CRUSH, DELAY, REVERB, SHIMMER
    {
        struct S { int sw, type; float a, b, mix; };
        const S list[] = { { swCrush, 4, 0.0f, 0.5f, 1.0f }, { swDelay, 13, 0.55f, 0.4f, 0.38f }, { swReverb, 14, 0.8f, 0.5f, 0.5f }, { swShimmer, 15, 0.85f, 0.55f, 0.45f },
                           { swChorus, 7, 0.35f, 0.45f, 0.6f }, { swPhaser, 9, 0.25f, 0.6f, 0.7f }, { swFlanger, 8, 0.3f, 0.6f, 0.6f }, { swRadio, 10, 0.6f, 0.5f, 1.0f }, { swVibrato, 6, 0.45f, 0.5f, 1.0f } };
        for (int k = 0; k < 9; ++k)
        {
            const float amt = sw (list[k].sw);
            if (amt <= 0.0f && ! switchFxActive[(size_t) k]) continue;
            switchFxActive[(size_t) k] = amt > 0.0f;
            const float a = list[k].sw == swCrush ? 0.25f + 0.7f * amt : list[k].a;
            switchFx[(size_t) k].process (list[k].type, a, list[k].b, list[k].mix * amt, left, right, numSamples, bpm);
        }
    }

    // DREAM: a huge soft reverb on top
    if (dream > 0.001f)
    {
        if ((int) dreamL.size() < numSamples) { dreamL.resize ((size_t) numSamples); dreamR.resize ((size_t) numSamples); }
        std::copy (left, left + numSamples, dreamL.begin());
        std::copy (right, right + numSamples, dreamR.begin());
        dreamReverb.process (dreamL.data(), dreamR.data(), numSamples, 0.92f, 0.35f);
        for (int i = 0; i < numSamples; ++i) { left[i] += dreamL[(size_t) i] * dream * 0.7f; right[i] += dreamR[(size_t) i] * dream * 0.7f; }
    }

    // CONSOLE: SYMPATHETIC strings, TILT and WOOD, then the SPACE room
    {
        if (tones != symTones) { symTones = tones; sympathetic.tune (tones, (float) sr); }
        sympathetic.process (left, right, numSamples, param (apvts, "strSympath"));
        colour.process (left, right, numSamples, param (apvts, "bodyTilt"), param (apvts, "morphWood"), (float) sr);
        hama::consolefx::Space::Settings st;
        st.room = juce::roundToInt (param (apvts, "spRoom"));
        st.size = param (apvts, "spSize"); st.tone = param (apvts, "spTone"); st.pre = param (apvts, "spPre"); st.mix = param (apvts, "spMix");
        st.width = param (apvts, "spWidth"); st.duck = param (apvts, "spDuck"); st.mod = param (apvts, "spMod"); st.lowCut = param (apvts, "spLowcut");
        st.reverse = param (apvts, "spReverse") > 0.5f; st.tail = param (apvts, "spTail") > 0.5f;
        space.process (left, right, numSamples, st);
    }

    PROBE (12, left);
    // master + soft limit + meters
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (mp ("master") + param (apvts, "presetTrim")));
    widthTarget = juce::jmin (2.0f, param (apvts, "width") + 0.9f * sw (swWide));
    polishNow = param (apvts, "polish");
    compAtt = 1.0f - std::exp (-1.0f / (0.012f * (float) sr));
    compRel = 1.0f - std::exp (-1.0f / (0.2f * (float) sr));
    airCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 9000.0f / (float) sr);
    sideLowCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 150.0f / (float) sr);
    float pl = 0.0f, pr = 0.0f;
    // clean peak limiter (ceiling -1 dB): no clipping, no crackle
    const float ceiling = 0.89f;
    const float release = std::exp (-1.0f / (0.12f * (float) sr));
    const float attackCoef = 1.0f - std::exp (-5.0f / (float) limLook);
    const float dcCoef = 1.0f - juce::MathConstants<float>::twoPi * 12.0f / (float) sr;   // 12 Hz DC blocker
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = masterGain.getNextValue();
        float l = left[i] * g, r = right[i] * g;
        // POLISH: gentle glue compression and a little air on top, like a mix bus
        if (polishNow > 0.001f)
        {
            const float lev = juce::jmax (std::abs (l), std::abs (r));
            compEnv += (lev - compEnv) * (lev > compEnv ? compAtt : compRel);
            const float thr = 0.18f;                             // about -15 dBFS
            float gr = 1.0f;
            if (compEnv > thr)
            {
                const float ratio = 1.0f + 2.0f * polishNow;
                gr = std::pow (compEnv / thr, 1.0f / ratio - 1.0f);
            }
            const float mk = 1.0f + 0.12f * polishNow;           // about +1 dB make-up
            l *= gr * mk; r *= gr * mk;
            airLpL += (l - airLpL) * airCoef; airLpR += (r - airLpR) * airCoef;
            const float air = 0.16f * polishNow;                 // up to about +1.3 dB above 9 kHz (no hiss boost)
            l += (l - airLpL) * air; r += (r - airLpR) * air;
        }
        // stereo width (mid/side): the bass stays mono when widening, so the low end keeps its punch
        {
            widthNow += (widthTarget - widthNow) * 0.002f;
            const float m = 0.5f * (l + r), sd = 0.5f * (l - r);
            sideLow += (sd - sideLow) * sideLowCoef;
            const float s2 = widthNow <= 1.0f ? sd * widthNow : sideLow + (sd - sideLow) * widthNow;
            l = m + s2; r = m - s2;
        }
        const float hl = l - dcInL + dcCoef * dcOutL; dcInL = l; dcOutL = hl; l = hl;
        const float hr = r - dcInR + dcCoef * dcOutR; dcInR = r; dcOutR = hr; r = hr;
        // look-ahead: remember the gain each sample needs, play the audio limLook samples later
        const float peak = juce::jmax (std::abs (l), std::abs (r));
        limNeed[(size_t) limPos] = peak > ceiling ? ceiling / peak : 1.0f;
        const float outL = limDelL[(size_t) limPos], outR = limDelR[(size_t) limPos];
        limDelL[(size_t) limPos] = l; limDelR[(size_t) limPos] = r;
        if (++limPos >= limLook) limPos = 0;
        float need = 1.0f;
        for (int k = 0; k < limLook; ++k) need = juce::jmin (need, limNeed[(size_t) k]);
        if (need < limGain) limGain += (need - limGain) * attackCoef;      // glide down over the look-ahead
        else                limGain = 1.0f - (1.0f - limGain) * release;   // recover slowly
        l = outL * limGain; r = outR * limGain;
        left[i] = juce::jlimit (-1.0f, 1.0f, l);
        right[i] = juce::jlimit (-1.0f, 1.0f, r);
        pl = juce::jmax (pl, std::abs (left[i]));
        pr = juce::jmax (pr, std::abs (right[i]));
    }
    // switching the sound mid-note: no step, glide from the last output sample into the new sound
    {
        std::uint64_t sig = 1469598103934665603ull;
        auto mix = [&sig] (int v) { sig = (sig ^ (std::uint64_t) (v + 7)) * 1099511628211ull; };
        mix (juce::roundToInt (param (apvts, "model")));
        mix (juce::roundToInt (param (apvts, "harpVoice")));
        mix (juce::roundToInt (param (apvts, "harpVoice2")));
        mix (juce::roundToInt (param (apvts, "dual")));
        mix (juce::roundToInt (param (apvts, "chordType")));
        mix (juce::roundToInt (param (apvts, "bassType")));
        for (int i = 1; i <= fx::numSlots; ++i)
        {
            mix (juce::roundToInt (param (apvts, ("fx" + juce::String (i) + "Type").toRawUTF8())));
            mix (juce::roundToInt (param (apvts, ("fx" + juce::String (i) + "On").toRawUTF8())));
        }
        if (sig != soundSignature)
        {
            if (soundSignature != 0) { declickLen = juce::jmax (16, (int) (0.003 * sr)); declickLeft = declickLen; }
            soundSignature = sig;
        }
        for (int i = 0; i < numSamples && declickLeft > 0; ++i, --declickLeft)
        {
            const float t = 1.0f - (float) declickLeft / (float) declickLen;
            const float w = t * t * (3.0f - 2.0f * t);
            left[i] = lastOutL + (left[i] - lastOutL) * w;
            right[i] = lastOutR + (right[i] - lastOutR) * w;
        }
        if (numSamples > 0) { lastOutL = left[numSamples - 1]; lastOutR = right[numSamples - 1]; }
    }
    if (buffer.getNumChannels() > 1)
    {
        buffer.copyFrom (0, 0, left, numSamples);
        buffer.copyFrom (1, 0, right, numSamples);
    }
    else
    {
        buffer.copyFrom (0, 0, left, numSamples, 0.5f);
        buffer.addFrom (0, 0, right, numSamples, 0.5f);
    }
    PROBE (13, left);
    meterLeft.store (juce::jmax (pl, meterLeft.load() * 0.85f));
    meterRight.store (juce::jmax (pr, meterRight.load() * 0.85f));
}

//==============================================================================
void Hamamatsu108Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiWidth2", editorWidth.load(), nullptr);   // 2 = the compact window (1156 wide at 100 %)
    state.setProperty ("keysV2", 1, nullptr);
    state.setProperty ("keysV3", 1, nullptr);
    state.setProperty ("stringsPick", stringsPick.load(), nullptr);
    state.setProperty ("stringsMode", stringsMode.load(), nullptr);
    {
        juce::StringArray pl;
        for (int i = 0; i < numStrings; ++i)
            pl.add (juce::String (plTone[(size_t) i].load(), 3) + ":" + juce::String (plBend[(size_t) i].load(), 3) + ":" + juce::String (plLevel[(size_t) i].load(), 3) + ":" + juce::String (plTime[(size_t) i].load(), 3));
        state.setProperty ("plate", pl.joinIntoString (","), nullptr);
        state.setProperty ("plateMute", (juce::int64) plMute.load(), nullptr);
        juce::StringArray sp;
        for (auto& v : consoleSpare) sp.add (juce::String (v.load(), 3));
        state.setProperty ("consoleSpare", sp.joinIntoString (","), nullptr);
    }
    state.setProperty ("captureMode", captureMode.load(), nullptr);
    state.setProperty ("captureQuant", captureQuant.load(), nullptr);
    state.setProperty ("gateMask", (juce::int64) gateMask.load(), nullptr);
    state.setProperty ("gateLen", gateLen.load(), nullptr);
    state.setProperty ("customPattern", patternString (13), nullptr);
    {
        juce::StringArray seq;
        seq.add (juce::String (seqLen.load()));
        for (int i = 0; i < seqSteps; ++i)
            seq.add (juce::String (seqDeg[(size_t) i].load()) + ":" + juce::String (seqOct[(size_t) i].load()) + ":" + juce::String (seqVel[(size_t) i].load(), 3) + ":"
                     + juce::String (seqRatchet[(size_t) i].load()) + ":" + juce::String (seqProb[(size_t) i].load(), 3) + ":" + juce::String (seqTie[(size_t) i].load()));
        state.setProperty ("arpSteps", seq.joinIntoString (","), nullptr);
        juce::StringArray pr;
        for (auto& sl : progSlots) pr.add (juce::String (sl.load()));
        state.setProperty ("progression", pr.joinIntoString (","), nullptr);
        juce::StringArray gc;
        for (auto& c : gridCells) gc.add (juce::String (c.load()));
        state.setProperty ("motion", gc.joinIntoString (","), nullptr);
        juce::StringArray ln;
        for (auto& v : motionLine) ln.add (juce::String (v.load(), 3));
        state.setProperty ("motionLine", ln.joinIntoString (","), nullptr);
        juce::StringArray sr;
        for (int r = 0; r < prog::gridRows; ++r) sr.add (juce::String (strRate[(size_t) r].load()) + ":" + juce::String (strOffset[(size_t) r].load()) + ":" + juce::String (strLevel[(size_t) r].load(), 2)
                                       + ":" + juce::String (strOct[(size_t) r].load()) + ":" + juce::String (strChance[(size_t) r].load())
                                       + ":" + juce::String (strRatchet[(size_t) r].load()) + ":" + juce::String (strMute[(size_t) r].load() ? 1 : 0));
        state.setProperty ("motionStrings", sr.joinIntoString (","), nullptr);
        state.setProperty ("progOn", progOn.load(), nullptr);
        state.setProperty ("progMode", progMode.load(), nullptr);
        state.setProperty ("progSeed", progSeed.load(), nullptr);
    }
    juce::StringArray mem;
    for (auto& m : memory) mem.add (juce::String (m.load()));
    juce::StringArray lens;
    for (auto& l : memLength) lens.add (juce::String (l.load()));
    state.setProperty ("memory", mem.joinIntoString (","), nullptr);
    state.setProperty ("memLength", lens.joinIntoString (","), nullptr);
    state.setProperty ("memBank", memBank.load(), nullptr);
    const int cur = presetManager->getCurrent();
    state.setProperty ("preset", cur >= 0 ? presetManager->all()[(size_t) cur].key() : juce::String(), nullptr);
    state.setProperty ("presetModified", presetManager->isModified(), nullptr);
    state.setProperty ("skin", skin.load(), nullptr);
    state.setProperty ("zoneOctave", zoneOctave.load(), nullptr);
    state.setProperty ("chordZones", chordZones.load(), nullptr);
    state.setProperty ("zonesVersion", 2, nullptr);
    state.setProperty ("tuning", tuningA4.load(), nullptr);
    state.setProperty ("voices", maxVoices.load(), nullptr);
    juce::StringArray learn;
    for (auto& c : ccMap) learn.add (juce::String (c.load()));
    state.setProperty ("midiLearn", learn.joinIntoString (","), nullptr);
    juce::StringArray locks;
    for (auto& l : presetManager->locked) locks.add (l);
    state.setProperty ("locked", locks.joinIntoString (","), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Hamamatsu108Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            editorWidth.store ((int) state.getProperty ("uiWidth2", 0));
            captureMode.store (juce::jlimit (0, 1, (int) state.getProperty ("captureMode", 1)));
            captureQuant.store (juce::jlimit (0, 4, (int) state.getProperty ("captureQuant", 1)));
            if (state.hasProperty ("gateMask")) gateMask.store ((std::uint32_t) (juce::int64) state.getProperty ("gateMask"));
            gateLen.store (juce::jlimit (1, gateSteps, (int) state.getProperty ("gateLen", 16)));
            {
                const auto cp = state.getProperty ("customPattern").toString();
                if (cp.length() >= 1 && cp.length() <= 16)
                {
                    customLen.store (cp.length());
                    for (int i = 0; i < cp.length(); ++i) customSteps[(size_t) i].store (juce::String ("DUduBHTxS.").containsChar (cp[i]) ? (int) cp[i] : '.');
                }
            }
            {
                const auto pr = juce::StringArray::fromTokens (state.getProperty ("progression").toString(), ",", {});
                for (int i = 0; i < prog::numSlots; ++i) progSlots[(size_t) i].store (i < pr.size() ? juce::jlimit (-1, 100000, pr[i].getIntValue()) : -1);
                progOn.store ((bool) state.getProperty ("progOn", false));
                progMode.store (juce::jlimit (0, prog::numModes - 1, (int) state.getProperty ("progMode", 0)));
                progSeed.store ((int) state.getProperty ("progSeed", 1));
                if (state.hasProperty ("motion"))
                {
                    const auto gc = juce::StringArray::fromTokens (state.getProperty ("motion").toString(), ",", {});
                    for (int i = 0; i < prog::gridSteps; ++i) gridCells[(size_t) i].store (i < gc.size() ? (gc[i].getIntValue() & 255) : 0);
                }
                if (state.hasProperty ("motionStrings"))
                {
                    const auto sr = juce::StringArray::fromTokens (state.getProperty ("motionStrings").toString(), ",", {});
                    for (int r = 0; r < prog::gridRows && r < sr.size(); ++r)
                    {
                        const auto f = juce::StringArray::fromTokens (sr[r], ":", {});
                        strRate[(size_t) r].store (juce::jlimit (0, 9, f[0].getIntValue()));
                        strOffset[(size_t) r].store (juce::jlimit (0, 15, f[1].getIntValue()));
                        strLevel[(size_t) r].store (juce::jlimit (0.0f, 1.0f, f[2].getFloatValue()));
                        strOct[(size_t) r].store (f.size() > 3 ? juce::jlimit (-1, 1, f[3].getIntValue()) : 0);
                        strChance[(size_t) r].store (f.size() > 4 ? juce::jlimit (5, 100, f[4].getIntValue()) : 100);
                        strRatchet[(size_t) r].store (f.size() > 5 ? juce::jlimit (1, 3, f[5].getIntValue()) : 1);
                        strMute[(size_t) r].store (f.size() > 6 && f[6].getIntValue() != 0);
                    }
                }
                if (state.hasProperty ("motionLine"))
                {
                    const auto ln = juce::StringArray::fromTokens (state.getProperty ("motionLine").toString(), ",", {});
                    for (int i = 0; i < prog::linePoints; ++i) motionLine[(size_t) i].store (i < ln.size() ? juce::jlimit (-1.0f, 1.0f, ln[i].getFloatValue()) : -1.0f);
                }
            }
            {
                const auto seq = juce::StringArray::fromTokens (state.getProperty ("arpSteps").toString(), ",", {});
                if (seq.size() == seqSteps + 1)
                {
                    seqLen.store (juce::jlimit (1, seqSteps, seq[0].getIntValue()));
                    for (int i = 0; i < seqSteps; ++i)
                    {
                        const auto f = juce::StringArray::fromTokens (seq[i + 1], ":", {});
                        if (f.size() < 6) continue;
                        seqDeg[(size_t) i].store (juce::jlimit (-1, 7, f[0].getIntValue()));
                        seqOct[(size_t) i].store (juce::jlimit (-1, 2, f[1].getIntValue()));
                        seqVel[(size_t) i].store (juce::jlimit (0.0f, 1.0f, f[2].getFloatValue()));
                        seqRatchet[(size_t) i].store (juce::jlimit (1, 4, f[3].getIntValue()));
                        seqProb[(size_t) i].store (juce::jlimit (0.0f, 1.0f, f[4].getFloatValue()));
                        seqTie[(size_t) i].store (f[5].getIntValue() != 0 ? 1 : 0);
                    }
                }
            }
            const auto mem = juce::StringArray::fromTokens (state.getProperty ("memory").toString(), ",", {});
            const auto lens = juce::StringArray::fromTokens (state.getProperty ("memLength").toString(), ",", {});
            for (size_t i = 0; i < memory.size(); ++i)
                memory[i].store (i < (size_t) mem.size() && chords::isValid (mem[(int) i].getIntValue()) ? mem[(int) i].getIntValue() : -1);
            for (size_t i = 0; i < memLength.size(); ++i)
                memLength[i].store (i < (size_t) lens.size() ? juce::jlimit (0, memSteps, lens[(int) i].getIntValue()) : 0);
            memBank.store (juce::jlimit (0, memBanks - 1, (int) state.getProperty ("memBank", 0)));
            skin.store (14);   // one look: Concrete
            zoneOctave.store (juce::jlimit (0, 3, (int) state.getProperty ("zoneOctave", zoneOctave.load())));
            if ((int) state.getProperty ("zonesVersion", 1) >= 2)   // older projects: zones off (every key plays)
                chordZones.store ((bool) state.getProperty ("chordZones", chordZones.load()));
            tuningA4.store ((float) (double) state.getProperty ("tuning", (double) tuningA4.load()));
            maxVoices.store (juce::jlimit (8, numVoices, (int) state.getProperty ("voices", maxVoices.load())));
            const auto learn = juce::StringArray::fromTokens (state.getProperty ("midiLearn").toString(), ",", {});
            for (size_t i = 0; i < ccMap.size(); ++i) ccMap[i].store (i < (size_t) learn.size() ? learn[(int) i].getIntValue() : -1);
            presetManager->locked.clear();
            for (auto& l : juce::StringArray::fromTokens (state.getProperty ("locked").toString(), ",", {}))
                if (l.isNotEmpty()) presetManager->locked.insert (l);
            const auto presetKey = state.getProperty ("preset").toString();
            const bool presetModified = (bool) state.getProperty ("presetModified", false);
            apvts.replaceState (state);
            stringsPick.store ((int) state.getProperty ("stringsPick", -1));
            stringsMode.store (juce::jlimit (0, 2, (int) state.getProperty ("stringsMode", 0)));
            resetPlate();
            {
                const auto pl = juce::StringArray::fromTokens (state.getProperty ("plate").toString(), ",", {});
                for (int i = 0; i < juce::jmin (numStrings, pl.size()); ++i)
                {
                    const auto v = juce::StringArray::fromTokens (pl[i], ":", {});
                    if (v.size() == 4)
                    {
                        plTone[(size_t) i].store (juce::jlimit (0.0f, 1.0f, v[0].getFloatValue())); plBend[(size_t) i].store (juce::jlimit (0.0f, 1.0f, v[1].getFloatValue()));
                        plLevel[(size_t) i].store (juce::jlimit (0.0f, 1.0f, v[2].getFloatValue())); plTime[(size_t) i].store (juce::jlimit (0.0f, 1.0f, v[3].getFloatValue()));
                    }
                }
                plMute.store ((std::uint32_t) (juce::int64) state.getProperty ("plateMute", 0));
                const auto sp = juce::StringArray::fromTokens (state.getProperty ("consoleSpare").toString(), ",", {});
                for (int i = 0; i < numSpare; ++i) consoleSpare[(size_t) i].store (i < sp.size() ? sp[i].getFloatValue() : -1.0f);
            }
            if (! state.hasProperty ("keysV2"))   // older sessions: keys play single notes now (Split made some keys strum)
                if (auto* kp = apvts.getParameter ("keysPlay")) kp->setValueNotifyingHost (kp->convertTo0to1 (0.0f));
            // sessions before v0.2.76: Flourish off left the keys on Split (low keys strummed); now they play notes
            if (! state.hasProperty ("keysV3") && juce::roundToInt (param (apvts, "keysPlay")) == 6)
                if (auto* kp = apvts.getParameter ("keysPlay")) kp->setValueNotifyingHost (kp->convertTo0to1 (0.0f));
            presetManager->restoreCurrent (presetKey, presetModified);
            undoManager.clearUndoHistory();
        }
}

juce::AudioProcessorEditor* Hamamatsu108Processor::createEditor()
{
    return new Hamamatsu108Editor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Hamamatsu108Processor();
}
