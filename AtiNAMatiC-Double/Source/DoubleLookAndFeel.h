#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace nd
{
    // -------- Palette --------
    namespace col
    {
        static const juce::Colour bg0       (0xff0e1218);
        static const juce::Colour bg1       (0xff161c25);
        static const juce::Colour panel     (0xff1a2230);
        static const juce::Colour panelEdge (0xff262f3f);
        static const juce::Colour text      (0xffffffff);
        static const juce::Colour textDim   (0xffc8d2e0);
        static const juce::Colour textFaint (0xff9da8bc);
        static const juce::Colour accent    (0xff3a7afe);   // electric royal blue — "stereo / doubling"
        static const juce::Colour accentAlt (0xff8a7df0);   // soft violet — second voice
        static const juce::Colour ok        (0xff5dd28d);
        static const juce::Colour warn      (0xffe8a64d);
        static const juce::Colour danger    (0xffe25b5b);
        static const juce::Colour gridLine  (0xff262f3f);
    }

    // -------- LookAndFeel --------
    class DoubleLNF : public juce::LookAndFeel_V4
    {
    public:
        DoubleLNF()
        {
            setColour (juce::Slider::rotarySliderFillColourId,    col::accent);
            setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff222831));
            setColour (juce::Slider::thumbColourId,               col::accent);
            setColour (juce::Slider::textBoxTextColourId,         col::text);
            setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);

            setColour (juce::Label::textColourId,                 col::text);
            setColour (juce::TextButton::buttonColourId,          juce::Colour (0xff222932));
            setColour (juce::TextButton::buttonOnColourId,        col::accent);
            setColour (juce::TextButton::textColourOnId,          juce::Colours::black);
            setColour (juce::TextButton::textColourOffId,         col::text);
        }

        juce::Font getLabelFont (juce::Label& l) override
        {
            return juce::Font ("Helvetica Neue", l.getFont().getHeight(), juce::Font::plain);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, const float startAngle, const float endAngle,
                               juce::Slider& slider) override
        {
            const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
            const auto cx = bounds.getCentreX();
            const auto cy = bounds.getCentreY();
            const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto angle  = startAngle + sliderPos * (endAngle - startAngle);
            const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

            // Glow spread scales down on small knobs -- otherwise a halo sized for
            // the big 200px knob swallows a thin 60px knob's ring and reads as blur.
            const auto glowScale = juce::jlimit (0.30f, 1.0f, radius / 95.0f);

            // Drop shadow beneath the whole knob.
            g.setColour (juce::Colours::black.withAlpha (0.40f));
            g.fillEllipse (cx - radius, cy - radius + 3.0f, radius * 2.0f, radius * 2.0f);

            // Outer bezel -- beveled dark-metal rim.
            juce::ColourGradient bezelGrad (juce::Colour (0xff394456), cx, cy - radius,
                                            juce::Colour (0xff111419), cx, cy + radius, false);
            g.setGradientFill (bezelGrad);
            g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

            const auto bezelInner = radius * 0.86f;
            g.setColour (juce::Colour (0xff0b0e13));
            g.fillEllipse (cx - bezelInner, cy - bezelInner, bezelInner * 2.0f, bezelInner * 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawEllipse (cx - radius + 1.0f, cy - radius + 1.0f, (radius - 1.0f) * 2.0f, (radius - 1.0f) * 2.0f, 1.0f);

            // Tick marks ringing the bezel -- lit up to the current value.
            constexpr int numTicks = 11;
            for (int t = 0; t < numTicks; ++t)
            {
                const float ta = startAngle + (float) t / (float) (numTicks - 1) * (endAngle - startAngle);
                const float r1 = bezelInner + 2.0f;
                const float r2 = radius - 3.0f;
                const float sx1 = cx + std::sin (ta) * r1;
                const float sy1 = cy - std::cos (ta) * r1;
                const float sx2 = cx + std::sin (ta) * r2;
                const float sy2 = cy - std::cos (ta) * r2;
                if (ta <= angle)
                {
                    g.setColour (accent.withAlpha (0.30f));
                    g.drawLine (sx1, sy1, sx2, sy2, 1.6f + 1.6f * glowScale);
                    g.setColour (accent.brighter (0.25f));
                    g.drawLine (sx1, sy1, sx2, sy2, 1.4f);
                }
                else
                {
                    g.setColour (juce::Colours::white.withAlpha (0.10f));
                    g.drawLine (sx1, sy1, sx2, sy2, 1.4f);
                }
            }

            // Progress arc track, inset inside the bezel.
            const auto trackThickness = juce::jmax (4.0f, bezelInner * 0.11f);
            const auto trackR = bezelInner - trackThickness * 0.5f - 3.0f;

            juce::Path track;
            track.addCentredArc (cx, cy, trackR, trackR, 0.0f, startAngle, endAngle, true);
            g.setColour (accent.darker (0.78f));
            g.strokePath (track, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved));

            juce::Path filled;
            filled.addCentredArc (cx, cy, trackR, trackR, 0.0f, startAngle, angle, true);

            // Layered glow (bloom) behind the lit arc -- spread is additive pixels
            // scaled by glowScale, so it stays proportional instead of swallowing
            // a thin small-knob ring in haze.
            g.setColour (accent.withAlpha (0.15f));
            g.strokePath (filled, juce::PathStrokeType (trackThickness + 8.0f * glowScale, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
            g.setColour (accent.withAlpha (0.30f));
            g.strokePath (filled, juce::PathStrokeType (trackThickness + 3.5f * glowScale, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            // Filled arc keeps the accent -> accentAlt gradient ("two voices" feel).
            juce::ColourGradient grad (col::accent.brighter (0.05f), cx - trackR, cy,
                                       col::accentAlt.brighter (0.1f), cx + trackR, cy, false);
            g.setGradientFill (grad);
            g.strokePath (filled, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            // Bright highlight core down the middle of the lit arc -- only on rings
            // thick enough that a translucent core stroke won't just look like mush.
            if (trackThickness > 6.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.strokePath (filled, juce::PathStrokeType (trackThickness * 0.3f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
            }

            // Glossy face.
            const auto inner = trackR - trackThickness * 0.9f;
            juce::ColourGradient faceGrad (juce::Colour (0xff2a3444), cx - inner * 0.4f, cy - inner * 0.5f,
                                           juce::Colour (0xff0a0d12), cx + inner * 0.3f, cy + inner * 0.7f, true);
            g.setGradientFill (faceGrad);
            g.fillEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f);

            juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.14f), cx - inner * 0.35f, cy - inner * 0.55f,
                                        juce::Colours::white.withAlpha (0.0f),  cx, cy + inner * 0.2f, true);
            g.setGradientFill (gloss);
            g.fillEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f);

            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f, 1.0f);

            // Pointer.
            juce::Path pointer;
            const float pw = juce::jmax (3.5f, inner * 0.09f);
            const float pl = inner * 0.74f;
            pointer.addRoundedRectangle (-pw * 0.5f, -inner * 0.86f, pw, pl, pw * 0.5f);
            const auto rot = juce::AffineTransform::rotation (angle).translated (cx, cy);

            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillPath (pointer, rot.translated (0.0f, 1.5f));
            g.setColour (accent.withAlpha (0.30f));
            g.strokePath (pointer, juce::PathStrokeType (1.0f + 2.0f * glowScale), rot);
            g.setColour (accent.brighter (0.35f));
            g.fillPath (pointer, rot);

            const float dotR = juce::jmax (2.5f, inner * 0.08f);
            g.setColour (juce::Colour (0xff05070a));
            g.fillEllipse (cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
            g.setColour (accent.withAlpha (0.85f));
            g.drawEllipse (cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f, 1.0f);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                   const juce::Colour&,
                                   bool /*shouldDrawButtonAsHighlighted*/,
                                   bool shouldDrawButtonAsDown) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            const bool on = b.getToggleState();
            juce::Colour fill = on ? col::accent : juce::Colour (0xff161c26);
            if (shouldDrawButtonAsDown) fill = fill.darker (0.15f);

            const float radius = r.getHeight() * 0.5f;
            const bool connectedLeft  = (b.getConnectedEdgeFlags() & juce::Button::ConnectedOnLeft)  != 0;
            const bool connectedRight = (b.getConnectedEdgeFlags() & juce::Button::ConnectedOnRight) != 0;

            juce::Path p;
            p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius, radius,
                                   ! connectedLeft, ! connectedRight, ! connectedLeft, ! connectedRight);

            g.setColour (fill);
            g.fillPath (p);

            if (! on)
            {
                g.setColour (col::panelEdge.withAlpha (0.7f));
                g.strokePath (p, juce::PathStrokeType (1.0f));
            }
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b,
                             bool /*hi*/, bool /*down*/) override
        {
            const bool on = b.getToggleState();
            g.setFont (juce::Font ("Helvetica Neue", 11.0f, juce::Font::bold));
            g.setColour (on ? juce::Colours::black : col::text);
            g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2),
                              juce::Justification::centred, 1);
        }
    };

    // -------- Mode picker --------
    class ModePicker : public juce::Component
    {
    public:
        std::function<void (int)> onChange;

        ModePicker()
        {
            for (int i = 0; i < 3; ++i)
            {
                auto* b = buttons.add (new juce::TextButton (labels[i]));
                b->setRadioGroupId (1);
                b->setClickingTogglesState (true);
                b->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
                b->onClick = [this, i] { if (onChange) onChange (i); };
                addAndMakeVisible (*b);
            }
        }

        void setIndex (int ix)
        {
            ix = juce::jlimit (0, 2, ix);
            buttons[ix]->setToggleState (true, juce::dontSendNotification);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff0c0f15).withAlpha (0.55f));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (col::panelEdge.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            const int w = r.getWidth() / 3;
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setBounds (r.removeFromLeft (i == buttons.size() - 1 ? r.getWidth() : w));
        }

    private:
        const juce::StringArray labels { "TIGHT", "LOOSE", "WIDE" };
        juce::OwnedArray<juce::TextButton> buttons;
    };

    // -------- Section heading --------
    class SectionHeader : public juce::Component
    {
    public:
        juce::String text;
        explicit SectionHeader (juce::String t) : text (std::move (t)) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (col::text);
            g.setFont (juce::Font ("Helvetica Neue", 10.0f, juce::Font::bold));
            g.drawText (text.toUpperCase(),
                        r.removeFromTop (14.0f).toNearestInt(),
                        juce::Justification::centred);
            auto line = r.withSizeKeepingCentre (r.getWidth() * 0.35f, 1.0f);
            g.setColour (col::panelEdge);
            g.fillRect (line);
        }
    };

    // -------- Stereo image visualiser --------
    // Top: dual horizontal level bars for L and R (RMS, dB-mapped).
    // Bottom: width strip with two animated dots showing the two voices'
    //         current micro-pan + drift positions.
    class StereoView : public juce::Component
    {
    public:
        std::atomic<float>* levelLSrc { nullptr };
        std::atomic<float>* levelRSrc { nullptr };
        std::atomic<float>* driftASrc { nullptr };
        std::atomic<float>* driftBSrc { nullptr };
        std::atomic<float>* widthSrc  { nullptr };

        float lDisp { -72.0f }, rDisp { -72.0f };
        float aDisp { 0.0f }, bDisp { 0.0f };
        float wDisp { 0.0f };

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();

            // Background panel
            g.setColour (col::bg1);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (col::panelEdge);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

            const float pad = 8.0f;
            auto inner = r.reduced (pad, pad);

            // Top: two stacked level bars
            const float barH = 8.0f;
            auto barsArea = inner.removeFromTop (barH * 2.0f + 4.0f);
            auto lBar = barsArea.removeFromTop (barH);
            barsArea.removeFromTop (4.0f);
            auto rBar = barsArea.removeFromTop (barH);

            drawLevelBar (g, lBar, lDisp, "L");
            drawLevelBar (g, rBar, rDisp, "R");

            inner.removeFromTop (8.0f);

            // Bottom: stereo field strip
            auto field = inner;
            g.setColour (col::bg0);
            g.fillRoundedRectangle (field, 4.0f);

            // Centre line
            const float cyLine = field.getCentreY();
            g.setColour (col::gridLine);
            g.drawLine (field.getX() + 4.0f, cyLine, field.getRight() - 4.0f, cyLine, 1.0f);

            // Tick marks at L / C / R
            for (float t : { 0.0f, 0.5f, 1.0f })
            {
                const float tx = field.getX() + 4.0f + t * (field.getWidth() - 8.0f);
                g.setColour (col::textFaint.withAlpha (0.5f));
                g.drawLine (tx, cyLine - 4.0f, tx, cyLine + 4.0f, 1.0f);
            }

            g.setColour (col::textFaint);
            g.setFont (juce::Font (8.5f, juce::Font::bold));
            g.drawText ("L", field.removeFromLeft (12).toNearestInt(),
                        juce::Justification::centredLeft);
            g.drawText ("R", field.removeFromRight (12).toNearestInt(),
                        juce::Justification::centredRight);

            const float fieldX = field.getX() + 2.0f;
            const float fieldW = field.getWidth() - 4.0f;

            // Two voice dots: voice A pans left, voice B pans right; drift
            // jitters their position around their pan centre.
            const float panAmt    = juce::jlimit (0.0f, 1.0f, wDisp);
            const float driftRng  = 0.06f + 0.08f * panAmt;  // visual range
            const float aPan      = 0.5f - 0.5f * panAmt + aDisp * driftRng;
            const float bPan      = 0.5f + 0.5f * panAmt + bDisp * driftRng;

            auto drawDot = [&] (float pan, juce::Colour c, float radius)
            {
                pan = juce::jlimit (0.0f, 1.0f, pan);
                const float dx = fieldX + pan * fieldW;
                const float dy = cyLine;

                for (int i = 4; i > 0; --i)
                {
                    const float rr = radius + (float) i * 1.6f;
                    g.setColour (c.withAlpha (0.05f + 0.09f * (1.0f - (float) i / 4.0f)));
                    g.fillEllipse (dx - rr, dy - rr, rr * 2.0f, rr * 2.0f);
                }
                g.setColour (c);
                g.fillEllipse (dx - radius, dy - radius, radius * 2.0f, radius * 2.0f);
                g.setColour (juce::Colours::white.withAlpha (0.5f));
                g.drawEllipse (dx - radius, dy - radius, radius * 2.0f, radius * 2.0f, 0.7f);
            };

            drawDot (aPan, col::accent,    4.0f);
            drawDot (bPan, col::accentAlt, 4.0f);
        }

    private:
        void drawLevelBar (juce::Graphics& g, juce::Rectangle<float> bar,
                           float db, const juce::String& tag)
        {
            const float vMin = -60.0f, vMax = 0.0f;
            const float t = juce::jlimit (0.0f, 1.0f, (db - vMin) / (vMax - vMin));

            // Tag
            auto tagArea = bar.removeFromLeft (12.0f);
            g.setColour (col::textFaint);
            g.setFont (juce::Font (8.5f, juce::Font::bold));
            g.drawText (tag, tagArea.toNearestInt(), juce::Justification::centredLeft);

            g.setColour (col::bg0);
            g.fillRoundedRectangle (bar, 2.5f);

            auto fill = bar.withWidth (t * bar.getWidth());
            juce::Colour fillCol =
                t > 0.85f ? col::danger
              : t > 0.65f ? col::warn
              :             col::accent;

            juce::ColourGradient fg (fillCol.darker (0.4f), fill.getX(), 0,
                                     fillCol,                fill.getRight(), 0, false);
            g.setGradientFill (fg);
            g.fillRoundedRectangle (fill.reduced (0.5f), 2.0f);
        }
    };

    // -------- About overlay (full-window, click to dismiss) --------
    class AboutOverlay : public juce::Component
    {
    public:
        AboutOverlay()
        {
            closeBtn.setButtonText ("CLOSE");
            closeBtn.onClick = [this] { setVisible (false); };
            addAndMakeVisible (closeBtn);
            setInterceptsMouseClicks (true, true);
        }

        void paint (juce::Graphics& g) override
        {
            // Dark backdrop
            g.setColour (juce::Colours::black.withAlpha (0.82f));
            g.fillAll();

            // Card
            auto card = cardBounds().toFloat();
            g.setColour (col::panel);
            g.fillRoundedRectangle (card, 14.0f);
            g.setColour (col::accent);
            g.drawRoundedRectangle (card.reduced (0.5f), 14.0f, 1.5f);

            // Top accent stripe
            juce::Path topBar;
            topBar.addRoundedRectangle (card.getX() + 1.5f, card.getY() + 1.5f,
                                        card.getWidth() - 3.0f, 4.0f,
                                        14.0f, 14.0f, true, true, false, false);
            g.setColour (col::accent);
            g.fillPath (topBar);

            // -- Text content --
            auto area = cardBounds().reduced (32, 26);
            int  y    = area.getY();

            // Plugin name -- two-tone via AttributedString (no truncation risk)
            {
                juce::Font fTitle (juce::FontOptions().withName ("Helvetica Neue")
                                                      .withHeight (24.0f).withStyle ("Bold"));
                juce::AttributedString as;
                as.setJustification (juce::Justification::centredLeft);
                as.append ("AtiNAMatiC", fTitle, col::text);
                as.append ("-Double",    fTitle, col::accent);
                juce::TextLayout tl;
                tl.createLayout (as, (float) area.getWidth());
                tl.draw (g, juce::Rectangle<float> ((float) area.getX(), (float) y,
                                                     (float) area.getWidth(), 30.0f));
            }

            y += 34;
            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue").withHeight (11.0f)));
            g.setColour (col::textDim);
            g.drawText ("Version 0.1.0   |   Studio double tracking effect",
                        area.withY (y).withHeight (16),
                        juce::Justification::centredLeft, false);

            // Divider
            y += 22;
            g.setColour (col::panelEdge);
            g.fillRect (area.getX(), y, area.getWidth(), 1);
            y += 14;

            // Studio
            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue")
                                                      .withHeight (13.0f).withStyle ("Bold")));
            g.setColour (col::text);
            g.drawText ("Atij 666 Studio", area.withY (y).withHeight (18),
                        juce::Justification::centredLeft, false);
            y += 22;
            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue").withHeight (11.0f)));
            g.setColour (col::textDim);
            g.drawText ("Designed & developed by Atij",
                        area.withY (y).withHeight (16),
                        juce::Justification::centredLeft, false);

            // Divider
            y += 26;
            g.setColour (col::panelEdge);
            g.fillRect (area.getX(), y, area.getWidth(), 1);
            y += 14;

            // Built with
            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue")
                                                      .withHeight (10.0f).withStyle ("Bold")));
            g.setColour (col::textFaint);
            g.drawText ("BUILT WITH & GRATEFUL TO",
                        area.withY (y).withHeight (14),
                        juce::Justification::centredLeft, false);
            y += 18;

            const juce::StringArray credits {
                "JUCE Framework  --  Raw Material Software Ltd.",
                "VST3 SDK  --  Steinberg Media Technologies GmbH",
                "Neural Amp Modeler (NAM)  --  Steven Atkinson & contributors",
                "The open-source audio & guitar-plugin community"
            };

            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue").withHeight (11.0f)));
            g.setColour (col::textDim);
            for (const auto& credit : credits)
            {
                g.drawText ("*  " + credit,
                            area.withY (y).withHeight (16),
                            juce::Justification::centredLeft, false);
                y += 17;
            }

            // Thank you
            y += 10;
            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue").withHeight (10.0f)));
            g.setColour (col::textFaint);
            g.drawText ("Thank you to everyone who makes open-source audio tools possible.",
                        area.withY (y).withHeight (14),
                        juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            auto card = cardBounds();
            closeBtn.setBounds (card.getRight() - 86, card.getBottom() - 38, 74, 24);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! cardBounds().contains (e.getPosition()))
                setVisible (false);
        }

    private:
        juce::Rectangle<int> cardBounds() const
        {
            return getLocalBounds().reduced (44, 56);
        }

        juce::TextButton closeBtn;
    };

    // -------- Title badge (two-colour clickable title) --------
    class TitleBadge : public juce::Component
    {
    public:
        std::function<void()> onClick;

        void paint (juce::Graphics& g) override
        {
            juce::Font f (juce::FontOptions().withName ("Helvetica Neue")
                                             .withHeight (22.0f).withStyle ("Bold"));
            juce::AttributedString as;
            as.setJustification (juce::Justification::centredLeft);
            as.append ("AtiNAMatiC", f, col::text.withAlpha (0.88f));
            as.append ("-Double",    f, col::accent);
            juce::TextLayout tl;
            tl.createLayout (as, (float) getWidth());
            tl.draw (g, getLocalBounds().toFloat());
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            if (onClick) onClick();
        }

        // Show a hand cursor to hint at interactivity
        juce::MouseCursor getMouseCursor() override
        {
            return juce::MouseCursor::PointingHandCursor;
        }
    };
}
