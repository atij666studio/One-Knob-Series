#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "HitZoneDSP.h"
#include "HitZoneLookAndFeel.h"

namespace nhz
{
    // Horizontal level meter with sweet-zone band, RMS bar, peak indicator and peak-hold dot.
    class HitZoneMeter : public juce::Component, private juce::Timer
    {
    public:
        explicit HitZoneMeter (HitZoneDSP& d) : dsp (d) { startTimerHz (45); }

        void paint (juce::Graphics& g) override
        {
            auto outer = getLocalBounds().toFloat().reduced (1.0f);

            // Frame
            {
                juce::DropShadow ds (juce::Colours::black.withAlpha (0.45f), 8, { 0, 2 });
                juce::Path fp;
                fp.addRoundedRectangle (outer, 8.0f);
                ds.drawForPath (g, fp);
            }
            juce::ColourGradient bgGrad (juce::Colour (0xFF0E141C), outer.getX(), outer.getY(),
                                         juce::Colour (0xFF0A0F17), outer.getX(), outer.getBottom(), false);
            g.setGradientFill (bgGrad);
            g.fillRoundedRectangle (outer, 8.0f);
            g.setColour (col::stroke);
            g.drawRoundedRectangle (outer, 8.0f, 1.0f);

            // Track area (leaves room for label strip on top, scale on bottom)
            auto labelStrip = outer.removeFromTop (16.0f);
            auto scaleStrip = outer.removeFromBottom (14.0f);
            auto track      = outer.reduced (8.0f, 4.0f);

            const float minDb = -60.0f, maxDb = 0.0f;
            auto dbToX = [&] (float db)
            {
                const float t = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
                return track.getX() + t * track.getWidth();
            };

            // Sweet zone band
            const float zx0 = dbToX (kTargetRmsLowDb);
            const float zx1 = dbToX (kTargetRmsHighDb);
            auto zoneRect = juce::Rectangle<float> (zx0, track.getY(),
                                                    zx1 - zx0, track.getHeight());

            const auto status = (HitStatus) dsp.statusEnum.load();
            juce::Colour zoneFill = juce::Colour (0xFF1B3A2A);
            juce::Colour zoneEdge = col::green.withAlpha (0.55f);
            if (status == HitStatus::TooHot)  { zoneFill = juce::Colour (0xFF4A1A1A); zoneEdge = col::red.withAlpha (0.55f); }
            if (status == HitStatus::TooCold) { zoneFill = juce::Colour (0xFF1A2440); zoneEdge = col::cold.withAlpha (0.45f); }

            // Pulse only when Perfect
            float pulse = 1.0f;
            if (status == HitStatus::Perfect)
                pulse = 0.78f + 0.22f * (float) std::sin (juce::Time::getMillisecondCounterHiRes() * 0.0045);

            g.setColour (zoneFill.withAlpha (pulse));
            g.fillRoundedRectangle (zoneRect, 4.0f);
            g.setColour (zoneEdge);
            g.drawRoundedRectangle (zoneRect, 4.0f, 1.2f);

            // Scale ticks (subtle, every 6 dB)
            g.setColour (juce::Colour (0xFF2A3648));
            for (int db = -60; db <= 0; db += 6)
            {
                const float x = dbToX ((float) db);
                g.drawVerticalLine ((int) x, scaleStrip.getY() + 1.0f, scaleStrip.getY() + 5.0f);
            }
            g.setColour (col::textMute);
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            for (int db : { -60, -36, -18, -6 })
            {
                const float x = dbToX ((float) db);
                g.drawText (juce::String (db), (int) (x - 14), (int) scaleStrip.getY() + 4, 28, 10,
                            juce::Justification::centred);
            }

            // RMS level bar
            const float rms  = dsp.currentRmsDb.load();
            const float lvlX = dbToX (rms);
            auto lvlRect = juce::Rectangle<float> (track.getX() + 2.0f, track.getY() + 3.0f,
                                                   juce::jmax (0.0f, lvlX - track.getX() - 2.0f),
                                                   track.getHeight() - 6.0f);

            juce::Colour lvlColour = col::mid;
            if (status == HitStatus::Perfect) lvlColour = col::green;
            if (status == HitStatus::TooHot)  lvlColour = col::red;

            // Outer glow on the bar
            if (lvlRect.getWidth() > 2.0f)
            {
                juce::DropShadow lg (lvlColour.withAlpha (0.35f), 10, {});
                juce::Path lp;
                lp.addRoundedRectangle (lvlRect, 3.0f);
                lg.drawForPath (g, lp);
            }
            juce::ColourGradient grad (lvlColour.withAlpha (0.30f), lvlRect.getX(), 0.0f,
                                       lvlColour,                    lvlRect.getRight(), 0.0f, false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (lvlRect, 3.0f);

            // Peak indicator (thin line)
            const float pX = dbToX (dsp.peakDb.load());
            g.setColour (lvlColour.brighter (0.4f).withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> (pX - 1.0f, track.getY() + 2.0f, 2.0f, track.getHeight() - 4.0f));

            // Peak-hold dot
            const float pH = dbToX (dsp.peakHoldDb.load());
            const auto pHColour = (dsp.peakHoldDb.load() > kPeakDangerDb) ? col::red : juce::Colours::white;
            g.setColour (pHColour);
            g.fillEllipse (pH - 3.0f, track.getCentreY() - 3.0f, 6.0f, 6.0f);

            // SWEET SPOT label centered above the band
            g.setColour (col::textDim);
            g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
            const auto zoneCx = (int) ((zx0 + zx1) * 0.5f);
            g.drawText ("SWEET SPOT", zoneCx - 60, (int) labelStrip.getY() + 2, 120, 14,
                        juce::Justification::centred);

            // LOW / HIGH labels at the edges
            g.setColour (col::textMute);
            g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
            g.drawText ("LOW",  (int) labelStrip.getX() + 4, (int) labelStrip.getY() + 3, 40, 12, juce::Justification::left);
            g.drawText ("HIGH", (int) labelStrip.getRight() - 44, (int) labelStrip.getY() + 3, 40, 12, juce::Justification::right);
        }

    private:
        void timerCallback() override { repaint(); }
        HitZoneDSP& dsp;
    };
}
