#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Pick";
        s.tagline     = "Change how picking feels, not the tone";
        s.index       = 3;
        s.accent      = juce::Colour (0xff3fd282);
        s.macroId     = "attack";
        s.macroLegend = "Attack";
        s.zoneLegends = { "SOFT", "NEUTRAL", "SHARP" };
        s.macroBipolar = true;
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

NAMPickEditor::NAMPickEditor (NAMPickProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",     okui::Row::Kind::Level,  [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output",    okui::Row::Kind::Level,  [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Transient", okui::Row::Kind::Amount, [&d] { return d.transientDisp.load(); }, 0.0f, 1.0f });
    auto* atk = proc.apvts.getRawParameterValue ("attack");
    activity.status = [atk]
    {
        const float a = atk->load();
        return juce::String (a < 0.47f ? "SOFTENING" : a > 0.53f ? "SHARPENING" : "NEUTRAL");
    };
    activity.statusLevel = [&d] { return d.transientDisp.load(); };
}
