#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr const char* kWidth  = "width";
    constexpr const char* kMode   = "mode";
    constexpr const char* kMix    = "mix";
    constexpr const char* kOutput = "output";
}

juce::AudioProcessorValueTreeState::ParameterLayout NAMDoubleProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kWidth, 1 }, "Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.45f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) {
                return juce::String (juce::roundToInt (v * 100.0f)) + "%";
            })));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { kMode, 1 }, "Mode",
        juce::StringArray { "Tight", "Loose", "Wide" }, 0));

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

NAMDoubleProcessor::NAMDoubleProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

void NAMDoubleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int ch = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    dsp.prepare (sampleRate, samplesPerBlock, juce::jmax (1, ch));
    setLatencySamples (0);
}

bool NAMDoubleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;

    // Output is always stereo (we make a stereo image from anything mono).
    if (out != juce::AudioChannelSet::stereo()) return false;

    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

void NAMDoubleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    // If the host gave us mono in / stereo out, mirror channel 0 into 1
    // BEFORE the DSP runs so the doubling logic always sees stereo source.
    if (numIn == 1 && numOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    // Clear any output channels we don't drive.
    for (int c = juce::jmax (numIn, 2); c < numOut; ++c)
        buffer.clear (c, 0, n);

    const float width = apvts.getRawParameterValue (kWidth)->load();
    const int   mode  = (int) apvts.getRawParameterValue (kMode)->load();
    const float mix   = apvts.getRawParameterValue (kMix)->load();
    const float outDb = apvts.getRawParameterValue (kOutput)->load();

    dsp.process (buffer, width, mode, mix, outDb);
}

juce::AudioProcessorEditor* NAMDoubleProcessor::createEditor()
{
    return new NAMDoubleEditor (*this);
}

void NAMDoubleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }
}

void NAMDoubleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NAMDoubleProcessor();
}
