#include "PresetManager.h"
#include "ParamIDs.h"

namespace
{
    // the parameters a preset stores
    constexpr const char* kPresetParams[] = { pid::hyperOn, pid::unison, pid::retrig, pid::rate, pid::detune, pid::spread, pid::hyperMix,
                                              pid::dimOn, pid::dimSize, pid::dimMix,
                                              pid::spaceyOn, pid::spaceyMode, pid::spaceySpacing, pid::spaceyAmount,
                                              pid::bassMono, pid::dryWet };

    constexpr const char* kExtension = ".hhdpreset";

    // the state property that locks a parameter, or nullptr if it cannot be locked
    const char* lockProperty (const char* id)
    {
        if (juce::String (id) == pid::dryWet)   return prop::lockDryWet;
        if (juce::String (id) == pid::bassMono) return prop::lockBassMono;
        return nullptr;
    }

    enum Mode { comb, twinDrive, phaseSmear, freqShift, prism, loFiTwins };

    struct FactoryPreset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;   // only what differs from the defaults
    };

    // Starting points, grouped by the Spacey mode they show off. Values not listed are the defaults
    // (Unison 4, Rate 0.8 Hz, Detune 20 ct, Spread 70 %, Hyper Mix 50 %, Size 50 %, Dimension Mix 30 %,
    // Comb, Spacing 50 %, Amount 100 %, Bass Mono off, Dry/Wet 100 %).
    std::vector<FactoryPreset> factoryPresets()
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

    juce::String modeTagFor (juce::AudioProcessorValueTreeState& apvts, float mode)
    {
        auto* p = apvts.getParameter (pid::spaceyMode);
        return p->getText (p->convertTo0to1 (mode), 32).toUpperCase();
    }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    rescan();
}

juce::File PresetManager::folder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
               .getChildFile ("GA").getChildFile ("Hyper! Hyper! Dimension").getChildFile ("Presets");
}

void PresetManager::rescan()
{
    presets.clear();
    for (auto& f : factoryPresets())
    {
        Preset p;
        p.name = f.name;
        for (auto& v : f.values)
            p.values[v.first] = v.second;
        p.modeTag = modeTagFor (apvts, valueOf (p, pid::spaceyMode));
        presets.push_back (std::move (p));
    }

    auto files = folder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    for (auto& file : files)
    {
        const auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("HHDPRESET"))
            continue;

        Preset p;
        p.isFactory = false;
        p.file = file;
        p.name = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
            p.values[e->getStringAttribute ("id")] = (float) e->getDoubleAttribute ("value");
        p.modeTag = modeTagFor (apvts, valueOf (p, pid::spaceyMode));
        presets.push_back (std::move (p));
    }
}

float PresetManager::valueOf (const Preset& p, const char* id) const
{
    if (auto it = p.values.find (id); it != p.values.end())
        return it->second;
    auto* param = apvts.getParameter (id);
    return param->convertFrom0to1 (param->getDefaultValue());
}

juce::String PresetManager::currentName() const
{
    return apvts.state.getProperty (prop::preset, "Init").toString();
}

int PresetManager::currentIndex() const
{
    const auto name = currentName();
    const bool isUser = apvts.state.getProperty (prop::presetIsUser, false);
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == name && presets[i].isFactory != isUser)
            return (int) i;
    return -1;
}

bool PresetManager::isLockable (const char* id)
{
    return lockProperty (id) != nullptr;
}

bool PresetManager::isLocked (const char* id) const
{
    const auto* property = lockProperty (id);
    return property != nullptr && (bool) apvts.state.getProperty (property, false);
}

void PresetManager::setLocked (const char* id, bool locked)
{
    if (const auto* property = lockProperty (id))
        apvts.state.setProperty (property, locked, nullptr);
}

bool PresetManager::isModified() const
{
    const int i = currentIndex();
    if (i < 0)
        return true;
    for (auto* id : kPresetParams)
    {
        if (isLocked (id))   // a locked value is the user's, not the preset's
            continue;
        auto* param = apvts.getParameter (id);
        if (std::abs (param->getValue() - param->convertTo0to1 (valueOf (presets[(size_t) i], id))) > 1.0e-4f)
            return true;
    }
    return false;
}

void PresetManager::setCurrent (const Preset& p)
{
    apvts.state.setProperty (prop::preset, p.name, nullptr);
    apvts.state.setProperty (prop::presetIsUser, ! p.isFactory, nullptr);
}

void PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;
    const auto& p = presets[(size_t) index];
    for (auto* id : kPresetParams)
    {
        if (isLocked (id))
            continue;
        auto* param = apvts.getParameter (id);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (valueOf (p, id)));
        param->endChangeGesture();
    }
    setCurrent (p);
}

void PresetManager::step (int delta)
{
    const int n = (int) presets.size();
    if (n == 0)
        return;
    const int i = currentIndex();
    load (i < 0 ? (delta > 0 ? 0 : n - 1) : ((i + delta) % n + n) % n);
}

bool PresetManager::save (const juce::String& name)
{
    const auto clean = name.trim();
    if (clean.isEmpty() || ! folder().createDirectory())
        return false;

    juce::XmlElement xml ("HHDPRESET");
    xml.setAttribute ("name", clean);
    for (auto* id : kPresetParams)
    {
        auto* e = xml.createNewChildElement ("PARAM");
        e->setAttribute ("id", id);
        e->setAttribute ("value", apvts.getParameter (id)->convertFrom0to1 (apvts.getParameter (id)->getValue()));
    }
    if (! xml.writeTo (folder().getChildFile (juce::File::createLegalFileName (clean) + kExtension)))
        return false;

    rescan();
    Preset saved;
    saved.name = clean;
    saved.isFactory = false;
    setCurrent (saved);
    return true;
}

bool PresetManager::rename (int index, const juce::String& newName)
{
    const auto clean = newName.trim();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()) || presets[(size_t) index].isFactory || clean.isEmpty())
        return false;

    const auto old = presets[(size_t) index];
    const bool wasCurrent = index == currentIndex();
    auto xml = juce::XmlDocument::parse (old.file);
    if (xml == nullptr)
        return false;
    xml->setAttribute ("name", clean);
    const auto target = folder().getChildFile (juce::File::createLegalFileName (clean) + kExtension);
    if (! xml->writeTo (target))
        return false;
    if (target != old.file)
        old.file.deleteFile();

    rescan();
    if (wasCurrent)
    {
        Preset renamed;
        renamed.name = clean;
        renamed.isFactory = false;
        setCurrent (renamed);
    }
    return true;
}

bool PresetManager::remove (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()) || presets[(size_t) index].isFactory)
        return false;
    const bool ok = presets[(size_t) index].file.deleteFile();
    rescan();
    return ok;
}
