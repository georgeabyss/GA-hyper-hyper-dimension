#include "MainPanel.h"
#include "ParamIDs.h"

namespace
{
    // ---- layout at 100 % (802 x 192, the content inside the margins; the goniometer comes from the editor shell)
    constexpr int   kHeaderInset = 4, kHeaderTop = 6, kHeaderHeight = 26;   // the title row, inside the content
    constexpr int   kPresetHeight = 24, kPresetOffset = 1;                  // the preset bar, nudged from the centre
    constexpr int   kPanelY = 52;
    constexpr int   kRowCY  = kPanelY + 50;    // centre line of all controls
    constexpr float kPitch  = 66.0f;           // the three sections fill the whole width
    constexpr int   kHyperX = 26,  kHyperW = 396;
    constexpr int   kDimX   = 434, kDimW   = 132;
    constexpr int   kSpX    = 578, kSpW    = 198;   // "Spacey" (the widener)

    float hyperSlot (int i) { return (float) kHyperX + kPitch * ((float) i + 0.5f); }
    float dimSlot   (int i) { return (float) kDimX   + kPitch * ((float) i + 0.5f); }
    float spSlot    (int i) { return (float) kSpX    + kPitch * ((float) i + 0.5f); }

    // section name and the rule underneath (the LED is a component)
    void drawHeader (juce::Graphics& g, int x, int w, const juce::String& title)
    {
        g.setFont (ga::ui::font (8.0f, true, 0.16f));
        g.setColour (juce::Colour (0xff8f92a0));
        g.drawText (title, x + 2, kPanelY + 5, 120, 10, juce::Justification::centredLeft, false);
        g.setColour (juce::Colours::black);
        g.drawHorizontalLine (kPanelY + 20, (float) x, (float) (x + w));
        g.setColour (juce::Colour (0xff2a2c31));
        g.drawHorizontalLine (kPanelY + 21, (float) x, (float) (x + w));
    }

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
    sections[0] = { "HYPER!",    kHyperX, kHyperW, pid::hyperOn,  pid::hyperMix,     &hyperLed,  { &unison, &retrig, &rate, &detune, &spread, &hmix } };
    sections[1] = { "DIMENSION", kDimX,   kDimW,   pid::dimOn,    pid::dimMix,       &dimLed,    { &dsize, &dmix } };
    sections[2] = { "SPACEY",    kSpX,    kSpW,    pid::spaceyOn, pid::spaceyAmount, &spaceyLed, { &wideMode, &wspace, &wamt } };

    for (auto& s : sections)
    {
        for (auto* c : s.controls)
            addAndMakeVisible (c);
        addAndMakeVisible (s.led);
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
        for (auto* c : s.controls)
            c->setAlpha (s.on && ! off ? 1.0f : ga::ui::kDimAlpha);
        s.led->setLit (s.on);
        s.led->setAlpha (off ? ga::ui::kDimAlpha : 1.0f);
    }

    allOff = off;
    sectionsKnown = true;
    repaint();   // headers and the names of Unison, Retrig and Mode are drawn here
}

void MainPanel::paint (juce::Graphics& g)
{
    // transparent: the editor shell draws the faceplate underneath, the header draws the title
    // section headers (the LEDs are components), dimmed while bypassed
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.setOpacity (allOff ? ga::ui::kDimAlpha : 1.0f);
        for (auto& s : sections)
            drawHeader (g, s.x, s.w, s.title);
    }

    // names for the controls that do not draw their own (dimmed with their section)
    auto namesOf = [&] (const Section& s, std::initializer_list<std::pair<float, const char*>> names)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.setOpacity (s.on && ! allOff ? 1.0f : ga::ui::kDimAlpha);
        for (auto& n : names)
            ga::ui::drawName (g, n.first, (float) kRowCY, n.second);
    };
    namesOf (sections[0], { { hyperSlot (0), "Unison" }, { hyperSlot (1), "Retrig" } });
    namesOf (sections[2], { { spSlot (0), "Mode" } });
}

void MainPanel::layout (juce::Rectangle<int> content, ga::BottomStrip& strip)
{
    header.setArea (content.reduced (kHeaderInset, 0).withTrimmedTop (kHeaderTop).withHeight (kHeaderHeight));
    strip.setArea (content.removeFromBottom (ga::BottomStrip::heightForWidth (content.getWidth())));
}

void MainPanel::resized()
{
    const int cy = kRowCY;
    auto place = [cy] (juce::Component& c, float cx, int w, int h)
    {
        c.setBounds (juce::Rectangle<int> (w, h).withCentre ({ juce::roundToInt (cx), cy }));
    };
    auto placeKnob = [cy] (juce::Component& k, float cx)
    {
        k.setBounds (juce::roundToInt (cx) - 28, cy - 22, 56, 70);
    };

    place (unison, hyperSlot (0), 30, 30);
    place (retrig, hyperSlot (1), 16, 16);
    placeKnob (rate,   hyperSlot (2));
    placeKnob (detune, hyperSlot (3));
    placeKnob (spread, hyperSlot (4));
    placeKnob (hmix,   hyperSlot (5));

    placeKnob (dsize, dimSlot (0));
    placeKnob (dmix,  dimSlot (1));

    place (wideMode, spSlot (0), 64, 19);
    placeKnob (wspace, spSlot (1));
    placeKnob (wamt,   spSlot (2));

    // LEDs at the right end of each header, in line with the section name
    for (auto& s : sections)
        s.led->setBounds (juce::Rectangle<int> (14, 14).withCentre ({ s.x + s.w - 10, kPanelY + 10 }));
}
