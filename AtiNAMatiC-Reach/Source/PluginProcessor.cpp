#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <OneKnobParams.h>

namespace
{
    constexpr const char* kMacro  = "reach";
    constexpr const char* kMode   = "mode";
    constexpr const char* kMix    = "mix";
    constexpr const char* kOutput = "output";
}

juce::AudioProcessorValueTreeState::ParameterLayout AtiNAMatiCReachProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kMacro, 1 }, "Reach",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f, ok::percent()));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { kMode, 1 }, "Shape",
        juce::StringArray { "Focus", "Wide", "Huge" }, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kMix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, ok::percent()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kOutput, 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f, ok::decibels()));

    return layout;
}

AtiNAMatiCReachProcessor::AtiNAMatiCReachProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

void AtiNAMatiCReachProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int ch = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    dsp.prepare (sampleRate, samplesPerBlock, juce::jmax (1, ch));
    setLatencySamples (0);
}

bool AtiNAMatiCReachProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    // Mono in -> stereo out is supported (it builds a stereo image).
    if (out == juce::AudioChannelSet::stereo())
        return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return in == out && in == juce::AudioChannelSet::mono();
}

void AtiNAMatiCReachProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;

    // Mono in, stereo out: start the right channel as a copy of the left.
    if (getTotalNumInputChannels() == 1 && getTotalNumOutputChannels() >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    dsp.process (buffer,
                 apvts.getRawParameterValue (kMacro)->load(),
                 (int) apvts.getRawParameterValue (kMode)->load(),
                 apvts.getRawParameterValue (kMix)->load(),
                 apvts.getRawParameterValue (kOutput)->load());
}

juce::AudioProcessorEditor* AtiNAMatiCReachProcessor::createEditor()
{
    return new AtiNAMatiCReachEditor (*this);
}

void AtiNAMatiCReachProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void AtiNAMatiCReachProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AtiNAMatiCReachProcessor();
}
