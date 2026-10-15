#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Bloom";
        s.tagline     = "Notes swell and sing as they sustain";
        s.index       = 17;
        s.accent      = juce::Colour (0xffe8a3d0);
        s.macroId     = "bloom";
        s.macroLegend = "Bloom";
        s.zoneLegends = { "DECAY", "HOLD", "SING" };
        s.modeId      = "mode";
        s.modeLabels  = { "Swell", "Sustain", "Singing" };
        s.modeLegend  = "Time";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCBloomEditor::AtiNAMatiCBloomEditor (AtiNAMatiCBloomProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,   [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,   [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Bloom",  okui::Row::Kind::Bipolar, [&d] { return d.bloomDb.load(); }, -9.0f, 9.0f });
    activity.status = [&d]
    {
        const float b = d.bloomDb.load();
        return juce::String (b < 0.5f ? "ATTACK" : b < 3.0f ? "BLOOMING" : "SINGING");
    };
    activity.statusLevel = [&d] { return juce::jlimit (0.0f, 1.0f, d.bloomDb.load() / 6.0f); };
}
