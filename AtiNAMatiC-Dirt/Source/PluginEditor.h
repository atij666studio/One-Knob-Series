#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <OneKnobUI.h>

class NAMDirtEditor : public okui::Editor
{
public:
    explicit NAMDirtEditor (NAMDirtProcessor&);

private:
    NAMDirtProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NAMDirtEditor)
};
