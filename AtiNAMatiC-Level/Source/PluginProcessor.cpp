#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <OneKnobParams.h>

namespace
{
    constexpr const char* kLevel  = "level";
    constexpr const char* kAuto   = "autoOn";
    constexpr const char* kTrim   = "trim";
}

juce::AudioProcessorValueTreeState::ParameterLayout NAMLevelProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kLevel, 1 }, "Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.50f,
        ok::percent()));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { kAuto, 1 }, "Auto", true));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kTrim, 1 }, "Trim",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, ok::decibels()));

    return layout;
}

NAMLevelProcessor::NAMLevelProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

void NAMLevelProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int ch = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    dsp.prepare (sampleRate, samplesPerBlock, juce::jmax (1, ch));
    setLatencySamples (0);
}

bool NAMLevelProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void NAMLevelProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const float macro  = apvts.getRawParameterValue (kLevel)->load();
    const bool  aut    = apvts.getRawParameterValue (kAuto) ->load() > 0.5f;
    const float trimDb = apvts.getRawParameterValue (kTrim) ->load();

    dsp.process (buffer, macro, aut, trimDb);
}

juce::AudioProcessorEditor* NAMLevelProcessor::createEditor()
{
    return new NAMLevelEditor (*this);
}

void NAMLevelProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void NAMLevelProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NAMLevelProcessor();
}
