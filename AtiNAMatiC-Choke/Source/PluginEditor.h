#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCChokeEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCChokeEditor (AtiNAMatiCChokeProcessor&);

private:
    AtiNAMatiCChokeProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCChokeEditor)
};
