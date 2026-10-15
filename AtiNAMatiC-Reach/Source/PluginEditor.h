#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCReachEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCReachEditor (AtiNAMatiCReachProcessor&);

private:
    AtiNAMatiCReachProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCReachEditor)
};
