#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Cohere";
        s.tagline     = "Tames the fizz, glues the guitar";
        s.index       = 14;
        s.accent      = juce::Colour (0xff9d9cff);
        s.macroId     = "cohere";
        s.macroLegend = "Cohere";
        s.zoneLegends = { "RAW", "SETTLED", "GLUED" };
        s.modeId      = "mode";
        s.modeLabels  = { "Smooth", "Glue", "Tape" };
        s.modeLegend  = "Character";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCCohereEditor::AtiNAMatiCCohereEditor (AtiNAMatiCCohereProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,     [&d] { return d.outputDb.load(); } });
    activity.rows.push_back ({ "Tame",   okui::Row::Kind::Reduction, [&d] { return d.tameDb.load(); }, 0.0f, 8.0f });
    activity.rows.push_back ({ "Glue",   okui::Row::Kind::Reduction, [&d] { return d.glueDb.load(); }, 0.0f, 8.0f });
    activity.status = [&d]
    {
        const float t = d.tameDb.load();
        return juce::String (t < 0.5f ? "SMOOTH" : t < 3.0f ? "TAMING" : "TAMING HARD");
    };
    activity.statusLevel = [&d] { return juce::jlimit (0.0f, 1.0f, d.tameDb.load() / 4.0f); };
}
