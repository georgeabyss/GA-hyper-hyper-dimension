#include "PluginEditor.h"
#include "ParamIDs.h"

//==============================================================================
HyperDimAudioProcessorEditor::HyperDimAudioProcessorEditor (HyperDimAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), panel (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (panel);

    panel.getScale = [this] { return (float) getHeight() / (float) MainPanel::kBaseHeight; };
    panel.setScale = [this] (float s) { applyScale (s); };
    panel.onLayoutChange = [this]
    {
        updateConstraints();
        applyScale ((float) getHeight() / (float) MainPanel::kBaseHeight);
    };

    setConstrainer (&constrainer);
    updateConstraints();
    setResizable (true, true);
    applyScale ((float) (double) p.apvts.state.getProperty (prop::uiScale, 1.0));   // saved with the session
}

HyperDimAudioProcessorEditor::~HyperDimAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void HyperDimAudioProcessorEditor::updateConstraints()
{
    const double w = (double) panel.baseWidth(), h = (double) MainPanel::kBaseHeight;
    constrainer.setFixedAspectRatio (w / h);
    constrainer.setSizeLimits (juce::roundToInt (w * 0.6), juce::roundToInt (h * 0.6),
                               juce::roundToInt (w * 2.5), juce::roundToInt (h * 2.5));
}

void HyperDimAudioProcessorEditor::applyScale (float scale)
{
    scale = juce::jlimit (0.6f, 2.5f, scale);
    setSize (juce::roundToInt ((float) panel.baseWidth() * scale), juce::roundToInt ((float) MainPanel::kBaseHeight * scale));
}

void HyperDimAudioProcessorEditor::resized()
{
    // the faceplate is laid out at 100 % and scaled as a whole
    const float scale = (float) getHeight() / (float) MainPanel::kBaseHeight;
    panel.setTransform (juce::AffineTransform::scale (scale));
    panel.setBounds (0, 0, panel.baseWidth(), MainPanel::kBaseHeight);
    proc.apvts.state.setProperty (prop::uiScale, scale, nullptr);
    panel.repaint();
}
