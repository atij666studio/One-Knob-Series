#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

namespace pid
{
    static const juce::String tight   { "tight" };
    static const juce::String mode    { "mode"  };
    static const juce::String mix     { "mix"   };
    static const juce::String output  { "output"};
}

APVTS::ParameterLayout NAMTightProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::tight, 1 },
        "Tight", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    params.push_back (std::make_unique<C> (juce::ParameterID { pid::mode, 1 },
        "Mode", juce::StringArray { "Classic", "Modern", "Extreme" }, 1));

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::mix, 1 },
        "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 1.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::output, 1 },
        "Output", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

NAMTightProcessor::NAMTightProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool NAMTightProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void NAMTightProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    dsp.prepare (sampleRate, samplesPerBlock, numCh);
    dryBuffer.setSize (juce::jmax (1, numCh), samplesPerBlock, false, false, true);
    mixSm.reset (sampleRate, 0.02);
    outSm.reset (sampleRate, 0.02);
    mixSm.setCurrentAndTargetValue (apvts.getRawParameterValue (pid::mix)->load());
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (pid::output)->load()));
}

void NAMTightProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int n     = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(),
                                  juce::jmax (totalIn, totalOut));
    if (n <= 0 || numCh <= 0) return;

    // Pull params
    const float tightVal  = apvts.getRawParameterValue (pid::tight)->load();
    const int   modeVal   = (int) apvts.getRawParameterValue (pid::mode)->load();
    const float mixVal    = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (pid::mix)->load());
    const float outDbVal  = apvts.getRawParameterValue (pid::output)->load();

    dsp.tight.store  (tightVal);
    dsp.modeIx.store (modeVal);

    mixSm.setTargetValue (mixVal);
    outSm.setTargetValue (juce::Decibels::decibelsToGain (outDbVal));

    // Process in chunks of the prepared size so the dry copy never needs
    // a fresh allocation on the audio thread.
    const int cap = dryBuffer.getNumSamples();
    for (int pos = 0; pos < n; pos += cap)
    {
        const int len = juce::jmin (cap, n - pos);
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), numCh, pos, len);

        // ---- Save dry ----
        for (int ch = 0; ch < numCh; ++ch)
            dryBuffer.copyFrom (ch, 0, view, ch, 0, len);

        // ---- Tighten in-place ----
        dsp.process (view);

        // ---- Wet/dry mix vs. original input + output trim (smoothed) ----
        for (int i = 0; i < len; ++i)
        {
            const float m = mixSm.getNextValue();
            const float g = outSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* wp = view.getWritePointer (ch);
                wp[i] = (dryBuffer.getSample (ch, i) * (1.0f - m) + wp[i] * m) * g;
            }
        }
    }
}

juce::AudioProcessorEditor* NAMTightProcessor::createEditor()
{
    return new NAMTightEditor (*this);
}

void NAMTightProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NAMTightProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new NAMTightProcessor();
}
