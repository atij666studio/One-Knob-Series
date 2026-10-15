#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Hum";
        s.tagline     = "Mains hum and buzz, gone";
        s.index       = 21;
        s.accent      = juce::Colour (0xff7fd6a0);
        s.macroId     = "hum";
        s.macroLegend = "Hum removal";
        s.zoneLegends = { "OFF", "HUM", "BUZZ" };
        s.modeId      = "mode";
        s.modeLabels  = { "Auto", "50 Hz", "60 Hz" };
        s.modeLegend  = "Mains";
        s.footer      = { { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCHumEditor::AtiNAMatiCHumEditor (AtiNAMatiCHumProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input",   okui::Row::Kind::Level,     [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Hum",     okui::Row::Kind::Level,     [&d] { return d.humDb.load(); }, -100.0f, -20.0f });
    activity.rows.push_back ({ "Removed", okui::Row::Kind::Reduction, [&d] { return d.removedDb.load(); }, 0.0f, 40.0f });
    activity.status = [&d]
    {
        const auto hz = juce::String (d.mainsHz.load()) + " HZ";
        return d.humDb.load() < -85.0f ? juce::String ("CLEAN") : hz + (d.removedDb.load() > 6.0f ? " REMOVED" : " HUM");
    };
    activity.statusLevel = [&d] { return juce::jlimit (0.0f, 1.0f, d.removedDb.load() / 20.0f); };
}
