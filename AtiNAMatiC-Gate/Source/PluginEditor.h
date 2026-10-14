#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class NAMGateEditor : public okui::Editor
{
public:
    explicit NAMGateEditor (NAMGateProcessor&);

private:
    NAMGateProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMGateEditor)
};
