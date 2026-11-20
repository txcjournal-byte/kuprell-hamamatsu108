#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include "Chords.h"
#include "HarpVoice.h"
#include "Layers.h"
#include "Progression.h"
#include "Effects.h"
#include "Modulation.h"
#include "Presets.h"

// Kuprell Hamamatsu 144 - step 2: 108 chords, strum plate, Live Strum, harp voices, basic FX.
class Hamamatsu108Processor : public juce::AudioProcessor, private juce::Timer
{
public:
    static constexpr int numStrings = 26;       // strings drawn on the strum plate
    static constexpr int numVoices = 64;
    static constexpr int memBanks = 8, memSteps = 16;

    // MIDI zones (fixed for now; editable in Settings later). FL Studio names: C2-B2, C3-G#3, A3 and up.
    // chord zones start at an adjustable octave (Settings); default C1 = MIDI 24 (FL: C2)
    std::atomic<int> zoneOctave { 1 };      // 0..3 -> root zone starts at C0..C3
    std::atomic<bool> keyswitches { false };   // C0-B0 pick the LIVE pattern, C-1..D#-1 the mode (below the chord zones)
    std::atomic<bool> pcKeys { false };
    std::atomic<bool> midiOut { false };       // send the harp (ch 1), chord layer (ch 2) and bass (ch 3) as MIDI
    //== custom strum pattern (pattern "Custom"): one stroke character per step, see strumPatternSteps()
    std::array<std::atomic<int>, 16> customSteps;
    std::atomic<int> customLen { 16 };
    juce::String patternString (int index) const;       // the computer keyboard plays chords / strings while the plugin window has focus
    std::atomic<int> pendingPattern { -1 }, pendingMode { -1 };   // keyswitch requests (applied on the message thread)
    //== rhythmic gate (32 steps): chops the harp or everything in time
    static constexpr int gateSteps = 32;
    std::atomic<std::uint32_t> gateMask { 0xDDDDDDDDu };
    std::atomic<int> gateLen { 16 };
    std::atomic<int> gateStepGui { -1 };
    static const juce::StringArray& gatePresetNames();
    void gateLoadPreset (int index);
    std::atomic<bool> chordZones { false };  // off: every key plays the harp
    int rootZoneLow() const  { return 12 * (zoneOctave.load() + 1); }
    int rootZoneHigh() const { return rootZoneLow() + 11; }
    int typeZoneLow() const  { return rootZoneLow() + 12; }
    int typeZoneHigh() const { return rootZoneLow() + 20; }

    //== Settings (saved with the project and as the user's defaults)
    std::atomic<int> skin { 14 };   // Concrete
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
    bool producesMidi() const override { return true; }   // MIDI OUT: what the plugin plays can drive any other instrument
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
    void releaseChord (int chordIndex) { pendingChordRelease.store (chordIndex); }
    void addChord (int chordIndex)     { pendingChordAdd.store (chordIndex); }       // Ctrl / Shift + click: stack chords
    std::array<std::atomic<int>, 4> stackGui { -1, -1, -1, -1 };                         // chords of the stack (GUI highlight)   // mouse up (matters when HOLD is off)
    void previewPreset() { previewRequest.store (true); }                            // short demo strum after a preset click
    void pluckString (int visualIndex) { pendingPlucks.fetch_or (1u << (unsigned) visualIndex); }
    void strumNow (bool down) { pendingStrum.store (down ? 1 : -1); }   // PC keyboard: a whole stroke
    // MOTION is on and plays a pattern (GRID / LINES / RHYTHM / STRINGS, not PLATE)
    bool motionActive() const { return *apvts.getRawParameterValue ("gridOn") > 0.5f && juce::roundToInt (apvts.getRawParameterValue ("motionType")->load()) != 4; }
    // hear the chord once: one bar of the MOTION pattern if MOTION is on, otherwise one stroke
    // the chord on the strings right now, and the player's own pick of notes for them (-1 = follow the keys)
    std::atomic<int> stringsNow { -1 }, stringsPick { -1 };
    bool inStroke = false;   // sweep() is shaping a stroke (the STROKE switches apply once)
    double lastBpm = 120.0;
    void audition() { if (motionActive()) { auditionBar.store (true); } else pendingStrum.store (1); }
    std::atomic<bool> auditionBar { false };

