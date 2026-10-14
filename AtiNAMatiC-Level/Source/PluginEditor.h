#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class NAMLevelEditor : public okui::Editor
{
public:
    explicit NAMLevelEditor (NAMLevelProcessor&);

private:
    NAMLevelProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMLevelEditor)
};
