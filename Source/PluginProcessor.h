#pragma once

#include "plugin/ProcessorBase.h"
#include "HyperDimEngine.h"
#include "Presets.h"

// Hyper! Hyper! Dimension on the GA processor base (oversampling, bypass, safety mute, scope,
// state): the engine is the effect, Retrig splits the block at every note-on.
class HyperDimAudioProcessor : public ga::ProcessorBase
{
public:
    HyperDimAudioProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    const juce::String getName() const override { return "Hyper! Hyper! Dimension"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    ga::PresetManager presets { apvts, presetConfig (apvts) };
    hd::Engine engine;

private:
    void prepareWet (double sampleRate, int channels, int oversampling) override { engine.prepare (sampleRate, channels, oversampling); }
    void updateWetParams() override { engine.setParams (engineParams()); }
    void processWet (float* const* io, int numSamples) override { engine.process (io, numSamples); }
    void resetWet() override { engine.reset(); }
    bool splitAtNoteOns() const override;
    void noteOn() override { engine.retrig(); }
    void migrateState (juce::ValueTree&) override;

    hd::Engine::Params engineParams() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HyperDimAudioProcessor)
};
