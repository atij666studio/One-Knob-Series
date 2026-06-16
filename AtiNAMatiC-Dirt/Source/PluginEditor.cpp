#include "PluginEditor.h"

namespace
{
    constexpr int kWidth  = 520;
    constexpr int kHeight = 600;
}

NAMDirtEditor::NAMDirtEditor (NAMDirtProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (kWidth, kHeight);

    auto setupLabel = [this] (juce::Label& l, const juce::String& text,
                              float fontHeight, bool bold,
                              juce::Justification j = juce::Justification::centred,
                              juce::Colour c = nd::col::text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font ("Helvetica Neue", fontHeight,
                               bold ? juce::Font::bold : juce::Font::plain));
        l.setJustificationType (j);
        l.setColour (juce::Label::textColourId, c);
        addAndMakeVisible (l);
    };

    setupLabel (brandLabel,    "Attitude without distortion",        11.0f, false,
                juce::Justification::centredRight, nd::col::textDim);

    addAndMakeVisible (titleBadge);
    titleBadge.onClick = [this]
    {
        aboutOverlay.setVisible (true);
        aboutOverlay.toFront (false);
    };
    addChildComponent (aboutOverlay);

    setupLabel (dirtValueLabel, "30 %",                              13.0f, true,
                juce::Justification::centred, nd::col::text);

    setupLabel (cleanTickLabel,  "CLEAN",                            9.0f, true,
                juce::Justification::centredLeft, nd::col::textFaint);
    setupLabel (midTickLabel,    "EDGE",                             9.0f, true,
                juce::Justification::centred, nd::col::textFaint);
    setupLabel (gritTickLabel,   "GRIT",                             9.0f, true,
                juce::Justification::centredRight, nd::col::textFaint);

    setupLabel (mixLabel, "MIX",    10.0f, true, juce::Justification::centred, nd::col::textFaint);
    setupLabel (outLabel, "OUTPUT", 10.0f, true, juce::Justification::centred, nd::col::textFaint);

    addAndMakeVisible (dirtHeader);
    addAndMakeVisible (typeHeader);
    addAndMakeVisible (meterHeader);

    auto setupRotary = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (s);
    };

    setupRotary (dirtKnob);
    setupRotary (mixKnob);
    setupRotary (outputKnob);

    outputKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    outputKnob.setTextValueSuffix (" dB");
    mixKnob.setTextBoxStyle    (juce::Slider::TextBoxBelow, false, 60, 16);

    aDirt = std::make_unique<SAtt> (proc.apvts, "dirt",   dirtKnob);
    aMix  = std::make_unique<SAtt> (proc.apvts, "mix",    mixKnob);
    aOut  = std::make_unique<SAtt> (proc.apvts, "output", outputKnob);

    dirtKnob.onValueChange = [this]
    {
        dirtValueLabel.setText (juce::String (juce::roundToInt (dirtKnob.getValue() * 100.0f)) + " %",
                                juce::dontSendNotification);
    };
    dirtKnob.onValueChange();

    addAndMakeVisible (typePicker);
    if (auto* mp = proc.apvts.getParameter ("type"))
        typePicker.setIndex ((int) mp->convertFrom0to1 (mp->getValue()));
    typePicker.onChange = [this] (int ix)
    {
        if (auto* mp = proc.apvts.getParameter ("type"))
        {
            mp->beginChangeGesture();
            mp->setValueNotifyingHost (mp->convertTo0to1 ((float) ix));
            mp->endChangeGesture();
        }
    };

    meter.inDbSrc  = &proc.dsp.inputDb;
    meter.outDbSrc = &proc.dsp.outputDb;
    meter.gritSrc  = &proc.dsp.gritAmount;
    addAndMakeVisible (meter);
    addAndMakeVisible (gritPill);

    startTimerHz (30);
}

NAMDirtEditor::~NAMDirtEditor()
{
    setLookAndFeel (nullptr);
}

void NAMDirtEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (nd::col::bg1, (float) getWidth() * 0.5f, 0.0f,
                             nd::col::bg0, (float) getWidth() * 0.5f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    auto topH = 56;
    g.setColour (nd::col::panel);
    g.fillRect (juce::Rectangle<int> (0, 0, getWidth(), topH));
    g.setColour (nd::col::accent);
    g.fillRect (0, topH - 1, getWidth(), 1);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRect (0, topH, getWidth(), 1);

    g.setColour (nd::col::accent);
    g.fillRect (16, 14, 3, topH - 28);

    const int botH = 124;
    auto footer = juce::Rectangle<int> (0, getHeight() - botH, getWidth(), botH);
    g.setColour (nd::col::panel);
    g.fillRect (footer);
    g.setColour (nd::col::panelEdge);
    g.fillRect (footer.getX(), footer.getY(), footer.getWidth(), 1);
}

void NAMDirtEditor::resized()
{
    auto r = getLocalBounds();

    {
        auto top = r.removeFromTop (56);
        top.removeFromLeft (24);
        titleBadge.setBounds    (top.removeFromLeft (180));
        brandLabel.setBounds    (top.withTrimmedRight (16));
    }

    aboutOverlay.setBounds (getLocalBounds());

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
        gritPill.setBounds (pillRow.withSizeKeepingCentre (140, 22)
                                    .withX (meterArea.getRight() - 148));
        meterArea.removeFromTop (4);
        meter.setBounds (meterArea);
    }

    {
        auto typeArea = r.removeFromBottom (54).reduced (24, 4);
        typeHeader.setBounds (typeArea.removeFromTop (16));
        typeArea.removeFromTop (2);
        typePicker.setBounds (typeArea);
    }

    {
        auto centre = r;
        dirtHeader.setBounds (centre.removeFromTop (18));
        const int knobSize = juce::jmin (centre.getWidth() - 60, centre.getHeight() - 60);
        const int kS = juce::jmax (180, knobSize);
        auto k = juce::Rectangle<int> (0, 0, kS, kS)
                     .withCentre ({ centre.getCentreX(), centre.getY() + 8 + kS / 2 });
        dirtKnob.setBounds (k);

        dirtValueLabel.setBounds (juce::Rectangle<int> (k.getCentreX() - 40,
                                                        k.getCentreY() - 9, 80, 18));

        auto ticks = juce::Rectangle<int> (k.getX() - 20, k.getBottom() + 2,
                                           k.getWidth() + 40, 12);
        const int tw = ticks.getWidth() / 3;
        cleanTickLabel.setBounds (ticks.removeFromLeft (tw));
        gritTickLabel.setBounds  (ticks.removeFromRight (tw));
        midTickLabel.setBounds   (ticks);
    }
}

void NAMDirtEditor::timerCallback()
{
    if (meter.inDbSrc != nullptr)
    {
        const float in   = meter.inDbSrc->load();
        const float out  = meter.outDbSrc->load();
        const float grit = meter.gritSrc->load();
        meter.inDbDisp  += (in   - meter.inDbDisp)  * 0.40f;
        meter.outDbDisp += (out  - meter.outDbDisp) * 0.40f;
        meter.gritDisp  += (grit - meter.gritDisp)  * 0.30f;
        meter.repaint();

        gritPill.gritDisp = meter.gritDisp;
        gritPill.repaint();
    }
}
