#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCMonoEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCMonoEditor (AtiNAMatiCMonoProcessor&);

private:
    AtiNAMatiCMonoProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCMonoEditor)
};
