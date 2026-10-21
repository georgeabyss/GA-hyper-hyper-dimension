#include "MainPanel.h"
#include "ParamIDs.h"

namespace
{
    // ---- layout at 100 % (802 x 192, plus kScopeExtra while the goniometer is open) ---
    constexpr int   kPanelY = 52;
    constexpr int   kRowCY  = kPanelY + 50;    // centre line of all controls
    constexpr float kPitch  = 66.0f;           // the three sections fill the whole width
    constexpr int   kHyperX = 26,  kHyperW = 396;
    constexpr int   kDimX   = 434, kDimW   = 132;
    constexpr int   kSpX    = 578, kSpW    = 198;   // "Spacey" (the widener)
    // bottom row: three boxes, 4 px apart: output | settings | phase
    constexpr int   kStripY = 156, kStripH = 28;
    constexpr int   kOutX = 26,  kOutW = 214;                     // Bypass, Dry/Wet, Bass Mono
    constexpr int   kSetX = kOutX + kOutW + 4, kSetW = 184;       // oversampling, filter, size
    constexpr int   kCorX = kSetX + kSetW + 4, kCorW = MainPanel::kBaseWidth - 26 - kCorX;   // correlation, Phase Protect, goniometer
    constexpr int   kScopeX = MainPanel::kBaseWidth - 14, kScopeY = 14, kScopeSize = MainPanel::kBaseHeight - 28;

    float hyperSlot (int i) { return (float) kHyperX + kPitch * ((float) i + 0.5f); }
    float dimSlot   (int i) { return (float) kDimX   + kPitch * ((float) i + 0.5f); }
    float spSlot    (int i) { return (float) kSpX    + kPitch * ((float) i + 0.5f); }

    // x positions inside the boxes
    constexpr int kSdBypass = kOutX + 9, kSdDryWet = kOutX + 36, kSdBass = kOutX + 118;
    constexpr int kSdOs = kSetX + 8, kSdFilt = kSdOs + 41, kSdSize = kSdFilt + 87;
    constexpr int kSdMeterW = 220;                                // the correlation meter, from the box's left edge + 7
    constexpr int kSdProtect = kCorX + 230, kSdProtectW = 84;

    constexpr int   kPresetX = 352, kPresetY = 15, kPresetW = 238, kPresetH = 24;   // centred between title and logo

    constexpr float kSizes[] = { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

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
      presetBar (p.presets),
      unison (parameter (p, pid::unison)),
      rate   (parameter (p, pid::rate),     "Rate"),
      detune (parameter (p, pid::detune),   "Detune"),
      spread (parameter (p, pid::spread),   "Spread"),
      hmix   (parameter (p, pid::hyperMix), "Mix"),
      dsize  (parameter (p, pid::dimSize),  "Size"),
      dmix   (parameter (p, pid::dimMix),   "Mix"),
      wideMode (parameter (p, pid::spaceyMode), true),
      wspace (parameter (p, pid::spaceySpacing), "Spacing", true),   // centre = the original setting
      wamt   (parameter (p, pid::spaceyAmount),  "Amount"),
      dryWet   (parameter (p, pid::dryWet),   "Dry/Wet",   false, ga::Knob::Style::mini),
      bassMono (parameter (p, pid::bassMono), "Bass Mono", false, ga::Knob::Style::mini),
      osFactor (parameter (p, pid::osFactor), true, false),
      osFilter (parameter (p, pid::osFilter), true, false)
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
    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &bypass, &dryWet, &bassMono, &phaseProtect, &osFactor, &osFilter, &sizeBox,
                                                             &correlation, &scopeButton })
        addAndMakeVisible (c);
    addChildComponent (goniometer);

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    retrigAtt = std::make_unique<ButtonAttachment> (p.apvts, pid::retrig, retrig);
    bypassAtt = std::make_unique<ButtonAttachment> (p.apvts, pid::bypass, bypass);
    protectAtt = std::make_unique<ButtonAttachment> (p.apvts, pid::phaseProtect, phaseProtect);

    // Dry/Wet and Bass Mono: click the name to lock the value against preset changes
    for (auto [knob, id] : { std::pair<ga::Knob*, const char*> { &dryWet, pid::dryWet }, { &bassMono, pid::bassMono } })
        knob->setLockable ([this, id = id] { proc.presets.setLocked (id, ! proc.presets.isLocked (id)); updateOutputControls(); });

    presetBar.isMuted = [&p] { return p.isSafetyMuted(); };
    presetBar.resume = [&p] { p.resumeAfterSafetyMute(); };
    for (size_t i = 0; i < sections.size(); ++i)
        ledAtt[i] = std::make_unique<ButtonAttachment> (p.apvts, sections[i].onId, *sections[i].led);

    hyperLed.setTooltip ("Hyper on / off (also off while Mix is at 0 %)");
    dimLed.setTooltip ("Dimension on / off (also off while Mix is at 0 %)");
    spaceyLed.setTooltip ("Spacey on / off (also off while Amount is at 0 %)");
    hmix.setTooltip ("Hyper mix. At 0 % the Hyper section is off.");
    dmix.setTooltip ("Dimension mix. At 0 % the Dimension section is off.");
    wamt.setTooltip ("Spacey amount. At 0 % the Spacey section is off.");
    bypass.setTooltip ("Bypass: click-free, at the same loudness as the effect. Dry/Wet at 0 % bypasses too.");
    phaseProtect.setTooltip ("Phase Protect: keeps the effect mono-compatible. The LED flashes orange while it audibly pulls the width back.");
    scopeButton.setTooltip ("Show / hide the goniometer");
    osFilter.setTooltip ("Anti-alias filter: Linear Phase (cleanest, more latency) or Low Latency (minimal delay)");
    sizeBox.setTooltip ("Window size (or drag the bottom right corner)");

    auto currentScale = [this] { return getScale != nullptr ? getScale() : 1.0f; };
    sizeBox.getItems = []
    {
        juce::StringArray items;
        for (float s : kSizes) items.add (juce::String (juce::roundToInt (s * 100.0f)) + "%");
        return items;
    };
    sizeBox.getSelected = [currentScale]
    {
        for (int i = 0; i < (int) std::size (kSizes); ++i)
            if (std::abs (kSizes[i] - currentScale()) < 0.01f) return i;
        return -1;
    };
    sizeBox.getText = [currentScale] { return juce::String (juce::roundToInt (currentScale() * 100.0f)) + "%"; };
    sizeBox.onSelect = [this] (int i) { if (setScale != nullptr) setScale (kSizes[i]); };

    scopeL.assign (16384, 0.0f);
    scopeR.assign (16384, 0.0f);

    const bool open = (bool) p.apvts.state.getProperty (prop::scopeOpen, false);   // saved with the session
    scopeButton.setToggleState (open, juce::dontSendNotification);
    scopeButton.onClick = [this] { setScopeOpen (scopeButton.getToggleState()); };
    goniometer.setVisible (open);
    setSize (baseWidth(), kBaseHeight);
    updateSections();
    startTimerHz (60);
}

