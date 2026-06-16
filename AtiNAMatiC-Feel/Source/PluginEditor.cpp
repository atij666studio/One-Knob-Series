#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    constexpr int kWidth  = 520;
    constexpr int kHeight = 600;
}

NAMFeelEditor::NAMFeelEditor (NAMFeelProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    background = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);

    setLookAndFeel (&lnf);
    setSize (kWidth, kHeight);

    auto setupLabel = [this] (juce::Label& l, const juce::String& text,
                              float fontHeight, bool bold,
                              juce::Justification j = juce::Justification::centred,
                              juce::Colour c = nf::col::text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font ("Helvetica Neue", fontHeight,
                               bold ? juce::Font::bold : juce::Font::plain));
        l.setJustificationType (j);
        l.setColour (juce::Label::textColourId, c);
        addAndMakeVisible (l);
    };

    // Title badge (two-colour clickable heading)
    addAndMakeVisible (titleBadge);
    titleBadge.onClick = [this] { aboutOverlay.setVisible (true); aboutOverlay.toFront (false); };

    // About overlay (full-window, starts hidden)
    addChildComponent (aboutOverlay);

    setupLabel (brandLabel, "Feel Enhancer", 11.0f, false,
                juce::Justification::centredRight, nf::col::textDim);

    setupLabel (assistValueLabel, "35 %",                             13.0f, true,
                juce::Justification::centred, nf::col::text);

    setupLabel (rawTickLabel,    "RAW",                                9.0f, true,
                juce::Justification::centredLeft, nf::col::textFaint);
    setupLabel (midTickLabel,    "FEEL",                               9.0f, true,
                juce::Justification::centred, nf::col::textFaint);
    setupLabel (smoothTickLabel, "SMOOTH",                             9.0f, true,
                juce::Justification::centredRight, nf::col::textFaint);

    setupLabel (outLabel, "OUTPUT", 10.0f, true, juce::Justification::centred, nf::col::textFaint);

    addAndMakeVisible (assistHeader);
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

    setupRotary (assistKnob);
    setupRotary (outputKnob);

    outputKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    outputKnob.setTextValueSuffix (" dB");

    aAssist = std::make_unique<SAtt> (proc.apvts, "assist", assistKnob);
    aOut    = std::make_unique<SAtt> (proc.apvts, "output", outputKnob);

    assistKnob.onValueChange = [this]
    {
        assistValueLabel.setText (juce::String (juce::roundToInt (assistKnob.getValue() * 100.0f)) + " %",
                                  juce::dontSendNotification);
        feelPill.assist = (float) assistKnob.getValue();
        feelPill.repaint();
    };
    assistKnob.onValueChange();

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

    meter.inDbSrc   = &proc.dsp.inputDb;
    meter.outDbSrc  = &proc.dsp.outputDb;
    meter.grDbSrc   = &proc.dsp.grDb;
    meter.smoothSrc = &proc.dsp.smoothAmount;
    addAndMakeVisible (meter);
    addAndMakeVisible (feelPill);

    startTimerHz (30);
}

NAMFeelEditor::~NAMFeelEditor()
{
    setLookAndFeel (nullptr);
}

void NAMFeelEditor::paint (juce::Graphics& g)
{
    if (background.isValid())
    {
        g.drawImage (background, getLocalBounds().toFloat());
    }
    else
    {
        juce::ColourGradient bg (nf::col::bg1, (float) getWidth() * 0.5f, 0.0f,
                                 nf::col::bg0, (float) getWidth() * 0.5f, (float) getHeight(), false);
        g.setGradientFill (bg);
        g.fillAll();
    }

    const auto topH = 56;
    g.setColour (nf::col::accent);
    g.fillRect (16, 14, 3, topH - 28);
    g.setColour (nf::col::accent.withAlpha (0.35f));
    g.fillRect (0, topH - 1, getWidth(), 1);

    const int botH = 124;
    g.setColour (nf::col::panelEdge.withAlpha (0.5f));
    g.fillRect (0, getHeight() - botH, getWidth(), 1);
}

void NAMFeelEditor::resized()
{
    auto r = getLocalBounds();

    {
        auto top = r.removeFromTop (56);
        top.removeFromLeft (24);
        titleBadge.setBounds (top.removeFromLeft (220));
        brandLabel.setBounds (top.withTrimmedRight (16));
    }

    // About overlay always covers the full window
    aboutOverlay.setBounds (getLocalBounds());

    {
        // Single OUTPUT knob centred in the footer.
        auto footer = r.removeFromBottom (124).reduced (24, 14);
        outLabel.setBounds   (footer.removeFromTop (14));
        outputKnob.setBounds (footer.withSizeKeepingCentre (84, 92));
    }

    {
        auto meterArea = r.removeFromBottom (128).reduced (16, 6);
        meterHeader.setBounds (meterArea.removeFromTop (18));
        auto pillRow  = meterArea.removeFromTop (22);
        feelPill.setBounds (pillRow.withSizeKeepingCentre (150, 22)
                                    .withX (meterArea.getRight() - 158));
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
        assistHeader.setBounds (centre.removeFromTop (18));
        const int knobSize = juce::jmin (centre.getWidth() - 60, centre.getHeight() - 60);
        const int kS = juce::jmax (180, knobSize);
        auto k = juce::Rectangle<int> (0, 0, kS, kS)
                     .withCentre ({ centre.getCentreX(), centre.getY() + 8 + kS / 2 });
        assistKnob.setBounds (k);

        assistValueLabel.setBounds (juce::Rectangle<int> (k.getCentreX() - 40,
                                                          k.getCentreY() - 9, 80, 18));

        auto ticks = juce::Rectangle<int> (k.getX() - 20, k.getBottom() + 2,
                                           k.getWidth() + 40, 12);
        const int tw = ticks.getWidth() / 3;
        rawTickLabel.setBounds    (ticks.removeFromLeft (tw));
        smoothTickLabel.setBounds (ticks.removeFromRight (tw));
        midTickLabel.setBounds    (ticks);
    }
}

void NAMFeelEditor::timerCallback()
{
    if (meter.inDbSrc != nullptr)
    {
        const float in   = meter.inDbSrc->load();
        const float out  = meter.outDbSrc->load();
        const float gr   = meter.grDbSrc->load();
        const float sm   = meter.smoothSrc->load();
        meter.inDbDisp   += (in  - meter.inDbDisp)  * 0.40f;
        meter.outDbDisp  += (out - meter.outDbDisp) * 0.40f;
        meter.grDbDisp   += (gr  - meter.grDbDisp)  * 0.35f;
        meter.smoothDisp += (sm  - meter.smoothDisp) * 0.30f;
        meter.repaint();

        feelPill.smoothDisp = meter.smoothDisp;
        feelPill.repaint();
    }
}
