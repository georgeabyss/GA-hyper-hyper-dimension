#include "PluginEditor.h"

namespace
{
    // what the bottom strip needs from the processor
    ga::BottomStrip::Host stripHost (HyperDimAudioProcessor& p)
    {
        ga::BottomStrip::Host h;
        h.isBypassed = [&p] { return p.isBypassedNow(); };
        h.isLocked = [&p] (const char* id) { return p.presets.isLocked (id); };
        h.toggleLock = [&p] (const char* id) { p.presets.setLocked (id, ! p.presets.isLocked (id)); };
        h.readScope = [&p] (float* l, float* r, int max) { return p.readScope (l, r, max); };
        h.protectActivity = [&p] { return p.engine.output.protectActivity.load (std::memory_order_relaxed); };
        h.latencySamples = [&p] { return p.getLatencySamples(); };
        h.oversamplingTooltip = "Oversampling: keeps Twin Drive and Lo-Fi Twins free of aliasing. Costs CPU.";
        return h;
    }
}

//==============================================================================
HyperDimAudioProcessorEditor::HyperDimAudioProcessorEditor (HyperDimAudioProcessor& p)
    : ga::EditorShell (p, p.apvts, stripHost (p)), panel (p)
{
    setPanel (panel, MainPanel::kBaseWidth, MainPanel::kBaseHeight);
}
