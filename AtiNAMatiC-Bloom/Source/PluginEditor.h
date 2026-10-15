#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCBloomEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCBloomEditor (AtiNAMatiCBloomProcessor&);

private:
    AtiNAMatiCBloomProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCBloomEditor)
};
