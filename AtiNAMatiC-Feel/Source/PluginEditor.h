#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "FeelLookAndFeel.h"

class NAMFeelEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NAMFeelEditor (NAMFeelProcessor&);
    ~NAMFeelEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NAMFeelProcessor& proc;
    nf::FeelLNF       lnf;
    juce::Image       background;

    nf::TitleBadge   titleBadge;
    nf::AboutOverlay aboutOverlay;

    juce::Label  brandLabel;
    juce::Label  assistValueLabel;
    juce::Label  rawTickLabel, midTickLabel, smoothTickLabel;
    juce::Label  outLabel;

    nf::SectionHeader assistHeader { "ASSIST" };
    nf::SectionHeader modeHeader   { "MODE" };
    nf::SectionHeader meterHeader  { "ACTIVITY" };

    juce::Slider     assistKnob, outputKnob;
    nf::ModePicker   modePicker;
    nf::FeelMeter    meter;
    nf::FeelPill     feelPill;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> aAssist, aOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMFeelEditor)
};
