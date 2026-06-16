#include "PluginEditor.h"

namespace
{
    constexpr int kWidth  = 520;
    constexpr int kHeight = 600;
}

NAMGateEditor::NAMGateEditor (NAMGateProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (kWidth, kHeight);

    // About overlay must be added first so all other children paint on top.
    addChildComponent (aboutOverlay);

    addAndMakeVisible (titleBadge);
    titleBadge.onClick = [this]
    {
        aboutOverlay.setVisible (true);
        aboutOverlay.toFront (false);
    };

    auto setupLabel = [this] (juce::Label& l, const juce::String& text,
                              float fontHeight, bool bold,
                              juce::Justification j = juce::Justification::centred,
                              juce::Colour c = ng::col::text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font ("Helvetica Neue", fontHeight,
                               bold ? juce::Font::bold : juce::Font::plain));
        l.setJustificationType (j);
        l.setColour (juce::Label::textColourId, c);
        addAndMakeVisible (l);
    };

    setupLabel (brandLabel,        "Tight, silent, and natural",         11.0f, false,
                juce::Justification::centredRight, ng::col::textDim);

    setupLabel (gateValueLabel,    "35 %",                               13.0f, true,
                juce::Justification::centred, ng::col::text);

    setupLabel (openTickLabel,     "OPEN",                               9.0f, true,
                juce::Justification::centredLeft, ng::col::textFaint);
    setupLabel (trackingTickLabel, "TRACKING",                           9.0f, true,
                juce::Justification::centred, ng::col::textFaint);
    setupLabel (tightTickLabel,    "TIGHT",                              9.0f, true,
                juce::Justification::centredRight, ng::col::textFaint);

    setupLabel (mixLabel,          "MIX",                                10.0f, true,
                juce::Justification::centred, ng::col::textFaint);
    setupLabel (outLabel,          "OUTPUT",                             10.0f, true,
                juce::Justification::centred, ng::col::textFaint);

    addAndMakeVisible (gateHeader);
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

    setupRotary (gateKnob);
    setupRotary (mixKnob);
    setupRotary (outputKnob);

    outputKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    outputKnob.setTextValueSuffix (" dB");
    mixKnob.setTextBoxStyle    (juce::Slider::TextBoxBelow, false, 60, 16);

    aGate = std::make_unique<SAtt> (proc.apvts, "gate",   gateKnob);
    aMix  = std::make_unique<SAtt> (proc.apvts, "mix",    mixKnob);
    aOut  = std::make_unique<SAtt> (proc.apvts, "output", outputKnob);

    gateKnob.onValueChange = [this]
    {
        gateValueLabel.setText (juce::String (juce::roundToInt (gateKnob.getValue() * 100.0f)) + " %",
                                juce::dontSendNotification);
    };
    gateKnob.onValueChange();

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

    meter.inputDbSrc       = &proc.dsp.inputDb;
    meter.threshDbSrc      = &proc.dsp.openThreshDb;
    meter.gainReductionSrc = &proc.dsp.gateGainDb;
    meter.stateSrc         = &proc.dsp.stateAtomic;
    addAndMakeVisible (meter);
    addAndMakeVisible (statePill);

    startTimerHz (30);
}

NAMGateEditor::~NAMGateEditor()
{
    setLookAndFeel (nullptr);
}

void NAMGateEditor::paint (juce::Graphics& g)
{
    // Background gradient (slight vignette feel)
    juce::ColourGradient bg (ng::col::bg1, (float) getWidth() * 0.5f, 0.0f,
                             ng::col::bg0, (float) getWidth() * 0.5f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Top brand bar
    auto topH = 56;
    g.setColour (ng::col::panel);
    g.fillRect (juce::Rectangle<int> (0, 0, getWidth(), topH));
    g.setColour (ng::col::accent);
    g.fillRect (0, topH - 1, getWidth(), 1);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRect (0, topH, getWidth(), 1);

    // Side accent strip on the title
    g.setColour (ng::col::accent);
    g.fillRect (16, 14, 3, topH - 28);

    // Footer panel for Mix / Output
    const int botH = 124;
    auto footer = juce::Rectangle<int> (0, getHeight() - botH, getWidth(), botH);
    g.setColour (ng::col::panel);
    g.fillRect (footer);
    g.setColour (ng::col::panelEdge);
    g.fillRect (footer.getX(), footer.getY(), footer.getWidth(), 1);
}

void NAMGateEditor::resized()
{
    auto r = getLocalBounds();

    // ---- Top brand bar -------------------------------------------------
    {
        auto top = r.removeFromTop (56);
        top.removeFromLeft (24);
        titleBadge.setBounds    (top.removeFromLeft (220));
        brandLabel.setBounds    (top.withTrimmedRight (16));
        aboutOverlay.setBounds  (getLocalBounds());
    }

    // ---- Footer (Mix / Output side-by-side) ----------------------------
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

    // ---- Activity (meter) section --------------------------------------
    {
        auto meterArea = r.removeFromBottom (128).reduced (16, 6);
        meterHeader.setBounds (meterArea.removeFromTop (18));
        auto pillRow  = meterArea.removeFromTop (22);
        statePill.setBounds (pillRow.withSizeKeepingCentre (140, 22)
                                    .withX (meterArea.getRight() - 148));
        meterArea.removeFromTop (4);
        meter.setBounds (meterArea);
    }

    // ---- Mode section --------------------------------------------------
    {
        auto modeArea = r.removeFromBottom (54).reduced (24, 4);
        modeHeader.setBounds (modeArea.removeFromTop (16));
        modeArea.removeFromTop (2);
        modePicker.setBounds (modeArea);
    }

    // ---- Big GATE knob fills the rest ----------------------------------
    {
        auto centre = r;
        gateHeader.setBounds (centre.removeFromTop (18));
        const int knobSize = juce::jmin (centre.getWidth() - 60, centre.getHeight() - 60);
        const int kS = juce::jmax (180, knobSize);
        auto k = juce::Rectangle<int> (0, 0, kS, kS)
                     .withCentre ({ centre.getCentreX(), centre.getY() + 8 + kS / 2 });
        gateKnob.setBounds (k);

        gateValueLabel.setBounds (juce::Rectangle<int> (k.getCentreX() - 40,
                                                        k.getCentreY() - 9, 80, 18));

        auto ticks = juce::Rectangle<int> (k.getX() - 20, k.getBottom() + 2,
                                           k.getWidth() + 40, 12);
        const int tw = ticks.getWidth() / 3;
        openTickLabel.setBounds     (ticks.removeFromLeft (tw));
        tightTickLabel.setBounds    (ticks.removeFromRight (tw));
        trackingTickLabel.setBounds (ticks);
    }
}

void NAMGateEditor::timerCallback()
{
    if (meter.inputDbSrc != nullptr)
    {
        const float in  = meter.inputDbSrc->load();
        const float thr = meter.threshDbSrc->load();
        const float gr  = meter.gainReductionSrc->load();
        const int   st  = meter.stateSrc->load();
        meter.inputDbDisp  += (in  - meter.inputDbDisp)  * 0.40f;
        meter.threshDbDisp += (thr - meter.threshDbDisp) * 0.30f;
        meter.grDbDisp     += (gr  - meter.grDbDisp)     * 0.40f;
        meter.stateDisp    = st;
        meter.repaint();

        statePill.stateDisp = st;
        statePill.repaint();
    }
}
