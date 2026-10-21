#pragma once

#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/Meters.h"
#include "PresetBar.h"

// The whole faceplate at 100 %. The editor scales it for the window size.
class MainPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int kBaseWidth = 802, kBaseHeight = 192;   // size at 100 %
    static constexpr int kScopeExtra = 176;                     // added to the width while the goniometer is open

    explicit MainPanel (HyperDimAudioProcessor&);
    ~MainPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isScopeOpen() const { return goniometer.isVisible(); }
    int baseWidth() const { return kBaseWidth + (isScopeOpen() ? kScopeExtra : 0); }

    std::function<void()> onLayoutChange;        // the goniometer opened / closed
    std::function<float()> getScale;             // current window scale, 1 = 100 %
    std::function<void (float)> setScale;

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
    void updateMeters();                 // correlation and goniometer from the processor's output
    void updateOutputControls();         // Bypass (not clickable at Dry/Wet 0), locks, Phase Protect
    void setScopeOpen (bool open);

    HyperDimAudioProcessor& proc;

    PresetBar presetBar;   // in the title bar, between the name and the GA logo

    // HYPER | DIMENSION | SPACEY, each ending with its amount
    ga::StepBox unison;
    ga::LampButton retrig;
    ga::Knob rate, detune, spread, hmix;
    ga::Knob dsize, dmix;
    ga::StepBox wideMode;
    ga::Knob wspace, wamt;
    ga::SectionLed hyperLed, dimLed, spaceyLed;
    std::array<Section, 3> sections;

    // bottom strip: Bypass | Dry/Wet, Bass Mono, Phase Protect | settings
    ga::SquareToggle bypass { ga::SquareToggle::Icon::power, true };   // lit while the effect is active
    ga::Knob dryWet, bassMono;
    ga::LedToggle phaseProtect { "Phase Protect" };
    ga::StepBox osFactor, osFilter;
    ga::MenuBox sizeBox;

    // correlation box; the goniometer opens to its right
    ga::CorrelationMeter correlation;
    ga::SquareToggle scopeButton { ga::SquareToggle::Icon::scope, false };
    ga::Goniometer goniometer;

    std::vector<float> scopeL, scopeR;
    double accLR = 0.0, accLL = 0.0, accRR = 0.0;
    int silentFrames = 0, shownLatency = -1;
    bool allOff = false;          // Bypass on or Dry/Wet at 0
    float protectFlash = 0.0f;    // 0..1, the orange flash of the Phase Protect LED
    double protectHoldUntil = 0.0;
    bool sectionsKnown = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> retrigAtt, bypassAtt, protectAtt, ledAtt[3];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};
