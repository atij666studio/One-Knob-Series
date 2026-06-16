#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "HitZoneLookAndFeel.h"
#include "MeterComponent.h"

class AtiNAMatiCHitZoneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AtiNAMatiCHitZoneEditor (AtiNAMatiCHitZoneProcessor&);
    ~AtiNAMatiCHitZoneEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateAutoVisuals();

    AtiNAMatiCHitZoneProcessor& proc;
    nhz::HitZoneLNF      lnf;

    nhz::TitleBadge  titleBadge;
    nhz::AboutOverlay aboutOverlay;

    juce::Label sweetLabel;
    juce::Label coldLabel, hotLabel;
    juce::Label statusLabel;
    juce::Label gainReadout;
    juce::Label mixLabel, outLabel, responseLabel;

    juce::Slider sweetSpot;
    juce::ToggleButton autoToggle { "AUTO" };
    juce::Slider mixSlider, outputSlider, responseSlider;
    juce::Slider manualGainSlider;

    nhz::HitZoneMeter meter;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SAtt> aSweet, aMix, aOut, aResp, aManual;
    std::unique_ptr<BAtt> aAuto;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCHitZoneEditor)
};
