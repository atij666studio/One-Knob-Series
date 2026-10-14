#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class NAMFeelEditor : public okui::Editor
{
public:
    explicit NAMFeelEditor (NAMFeelProcessor&);

private:
    NAMFeelProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMFeelEditor)
};
