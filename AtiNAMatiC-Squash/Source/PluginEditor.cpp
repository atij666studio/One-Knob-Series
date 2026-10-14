#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Squash";
        s.tagline     = "Bus glue, punch & squash";
        s.index       = 10;
        s.accent      = juce::Colour (0xffff3355);
        s.macroId     = "squash";
        s.macroLegend = "Squash";
        s.zoneLegends = { "LIGHT", "GLUE", "HEAVY" };
        s.modeId      = "mode";
        s.modeLabels  = { "Gentle", "Standard", "Smash" };
        s.modeLegend  = "Mode";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

NAMSquashEditor::NAMSquashEditor (NAMSquashProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& p2 = proc;
    activity.rows.push_back ({ "Input",  okui::Row::Kind::Level,     [&p2] { return p2.inMeterDb.load(); } });
    activity.rows.push_back ({ "Output", okui::Row::Kind::Level,     [&p2] { return p2.outMeterDb.load(); } });
    activity.rows.push_back ({ "Squash", okui::Row::Kind::Reduction, [&p2] { return -p2.dsp.grDb.load(); }, 0.0f, 24.0f });
    activity.status = [&p2]
    {
        const float gr = -p2.dsp.grDb.load();
        return juce::String (gr < 1.0f ? "IDLE" : gr < 4.0f ? "GLUE" : gr < 10.0f ? "SQUASH" : "SMASH");
    };
    activity.statusLevel = [&p2] { return juce::jlimit (0.0f, 1.0f, -p2.dsp.grDb.load() / 12.0f); };
}
