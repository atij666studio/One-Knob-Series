#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class NAMBoostEditor : public okui::Editor
{
public:
    explicit NAMBoostEditor (NAMBoostProcessor&);

private:
    NAMBoostProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMBoostEditor)
};