    //== audio -> GUI state
    std::atomic<int> activeChord { -1 };
    std::atomic<int> detectedChord { -1 };         // chord recognised from the notes played (piano roll / keys)
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
    std::atomic<int> captureMode { 1 };    // what a dragged take contains: 0 detail (every plucked note), 1 smooth (no tiny notes)
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
    struct Progression { const char* name; std::vector<std::pair<int, int>> steps; };
    static const std::vector<Progression>& progressions();
    void memLoad (int bank, int progressionIndex, int keyRoot);   // fill a bank with a ready-made progression in the key
    void memSet (int bank, int step, int chord);
    static double memStepQuarters (int index);

    // last editor width in pixels (0 = not set yet), saved with the project
    std::atomic<int> editorWidth { 0 };

    static const juce::StringArray& strumModeNames();
    static const juce::StringArray& strumPatternNames();
    static const juce::StringArray& keysPlayNames();
    static const juce::StringArray& strumZoneNames();
    // SWITCHES on the main page
    static constexpr int numSwitches = 24;
    enum { swSparkle, swSub, swSwell, swFreeze, swDrive, swCrush, swFilter, swTape, swDream, swWobble, swWide, swDelay, swReverb, swShimmer, swHalftime, swHuman,
           swChorus, swPhaser, swFlanger, swRadio, swFifth, swDetune, swStutter, swVibrato };
    static const juce::StringArray& switchNames();
    static const juce::StringArray& switchHelp();
    float sw (int i) const;   // amount of a switch, 0 when it is off
    static const juce::StringArray& strumGestureNames();
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

    //== ARP step sequencer (16 steps, saved with the project). Velocity, chance, ratchet and tie shape every
    // arp pattern; NOTE and OCT are used by the "Steps" pattern (note = chord tone 1..8, -1 = rest).
    static constexpr int seqSteps = 16;
    std::atomic<int> seqLen { 16 };
    std::array<std::atomic<int>, seqSteps> seqDeg, seqOct, seqRatchet, seqTie;
    std::array<std::atomic<float>, seqSteps> seqVel, seqProb;
    std::atomic<int> arpStepGui { -1 };   // step playing now (GUI)
    static const juce::StringArray& seqPresetNames();
    void seqLoadPreset (int index);
    void seqRandomise();
    static const juce::StringArray& memStepNames();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct NoteEvent { int delay; int note; float velocity; int visual; bool capture; };

