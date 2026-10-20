#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamIDs.h"

namespace
{
    // sessions from before v0.7: Unison 0 meant "Hyper off" (Unison now starts at 1)
    void migrateLegacyState (juce::ValueTree& state)
    {
        const auto unison = state.getChildWithProperty ("id", pid::unison);
        if (! unison.isValid() || (double) unison.getProperty ("value") >= 0.5)
            return;

        auto hyperOn = state.getChildWithProperty ("id", pid::hyperOn);
        if (! hyperOn.isValid())
        {
            hyperOn = juce::ValueTree ("PARAM");
            hyperOn.setProperty ("id", pid::hyperOn, nullptr);
            state.appendChild (hyperOn, nullptr);
        }
        hyperOn.setProperty ("value", 0.0, nullptr);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout HyperDimAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    // 0..100 %, shown as a whole number
    auto percent = [&p] (const char* id, const char* name, float defaultValue)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.f, 100.f, 0.1f), defaultValue,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                            {
                                                                return String (roundToInt (v)) + " %";
                                                            })));
    };
    auto toggle = [&p] (const char* id, const char* name, bool defaultValue)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, defaultValue));
    };
    // not automatable: these change the latency
    auto setting = [&p] (const char* id, const char* name, const StringArray& choices, int defaultIndex)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, defaultIndex,
                                                             AudioParameterChoiceAttributes().withAutomatable (false)));
    };

    // the order below is the order hosts list the parameters in
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { pid::unison, 1 }, "Unison", 1, hd::Engine::kMaxVoices, 4));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::rate, 1 }, "Rate",
                                                        NormalisableRange<float> (0.05f, 20.f, 0.01f, 0.4f), 0.8f,
                                                        AudioParameterFloatAttributes().withLabel ("Hz")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::detune, 1 }, "Detune",
                                                        NormalisableRange<float> (0.f, 100.f, 0.1f), 20.f,
                                                        AudioParameterFloatAttributes().withLabel ("ct")));
    percent (pid::spread, "Spread", 70.f);
    toggle (pid::retrig, "Retrig", false);
    percent (pid::hyperMix, "Hyper Mix", 50.f);
    percent (pid::dimSize, "Dimension Size", 50.f);
    percent (pid::dimMix, "Dimension Mix", 30.f);
    // section on/off (the LEDs). A section is off while its LED is off or its amount is at 0.
    toggle (pid::hyperOn, "Hyper On", true);
    toggle (pid::dimOn, "Dimension On", true);
    toggle (pid::spaceyOn, "Spacey On", true);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::spaceyMode, 1 }, "Wide Mode",
                                                         StringArray { "Comb", "Twin Drive", "Phase Smear", "Freq Shift", "Prism", "Lo-Fi Twins" }, 0));
    percent (pid::spaceyAmount, "Spacey Amount", 100.f);
    percent (pid::spaceySpacing, "Spacey Spacing", 50.f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::bassMono, 1 }, "Bass Mono",
                                                        NormalisableRange<float> (20.f, 400.f, 1.f, 0.5f), 20.f,
                                                        AudioParameterFloatAttributes()
                                                            .withStringFromValueFunction ([] (float v, int) { return v <= 20.5f ? String ("Off") : String ((int) v) + " Hz"; })
                                                            .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue() > 20.5f ? t.getFloatValue() : 20.0f; })));
    percent (pid::dryWet, "Dry/Wet", 100.f);
    toggle (pid::bypass, "Bypass", false);
    setting (pid::osFactor, "Oversampling", { "Off", "2x", "4x", "8x" }, 1);
    setting (pid::osFilter, "Anti-Alias Filter", { "Linear Phase", "Low Latency" }, 0);
    return { p.begin(), p.end() };
}

HyperDimAudioProcessor::HyperDimAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    bypassParam = apvts.getParameter (pid::bypass);
    scopeL.assign ((size_t) kScopeSize, 0.0f);
    scopeR.assign ((size_t) kScopeSize, 0.0f);
}

bool HyperDimAudioProcessor::isBypassedNow() const
{
    return param (pid::bypass) > 0.5f || param (pid::dryWet) <= 0.0f;
}

