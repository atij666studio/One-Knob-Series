#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "BoostLookAndFeel.h"

class NAMBoostEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NAMBoostEditor (NAMBoostProcessor&);
    ~NAMBoostEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NAMBoostProcessor& proc;
    nb::BoostLNF       lnf;
    juce::Image        background;

    nb::TitleBadge   titleBadge;
    nb::AboutOverlay aboutOverlay;

    juce::Label  brandLabel;
    juce::Label  boostValueLabel;
    juce::Label  cleanTickLabel, sweetTickLabel, pushTickLabel;
    juce::Label  mixLabel, outLabel;

    nb::SectionHeader boostHeader { "BOOST" };
    nb::SectionHeader modeHeader  { "CHARACTER" };
    nb::SectionHeader meterHeader { "ACTIVITY" };

    juce::Slider     boostKnob, mixKnob, outputKnob;
    nb::ModePicker   modePicker;
    nb::BoostMeter   meter;
    nb::SparkPill    sparkPill;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> aBoost, aMix, aOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMBoostEditor)
};
