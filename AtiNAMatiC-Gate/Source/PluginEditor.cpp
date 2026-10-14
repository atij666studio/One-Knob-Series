#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Gate";
        s.tagline     = "Silent between notes, never chops";
        s.index       = 1;
        s.accent      = juce::Colour (0xffffb347);
        s.macroId     = "gate";
        s.macroLegend = "Gate";
        s.zoneLegends = { "OPEN", "TRACKING", "TIGHT" };
        s.modeId      = "mode";
        s.modeLabels  = { "Adaptive", "Fast", "Natural" };
        s.modeLegend  = "Mode";
        s.footer      = { { "mix", "Mix", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

NAMGateEditor::NAMGateEditor (NAMGateProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    okui::Row in { "Input", okui::Row::Kind::Level, [&d] { return d.inputDb.load(); }, -90.0f, 0.0f };
    in.marker = [&d] { return d.openThreshDb.load(); };
    activity.rows.push_back (in);
    activity.rows.push_back ({ "Floor",  okui::Row::Kind::Level,     [&d] { return d.noiseFloorDb.load(); }, -90.0f, 0.0f });
    activity.rows.push_back ({ "Closed", okui::Row::Kind::Reduction, [&d] { return -d.gateGainDb.load(); }, 0.0f, 65.0f });
    activity.status = [&d]
    {
        const int st = d.stateAtomic.load();
        return juce::String (st == 2 ? "OPEN" : st == 1 ? "MOVING" : "CLOSED");
    };
    activity.statusLevel = [&d] { return d.stateAtomic.load() * 0.5f; };
}
