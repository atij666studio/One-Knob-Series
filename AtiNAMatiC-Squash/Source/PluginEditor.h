#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "SquashLookAndFeel.h"

class NAMSquashEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NAMSquashEditor (NAMSquashProcessor&);
    ~NAMSquashEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NAMSquashProcessor& proc;
    sq::SquashLNF       lnf;
    juce::Image          background;

    sq::TitleBadge   titleBadge;
    sq::AboutOverlay aboutOverlay;

    juce::Label brandLabel;
    juce::Label squashLabel, lightTickLabel, heavyTickLabel;
    juce::Label modeLabel, mixLabel, outLabel;
    juce::Label grCaption, grMaxLabel;

    juce::Slider squashKnob, mixKnob, outputKnob;
    sq::ModePicker modePicker;
    sq::GrMeter    grMeter;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> aSquash, aMix, aOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMSquashEditor)
};
