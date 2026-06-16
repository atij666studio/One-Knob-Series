#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "DoubleLookAndFeel.h"

class NAMDoubleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NAMDoubleEditor (NAMDoubleProcessor&);
    ~NAMDoubleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NAMDoubleProcessor& proc;
    nd::DoubleLNF       lnf;

    nd::TitleBadge   titleBadge;
    nd::AboutOverlay aboutOverlay;

    juce::Label  brandLabel;
    juce::Label  widthValueLabel;
    juce::Label  monoTickLabel, doubleTickLabel, wideTickLabel;
    juce::Label  mixLabel, outLabel;

    nd::SectionHeader widthHeader { "WIDTH" };
    nd::SectionHeader modeHeader  { "MODE" };
    nd::SectionHeader imageHeader { "STEREO IMAGE" };

    juce::Slider widthKnob, mixKnob, outputKnob;
    nd::ModePicker  modePicker;
    nd::StereoView  stereoView;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> aWidth, aMix, aOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMDoubleEditor)
};
