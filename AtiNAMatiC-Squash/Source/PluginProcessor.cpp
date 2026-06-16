#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace pid
{
    static const juce::String squash { "squash" };
    static const juce::String mode   { "mode"   };
    static const juce::String mix    { "mix"    };
    static const juce::String output { "output" };
}

juce::AudioProcessorValueTreeState::ParameterLayout NAMSquashProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::squash, 1 },
        "Squash", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<C> (juce::ParameterID { pid::mode, 1 },
        "Mode", juce::StringArray { "Gentle", "Standard", "Smash" }, 1));

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::mix, 1 },
        "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 1.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::output, 1 },
        "Output", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

NAMSquashProcessor::NAMSquashProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool NAMSquashProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void NAMSquashProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    dsp.prepare (sampleRate, samplesPerBlock, numCh);
}

void NAMSquashProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numCh = juce::jmin (buffer.getNumChannels(), juce::jmax (totalIn, totalOut));
    const int n     = buffer.getNumSamples();
    if (n <= 0 || numCh <= 0) return;

    const float squashVal = apvts.getRawParameterValue (pid::squash)->load();
    const int   modeVal   = (int) apvts.getRawParameterValue (pid::mode)->load();
    const float mixVal    = juce::jlimit (0.0f, 1.0f,
                                apvts.getRawParameterValue (pid::mix)->load());
    const float outDbVal  = apvts.getRawParameterValue (pid::output)->load();

    dsp.squash.store (squashVal);
    dsp.modeIx.store (modeVal);

    // Save dry for parallel mix
    juce::AudioBuffer<float> dry (numCh, n);
    for (int ch = 0; ch < numCh; ++ch)
        dry.copyFrom (ch, 0, buffer, ch, 0, n);

    // Compress in-place
    dsp.process (buffer);

    // Wet/dry blend + output trim
    const float outGain = std::pow (10.0f, outDbVal * 0.05f);
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto*       wp = buffer.getWritePointer (ch);
        const auto* dp = dry.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            wp[i] = (dp[i] * (1.0f - mixVal) + wp[i] * mixVal) * outGain;
    }
}

juce::AudioProcessorEditor* NAMSquashProcessor::createEditor()
{
    return new NAMSquashEditor (*this);
}

void NAMSquashProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NAMSquashProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid())
            apvts.replaceState (tree);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NAMSquashProcessor();
}
