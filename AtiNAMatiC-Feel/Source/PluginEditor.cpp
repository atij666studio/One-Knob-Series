#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Feel";
        s.tagline     = "Forgiving dynamics, same tone";
        s.index       = 4;
        s.accent      = juce::Colour (0xffff8a5c);
        s.macroId     = "assist";
        s.macroLegend = "Assist";
        s.zoneLegends = { "RAW", "FEEL", "SMOOTH" };
        s.modeId      = "mode";
        s.modeLabels  = { "Tight", "Even", "Smooth" };
        s.modeLegend  = "Mode";
        s.footer      = { { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

NAMFeelEditor::NAMFeelEditor (NAMFeelProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,     [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,     [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Assist", okui::Row::Kind::Reduction, [&d] { return d.grDb.load(); }, 0.0f, 18.0f });
    activity.status = [&d]
    {
        const float gr = d.grDb.load();
        return juce::String (gr < 1.0f ? "NATURAL" : gr < 4.0f ? "SMOOTHING" : "ASSISTING");
    };
    activity.statusLevel = [&d] { return juce::jlimit (0.0f, 1.0f, d.grDb.load() / 8.0f); };
}
