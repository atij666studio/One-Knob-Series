#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "GateLookAndFeel.h"

class NAMGateEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NAMGateEditor (NAMGateProcessor&);
    ~NAMGateEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NAMGateProcessor& proc;
    ng::GateLNF       lnf;
    juce::Image       background;

    ng::TitleBadge   titleBadge;
    ng::AboutOverlay aboutOverlay;
    juce::Label  brandLabel;
    juce::Label  gateValueLabel;
    juce::Label  openTickLabel, trackingTickLabel, tightTickLabel;
    juce::Label  mixLabel, outLabel;

    ng::SectionHeader gateHeader  { "GATE" };
    ng::SectionHeader modeHeader  { "MODE" };
    ng::SectionHeader meterHeader { "ACTIVITY" };

    juce::Slider gateKnob, mixKnob, outputKnob;
    ng::ModePicker  modePicker;
    ng::GateMeter   meter;
    ng::StatePill   statePill;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> aGate, aMix, aOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMGateEditor)
};
