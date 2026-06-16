#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    constexpr int kWidth  = 520;
    constexpr int kHeight = 540;
}

NAMSquashEditor::NAMSquashEditor (NAMSquashProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    background = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);

    setLookAndFeel (&lnf);
    setSize (kWidth, kHeight);

    addChildComponent (aboutOverlay);

    addAndMakeVisible (titleBadge);
    titleBadge.onClick = [this]
    {
        aboutOverlay.setVisible (true);
        aboutOverlay.toFront (false);
    };

    auto setupLabel = [this] (juce::Label& l, const juce::String& text,
                              float fontH, bool bold,
                              juce::Justification j = juce::Justification::centred,
                              juce::Colour c = sq::col::text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font ("Helvetica Neue", fontH,
                               bold ? juce::Font::bold : juce::Font::plain));
        l.setJustificationType (j);
        l.setColour (juce::Label::textColourId, c);
        addAndMakeVisible (l);
    };

    setupLabel (brandLabel,     "Squash, punch, and glue",  11.0f, false,
                juce::Justification::centredRight, sq::col::textDim);
    setupLabel (squashLabel,    "SQUASH",                   14.0f, true);
    setupLabel (lightTickLabel, "Light",                    10.0f, false,
                juce::Justification::centredLeft,  sq::col::textFaint);
    setupLabel (heavyTickLabel, "Heavy",                    10.0f, false,
                juce::Justification::centredRight, sq::col::textFaint);
    setupLabel (modeLabel,      "MODE",                     10.0f, true,
                juce::Justification::centredLeft,  sq::col::textFaint);
    setupLabel (mixLabel,       "MIX",                      10.0f, true,
                juce::Justification::centred,      sq::col::textFaint);
    setupLabel (outLabel,       "OUTPUT",                   10.0f, true,
                juce::Justification::centred,      sq::col::textFaint);
    setupLabel (grCaption,      "GR",                       10.0f, true,
                juce::Justification::centredLeft,  sq::col::textFaint);
    setupLabel (grMaxLabel,     "18 dB",                    10.0f, false,
                juce::Justification::centredRight, sq::col::textFaint);

    auto setupRotary = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (s);
    };

    setupRotary (squashKnob);
    setupRotary (mixKnob);
    setupRotary (outputKnob);

    outputKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    outputKnob.setTextValueSuffix (" dB");
    mixKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);

    aSquash = std::make_unique<SAtt> (proc.apvts, "squash", squashKnob);
    aMix    = std::make_unique<SAtt> (proc.apvts, "mix",    mixKnob);
    aOut    = std::make_unique<SAtt> (proc.apvts, "output", outputKnob);

    addAndMakeVisible (modePicker);
    if (auto* mp = proc.apvts.getParameter ("mode"))
        modePicker.setIndex ((int) mp->convertFrom0to1 (mp->getValue()));
    modePicker.onChange = [this] (int ix)
    {
        if (auto* mp = proc.apvts.getParameter ("mode"))
        {
            mp->beginChangeGesture();
            mp->setValueNotifyingHost (mp->convertTo0to1 ((float) ix));
            mp->endChangeGesture();
        }
    };

    grMeter.sourceGrDb = &proc.dsp.grDb;
    addAndMakeVisible (grMeter);

    startTimerHz (30);
}

NAMSquashEditor::~NAMSquashEditor()
{
    setLookAndFeel (nullptr);
}

void NAMSquashEditor::paint (juce::Graphics& g)
{
    if (background.isValid())
    {
        g.drawImage (background, getLocalBounds().toFloat());
    }
    else
    {
        juce::ColourGradient bg (sq::col::bg1, (float) getWidth() * 0.5f, 0.0f,
                                 sq::col::bg0, (float) getWidth() * 0.5f, (float) getHeight(), false);
        g.setGradientFill (bg);
        g.fillAll();
    }

    const int topH = 56;
    g.setColour (sq::col::accent);
    g.fillRect (16, 14, 3, topH - 28);
    g.setColour (sq::col::accent.withAlpha (0.35f));
    g.fillRect (0, topH - 1, getWidth(), 1);
}

void NAMSquashEditor::resized()
{
    auto r = getLocalBounds();

    // ---- Top brand bar -------------------------------------------------
    {
        auto top = r.removeFromTop (56);
        top.removeFromLeft (24);
        titleBadge.setBounds (top.removeFromLeft (230));
        brandLabel.setBounds (top.withTrimmedRight (16));
        aboutOverlay.setBounds (getLocalBounds());
    }

    // ---- GR meter strip at bottom -------------------------------------
    {
        auto meterRow = r.removeFromBottom (48).reduced (16, 8);
        grCaption.setBounds  (meterRow.removeFromLeft (28));
        grMaxLabel.setBounds (meterRow.removeFromRight (40));
        grMeter.setBounds    (meterRow.reduced (4, 0));
    }

    // ---- Mix / Output side-by-side -------------------------------------
    {
        auto knobs = r.removeFromBottom (110).reduced (24, 4);
        const int colW = knobs.getWidth() / 2;
        auto col1 = knobs.removeFromLeft (colW);
        auto col2 = knobs;

        mixLabel.setBounds (col1.removeFromTop (16));
        mixKnob.setBounds  (col1.withSizeKeepingCentre (78, 78));

        outLabel.setBounds   (col2.removeFromTop (16));
        outputKnob.setBounds (col2.withSizeKeepingCentre (78, 78));
    }

    // ---- Mode picker ---------------------------------------------------
    {
        auto modeArea = r.removeFromBottom (56).reduced (24, 4);
        modeLabel.setBounds (modeArea.removeFromTop (16));
        modeArea.removeFromTop (4);
        modePicker.setBounds (modeArea);
    }

    // ---- Big SQUASH knob fills what's left ----------------------------
    {
        auto centre = r;
        const int kS = juce::jmax (180,
            juce::jmin (centre.getWidth() - 60, centre.getHeight() - 70));
        auto k = juce::Rectangle<int> (0, 0, kS, kS)
                     .withCentre ({ centre.getCentreX(), centre.getY() + 12 + kS / 2 });
        squashKnob.setBounds (k);

        squashLabel.setBounds (
            juce::Rectangle<int> (k.getX(), k.getBottom() + 4, k.getWidth(), 18));

        auto ticks = juce::Rectangle<int> (k.getX() - 30, k.getBottom() + 24,
                                            k.getWidth() + 60, 14);
        lightTickLabel.setBounds (ticks.removeFromLeft (50));
        heavyTickLabel.setBounds (ticks.removeFromRight (50));
    }
}

void NAMSquashEditor::timerCallback()
{
    if (grMeter.sourceGrDb == nullptr) return;

    const float v = grMeter.sourceGrDb->load();  // ≤ 0, DSP already has ballistics
    grMeter.currentGrDb = v;

    // Peak hold: latch the most-negative value, hold 60 frames (~2 s), then decay
    if (v < grMeter.peakGrDb)
    {
        grMeter.peakGrDb      = v;
        grMeter.peakHoldFrames = 60;
    }
    else if (grMeter.peakHoldFrames > 0)
    {
        --grMeter.peakHoldFrames;
    }
    else
    {
        // Slow decay back to zero
        grMeter.peakGrDb += (0.0f - grMeter.peakGrDb) * 0.04f;
        if (std::abs (grMeter.peakGrDb) < 0.01f) grMeter.peakGrDb = 0.0f;
    }

    grMeter.repaint();
}
