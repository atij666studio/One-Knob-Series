#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PickLookAndFeel.h"

class NAMPickEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NAMPickEditor (NAMPickProcessor&);
    ~NAMPickEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NAMPickProcessor& proc;
    np::PickLNF       lnf;
    juce::Image       background;

    np::TitleBadge   titleBadge;
    np::AboutOverlay aboutOverlay;
    juce::Label brandLabel;
    juce::Label attackValueLabel;
    juce::Label softTickLabel, neutralTickLabel, sharpTickLabel;
    juce::Label mixLabel, outLabel;

    np::SectionHeader attackHeader { "ATTACK" };
    np::SectionHeader meterHeader  { "ACTIVITY" };

    juce::Slider          attackKnob, mixKnob, outputKnob;
    np::TransientPill     transientPill;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> aAttack, aMix, aOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMPickEditor)
};
