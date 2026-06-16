#include "PluginEditor.h"

namespace
{
    constexpr int kWidth  = 520;
    constexpr int kHeight = 600;
}

NAMDoubleEditor::NAMDoubleEditor (NAMDoubleProcessor& p)
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

    // Title badge (two-colour clickable heading)
    addAndMakeVisible (titleBadge);
    titleBadge.onClick = [this] { aboutOverlay.setVisible (true); aboutOverlay.toFront (false); };

    // About overlay (full-window, starts hidden)
    addChildComponent (aboutOverlay);

    setupLabel (brandLabel,        "Instant double tracking -- no chorus", 11.0f, false,
                juce::Justification::centredRight, nd::col::textDim);

    setupLabel (widthValueLabel,   "45 %",                               13.0f, true,
                juce::Justification::centred, nd::col::text);

    setupLabel (monoTickLabel,     "MONO",                               9.0f, true,
                juce::Justification::centredLeft, nd::col::textFaint);
    setupLabel (doubleTickLabel,   "DOUBLE",                             9.0f, true,
                juce::Justification::centred, nd::col::textFaint);
    setupLabel (wideTickLabel,     "WIDE",                               9.0f, true,
                juce::Justification::centredRight, nd::col::textFaint);

    setupLabel (mixLabel,          "MIX",                                10.0f, true,
                juce::Justification::centred, nd::col::textFaint);
    setupLabel (outLabel,          "OUTPUT",                             10.0f, true,
                juce::Justification::centred, nd::col::textFaint);

    addAndMakeVisible (widthHeader);
    addAndMakeVisible (modeHeader);
    addAndMakeVisible (imageHeader);

    auto setupRotary = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (s);
    };

    setupRotary (widthKnob);
    setupRotary (mixKnob);
    setupRotary (outputKnob);

    outputKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    outputKnob.setTextValueSuffix (" dB");
    mixKnob.setTextBoxStyle    (juce::Slider::TextBoxBelow, false, 60, 16);

    aWidth = std::make_unique<SAtt> (proc.apvts, "width",  widthKnob);
    aMix   = std::make_unique<SAtt> (proc.apvts, "mix",    mixKnob);
    aOut   = std::make_unique<SAtt> (proc.apvts, "output", outputKnob);

    widthKnob.onValueChange = [this]
    {
        widthValueLabel.setText (juce::String (juce::roundToInt (widthKnob.getValue() * 100.0f)) + " %",
                                 juce::dontSendNotification);
    };
    widthKnob.onValueChange();

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

    stereoView.levelLSrc = &proc.dsp.levelL;
    stereoView.levelRSrc = &proc.dsp.levelR;
    stereoView.driftASrc = &proc.dsp.driftDispA;
    stereoView.driftBSrc = &proc.dsp.driftDispB;
    stereoView.widthSrc  = &proc.dsp.widthDisp;
    addAndMakeVisible (stereoView);

    startTimerHz (30);
}

NAMDoubleEditor::~NAMDoubleEditor()
{
    setLookAndFeel (nullptr);
}

void NAMDoubleEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (nd::col::bg1, (float) getWidth() * 0.5f, 0.0f,
                             nd::col::bg0, (float) getWidth() * 0.5f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Top brand bar
    auto topH = 56;
    g.setColour (nd::col::panel);
    g.fillRect (juce::Rectangle<int> (0, 0, getWidth(), topH));
    g.setColour (nd::col::accent);
    g.fillRect (0, topH - 1, getWidth(), 1);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRect (0, topH, getWidth(), 1);

    g.setColour (nd::col::accent);
    g.fillRect (16, 14, 3, topH - 28);

    // Footer panel
    const int botH = 124;
    auto footer = juce::Rectangle<int> (0, getHeight() - botH, getWidth(), botH);
    g.setColour (nd::col::panel);
    g.fillRect (footer);
    g.setColour (nd::col::panelEdge);
    g.fillRect (footer.getX(), footer.getY(), footer.getWidth(), 1);
}

void NAMDoubleEditor::resized()
{
    auto r = getLocalBounds();

    // Top brand bar
    {
        auto top = r.removeFromTop (56);
        top.removeFromLeft (24);
        titleBadge.setBounds    (top.removeFromLeft (210));
        brandLabel.setBounds    (top.withTrimmedRight (16));
    }

    // About overlay always covers the full window
    aboutOverlay.setBounds (getLocalBounds());

    // Footer (Mix / Output)
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

    // Stereo image visualiser
    {
        auto imgArea = r.removeFromBottom (110).reduced (16, 6);
        imageHeader.setBounds (imgArea.removeFromTop (18));
        imgArea.removeFromTop (4);
        stereoView.setBounds (imgArea);
    }

    // Mode picker
    {
        auto modeArea = r.removeFromBottom (54).reduced (24, 4);
        modeHeader.setBounds (modeArea.removeFromTop (16));
        modeArea.removeFromTop (2);
        modePicker.setBounds (modeArea);
    }

    // Big WIDTH knob
    {
        auto centre = r;
        widthHeader.setBounds (centre.removeFromTop (18));
        const int knobSize = juce::jmin (centre.getWidth() - 60, centre.getHeight() - 60);
        const int kS = juce::jmax (180, knobSize);
        auto k = juce::Rectangle<int> (0, 0, kS, kS)
                     .withCentre ({ centre.getCentreX(), centre.getY() + 8 + kS / 2 });
        widthKnob.setBounds (k);

        widthValueLabel.setBounds (juce::Rectangle<int> (k.getCentreX() - 40,
                                                         k.getCentreY() - 9, 80, 18));

        auto ticks = juce::Rectangle<int> (k.getX() - 20, k.getBottom() + 2,
                                           k.getWidth() + 40, 12);
        const int tw = ticks.getWidth() / 3;
        monoTickLabel.setBounds   (ticks.removeFromLeft (tw));
        wideTickLabel.setBounds   (ticks.removeFromRight (tw));
        doubleTickLabel.setBounds (ticks);
    }
}

void NAMDoubleEditor::timerCallback()
{
    if (stereoView.levelLSrc != nullptr)
    {
        const float l = stereoView.levelLSrc->load();
        const float rr = stereoView.levelRSrc->load();
        const float a  = stereoView.driftASrc->load();
        const float b  = stereoView.driftBSrc->load();
        const float w  = stereoView.widthSrc->load();
        // Smooth display so the bars don't jitter at refresh rate.
        stereoView.lDisp += (l  - stereoView.lDisp) * 0.40f;
        stereoView.rDisp += (rr - stereoView.rDisp) * 0.40f;
        stereoView.aDisp += (a  - stereoView.aDisp) * 0.25f;
        stereoView.bDisp += (b  - stereoView.bDisp) * 0.25f;
        stereoView.wDisp += (w  - stereoView.wDisp) * 0.30f;
        stereoView.repaint();
    }
}
