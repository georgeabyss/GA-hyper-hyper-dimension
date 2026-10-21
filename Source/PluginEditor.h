#pragma once

#include "PluginProcessor.h"
#include "MainPanel.h"
#include "ui/EditorShell.h"

// The GA editor shell (scaling, bottom strip, goniometer) with Hyper! Hyper! Dimension's panel on it.
class HyperDimAudioProcessorEditor : public ga::EditorShell
{
public:
    explicit HyperDimAudioProcessorEditor (HyperDimAudioProcessor&);

private:
    MainPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HyperDimAudioProcessorEditor)
};
