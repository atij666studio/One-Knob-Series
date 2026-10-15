#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec (juce::AudioProcessorValueTreeState& apvts)
    {
        okui::Spec s;
        s.product     = "Pitch";
        s.tagline     = "Drop tuning, detune & octave";
        s.index       = 22;
        s.accent      = juce::Colour (0xff6aa8ff);
        s.macroId     = "pitch";
        s.macroLegend = "Pitch";
        s.zoneLegends = { "0", "SHIFT", "MAX" };
        s.modeId      = "mode";
        s.modeLabels  = { "Drop", "Detune", "Octave" };
        s.modeLegend  = "Mode";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);

        // The readout speaks the current mode's units.
        auto* mode = apvts.getRawParameterValue ("mode");
        s.macroText = [mode] (float v)
        {
            switch ((int) mode->load())
            {
                case 0:  { const int st = (int) std::round (v * 7.0f); return st == 0 ? juce::String ("0 st") : "-" + juce::String (st) + " st"; }
                case 1:  return juce::String (juce::roundToInt (v * 25.0f)) + " cents";
                default: return "-1 oct  " + juce::String (juce::roundToInt (v * 100.0f)) + " %";
            }
        };
        return s;
    }
}

AtiNAMatiCPitchEditor::AtiNAMatiCPitchEditor (AtiNAMatiCPitchProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec (p.apvts)), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level, [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level, [&d] { return d.outputDb.load(); } });
    auto* mode = proc.apvts.getRawParameterValue ("mode");
    activity.status = [&d, mode]
    {
        const int m = (int) mode->load();
        const float v = d.shiftSt.load();
        if (m == 0) return v == 0.0f ? juce::String ("IN TUNE") : "DROP " + juce::String ((int) v) + " ST";
        if (m == 1) return "DETUNE +/-" + juce::String (juce::roundToInt (v)) + " CENTS";
        return juce::String ("OCTAVE DOWN");
    };
    activity.statusLevel = [&d] { return d.shiftSt.load() != 0.0f ? 1.0f : 0.0f; };
}
