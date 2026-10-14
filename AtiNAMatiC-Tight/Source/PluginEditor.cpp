#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Tight";
        s.tagline     = "Instant modern tightness";
        s.index       = 2;
        s.accent      = juce::Colour (0xffb06cf7);
        s.macroId     = "tight";
        s.macroLegend = "Tight";
        s.zoneLegends = { "LOOSE", "FIRM", "TIGHT" };
        s.modeId      = "mode";
        s.modeLabels  = { "Classic", "Modern", "Extreme" };
        s.modeLegend  = "Mode";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

NAMTightEditor::NAMTightEditor (NAMTightProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& p2 = proc;
    activity.rows.push_back ({ "Input",    okui::Row::Kind::Level, [&p2] { return p2.inMeterDb.load(); } });
    activity.rows.push_back ({ "Output",   okui::Row::Kind::Level, [&p2] { return p2.outMeterDb.load(); } });
    activity.rows.push_back ({ "Low end",  okui::Row::Kind::Level, [&p2] { return p2.dsp.lowEnergyDb.load(); } });
    auto* t = proc.apvts.getRawParameterValue ("tight");
    activity.status = [t]
    {
        const float v = t->load();
        return juce::String (v < 0.3f ? "LOOSE" : v < 0.7f ? "FIRM" : "TIGHT");
    };
    activity.statusLevel = [t] { return t->load(); };
}
