#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Reach";
        s.tagline     = "Steps forward, spreads wide, stays mono-safe";
        s.index       = 15;
        s.accent      = juce::Colour (0xffc9d94a);
        s.macroId     = "reach";
        s.macroLegend = "Reach";
        s.zoneLegends = { "NEAR", "FORWARD", "HUGE" };
        s.modeId      = "mode";
        s.modeLabels  = { "Focus", "Wide", "Huge" };
        s.modeLegend  = "Shape";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCReachEditor::AtiNAMatiCReachEditor (AtiNAMatiCReachProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.title = "Stereo image";
    activity.rows.push_back ({ "Left",  okui::Row::Kind::Level,  [&d] { return d.leftDb.load(); } });
    activity.rows.push_back ({ "Right", okui::Row::Kind::Level,  [&d] { return d.rightDb.load(); } });
    activity.rows.push_back ({ "Width", okui::Row::Kind::Amount, [&d] { return d.widthAmt.load(); }, 0.0f, 1.0f });
    activity.status = [&d]
    {
        const float w = d.widthAmt.load();
        return juce::String (w < 0.1f ? "MONO" : w < 0.45f ? "FOCUSED" : w < 0.75f ? "WIDE" : "HUGE");
    };
    activity.statusLevel = [&d] { return d.widthAmt.load(); };
}
