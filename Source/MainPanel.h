#pragma once

#include "PluginProcessor.h"
#include "Controls.h"
#include "Meters.h"
#include "PresetBar.h"

// The whole faceplate at 100 %. The editor scales it for the window size.
class MainPanel : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel (HyperDimAudioProcessor&);
    ~MainPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isScopeOpen() const { return goniometer.isVisible(); }
    int baseWidth() const { return ui::kWidth + (isScopeOpen() ? ui::kScopeExtra : 0); }

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
        SectionLed* led;
        std::vector<juce::Component*> controls;
        bool on = true;
    };

    void timerCallback() override;
    void updateSections();               // LEDs and dimming, also for the whole effect bypassed
    void updateMeters();                 // correlation and goniometer from the processor's output
    void updateOutputControls();         // Bypass (not clickable at Dry/Wet 0), locks, Phase Protect
    void setScopeOpen (bool open);
    void paintTitle (juce::Graphics&);

    HyperDimAudioProcessor& proc;

    PresetBar presetBar;   // in the title bar, between the name and the GA logo

    // HYPER | DIMENSION | SPACEY, each ending with its amount
    StepBox unison;
    LampButton retrig;
    Knob rate, detune, spread, hmix;
    Knob dsize, dmix;
    StepBox wideMode;
    Knob wspace, wamt;
    SectionLed hyperLed, dimLed, spaceyLed;
    std::array<Section, 3> sections;

    // bottom strip: Bypass | Dry/Wet, Bass Mono, Phase Protect | settings
    SquareToggle bypass { SquareToggle::Icon::power, true };   // lit while the effect is active
    Knob dryWet, bassMono;
    LedToggle phaseProtect { "Phase Protect" };
    StepBox osFactor, osFilter;
    MenuBox sizeBox;

    // correlation box; the goniometer opens to its right
    CorrelationMeter correlation;
    SquareToggle scopeButton { SquareToggle::Icon::scope, false };
    Goniometer goniometer;

    std::vector<float> scopeL, scopeR;
    double accLR = 0.0, accLL = 0.0, accRR = 0.0;
    int silentFrames = 0, shownLatency = -1;
    bool allOff = false;          // Bypass on or Dry/Wet at 0
    float protectFlash = 0.0f;    // 0..1, the orange flash of the correlation box
    double protectHoldUntil = 0.0;
    bool sectionsKnown = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> retrigAtt, bypassAtt, protectAtt, ledAtt[3];
    juce::Path gothicGA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};
