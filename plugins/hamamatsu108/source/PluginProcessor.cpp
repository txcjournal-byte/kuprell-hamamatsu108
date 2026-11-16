#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    float param (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue (id)->load(); }

    double rateInQuarters (int rateIndex)
    {
        static const double r[] = { 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25 };
        return r[juce::jlimit (0, 4, rateIndex)];
    }
}

const juce::StringArray& Hamamatsu108Processor::strumModeNames()
{
    static const juce::StringArray n { "Live", "Arp", "Memo", "Off" };
    return n;
}

const juce::StringArray& Hamamatsu108Processor::arpPatternNames()
{
    static const juce::StringArray n { "Up", "Down", "Up / Down", "Thumb", "Mirror", "Random" };
    return n;
}

const juce::StringArray& Hamamatsu108Processor::arpRegenNames()
{
    static const juce::StringArray n { "Off", "1 bar", "2 bars", "4 bars" };
    return n;
}

const juce::StringArray& Hamamatsu108Processor::memStepNames()
{
    static const juce::StringArray n { "1/2 bar", "1 bar", "2 bars" };
    return n;
}

double Hamamatsu108Processor::memStepQuarters (int index)
{
    static const double q[] = { 2.0, 4.0, 8.0 };
    return q[juce::jlimit (0, 2, index)];
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
    static const juce::StringArray n { "1/2", "1/4", "1/8", "1/8T", "1/16" };
    return n;
}

//==============================================================================
Hamamatsu108Processor::Hamamatsu108Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Hamamatsu108", createParameterLayout())
{
    tones.reserve (64);
    for (auto& m : memory) m.store (-1);
    for (auto& l : memLength) l.store (0);
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

    fl ("master", "Master Volume", -60.0f, 6.0f, -3.0f, [] (float v, int) { return String (v, 1) + " dB"; });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "harpVoice", 1 }, "Harp Voice", harp::voiceNames(), 0));
    fl ("harpTone", "Harp Tone", 0.0f, 1.0f, 0.62f, pct);
    fl ("harpDecay", "Harp Decay", 0.15f, 8.0f, 1.8f, [] (float v, int) { return String (v, 2) + " s"; }, 1.5f);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "harpRange", 1 }, "Harp Range", 1, 4, 3,
                                                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + " oct"; })));

    fl ("vibRate", "Vibrato Rate", 0.5f, 9.0f, 5.2f, [] (float v, int) { return String (v, 1) + " Hz"; });
    fl ("vibDepth", "Vibrato Depth", 0.0f, 1.0f, 0.12f, pct);

    fl ("fxChorus", "Chorus", 0.0f, 1.0f, 0.45f, pct);
    fl ("fxDelay", "Delay", 0.0f, 1.0f, 0.22f, pct);
    fl ("fxReverb", "Reverb", 0.0f, 1.0f, 0.55f, pct);

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "strumMode", 1 }, "Strum Mode", strumModeNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "strumRate", 1 }, "Strum Rate", strumRateNames(), 2));
    fl ("strumFeel", "Strum Feel", 0.0f, 1.0f, 0.62f, pct);
    fl ("strumSpeed", "Strum Speed", 4.0f, 80.0f, 22.0f, [] (float v, int) { return String (roundToInt (v)) + " ms"; }, 20.0f);

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "hold", 1 }, "Hold", true));

    // step 3: arpeggio, dual, chord layer, bass, memory
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "arpPattern", 1 }, "Arp Pattern", arpPatternNames(), 0));
    fl ("arpChance", "Arp Chance", 0.0f, 1.0f, 1.0f, pct);
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
    pitchMul.assign ((size_t) juce::jmax (32, samplesPerBlock), 1.0f);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };
    chorus.prepare (spec);
    chorus.setRate (0.55f);
    chorus.setDepth (0.22f);
    chorus.setCentreDelay (9.0f);
    chorus.setFeedback (0.0f);

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    delayBuffer.setSize (2, (int) (sampleRate * 2.5) + 1);
    delayBuffer.clear();
    delayWrite = 0;

    for (auto* s : { &masterGain, &chorusMix, &delayMix, &reverbMix })
        s->reset (sampleRate, 0.05);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param (apvts, "master")));
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
        return chords::index (lastRoot, type);
    }
    if (holdChord >= 0) return holdChord;
    return latchedChord;
}

