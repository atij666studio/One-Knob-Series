#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "DirtLookAndFeel.h"

class NAMDirtEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NAMDirtEditor (NAMDirtProcessor&);
    ~NAMDirtEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NAMDirtProcessor& proc;
    nd::DirtLNF       lnf;
    juce::Image       background;

    nd::AboutOverlay aboutOverlay;
    nd::TitleBadge   titleBadge;

    juce::Label  brandLabel;
    juce::Label  dirtValueLabel;
    juce::Label  cleanTickLabel, midTickLabel, gritTickLabel;
    juce::Label  mixLabel, outLabel;

    nd::SectionHeader dirtHeader  { "DIRT" };
    nd::SectionHeader typeHeader  { "TYPE" };
    nd::SectionHeader meterHeader { "ACTIVITY" };

    juce::Slider     dirtKnob, mixKnob, outputKnob;
    nd::TypePicker   typePicker;
    nd::DirtMeter    meter;
    nd::GritPill     gritPill;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> aDirt, aMix, aOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMDirtEditor)
};
