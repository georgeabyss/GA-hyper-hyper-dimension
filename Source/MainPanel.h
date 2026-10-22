#pragma once

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/Header.h"
#include "ui/EditorShell.h"

// Title, preset bar, the three sections and the bottom strip, at 100 %. It sits on the GA editor
// shell's faceplate, which adds the goniometer and the scaling.
class MainPanel : public ga::Panel, private juce::Timer
{
public:
    static constexpr int kBaseWidth = 802, kBaseHeight = 192;   // size at 100 %, bottom strip included
    static constexpr int kMarginX = 26, kMarginY = 8;           // around the content

    explicit MainPanel (HyperDimAudioProcessor&);
    ~MainPanel() override;

    void layout (juce::Rectangle<int> content, ga::BottomStrip&) override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // A section of the main row. It is off while its LED is off or its amount (the last
    // control) is at 0; then its LED goes dark and its controls are dimmed.
    struct Section
    {
        const char* title;
        int x, w;
        const char* onId;
        const char* amountId;
        ga::SectionLed* led;
        std::vector<juce::Component*> controls;
        bool on = true;
    };

    void timerCallback() override;
    void updateSections();               // LEDs and dimming, also for the whole effect bypassed

    HyperDimAudioProcessor& proc;

    ga::Header header;   // title, preset bar and the GA logo

    // HYPER | DIMENSION | SPACEY, each ending with its amount
    ga::StepBox unison;
    ga::LampButton retrig;
    ga::Knob rate, detune, spread, hmix;
    ga::Knob dsize, dmix;
    ga::StepBox wideMode;
    ga::Knob wspace, wamt;
    ga::SectionLed hyperLed, dimLed, spaceyLed;
    std::array<Section, 3> sections;

    bool allOff = false;          // Bypass on or Dry/Wet at 0
    bool sectionsKnown = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> retrigAtt, ledAtt[3];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};
