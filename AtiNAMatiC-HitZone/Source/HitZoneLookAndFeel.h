#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace nhz
{
    namespace col
    {
        const juce::Colour bgTop    { 0xFF161D26 };
        const juce::Colour bgBot    { 0xFF0B1017 };
        const juce::Colour panel    { 0xFF1A222E };
        const juce::Colour panelHi  { 0xFF222C3B };
        const juce::Colour stroke   { 0xFF2A3548 };
        const juce::Colour text     { 0xFFFFFFFF };
        const juce::Colour textDim  { 0xFFC8D2E0 };
        const juce::Colour textFaint{ 0xFF9DA8BC };
        const juce::Colour textMute { 0xFF9DA8BC };

        const juce::Colour cold     { 0xFF6E80FF };
        const juce::Colour mid      { 0xFF7AC0FF };
        const juce::Colour hot      { 0xFFFF8A4C };
        const juce::Colour green    { 0xFF6FE39A };
        const juce::Colour red      { 0xFFFF5A4C };
        const juce::Colour accent   { 0xFF7AC0FF };
    }

    inline juce::Colour mixCold (float pos)
    {
        const float p = juce::jlimit (0.0f, 1.0f, pos);
        if (p < 0.5f) return col::cold.interpolatedWith (col::mid, p * 2.0f);
        return col::mid.interpolatedWith (col::hot, (p - 0.5f) * 2.0f);
    }

    class HitZoneLNF : public juce::LookAndFeel_V4
    {
    public:
        HitZoneLNF()
        {
            setColour (juce::Slider::rotarySliderFillColourId,    col::mid);
            setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xFF202833));
            setColour (juce::Slider::thumbColourId,               juce::Colours::white);
            setColour (juce::Label::textColourId,                 col::textDim);
            setColour (juce::ToggleButton::textColourId,          col::text);
            setColour (juce::ToggleButton::tickColourId,          col::mid);
            setColour (juce::ToggleButton::tickDisabledColourId,  juce::Colour (0xFF40505F));
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                               float pos, float startAng, float endAng,
                               juce::Slider& s) override
        {
            const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY();
            const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

            const bool   bigKnob = w >= 160;
            const float  ringW   = juce::jmax (4.0f, radius * (bigKnob ? 0.085f : 0.11f));
            const float  arcR    = radius - ringW * 0.5f;
            const float  ang     = startAng + pos * (endAng - startAng);
            const auto   fill    = mixCold (pos);

            // Outer drop shadow
            {
                juce::DropShadow ds (juce::Colours::black.withAlpha (0.55f), 12, { 0, 4 });
                juce::Path shadowP;
                shadowP.addEllipse (cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f);
                ds.drawForPath (g, shadowP);
            }

            // Background ring
            juce::Path bg;
            bg.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAng, endAng, true);
            g.setColour (juce::Colour (0xFF1A2230));
            g.strokePath (bg, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Tick marks around the ring (subtle)
            if (bigKnob)
            {
                g.setColour (juce::Colour (0xFF3A4658));
                const int ticks = 11;
                for (int i = 0; i < ticks; ++i)
                {
                    const float t = (float) i / (float) (ticks - 1);
                    const float a = startAng + t * (endAng - startAng);
                    const float r0 = arcR + ringW * 0.55f;
                    const float r1 = r0 + 4.0f;
                    juce::Path p;
                    p.startNewSubPath (cx + std::sin (a) * r0, cy - std::cos (a) * r0);
                    p.lineTo          (cx + std::sin (a) * r1, cy - std::cos (a) * r1);
                    g.strokePath (p, juce::PathStrokeType (1.2f));
                }
            }

            // Glow behind value arc
            {
                juce::Path val;
                val.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAng, ang, true);
                g.setColour (fill.withAlpha (0.20f));
                g.strokePath (val, juce::PathStrokeType (ringW + 6.0f,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
                g.setColour (fill);
                g.strokePath (val, juce::PathStrokeType (ringW,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
            }

            // Knob body — radial gradient for a slightly 3D feel
            const float bodyR = arcR - ringW * 1.4f;
            juce::ColourGradient body (juce::Colour (0xFF323F52), cx, cy - bodyR * 0.6f,
                                       juce::Colour (0xFF0E141D), cx, cy + bodyR, false);
            body.isRadial = true;
            body.point1   = { cx, cy - bodyR * 0.5f };
            body.point2   = { cx, cy + bodyR };
            g.setGradientFill (body);
            g.fillEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

            // Inner highlight
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawEllipse (cx - bodyR + 2.0f, cy - bodyR + 2.0f,
                           bodyR * 2.0f - 4.0f, bodyR * 2.0f - 4.0f, 1.0f);

            // Outline
            g.setColour (juce::Colour (0xFF050810));
            g.drawEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.5f);

            // Pointer
            const float pLen = bodyR * 0.82f;
            const float pW   = juce::jmax (2.5f, bodyR * 0.075f);
            juce::Path p;
            p.addRoundedRectangle (-pW * 0.5f, -pLen, pW, pLen * 0.96f, pW * 0.5f);

            // Pointer glow
            juce::DropShadow ptrGlow (fill.withAlpha (0.85f), 8, {});
            juce::Path glowP = p;
            glowP.applyTransform (juce::AffineTransform::rotation (ang).translated (cx, cy));
            ptrGlow.drawForPath (g, glowP);

            g.setColour (fill.brighter (0.6f));
            g.fillPath (p, juce::AffineTransform::rotation (ang).translated (cx, cy));

            // Center cap
            const float capR = bodyR * 0.18f;
            g.setColour (juce::Colour (0xFF0A0E14));
            g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
            g.setColour (fill.withAlpha (0.6f));
            g.drawEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);

            juce::ignoreUnused (s);
        }

        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                               bool /*shouldDrawButtonAsHighlighted*/,
                               bool /*shouldDrawButtonAsDown*/) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            const bool on = b.getToggleState();

            // Pill background
            const float h = r.getHeight();
            const float pillW = h * 1.9f;
            auto pill = juce::Rectangle<float> (r.getX(), r.getY(), pillW, h);

            g.setColour (on ? col::mid.withAlpha (0.85f) : juce::Colour (0xFF1F2A38));
            g.fillRoundedRectangle (pill, h * 0.5f);
            g.setColour (juce::Colour (0xFF0A0F16));
            g.drawRoundedRectangle (pill, h * 0.5f, 1.0f);

            // Knob inside pill
            const float kR = h * 0.42f;
            const float kx = on ? pill.getRight() - kR * 2.0f - 3.0f : pill.getX() + 3.0f;
            const float ky = pill.getCentreY() - kR;
            juce::DropShadow ds (juce::Colours::black.withAlpha (0.45f), 4, { 0, 1 });
            juce::Path kp;
            kp.addEllipse (kx, ky, kR * 2.0f, kR * 2.0f);
            ds.drawForPath (g, kp);
            g.setColour (juce::Colours::white);
            g.fillEllipse (kx, ky, kR * 2.0f, kR * 2.0f);

            // Label
            auto textArea = r.withTrimmedLeft (pillW + 10.0f);
            g.setColour (col::text);
            g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
            g.drawText (b.getButtonText(), textArea.toNearestInt(), juce::Justification::centredLeft);
        }

        juce::Font getLabelFont (juce::Label& l) override
        {
            return l.getFont();
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
                g.setColour (col::stroke);
                g.drawRoundedRectangle (r, 4.0f, 1.0f);
            }
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
        {
            const bool on = b.getToggleState();
            g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
            g.setColour (on ? juce::Colours::black : col::text);
            g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2),
                              juce::Justification::centred, 1);
        }
    };

    // ======================================================================
    // About overlay -- shown when the title badge is clicked.
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
            g.setColour (juce::Colours::black.withAlpha (0.82f));
            g.fillAll();

            auto card = cardBounds().toFloat();
            g.setColour (col::panel);
            g.fillRoundedRectangle (card, 14.0f);
            g.setColour (col::accent);
            g.drawRoundedRectangle (card.reduced (0.5f), 14.0f, 1.5f);

            juce::Path topBar;
            topBar.addRoundedRectangle (card.getX() + 1.5f, card.getY() + 1.5f,
                                        card.getWidth() - 3.0f, 4.0f,
                                        14.0f, 14.0f, true, true, false, false);
            g.setColour (col::accent);
            g.fillPath (topBar);

            auto area = cardBounds().reduced (32, 26);
            int  y    = area.getY();

            juce::Font fTitle (juce::FontOptions (24.0f).withStyle ("Bold"));
            g.setFont (fTitle);
            g.setColour (col::text);
            g.drawText ("AtiNAMatiC", area.withY (y).withHeight (30),
                        juce::Justification::centredLeft, false);

            {
                juce::GlyphArrangement ga;
                ga.addLineOfText (fTitle, "AtiNAMatiC", 0.0f, 0.0f);
                const int xSuffix = area.getX() + (int) std::round (ga.getBoundingBox (0, -1, true).getWidth());
                g.setColour (col::accent);
                g.drawText ("-HitZone", juce::Rectangle<int> (xSuffix, y, 120, 30),
                            juce::Justification::centredLeft, false);
            }

            y += 34;
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.setColour (col::textDim);
            g.drawText ("Version 0.1.0   |   Always hit the perfect level for NAM",
                        area.withY (y).withHeight (16), juce::Justification::centredLeft, false);

            y += 22;
            g.setColour (col::stroke);
            g.fillRect (area.getX(), y, area.getWidth(), 1);
            y += 14;

            g.setFont (juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")));
            g.setColour (col::text);
            g.drawText ("Atij 666 Studio", area.withY (y).withHeight (18),
                        juce::Justification::centredLeft, false);
            y += 22;
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.setColour (col::textDim);
            g.drawText ("Designed & developed by Atij",
                        area.withY (y).withHeight (16), juce::Justification::centredLeft, false);

            y += 26;
            g.setColour (col::stroke);
            g.fillRect (area.getX(), y, area.getWidth(), 1);
            y += 14;

            g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
            g.setColour (col::textFaint);
            g.drawText ("BUILT WITH & GRATEFUL TO",
                        area.withY (y).withHeight (14), juce::Justification::centredLeft, false);
            y += 18;

            const juce::StringArray credits {
                "JUCE Framework  --  Raw Material Software Ltd.",
                "VST3 SDK  --  Steinberg Media Technologies GmbH",
                "Neural Amp Modeler (NAM)  --  Steven Atkinson & contributors",
                "The open-source audio & guitar-plugin community"
            };

            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.setColour (col::textDim);
            for (const auto& credit : credits)
            {
                g.drawText ("*  " + credit,
                            area.withY (y).withHeight (16), juce::Justification::centredLeft, false);
                y += 17;
            }

            y += 10;
            g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Italic")));
            g.setColour (col::textFaint);
            g.drawText ("Thank you to everyone who makes open-source audio tools possible.",
                        area.withY (y).withHeight (14), juce::Justification::centredLeft, false);
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
    // TitleBadge -- draws "AtiNAMatiC" + "-HitZone" in two colours, clickable.
    // ======================================================================
    class TitleBadge : public juce::Component
    {
    public:
        std::function<void()> onClick;

        TitleBadge() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        void paint (juce::Graphics& g) override
        {
            juce::Font f (juce::FontOptions (22.0f).withStyle ("Bold"));
            g.setFont (f);

            const float a = hovered ? 1.0f : 0.88f;

            auto strW = [] (const juce::Font& font, const juce::String& s) -> int
            {
                juce::GlyphArrangement ga;
                ga.addLineOfText (font, s, 0.0f, 0.0f);
                return (int) std::round (ga.getBoundingBox (0, -1, true).getWidth());
            };

            const int wA = strW (f, "AtiNAMatiC");
            const int wB = strW (f, "-HitZone");
            const int totalW = wA + wB;
            const int startX = (getWidth() - totalW) / 2;

            g.setColour (col::text.withMultipliedAlpha (a));
            g.drawText ("AtiNAMatiC", startX, 0, wA + 2, getHeight(),
                        juce::Justification::centredLeft, false);

            g.setColour (col::accent.withMultipliedAlpha (a));
            g.drawText ("-HitZone", startX + wA, 0, wB + 2, getHeight(),
                        juce::Justification::centredLeft, false);

            if (hovered)
            {
                g.setColour (col::accent.withAlpha (0.55f));
                g.fillRect (startX, getHeight() - 2, totalW, 1);
            }
        }

        void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }
        void mouseUp    (const juce::MouseEvent&) override { if (onClick) onClick(); }

    private:
        bool hovered { false };
    };
}
