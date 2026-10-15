#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCRoomEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCRoomEditor (AtiNAMatiCRoomProcessor&);

private:
    AtiNAMatiCRoomProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCRoomEditor)
};
