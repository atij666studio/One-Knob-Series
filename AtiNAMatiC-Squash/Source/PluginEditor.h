#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class NAMSquashEditor : public okui::Editor
{
public:
    explicit NAMSquashEditor (NAMSquashProcessor&);

private:
    NAMSquashProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMSquashEditor)
};
