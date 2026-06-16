#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    void configureRotary (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, true);
    }

    void styleLabel (juce::Label& l, const juce::String& t,
                     float fontSize, juce::Justification j,
                     juce::Colour c, bool bold = true)
    {
        l.setText (t, juce::dontSendNotification);
        l.setJustificationType (j);
        auto opts = juce::FontOptions (fontSize);
        if (bold) opts = opts.withStyle ("Bold");
        l.setFont (juce::Font (opts));
        l.setColour (juce::Label::textColourId, c);
    }
}

AtiNAMatiCHitZoneEditor::AtiNAMatiCHitZoneEditor (AtiNAMatiCHitZoneProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), meter (p.dsp)
{
    background = juce::ImageCache::getFromMemory (BinaryData::Background_png, BinaryData::Background_pngSize);

    setLookAndFeel (&lnf);
    setOpaque (true);

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

    styleLabel (sweetLabel,    "SWEET SPOT", 11.0f, juce::Justification::centred, nhz::col::textDim);
    sweetLabel.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")).withExtraKerningFactor (0.25f));

    styleLabel (coldLabel,     "COLD",       10.0f, juce::Justification::centredLeft,  nhz::col::cold);
    styleLabel (hotLabel,      "HOT",        10.0f, juce::Justification::centredRight, nhz::col::hot);
    coldLabel.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")).withExtraKerningFactor (0.25f));
    hotLabel.setFont  (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")).withExtraKerningFactor (0.25f));

    styleLabel (statusLabel,   "Perfect",    20.0f, juce::Justification::centred, nhz::col::green);
    statusLabel.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")));

    styleLabel (gainReadout,   "0.0 dB",     10.0f, juce::Justification::centred, nhz::col::textFaint);

    styleLabel (mixLabel,      "MIX",        10.0f, juce::Justification::centred, nhz::col::textDim);
    styleLabel (outLabel,      "OUTPUT",     10.0f, juce::Justification::centred, nhz::col::textDim);
    styleLabel (responseLabel, "RESPONSE",   10.0f, juce::Justification::centred, nhz::col::textDim);
    for (auto* l : { &mixLabel, &outLabel, &responseLabel })
        l->setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")).withExtraKerningFactor (0.25f));

    addAndMakeVisible (sweetLabel);
    addAndMakeVisible (coldLabel);
    addAndMakeVisible (hotLabel);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (gainReadout);
    addAndMakeVisible (mixLabel);
    addAndMakeVisible (outLabel);
    addAndMakeVisible (responseLabel);

    configureRotary (sweetSpot);
    configureRotary (mixSlider);
    configureRotary (outputSlider);
    configureRotary (responseSlider);
    configureRotary (manualGainSlider);

    addAndMakeVisible (sweetSpot);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (outputSlider);
    addAndMakeVisible (responseSlider);
    addAndMakeVisible (manualGainSlider);
    addAndMakeVisible (autoToggle);
    addAndMakeVisible (meter);

    aSweet  = std::make_unique<SAtt> (proc.apvts, "sweetSpot",  sweetSpot);
    aMix    = std::make_unique<SAtt> (proc.apvts, "mix",        mixSlider);
    aOut    = std::make_unique<SAtt> (proc.apvts, "output",     outputSlider);
    aResp   = std::make_unique<SAtt> (proc.apvts, "response",   responseSlider);
    aManual = std::make_unique<SAtt> (proc.apvts, "manualGain", manualGainSlider);
    aAuto   = std::make_unique<BAtt> (proc.apvts, "autoOn",     autoToggle);

    // Professional: double-click to reset defaults
    sweetSpot.setDoubleClickReturnValue        (true, 0.5);
    mixSlider.setDoubleClickReturnValue        (true, 1.0);
    outputSlider.setDoubleClickReturnValue     (true, 0.0);
    responseSlider.setDoubleClickReturnValue   (true, 0.5);
    manualGainSlider.setDoubleClickReturnValue (true, 0.0);

    // Professional: value display functions (shown in slider popup / tooltip)
    sweetSpot.textFromValueFunction        = [] (double v) { return juce::String (int (v * 100)) + "%"; };
    mixSlider.textFromValueFunction        = [] (double v) { return juce::String (int (v * 100)) + "%"; };
    outputSlider.textFromValueFunction     = [] (double v) { return juce::String (v, 1) + " dB"; };
    responseSlider.textFromValueFunction   = [] (double v) { return juce::String (int (v * 100)) + "%"; };
    manualGainSlider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };

    // Professional: tooltips
    sweetSpot.setTooltip        ("Target level zone. COLD = signal too quiet for NAM, HOT = too loud.");
    mixSlider.setTooltip        ("Wet / Dry blend -- 100%% = fully processed.");
    outputSlider.setTooltip     ("Output level trim (dB).");
    responseSlider.setTooltip   ("Auto-gain tracking speed -- higher = faster response.");
    manualGainSlider.setTooltip ("Manual input gain (dB). Used when AUTO is off.");
    autoToggle.setTooltip       ("Enable automatic gain adjustment to hit the sweet zone.");

    autoToggle.onClick = [this] { updateAutoVisuals(); };
    updateAutoVisuals();

    setResizable (false, false);
    setSize (560, 520);

    startTimerHz (20);
}

AtiNAMatiCHitZoneEditor::~AtiNAMatiCHitZoneEditor()
{
    setLookAndFeel (nullptr);
}

