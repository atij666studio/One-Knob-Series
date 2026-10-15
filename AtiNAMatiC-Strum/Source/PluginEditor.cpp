#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Strum";
        s.tagline     = "String clarity without the boom";
        s.index       = 13;
        s.accent      = juce::Colour (0xff3fc8c0);
        s.macroId     = "strum";
        s.macroLegend = "Strum";
        s.zoneLegends = { "RAW", "SHAPED", "POLISHED" };
        s.modeId      = "mode";
        s.modeLabels  = { "Clean", "Crunch", "Acoustic" };
        s.modeLegend  = "Voicing";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCStrumEditor::AtiNAMatiCStrumEditor (AtiNAMatiCStrumProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,     [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Strike", okui::Row::Kind::Amount,    [&d] { return d.strike.load(); }, 0.0f, 1.0f });
    activity.rows.push_back ({ "Thump",  okui::Row::Kind::Reduction, [&d] { return d.thumpDb.load(); }, 0.0f, 6.0f });
    activity.status = [&d]
    {
        const float s = d.strike.load();
        return juce::String (s < 0.15f ? "RINGING" : s < 0.5f ? "PICKING" : "STRUMMING");
    };
    activity.statusLevel = [&d] { return d.strike.load(); };
}
