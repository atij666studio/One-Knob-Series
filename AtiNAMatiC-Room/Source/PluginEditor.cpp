#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "Room";
        s.tagline     = "Ambience that ducks out of the way";
        s.index       = 16;
        s.accent      = juce::Colour (0xff5fc4e8);
        s.macroId     = "room";
        s.macroLegend = "Room";
        s.zoneLegends = { "DRY", "AMBIENT", "LARGE" };
        s.modeId      = "mode";
        s.modeLabels  = { "Booth", "Studio", "Hall" };
        s.modeLegend  = "Space";
        s.footer      = { { "duck", "Duck", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCRoomEditor::AtiNAMatiCRoomEditor (AtiNAMatiCRoomProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    activity.rows.push_back ({ "Input", okui::Row::Kind::Level,     [&d] { return d.inputDb.load(); } });
    activity.rows.push_back ({ "Room",  okui::Row::Kind::Level,     [&d] { return d.roomDb.load(); } });
    activity.rows.push_back ({ "Duck",  okui::Row::Kind::Reduction, [&d] { return d.duckDb.load(); }, 0.0f, 10.0f });
    activity.status = [&d]
    {
        const float k = d.duckDb.load();
        return juce::String (d.roomDb.load() < -70.0f ? "DRY" : k > 3.0f ? "DUCKING" : "BLOOMING");
    };
    activity.statusLevel = [&d] { return juce::jlimit (0.0f, 1.0f, (d.roomDb.load() + 60.0f) / 50.0f); };
}
