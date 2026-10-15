#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Cab";
        s.tagline     = "Speaker cabinet with one mic knob";
        s.index       = 18;
        s.accent      = juce::Colour (0xffc79a6b);
        s.macroId     = "mic";
        s.macroLegend = "Mic position";
        s.zoneLegends = { "CAP", "CONE", "EDGE" };
        s.modeId      = "mode";
        s.modeLabels  = { "1x12", "2x12", "4x12" };
        s.modeLegend  = "Cabinet";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCCabEditor::AtiNAMatiCCabEditor (AtiNAMatiCCabProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,  [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,  [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Bright", okui::Row::Kind::Amount, [&d] { return d.brightAmt.load(); }, 0.0f, 1.0f });
    activity.status = [&d]
    {
        const float b = d.brightAmt.load();
        return juce::String (b > 0.66f ? "ON THE CAP" : b > 0.33f ? "MID CONE" : "CONE EDGE");
    };
    activity.statusLevel = [&d] { return d.brightAmt.load(); };
}
