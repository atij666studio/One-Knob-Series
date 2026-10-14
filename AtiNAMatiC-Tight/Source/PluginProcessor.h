#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "TightDSP.h"

class NAMTightProcessor : public juce::AudioProcessor
{
public:
    NAMTightProcessor();
    ~NAMTightProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                  { return true; }

    const juce::String getName() const override      { return "AtiNAMatiC-Tight"; }
    bool acceptsMidi()  const override               { return false; }
    bool producesMidi() const override               { return false; }
    bool isMidiEffect() const override               { return false; }
    double getTailLengthSeconds() const override     { return 0.0; }

    int getNumPrograms() override                    { return 1; }
    int getCurrentProgram() override                 { return 0; }
    void setCurrentProgram (int) override            {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    nt::TightDSP dsp;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Input / output peak meters (dBFS, ~60 ms ballistics) for the editor.
    std::atomic<float> inMeterDb { -90.0f }, outMeterDb { -90.0f };

private:
    float inEnv { 0.0f }, outEnv { 0.0f };
    void updateMeters (const juce::AudioBuffer<float>& b, int numCh, bool input);

    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float> mixSm, outSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMTightProcessor)
};
