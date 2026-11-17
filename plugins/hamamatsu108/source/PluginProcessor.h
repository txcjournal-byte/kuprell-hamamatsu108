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
#include "Presets.h"

// Kuprell Hamamatsu 108 - step 2: 108 chords, strum plate, Live Strum, harp voices, basic FX.
class Hamamatsu108Processor : public juce::AudioProcessor, private juce::Timer
{
public:
    static constexpr int numStrings = 26;       // strings drawn on the strum plate
    static constexpr int numVoices = 64;
    static constexpr int memBanks = 8, memSteps = 16;

    // MIDI zones (fixed for now; editable in Settings later). FL Studio names: C2-B2, C3-G#3, A3 and up.
    // chord zones start at an adjustable octave (Settings); default C1 = MIDI 24 (FL: C2)
    std::atomic<int> zoneOctave { 1 };      // 0..3 -> root zone starts at C0..C3
    std::atomic<bool> chordZones { true };  // off: every key plays the harp
    int rootZoneLow() const  { return 12 * (zoneOctave.load() + 1); }
    int rootZoneHigh() const { return rootZoneLow() + 11; }
    int typeZoneLow() const  { return rootZoneLow() + 12; }
    int typeZoneHigh() const { return rootZoneLow() + 20; }

    //== Settings (saved with the project and as the user's defaults)
    std::atomic<int> skin { 0 };
    std::atomic<float> tuningA4 { 440.0f };
    std::atomic<int> maxVoices { 64 };
    std::atomic<bool> mtsEnabled { true };
    float noteHz (int midiNote) const;
    bool mtsConnected() const;
    void saveUserDefaults() const;
    void loadUserDefaults();

    //== MIDI Learn: CC -> parameter
    std::array<std::atomic<int>, 128> ccMap;        // index into getParameters(), -1 = none
    std::atomic<int> learnParam { -1 };             // parameter waiting for a CC
    juce::String ccName (int cc) const;
    int ccForParam (int paramIndex) const;
    // every other note plays the harp at exactly its written pitch

    Hamamatsu108Processor();
    ~Hamamatsu108Processor() override;

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

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<presets::Manager> presetManager;

    //== GUI -> audio requests (lock free)
    void clickChord (int chordIndex)   { pendingChordClick.store (chordIndex); }
    void latchChord (int chordIndex)   { pendingChordLatch.store (chordIndex); }   // chord buttons by mouse: click on, click again off
    void releaseChord (int chordIndex) { pendingChordRelease.store (chordIndex); }   // mouse up (matters when HOLD is off)
    void previewPreset() { previewRequest.store (true); }                            // short demo strum after a preset click
    void pluckString (int visualIndex) { pendingPlucks.fetch_or (1u << (unsigned) visualIndex); }

    //== audio -> GUI state
    std::atomic<int> activeChord { -1 };
    std::atomic<bool> modActive { false };         // modulators run only while something sounds
    std::atomic<float> funcPhase { 0.0f };         // Function playhead 0..1
    std::array<std::atomic<float>, 2> lfoPhase {};  // LFO playheads 0..1
    void funcShape (std::array<float, mod::funcPoints>& xs, std::array<float, mod::funcPoints>& ys) const;          // chord sounding now (-1 = none)
    std::atomic<std::uint32_t> stringFlashes { 0 }; // strings plucked since the GUI last looked
    std::atomic<float> meterLeft { 0.0f }, meterRight { 0.0f };
    std::atomic<int> voicesInUse { 0 };
    std::atomic<double> hostBpm { 120.0 };

    // notes the plugin creates itself (Live Strum, strum plate), for "drag MIDI to the DAW".
    // Notes coming in from the host are not captured: the DAW already has them.
    struct CapturedNote { double seconds; int note; float velocity; int layer = 0; bool on = true; };   // layer: 0 harp, 1 chord layer, 2 bass, 3 chord blocks
    std::atomic<int> captureMode { 2 };    // what a dragged take contains: 0 detail (every plucked note), 1 chords (long notes stacked), 2 both
    std::atomic<int> captureQuant { 1 };   // chord blocks snap to: 0 off, 1 1/16, 2 1/8, 3 1/4, 4 bar
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
    static const juce::StringArray& strumPatternNames();
    static const juce::StringArray& keysPlayNames();
    static const char* strumPatternSteps (int index);   // one bar, 1/16 per step (see stroke())
    static const juce::StringArray& strumRateNames();
    static const juce::StringArray& arpPatternNames();
    static const juce::StringArray& modelNames();

