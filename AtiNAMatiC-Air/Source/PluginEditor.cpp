#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Air";
        s.tagline     = "Cab feel & amp-in-the-room depth";
        s.index       = 8;
        s.accent      = juce::Colour (0xffff6aa8);
        s.macroId     = "air";
        s.macroLegend = "Air";
        s.zoneLegends = { "DRY", "AIR", "OPEN" };
        s.modeId      = "mode";
        s.modeLabels  = { "Studio", "Room" };
        s.modeLegend  = "Mode";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCAirEditor::AtiNAMatiCAirEditor (AtiNAMatiCAirProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,  [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,  [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Depth",  okui::Row::Kind::Amount, [&d] { return d.airAmount.load(); }, 0.0f, 1.0f });
    activity.status = [&d]
    {
        const float a = d.airAmount.load();
        return juce::String (a < 0.10f ? "DRY" : a < 0.45f ? "AIRY" : a < 0.75f ? "OPEN" : "WIDE");
    };
    activity.statusLevel = [&d] { return d.airAmount.load(); };
}
