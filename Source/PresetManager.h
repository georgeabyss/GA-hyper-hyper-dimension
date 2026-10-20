#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Factory presets (built in) and user presets (one .hhdpreset file each in
// ~/Music/GA/Hyper! Hyper! Dimension/Presets), browsed as one list: factory first.
// A preset holds the sound only: oversampling, the anti-alias filter and Bypass stay as they are.
// Message thread only.
class PresetManager
{
public:
    struct Preset
    {
        juce::String name;
        bool isFactory = true;
        juce::String modeTag;                    // the Spacey mode, shown in the list
        std::map<juce::String, float> values;    // parameter ID -> value; missing = default
        juce::File file;                         // user presets only
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState&);

    static juce::File folder();
    void rescan();                               // re-read the user presets from the folder

    const std::vector<Preset>& all() const { return presets; }
    int currentIndex() const;                    // -1 if the current preset no longer exists
    juce::String currentName() const;
    bool isModified() const;                     // the settings differ from the preset as saved

    // Dry/Wet and Bass Mono can be locked: a locked value stays when a preset loads.
    // The locks are saved with the session, not in presets.
    static bool isLockable (const char* id);
    bool isLocked (const char* id) const;
    void setLocked (const char* id, bool locked);

    void load (int index);
    void step (int delta);                       // previous / next, wraps around
    bool save (const juce::String& name);        // as a user preset; replaces one of the same name
    bool rename (int index, const juce::String& newName);
    bool remove (int index);

private:
    float valueOf (const Preset&, const char* id) const;
    void setCurrent (const Preset&);

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> presets;
};
