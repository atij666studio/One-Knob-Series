#pragma once

// ONE KNOB series UI kit.
//
// Visual language: a matte graphite faceplate with silkscreened legends, a
// machined knob with a printed 0-10 scale, one accent colour per plug-in
// used sparingly (pointer, active switch LED, value ring). No glow, no
// stock textures -- everything is drawn in code so it stays sharp at any
// scale.
//
// Each plug-in describes itself with an okui::Spec and gets the same frame:
//   header (wordmark + catalogue number), macro knob, optional mode switch,
//   activity window (plug-in specific rows), footer knobs.

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <BinaryData.h>
#include <functional>

namespace okui
{
    //==========================================================================
    namespace col
    {
        static const juce::Colour plate     (0xff18191b);   // faceplate
        static const juce::Colour plateLo   (0xff111214);
        static const juce::Colour window    (0xff0c0d0e);   // display windows
        static const juce::Colour rule      (0xff2b2d31);   // printed rules
        static const juce::Colour legend    (0xffe6e2d8);   // silkscreen white (warm)
        static const juce::Colour legendDim (0xff9a968c);
        static const juce::Colour legendLow (0xff5d5a54);
        static const juce::Colour capTop    (0xff2c2d30);
        static const juce::Colour capBot    (0xff131416);
        static const juce::Colour warn      (0xffe0a040);
        static const juce::Colour hot       (0xffe0553f);
    }

    //==========================================================================
    namespace fonts
    {
        inline juce::Typeface::Ptr load (const char* data, int size)
        {
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        }
        inline juce::Typeface::Ptr labelFace()
        {
            static auto t = load (BinaryData::BarlowCondensedMedium_ttf, BinaryData::BarlowCondensedMedium_ttfSize);
            return t;
        }
        inline juce::Typeface::Ptr headFace()
        {
            static auto t = load (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
            return t;
        }
        inline juce::Typeface::Ptr monoFace()
        {
            static auto t = load (BinaryData::IBMPlexMonoMedium_ttf, BinaryData::IBMPlexMonoMedium_ttfSize);
            return t;
        }

        // Silkscreen legends: condensed caps with a little tracking.
        inline juce::Font label (float h, float tracking = 0.08f)
        {
            return juce::Font (juce::FontOptions (labelFace()).withHeight (h)).withExtraKerningFactor (tracking);
        }
        inline juce::Font head (float h, float tracking = 0.02f)
        {
            return juce::Font (juce::FontOptions (headFace()).withHeight (h)).withExtraKerningFactor (tracking);
        }
        inline juce::Font mono (float h)
        {
            return juce::Font (juce::FontOptions (monoFace()).withHeight (h));
        }
    }

    //==========================================================================
    // Cached faceplate grain: fine, low-contrast powder-coat noise.
    inline const juce::Image& grain()
    {
        static juce::Image img = []
        {
            juce::Image im (juce::Image::ARGB, 256, 256, true);
            juce::Random r (0x0ce);
            for (int y = 0; y < im.getHeight(); ++y)
                for (int x = 0; x < im.getWidth(); ++x)
                {
                    const float v = r.nextFloat();
                    if (v > 0.5f) im.setPixelAt (x, y, juce::Colours::white.withAlpha ((v - 0.5f) * 0.045f));
                    else          im.setPixelAt (x, y, juce::Colours::black.withAlpha ((0.5f - v) * 0.10f));
                }
            return im;
        }();
        return img;
    }

