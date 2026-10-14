#pragma once

// Consistent parameter text across the ONE KNOB series, so automation lanes
// and the UI read the same everywhere: "72 %", "+1.5 dB".

#include <juce_audio_processors/juce_audio_processors.h>

namespace ok
{
    inline juce::AudioParameterFloatAttributes percent()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("-0123456789.").getFloatValue() / 100.0f; })
            .withLabel ("%");
    }

    inline juce::AudioParameterFloatAttributes decibels()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int)
            {
                const float r = std::round (v * 10.0f) / 10.0f;
                return (r > 0.0f ? "+" : "") + juce::String (r == 0.0f ? 0.0f : r, 1) + " dB";
            })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("-0123456789.").getFloatValue(); })
            .withLabel ("dB");
    }
}
