#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

// Kuprell Hamamatsu 108 - step 1 (skeleton).
// The real engine arrives in step 2; for now a soft test chime confirms that MIDI and audio work.
class Hamamatsu108Processor : public juce::AudioProcessor
{
public:
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
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // read by the editor (GUI thread), written on the audio thread
    std::atomic<int> lastNote { -1 };
    std::atomic<int> heldNotes { 0 };
    std::atomic<float> outputLevel { 0.0f };

    // last editor width in pixels (0 = not set yet), saved with the project
    std::atomic<int> editorWidth { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::Synthesiser synth;
    juce::SmoothedValue<float> masterGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Hamamatsu108Processor)
};
