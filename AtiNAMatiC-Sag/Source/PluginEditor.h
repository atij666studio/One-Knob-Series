#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCSagEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCSagEditor (AtiNAMatiCSagProcessor&);

private:
    AtiNAMatiCSagProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCSagEditor)
};
