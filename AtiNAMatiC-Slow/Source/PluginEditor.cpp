#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Slow";
        s.tagline     = "Every note swells in";
        s.index       = 23;
        s.accent      = juce::Colour (0xff8fd3ff);
        s.macroId     = "slow";
        s.macroLegend = "Swell time";
        s.zoneLegends = { "FAST", "SWELL", "PAD" };
        s.modeId      = "mode";
        s.modeLabels  = { "Swell", "Pad", "Reverse" };
        s.modeLegend  = "Curve";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        s.macroText   = [] (float v)
        {
            const float ms = 50.0f * std::pow (40.0f, v);
            return ms < 1000.0f ? juce::String (juce::roundToInt (ms)) + " ms" : juce::String (ms / 1000.0f, 2) + " s";
        };
        return s;
    }
}

AtiNAMatiCSlowEditor::AtiNAMatiCSlowEditor (AtiNAMatiCSlowProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,  [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,  [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Swell",  okui::Row::Kind::Amount, [&d] { return d.swellAmt.load(); }, 0.0f, 1.0f });
    activity.status = [&d]
    {
        const float a = d.swellAmt.load();
        return juce::String (a < 0.05f ? "WAITING" : a < 0.98f ? "SWELLING" : "OPEN");
    };
    activity.statusLevel = [&d] { return d.swellAmt.load(); };
}
