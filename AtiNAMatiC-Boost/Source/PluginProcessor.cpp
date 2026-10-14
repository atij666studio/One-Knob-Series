#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr const char* kBoost  = "boost";
    constexpr const char* kMode   = "mode";
    constexpr const char* kMix    = "mix";
    constexpr const char* kOutput = "output";
}

juce::AudioProcessorValueTreeState::ParameterLayout NAMBoostProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kBoost, 1 }, "Boost",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) {
                return juce::String (juce::roundToInt (v * 100.0f)) + "%";
            })));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { kMode, 1 }, "Character",
        juce::StringArray { "Balanced", "Bright", "Thick" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kMix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) {
                return juce::String (juce::roundToInt (v * 100.0f)) + "%";
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kOutput, 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f));

    return layout;
}

NAMBoostProcessor::NAMBoostProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

void NAMBoostProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int ch = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    dsp.prepare (sampleRate, samplesPerBlock, juce::jmax (1, ch));
    setLatencySamples (dsp.getLatencySamples());
}

bool NAMBoostProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void NAMBoostProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const float boost = apvts.getRawParameterValue (kBoost)->load();
    const int   mode  = (int) apvts.getRawParameterValue (kMode)->load();
    const float mix   = apvts.getRawParameterValue (kMix)->load();
    const float outDb = apvts.getRawParameterValue (kOutput)->load();

    dsp.process (buffer, boost, mode, mix, outDb);
}

juce::AudioProcessorEditor* NAMBoostProcessor::createEditor()
{
    return new NAMBoostEditor (*this);
}

void NAMBoostProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void NAMBoostProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NAMBoostProcessor();
}
