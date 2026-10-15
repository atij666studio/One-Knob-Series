#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Choke";
        s.tagline     = "Shorter ring-out, tighter stops";
        s.index       = 19;
        s.accent      = juce::Colour (0xffe05b6a);
        s.macroId     = "choke";
        s.macroLegend = "Choke";
        s.zoneLegends = { "RING", "TIGHT", "CUT" };
        s.modeId      = "mode";
        s.modeLabels  = { "Tight", "Staccato", "Djent" };
        s.modeLegend  = "Style";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCChokeEditor::AtiNAMatiCChokeEditor (AtiNAMatiCChokeProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,     [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,     [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Choke",  okui::Row::Kind::Reduction, [&d] { return d.chokeDb.load(); }, 0.0f, 30.0f });
    activity.status = [&d]
    {
        const float c = d.chokeDb.load();
        return juce::String (c < 1.0f ? "RINGING" : c < 10.0f ? "TIGHTENING" : "CHOKED");
    };
    activity.statusLevel = [&d] { return juce::jlimit (0.0f, 1.0f, d.chokeDb.load() / 15.0f); };
}
