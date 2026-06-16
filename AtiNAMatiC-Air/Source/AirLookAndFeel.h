#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace na
{
    // -------- Palette (cool / airy) --------
    namespace col
    {
        static const juce::Colour bg0       (0xff0e131a);
        static const juce::Colour bg1       (0xff182230);
        static const juce::Colour panel     (0xff1a2433);
        static const juce::Colour panelEdge (0xff2a3848);
        static const juce::Colour text      (0xffeef3f9);
        static const juce::Colour textDim   (0xffd0dae8);   // was 8e9bb0 — bumped up
        static const juce::Colour textFaint (0xffaab6c8);   // was 5c6a80 — bumped up
        static const juce::Colour accent    (0xffff5fa8);   // electric magenta — modern "air"
        static const juce::Colour accentLo  (0xff7a2c54);
        static const juce::Colour ok        (0xff5dd28d);
        static const juce::Colour warn      (0xffe8a64d);
        static const juce::Colour danger    (0xffe25b5b);
        static const juce::Colour gridLine  (0xff232f3e);
    }

    // -------- LookAndFeel --------
    class AirLNF : public juce::LookAndFeel_V4
    {
    public:
        AirLNF()
        {
            setColour (juce::Slider::rotarySliderFillColourId,    col::accent);
            setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff202831));
            setColour (juce::Slider::thumbColourId,               col::accent);
            setColour (juce::Slider::textBoxTextColourId,         col::text);
            setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);

            setColour (juce::Label::textColourId,                 col::text);
            setColour (juce::TextButton::buttonColourId,          juce::Colour (0xff202934));
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

            const auto trackThickness = juce::jmax (4.0f, radius * 0.10f);
            const auto trackR = radius - trackThickness * 0.5f;

            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawEllipse (cx - radius, cy - radius + 2.0f, radius * 2.0f, radius * 2.0f, 1.0f);

            juce::Path track;
            track.addCentredArc (cx, cy, trackR, trackR, 0.0f, startAngle, endAngle, true);
            g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
            g.strokePath (track, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved));

            const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
            juce::Path filled;
            filled.addCentredArc (cx, cy, trackR, trackR, 0.0f, startAngle, angle, true);
            juce::ColourGradient grad (accent.darker (0.6f), cx - trackR, cy,
                                       accent.brighter (0.1f), cx + trackR, cy, false);
            g.setGradientFill (grad);
            g.strokePath (filled, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            // Outer glow that intensifies with value (the "air halo").
            const auto inner = radius - trackThickness * 1.7f;
            const auto glow  = juce::jlimit (0.0f, 1.0f, sliderPos);
            for (int i = 12; i > 0; --i)
            {
                const float r = inner + (float) i * 1.0f;
                const float a = (0.04f + 0.16f * glow) * (1.0f - (float) i / 12.0f);
                g.setColour (accent.withAlpha (a));
                g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
            }

            for (int t = 0; t <= 4; ++t)
            {
                const float ta = startAngle + (float) t / 4.0f * (endAngle - startAngle);
                const float r1 = inner + 1.0f;
                const float r2 = inner + 4.0f;
                const float sx1 = cx + std::sin (ta) * r1;
                const float sy1 = cy - std::cos (ta) * r1;
                const float sx2 = cx + std::sin (ta) * r2;
                const float sy2 = cy - std::cos (ta) * r2;
                g.setColour (col::textFaint.withAlpha (0.6f));
                g.drawLine (sx1, sy1, sx2, sy2, 1.0f);
            }

            juce::ColourGradient faceGrad (juce::Colour (0xff223040), cx, cy - inner,
                                           juce::Colour (0xff0e131a), cx, cy + inner, false);
            g.setGradientFill (faceGrad);
            g.fillEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f);

            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f, 1.0f);

            juce::Path pointer;
            const float pw = juce::jmax (3.0f, inner * 0.10f);
            const float pl = inner * 0.78f;
            pointer.addRoundedRectangle (-pw * 0.5f, -inner * 0.92f, pw, pl, pw * 0.5f);
            const auto rot = juce::AffineTransform::rotation (angle).translated (cx, cy);

            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillPath (pointer, rot.translated (0.0f, 1.5f));
            g.setColour (accent.brighter (0.3f));
            g.fillPath (pointer, rot);

            const float dotR = juce::jmax (2.0f, inner * 0.07f);
            g.setColour (juce::Colour (0xff0d1117));
            g.fillEllipse (cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
            g.setColour (accent.withAlpha (0.4f));
            g.drawEllipse (cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f, 1.0f);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                   const juce::Colour& backgroundColour,
                                   bool, bool shouldDrawButtonAsDown) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            const bool on = b.getToggleState();
            juce::Colour fill = on ? col::accent : backgroundColour;
            if (shouldDrawButtonAsDown) fill = fill.darker (0.15f);

            g.setColour (fill);
            g.fillRoundedRectangle (r, 4.0f);

            if (! on)
            {
                g.setColour (col::panelEdge);
                g.drawRoundedRectangle (r, 4.0f, 1.0f);
            }
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
        {
            const bool on = b.getToggleState();
            g.setFont (juce::Font ("Helvetica Neue", 11.0f, juce::Font::bold));
            g.setColour (on ? juce::Colours::black : col::text);
            g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2),
                              juce::Justification::centred, 1);
        }
    };

    // -------- Mode picker (Studio / Room) --------
    class ModePicker : public juce::Component
    {
    public:
        std::function<void (int)> onChange;

        ModePicker()
        {
            for (int i = 0; i < labels.size(); ++i)
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
            ix = juce::jlimit (0, buttons.size() - 1, ix);
            buttons[ix]->setToggleState (true, juce::dontSendNotification);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            const int w = r.getWidth() / buttons.size();
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setBounds (r.removeFromLeft (i == buttons.size() - 1 ? r.getWidth() : w));
        }

    private:
        const juce::StringArray labels { "STUDIO", "ROOM" };
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
            g.setColour (col::textFaint);
            g.setFont (juce::Font ("Helvetica Neue", 10.0f, juce::Font::bold));
            g.drawText (text.toUpperCase(),
                        r.removeFromTop (14.0f).toNearestInt(),
                        juce::Justification::centred);
            auto line = r.withSizeKeepingCentre (r.getWidth() * 0.35f, 1.0f);
            g.setColour (col::panelEdge);
            g.fillRect (line);
        }
    };

    // -------- Activity meter --------
    // Top : input level. Bottom : output level, tinted by air amount.
    class AirMeter : public juce::Component
    {
    public:
        std::atomic<float>* inDbSrc  { nullptr };
        std::atomic<float>* outDbSrc { nullptr };
        std::atomic<float>* airSrc   { nullptr };

        float inDbDisp  { -60.0f };
        float outDbDisp { -60.0f };
        float airDisp   {   0.0f };

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();

            g.setColour (col::bg1);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (col::panelEdge);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

            const float pad    = 8.0f;
            const float scaleH = 14.0f;
            auto inner = r.reduced (pad, pad);
            const float bandH = (inner.getHeight() - 8.0f - scaleH) * 0.5f;
            auto inBand  = inner.removeFromTop (bandH);
            inner.removeFromTop (8.0f);
            auto outBand = inner.removeFromTop (bandH);

            const float vMin = -60.0f, vMax = 0.0f;
            auto map = [&] (juce::Rectangle<float> band, float db)
            {
                const float t = juce::jlimit (0.0f, 1.0f, (db - vMin) / (vMax - vMin));
                return band.getX() + t * band.getWidth();
            };

            auto drawBand = [&] (juce::Rectangle<float> band, float db,
                                 juce::Colour fillCol, const char* label)
            {
                g.setColour (col::bg0);
                g.fillRoundedRectangle (band, 3.0f);
                for (float dB = -48.0f; dB <= 0.0f; dB += 12.0f)
                {
                    const float gx = map (band, dB);
                    g.setColour (col::gridLine);
                    g.drawLine (gx, band.getY() + 1.0f, gx, band.getBottom() - 1.0f, 0.5f);
                }
                const float xv = map (band, juce::jlimit (vMin, vMax, db));
                auto fill = juce::Rectangle<float> (band.getX(), band.getY(),
                                                    xv - band.getX(), band.getHeight());
                juce::ColourGradient fg (fillCol.darker (0.3f), fill.getX(), 0,
                                         fillCol,                fill.getRight(), 0, false);
                g.setGradientFill (fg);
                g.fillRoundedRectangle (fill.reduced (0.5f), 2.5f);

                g.setColour (col::text);
                g.setFont (juce::Font (10.0f, juce::Font::bold));
                g.drawText (label, band.translated (4.0f, 0.0f).toNearestInt(),
                            juce::Justification::centredLeft);
                g.setFont (juce::Font (10.0f));
                g.setColour (col::text);
                g.drawText (juce::String (juce::roundToInt (db)) + " dB",
                            band.translated (-6.0f, 0.0f).toNearestInt(),
                            juce::Justification::centredRight);
            };

            drawBand (inBand, inDbDisp, juce::Colour (0xff5a6680), "INPUT");

            const float s = juce::jlimit (0.0f, 1.0f, airDisp);
            const auto outCol = juce::Colour (0xff5a6680).interpolatedWith (col::accent, s);
            drawBand (outBand, outDbDisp, outCol, "OUTPUT");

            g.setColour (col::text);
            g.setFont (juce::Font (9.0f));
            g.drawText ("-60   -48   -36   -24   -12   0 dB",
                        juce::Rectangle<float> (inner.getX(), inner.getBottom() - scaleH + 2.0f,
                                                inner.getWidth(), scaleH).toNearestInt(),
                        juce::Justification::centred);
        }
    };

    // -------- Air pill (LED-style activity indicator) --------
    class AirPill : public juce::Component
    {
    public:
        float airDisp { 0.0f };

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (col::panel.darker (0.3f));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (col::panelEdge);
            g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);

            const float s = juce::jlimit (0.0f, 1.0f, airDisp);
            const char* txt = nullptr;
            if      (s < 0.10f) txt = "DRY";
            else if (s < 0.40f) txt = "LIFT";
            else if (s < 0.70f) txt = "OPEN";
            else                txt = "WIDE";

            // LED is always accent cyan — alpha scales from 0.55 (DRY) to 1.0 (WIDE)
            // so DRY still looks clearly lit. No white overdraw: it washes out the colour.
            const float dotAlpha = 0.55f + 0.45f * s;
            const auto  ledCol   = col::accent;

            const float dotR = r.getHeight() * 0.22f;
            const float dotX = r.getX() + r.getHeight() * 0.45f;
            const float dotY = r.getCentreY();

            // Glow — base 0.35 so DRY halo is clearly visible
            for (int i = 4; i > 0; --i)
            {
                const float rr = dotR + (float) i * (1.5f + 2.0f * s);
                g.setColour (ledCol.withAlpha ((0.35f + 0.30f * s) * (1.0f - (float) i / 4.0f)));
                g.fillEllipse (dotX - rr, dotY - rr, rr * 2.0f, rr * 2.0f);
            }
            g.setColour (ledCol.withAlpha (dotAlpha));
            g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

            g.setColour (col::text);
            g.setFont (juce::Font ("Helvetica Neue", r.getHeight() * 0.42f, juce::Font::bold));
            auto txtArea = r.withTrimmedLeft (r.getHeight() * 0.85f).withTrimmedRight (8.0f);
            g.drawFittedText (txt, txtArea.toNearestInt(),
                              juce::Justification::centredLeft, 1);
        }
    };

    // ======================================================================
    // About overlay — shown when the title badge is clicked.
    // Covers the full editor with a dark backdrop and a card in the centre.
    // ======================================================================
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

            // ── Text content ──────────────────────────────────────────────
            auto area = cardBounds().reduced (32, 26);
            int  y    = area.getY();

            // Plugin name — two-tone
            juce::Font fTitle ("Helvetica Neue", 24.0f, juce::Font::bold);
            g.setFont (fTitle);
            g.setColour (col::text);
            g.drawText ("AtiNAMatiC", area.withY (y).withHeight (30),
                        juce::Justification::centredLeft, false);

            const int xAir = area.getX() + (int) std::round (fTitle.getStringWidthFloat ("AtiNAMatiC"));
            g.setColour (col::accent);
            g.drawText ("-Air", juce::Rectangle<int> (xAir, y, 70, 30),
                        juce::Justification::centredLeft, false);

            y += 34;
            g.setFont (juce::Font ("Helvetica Neue", 11.0f, juce::Font::plain));
            g.setColour (col::textDim);
            g.drawText ("Version 0.1.0   |   Cab feel & amp-in-room enhancer",
                        area.withY (y).withHeight (16),
                        juce::Justification::centredLeft, false);

            // Divider
            y += 22;
            g.setColour (col::panelEdge);
            g.fillRect (area.getX(), y, area.getWidth(), 1);
            y += 14;

            // Studio
            g.setFont (juce::Font ("Helvetica Neue", 13.0f, juce::Font::bold));
            g.setColour (col::text);
            g.drawText ("Atij 666 Studio", area.withY (y).withHeight (18),
                        juce::Justification::centredLeft, false);
            y += 22;
            g.setFont (juce::Font ("Helvetica Neue", 11.0f, juce::Font::plain));
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
            g.setFont (juce::Font ("Helvetica Neue", 10.0f, juce::Font::bold));
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

            g.setFont (juce::Font ("Helvetica Neue", 11.0f, juce::Font::plain));
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
            g.setFont (juce::Font ("Helvetica Neue", 10.0f, juce::Font::italic));
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

    // ======================================================================
    // TitleBadge — draws "AtiNAMatiC" + "-Air" in two colours, single line,
    // no gap.  Click opens the About overlay.
    // ======================================================================
    class TitleBadge : public juce::Component
    {
    public:
        std::function<void()> onClick;

        TitleBadge() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        void paint (juce::Graphics& g) override
        {
            juce::Font f ("Helvetica Neue", 22.0f, juce::Font::bold);
            g.setFont (f);

            const float a = hovered ? 1.0f : 0.88f;

            g.setColour (col::text.withMultipliedAlpha (a));
            g.drawText ("AtiNAMatiC", getLocalBounds(),
                        juce::Justification::centredLeft, false);

            const int xOff = (int) std::round (f.getStringWidthFloat ("AtiNAMatiC"));
            g.setColour (col::accent.withMultipliedAlpha (a));
            g.drawText ("-Air", getLocalBounds().withTrimmedLeft (xOff),
                        juce::Justification::centredLeft, false);

            if (hovered)
            {
                const int totalW = xOff + (int) std::round (f.getStringWidthFloat ("-Air"));
                g.setColour (col::accent.withAlpha (0.55f));
                g.fillRect (0, getHeight() - 2, totalW, 1);
            }
        }

        void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }
        void mouseUp    (const juce::MouseEvent&) override { if (onClick) onClick(); }

    private:
        bool hovered { false };
    };
}