void AtiNAMatiCHitZoneEditor::paint (juce::Graphics& g)
{
    if (background.isValid())
    {
        g.drawImage (background, getLocalBounds().toFloat());
    }
    else
    {
        juce::ColourGradient bg (nhz::col::bgTop, 0.0f, 0.0f,
                                 nhz::col::bgBot, 0.0f, (float) getHeight(), false);
        g.setGradientFill (bg);
        g.fillRect (getLocalBounds());
    }

    // Title bar strip
    auto top = getLocalBounds().removeFromTop (62).toFloat().reduced (12.0f, 8.0f);
    {
        juce::DropShadow ds (juce::Colours::black.withAlpha (0.45f), 8, { 0, 2 });
        juce::Path tp;
        tp.addRoundedRectangle (top, 8.0f);
        ds.drawForPath (g, tp);
    }
    juce::ColourGradient tgrad (nhz::col::panelHi, 0.0f, top.getY(),
                                nhz::col::panel,   0.0f, top.getBottom(), false);
    g.setGradientFill (tgrad);
    g.fillRoundedRectangle (top, 8.0f);
    g.setColour (nhz::col::stroke);
    g.drawRoundedRectangle (top, 8.0f, 1.0f);

    // Tagline
    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Italic")));
    g.setColour (nhz::col::textFaint.withAlpha (0.8f));
    g.drawText ("Always Hit the Perfect Level",
                (int) top.getX(), (int) top.getBottom() - 16, (int) top.getWidth(), 13,
                juce::Justification::centred, false);

    // Subtle divider above the bottom-controls row
    const float divY = (float) getHeight() - 140.0f;
    g.setColour (nhz::col::stroke.withAlpha (0.6f));
    g.drawHorizontalLine ((int) divY, 24.0f, (float) getWidth() - 24.0f);
}

void AtiNAMatiCHitZoneEditor::resized()
{
    auto r = getLocalBounds();

    // Title strip — badge in upper portion, tagline drawn below it in paint()
    auto title = r.removeFromTop (62);
    auto titleInner = title.reduced (16, 0);
    titleBadge.setBounds (juce::Rectangle<int> (titleInner.getX(), title.getY() + 7, titleInner.getWidth(), 27));

    // About overlay always covers everything
    aboutOverlay.setBounds (getLocalBounds());

    r.removeFromTop (6);

    // Big knob
    auto knobArea = r.removeFromTop (210);
    auto knobBox  = knobArea.withSizeKeepingCentre (210, 200);
    sweetSpot.setBounds (knobBox.withTrimmedBottom (22));
    sweetLabel.setBounds (knobBox.removeFromBottom (20));

    // Cold / Hot labels under the knob
    auto coldHot = r.removeFromTop (16).reduced (40, 0);
    coldLabel.setBounds (coldHot.removeFromLeft (60));
    hotLabel.setBounds  (coldHot.removeFromRight (60));

    r.removeFromTop (4);

    // Meter
    auto meterRow = r.removeFromTop (54).reduced (24, 0);
    meter.setBounds (meterRow);

    // Status + gain readout
    auto statusRow = r.removeFromTop (28);
    gainReadout.setBounds (statusRow.removeFromRight (90).reduced (4, 4));
    statusLabel.setBounds (statusRow);

    r.removeFromTop (4);

    // Auto toggle pill (centered)
    auto autoRow = r.removeFromTop (26);
    autoToggle.setBounds (autoRow.withSizeKeepingCentre (110, 22));

    r.removeFromTop (8);

    // Bottom controls row -- pulled in from the edges so Mix/Output don't
    // sit on top of the baseplate's corner circuit-trace decoration.
    auto bottom = r.removeFromTop (110).reduced (64, 4);
    const int colW = bottom.getWidth() / 3;
    auto col1 = bottom.removeFromLeft (colW);
    auto col2 = bottom.removeFromLeft (colW);
    auto col3 = bottom;

    auto layoutKnob = [] (juce::Rectangle<int> area, juce::Slider& s, juce::Label& l)
    {
        l.setBounds (area.removeFromBottom (16));
        s.setBounds (area.reduced (12, 2));
    };

    layoutKnob (col1, mixSlider,      mixLabel);
    layoutKnob (col2, responseSlider, responseLabel);
    layoutKnob (col3, outputSlider,   outLabel);

    // Manual gain shares the response slot when Auto is OFF
    manualGainSlider.setBounds (responseSlider.getBounds());
}

void AtiNAMatiCHitZoneEditor::updateAutoVisuals()
{
    const bool on = autoToggle.getToggleState();
    responseSlider.setVisible (on);
    manualGainSlider.setVisible (! on);
    responseLabel.setText (on ? "RESPONSE" : "GAIN", juce::dontSendNotification);
    gainReadout.setVisible (true);
}

void AtiNAMatiCHitZoneEditor::timerCallback()
{
    using nhz::HitStatus;
    const auto s = (HitStatus) proc.dsp.statusEnum.load();
    juce::String t;
    juce::Colour c;
    switch (s)
    {
        case HitStatus::TooCold: t = "Too Cold"; c = nhz::col::cold;  break;
        case HitStatus::TooHot:  t = "Too Hot";  c = nhz::col::red;   break;
        default:                 t = "Perfect";  c = nhz::col::green; break;
    }
    statusLabel.setText (t, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, c);

    const float gDb = proc.dsp.appliedGainDb.load();
    gainReadout.setText (juce::String (gDb, 1) + " dB", juce::dontSendNotification);
}
