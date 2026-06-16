#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "TightLookAndFeel.h"

class NAMTightEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NAMTightEditor (NAMTightProcessor&);
    ~NAMTightEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NAMTightProcessor& proc;
    nt::TightLNF       lnf;

    nt::TitleBadge   titleBadge;
    nt::AboutOverlay aboutOverlay;
    juce::Label  brandLabel;
    juce::Label  tightLabel, looseTickLabel, tightTickLabel;
    juce::Label  modeLabel, mixLabel, outLabel;
    juce::Label  meterCaption;

    juce::Slider tightKnob, mixKnob, outputKnob;
    nt::ModePicker modePicker;
    nt::LowMeter meter;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> aTight, aMix, aOut;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aMode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMTightEditor)
};
