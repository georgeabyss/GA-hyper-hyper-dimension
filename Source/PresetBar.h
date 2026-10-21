#pragma once

#include "PresetManager.h"
#include "ui/Style.h"

// Preset browser in the title bar: previous button, a dot-matrix display, next button.
// The display is one grid of square LEDs; the text is made by lighting cells of that grid
// (5 x 7 dots per letter). Click the display for the preset list and Save / Rename / Delete.
class PresetBar : public juce::Component, public juce::TooltipClient
{
public:
    explicit PresetBar (PresetManager&);

    void refresh();   // call regularly: picks up loads by the host, edits (the star) and the safety mute

    // safety mute: while it is on, the display blinks a warning and a click resumes
    std::function<bool()> isMuted;
    std::function<void()> resume;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

private:
    enum class Zone { none, prev, display, next };
    Zone zoneAt (juce::Point<float>) const;
    juce::Rectangle<float> zoneRect (Zone) const;

    void paintDisplay (juce::Graphics&, juce::Rectangle<float>);
    void paintArrowButton (juce::Graphics&, Zone);
    void showMenu();
    void askForName (const juce::String& title, const juce::String& initial, std::function<void (juce::String)> done);
    void confirm (const juce::String& title, const juce::String& message, std::function<void()> done);

    PresetManager& presets;
    juce::String text;          // what the display shows, e.g. "07 TWIN PEAKS"
    bool modified = false, menuOpen = false, muted = false, blinkOn = true;
    Zone hover = Zone::none;
    juce::SharedResourcePointer<ga::LookAndFeel> dialogLook;   // outlives any dialog that is still open
};
