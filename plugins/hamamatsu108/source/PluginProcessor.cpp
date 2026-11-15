#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct TestSound : public juce::SynthesiserSound
    {
        bool appliesToNote (int) override    { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    // Simple chime (sine + octave partial, fast attack, ringing decay). Only for testing step 1.
    class TestVoice : public juce::SynthesiserVoice
    {
    public:
        bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<TestSound*> (s) != nullptr; }

        void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
        {
            phase = 0.0;
            phase2 = 0.0;
            frequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);
            level = 0.16f * velocity;
            envelope.setSampleRate (getSampleRate());
            envelope.setParameters ({ 0.002f, 1.4f, 0.0f, 0.5f });
            envelope.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
                envelope.noteOff();
            else
            {
                envelope.reset();
                clearCurrentNote();
            }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
        {
            if (! isVoiceActive())
                return;

            const double twoPi = juce::MathConstants<double>::twoPi;
            const double increment = twoPi * frequency / getSampleRate();

            for (int i = 0; i < numSamples; ++i)
            {
                const float env = envelope.getNextSample();
                const float sample = (float) (std::sin (phase) + 0.35 * std::sin (phase2)) * level * env;

                phase += increment;
                phase2 += increment * 2.0;
                if (phase >= twoPi)  phase -= twoPi;
                if (phase2 >= twoPi) phase2 -= twoPi;

                for (int ch = 0; ch < output.getNumChannels(); ++ch)
                    output.addSample (ch, startSample + i, sample);

                if (! envelope.isActive())
                {
                    clearCurrentNote();
                    break;
                }
            }
        }

    private:
        juce::ADSR envelope;
        double phase = 0.0, phase2 = 0.0, frequency = 440.0;
        float level = 0.1f;
    };
}

//==============================================================================
Hamamatsu108Processor::Hamamatsu108Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Hamamatsu108", createParameterLayout())
{
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new TestVoice());

    synth.addSound (new TestSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout Hamamatsu108Processor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "master", 1 }, "Master Volume",
        juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f), -6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

void Hamamatsu108Processor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    masterGain.reset (sampleRate, 0.05);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("master")->load()));
}

bool Hamamatsu108Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void Hamamatsu108Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            lastNote.store (message.getNoteNumber());
            heldNotes.store (heldNotes.load() + 1);
        }
        else if (message.isNoteOff())
        {
            heldNotes.store (juce::jmax (0, heldNotes.load() - 1));
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            heldNotes.store (0);
        }
    }

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("master")->load()));
    masterGain.applyGain (buffer, buffer.getNumSamples());

    // peak level for the editor's meter
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    outputLevel.store (juce::jmax (peak, outputLevel.load() * 0.9f));
}

void Hamamatsu108Processor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void Hamamatsu108Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* Hamamatsu108Processor::createEditor()
{
    return new Hamamatsu108Editor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Hamamatsu108Processor();
}
