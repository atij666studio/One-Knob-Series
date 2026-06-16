#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    constexpr int kWidth  = 520;
    constexpr int kHeight = 600;
}

NAMBoostEditor::NAMBoostEditor (NAMBoostProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    background = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);

    setLookAndFeel (&lnf);
    setSize (kWidth, kHeight);

    auto setupLabel = [this] (juce::Label& l, const juce::String& text,
                              float fontHeight, bool bold,
                              juce::Justification j = juce::Justification::centred,
                              juce::Colour c = nb::col::text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font ("Helvetica Neue", fontHeight,
                               bold ? juce::Font::bold : juce::Font::plain));
        l.setJustificationType (j);
        l.setColour (juce::Label::textColourId, c);
        addAndMakeVisible (l);
    };

    setupLabel (brandLabel,    "Life, punch and clarity",            11.0f, false,
                juce::Justification::centredRight, nb::col::textDim);

    // Title badge — clickable, opens About overlay
    titleBadge.onClick = [this]
    {
        aboutOverlay.setVisible (!aboutOverlay.isVisible());
        aboutOverlay.toFront (false);
    };
    addAndMakeVisible (titleBadge);
    addChildComponent (aboutOverlay);

    setupLabel (boostValueLabel, "35 %",                             13.0f, true,
                juce::Justification::centred, nb::col::text);

    setupLabel (cleanTickLabel,  "CLEAN",                            9.0f, true,
                juce::Justification::centredLeft,  nb::col::text);
    setupLabel (sweetTickLabel,  "SWEET",                            9.0f, true,
                juce::Justification::centred,      nb::col::text);
    setupLabel (pushTickLabel,   "PUSH",                             9.0f, true,
                juce::Justification::centredRight, nb::col::text);

    setupLabel (mixLabel, "MIX",    10.0f, true, juce::Justification::centred, nb::col::text);
    setupLabel (outLabel, "OUTPUT", 10.0f, true, juce::Justification::centred, nb::col::text);
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

    setupRotary (boostKnob);
    setupRotary (mixKnob);
    setupRotary (outputKnob);

    outputKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    outputKnob.setTextValueSuffix (" dB");
    mixKnob.setTextBoxStyle    (juce::Slider::TextBoxBelow, false, 60, 16);

    aBoost = std::make_unique<SAtt> (proc.apvts, "boost",  boostKnob);
    aMix   = std::make_unique<SAtt> (proc.apvts, "mix",    mixKnob);
    aOut   = std::make_unique<SAtt> (proc.apvts, "output", outputKnob);

    boostKnob.onValueChange = [this]
    {
        boostValueLabel.setText (juce::String (juce::roundToInt (boostKnob.getValue() * 100.0f)) + " %",
                                 juce::dontSendNotification);
    };
    boostKnob.onValueChange();

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
    meter.sparkSrc = &proc.dsp.sparkAmount;
    addAndMakeVisible (meter);
    addAndMakeVisible (sparkPill);

    startTimerHz (30);
}

NAMBoostEditor::~NAMBoostEditor()
{
    setLookAndFeel (nullptr);
}

void NAMBoostEditor::paint (juce::Graphics& g)
{
    if (background.isValid())
    {
        g.drawImage (background, getLocalBounds().toFloat());
    }
    else
    {
        juce::ColourGradient bg (nb::col::bg1, (float) getWidth() * 0.5f, 0.0f,
                                 nb::col::bg0, (float) getWidth() * 0.5f, (float) getHeight(), false);
        g.setGradientFill (bg);
        g.fillAll();
    }

    const auto topH = 56;
    g.setColour (nb::col::accent);
    g.fillRect (16, 14, 3, topH - 28);
    g.setColour (nb::col::accent.withAlpha (0.35f));
    g.fillRect (0, topH - 1, getWidth(), 1);

    const int botH = 124;
    g.setColour (nb::col::panelEdge.withAlpha (0.5f));
    g.fillRect (0, getHeight() - botH, getWidth(), 1);
}

void NAMBoostEditor::resized()
{
    auto r = getLocalBounds();

    aboutOverlay.setBounds (getLocalBounds());

    {
        auto top = r.removeFromTop (56);
        top.removeFromLeft (24);
        titleBadge.setBounds  (top.removeFromLeft (200));
        brandLabel.setBounds  (top.withTrimmedRight (16));
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
        sparkPill.setBounds (pillRow.withSizeKeepingCentre (140, 22)
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
        boostHeader.setBounds (centre.removeFromTop (18));
        const int knobSize = juce::jmin (centre.getWidth() - 60, centre.getHeight() - 60);
        const int kS = juce::jmax (180, knobSize);
        auto k = juce::Rectangle<int> (0, 0, kS, kS)
                     .withCentre ({ centre.getCentreX(), centre.getY() + 8 + kS / 2 });
        boostKnob.setBounds (k);

        boostValueLabel.setBounds (juce::Rectangle<int> (k.getCentreX() - 40,
                                                         k.getCentreY() - 9, 80, 18));

        auto ticks = juce::Rectangle<int> (k.getX() - 20, k.getBottom() + 2,
                                           k.getWidth() + 40, 12);
        const int tw = ticks.getWidth() / 3;
        cleanTickLabel.setBounds  (ticks.removeFromLeft (tw));
        pushTickLabel.setBounds   (ticks.removeFromRight (tw));
        sweetTickLabel.setBounds  (ticks);
    }
}

void NAMBoostEditor::timerCallback()
{
    if (meter.inDbSrc != nullptr)
    {
        const float in    = meter.inDbSrc->load();
        const float out   = meter.outDbSrc->load();
        const float spark = meter.sparkSrc->load();
        meter.inDbDisp  += (in    - meter.inDbDisp)  * 0.40f;
        meter.outDbDisp += (out   - meter.outDbDisp) * 0.40f;
        meter.sparkDisp += (spark - meter.sparkDisp) * 0.30f;
        meter.repaint();

        sparkPill.sparkDisp = meter.sparkDisp;
        sparkPill.repaint();
    }
}
