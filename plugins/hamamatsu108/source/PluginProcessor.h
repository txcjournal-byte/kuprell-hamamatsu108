#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include "Chords.h"
#include "HarpVoice.h"

// Kuprell Hamamatsu 108 - step 2: 108 chords, strum plate, Live Strum, harp voices, basic FX.
class Hamamatsu108Processor : public juce::AudioProcessor
{
public:
    static constexpr int numStrings = 26;       // strings drawn on the strum plate
    static constexpr int numVoices = 48;

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

    // last editor width in pixels (0 = not set yet), saved with the project
    std::atomic<int> editorWidth { 0 };

    static const juce::StringArray& strumModeNames();
    static const juce::StringArray& strumRateNames();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct NoteEvent { int delay; int note; float velocity; int visual; bool capture; };

    void handleMidi (const juce::MidiMessage&, int samplePos);
    void updateTones (int chordIndex, int transpose, int rangeOctaves);
    void scheduleStrum (int offset, bool down);
    void scheduleNote (int delay, int toneIndex, float velocity);
    void schedulePitch (int delay, int note, float velocity);
    void startVoice (int note, float velocity);
    int resolveChord() const;

    harp::Settings voiceSettings;
    std::array<harp::Voice, numVoices> voices;
    std::array<NoteEvent, 512> events {};
    int numEvents = 0;
    juce::Random rng;
    double sr = 44100.0;

    // chord state (audio thread)
    std::atomic<int> pendingChordClick { -1 };
    std::atomic<std::uint32_t> pendingPlucks { 0 };
    int latchedChord = -1, holdChord = -1;
    int rootHeldMask = 0, lastRoot = -1, typeHeldStack[16] {}, numTypesHeld = 0;
    int sounding = -1;
    std::vector<int> tones;
    int tonesChord = -2, tonesTranspose = 0, tonesRange = 0;

    // strum clock
    double samplesToNextStrum = 0.0;
    juce::int64 strumCount = 0;
    int samplesSinceTrigger = 1 << 30;
    bool wasStrumming = false;

    // vibrato
    float vibPhase = 0.0f;
    std::vector<float> pitchMul;

    // FX
    juce::dsp::Chorus<float> chorus;
    juce::Reverb reverb;
    juce::AudioBuffer<float> delayBuffer;
    int delayWrite = 0;
    float delayLpL = 0.0f, delayLpR = 0.0f;
    juce::SmoothedValue<float> masterGain, chorusMix, delayMix, reverbMix;
    double bpm = 120.0;
    juce::int64 sampleClock = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Hamamatsu108Processor)
};
