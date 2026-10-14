#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class NAMPickEditor : public okui::Editor
{
public:
    explicit NAMPickEditor (NAMPickProcessor&);

private:
    NAMPickProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMPickEditor)
};
