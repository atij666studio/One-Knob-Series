#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace sq
{
    // -------- Palette --------------------------------------------------------
    namespace col
    {
        static const juce::Colour bg0       (0xff0e1318);
        static const juce::Colour bg1       (0xff161c23);
        static const juce::Colour panel     (0xff1a2029);
        static const juce::Colour panelEdge (0xff262d38);
        static const juce::Colour text      (0xffffffff);
        static const juce::Colour textDim   (0xffc8d2e0);
        static const juce::Colour textFaint (0xff9da8bc);
        static const juce::Colour accent    (0xffff2244);  // vivid red
        static const juce::Colour accentLo  (0xffcc0022);
        static const juce::Colour ok        (0xff5dd28d);
        static const juce::Colour warn      (0xffe8a64d);
        static const juce::Colour danger    (0xffe25b5b);
        static const juce::Colour gridLine  (0xff262d38);
    }

    // -------- LookAndFeel ----------------------------------------------------
    class SquashLNF : public juce::LookAndFeel_V4
    {
    public:
        SquashLNF()
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
            setColour (juce::ComboBox::backgroundColourId,        juce::Colour (0xff1a1f25));
            setColour (juce::ComboBox::textColourId,              col::text);
            setColour (juce::ComboBox::outlineColourId,           col::panelEdge);
            setColour (juce::PopupMenu::backgroundColourId,       juce::Colour (0xff1a1f25));
            setColour (juce::PopupMenu::textColourId,             col::text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
            setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::black);
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

            // Outer shadow
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawEllipse (cx - radius, cy - radius + 2.0f, radius * 2.0f, radius * 2.0f, 1.0f);

            // Track background
            juce::Path track;
            track.addCentredArc (cx, cy, trackR, trackR, 0.0f, startAngle, endAngle, true);
            g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
            g.strokePath (track, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved));

            // Filled arc with gradient
            const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
            juce::Path filled;
            filled.addCentredArc (cx, cy, trackR, trackR, 0.0f, startAngle, angle, true);
            juce::ColourGradient grad (accent.darker (0.6f), cx - trackR, cy,
                                       accent.brighter (0.1f), cx + trackR, cy, false);
            g.setGradientFill (grad);
            g.strokePath (filled, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            // Glow
            const auto inner = radius - trackThickness * 1.7f;
            const auto glow  = juce::jlimit (0.0f, 1.0f, sliderPos);
            for (int i = 10; i > 0; --i)
            {
                const float r = inner + (float) i * 0.9f;
                const float a = (0.04f + 0.10f * glow) * (1.0f - (float) i / 10.0f);
                g.setColour (accent.withAlpha (a));
                g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
            }

            // Tick marks at 0 / 25 / 50 / 75 / 100 %
            for (int t = 0; t <= 4; ++t)
            {
                const float ta = startAngle + (float) t / 4.0f * (endAngle - startAngle);
                const float r1 = inner + 1.0f;
                const float r2 = inner + 4.0f;
                g.setColour (col::textFaint.withAlpha (0.6f));
                g.drawLine (cx + std::sin (ta) * r1, cy - std::cos (ta) * r1,
                            cx + std::sin (ta) * r2, cy - std::cos (ta) * r2, 1.0f);
            }

            // Brushed dark face
            juce::ColourGradient faceGrad (juce::Colour (0xff232a34), cx, cy - inner,
                                           juce::Colour (0xff10141a), cx, cy + inner, false);
            g.setGradientFill (faceGrad);
            g.fillEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f);

            // Inner rim
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f, 1.0f);

            // Indicator
            juce::Path pointer;
            const float pw = juce::jmax (3.0f, inner * 0.10f);
            const float pl = inner * 0.78f;
            pointer.addRoundedRectangle (-pw * 0.5f, -inner * 0.92f, pw, pl, pw * 0.5f);
            const auto rot = juce::AffineTransform::rotation (angle).translated (cx, cy);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillPath (pointer, rot.translated (0.0f, 1.5f));
            g.setColour (accent.brighter (0.3f));
            g.fillPath (pointer, rot);

            // Centre dot
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
            if (! on) { g.setColour (col::panelEdge); g.drawRoundedRectangle (r, 4.0f, 1.0f); }
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

    // -------- Mode picker (GENTLE / STANDARD / SMASH) --------------------
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
            buttons[juce::jlimit (0, 2, ix)]->setToggleState (true, juce::dontSendNotification);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            const int w = r.getWidth() / 3;
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setBounds (r.removeFromLeft (i == buttons.size() - 1 ? r.getWidth() : w));
        }

    private:
        const juce::StringArray labels { "GENTLE", "STANDARD", "SMASH" };
        juce::OwnedArray<juce::TextButton> buttons;
    };

    // -------- Gain-Reduction meter ----------------------------------------
    // Fills left→right showing |grDb| out of 24 dB max range.
    // Horizontal segmented VU-style GR meter.
    // currentGrDb (≤ 0)   : smoothed display value, updated by editor timer
    // peakGrDb    (≤ 0)   : peak hold, updated by editor timer
    // peakHoldFrames (int): frames remaining before peak starts decaying
    class GrMeter : public juce::Component
    {
    public:
        std::atomic<float>* sourceGrDb { nullptr }; // ≤ 0, owned by DSP
        float currentGrDb   { 0.0f };
        float peakGrDb      { 0.0f };
        int   peakHoldFrames { 0 };

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (1.0f);

            // Background track
            g.setColour (col::bg0);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (col::panelEdge);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);

            const float maxGr   = 18.0f;
            const int   numSegs = 48;
            const float inner   = r.getWidth() - 4.0f;
            const float segW    = inner / float (numSegs);
            const float segH    = r.getHeight() - 4.0f;
            const float startX  = r.getX() + 2.0f;
            const float startY  = r.getY() + 2.0f;

            const float filledF  = juce::jlimit (0.0f, float (numSegs),
                                       std::abs (currentGrDb) / maxGr * numSegs);
            const int   filledN  = (int) filledF;
            const float peakF    = juce::jlimit (0.0f, float (numSegs - 1),
                                       std::abs (peakGrDb) / maxGr * numSegs);
            const int   peakN    = (int) peakF;

            for (int i = 0; i < numSegs; ++i)
            {
                const float frac = float (i) / float (numSegs);
                const juce::Rectangle<float> seg (startX + i * segW + 0.5f,
                                                  startY,
                                                  segW - 1.0f,
                                                  segH);
                if (i == peakN && peakN > 0)
                {
                    // Peak hold — always fully bright
                    const juce::Colour pc = frac < 0.50f ? col::ok.brighter (0.4f)
                                          : frac < 0.75f ? col::warn.brighter (0.3f)
                                          :                 col::danger.brighter (0.2f);
                    g.setColour (pc);
                    g.fillRoundedRectangle (seg, 1.5f);
                }
                else if (i < filledN)
                {
                    // Active fill — green → amber → red
                    const juce::Colour fc = frac < 0.50f ? col::ok.withAlpha (0.85f)
                                          : frac < 0.75f ? col::warn.withAlpha (0.85f)
                                          :                 col::danger.withAlpha (0.85f);
                    g.setColour (fc);
                    g.fillRoundedRectangle (seg, 1.5f);
                }
                else
                {
                    // Dim unlit segment
                    g.setColour (col::panelEdge.withAlpha (0.55f));
                    g.fillRoundedRectangle (seg, 1.5f);
                }
            }

            // Tick marks at -6 / -12 / -18 dB (overdrawn)
            for (float gr = 6.0f; gr < maxGr; gr += 6.0f)
            {
                const float gx = startX + inner * (gr / maxGr);
                g.setColour (col::bg0.withAlpha (0.75f));
                g.drawLine (gx, startY, gx, startY + segH, 1.0f);
            }
        }
    };

    // -------- Title badge ("AtiNAMatiC" + "-Squash" accent, clickable) ---
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
            as.append ("AtiNAMatiC", f, col::text.withAlpha (0.70f));
            as.append ("-Squash",    f, col::accent);
            juce::TextLayout tl;
            tl.createLayout (as, (float) getWidth());
            tl.draw (g, getLocalBounds().toFloat());
        }

        void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
        juce::MouseCursor getMouseCursor() override { return juce::MouseCursor::PointingHandCursor; }
    };

    // -------- About overlay ----------------------------------------------
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

            {
                juce::Font fTitle (juce::FontOptions().withName ("Helvetica Neue")
                                                      .withHeight (24.0f).withStyle ("Bold"));
                juce::AttributedString as;
                as.setJustification (juce::Justification::centredLeft);
                as.append ("AtiNAMatiC", fTitle, col::text);
                as.append ("-Squash",    fTitle, col::accent);
                juce::TextLayout tl;
                tl.createLayout (as, (float) area.getWidth());
                tl.draw (g, juce::Rectangle<float> ((float) area.getX(), (float) y,
                                                     (float) area.getWidth(), 30.0f));
            }

            y += 34;
            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue").withHeight (11.0f)));
            g.setColour (col::textDim);
            g.drawText ("Version 0.1.0   |   One-knob bus and mix compressor",
                        area.withY (y).withHeight (16), juce::Justification::centredLeft, false);

            y += 22;
            g.setColour (col::panelEdge);
            g.fillRect (area.getX(), y, area.getWidth(), 1);
            y += 14;

            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue")
                                                      .withHeight (13.0f).withStyle ("Bold")));
            g.setColour (col::text);
            g.drawText ("Atij 666 Studio", area.withY (y).withHeight (18),
                        juce::Justification::centredLeft, false);
            y += 22;
            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue").withHeight (11.0f)));
            g.setColour (col::textDim);
            g.drawText ("Designed & developed by Atij",
                        area.withY (y).withHeight (16), juce::Justification::centredLeft, false);

            y += 26;
            g.setColour (col::panelEdge);
            g.fillRect (area.getX(), y, area.getWidth(), 1);
            y += 14;

            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue")
                                                      .withHeight (10.0f).withStyle ("Bold")));
            g.setColour (col::textFaint);
            g.drawText ("BUILT WITH & GRATEFUL TO",
                        area.withY (y).withHeight (14), juce::Justification::centredLeft, false);
            y += 18;

            const juce::StringArray credits {
                "JUCE Framework  --  Raw Material Software Ltd.",
                "VST3 SDK  --  Steinberg Media Technologies GmbH",
                "clap-juce-extensions  --  Free Audio contributors",
                "The open-source audio & plugin community"
            };
            g.setFont (juce::Font (juce::FontOptions().withName ("Helvetica Neue").withHeight (11.0f)));
            g.setColour (col::textDim);
            for (const auto& credit : credits)
            {
                g.drawText ("*  " + credit, area.withY (y).withHeight (16),
                            juce::Justification::centredLeft, false);
                y += 17;
            }
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
        juce::Rectangle<int> cardBounds() const { return getLocalBounds().reduced (44, 56); }
        juce::TextButton closeBtn;
    };
}