MainPanel::~MainPanel()
{
    stopTimer();
}

void MainPanel::setScopeOpen (bool open)
{
    proc.apvts.state.setProperty (prop::scopeOpen, open, nullptr);
    goniometer.setVisible (open);
    setSize (baseWidth(), kBaseHeight);
    if (onLayoutChange != nullptr)
        onLayoutChange();
    repaint();
}

void MainPanel::timerCallback()
{
    updateSections();
    updateOutputControls();
    updateMeters();
    presetBar.refresh();

    if (proc.getLatencySamples() != shownLatency)
    {
        shownLatency = proc.getLatencySamples();
        osFactor.setTooltip ("Oversampling: keeps Twin Drive and Lo-Fi Twins free of aliasing. Costs CPU. Latency now: "
                             + juce::String (shownLatency) + " samples.");
    }
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
    // that shapes the sound; Bypass itself, the settings and the meters stay bright.
    for (auto& s : sections)
    {
        for (auto* c : s.controls)
            c->setAlpha (s.on && ! off ? 1.0f : ga::ui::kDimAlpha);
        s.led->setLit (s.on);
        s.led->setAlpha (off ? ga::ui::kDimAlpha : 1.0f);
    }
    for (auto* c : { (juce::Component*) &dryWet, (juce::Component*) &bassMono })
        c->setAlpha (off ? ga::ui::kDimAlpha : 1.0f);

    allOff = off;
    sectionsKnown = true;
    repaint();   // headers and the names of Unison, Retrig and Mode are drawn here
}

// Bypass, the locks and Phase Protect's dimming; cheap, so it runs on every timer tick
void MainPanel::updateOutputControls()
{
    // at Dry/Wet 0 the effect is already bypassed: Bypass cannot be clicked then, so turning
    // Dry/Wet up again always brings the effect back
    const bool canBypass = proc.apvts.getRawParameterValue (pid::dryWet)->load() > 0.0f;
    if (bypass.isEnabled() != canBypass)
    {
        bypass.setEnabled (canBypass);
        bypass.setAlpha (canBypass ? 1.0f : 0.35f);
    }
    phaseProtect.setAlpha (allOff ? ga::ui::kDimAlpha : 1.0f);
    dryWet.setLocked (proc.presets.isLocked (pid::dryWet));
    bassMono.setLocked (proc.presets.isLocked (pid::bassMono));
}

