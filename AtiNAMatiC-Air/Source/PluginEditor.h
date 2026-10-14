#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class AtiNAMatiCAirEditor : public okui::Editor
{
public:
    explicit AtiNAMatiCAirEditor (AtiNAMatiCAirProcessor&);

private:
    AtiNAMatiCAirProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtiNAMatiCAirEditor)
};
