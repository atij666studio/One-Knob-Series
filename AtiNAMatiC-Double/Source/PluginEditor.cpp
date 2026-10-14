#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Double";
        s.tagline     = "Double tracking without chorus";
        s.index       = 9;
        s.accent      = juce::Colour (0xff4f86ff);
        s.macroId     = "width";
        s.macroLegend = "Width";
        s.zoneLegends = { "MONO", "DOUBLE", "WIDE" };
        s.modeId      = "mode";
        s.modeLabels  = { "Tight", "Loose", "Wide" };
        s.modeLegend  = "Mode";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

NAMDoubleEditor::NAMDoubleEditor (NAMDoubleProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.title = "Stereo image";
    activity.rows.push_back ({ "Left",  okui::Row::Kind::Level,  [&d] { return d.levelL.load(); } });
    activity.rows.push_back ({ "Right", okui::Row::Kind::Level,  [&d] { return d.levelR.load(); } });
    activity.rows.push_back ({ "Width", okui::Row::Kind::Amount, [&d] { return d.widthDisp.load(); }, 0.0f, 1.0f });
    activity.status = [&d]
    {
        const float w = d.widthDisp.load();
        return juce::String (w < 0.05f ? "MONO" : w < 0.35f ? "TIGHT DOUBLE" : w < 0.7f ? "DOUBLE" : "WIDE");
    };
    activity.statusLevel = [&d] { return d.widthDisp.load(); };
}
