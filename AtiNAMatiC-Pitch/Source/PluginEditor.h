#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCPitchEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCPitchEditor (AtiNAMatiCPitchProcessor&);

private:
    AtiNAMatiCPitchProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCPitchEditor)
};
