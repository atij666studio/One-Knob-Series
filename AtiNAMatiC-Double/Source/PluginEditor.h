#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class NAMDoubleEditor : public okui::Editor
{
public:
    explicit NAMDoubleEditor (NAMDoubleProcessor&);

private:
    NAMDoubleProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMDoubleEditor)
};
