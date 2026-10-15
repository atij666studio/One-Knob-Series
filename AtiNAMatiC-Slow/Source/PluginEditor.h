#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCSlowEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCSlowEditor (AtiNAMatiCSlowProcessor&);

private:
    AtiNAMatiCSlowProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCSlowEditor)
};
