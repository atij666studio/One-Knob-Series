#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Mono";
        s.tagline     = "Mono where it matters, stereo where it doesn't";
        s.index       = 20;
        s.accent      = juce::Colour (0xffb8c2cc);
        s.macroId     = "mono";
        s.macroLegend = "Mono";
        s.zoneLegends = { "STEREO", "FOCUS", "MONO" };
        s.modeId      = "mode";
        s.modeLabels  = { "Bass", "Full", "Safe" };
        s.modeLegend  = "Mode";
        s.footer      = { { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCMonoEditor::AtiNAMatiCMonoEditor (AtiNAMatiCMonoProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.title = "Stereo image";
    activity.rows.push_back ({ "Left",  okui::Row::Kind::Level,  [&d] { return d.leftDb.load(); } });
    activity.rows.push_back ({ "Right", okui::Row::Kind::Level,  [&d] { return d.rightDb.load(); } });
    activity.rows.push_back ({ "Width", okui::Row::Kind::Amount, [&d] { return d.widthAmt.load(); }, 0.0f, 1.0f });
    activity.status = [&d]
    {
        if (d.correlation.load() < -0.1f) return juce::String ("PHASE ISSUE");
        const float w = d.widthAmt.load();
        return juce::String (w < 0.05f ? "MONO" : w < 0.5f ? "NARROW" : "STEREO");
    };
    activity.statusLevel = [&d] { return d.correlation.load() < -0.1f ? 1.0f : d.widthAmt.load(); };
}
