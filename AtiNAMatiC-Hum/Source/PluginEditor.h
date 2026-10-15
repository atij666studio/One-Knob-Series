#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCHumEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCHumEditor (AtiNAMatiCHumProcessor&);

private:
    AtiNAMatiCHumProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCHumEditor)
};
