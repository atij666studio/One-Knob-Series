#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <cmath>

namespace np
{
    // -------- Palette: cool, "high-tech" cyan/teal -----------------------
    namespace col
    {
        static const juce::Colour bg0       (0xff0c1118);
        static const juce::Colour bg1       (0xff141b27);
        static const juce::Colour panel     (0xff182030);
        static const juce::Colour panelEdge (0xff263048);
        static const juce::Colour text      (0xffeaf3ff);
        static const juce::Colour textDim   (0xff8995ad);
        static const juce::Colour textFaint (0xff5b6478);
        static const juce::Colour accent    (0xff35d07a);   // emerald green — "fresh attack"
        static const juce::Colour accentSoft(0xff7a8cff);   // soft side hint (cool indigo)
        static const juce::Colour accentSharp(0xffffb84d);  // sharp side hint (warm amber)
        static const juce::Colour gridLine  (0xff222a3a);
    }

    // -------- LookAndFeel: bipolar rotary --------------------------------
    // The big ATTACK knob is bipolar (Soft @ left, Neutral @ centre, Sharp
    // @ right). The LNF draws the filled arc starting from 12 o'clock so
    // the user sees how far either side of neutral they are.
    class PickLNF : public juce::LookAndFeel_V4
    {
    public:
        PickLNF()
        {
            setColour (juce::Slider::rotarySliderFillColourId,    col::accent);
            setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff1d2433));
            setColour (juce::Slider::thumbColourId,               col::accent);
            setColour (juce::Slider::textBoxTextColourId,         col::text);
            setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);

            setColour (juce::Label::textColourId, col::text);
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

            const bool bipolar = slider.getProperties().getWithDefault ("bipolar", false);

            const auto trackThickness = juce::jmax (4.0f, radius * 0.10f);
            const auto trackR = radius - trackThickness * 0.5f;

            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawEllipse (cx - radius, cy - radius + 2.0f, radius * 2.0f, radius * 2.0f, 1.0f);

            // Background track.
            juce::Path track;
            track.addCentredArc (cx, cy, trackR, trackR, 0.0f, startAngle, endAngle, true);
            g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
            g.strokePath (track, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved));

            // Filled track. Bipolar fills from 12 o'clock outward; otherwise
            // from start to current.
            const auto centreAngle = (startAngle + endAngle) * 0.5f;
            const float arcStart = bipolar ? centreAngle : startAngle;
            const float arcEnd   = angle;

            // Pick colour: bipolar interpolates soft<->accent<->sharp.
            juce::Colour fillCol = slider.findColour (juce::Slider::rotarySliderFillColourId);
            if (bipolar)
            {
                const float t = (sliderPos - 0.5f) * 2.0f; // -1..+1
                if (t >= 0.0f) fillCol = col::accent.interpolatedWith (col::accentSharp, t);
                else            fillCol = col::accent.interpolatedWith (col::accentSoft, -t);
            }

            juce::Path filled;
            const float a0 = juce::jmin (arcStart, arcEnd);
            const float a1 = juce::jmax (arcStart, arcEnd);
            filled.addCentredArc (cx, cy, trackR, trackR, 0.0f, a0, a1, true);
            juce::ColourGradient grad (fillCol.darker (0.5f), cx - trackR, cy,
                                       fillCol.brighter (0.15f), cx + trackR, cy, false);
            g.setGradientFill (grad);
            g.strokePath (filled, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            // Centre tick (only on bipolar) so neutral is visually obvious.
            if (bipolar)
            {
                const float tickR1 = trackR + trackThickness * 0.55f;
                const float tickR2 = trackR + trackThickness * 1.4f;
                const float tx1 = cx + std::sin (centreAngle) * tickR1;
                const float ty1 = cy - std::cos (centreAngle) * tickR1;
                const float tx2 = cx + std::sin (centreAngle) * tickR2;
                const float ty2 = cy - std::cos (centreAngle) * tickR2;
                g.setColour (col::textFaint);
                g.drawLine (tx1, ty1, tx2, ty2, 1.5f);
            }

            // Inner halo.
            const auto inner = radius - trackThickness * 1.7f;
            const float halo = bipolar ? std::abs (sliderPos - 0.5f) * 2.0f
                                       : juce::jlimit (0.0f, 1.0f, sliderPos);
            for (int i = 12; i > 0; --i)
            {
                const float r = inner + (float) i * 1.0f;
                const float a = (0.04f + 0.18f * halo) * (1.0f - (float) i / 12.0f);
                g.setColour (fillCol.withAlpha (a));
                g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
            }

            // Tick marks around track.
            const int nTicks = bipolar ? 5 : 5;
            for (int t = 0; t <= nTicks - 1; ++t)
            {
                const float ta = startAngle + (float) t / (float) (nTicks - 1) * (endAngle - startAngle);
                const float r1 = inner + 1.0f;
                const float r2 = inner + 4.0f;
                const float sx1 = cx + std::sin (ta) * r1;
                const float sy1 = cy - std::cos (ta) * r1;
                const float sx2 = cx + std::sin (ta) * r2;
                const float sy2 = cy - std::cos (ta) * r2;
                g.setColour (col::textFaint.withAlpha (0.55f));
                g.drawLine (sx1, sy1, sx2, sy2, 1.0f);
            }

            // Knob face.
            juce::ColourGradient faceGrad (juce::Colour (0xff1f283a), cx, cy - inner,
                                           juce::Colour (0xff0c1118), cx, cy + inner, false);
            g.setGradientFill (faceGrad);
            g.fillEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f);

            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f, 1.0f);

            // Pointer.
            juce::Path pointer;
            const float pw = juce::jmax (3.0f, inner * 0.10f);
            const float pl = inner * 0.78f;
            pointer.addRoundedRectangle (-pw * 0.5f, -inner * 0.92f, pw, pl, pw * 0.5f);
            const auto rot = juce::AffineTransform::rotation (angle).translated (cx, cy);

            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillPath (pointer, rot.translated (0.0f, 1.5f));
            g.setColour (fillCol.brighter (0.3f));
            g.fillPath (pointer, rot);

            const float dotR = juce::jmax (2.0f, inner * 0.07f);
            g.setColour (juce::Colour (0xff0a0e15));
            g.fillEllipse (cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
            g.setColour (fillCol.withAlpha (0.4f));
            g.drawEllipse (cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f, 1.0f);
        }
    };

    // -------- TitleBadge (click to open About) ---------------------------
    class TitleBadge : public juce::Component
    {
    public:
        std::function<void()> onClick;

        void paint (juce::Graphics& g) override
        {
            juce::Font f (juce::FontOptions().withName ("Helvetica Neue")
                                             .withHeight (22.0f)
                                             .withStyle ("Bold"));
            juce::AttributedString as;
            as.setJustification (juce::Justification::centredLeft);
            as.append ("AtiNAMatiC", f, col::text.withAlpha (0.70f));
            as.append ("-Pick",      f, col::accent);
            juce::TextLayout tl;
            tl.createLayout (as, (float) getWidth());
            tl.draw (g, getLocalBounds().toFloat());
        }

        void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

        juce::MouseCursor getMouseCursor() override
        {
            return juce::MouseCursor::PointingHandCursor;
        }
    };

    // -------- About overlay (click outside card or CLOSE to dismiss) -----
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

            // Plugin name -- two-tone via AttributedString
            {
                juce::Font fTitle (juce::FontOptions().withName ("Helvetica Neue")
                                                      .withHeight (24.0f).withStyle ("Bold"));
                juce::AttributedString as;
                as.setJustification (juce::Justification::centredLeft);
                as.append ("AtiNAMatiC", fTitle, col::text);
                as.append ("-Pick",      fTitle, col::accent);
                juce::TextLayout tl;
                tl.createLayout (as, (float) area.getWidth());
                tl.draw (g, juce::Rectangle<float> ((float) area.getX(), (float) y,
                                                     (float) area.getWidth(), 30.0f));
            }

            y += 34;
            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue").withHeight (11.0f)));
            g.setColour (col::textDim);
            g.drawText ("Version 0.1.0   |   Pick attack designer for guitar",
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

    // -------- Section heading -------------------------------------------
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

    // -------- Transient activity LED bar --------------------------------
    // A horizontal bar that lights up briefly with each pick attack
    // detected. Tinted with the accent colour shifted toward soft/sharp
    // hue depending on attack direction.
    class TransientPill : public juce::Component
    {
    public:
        std::atomic<float>* transientSrc { nullptr };
        float disp { 0.0f };
        float direction { 0.0f }; // -1 soft .. +1 sharp

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (col::bg0);
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (col::panelEdge);
            g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);

            const float t = juce::jlimit (0.0f, 1.0f, disp);
            juce::Colour c = col::accent;
            if (direction >= 0.0f) c = col::accent.interpolatedWith (col::accentSharp, direction);
            else                   c = col::accent.interpolatedWith (col::accentSoft, -direction);

            const float pad = 4.0f;
            auto inner = r.reduced (pad, pad);
            const float w = inner.getWidth() * t;
            auto fill = juce::Rectangle<float> (inner.getX(), inner.getY(),
                                                w, inner.getHeight());
            juce::ColourGradient grad (c.darker (0.4f), fill.getX(), 0,
                                       c.brighter (0.15f), fill.getRight(), 0, false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill, fill.getHeight() * 0.5f);

            // Glow.
            for (int i = 4; i > 0; --i)
            {
                const float a = (0.05f + 0.18f * t) * (1.0f - (float) i / 4.0f);
                g.setColour (c.withAlpha (a));
                g.drawRoundedRectangle (r.expanded ((float) i, (float) i),
                                        r.getHeight() * 0.5f + (float) i, 1.0f);
            }

            g.setColour (col::textFaint);
            g.setFont (juce::Font ("Helvetica Neue", r.getHeight() * 0.42f, juce::Font::bold));
            g.drawText ("TRANSIENT", r.toNearestInt(), juce::Justification::centred);
        }
    };
}
