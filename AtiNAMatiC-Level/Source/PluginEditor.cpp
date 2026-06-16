#include "PluginEditor.h"

namespace
{
    constexpr int kWidth  = 520;
    constexpr int kHeight = 600;
}

NAMLevelEditor::NAMLevelEditor (NAMLevelProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
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

    // Title badge (two-colour, clickable -> About)
    addAndMakeVisible (titleBadge);
    titleBadge.onClick = [this]
    {
        aboutOverlay.setBounds (getLocalBounds());
        aboutOverlay.setVisible (true);
        aboutOverlay.toFront (true);
    };

    // About overlay — hidden until badge is clicked
    addChildComponent (aboutOverlay);

    setupLabel (brandLabel, "Auto output leveler", 11.0f, false,
                juce::Justification::centredRight, na::col::textDim);

    setupLabel (levelValueLabel, "50 %",                             13.0f, true,
                juce::Justification::centred, na::col::text);

    setupLabel (softTickLabel,   "SOFT",                             9.0f, true,
                juce::Justification::centredLeft, na::col::textFaint);
    setupLabel (midTickLabel,    "HOLD",                             9.0f, true,
                juce::Justification::centred, na::col::textFaint);
    setupLabel (lockedTickLabel, "LOCKED",                           9.0f, true,
                juce::Justification::centredRight, na::col::textFaint);

    setupLabel (trimLabel, "TRIM", 10.0f, true, juce::Justification::centred, na::col::textFaint);

    addAndMakeVisible (levelHeader);
    addAndMakeVisible (autoHeader);
    addAndMakeVisible (meterHeader);

    auto setupRotary = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (s);
    };

    setupRotary (levelKnob);
    setupRotary (trimKnob);

    trimKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    trimKnob.setTextValueSuffix (" dB");

    aLevel = std::make_unique<SAtt> (proc.apvts, "level", levelKnob);
    aTrim  = std::make_unique<SAtt> (proc.apvts, "trim",  trimKnob);

    levelKnob.onValueChange = [this]
    {
        levelValueLabel.setText (juce::String (juce::roundToInt (levelKnob.getValue() * 100.0f)) + " %",
                                 juce::dontSendNotification);
    };
    levelKnob.onValueChange();

    addAndMakeVisible (autoToggle);
    if (auto* ap = proc.apvts.getParameter ("autoOn"))
        autoToggle.set (ap->getValue() > 0.5f);
    autoToggle.onChange = [this] (bool on)
    {
        if (auto* ap = proc.apvts.getParameter ("autoOn"))
        {
            ap->beginChangeGesture();
            ap->setValueNotifyingHost (on ? 1.0f : 0.0f);
            ap->endChangeGesture();
        }
    };

    meter.inDbSrc   = &proc.dsp.inputDb;
    meter.outDbSrc  = &proc.dsp.outputDb;
    meter.grSrc     = &proc.dsp.grAmount;
    meter.gainDbSrc = &proc.dsp.gainDb;
    addAndMakeVisible (meter);
    addAndMakeVisible (levelPill);

    startTimerHz (30);
}

NAMLevelEditor::~NAMLevelEditor()
{
    setLookAndFeel (nullptr);
}

void NAMLevelEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (na::col::bg1, (float) getWidth() * 0.5f, 0.0f,
                             na::col::bg0, (float) getWidth() * 0.5f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    auto topH = 56;
    g.setColour (na::col::panel);
    g.fillRect (juce::Rectangle<int> (0, 0, getWidth(), topH));
    g.setColour (na::col::accent);
    g.fillRect (0, topH - 1, getWidth(), 1);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRect (0, topH, getWidth(), 1);

    g.setColour (na::col::accent);
    g.fillRect (16, 14, 3, topH - 28);

    const int botH = 124;
    auto footer = juce::Rectangle<int> (0, getHeight() - botH, getWidth(), botH);
    g.setColour (na::col::panel);
    g.fillRect (footer);
    g.setColour (na::col::panelEdge);
    g.fillRect (footer.getX(), footer.getY(), footer.getWidth(), 1);
}

void NAMLevelEditor::resized()
{
    auto r = getLocalBounds();

    {
        auto top = r.removeFromTop (56);
        top.removeFromLeft (24);
        titleBadge.setBounds  (top.removeFromLeft (210));
        brandLabel.setBounds  (top.withTrimmedRight (16));
        aboutOverlay.setBounds (getLocalBounds());
    }

    // Footer: only TRIM knob (centered).
    {
        auto footer = r.removeFromBottom (124).reduced (24, 14);
        trimLabel.setBounds (footer.removeFromTop (14));
        trimKnob.setBounds  (footer.withSizeKeepingCentre (78, 86));
    }

    {
        auto meterArea = r.removeFromBottom (128).reduced (16, 6);
        meterHeader.setBounds (meterArea.removeFromTop (18));
        auto pillRow  = meterArea.removeFromTop (22);
        levelPill.setBounds (pillRow.withSizeKeepingCentre (140, 22)
                                    .withX (meterArea.getRight() - 148));
        meterArea.removeFromTop (4);
        meter.setBounds (meterArea);
    }

    {
        auto modeArea = r.removeFromBottom (54).reduced (24, 4);
        autoHeader.setBounds (modeArea.removeFromTop (16));
        modeArea.removeFromTop (2);
        autoToggle.setBounds (modeArea.withSizeKeepingCentre (160, modeArea.getHeight()));
    }

    {
        auto centre = r;
        levelHeader.setBounds (centre.removeFromTop (18));
        const int knobSize = juce::jmin (centre.getWidth() - 60, centre.getHeight() - 60);
        const int kS = juce::jmax (180, knobSize);
        auto k = juce::Rectangle<int> (0, 0, kS, kS)
                     .withCentre ({ centre.getCentreX(), centre.getY() + 8 + kS / 2 });
        levelKnob.setBounds (k);

        levelValueLabel.setBounds (juce::Rectangle<int> (k.getCentreX() - 40,
                                                         k.getCentreY() - 9, 80, 18));

        auto ticks = juce::Rectangle<int> (k.getX() - 20, k.getBottom() + 2,
                                           k.getWidth() + 40, 12);
        const int tw = ticks.getWidth() / 3;
        softTickLabel.setBounds   (ticks.removeFromLeft (tw));
        lockedTickLabel.setBounds (ticks.removeFromRight (tw));
        midTickLabel.setBounds    (ticks);
    }
}

void NAMLevelEditor::timerCallback()
{
    if (meter.inDbSrc != nullptr)
    {
        const float in    = meter.inDbSrc->load();
        const float out   = meter.outDbSrc->load();
        const float gr    = meter.grSrc->load();
        const float gainD = meter.gainDbSrc->load();
        meter.inDbDisp   += (in    - meter.inDbDisp)   * 0.40f;
        meter.outDbDisp  += (out   - meter.outDbDisp)  * 0.40f;
        meter.grDisp     += (gr    - meter.grDisp)     * 0.30f;
        meter.gainDbDisp += (gainD - meter.gainDbDisp) * 0.30f;
        meter.repaint();

        levelPill.grDisp     = meter.grDisp;
        levelPill.gainDbDisp = meter.gainDbDisp;
        levelPill.autoOn     = autoToggle.isOn();
        levelPill.repaint();
    }

    // Keep the auto toggle in sync if param is changed externally.
    if (auto* ap = proc.apvts.getParameter ("autoOn"))
    {
        const bool on = ap->getValue() > 0.5f;
        if (on != autoToggle.isOn()) autoToggle.set (on);
    }
}
