#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCCohereEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCCohereEditor (AtiNAMatiCCohereProcessor&);

private:
    AtiNAMatiCCohereProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCCohereEditor)
};
