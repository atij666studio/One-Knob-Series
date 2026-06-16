#include "PluginEditor.h"

namespace
{
    constexpr int kWidth  = 520;
    constexpr int kHeight = 540;
}

NAMTightEditor::NAMTightEditor (NAMTightProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (kWidth, kHeight);

    // About overlay must be added first so all controls paint on top.
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
                              juce::Colour c = nt::col::text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font ("Helvetica Neue", fontHeight,
                               bold ? juce::Font::bold : juce::Font::plain));
        l.setJustificationType (j);
        l.setColour (juce::Label::textColourId, c);
        addAndMakeVisible (l);
    };

    setupLabel (brandLabel,      "Tight, silent, and fast",              11.0f, false,
                juce::Justification::centredRight, nt::col::textDim);
    setupLabel (tightLabel,      "TIGHT",                                14.0f, true);
    setupLabel (looseTickLabel,  "Loose",                                10.0f, false,
                juce::Justification::centredLeft,  nt::col::textFaint);
    setupLabel (tightTickLabel,  "Tight",                                10.0f, false,
                juce::Justification::centredRight, nt::col::textFaint);
    setupLabel (modeLabel,       "MODE",                                 10.0f, true,
                juce::Justification::centredLeft,  nt::col::textFaint);
    setupLabel (mixLabel,        "MIX",                                  10.0f, true,
                juce::Justification::centred,      nt::col::textFaint);
    setupLabel (outLabel,        "OUTPUT",                               10.0f, true,
                juce::Justification::centred,      nt::col::textFaint);
    setupLabel (meterCaption,    "LOW END",                              10.0f, true,
                juce::Justification::centredLeft,  nt::col::textFaint);

    auto setupRotary = [this] (juce::Slider& s, juce::Slider::Listener* /*l*/)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (s);
    };

    setupRotary (tightKnob,    nullptr);
    setupRotary (mixKnob,      nullptr);
    setupRotary (outputKnob,   nullptr);

    // Show readouts where they read nicely.
    tightKnob.setTextValueSuffix ("");
    outputKnob.setTextBoxStyle  (juce::Slider::TextBoxBelow, false, 64, 16);
    outputKnob.setTextValueSuffix (" dB");
    mixKnob.setTextBoxStyle     (juce::Slider::TextBoxBelow, false, 56, 16);

    aTight = std::make_unique<SAtt> (proc.apvts, "tight",  tightKnob);
    aMix   = std::make_unique<SAtt> (proc.apvts, "mix",    mixKnob);
    aOut   = std::make_unique<SAtt> (proc.apvts, "output", outputKnob);

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

    meter.sourceDb = &proc.dsp.lowEnergyDb;
    addAndMakeVisible (meter);

    startTimerHz (30);
}

NAMTightEditor::~NAMTightEditor()
{
    setLookAndFeel (nullptr);
}

void NAMTightEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (nt::col::bg1, (float) getWidth() * 0.5f, 0.0f,
                             nt::col::bg0, (float) getWidth() * 0.5f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Top brand bar
    const int topH = 56;
    g.setColour (nt::col::panel);
    g.fillRect (juce::Rectangle<int> (0, 0, getWidth(), topH));
    g.setColour (nt::col::accent);
    g.fillRect (0, topH - 1, getWidth(), 1);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRect (0, topH, getWidth(), 1);

    // Side accent strip on title
    g.setColour (nt::col::accent);
    g.fillRect (16, 14, 3, topH - 28);
}

void NAMTightEditor::resized()
{
    auto r = getLocalBounds();

    // ---- Top brand bar -------------------------------------------------
    {
        auto top = r.removeFromTop (56);
        top.removeFromLeft (24);
        titleBadge.setBounds   (top.removeFromLeft (220));
        brandLabel.setBounds   (top.withTrimmedRight (16));
        aboutOverlay.setBounds (getLocalBounds());
    }

    // ---- Low end meter strip just above bottom -------------------------
    {
        auto meterRow = r.removeFromBottom (32).reduced (16, 6);
        meterCaption.setBounds (meterRow.removeFromLeft (70));
        meter.setBounds        (meterRow.reduced (4, 0));
    }

    // ---- Mix / Output side-by-side -------------------------------------
    {
        auto knobs = r.removeFromBottom (110).reduced (24, 4);
        const int colW = knobs.getWidth() / 2;
        auto col1 = knobs.removeFromLeft (colW);
        auto col2 = knobs;

        mixLabel.setBounds (col1.removeFromTop (16));
        mixKnob.setBounds  (col1.withSizeKeepingCentre (78, 78));

        outLabel.setBounds  (col2.removeFromTop (16));
        outputKnob.setBounds(col2.withSizeKeepingCentre (78, 78));
    }

    // ---- Mode picker ---------------------------------------------------
    {
        auto modeArea = r.removeFromBottom (56).reduced (24, 4);
        modeLabel.setBounds (modeArea.removeFromTop (16));
        modeArea.removeFromTop (4);
        modePicker.setBounds (modeArea);
    }

    // ---- Big TIGHT knob fills what's left ------------------------------
    {
        auto centre = r;
        const int knobSize = juce::jmin (centre.getWidth() - 60, centre.getHeight() - 70);
        const int kS = juce::jmax (180, knobSize);
        auto k = juce::Rectangle<int> (0, 0, kS, kS)
                     .withCentre ({ centre.getCentreX(), centre.getY() + 12 + kS / 2 });
        tightKnob.setBounds (k);

        auto labelLine = juce::Rectangle<int> (k.getX(), k.getBottom() + 4, k.getWidth(), 18);
        tightLabel.setBounds (labelLine);

        auto ticks = juce::Rectangle<int> (k.getX() - 30, k.getBottom() + 24, k.getWidth() + 60, 14);
        looseTickLabel.setBounds (ticks.removeFromLeft (60));
        tightTickLabel.setBounds (ticks.removeFromRight (60));
    }
}

void NAMTightEditor::timerCallback()
{
    if (meter.sourceDb != nullptr)
    {
        const float v = meter.sourceDb->load();
        // Light smoothing for visuals.
        meter.currentDb += (v - meter.currentDb) * 0.25f;
        meter.repaint();
    }
}
