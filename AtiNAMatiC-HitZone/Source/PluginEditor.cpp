#include "PluginEditor.h"

namespace
{
    okui::Spec makeSpec()
    {
        okui::Spec s;
        s.product     = "HitZone";
        s.tagline     = "Always hit the amp at the right level";
        s.index       = 5;
        s.accent      = juce::Colour (0xff5aa9ff);
        s.macroId     = "sweetSpot";
        s.macroLegend = "Sweet spot";
        s.zoneLegends = { "COLD", "SWEET", "HOT" };
        s.modeId      = "autoOn";
        s.modeLabels  = { "Manual", "Auto" };
        s.modeLegend  = "Input gain";
        s.footer      = { { "mix", "Mix", {} }, { "response", "Response", {} }, { "manualGain", "Gain", {} }, { "output", "Output", {} } };
        s.background  = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);
        return s;
    }
}

AtiNAMatiCHitZoneEditor::AtiNAMatiCHitZoneEditor (AtiNAMatiCHitZoneProcessor& p)
    : okui::Editor (p, p.apvts, makeSpec()), proc (p)
{
    auto& d = proc.dsp;
    okui::Row toAmp { "To amp", okui::Row::Kind::Level, [&d] { return d.currentRmsDb.load(); }, -42.0f, 0.0f };
    toAmp.zoneLo = nhz::kTargetRmsLowDb;
    toAmp.zoneHi = nhz::kTargetRmsHighDb;
    toAmp.marker = [] { return nhz::kTargetRmsIdeal; };
    activity.rows.push_back (toAmp);
    activity.rows.push_back ({ "Peak", okui::Row::Kind::Level,   [&d] { return d.peakHoldDb.load(); }, -42.0f, 0.0f });
    activity.rows.push_back ({ "Gain", okui::Row::Kind::Bipolar, [&d] { return d.appliedGainDb.load(); }, -24.0f, 24.0f });
    activity.status = [&d]
    {
        const auto st = (nhz::HitStatus) d.statusEnum.load();
        return juce::String (st == nhz::HitStatus::TooCold ? "TOO COLD" : st == nhz::HitStatus::TooHot ? "TOO HOT" : "IN THE ZONE");
    };
    activity.statusLevel = [&d] { return d.statusEnum.load() == (int) nhz::HitStatus::Perfect ? 1.0f : 0.25f; };
    lastAuto = proc.apvts.getRawParameterValue ("autoOn")->load() > 0.5f;
    relayout();   // the base constructor laid out before our override existed
}

void AtiNAMatiCHitZoneEditor::layoutExtras (juce::Rectangle<int>)
{
    // Response (Auto) and Gain (Manual) share the middle footer slot.
    auto* mix  = footerKnob ("mix");
    auto* resp = footerKnob ("response");
    auto* gain = footerKnob ("manualGain");
    auto* out  = footerKnob ("output");
    if (mix == nullptr || resp == nullptr || gain == nullptr || out == nullptr) return;
    const int y = mix->getY();
    mix ->setTopLeftPosition (baseW / 2 - 140 - 32, y);
    resp->setTopLeftPosition (baseW / 2 - 32,       y);
    gain->setTopLeftPosition (baseW / 2 - 32,       y);
    out ->setTopLeftPosition (baseW / 2 + 140 - 32, y);
    const bool isAuto = proc.apvts.getRawParameterValue ("autoOn")->load() > 0.5f;
    resp->setVisible (isAuto);
    gain->setVisible (! isAuto);
}

void AtiNAMatiCHitZoneEditor::onTick()
{
    const bool isAuto = proc.apvts.getRawParameterValue ("autoOn")->load() > 0.5f;
    if (isAuto != lastAuto)
    {
        lastAuto = isAuto;
        relayout();
    }
}
