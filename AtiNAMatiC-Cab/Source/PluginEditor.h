#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCCabEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCCabEditor (AtiNAMatiCCabProcessor&);

private:
    AtiNAMatiCCabProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCCabEditor)
};
