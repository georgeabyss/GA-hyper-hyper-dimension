#pragma once

#include "PluginProcessor.h"
#include "MainPanel.h"

// Resizable editor: drag the corner (fixed aspect ratio) or pick a size in the bottom strip.
// The faceplate (MainPanel) is laid out at 100 % and scaled as a whole.
class HyperDimAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit HyperDimAudioProcessorEditor (HyperDimAudioProcessor&);
    ~HyperDimAudioProcessorEditor() override;

    void resized() override;

private:
    void updateConstraints();
    void applyScale (float scale);

    HyperDimAudioProcessor& proc;
    HyperLookAndFeel laf;
    MainPanel panel;
    juce::ComponentBoundsConstrainer constrainer;
    juce::TooltipWindow tooltips { this, 900 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HyperDimAudioProcessorEditor)
};
