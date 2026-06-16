#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "AirLookAndFeel.h"

class AtiNAMatiCAirEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AtiNAMatiCAirEditor (AtiNAMatiCAirProcessor&);
    ~AtiNAMatiCAirEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    AtiNAMatiCAirProcessor& proc;
    na::AirLNF       lnf;
    juce::Image      background;

    na::AboutOverlay aboutOverlay;
    na::TitleBadge   titleBadge;

    juce::Label  brandLabel;
    juce::Label  airValueLabel;
    juce::Label  dryTickLabel, midTickLabel, openTickLabel;
    juce::Label  mixLabel, outLabel;

    na::SectionHeader airHeader   { "AIR" };
    na::SectionHeader modeHeader  { "MODE" };
    na::SectionHeader meterHeader { "ACTIVITY" };

    juce::Slider     airKnob, mixKnob, outputKnob;
    na::ModePicker   modePicker;
    na::AirMeter     meter;
    na::AirPill      airPill;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SAtt> aAir, aMix, aOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCAirEditor)
};
