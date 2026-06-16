#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

namespace pid
{
    static const juce::String sweetSpot { "sweetSpot" };
    static const juce::String autoOn    { "autoOn"    };
    static const juce::String response  { "response"  };
    static const juce::String mix       { "mix"       };
    static const juce::String output    { "output"    };
    static const juce::String manualGain{ "manualGain"};
}

APVTS::ParameterLayout AtiNAMatiCHitZoneProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::sweetSpot, 1 },
        "Sweet Spot", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<B> (juce::ParameterID { pid::autoOn, 1 },
        "Auto", true));

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::response, 1 },
        "Response", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.35f));

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::mix, 1 },
        "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 1.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::output, 1 },
        "Output", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::manualGain, 1 },
        "Manual Gain", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

AtiNAMatiCHitZoneProcessor::AtiNAMatiCHitZoneProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool AtiNAMatiCHitZoneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void AtiNAMatiCHitZoneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    dsp.prepare (sampleRate, samplesPerBlock, numCh);
}

void AtiNAMatiCHitZoneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Pull live parameter values into the DSP's atomics.
    dsp.sweetSpot.store    (apvts.getRawParameterValue (pid::sweetSpot)->load());
    dsp.autoOn.store       (apvts.getRawParameterValue (pid::autoOn)->load() > 0.5f);
    dsp.response.store     (apvts.getRawParameterValue (pid::response)->load());
    dsp.mix.store          (apvts.getRawParameterValue (pid::mix)->load());
    dsp.outputDb.store     (apvts.getRawParameterValue (pid::output)->load());
    dsp.manualGainDb.store (apvts.getRawParameterValue (pid::manualGain)->load());

    dsp.process (buffer);
}

juce::AudioProcessorEditor* AtiNAMatiCHitZoneProcessor::createEditor()
{
    return new AtiNAMatiCHitZoneEditor (*this);
}

void AtiNAMatiCHitZoneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }
}

void AtiNAMatiCHitZoneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AtiNAMatiCHitZoneProcessor();
}
