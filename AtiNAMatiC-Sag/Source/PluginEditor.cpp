#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Sag";
        s.tagline     = "Power-supply sag, bloom & touch";
        s.index       = 12;
        s.accent      = juce::Colour (0xffd98a55);
        s.macroId     = "sag";
        s.macroLegend = "Sag";
        s.zoneLegends = { "STIFF", "WARM", "SPONGY" };
        s.modeId      = "mode";
        s.modeLabels  = { "Rectifier", "Variac", "Bloom" };
        s.modeLegend  = "Supply";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCSagEditor::AtiNAMatiCSagEditor (AtiNAMatiCSagProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,     [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,     [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Droop",  okui::Row::Kind::Reduction, [&d] { return d.sagDb.load(); }, 0.0f, 12.0f });
    activity.status = [&d]
    {
        const float s = d.sagDb.load();
        return juce::String (s < 0.5f ? "STIFF" : s < 3.0f ? "BREATHING" : "SAGGING");
    };
    activity.statusLevel = [&d] { return juce::jlimit (0.0f, 1.0f, d.sagDb.load() / 6.0f); };
}
