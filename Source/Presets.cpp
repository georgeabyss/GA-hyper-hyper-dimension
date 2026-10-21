#include "Presets.h"
#include "ParamIDs.h"

namespace
{
    // the parameters a preset stores
    constexpr const char* kPresetParams[] = { pid::hyperOn, pid::unison, pid::retrig, pid::rate, pid::detune, pid::spread, pid::hyperMix,
                                              pid::dimOn, pid::dimSize, pid::dimMix,
                                              pid::spaceyOn, pid::spaceyMode, pid::spaceySpacing, pid::spaceyAmount,
                                              pid::bassMono, pid::dryWet };

    enum Mode { comb, twinDrive, phaseSmear, freqShift, prism, loFiTwins };

    // Starting points, grouped by the Spacey mode they show off. Values not listed are the defaults
    // (Unison 4, Rate 0.8 Hz, Detune 20 ct, Spread 70 %, Hyper Mix 50 %, Size 50 %, Dimension Mix 30 %,
    // Comb, Spacing 50 %, Amount 100 %, Bass Mono off, Dry/Wet 100 %).
    std::vector<ga::PresetManager::FactoryPreset> factoryPresets()
    {
        using namespace pid;
        return {
            { "Init", {} },
            { "Mild Salsa",    { { unison, 3 }, { rate, 0.5f }, { detune, 8 }, { spread, 50 }, { hyperMix, 30 },
                                 { dimSize, 35 }, { dimMix, 20 }, { spaceyAmount, 40 }, { spaceySpacing, 40 } } },
            { "Chorus Line",   { { unison, 2 }, { rate, 0.9f }, { detune, 14 }, { spread, 80 },
                                 { dimSize, 55 }, { dimMix, 35 }, { spaceyAmount, 50 } } },
            { "Gang of 17",    { { unison, 17 }, { rate, 0.35f }, { detune, 28 }, { spread, 100 }, { hyperMix, 70 },
                                 { dimSize, 60 }, { dimMix, 25 }, { spaceyMode, prism }, { spaceyAmount, 60 } } },
            { "Prism Break",   { { rate, 0.6f }, { detune, 12 }, { hyperMix, 40 },
                                 { spaceyMode, prism }, { spaceySpacing, 80 } } },
            { "Space Cadet",   { { unison, 6 }, { rate, 0.25f }, { detune, 22 }, { spread, 90 }, { hyperMix, 55 },
                                 { dimSize, 100 }, { dimMix, 60 }, { spaceyMode, prism }, { spaceyAmount, 80 }, { spaceySpacing, 65 } } },
            { "Twin Peaks",    { { unison, 2 }, { rate, 0.7f }, { detune, 10 }, { spread, 60 }, { hyperMix, 35 },
                                 { dimSize, 40 }, { dimMix, 25 }, { spaceyMode, twinDrive }, { spaceyAmount, 50 } } },
            { "Hairdryer",     { { unison, 5 }, { rate, 1.5f }, { detune, 18 }, { spread, 80 },
                                 { spaceyMode, twinDrive }, { spaceySpacing, 95 } } },
            { "Butter Knife",  { { unison, 3 }, { rate, 0.4f }, { detune, 9 }, { hyperMix, 40 },
                                 { dimSize, 45 }, { spaceyMode, phaseSmear }, { spaceyAmount, 80 }, { spaceySpacing, 35 } } },
            { "Seasick",       { { rate, 0.3f }, { detune, 45 }, { spread, 100 }, { hyperMix, 60 },
                                 { dimSize, 70 }, { dimMix, 40 }, { spaceyMode, phaseSmear }, { spaceySpacing, 100 } } },
            { "Wobbly Bits",   { { unison, 3 }, { rate, 1.2f }, { detune, 15 }, { hyperMix, 45 },
                                 { spaceyMode, freqShift }, { spaceyAmount, 70 } } },
            { "Zap Zap Zap",   { { unison, 8 }, { retrig, 1 }, { rate, 3.5f }, { detune, 40 }, { spread, 90 }, { hyperMix, 70 },
                                 { dimMix, 20 }, { spaceyMode, freqShift }, { spaceyAmount, 60 }, { spaceySpacing, 75 } } },
            { "Cassette Kid",  { { unison, 2 }, { rate, 0.6f }, { detune, 12 }, { spread, 50 }, { hyperMix, 45 },
                                 { dimSize, 40 }, { dimMix, 20 }, { spaceyMode, loFiTwins }, { spaceyAmount, 45 }, { spaceySpacing, 40 } } },
            { "8-Bit Twins",   { { rate, 1.0f }, { detune, 10 }, { hyperMix, 40 }, { dimMix, 20 },
                                 { spaceyMode, loFiTwins }, { spaceySpacing, 100 } } },
            { "Wide Ahh",      { { unison, 9 }, { detune, 25 }, { spread, 100 }, { hyperMix, 60 },
                                 { dimSize, 80 }, { dimMix, 50 }, { spaceySpacing, 75 } } },
            { "Mono Safe-ish", { { detune, 16 }, { spread, 60 }, { hyperMix, 45 },
                                 { spaceyAmount, 70 }, { bassMono, 150 } } },
        };
    }
}

ga::PresetManager::Config presetConfig (juce::AudioProcessorValueTreeState& apvts)
{
    ga::PresetManager::Config c;
    c.folder = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                   .getChildFile ("GA").getChildFile ("Hyper! Hyper! Dimension").getChildFile ("Presets");
    c.extension = ".hhdpreset";
    c.fileTag = "HHDPRESET";
    c.parameters.assign (std::begin (kPresetParams), std::end (kPresetParams));
    c.lockable = { { pid::dryWet, prop::lockDryWet }, { pid::bassMono, prop::lockBassMono } };
    c.factory = factoryPresets();

    // the list shows each preset's Spacey mode on the right
    c.listTag = [&apvts] (const ga::PresetManager& presets, const ga::PresetManager::Preset& p)
    {
        auto* mode = apvts.getParameter (pid::spaceyMode);
        return mode->getText (mode->convertTo0to1 (presets.valueOf (p, pid::spaceyMode)), 32).toUpperCase();
    };
    return c;
}