    inline void paintPlate (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (col::plate, r.getCentreX(), r.getY(),
                                                 col::plateLo, r.getCentreX(), r.getBottom(), false));
        g.fillRect (r);
        g.setTiledImageFill (grain(), 0, 0, 1.0f);
        g.fillRect (r);
        // a whisper of top light, like a lit studio desk
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.035f), r.getCentreX(), r.getY(),
                                                 juce::Colours::transparentWhite, r.getCentreX(), r.getY() + r.getHeight() * 0.45f, false));
        g.fillRect (r);
    }

    // A recessed display window (meters live in these).
    inline void paintWindow (juce::Graphics& g, juce::Rectangle<float> r, float radius = 4.0f)
    {
        g.setColour (col::window);
        g.fillRoundedRectangle (r, radius);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine (r.getX() + radius, r.getBottom() + 0.5f, r.getRight() - radius, r.getBottom() + 0.5f, 1.0f);
    }

    // Small caps legend with hairline rules either side:  ——— LABEL ———
    inline void paintLegendRule (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text,
                                 juce::Colour c = col::legendDim)
    {
        auto f = fonts::label (11.0f, 0.18f);
        g.setFont (f);
        const float tw = juce::GlyphArrangement::getStringWidth (f, text.toUpperCase()) + 14.0f;
        const float cy = r.getCentreY();
        g.setColour (col::rule);
        g.fillRect (juce::Rectangle<float> (r.getX(), cy, (r.getWidth() - tw) * 0.5f, 1.0f));
        g.fillRect (juce::Rectangle<float> (r.getCentreX() + tw * 0.5f, cy, (r.getWidth() - tw) * 0.5f, 1.0f));
        g.setColour (c);
        g.drawText (text.toUpperCase(), r, juce::Justification::centred, false);
    }

    //==========================================================================
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit LookAndFeel (juce::Colour a) : accent (a)
        {
            setColour (juce::Slider::textBoxTextColourId,       col::legend);
            setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            setColour (juce::Label::textColourId,               col::legend);
            setColour (juce::TextButton::buttonColourId,        col::window);
            setColour (juce::TextButton::textColourOffId,       col::legend);
            setColour (juce::TextEditor::backgroundColourId,    col::window);
            setColour (juce::TextEditor::textColourId,          col::legend);
            setColour (juce::TextEditor::highlightColourId,     accent.withAlpha (0.35f));
            setColour (juce::CaretComponent::caretColourId,     accent);
            setDefaultSansSerifTypeface (fonts::labelFace());
        }

        juce::Colour accent;

        juce::Font getLabelFont (juce::Label& l) override  { return fonts::mono (l.getFont().getHeight()); }

        // Big (macro) knobs carry a printed 0-10 scale; small ones just ticks.
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                               float pos, float a0, float a1, juce::Slider& s) override
        {
            const auto b      = juce::Rectangle<int> (x, y, w, h).toFloat();
            const bool big    = (bool) s.getProperties().getWithDefault ("okBig", false);
            const float R     = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 1.0f;
            const float cx    = b.getCentreX(), cy = b.getCentreY();
            const float ang   = a0 + pos * (a1 - a0);
            const bool  bipol = (bool) s.getProperties().getWithDefault ("okBipolar", false);
            const float aRef  = bipol ? (a0 + a1) * 0.5f : a0;

            const float scaleR = big ? R * 0.80f : R * 0.98f;   // tick ring outer radius
            const float capR   = big ? R * 0.60f : R * 0.70f;

            auto polar = [&] (float a, float r) { return juce::Point<float> (cx + std::sin (a) * r, cy - std::cos (a) * r); };

            // --- printed scale ---------------------------------------------
            const int majors = 11, minorsPer = big ? 4 : 0;
            for (int i = 0; i < majors; ++i)
            {
                const float t  = (float) i / (float) (majors - 1);
                const float a  = a0 + t * (a1 - a0);
                const bool lit = bipol ? ((a >= juce::jmin (aRef, ang) - 1.0e-3f) && (a <= juce::jmax (aRef, ang) + 1.0e-3f))
                                       : (a <= ang + 1.0e-3f);
                const float len = big ? R * 0.075f : R * 0.12f;
                g.setColour (lit ? accent : col::legendDim);
                g.drawLine (juce::Line<float> (polar (a, scaleR - len), polar (a, scaleR)), big ? 1.6f : 1.3f);

                if (big)
                {
                    g.setColour (col::legendDim);
                    g.setFont (fonts::label (R * 0.095f, 0.0f));
                    const auto p = polar (a, scaleR + R * 0.105f);
                    g.drawText (juce::String (i), juce::Rectangle<float> (40.0f, 16.0f).withCentre (p),
                                juce::Justification::centred, false);
                }
                if (i < majors - 1)
                    for (int m = 1; m <= minorsPer; ++m)
                    {
                        const float am = a + (float) m / (float) (minorsPer + 1) * (a1 - a0) / (float) (majors - 1);
                        g.setColour (col::legendLow);
                        g.drawLine (juce::Line<float> (polar (am, scaleR - R * 0.035f), polar (am, scaleR)), 1.0f);
                    }
            }

            // --- value ring: a hairline, no glow ---------------------------
            {
                juce::Path ring;
                const float rr = capR + (scaleR - (big ? R * 0.075f : R * 0.12f) - capR) * 0.45f;
                ring.addCentredArc (cx, cy, rr, rr, 0.0f, juce::jmin (aRef, ang), juce::jmax (aRef, ang), true);
                g.setColour (accent.withAlpha (s.isEnabled() ? 0.9f : 0.3f));
                g.strokePath (ring, juce::PathStrokeType (big ? 2.0f : 1.6f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::butt));
            }

            // --- cap: soft contact shadow, knurled skirt, machined face ----
            for (int i = 6; i > 0; --i)
            {
                const float rr = capR + (float) i * (big ? 2.2f : 1.0f);
                g.setColour (juce::Colours::black.withAlpha (0.07f));
                g.fillEllipse (cx - rr, cy - rr + (big ? 5.0f : 2.0f), rr * 2.0f, rr * 2.0f);
            }
            g.setGradientFill (juce::ColourGradient (col::capTop, cx, cy - capR, col::capBot, cx, cy + capR, false));
            g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

            // knurling (rotates with the knob)
            const int knurls = big ? 72 : 32;
            const float kIn = capR * (big ? 0.90f : 0.84f);
            for (int i = 0; i < knurls; ++i)
            {
                const float a = ang + juce::MathConstants<float>::twoPi * (float) i / (float) knurls;
                const float lightness = 0.5f + 0.5f * std::cos (a + 0.6f);       // lit from top-left
                g.setColour (juce::Colours::white.withAlpha (0.03f + 0.07f * lightness));
                g.drawLine (juce::Line<float> (polar (a, kIn), polar (a, capR - 0.5f)), 1.0f);
            }

            // face
            const float faceR = kIn - (big ? 3.0f : 1.5f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff26272a), cx - faceR * 0.5f, cy - faceR * 0.7f,
                                                     juce::Colour (0xff161719), cx + faceR * 0.5f, cy + faceR * 0.8f, false));
            g.fillEllipse (cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f);
            if (big)   // spun finish: a few concentric hairlines
                for (int i = 1; i < 7; ++i)
                {
                    const float rr = faceR * (float) i / 7.0f;
                    g.setColour (juce::Colours::white.withAlpha (0.012f + 0.004f * (float) (i % 2)));
                    g.drawEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 0.8f);
                }
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawEllipse (cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.drawEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);

            // pointer: an engraved line filled with the accent
            const auto p0 = polar (ang, faceR * (big ? 0.28f : 0.18f));
            const auto p1 = polar (ang, faceR * 0.92f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawLine (juce::Line<float> (p0, p1).withShortenedStart (0.0f), big ? 5.0f : 3.4f);
            g.setColour (s.isEnabled() ? accent : col::legendDim);
            g.drawLine (juce::Line<float> (p0, p1), big ? 2.6f : 1.8f);
        }

        void drawLabel (juce::Graphics& g, juce::Label& l) override
        {
            g.setColour (l.findColour (juce::Label::textColourId));
            g.setFont (getLabelFont (l));
            g.drawText (l.getText(), l.getLocalBounds(), l.getJustificationType(), false);
        }

        juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
        {
            auto layout = juce::LookAndFeel_V4::getSliderLayout (s);
            return layout;
        }

        juce::Label* createSliderTextBox (juce::Slider& s) override
        {
            auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
            l->setFont (fonts::mono (12.0f));
            l->setColour (juce::Label::textColourId, col::legend);
            l->setColour (juce::Label::outlineWhenEditingColourId, accent);
            return l;
        }
    };

    //==========================================================================
    // Rotary control with double-click reset, Shift for fine adjust.
    class Knob : public juce::Slider
    {
    public:
        Knob (bool isBig = false)
        {
            setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                 juce::MathConstants<float>::pi * 2.75f, true);
            getProperties().set ("okBig", isBig);
            setMouseDragSensitivity (isBig ? 320 : 220);
            setVelocityModeParameters (0.4, 1, 0.02, false);
            setScrollWheelEnabled (true);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            setMouseDragSensitivity (e.mods.isShiftDown() ? 1600 : ((bool) getProperties()["okBig"] ? 320 : 220));
            juce::Slider::mouseDown (e);
        }
    };

    //==========================================================================
    // Hardware-style selector: a row of legends with a small LED above the
    // active one. Bound to a choice or bool parameter and follows host
    // automation / preset recall through a ParameterAttachment.
    class ModeSwitch : public juce::Component
    {
    public:
        ModeSwitch (juce::RangedAudioParameter& p, juce::StringArray labelsIn, juce::Colour acc)
            : param (p), labels (std::move (labelsIn)), accent (acc),
              attachment (p, [this] (float v) { index = juce::roundToInt (v); repaint(); })
        {
            attachment.sendInitialUpdate();
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            setTitle (p.getName (64));
            setWantsKeyboardFocus (false);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const int n = labels.size();
            const float w = r.getWidth() / (float) n;
            for (int i = 0; i < n; ++i)
            {
                auto cell = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
                const bool on = (i == index);
                const bool hov = (i == hover);

                // LED
                const auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ cell.getCentreX(), cell.getY() + 6.0f });
                g.setColour (juce::Colours::black.withAlpha (0.8f));
                g.fillEllipse (led.expanded (1.5f));
                if (on)
                {
                    g.setColour (accent.withAlpha (0.25f));
                    g.fillEllipse (led.expanded (3.0f));
                    g.setColour (accent);
                }
                else
                    g.setColour (juce::Colour (0xff2a2b2e));
                g.fillEllipse (led);
                if (on)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.5f));
                    g.fillEllipse (led.withSizeKeepingCentre (2.5f, 2.5f).translated (-1.0f, -1.0f));
                }

                // legend
                g.setFont (fonts::label (14.0f, 0.14f));
                g.setColour (on ? col::legend : (hov ? col::legend.withAlpha (0.8f) : col::legendDim));
                g.drawText (labels[i].toUpperCase(), cell.withTrimmedTop (14.0f), juce::Justification::centred, false);

                if (i > 0)
                {
                    g.setColour (col::rule);
                    g.fillRect (juce::Rectangle<float> (cell.getX(), cell.getY() + 16.0f, 1.0f, cell.getHeight() - 22.0f));
                }
            }
        }

        void mouseMove (const juce::MouseEvent& e) override  { hover = cellAt (e.x); repaint(); }
        void mouseExit (const juce::MouseEvent&) override    { hover = -1; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            const int i = cellAt (e.x);
            if (i >= 0 && i != index) attachment.setValueAsCompleteGesture ((float) i);
        }

    private:
        int cellAt (int x) const
        {
            const int n = labels.size();
            return n > 0 ? juce::jlimit (0, n - 1, x * n / juce::jmax (1, getWidth())) : -1;
        }

        juce::RangedAudioParameter& param;
        juce::StringArray labels;
        juce::Colour accent;
        int index { 0 }, hover { -1 };
        juce::ParameterAttachment attachment;
    };

    //==========================================================================
    // Activity window: a stack of thin meter rows plus a status legend.
    struct Row
    {
        enum class Kind { Level, Reduction, Amount, Bipolar };
        juce::String label;
        Kind kind { Kind::Level };
        std::function<float()> value;                  // dB for Level/Reduction, 0..1 for Amount
        float lo { -60.0f }, hi { 0.0f };
        std::function<float()> marker;                 // optional (same units)
        float zoneLo { 0.0f }, zoneHi { 0.0f };        // optional shaded target zone
        juce::String unit { "dB" };
        juce::Colour tint { 0xff3ee06a };              // Amount rows (set to the accent by the panel)
        float smoothed { -100.0f };
        float peak { -100.0f };
    };

    inline juce::Colour spec_accentFor (const Row& row, float segDb,
                                        juce::Colour green, juce::Colour amber, juce::Colour red)
    {
        if (row.kind == Row::Kind::Reduction) return amber;
        if (row.kind == Row::Kind::Amount || row.kind == Row::Kind::Bipolar) return row.tint;
        if (segDb > -3.0f)  return red;
        if (segDb > -12.0f) return amber;
        return green;
    }

    class ActivityPanel : public juce::Component
    {
    public:
        ActivityPanel (juce::Colour acc) : accent (acc) {}

        std::vector<Row> rows;
        std::function<juce::String()> status;          // e.g. "PUSH", "OPEN", "PERFECT"
        std::function<float()> statusLevel;            // 0..1 lights the status LED
        std::function<void (juce::Graphics&, juce::Rectangle<float>)> custom;   // replaces rows if set
        juce::String title { "ACTIVITY" };

        void tick()
        {
            for (auto& r : rows)
            {
                if (! r.value) continue;
                const float v = r.value();
                const float coef = (r.kind == Row::Kind::Amount) ? 0.3f : 0.45f;
                if (r.smoothed < -99.0f) r.smoothed = v;
                r.smoothed += (v - r.smoothed) * coef;
                r.peak = juce::jmax (v, r.peak - (r.kind == Row::Kind::Amount ? 0.02f : 0.6f));
            }
            if (statusLevel) statusSm += (statusLevel() - statusSm) * 0.3f;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            paintLegendRule (g, r.removeFromTop (16.0f), title);
            r.removeFromTop (6.0f);
            paintWindow (g, r);
            auto in = r.reduced (12.0f, 9.0f);

            // status line
            if (status)
            {
                auto st = in.removeFromTop (16.0f);
                const auto led = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ st.getRight() - 4.0f, st.getCentreY() });
                g.setColour (accent.withAlpha (0.15f + 0.85f * juce::jlimit (0.0f, 1.0f, statusSm)));
                g.fillEllipse (led);
                g.setFont (fonts::label (13.0f, 0.2f));
                g.setColour (col::legend);
                g.drawText (status(), st.withTrimmedRight (14.0f), juce::Justification::centredRight, false);
                in.removeFromTop (4.0f);
            }

            if (custom) { custom (g, in); return; }

            const int n = (int) rows.size();
            if (n == 0) return;
            for (auto& row : rows) row.tint = accent;
            const float rowH = juce::jmin (22.0f, in.getHeight() / (float) n);
            for (auto& row : rows)
                paintRow (g, in.removeFromTop (rowH), row);
        }

    private:
        void paintRow (juce::Graphics& g, juce::Rectangle<float> r, const Row& row)
        {
            auto lab = r.removeFromLeft (64.0f);
            auto val = r.removeFromRight (58.0f);
            g.setFont (fonts::label (12.0f, 0.12f));
            g.setColour (col::legendDim);
            g.drawText (row.label.toUpperCase(), lab, juce::Justification::centredLeft, false);

            auto bar = r.withSizeKeepingCentre (r.getWidth(), 6.0f);
            auto norm = [&] (float v) { return juce::jlimit (0.0f, 1.0f, (v - row.lo) / (row.hi - row.lo)); };

            // Segmented LED bar -- the series' VU style. Level rows run
            // green -> amber (-12) -> red (-3); amounts glow in the accent;
            // reductions light amber from the right.
            static const juce::Colour ledGreen (0xff3ee06a), ledAmber (0xffffc233), ledRed (0xffff3b3b);
            constexpr int   numSeg = 28;
            constexpr float gap    = 2.0f;
            bar = bar.withSizeKeepingCentre (bar.getWidth(), 9.0f);
            const float segW = (bar.getWidth() - gap * (float) (numSeg - 1)) / (float) numSeg;
            const float v    = row.smoothed;
            const float f    = norm (v);
            const float pk   = norm (row.peak);

            for (int i = 0; i < numSeg; ++i)
            {
                const float t   = ((float) i + 0.5f) / (float) numSeg;          // segment centre, 0..1
                const float sDb = row.lo + (row.hi - row.lo) * t;
                juce::Colour zone = spec_accentFor (row, sDb, ledGreen, ledAmber, ledRed);
                const float c0 = norm (0.0f);
                bool lit = (row.kind == Row::Kind::Reduction) ? (t >= 1.0f - f)
                         : (row.kind == Row::Kind::Bipolar)   ? (t >= juce::jmin (c0, f) && t <= juce::jmax (c0, f) && std::abs (f - c0) > 0.5f / (float) numSeg)
                                                              : (t <= f);
                const bool peakSeg = row.kind == Row::Kind::Level && std::abs (t - pk) < 0.5f / (float) numSeg && pk > 0.01f;
                const bool inZone  = row.zoneHi > row.zoneLo && sDb >= row.zoneLo && sDb <= row.zoneHi;

                auto seg = juce::Rectangle<float> (bar.getX() + (float) i * (segW + gap), bar.getY(), segW, bar.getHeight());
                if (lit || peakSeg)
                {
                    g.setColour (zone.withAlpha (0.35f));
                    g.fillRoundedRectangle (seg.expanded (0.6f, 1.2f), 1.6f);
                    g.setColour (zone);
                    g.fillRoundedRectangle (seg, 1.2f);
                }
                else
                {
                    g.setColour (zone.withAlpha (inZone ? 0.30f : 0.14f));
                    g.fillRoundedRectangle (seg, 1.2f);
                }
            }

            if (row.marker)
            {
                const float m = norm (row.marker());
                g.setColour (col::legend);
                const float mx = bar.getX() + bar.getWidth() * m;
                juce::Path tri;
                tri.addTriangle (mx - 4.0f, bar.getY() - 6.0f, mx + 4.0f, bar.getY() - 6.0f, mx, bar.getY() - 1.0f);
                g.fillPath (tri);
                g.fillRect (juce::Rectangle<float> (mx - 0.5f, bar.getY(), 1.0f, bar.getHeight()));
            }

            // numeric
            g.setFont (fonts::mono (11.5f));
            g.setColour (col::legend);
            juce::String txt;
            if (row.kind == Row::Kind::Amount)         txt = juce::String (juce::roundToInt (v * 100.0f)) + " %";
            else if (row.kind == Row::Kind::Reduction) txt = (v > 0.05f ? "-" : "") + juce::String (v, 1) + " " + row.unit;
            else if (row.kind == Row::Kind::Bipolar)   txt = (v > 0.05f ? "+" : "") + juce::String (v, 1) + " " + row.unit;
            else if (v <= row.lo + 0.01f)              txt = "-inf";
            else                                       txt = juce::String (v, 1) + " " + row.unit;
            g.drawText (txt, val, juce::Justification::centredRight, false);
        }

        juce::Colour accent;
        float statusSm { 0.0f };
    };

    //==========================================================================
    // About card (click the wordmark).
    class AboutOverlay : public juce::Component
    {
    public:
        AboutOverlay (juce::String nameIn, juce::String tagIn, juce::String catIn, juce::Colour acc)
            : name (std::move (nameIn)), tag (std::move (tagIn)), cat (std::move (catIn)), accent (acc)
        {
            setInterceptsMouseClicks (true, true);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::black.withAlpha (0.78f));
            auto card = getLocalBounds().toFloat().reduced (40.0f, 70.0f);
            paintPlate (g, card);
            g.setColour (col::rule);
            g.drawRect (card, 1.0f);
            g.setColour (accent);
            g.fillRect (card.removeFromTop (3.0f));

            auto a = card.reduced (28.0f, 22.0f);
            g.setFont (fonts::label (12.0f, 0.25f));
            g.setColour (col::legendDim);
            g.drawText ("ONE KNOB SERIES  " + cat, a.removeFromTop (18.0f), juce::Justification::centredLeft, false);
            a.removeFromTop (6.0f);
            g.setFont (fonts::head (34.0f));
            g.setColour (col::legend);
            g.drawText ("AtiNAMatiC", a.removeFromTop (38.0f), juce::Justification::centredLeft, false);
            g.setColour (accent);
            g.drawText (name.toUpperCase(), a.removeFromTop (36.0f), juce::Justification::centredLeft, false);
            g.setFont (fonts::label (15.0f, 0.04f));
            g.setColour (col::legendDim);
            g.drawText (tag + "   v" + juce::String (JucePlugin_VersionString), a.removeFromTop (22.0f), juce::Justification::centredLeft, false);
            a.removeFromTop (14.0f);
            g.setColour (col::rule);
            g.fillRect (a.removeFromTop (1.0f));
            a.removeFromTop (14.0f);

            g.setFont (fonts::label (15.0f, 0.04f));
            g.setColour (col::legend);
            g.drawText ("Atij 666 Studio  /  designed & developed by Atij", a.removeFromTop (22.0f), juce::Justification::centredLeft, false);
            a.removeFromTop (12.0f);
            g.setFont (fonts::label (12.0f, 0.2f));
            g.setColour (col::legendDim);
            g.drawText ("BUILT WITH & GRATEFUL TO", a.removeFromTop (18.0f), juce::Justification::centredLeft, false);
            g.setFont (fonts::label (14.0f, 0.03f));
            for (auto s : { "JUCE  -  Raw Material Software", "VST3 SDK  -  Steinberg Media Technologies",
                            "CLAP & clap-juce-extensions  -  free-audio", "Neural Amp Modeler  -  Steven Atkinson & contributors",
                            "Barlow & IBM Plex  -  SIL Open Font License" })
                g.drawText (s, a.removeFromTop (19.0f), juce::Justification::centredLeft, false);
            a.removeFromTop (12.0f);
            g.setColour (col::legendLow);
            g.drawText ("Drag knobs  /  Shift = fine  /  Double-click = default  /  drag corner to resize",
                        a.removeFromTop (18.0f), juce::Justification::centredLeft, false);
        }

        void mouseUp (const juce::MouseEvent&) override { setVisible (false); }

    private:
        juce::String name, tag, cat;
        juce::Colour accent;
    };

    //==========================================================================
    // Header: wordmark + product, catalogue number, tagline.
    class Header : public juce::Component
    {
    public:
        Header (juce::String productIn, juce::String tagIn, juce::String catIn, juce::Colour acc)
            : product (std::move (productIn)), tag (std::move (tagIn)), cat (std::move (catIn)), accent (acc)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        std::function<void()> onClick;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (26.0f, 0.0f);
            auto f = fonts::head (27.0f, 0.01f);
            g.setFont (f);
            const float w1 = juce::GlyphArrangement::getStringWidth (f, "AtiNAMatiC");
            g.setColour (col::legend.withAlpha (hover ? 1.0f : 0.92f));
            g.drawText ("AtiNAMatiC", r.withWidth (w1 + 4.0f), juce::Justification::centredLeft, false);
            g.setColour (accent);
            g.drawText (product.toUpperCase(), r.withTrimmedLeft (w1 + 8.0f), juce::Justification::centredLeft, false);

            auto right = r.removeFromRight (240.0f);
            g.setFont (fonts::label (11.5f, 0.24f));
            g.setColour (col::legendDim);
            g.drawText ("ATIJ 666  /  ONE KNOB  " + cat, right.removeFromTop (r.getHeight() * 0.5f).withTrimmedTop (6.0f),
                        juce::Justification::bottomRight, false);
            g.setFont (fonts::label (13.0f, 0.03f));
            g.setColour (col::legendLow.brighter (0.3f));
            g.drawText (tag, right.withTrimmedBottom (6.0f), juce::Justification::topRight, false);
        }

        void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
        void mouseUp    (const juce::MouseEvent&) override { if (onClick) onClick(); }

    private:
        juce::String product, tag, cat;
        juce::Colour accent;
        bool hover { false };
    };

    //==========================================================================
    // Everything a plug-in needs to describe its face.
    struct Spec
    {
        juce::String product;          // "Boost"
        juce::String tagline;          // "Life, punch & clarity"
        int          index { 1 };      // catalogue number 1..11
        juce::Colour accent;

        juce::String macroId, macroLegend;
        juce::StringArray zoneLegends; // three legends around the macro knob (start / mid / end)
        bool macroBipolar { false };
        std::function<juce::String (float)> macroText;   // value readout under the knob

        juce::String modeId;           // optional
        juce::StringArray modeLabels;
        juce::String modeLegend { "MODE" };

        juce::Image background;        // the plug-in's own faceplate artwork (optional)

        struct Small { juce::String id, legend; std::function<juce::String (float)> text; };
        std::vector<Small> footer;     // up to 3
    };

    //==========================================================================
    // Base editor: draws the faceplate and lays out the shared frame inside
    // a fixed-size "content" component that is scaled as the window resizes.
    class Editor : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        static constexpr int baseW = 500, baseH = 640;

        Editor (juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& s, Spec specIn)
            : juce::AudioProcessorEditor (p), apvts (s), spec (std::move (specIn)), lnf (spec.accent),
              header (spec.product, spec.tagline, catalogue (spec.index), spec.accent),
              about (spec.product, spec.tagline, catalogue (spec.index), spec.accent),
              macro (true), activity (spec.accent)
        {
            setLookAndFeel (&lnf);
            addAndMakeVisible (content);
            content.setBounds (0, 0, baseW, baseH);
            content.onPaint = [this] (juce::Graphics& g) { paintFace (g); };

            header.onClick = [this] { about.setVisible (true); about.toFront (false); };
            content.addAndMakeVisible (header);

            macro.getProperties().set ("okBipolar", spec.macroBipolar);
            macro.setTitle (spec.macroLegend);
            content.addAndMakeVisible (macro);
            attach (macro, spec.macroId);
            macro.onValueChange = [this] { content.repaint (readoutArea); };

            if (spec.modeId.isNotEmpty())
                if (auto* mp = apvts.getParameter (spec.modeId))
                {
                    modeSwitch = std::make_unique<ModeSwitch> (*mp, spec.modeLabels, spec.accent);
                    content.addAndMakeVisible (*modeSwitch);
                }

            content.addAndMakeVisible (activity);

            for (auto& f : spec.footer)
            {
                auto k = std::make_unique<Knob> (false);
                k->setTitle (f.legend);
                content.addAndMakeVisible (*k);
                attach (*k, f.id);
                auto* kp = k.get();
                k->onValueChange = [this] { content.repaint (footerArea); };
                juce::ignoreUnused (kp);
                footerKnobs.push_back (std::move (k));
            }

            content.addChildComponent (about);

            // Resizable 75..150 %, aspect locked, size remembered with the session.
            const double scale = (double) apvts.state.getProperty ("uiScale", 1.0);
            setResizable (true, true);
            setResizeLimits ((int) (baseW * 0.75), (int) (baseH * 0.75), (int) (baseW * 1.5), (int) (baseH * 1.5));
            if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
            setSize (juce::roundToInt (baseW * juce::jlimit (0.75, 1.5, scale)),
                     juce::roundToInt (baseH * juce::jlimit (0.75, 1.5, scale)));

            startTimerHz (30);
        }

        ~Editor() override
        {
            stopTimer();
            setLookAndFeel (nullptr);
        }

        void resized() override
        {
            const float sc = (float) getWidth() / (float) baseW;
            content.setTransform (juce::AffineTransform::scale (sc));
            apvts.state.setProperty ("uiScale", sc, nullptr);
            layout();
        }

    protected:
        juce::AudioProcessorValueTreeState& apvts;
        Spec spec;
        LookAndFeel lnf;

        struct Content : public juce::Component
        {
            std::function<void (juce::Graphics&)> onPaint;
            void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
        } content;

        Header header;
        AboutOverlay about;
        Knob macro;
        std::unique_ptr<ModeSwitch> modeSwitch;
        ActivityPanel activity;
        std::vector<std::unique_ptr<Knob>> footerKnobs;

        // For plug-ins that need an extra control (e.g. HitZone's AUTO).
        virtual void layoutExtras (juce::Rectangle<int>) {}
        virtual void onTick() {}

        Knob* footerKnob (const juce::String& id)
        {
            for (size_t i = 0; i < spec.footer.size(); ++i)
                if (spec.footer[i].id == id) return footerKnobs[i].get();
            return nullptr;
        }

    private:
        using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
        std::vector<std::unique_ptr<SAtt>> attachments;
        juce::Rectangle<int> readoutArea, footerArea, macroArea, modeArea;

        static juce::String catalogue (int i)
        {
            return juce::String (i).paddedLeft ('0', 2) + " / 11";
        }

        void attach (juce::Slider& s, const juce::String& id)
        {
            if (auto* p = apvts.getParameter (id))
            {
                attachments.push_back (std::make_unique<SAtt> (apvts, id, s));
                s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
                s.textFromValueFunction = [p] (double v) { return p->getText (p->convertTo0to1 ((float) v), 32); };
            }
        }

        void layout()
        {
            auto r = juce::Rectangle<int> (0, 0, baseW, baseH);
            about.setBounds (r);
            header.setBounds (r.removeFromTop (72));

            auto foot = r.removeFromBottom (128);
            footerArea = foot;
            auto act = r.removeFromBottom (spec.modeId.isNotEmpty() ? 118 : 136).reduced (26, 4);
            activity.setBounds (act);

            if (modeSwitch != nullptr)
            {
                modeArea = r.removeFromBottom (70).reduced (40, 6);
                modeSwitch->setBounds (modeArea.withTrimmedTop (18));
            }

            macroArea = r;
            const int ks = juce::jmin (230, juce::jmin (r.getWidth() - 160, r.getHeight() - 60));
            macro.setBounds (juce::Rectangle<int> (ks, ks).withCentre ({ r.getCentreX(), r.getCentreY() + 4 }));
            readoutArea = juce::Rectangle<int> (r.getCentreX() - 80, macro.getBottom() - 26, 160, 30);

            const int n = (int) footerKnobs.size();
            if (n > 0)
            {
                auto fr = foot.reduced (40, 10);
                const int cw = fr.getWidth() / juce::jmax (n, 2);
                const int x0 = fr.getCentreX() - cw * n / 2;
                for (int i = 0; i < n; ++i)
                {
                    auto cell = juce::Rectangle<int> (x0 + cw * i, fr.getY(), cw, fr.getHeight());
                    footerKnobs[(size_t) i]->setBounds (cell.withSizeKeepingCentre (64, 64).translated (0, -2));
                }
            }

            layoutExtras (act);
        }

        void paintFace (juce::Graphics& g)
        {
            auto r = juce::Rectangle<float> (0, 0, (float) baseW, (float) baseH);
            if (spec.background.isValid())
            {
                g.drawImage (spec.background, r, juce::RectanglePlacement::fillDestination);
                // keep legends readable over the artwork without hiding it
                g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.10f), r.getCentreX(), r.getCentreY(),
                                                         juce::Colours::black.withAlpha (0.35f), r.getX(), r.getY(), true));
                g.fillRect (r);
            }
            else
                paintPlate (g, r);

            // header rule + accent tab
            g.setColour (col::rule);
            g.fillRect (juce::Rectangle<float> (26.0f, 71.0f, (float) baseW - 52.0f, 1.0f));
            g.setColour (spec.accent);
            g.fillRect (juce::Rectangle<float> (26.0f, 70.0f, 34.0f, 3.0f));

            // macro legend + zone legends
            g.setFont (fonts::label (12.0f, 0.3f));
            g.setColour (col::legendDim);
            g.drawText (spec.macroLegend.toUpperCase(), readoutArea.translated (0, 24).withHeight (16),
                        juce::Justification::centred, false);

            if (spec.zoneLegends.size() == 3)
            {
                const auto kb = macro.getBounds().toFloat();
                const float R = kb.getWidth() * 0.5f;
                const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
                g.setFont (fonts::label (12.5f, 0.2f));
                const juce::Point<float> c = kb.getCentre();
                auto at = [&] (float a, float rr) { return juce::Point<float> (c.x + std::sin (a) * rr, c.y - std::cos (a) * rr); };
                g.setColour (col::legend.withAlpha (0.85f));
                g.drawText (spec.zoneLegends[0], juce::Rectangle<float> (90.0f, 18.0f).withCentre (at (a0, R * 1.08f).translated (-34.0f, 4.0f)), juce::Justification::centredRight, false);
                g.drawText (spec.zoneLegends[2], juce::Rectangle<float> (90.0f, 18.0f).withCentre (at (a1, R * 1.08f).translated (34.0f, 4.0f)), juce::Justification::centredLeft, false);
                g.drawText (spec.zoneLegends[1], juce::Rectangle<float> (90.0f, 18.0f).withCentre (at (0.0f, R * 1.07f)), juce::Justification::centred, false);
            }

            // macro value readout
            {
                const float v = (float) macro.getValue();
                const auto txt = spec.macroText ? spec.macroText (v) : macro.getTextFromValue (v);
                g.setFont (fonts::mono (17.0f));
                g.setColour (col::legend);
                g.drawText (txt, readoutArea, juce::Justification::centred, false);
            }

            if (modeSwitch != nullptr)
                paintLegendRule (g, modeArea.toFloat().withHeight (14.0f), spec.modeLegend);

            // footer
            auto foot = footerArea.toFloat();
            g.setColour (col::rule);
            g.fillRect (juce::Rectangle<float> (26.0f, foot.getY(), (float) baseW - 52.0f, 1.0f));
            for (size_t i = 0; i < footerKnobs.size(); ++i)
            {
                if (! footerKnobs[i]->isVisible()) continue;
                const auto kb = footerKnobs[i]->getBounds().toFloat();
                g.setFont (fonts::label (12.0f, 0.24f));
                g.setColour (col::legendDim);
                g.drawText (spec.footer[i].legend.toUpperCase(), kb.withY (kb.getY() - 22.0f).withHeight (16.0f).expanded (30.0f, 0.0f),
                            juce::Justification::centred, false);
                const float v = (float) footerKnobs[i]->getValue();
                const auto txt = spec.footer[i].text ? spec.footer[i].text (v) : footerKnobs[i]->getTextFromValue (v);
                g.setFont (fonts::mono (12.5f));
                g.setColour (col::legend);
                g.drawText (txt, kb.withY (kb.getBottom() + 4.0f).withHeight (16.0f).expanded (30.0f, 0.0f),
                            juce::Justification::centred, false);
            }

        }

        void timerCallback() override
        {
            activity.tick();
            onTick();
        }

    protected:
        void relayout() { layout(); content.repaint(); }
    };
}