    struct ModelSpec { float cutoff, width, hiss, bits; int hold; float drift, sat, tone, decay, chorus;
                       float boardSize, boardDecay, boardDark, boardMix; };   // the instrument's body
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
    void scheduleStrum (int offset, bool down, float velScale = 1.0f, float speedMul = 1.0f);
    void sweep (int offset, bool down, int first, int count, float vel, float speedMul);
    void stroke (int offset, char type, float accent);
    void patternStep (int offset, juce::int64 k);
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
    std::atomic<int> pendingChordClick { -1 }, pendingChordRelease { -1 }, pendingChordLatch { -1 };
    std::atomic<bool> previewRequest { false };
    int previewSamplesLeft = 0;
    bool previewStroke = false, previewMute = false;   // preview = one stroke, never recorded
    void capture (double seconds, int note, float velocity, int layer, bool on);
    std::atomic<std::uint32_t> pendingPlucks { 0 };
    int latchedChord = -1, holdChord = -1, deferredRelease = -1;
    int rootHeldMask = 0, lastRoot = -1, typeHeldStack[16] {}, numTypesHeld = 0;
    int sounding = -1;
    std::vector<int> tones;
    std::array<bool, 128> heldMelodic {};   // MIDI notes held outside the chord zones (ARP plays them)
    int heldMelodicCount = 0;
    bool eventsFromHost = false;      // ARP over piano-roll notes: never captured
    bool layersFromHost = false;
    int layerGateLeft = 0;
    bool hostDriven = false;
    std::array<std::array<bool, 128>, 4> capturedOn {};   // note-offs are recorded only for recorded note-ons          // the piano roll is playing the plugin: MIDI capture records nothing            // chord layer + bass sound while the harp is being played, then release      // chord layer / bass following piano-roll notes: never captured
    int chordFromHeldNotes();   // chord that fits the held MIDI notes (for chord layer and bass)
    int tonesChord = -2, tonesTranspose = 0, tonesRange = 0;

    // strum clock
    double samplesToNextStrum = 0.0;
    juce::int64 strumCount = 0;
    int samplesSinceTrigger = 1 << 30;
    bool wasStrumming = false;
    bool lastHoldOn = true;
    int lastMode = -1;

    // strum patterns and KEYS play
    int bassAlt = 0;
    bool sweepDown = true;
    int keyStrumAt = -1;              // KEYS Strum: samples until the collected keys are strummed
    float keyStrumVel = 0.0f;
    std::array<bool, 128> keyNotes {}; // keys pressed since the stroke was armed
    int lastStrumId = -1;              // chord or held-key set the hand is playing (a change = instant stroke)
    std::vector<int> blockNotes, prevBlock;   // chord block for the MIDI capture
    int blockId = -1;

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
    hama::FdnReverb dreamReverb;
    std::vector<float> dreamL, dreamR;
    juce::SmoothedValue<float> masterGain;
    float mlp1L = 0, mlp2L = 0, mlp1R = 0, mlp2R = 0, heldL = 0, heldR = 0, crushEnv = 0, hissEnv = 0, limGain = 1.0f, chorusPhase = 0;
    fx::DelayLine modelChorusL, modelChorusR;
    float dcInL = 0, dcOutL = 0, dcInR = 0, dcOutR = 0;
    // look-ahead limiter: the gain is lowered smoothly before a peak arrives (no crackle)
    static constexpr int limLookMax = 256;
    std::array<float, limLookMax> limDelL {}, limDelR {}, limNeed {};
    int limLook = 64, limPos = 0;
    // de-click when the sound is switched (preset, model, voice, FX type): glide from the last sample for 3 ms
    std::uint64_t soundSignature = 0;
    hama::Soundboard soundboard;
    int boardModel = -1;
    int declickLeft = 0, declickLen = 132;
    float lastOutL = 0.0f, lastOutR = 0.0f;
    int holdCount = 0;
    float wowPhase = 0, flutterPhase = 0, wobblePhase = 0, morphNow = 0.35f;
    std::array<mod::Lfo, 2> lfos;
    mod::Function func;
    mod::Random randomSrc;
    std::array<float, 32> modOffset {};
    std::array<bool, 32> modRouted {};
    // harp bus modulation (filter / volume / pan / pitch), smoothed per sample
    float busCut = 1.0f, busVol = 1.0f, busPan = 0.0f, busPitch = 0.0f;
    float busIc1L = 0, busIc2L = 0, busIc1R = 0, busIc2R = 0;
    float lastVelocity = 0.8f, modWheel = 0.0f, aftertouch = 0.0f;

    // MIDI learn hand-over from the audio thread
    std::array<std::atomic<float>, 128> ccPending;
    std::atomic<std::uint64_t> ccDirtyLo { 0 }, ccDirtyHi { 0 };
    void timerCallback() override;
    void* mtsClient = nullptr;
    double bpm = 120.0;
    juce::int64 sampleClock = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Hamamatsu108Processor)
};
