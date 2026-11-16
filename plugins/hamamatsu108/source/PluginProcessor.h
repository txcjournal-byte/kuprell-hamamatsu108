#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include "Chords.h"
#include "HarpVoice.h"
#include "Layers.h"
#include "Effects.h"
#include "Modulation.h"

// Kuprell Hamamatsu 108 - step 2: 108 chords, strum plate, Live Strum, harp voices, basic FX.
class Hamamatsu108Processor : public juce::AudioProcessor
{
public:
    static constexpr int numStrings = 26;       // strings drawn on the strum plate
    static constexpr int numVoices = 64;
    static constexpr int memBanks = 8, memSteps = 16;

    // MIDI zones (fixed for now; editable in Settings later). FL Studio names: C2-B2, C3-G#3, A3 and up.
    static constexpr int rootZoneLow = 24, rootZoneHigh = 35;    // chord root
    static constexpr int typeZoneLow = 36, typeZoneHigh = 44;    // chord type
    // every other note plays the harp at exactly its written pitch

    Hamamatsu108Processor();
    ~Hamamatsu108Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    //== GUI -> audio requests (lock free)
    void clickChord (int chordIndex)   { pendingChordClick.store (chordIndex); }
    void releaseChord (int chordIndex) { pendingChordRelease.store (chordIndex); }   // mouse up (matters when HOLD is off)
    void pluckString (int visualIndex) { pendingPlucks.fetch_or (1u << (unsigned) visualIndex); }

    //== audio -> GUI state
    std::atomic<int> activeChord { -1 };          // chord sounding now (-1 = none)
    std::atomic<std::uint32_t> stringFlashes { 0 }; // strings plucked since the GUI last looked
    std::atomic<float> meterLeft { 0.0f }, meterRight { 0.0f };
    std::atomic<int> voicesInUse { 0 };
    std::atomic<double> hostBpm { 120.0 };

    // notes the plugin creates itself (Live Strum, strum plate), for "drag MIDI to the DAW".
    // Notes coming in from the host are not captured: the DAW already has them.
    struct CapturedNote { double seconds; int note; float velocity; };
    juce::AbstractFifo captureFifo { 2048 };
    std::array<CapturedNote, 2048> captureBuffer {};

    //== Chord Memory: 8 banks x 16 chords (-1 = empty). GUI and audio both edit; ints are atomic.
    std::array<std::atomic<int>, memBanks * memSteps> memory;
    std::array<std::atomic<int>, memBanks> memLength;
    std::atomic<int> memBank { 0 };
    std::atomic<bool> memRecord { false };
    std::atomic<int> memPlayStep { -1 };   // step sounding in MEMO mode (-1 = none)
    int memChord (int bank, int step) const { return memory[(size_t) (bank * memSteps + step)].load(); }
    void memClear (int bank);
    void memDelete (int bank, int step);
    static double memStepQuarters (int index);

    // last editor width in pixels (0 = not set yet), saved with the project
    std::atomic<int> editorWidth { 0 };

    static const juce::StringArray& strumModeNames();
    static const juce::StringArray& strumRateNames();
    static const juce::StringArray& arpPatternNames();
    static const juce::StringArray& modelNames();

    struct ModelSpec { float cutoff, width, hiss, bits; int hold; float drift, sat, tone, decay; };
    static const ModelSpec& modelSpec (int model);
    static int targetIndex (const juce::String& id);

    // current value of each modulation source (for the EDIT page displays)
    std::array<std::atomic<float>, mod::numSources> modSources {};
    static const juce::StringArray& arpRegenNames();
    static const juce::StringArray& memStepNames();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct NoteEvent { int delay; int note; float velocity; int visual; bool capture; };

    void handleMidi (const juce::MidiMessage&, int samplePos);
    void updateTones (int chordIndex, int transpose, int rangeOctaves);
    void scheduleStrum (int offset, bool down);
    void scheduleNote (int delay, int toneIndex, float velocity);
    void schedulePitch (int delay, int note, float velocity);
    void startVoice (int note, float velocity);
    void startHarp (const harp::Settings&, int note, float velocity);
    void scheduleArpStep (int offset);
    void updateChordLayer (int chord, int transpose);
    void bassBeat (int offset, int beatIndex);
    void memRecordChord (int chord);
    void startBassNote();
    float mp (const char* id) const;   // parameter value including modulation
    void updateModulation (int numSamples, bool hostPlaying, double ppq);
    int resolveChord() const;

    harp::Settings voiceSettings, voiceSettings2;
    bool dualOn = false;
    std::array<harp::Voice, numVoices> voices;
    std::array<NoteEvent, 512> events {};
    int numEvents = 0;
    juce::Random rng;
    double sr = 44100.0;

    // chord state (audio thread)
    std::atomic<int> pendingChordClick { -1 }, pendingChordRelease { -1 };
    std::atomic<std::uint32_t> pendingPlucks { 0 };
    int latchedChord = -1, holdChord = -1, deferredRelease = -1;
    int rootHeldMask = 0, lastRoot = -1, typeHeldStack[16] {}, numTypesHeld = 0;
    int sounding = -1;
    std::vector<int> tones;
    int tonesChord = -2, tonesTranspose = 0, tonesRange = 0;

    // strum clock
    double samplesToNextStrum = 0.0;
    juce::int64 strumCount = 0;
    int samplesSinceTrigger = 1 << 30;
    bool wasStrumming = false;
    bool lastHoldOn = true;
    int lastMode = -1;

    // arpeggio
    juce::int64 arpCounter = 0;
    std::uint32_t arpSeed = 1;

    // memory playback (free clock when the host is stopped)
    double memSamples = 0.0;
    int memRecordedLast = -1;

    // chord layer and bass
    std::array<layers::SustainVoice, 8> chordVoices;
    std::array<layers::SustainVoice, 2> bassVoices;
    std::vector<int> voicingNotes;
    int layerChord = -2, layerType = -1, layerTranspose = 0, layerVoicing = -1;
    int bassChord = -2, bassType = -1, bassNext = 0, bassPendingOffset = -1, bassVoiceIndex = 0;
    double bassSamplesToNext = 0.0;
    juce::int64 bassCount = 0;

    // vibrato
    float vibPhase = 0.0f;
    std::vector<float> pitchMul;

    // model, FX, modulation
    juce::AudioBuffer<float> work;
    std::array<fx::FxUnit, fx::numSlots> fxUnits;
    juce::Reverb dreamReverb;
    juce::SmoothedValue<float> masterGain;
    float mlp1L = 0, mlp2L = 0, mlp1R = 0, mlp2R = 0, heldL = 0, heldR = 0;
    int holdCount = 0;
    float wowPhase = 0, flutterPhase = 0, wobblePhase = 0, morphNow = 0.35f;
    std::array<mod::Lfo, 2> lfos;
    mod::Function func;
    mod::Random randomSrc;
    std::array<float, 32> modOffset {};
    float lastVelocity = 0.8f, modWheel = 0.0f, aftertouch = 0.0f;
    double bpm = 120.0;
    juce::int64 sampleClock = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Hamamatsu108Processor)
};
