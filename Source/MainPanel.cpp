#include "MainPanel.h"
#include "ParamIDs.h"

namespace
{
    // ---- layout at 100 % (802 x 192, the content inside the margins; the goniometer comes from the editor shell)
    constexpr int   kHeaderInset = 4, kHeaderTop = 6, kHeaderHeight = 26;   // the title row, inside the content
    constexpr int   kPresetHeight = 24, kPresetOffset = 1;                  // the preset bar, nudged from the centre
    constexpr int   kSectionsTop = 44, kSectionsHeight = 100;               // the row of sections, inside the content
    constexpr int   kColumns = 11, kSectionGap = 12;                        // HYPER! | DIMENSION | SPACEY: 6 | 2 | 3 columns

    juce::RangedAudioParameter& parameter (HyperDimAudioProcessor& p, const char* id)
    {
        return *p.apvts.getParameter (id);
    }
}

MainPanel::MainPanel (HyperDimAudioProcessor& p)
    : proc (p),
      header (p.presets, "HYPER! HYPER!", "DIMENSION"),
      unison (parameter (p, pid::unison)),
      rate   (parameter (p, pid::rate),     "Rate"),
      detune (parameter (p, pid::detune),   "Detune"),
      spread (parameter (p, pid::spread),   "Spread"),
      hmix   (parameter (p, pid::hyperMix), "Mix"),
      dsize  (parameter (p, pid::dimSize),  "Size"),
      dmix   (parameter (p, pid::dimMix),   "Mix"),
      wideMode (parameter (p, pid::spaceyMode), true),
      wspace (parameter (p, pid::spaceySpacing), "Spacing", true),   // centre = the original setting
      wamt   (parameter (p, pid::spaceyAmount),  "Amount")
{
    sections[0] = { &hyper,     pid::hyperOn,  pid::hyperMix,     &hyperLed };
    sections[1] = { &dimension, pid::dimOn,    pid::dimMix,       &dimLed };
    sections[2] = { &spacey,    pid::spaceyOn, pid::spaceyAmount, &spaceyLed };

    hyper.setControl (0, unison, "Unison");
    hyper.setControl (1, retrig, "Retrig");
    hyper.setControl (2, rate);
    hyper.setControl (3, detune);
    hyper.setControl (4, spread);
    hyper.setControl (5, hmix);
    dimension.setControl (0, dsize);
    dimension.setControl (1, dmix);
    spacey.setControl (0, wideMode, "Mode");
    spacey.setControl (1, wspace);
    spacey.setControl (2, wamt);
    for (auto& s : sections)
    {
        s.view->setHeaderControls ({ s.led });
        addAndMakeVisible (s.view);
    }
    addAndMakeVisible (header);
    header.setPresetBar (kPresetHeight, kPresetOffset);

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    retrigAtt = std::make_unique<ButtonAttachment> (p.apvts, pid::retrig, retrig);

    header.presetBar.isMuted = [&p] { return p.isSafetyMuted(); };
    header.presetBar.resume = [&p] { p.resumeAfterSafetyMute(); };
    for (size_t i = 0; i < sections.size(); ++i)
        ledAtt[i] = std::make_unique<ButtonAttachment> (p.apvts, sections[i].onId, *sections[i].led);

    hyperLed.setTooltip ("Hyper on / off (also off while Mix is at 0 %)");
    dimLed.setTooltip ("Dimension on / off (also off while Mix is at 0 %)");
    spaceyLed.setTooltip ("Spacey on / off (also off while Amount is at 0 %)");
    hmix.setTooltip ("Hyper mix. At 0 % the Hyper section is off.");
    dmix.setTooltip ("Dimension mix. At 0 % the Dimension section is off.");
    wamt.setTooltip ("Spacey amount. At 0 % the Spacey section is off.");

    updateSections();
    startTimerHz (60);
}

MainPanel::~MainPanel()
{
    stopTimer();
}

void MainPanel::timerCallback()
{
    updateSections();
    header.presetBar.refresh();
}

void MainPanel::updateSections()
{
    auto value = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const bool off = proc.isBypassedNow();

    bool changed = ! sectionsKnown || off != allOff;
    for (auto& s : sections)
    {
        const bool on = value (s.onId) > 0.5f && value (s.amountId) > 0.0f;
        changed = changed || on != s.on;
        s.on = on;
    }
    if (! changed)
        return;

    // whatever is off is dimmed, but stays fully usable. Bypass (or Dry/Wet at 0) dims everything
    // that shapes the sound (the bottom strip dims its own controls).
    for (auto& s : sections)
    {
        s.view->setDimmed (off, ! s.on || off);
        s.led->setLit (s.on);
    }

    allOff = off;
    sectionsKnown = true;
}

void MainPanel::layout (juce::Rectangle<int> content, ga::BottomStrip& strip)
{
    header.setArea (content.reduced (kHeaderInset, 0).withTrimmedTop (kHeaderTop).withHeight (kHeaderHeight));
    strip.setArea (content.removeFromBottom (ga::BottomStrip::heightForWidth (content.getWidth())));

    const ga::ColumnGrid grid (content.withTrimmedTop (kSectionsTop).withHeight (kSectionsHeight), kColumns, kSectionGap, { 6, 2, 3 });
    hyper.setBounds (grid.section (0));
    dimension.setBounds (grid.section (1));
    spacey.setBounds (grid.section (2));
}
