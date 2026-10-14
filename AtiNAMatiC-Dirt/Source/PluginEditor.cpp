#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Dirt";
        s.tagline     = "Attitude without full distortion";
        s.index       = 7;
        s.accent      = juce::Colour (0xffe0532f);
        s.macroId     = "dirt";
        s.macroLegend = "Dirt";
        s.zoneLegends = { "CLEAN", "EDGE", "GRIT" };
        s.modeId      = "type";
        s.modeLabels  = { "Edge", "Bite", "Crunch" };
        s.modeLegend  = "Type";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

NAMDirtEditor::NAMDirtEditor (NAMDirtProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,  [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,  [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Grit",   okui::Row::Kind::Amount, [&d] { return d.gritAmount.load(); }, 0.0f, 1.0f });
    activity.status = [&d]
    {
        const float a = d.gritAmount.load();
        return juce::String (a < 0.10f ? "CLEAN" : a < 0.45f ? "EDGE" : a < 0.75f ? "BITE" : "GRIT");
    };
    activity.statusLevel = [&d] { return d.gritAmount.load(); };
}
