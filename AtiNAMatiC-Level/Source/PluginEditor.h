#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "LevelLookAndFeel.h"

class NAMLevelEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NAMLevelEditor (NAMLevelProcessor&);
    ~NAMLevelEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NAMLevelProcessor& proc;
    na::LevelLNF       lnf;

    na::AboutOverlay aboutOverlay;
    na::TitleBadge   titleBadge;

    juce::Label  brandLabel;
    juce::Label  levelValueLabel;
    juce::Label  softTickLabel, midTickLabel, lockedTickLabel;
    juce::Label  trimLabel;

    na::SectionHeader levelHeader  { "LEVEL" };
    na::SectionHeader autoHeader   { "MODE" };
    na::SectionHeader meterHeader  { "OUTPUT" };

    juce::Slider     levelKnob, trimKnob;
    na::AutoToggle   autoToggle;
    na::LevelMeter   meter;
    na::LevelPill    levelPill;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> aLevel, aTrim;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMLevelEditor)
};
