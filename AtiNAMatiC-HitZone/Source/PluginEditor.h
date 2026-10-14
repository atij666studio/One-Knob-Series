#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCHitZoneEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCHitZoneEditor (AtiNAMatiCHitZoneProcessor&);

private:
    AtiNAMatiCHitZoneProcessor& proc;

    void layoutExtras (juce::Rectangle<int>) override;
    void onTick() override;
    bool lastAuto { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCHitZoneEditor)
};
