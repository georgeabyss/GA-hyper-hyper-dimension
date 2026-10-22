#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamIDs.h"
#include "plugin/SharedParams.h"

namespace
{
    // what Hyper! Hyper! Dimension uses of the GA processor: everything, plus MIDI input for Retrig
    ga::ProcessorConfig processorConfig()
    {
        ga::ProcessorConfig c;
        c.midiInput = true;
        return c;
    }

    // sessions from before v0.7: Unison 0 meant "Hyper off" (Unison now starts at 1)
    void migrateUnison (juce::ValueTree& state)
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
    // then the GA ones: Bass Mono, Dry/Wet, Bypass, Oversampling, Anti-Alias Filter, Phase Protect
    ga::addSharedParameters (p, processorConfig());
    return { p.begin(), p.end() };
}

HyperDimAudioProcessor::HyperDimAudioProcessor()
    : ga::ProcessorBase (createParameterLayout(), processorConfig())
{
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
    p.phaseProtect = param (pid::phaseProtect) > 0.5f;
    return p;
}

bool HyperDimAudioProcessor::splitAtNoteOns() const
{
    return param (pid::retrig) > 0.5f;   // so Retrig lands on the exact sample
}

juce::AudioProcessorEditor* HyperDimAudioProcessor::createEditor()
{
    return new HyperDimAudioProcessorEditor (*this);
}

void HyperDimAudioProcessor::migrateState (juce::ValueTree& state)
{
    migrateUnison (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HyperDimAudioProcessor();
}