void Hamamatsu108Processor::handleMidi (const juce::MidiMessage& m, int samplePos)
{
    if (m.isNoteOn())
    {
        const int n = m.getNoteNumber();
        if (n >= rootZoneLow && n <= rootZoneHigh)
        {
            lastRoot = n - rootZoneLow;
            rootHeldMask |= 1 << lastRoot;
            latchedChord = -1;
        }
        else if (n >= typeZoneLow && n <= typeZoneHigh)
        {
            if (numTypesHeld < 16) typeHeldStack[numTypesHeld++] = n - typeZoneLow;
        }
        else
        {
            // melodic note: plays exactly the written pitch (like any pro instrument)
            const int pitch = n;
            schedulePitch (samplePos, pitch, m.getFloatVelocity());
        }
    }
    else if (m.isNoteOff())
    {
        const int n = m.getNoteNumber();
        if (n >= rootZoneLow && n <= rootZoneHigh)
        {
            const int r = n - rootZoneLow;
            if (rootHeldMask == (1 << r) && param (apvts, "hold") > 0.5f)
                holdChord = resolveChord();   // keep the chord after release
            rootHeldMask &= ~(1 << r);
            if (r == lastRoot && rootHeldMask != 0)
                for (int i = 0; i < 12; ++i)
                    if (rootHeldMask & (1 << i)) { lastRoot = i; break; }
            if (rootHeldMask == 0 && param (apvts, "hold") <= 0.5f)
                holdChord = -1;
        }
        else if (n >= typeZoneLow && n <= typeZoneHigh)
        {
            const int t = n - typeZoneLow;
            for (int i = 0; i < numTypesHeld; ++i)
                if (typeHeldStack[i] == t)
                {
                    for (int j = i; j < numTypesHeld - 1; ++j) typeHeldStack[j] = typeHeldStack[j + 1];
                    --numTypesHeld;
                    break;
                }
        }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        rootHeldMask = 0;
        numTypesHeld = 0;
        holdChord = -1;
        if (m.isAllSoundOff())
            for (auto& v : voices) v.release();
    }
}

void Hamamatsu108Processor::updateTones (int chordIndex, int transpose, int rangeOctaves)
{
    if (chordIndex == tonesChord && transpose == tonesTranspose && rangeOctaves == tonesRange)
        return;

    tonesChord = chordIndex;
    tonesTranspose = transpose;
    tonesRange = rangeOctaves;
    tones.clear();

    if (chordIndex < 0) return;

    const int mask = chords::pitchClassMask (chordIndex, transpose);
    // never below C3 (MIDI 48), so recorded strums never land in the chord zones
    static const int lows[] = { 60, 60, 54, 48, 48 };
    const int low = lows[juce::jlimit (1, 4, rangeOctaves)];
    const int high = low + 12 * rangeOctaves;
    for (int n = low; n <= high; ++n)
        if (mask & (1 << (n % 12)))
            tones.push_back (n);
}

void Hamamatsu108Processor::scheduleNote (int delay, int toneIndex, float velocity)
{
    if (numEvents >= (int) events.size() || tones.empty()) return;
    toneIndex = juce::jlimit (0, (int) tones.size() - 1, toneIndex);
    const int visual = tones.size() > 1 ? juce::roundToInt ((float) toneIndex * (numStrings - 1) / (float) (tones.size() - 1)) : numStrings / 2;
    events[(size_t) numEvents++] = { juce::jmax (0, delay), tones[(size_t) toneIndex], juce::jlimit (0.05f, 1.0f, velocity), visual, true };
}

void Hamamatsu108Processor::schedulePitch (int delay, int note, float velocity)
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
    events[(size_t) numEvents++] = { juce::jmax (0, delay), juce::jlimit (0, 127, note), juce::jlimit (0.05f, 1.0f, velocity), visual, false };
}

