#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Boost";
        s.tagline     = "Life, punch & clarity into the amp";
        s.index       = 6;
        s.accent      = juce::Colour (0xffa8e04a);
        s.macroId     = "boost";
        s.macroLegend = "Boost";
        s.zoneLegends = { "CLEAN", "SWEET", "PUSH" };
        s.modeId      = "mode";
        s.modeLabels  = { "Balanced", "Bright", "Thick" };
        s.modeLegend  = "Character";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

NAMBoostEditor::NAMBoostEditor (NAMBoostProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,  [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,  [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Harmonics", okui::Row::Kind::Amount, [&d] { return d.sparkAmount.load(); }, 0.0f, 1.0f });
    activity.status = [&d]
    {
        const float s = d.sparkAmount.load();
        return juce::String (s < 0.10f ? "CLEAN" : s < 0.45f ? "LIFT" : s < 0.75f ? "ALIVE" : "PUSH");
    };
    activity.statusLevel = [&d] { return d.sparkAmount.load(); };
}
