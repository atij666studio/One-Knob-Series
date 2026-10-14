#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Level";
        s.tagline     = "Auto output leveler";
        s.index       = 11;
        s.accent      = juce::Colour (0xffe9c46a);
        s.macroId     = "level";
        s.macroLegend = "Level";
        s.zoneLegends = { "SOFT", "HOLD", "LOCKED" };
        s.modeId      = "autoOn";
        s.modeLabels  = { "Off", "Auto" };
        s.modeLegend  = "Leveling";
        s.footer      = { { "trim", "Trim", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

NAMLevelEditor::NAMLevelEditor (NAMLevelProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,   [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,   [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Gain",   okui::Row::Kind::Bipolar, [&d] { return d.gainDb.load(); }, -18.0f, 18.0f });
    activity.status = [&d]
    {
        const float g = d.gainDb.load();
        return std::abs (g) < 0.5f ? juce::String ("HOLDING") : (g > 0.0f ? juce::String ("LIFTING") : juce::String ("TAMING"));
    };
    activity.statusLevel = [&d] { return d.grAmount.load(); };
}