// one hand gesture across the strings, with human variation driven by Feel
void Hamamatsu108Processor::scheduleStrum (int offset, bool down)
{
    const int n = (int) tones.size();
    if (n == 0) return;

    const float feel = param (apvts, "strumFeel");
    const float speedMs = param (apvts, "strumSpeed");
    auto rnd = [this] { return rng.nextFloat(); };

    // a loose hand sometimes skips a light up-stroke
    if (! down && rnd() < 0.3f * feel) return;

    const float accent = down ? 0.85f : 0.62f;
    const float baseVel = juce::jlimit (0.15f, 1.0f, accent + (rnd() - 0.5f) * 0.4f * feel);
    const int lead = juce::roundToInt ((rnd() - 0.3f) * feel * 0.012f * (float) sr);

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

    const float stepMs = speedMs * (1.0f + (rnd() - 0.5f) * 0.6f * feel);
    float t = (float) juce::jmax (0, offset + lead);
    for (int i = 0; i < count; ++i)
    {
        const int tone = down ? first + i : first - i;
        if (tone < 0 || tone >= n) break;
        const float v = baseVel * (1.0f - 0.25f * (float) i / (float) count) * (1.0f + (rnd() - 0.5f) * 0.25f * feel);
        scheduleNote (juce::roundToInt (t), tone, v);
        const float jitter = 1.0f + (rnd() - 0.5f) * 0.5f * feel;
        t += stepMs * 0.001f * (float) sr * jitter;
    }
}

void Hamamatsu108Processor::startVoice (int note, float velocity)
{
    startHarp (voiceSettings, note, velocity);
    if (dualOn)
        startHarp (voiceSettings2, note, velocity * 0.75f);
}

void Hamamatsu108Processor::startHarp (const harp::Settings& settings, int note, float velocity)
{
    harp::Voice* target = nullptr;

    for (auto& v : voices)
        if (! v.isActive()) { target = &v; break; }

    if (target == nullptr)   // steal the oldest
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.getAge() > target->getAge()) target = &v;
    }

    const float pan = juce::jlimit (-0.6f, 0.6f, (float) (note - 60) / 30.0f);
    target->start (note, velocity, pan, settings, rng);
}

// one arpeggio step: pattern over the chord tones, Chance thins notes out, Regen re-rolls the variation
void Hamamatsu108Processor::scheduleArpStep (int offset)
{
    const int n = (int) tones.size();
    if (n == 0) return;

    const int pattern = juce::roundToInt (param (apvts, "arpPattern"));
    const float chance = param (apvts, "arpChance");
    const int regen = juce::roundToInt (param (apvts, "arpRegen"));
    const double stepQ = rateInQuarters (juce::roundToInt (param (apvts, "strumRate")));
    static const int regenBars[] = { 1, 1, 2, 4 };
    const int cycle = juce::jmax (1, (int) std::round (regenBars[juce::jlimit (0, 3, regen)] * 4.0 / stepQ));

    const juce::int64 c = arpCounter++;
    if (regen > 0 && c > 0 && c % cycle == 0) ++arpSeed;   // new variation every N bars
    const int inCycle = (int) (c % cycle);

    auto hash = [this] (int i)
    {
        std::uint32_t h = (std::uint32_t) i * 2654435761u ^ (arpSeed * 40503u + 0x9e3779b9u);
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        return (float) (h & 0xffff) / 65535.0f;
    };

    int idx = 0;
    const int span = juce::jmax (1, 2 * n - 2);
    switch (pattern)
    {
        case 0: idx = (int) (c % n); break;                                   // up
        case 1: idx = n - 1 - (int) (c % n); break;                           // down
        case 2: { const int p = (int) (c % span); idx = p < n ? p : span - p; break; }       // up / down
        case 3: idx = (c % 2 == 0) ? 0 : 1 + (int) ((c / 2) % juce::jmax (1, n - 1)); break; // thumb
        case 4: { const int p = (int) (c % (2 * n)); idx = p < n ? p : 2 * n - 1 - p; break; } // mirror (ends repeat)
        default: idx = juce::jlimit (0, n - 1, (int) (hash (inCycle + 7919) * n)); break;    // random
    }

    if (inCycle > 0 && hash (inCycle) > chance) return;   // the first step of a cycle always plays
    const bool onBeat = std::fmod ((double) c * stepQ, 1.0) < 1.0e-6;
    scheduleNote (offset, idx, onBeat ? 0.82f : 0.66f);
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
        for (auto& v : chordVoices) v.release();
        return;
    }

    voicingNotes = chords::voicing (chord, transpose, inv, voicingNotes);
    const auto kind = (layers::Kind) type;   // organ .. choir map 1..4

    // keep common tones ringing, release the rest, start the new ones
    for (auto& v : chordVoices)
        if (v.isActive() && ! v.isReleasing())
            if (typeChanged || std::find (voicingNotes.begin(), voicingNotes.end(), v.getNote()) == voicingNotes.end())
                v.release();

    for (int note : voicingNotes)
    {
        bool held = false;
        for (auto& v : chordVoices)
            if (v.isActive() && ! v.isReleasing() && v.getNote() == note) held = true;
        if (held) continue;
        layers::SustainVoice* target = nullptr;
        for (auto& v : chordVoices) if (! v.isActive()) { target = &v; break; }
        if (target == nullptr) for (auto& v : chordVoices) if (v.isReleasing()) { target = &v; break; }
        if (target != nullptr) target->start (note, 0.8f, kind, rng);
    }
}

