#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCStrumEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCStrumEditor (AtiNAMatiCStrumProcessor&);

private:
    AtiNAMatiCStrumProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCStrumEditor)
};
