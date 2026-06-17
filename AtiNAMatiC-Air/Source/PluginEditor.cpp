#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    constexpr int kWidth  = 520;
    constexpr int kHeight = 600;
}

AtiNAMatiCAirEditor::AtiNAMatiCAirEditor (AtiNAMatiCAirProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    background = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);

    setLookAndFeel (&lnf);
    setSize (kWidth, kHeight);

    auto setupLabel = [this] (juce::Label& l, const juce::String& text,
                              float fontHeight, bool bold,
                              juce::Justification j = juce::Justification::centred,
                              juce::Colour c = na::col::text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font ("Helvetica Neue", fontHeight,
                               bold ? juce::Font::bold : juce::Font::plain));
        l.setJustificationType (j);
        l.setColour (juce::Label::textColourId, c);
        addAndMakeVisible (l);
    };

    // Title badge (two-colour, single draw, clickable → About)
    addAndMakeVisible (titleBadge);
    titleBadge.onClick = [this]
    {
        aboutOverlay.setBounds (getLocalBounds());
        aboutOverlay.setVisible (true);
        aboutOverlay.toFront (true);
    };

    // About overlay — hidden until badge is clicked
    addChildComponent (aboutOverlay);

    setupLabel (brandLabel,    "Cab feel & amp-in-room",             11.0f, false,
                juce::Justification::centredRight, na::col::textDim);

    setupLabel (airValueLabel,   "40 %",                             13.0f, true,
                juce::Justification::centred, na::col::text);

    setupLabel (dryTickLabel,    "DRY",                              9.0f, true,
                juce::Justification::centredLeft, na::col::text);
    setupLabel (midTickLabel,    "AIR",                              9.0f, true,
                juce::Justification::centred, na::col::text);
    setupLabel (openTickLabel,   "OPEN",                             9.0f, true,
                juce::Justification::centredRight, na::col::text);

    setupLabel (mixLabel, "MIX",    10.0f, true, juce::Justification::centred, na::col::text);
    setupLabel (outLabel, "OUTPUT", 10.0f, true, juce::Justification::centred, na::col::text);

    addAndMakeVisible (airHeader);
    addAndMakeVisible (modeHeader);
    addAndMakeVisible (meterHeader);

    auto setupRotary = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (s);
    };

    setupRotary (airKnob);
    setupRotary (mixKnob);
    setupRotary (outputKnob);

    outputKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    outputKnob.setTextValueSuffix (" dB");
    mixKnob.setTextBoxStyle    (juce::Slider::TextBoxBelow, false, 60, 16);

    aAir = std::make_unique<SAtt> (proc.apvts, "air",    airKnob);
    aMix = std::make_unique<SAtt> (proc.apvts, "mix",    mixKnob);
    aOut = std::make_unique<SAtt> (proc.apvts, "output", outputKnob);

    airKnob.onValueChange = [this]
    {
        airValueLabel.setText (juce::String (juce::roundToInt (airKnob.getValue() * 100.0f)) + " %",
                               juce::dontSendNotification);
    };
    airKnob.onValueChange();

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

    meter.inDbSrc  = &proc.dsp.inputDb;
    meter.outDbSrc = &proc.dsp.outputDb;
    meter.airSrc   = &proc.dsp.airAmount;
    addAndMakeVisible (meter);
    addAndMakeVisible (airPill);

    startTimerHz (30);
}

AtiNAMatiCAirEditor::~AtiNAMatiCAirEditor()
{
    setLookAndFeel (nullptr);
}

void AtiNAMatiCAirEditor::paint (juce::Graphics& g)
{
    if (background.isValid())
    {
        g.drawImage (background, getLocalBounds().toFloat());
    }
    else
    {
        juce::ColourGradient bg (na::col::bg1, (float) getWidth() * 0.5f, 0.0f,
                                 na::col::bg0, (float) getWidth() * 0.5f, (float) getHeight(), false);
        g.setGradientFill (bg);
        g.fillAll();
    }

    const auto topH = 56;
    g.setColour (na::col::accent);
    g.fillRect (16, 14, 3, topH - 28);
    g.setColour (na::col::accent.withAlpha (0.35f));
    g.fillRect (0, topH - 1, getWidth(), 1);

    const int botH = 124;
    g.setColour (na::col::panelEdge.withAlpha (0.5f));
    g.fillRect (0, getHeight() - botH, getWidth(), 1);
}

void AtiNAMatiCAirEditor::resized()
{
    auto r = getLocalBounds();

    {
        auto top = r.removeFromTop (56);
        top.removeFromLeft (24);
        titleBadge.setBounds  (top.removeFromLeft (210));
        brandLabel.setBounds  (top.withTrimmedRight (16));
        aboutOverlay.setBounds (getLocalBounds());
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
        auto meterArea = r.removeFromBottom (128).reduced (16, 6);
        meterHeader.setBounds (meterArea.removeFromTop (18));
        auto pillRow  = meterArea.removeFromTop (22);
        airPill.setBounds (pillRow.withSizeKeepingCentre (140, 22)
                                  .withX (meterArea.getRight() - 148));
        meterArea.removeFromTop (4);
        meter.setBounds (meterArea);
    }

    {
        auto modeArea = r.removeFromBottom (54).reduced (24, 4);
        modeHeader.setBounds (modeArea.removeFromTop (16));
        modeArea.removeFromTop (2);
        modePicker.setBounds (modeArea);
    }

    {
        auto centre = r;
        airHeader.setBounds (centre.removeFromTop (18));
        const int knobSize = juce::jmin (centre.getWidth() - 60, centre.getHeight() - 60);
        const int kS = juce::jmax (180, knobSize);
        auto k = juce::Rectangle<int> (0, 0, kS, kS)
                     .withCentre ({ centre.getCentreX(), centre.getY() + 8 + kS / 2 });
        airKnob.setBounds (k);

        airValueLabel.setBounds (juce::Rectangle<int> (k.getCentreX() - 40,
                                                       k.getCentreY() - 9, 80, 18));

        auto ticks = juce::Rectangle<int> (k.getX() - 20, k.getBottom() + 2,
                                           k.getWidth() + 40, 12);
        const int tw = ticks.getWidth() / 3;
        dryTickLabel.setBounds  (ticks.removeFromLeft (tw));
        openTickLabel.setBounds (ticks.removeFromRight (tw));
        midTickLabel.setBounds  (ticks);
    }
}

void AtiNAMatiCAirEditor::timerCallback()
{
    if (meter.inDbSrc != nullptr)
    {
        const float in    = meter.inDbSrc->load();
        const float out   = meter.outDbSrc->load();
        const float air   = meter.airSrc->load();
        meter.inDbDisp  += (in    - meter.inDbDisp)  * 0.40f;
        meter.outDbDisp += (out   - meter.outDbDisp) * 0.40f;
        meter.airDisp   += (air   - meter.airDisp)   * 0.30f;
        meter.repaint();

        airPill.airDisp = meter.airDisp;
        airPill.repaint();
    }
}