void Hamamatsu108Processor::bassBeat (int offset, int beatIndex)
{
    const int chord = bassChord >= 0 ? bassChord : sounding;
    const int c = chord >= 0 ? chord : resolveChord();
    if (c < 0) return;

    const auto& t = chords::types()[(size_t) chords::typeOf (c)];
    const int mode = juce::roundToInt (param (apvts, "bassMode"));
    int interval = 0;
    if (mode == 1)      interval = (beatIndex % 2 == 0) ? 0 : t.intervals[2];                   // root + 5th
    else if (mode == 2) { static const int step[] = { 0, 1, 2, 1 }; interval = t.intervals[(size_t) step[beatIndex % 4]] % 12; }  // walking

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
    bassVoices[(size_t) bassVoiceIndex].release();
    bassVoiceIndex = 1 - bassVoiceIndex;
    bassVoices[(size_t) bassVoiceIndex].start (bassNext, 0.85f, kind, rng);
}

//==============================================================================
void Hamamatsu108Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    if ((int) pitchMul.size() < numSamples) pitchMul.resize ((size_t) numSamples, 1.0f);

    // parameters
    voiceSettings.model = (harp::Model) juce::roundToInt (param (apvts, "harpVoice"));
    voiceSettings.tone = param (apvts, "harpTone");
    voiceSettings.decay = param (apvts, "harpDecay");
    const int range = juce::roundToInt (param (apvts, "harpRange"));
    const int transpose = juce::roundToInt (param (apvts, "transpose"));
    const int mode = juce::roundToInt (param (apvts, "strumMode"));
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

    // dual voice
    dualOn = param (apvts, "dual") > 0.5f;
    voiceSettings2 = voiceSettings;
    voiceSettings2.model = (harp::Model) juce::roundToInt (param (apvts, "harpVoice2"));

    // GUI requests
    // chord buttons: HOLD on = click toggles the chord; HOLD off = chord sounds while the button is held
    const bool holdOn = param (apvts, "hold") > 0.5f;
    if (lastHoldOn && ! holdOn)
    {
        // HOLD switched off: stop whatever was being held
        latchedChord = -1;
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
    }
    deferredRelease = pendingChordRelease.exchange (-1);

    for (const auto meta : midi)
        handleMidi (meta.getMessage(), meta.samplePosition);

    int chord = resolveChord();

    // Chord Memory: record new chords while REC is on
    if (chord >= 0 && chord != memRecordedLast) memRecordChord (chord);
    memRecordedLast = chord;

    // MEMO mode plays the stored sequence (unless recording)
    if (mode != lastMode) { memSamples = 0.0; arpCounter = 0; }
    lastMode = mode;
    const int bank = memBank.load();
    const int memLen = memLength[(size_t) bank].load();
    if (mode == 2 && memLen > 0 && ! memRecord.load())
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

    updateTones (chord, transpose, range);
    activeChord.store (chord);

    if (const auto plucks = pendingPlucks.exchange (0); plucks != 0 && ! tones.empty())
        for (int i = 0; i < numStrings; ++i)
            if (plucks & (1u << (unsigned) i))
                scheduleNote (0, juce::roundToInt ((float) i * (float) (tones.size() - 1) / (float) (numStrings - 1)), 0.8f);

    // Live Strum / Arp clock (MEMO uses the Live Strum hand)
    const bool arp = chord >= 0 && mode == 1;
    const bool strumming = chord >= 0 && (mode == 0 || mode == 2 || mode == 1);
    const double stepQuarters = rateInQuarters (rateIndex);
    const double samplesPerStep = stepQuarters * 60.0 / bpm * sr;
    auto hit = [&] (int offset, juce::int64 k)
    {
        if (arp) scheduleArpStep (offset);
        else scheduleStrum (offset, (k % 2) == 0);
    };

    if (strumming)
    {
        if (chord != sounding || ! wasStrumming)
        {
            if (! arp || ! wasStrumming) arpCounter = 0;
            hit (0, 0);   // instant stroke on a new chord
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

    // chord layer and bass follow the sounding chord
    updateChordLayer (chord, transpose);
    {
        const int newBassType = juce::roundToInt (param (apvts, "bassType"));
        if (newBassType != bassType || chord != bassChord)
        {
            bassType = newBassType;
            bassChord = chord;
            if (chord < 0 || newBassType == 0)
                for (auto& v : bassVoices) v.release();
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

    sounding = chord;
    wasStrumming = strumming;
    samplesSinceTrigger = juce::jmin (samplesSinceTrigger + numSamples, 1 << 29);

    // vibrato
    const float vibRate = param (apvts, "vibRate");
    const float vibDepth = param (apvts, "vibDepth") * 0.45f / 12.0f;   // up to ~0.45 semitone
    const float vibInc = vibRate / (float) sr;
    for (int i = 0; i < numSamples; ++i)
    {
        pitchMul[(size_t) i] = std::exp2 (vibDepth * harp::sine() (vibPhase));
        vibPhase = harp::wrap (vibPhase + vibInc);
    }

    const float chordTone = param (apvts, "chordFilter");
    const float chordGain = juce::Decibels::decibelsToGain (param (apvts, "chordLevel"), -47.9f) * 0.9f;
    const float bassTone = param (apvts, "bassTone");
    const float bassGain = juce::Decibels::decibelsToGain (param (apvts, "bassLevel"), -47.9f) * 0.9f;

    // render in small slices so strum notes start close to their exact time
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    std::array<float, 32> tmpR {};
    std::uint32_t flashes = 0;

    for (int start = 0; start < numSamples; start += 32)
    {
        const int len = juce::jmin (32, numSamples - start);

        for (int e = 0; e < numEvents;)
        {
            if (events[(size_t) e].delay < start + len)
            {
                startVoice (events[(size_t) e].note, events[(size_t) e].velocity);
                if (events[(size_t) e].capture)
                {
                    const auto scope = captureFifo.write (1);
                    const CapturedNote cn { (double) (sampleClock + start) / sr, events[(size_t) e].note, events[(size_t) e].velocity };
                    if (scope.blockSize1 > 0) captureBuffer[(size_t) scope.startIndex1] = cn;
                    else if (scope.blockSize2 > 0) captureBuffer[(size_t) scope.startIndex2] = cn;
                }
                flashes |= 1u << (unsigned) events[(size_t) e].visual;
                events[(size_t) e] = events[(size_t) --numEvents];
            }
            else ++e;
        }

        float* r = right != nullptr ? right + start : tmpR.data();
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

    for (int e = 0; e < numEvents; ++e) events[(size_t) e].delay -= numSamples;
    sampleClock += numSamples;
    hostBpm.store (bpm);
    if (flashes != 0) stringFlashes.fetch_or (flashes);

    int inUse = 0;
    for (auto& v : voices) inUse += v.isActive() ? 1 : 0;
    voicesInUse.store (inUse);

    if (right == nullptr) // mono host: nothing more to spread
        right = left;

    // FX: chorus -> delay -> reverb
    chorusMix.setTargetValue (param (apvts, "fxChorus"));
    delayMix.setTargetValue (param (apvts, "fxDelay"));
    reverbMix.setTargetValue (param (apvts, "fxReverb"));
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (param (apvts, "master")));

    if (buffer.getNumChannels() > 1)
    {
        chorus.setMix (chorusMix.getNextValue() * 0.55f);
        chorusMix.skip (numSamples - 1);
        juce::dsp::AudioBlock<float> block (buffer);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    const int delayLen = delayBuffer.getNumSamples();
    const int delaySamples = juce::jlimit (1, delayLen - 1, (int) (0.75 * 60.0 / bpm * sr));   // dotted 1/8
    auto* dl = delayBuffer.getWritePointer (0);
    auto* dr = delayBuffer.getWritePointer (1);
    for (int i = 0; i < numSamples; ++i)
    {
        const int readPos = (delayWrite - delaySamples + delayLen) % delayLen;
        const float yl = dl[readPos], yr = dr[readPos];
        delayLpL += (yl - delayLpL) * 0.35f;
        delayLpR += (yr - delayLpR) * 0.35f;
        const float inL = left[i], inR = right[i];
        // cross-feedback = gentle ping-pong
        dl[delayWrite] = inL + delayLpR * 0.42f;
        dr[delayWrite] = inR + delayLpL * 0.42f;
        delayWrite = (delayWrite + 1) % delayLen;
        const float mix = delayMix.getNextValue() * 0.5f;
        left[i] = inL + yl * mix;
        if (right != left) right[i] = inR + yr * mix;
    }

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.82f;
    rp.damping = 0.45f;
    rp.width = 1.0f;
    rp.wetLevel = reverbMix.getNextValue() * 0.55f;
    rp.dryLevel = 1.0f - reverbMix.getCurrentValue() * 0.25f;
    reverbMix.skip (numSamples - 1);
    reverb.setParameters (rp);
    if (right != left) reverb.processStereo (left, right, numSamples);
    else reverb.processMono (left, numSamples);

    // master + soft limit + meters
    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = masterGain.getNextValue();
        left[i] = std::tanh (left[i] * g);
        pl = juce::jmax (pl, std::abs (left[i]));
        if (right != left)
        {
            right[i] = std::tanh (right[i] * g);
            pr = juce::jmax (pr, std::abs (right[i]));
        }
    }
    if (right == left) pr = pl;
    meterLeft.store (juce::jmax (pl, meterLeft.load() * 0.85f));
    meterRight.store (juce::jmax (pr, meterRight.load() * 0.85f));
}

//==============================================================================
void Hamamatsu108Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiWidth", editorWidth.load(), nullptr);
    juce::StringArray mem;
    for (auto& m : memory) mem.add (juce::String (m.load()));
    juce::StringArray lens;
    for (auto& l : memLength) lens.add (juce::String (l.load()));
    state.setProperty ("memory", mem.joinIntoString (","), nullptr);
    state.setProperty ("memLength", lens.joinIntoString (","), nullptr);
    state.setProperty ("memBank", memBank.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Hamamatsu108Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            editorWidth.store ((int) state.getProperty ("uiWidth", 0));
            const auto mem = juce::StringArray::fromTokens (state.getProperty ("memory").toString(), ",", {});
            const auto lens = juce::StringArray::fromTokens (state.getProperty ("memLength").toString(), ",", {});
            for (size_t i = 0; i < memory.size(); ++i)
                memory[i].store (i < (size_t) mem.size() ? juce::jlimit (-1, chords::numChords - 1, mem[(int) i].getIntValue()) : -1);
            for (size_t i = 0; i < memLength.size(); ++i)
                memLength[i].store (i < (size_t) lens.size() ? juce::jlimit (0, memSteps, lens[(int) i].getIntValue()) : 0);
            memBank.store (juce::jlimit (0, memBanks - 1, (int) state.getProperty ("memBank", 0)));
            apvts.replaceState (state);
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
