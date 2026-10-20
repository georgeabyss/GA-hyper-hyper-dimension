#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "HyperDimEngine.h"

class HyperDimAudioProcessor : public juce::AudioProcessor
{
public:
    HyperDimAudioProcessor();
    ~HyperDimAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Hyper! Hyper! Dimension"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Output samples for the correlation meter and the goniometer: the audio thread writes,
    // the editor reads. Samples are dropped while nobody reads (editor closed).
    int readScope (float* l, float* r, int maxNum);

    // the Bypass button, or Dry/Wet at 0: both bypass the whole effect (and dim the editor)
    bool isBypassedNow() const;

    juce::AudioProcessorValueTreeState apvts;
    hd::Engine engine;

private:
    static constexpr int kScopeSize = 16384;
    static constexpr int kMaxOsStages = 3;          // 2x, 4x, 8x
    static constexpr int kDryDelaySize = 4096;      // more than the longest oversampling latency

    void updateOversampling();                      // audio thread, after a change of oversampling or filter
    hd::Engine::Params engineParams() const;
    void processSegment (juce::AudioBuffer<float>&, int start, int num);
    void storeDry (const juce::AudioBuffer<float>&);
    void applyBypass (juce::AudioBuffer<float>&);
    void pushScope (const juce::AudioBuffer<float>&);

    juce::AudioProcessorParameter* bypassParam = nullptr;
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float> bypassMix { 0.0f };   // 0 = effect, 1 = bypassed

    // [filter type][stage - 1]; 0 = linear phase FIR, 1 = low latency IIR
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[2][kMaxOsStages];
    juce::dsp::Oversampling<float>* activeOs = nullptr;
    int osStages = -1, osFilter = -1, maxBlock = 0, numCh = 2;
    double baseRate = 44100.0;
    bool fadeIn = false;

    // the dry signal for the bypass crossfade is delayed by the oversampling latency
    juce::AudioBuffer<float> dryDelay { 2, kDryDelaySize };
    int dryDelayPos = 0, latency = 0;

    juce::AbstractFifo scopeFifo { kScopeSize };
    std::vector<float> scopeL, scopeR;

    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HyperDimAudioProcessor)
};
