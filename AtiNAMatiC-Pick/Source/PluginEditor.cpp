#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    constexpr int kWidth  = 520;
    constexpr int kHeight = 540;
}

NAMPickEditor::NAMPickEditor (NAMPickProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    background = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);

    setLookAndFeel (&lnf);
    setSize (kWidth, kHeight);

    auto setupLabel = [this] (juce::Label& l, const juce::String& text,
                              float fontHeight, bool bold,
                              juce::Justification j = juce::Justification::centred,
                              juce::Colour c = np::col::text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font ("Helvetica Neue", fontHeight,
                               bold ? juce::Font::bold : juce::Font::plain));
        l.setJustificationType (j);
        l.setColour (juce::Label::textColourId, c);
        addAndMakeVisible (l);
    };

    addChildComponent (aboutOverlay);

    addAndMakeVisible (titleBadge);
    titleBadge.onClick = [this]
    {
        aboutOverlay.setVisible (true);
        aboutOverlay.toFront (false);
    };

    setupLabel (brandLabel,    "Pick attack designer",           11.0f, false,
                juce::Justification::centredRight, np::col::textDim);

    setupLabel (attackValueLabel, "Neutral",                     13.0f, true,
                juce::Justification::centred, np::col::text);

    setupLabel (softTickLabel,    "SOFT",                         9.0f, true,
                juce::Justification::centredLeft,  np::col::accentSoft);
    setupLabel (neutralTickLabel, "NEUTRAL",                      9.0f, true,
                juce::Justification::centred,      np::col::textFaint);
    setupLabel (sharpTickLabel,   "SHARP",                        9.0f, true,
                juce::Justification::centredRight, np::col::accentSharp);

    setupLabel (mixLabel, "MIX",    10.0f, true, juce::Justification::centred, np::col::textFaint);
    setupLabel (outLabel, "OUTPUT", 10.0f, true, juce::Justification::centred, np::col::textFaint);

    addAndMakeVisible (attackHeader);
    addAndMakeVisible (meterHeader);

    auto setupRotary = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (s);
    };

    setupRotary (attackKnob);
    setupRotary (mixKnob);
    setupRotary (outputKnob);

    attackKnob.getProperties().set ("bipolar", true);
    attackKnob.setDoubleClickReturnValue (true, 0.5);

    outputKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    outputKnob.setTextValueSuffix (" dB");
    mixKnob.setTextBoxStyle    (juce::Slider::TextBoxBelow, false, 60, 16);

    aAttack = std::make_unique<SAtt> (proc.apvts, "attack", attackKnob);
    aMix    = std::make_unique<SAtt> (proc.apvts, "mix",    mixKnob);
    aOut    = std::make_unique<SAtt> (proc.apvts, "output", outputKnob);

    attackKnob.onValueChange = [this]
    {
        const float v = (float) attackKnob.getValue();
        const int pct = juce::roundToInt ((v - 0.5f) * 200.0f);
        juce::String s;
        if (pct == 0)        s = "Neutral";
        else if (pct > 0)    s = "Sharp +" + juce::String (pct) + "%";
        else                 s = "Soft "   + juce::String (pct) + "%";
        attackValueLabel.setText (s, juce::dontSendNotification);
        transientPill.direction = juce::jlimit (-1.0f, 1.0f, (v - 0.5f) * 2.0f);
        transientPill.repaint();
    };
    attackKnob.onValueChange();

    transientPill.transientSrc = &proc.dsp.transientDisp;
    addAndMakeVisible (transientPill);

    startTimerHz (30);
}

NAMPickEditor::~NAMPickEditor()
{
    setLookAndFeel (nullptr);
}

void NAMPickEditor::paint (juce::Graphics& g)
{
    if (background.isValid())
    {
        g.drawImage (background, getLocalBounds().toFloat());
    }
    else
    {
        juce::ColourGradient bg (np::col::bg1, (float) getWidth() * 0.5f, 0.0f,
                                 np::col::bg0, (float) getWidth() * 0.5f, (float) getHeight(), false);
        g.setGradientFill (bg);
        g.fillAll();
    }

    const auto topH = 56;
    g.setColour (np::col::accent);
    g.fillRect (16, 14, 3, topH - 28);
    g.setColour (np::col::accent.withAlpha (0.35f));
    g.fillRect (0, topH - 1, getWidth(), 1);

    const int botH = 124;
    g.setColour (np::col::panelEdge.withAlpha (0.5f));
    g.fillRect (0, getHeight() - botH, getWidth(), 1);
}

void NAMPickEditor::resized()
{
    auto r = getLocalBounds();

    {
        auto top = r.removeFromTop (56);
        top.removeFromLeft (24);
        titleBadge.setBounds    (top.removeFromLeft (220));
        brandLabel.setBounds    (top.withTrimmedRight (16));
        aboutOverlay.setBounds  (getLocalBounds());
    }

    {
        auto footer = r.removeFromBottom (124).reduced (24, 14);
        const int colW = footer.getWidth() / 2;
        auto col1 = footer.removeFromLeft (colW);
        auto col2 = footer;

        mixLabel.setBounds (col1.removeFromTop (14));
        mixKnob.setBounds  (col1.withSizeKeepingCentre (78, 86));

        outLabel.setBounds   (col2.removeFromTop (14));
        outputKnob.setBounds (col2.withSizeKeepingCentre (78, 86));
    }

    {
        auto meterArea = r.removeFromBottom (60).reduced (24, 6);
        meterHeader.setBounds (meterArea.removeFromTop (16));
        meterArea.removeFromTop (6);
        transientPill.setBounds (meterArea.withSizeKeepingCentre (
                                     juce::jmin (meterArea.getWidth(), 320), 22));
    }

    {
        auto centre = r;
        attackHeader.setBounds (centre.removeFromTop (18));
        const int knobSize = juce::jmin (centre.getWidth() - 60, centre.getHeight() - 60);
        const int kS = juce::jmax (200, knobSize);
        auto k = juce::Rectangle<int> (0, 0, kS, kS)
                     .withCentre ({ centre.getCentreX(), centre.getY() + 8 + kS / 2 });
        attackKnob.setBounds (k);

        attackValueLabel.setBounds (juce::Rectangle<int> (k.getCentreX() - 60,
                                                          k.getCentreY() - 9, 120, 18));

        auto ticks = juce::Rectangle<int> (k.getX() - 20, k.getBottom() + 2,
                                           k.getWidth() + 40, 12);
        const int tw = ticks.getWidth() / 3;
        softTickLabel.setBounds    (ticks.removeFromLeft (tw));
        sharpTickLabel.setBounds   (ticks.removeFromRight (tw));
        neutralTickLabel.setBounds (ticks);
    }
}

void NAMPickEditor::timerCallback()
{
    if (transientPill.transientSrc != nullptr)
    {
        const float t = transientPill.transientSrc->load();
        // Fast snap up so the peak is always caught; moderate release
        // (~230 ms at 30 Hz) so each pick blinks visibly.
        const float a = (t > transientPill.disp) ? 0.85f : 0.20f;
        transientPill.disp += (t - transientPill.disp) * a;
        transientPill.repaint();
    }
}
