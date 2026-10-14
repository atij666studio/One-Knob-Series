#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class NAMTightEditor : public okui::Editor
{
public:
    explicit NAMTightEditor (NAMTightProcessor&);

private:
    NAMTightProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMTightEditor)
};