    void handleMidi (const juce::MidiMessage&, int samplePos);
    void updateTones (int chordIndex, int transpose, int rangeOctaves);
    void scheduleStrum (int offset, bool down, float velScale = 1.0f, float speedMul = 1.0f);
    void sweep (int offset, bool down, int first, int count, float vel, float speedMul);
    void stroke (int offset, char type, float accent);
    void midiNote (int samplePos, int channel, int note, float velocity, bool on);
    void flourish (int samplePos, int key, float velocity, bool up);
public:
    int flourishType (int key) const;   // FLOURISH: chord type of a key (by octave)
private:
    int flourishChord = -1;
    juce::MidiBuffer midiOutBuffer;
    struct PendingOff { int samples; int channel; int note; };
    std::array<PendingOff, 256> pendingOffs {};
    int numPendingOffs = 0;
    std::array<std::array<std::uint8_t, 128>, 3> outOn {};
    int blockSamples = 0;
    juce::int64 patOrigin = 0;
    void patternStep (int offset, juce::int64 k);
    void scheduleNote (int delay, int toneIndex, float velocity);
    void schedulePitch (int delay, int note, float velocity, bool record = false);
    std::array<int, 32> pressOrder {};   // held notes in the order they were pressed (ARP "As Played")
    int pressCount = 0;
    float bendPos = -1.0f;              // pitch wheel strum: last position on the strings (0..1, -1 = not moved yet)
    float wheelPos = -1.0f;
public:
    // PROGRESSION: up to 8 chords, one bar each; plays with the song and drags out as MIDI
    std::array<std::atomic<int>, prog::numSlots> progSlots;
    std::atomic<bool> progOn { false };
    std::atomic<int> progMode { prog::strum }, progSeed { 1 }, progEdit { -1 }, progSlotGui { -1 };
    prog::Settings progSettings();
    // MOTION grid: 8 rows (strings of the chord) x 16 steps, bit r of a step = row r lit
    std::array<std::atomic<int>, prog::gridSteps> gridCells;
    std::atomic<bool> gridPlay { false };          // PLAY: loop the chord without holding keys
    std::array<std::atomic<float>, prog::linePoints> motionLine;   // LINES: the drawn path (-1 = lifted)
    std::array<std::atomic<int>, prog::gridRows> strRate, strOffset;   // STRINGS: each string its own pulse
    std::array<std::atomic<float>, prog::gridRows> strLevel;
    std::array<std::atomic<int>, prog::gridRows> strOct, strChance, strRatchet;   // STRINGS: octave, chance %, quick repeats
    std::array<std::atomic<bool>, prog::gridRows> strMute;
    std::atomic<int> gridStepGui { -1 };
    void setGridPreset (int i) { const auto& g = prog::gridPresets()[(size_t) juce::jlimit (0, (int) prog::gridPresets().size() - 1, i)]; for (int k = 0; k < prog::gridSteps; ++k) gridCells[(size_t) k].store (g.cells[(size_t) k]); }
private:
    std::vector<prog::Note> progNotes;   // the bar being played
    int progBarCached = -1000;
    bool lastProgPlaying = false, progWasPlaying = false;
    int progBarOffset = 0, progLastHostBar = 0;
    double progOwnPpq = 0.0;     // PLAY without FL: the plugin's own clock
    double gridOwnPpq = 0.0;     // the grid's clock when FL is stopped (starts with the chord)
    int gridLastChord = -1, gridBarCached = -1000, gridCachedChord = -1;
    std::vector<prog::Note> gridNotes;
    bool progOnHost = false;
    layers::Svf masterFiltL, masterFiltR;   // FILTER on the whole instrument
    float cutSmooth = 20000.0f;             // mod wheel strum: the same for CC 1
    void wheelStrum (int samplePos, float pos, float& last);
    // SPLIT keys: below C4 the left hand picks the chord, from C4 up the white keys are the strings
    static constexpr int splitPoint = 60;
    int splitChord = -1, splitHeld = 0;
    std::array<bool, 128> splitLeft {};
    struct SplitHit { int pos; int string; float vel; int strum; };   // strum: 0 = one string, 1 = down, -1 = up
    std::array<SplitHit, 64> splitHits {};
    int numSplitHits = 0;
    int chordFromMask (int mask, int lowest);
    double gateClock = 0.0;              // free-running gate position (host stopped), in steps
    float gateGain = 1.0f;
    std::vector<float> gateBuf;
    struct PendingRelease { int delay; int note; float secs = 0.04f; };
    std::array<PendingRelease, 64> releases {};
    int numReleases = 0;
    void startVoice (int note, float velocity);
    void startHarp (const harp::Settings&, int note, float velocity, bool layerB = false);
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
    std::atomic<int> pendingChordClick { -1 }, pendingChordRelease { -1 }, pendingChordLatch { -1 }, pendingChordAdd { -1 };
    std::vector<int> stackParts;
    std::atomic<bool> previewRequest { false };
    int previewSamplesLeft = 0;
    bool previewStroke = false, previewMute = false;   // preview = one stroke, never recorded
    void capture (double seconds, int note, float velocity, int layer, bool on);
    std::atomic<std::uint32_t> pendingPlucks { 0 };
    std::atomic<int> pendingStrum { 0 };
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
    std::array<fx::FxUnit, 9> switchFx;   // CRUSH, DELAY, REVERB, SHIMMER, CHORUS, PHASE, FLANGE, RADIO, VIBRATO switches
    std::array<bool, 9> switchFxActive {};
    juce::Random humanRng;
    void startSwitchVoices (int note, float velocity);
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
    int motionKeysHeld = 0, motionOneShot = 0, tremoloLeft = 0;
    bool motionRestart = false, tremoloHeld = false, tremoloUp = true, dampNow = false;
    float tremoloVel = 0.8f;
    bool sideToSideUp = true;
    int lastHeldChord = -1;
    float widthNow = 1.0f, widthTarget = 1.0f, sideLow = 0.0f, sideLowCoef = 0.02f;
    float polishNow = 0.4f, compEnv = 0.0f, compAtt = 0.01f, compRel = 0.001f, airLpL = 0.0f, airLpR = 0.0f, airCoef = 0.5f;
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