void MainPanel::updateMeters()
{
    const int n = proc.readScope (scopeL.data(), scopeR.data(), (int) scopeL.size());

    double lr = 0.0, ll = 0.0, rr = 0.0;
    for (int i = 0; i < n; ++i)
    {
        lr += (double) scopeL[(size_t) i] * scopeR[(size_t) i];
        ll += (double) scopeL[(size_t) i] * scopeL[(size_t) i];
        rr += (double) scopeR[(size_t) i] * scopeR[(size_t) i];
    }

    // nothing arriving (transport stopped) or below about -80 dBFS for ~200 ms: the meter glides back to 0
    const bool quiet = n == 0 || ll + rr < 1.0e-8 * (double) n;
    silentFrames = quiet ? silentFrames + 1 : 0;
    if (silentFrames > 12)
    {
        accLR = accLL = accRR = 0.0;
        correlation.setTarget (0.0f, false);
    }
    else if (n > 0)
    {
        // correlation = <L R> / sqrt (<L L> <R R>), averaged over roughly the last 150 ms
        accLR = accLR * 0.88 + lr;
        accLL = accLL * 0.88 + ll;
        accRR = accRR * 0.88 + rr;
        correlation.setTarget ((float) (accLR / std::sqrt (accLL * accRR + 1.0e-30)), true);
    }

    if (goniometer.isVisible())
        goniometer.push (scopeL.data(), scopeR.data(), n);

    // Phase Protect making an audible difference (side pulled back by more than 2 dB): its LED
    // flashes orange, held 0.5 s so short bursts are seen. Below 2 dB it only fine-tunes; measured
    // on the factory presets, only those that would otherwise go negative get past 2 dB.
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (! allOff && proc.engine.protectActivity.load (std::memory_order_relaxed) > 0.206f)   // 1 - 10^(-2/20)
        protectHoldUntil = now + 500.0;
    const float flash = now < protectHoldUntil ? 0.5f + 0.5f * std::cos ((float) (now * 0.001) * juce::MathConstants<float>::twoPi * 1.4f) : 0.0f;
    if (std::abs (flash - protectFlash) > 0.01f)
    {
        protectFlash = flash;
        phaseProtect.setAlarm (flash);
    }
}

void MainPanel::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();

    // one long faceplate
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1b1e), 0.0f, 0.0f, juce::Colour (0xff0d0e10), 0.0f, H, false));
    g.fillAll();
    g.setColour (juce::Colour (0xff4a4d54).withAlpha (0.7f));
    g.drawLine (0.0f, 1.5f, W, 1.5f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawLine (0.0f, H - 1.5f, W, H - 1.5f, 1.0f);

    ga::ui::drawTitle (g, "HYPER! HYPER!", "DIMENSION");
    ga::ui::drawLogo (g, (float) kBaseWidth - 30.0f, 14.0f);   // top right of the main panel, never higher than the title

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

    // bottom row: output | settings | phase (the goniometer opens to the right of the phase box)
    for (auto [x, w] : { std::pair<int, int> { kOutX, kOutW }, { kSetX, kSetW }, { kCorX, kCorW } })
        ga::ui::drawInsetPanel (g, juce::Rectangle<float> ((float) x, (float) kStripY, (float) w, (float) kStripH));

    if (goniometer.isVisible())
    {
        ga::ui::drawInsetPanel (g, juce::Rectangle<float> ((float) kScopeX, (float) kScopeY, (float) kScopeSize, (float) kScopeSize));
        g.setFont (ga::ui::font (6.5f, true, 0.14f));
        g.setColour (ga::ui::labelGrey);
        g.drawText ("GONIOMETER", kScopeX + 8, kScopeY + 6, 80, 9, juce::Justification::centredLeft, false);
    }

    // rack screws
    for (float x : { 12.0f, W - 12.0f })
        for (float y : { 22.0f, H - 22.0f })
            ga::ui::drawScrew (g, x, y);
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

    presetBar.setBounds (kPresetX, kPresetY, kPresetW, kPresetH);

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

    const int scy = kStripY + kStripH / 2;
    bypass.setBounds   (kSdBypass, scy - 9, 18, 18);
    dryWet.setBounds   (kSdDryWet, kStripY + 1, 78, kStripH - 2);
    bassMono.setBounds (kSdBass,   kStripY + 1, 86, kStripH - 2);
    phaseProtect.setBounds (kSdProtect, scy - 9, kSdProtectW, 18);
    osFactor.setBounds (kSdOs,   scy - 8, 38, 17);
    osFilter.setBounds (kSdFilt, scy - 8, 84, 17);
    sizeBox.setBounds  (kSdSize, scy - 8, 42, 17);

    correlation.setBounds (kCorX + 7, kStripY, kSdMeterW, kStripH);
    scopeButton.setBounds (kCorX + kCorW - 26, scy - 9, 18, 18);

    goniometer.setBounds (juce::Rectangle<int> (kScopeX, kScopeY, kScopeSize, kScopeSize).reduced (6));
}