bool HyperDimAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void HyperDimAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseRate = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);
    numCh = juce::jmax (1, juce::jmin (2, getTotalNumInputChannels()));

    using OS = juce::dsp::Oversampling<float>;
    for (int f = 0; f < 2; ++f)
        for (int s = 0; s < kMaxOsStages; ++s)
        {
            auto& o = oversamplers[f][s];
            if (o == nullptr)
                o = std::make_unique<OS> (2, (size_t) (s + 1),
                                          f == 0 ? OS::filterHalfBandFIREquiripple : OS::filterHalfBandPolyphaseIIR,
                                          true, true);   // max quality, integer latency
            o->initProcessing ((size_t) maxBlock);
        }

    engine.prepare (sampleRate * (1 << kMaxOsStages), numCh, 1 << kMaxOsStages);   // allocate for the highest rate
    osStages = osFilter = -1;
    updateOversampling();
    fadeIn = false;

    dryBuffer.setSize (2, maxBlock);
    bypassMix.reset (sampleRate, 0.02);
    bypassMix.setCurrentAndTargetValue (isBypassedNow() ? 1.0f : 0.0f);
}

void HyperDimAudioProcessor::updateOversampling()
{
    const int stages = juce::jlimit (0, kMaxOsStages, (int) (param (pid::osFactor) + 0.5f));
    const int filter = juce::jlimit (0, 1, (int) (param (pid::osFilter) + 0.5f));
    if (stages == osStages && filter == osFilter)
        return;

    osStages = stages;
    osFilter = filter;
    activeOs = stages > 0 ? oversamplers[filter][stages - 1].get() : nullptr;
    if (activeOs != nullptr)
        activeOs->reset();

    // no allocation here: everything was sized for the highest rate in prepareToPlay
    engine.prepare (baseRate * (1 << stages), numCh, 1 << stages);
    latency = activeOs != nullptr ? juce::roundToInt (activeOs->getLatencyInSamples()) : 0;
    setLatencySamples (latency);
    dryDelay.clear();
    dryDelayPos = 0;
    fadeIn = true;   // the engine restarts empty, so fade the output back in
}

hd::Engine::Params HyperDimAudioProcessor::engineParams() const
{
    hd::Engine::Params p;
    p.unison = (int) param (pid::unison);
    p.rateHz = param (pid::rate);
    p.detuneCents = param (pid::detune);
    p.spread = param (pid::spread) * 0.01f;
    // a section that is off fades its mix to 0 (20 ms, click-free); at 0 the engine skips it
    p.hyperMix = param (pid::hyperOn) > 0.5f ? param (pid::hyperMix) * 0.01f : 0.0f;
    p.dimSize = param (pid::dimSize) * 0.01f;
    p.dimMix = param (pid::dimOn) > 0.5f ? param (pid::dimMix) * 0.01f : 0.0f;
    p.wideMode = (int) (param (pid::spaceyMode) + 0.5f);
    p.wideAmount = param (pid::spaceyAmount) * 0.01f;
    p.wide = param (pid::spaceyOn) > 0.5f && p.wideAmount > 0.0f;
    p.wideSpacing = param (pid::spaceySpacing) * 0.01f;
    p.bassMonoHz = param (pid::bassMono);
    p.globalMix = param (pid::dryWet) * 0.01f;
    return p;
}

void HyperDimAudioProcessor::processSegment (juce::AudioBuffer<float>& buffer, int start, int num)
{
    const int chans = juce::jmin (buffer.getNumChannels(), 2);
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) chans, (size_t) buffer.getNumSamples());

    for (int done = 0; done < num;)   // the oversamplers take at most maxBlock samples at a time
    {
        const int len = juce::jmin (maxBlock, num - done);
        auto sub = block.getSubBlock ((size_t) (start + done), (size_t) len);
        if (activeOs != nullptr)
        {
            auto up = activeOs->processSamplesUp (sub);
            float* io[2] = { up.getChannelPointer (0), chans > 1 ? up.getChannelPointer (1) : nullptr };
            engine.process (io, (int) up.getNumSamples());
            activeOs->processSamplesDown (sub);
        }
        else
        {
            float* io[2] = { sub.getChannelPointer (0), chans > 1 ? sub.getChannelPointer (1) : nullptr };
            engine.process (io, len);
        }
        done += len;
    }
}

void HyperDimAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    if (n == 0 || buffer.getNumChannels() == 0)
        return;

    updateOversampling();
    storeDry (buffer);
    engine.setParams (engineParams());

    // process in segments split at every note-on, so Retrig lands on the exact sample
    int done = 0;
    auto processUpTo = [&] (int end)
    {
        if (end <= done) return;
        processSegment (buffer, done, end - done);
        done = end;
    };
    if (param (pid::retrig) > 0.5f)
        for (const auto meta : midi)
            if (meta.getMessage().isNoteOn())
            {
                processUpTo (juce::jlimit (0, n, meta.samplePosition));
                engine.retrig();
            }
    processUpTo (n);

    const int chans = juce::jmin (buffer.getNumChannels(), 2);
    if (fadeIn)
    {
        for (int ch = 0; ch < chans; ++ch)
            buffer.applyGainRamp (ch, 0, n, 0.0f, 1.0f);
        fadeIn = false;
    }

    applyBypass (buffer);
    pushScope (buffer);
}

void HyperDimAudioProcessor::storeDry (const juce::AudioBuffer<float>& buffer)
{
    // the dry copy for the click-free bypass crossfade, delayed by the oversampling latency
    const int n = buffer.getNumSamples();
    if (dryBuffer.getNumSamples() < n)
        dryBuffer.setSize (2, n, false, false, true);
    for (int ch = 0; ch < juce::jmin (buffer.getNumChannels(), 2); ++ch)
    {
        auto* ring = dryDelay.getWritePointer (ch);
        const auto* in = buffer.getReadPointer (ch);
        auto* dry = dryBuffer.getWritePointer (ch);
        for (int i = 0, pos = dryDelayPos; i < n; ++i, pos = (pos + 1) & (kDryDelaySize - 1))
        {
            ring[pos] = in[i];
            dry[i] = ring[(pos - latency) & (kDryDelaySize - 1)];
        }
    }
    dryDelayPos = (dryDelayPos + n) & (kDryDelaySize - 1);
}

void HyperDimAudioProcessor::applyBypass (juce::AudioBuffer<float>& buffer)
{
    bypassMix.setTargetValue (isBypassedNow() ? 1.0f : 0.0f);
    if (! bypassMix.isSmoothing() && bypassMix.getCurrentValue() <= 0.0f)
        return;

    const int chans = juce::jmin (buffer.getNumChannels(), 2);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float m = bypassMix.getNextValue();
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* out = buffer.getWritePointer (ch);
            out[i] += (dryBuffer.getReadPointer (ch)[i] - out[i]) * m;
        }
    }
}

void HyperDimAudioProcessor::pushScope (const juce::AudioBuffer<float>& buffer)
{
    const auto* outL = buffer.getReadPointer (0);
    const auto* outR = buffer.getReadPointer (buffer.getNumChannels() > 1 ? 1 : 0);
    const auto w = scopeFifo.write (buffer.getNumSamples());
    auto put = [&] (int startIndex, int size, int offset)
    {
        std::copy (outL + offset, outL + offset + size, scopeL.begin() + startIndex);
        std::copy (outR + offset, outR + offset + size, scopeR.begin() + startIndex);
    };
    put (w.startIndex1, w.blockSize1, 0);
    put (w.startIndex2, w.blockSize2, w.blockSize1);
}

int HyperDimAudioProcessor::readScope (float* l, float* r, int maxNum)
{
    const auto rd = scopeFifo.read (juce::jmin (maxNum, scopeFifo.getNumReady()));
    auto get = [&] (int startIndex, int size, int offset)
    {
        std::copy (scopeL.begin() + startIndex, scopeL.begin() + startIndex + size, l + offset);
        std::copy (scopeR.begin() + startIndex, scopeR.begin() + startIndex + size, r + offset);
    };
    get (rd.startIndex1, rd.blockSize1, 0);
    get (rd.startIndex2, rd.blockSize2, rd.blockSize1);
    return rd.blockSize1 + rd.blockSize2;
}

juce::AudioProcessorEditor* HyperDimAudioProcessor::createEditor()
{
    return new HyperDimAudioProcessorEditor (*this);
}

void HyperDimAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HyperDimAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            migrateLegacyState (state);
            apvts.replaceState (state);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HyperDimAudioProcessor();
}
