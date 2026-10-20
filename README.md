# Hyper! Hyper! Dimension

Build, then install system-wide into /Library/Audio/Plug-Ins:

    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=$HOME/Downloads/JUCE
    cmake --build build
    ./install.sh        # asks for your admin password, copies + signs VST3 and AU

If the build complains about the "!" in the product name, change PRODUCT_NAME in
CMakeLists.txt to "Hyper Hyper Dimension" (the in-plugin title stays the same).

Retrig ("ZAP!") needs MIDI routed to the plugin: every note-on resets all voices to zero detune.


## Interface (v0.7)
- Window 802 x 192 at 100 % (978 wide with the goniometer open), resizable: drag the bottom right corner
  (fixed aspect ratio, 60-250 %) or pick 75-200 % in the SIZE box. The size is saved with the session.
- Main row: HYPER! | DIMENSION | SPACEY across the whole width. Each section ends with its amount
  (Hyper Mix, Dimension Mix, Spacey Amount). The round LED at the right end of each header switches the
  section on/off instantly (20 ms fade, the knobs stay where they are). A section is off while its LED is off
  or its amount is at 0: the LED goes dark, its controls are dimmed, and it is skipped (no CPU for Hyper).
  Old sessions with Unison 0 load with Hyper switched off, so they sound the same.
- Bypass or Dry/Wet at 0 bypasses the whole effect and dims everything that shapes the sound
  (Bypass, the settings and the meters stay bright).
- one long rack faceplate, purple accent, silver gothic "GA" top right (company name in the build is also "GA").
- UNISON: number box, click the small arrows, drag up/down, or use the mouse wheel.
- RETRIG: only the small lens lamp reacts to clicks.
- Knobs: the whole knob area (including value and name) reacts. Drag up/down (the cursor hides and never hits the
  screen edge), Shift or Cmd for fine, wheel / trackpad, double-click or Alt-click resets to the default.
  SPACING holds briefly at its centre (= the original setting of every mode) while you drag across it.
- Right-click any knob or number/mode box: value list or "Enter value...", reset to default, and the host's own
  items (automation, MIDI learn) where the host provides them (VST3).
- SPACEY: MODE (click for the list; arrows and wheel step) | SPACING | AMOUNT.
- UNISON: 1 to 17 voices.
- SPACEY (the widener) has six modes:
  COMB (staggered comb peaks, mono sum unchanged), TWIN DRIVE (different distortion per channel),
  PHASE SMEAR (different moving allpass chains), FREQ SHIFT (+/- 2.2 Hz single-sideband beat),
  PRISM (8 bands, each auto-pans on its own LFO), LO-FI TWINS (different bit depth / sample rate per channel).
  The bass below about 150 Hz stays mono in every mode except FREQ SHIFT/PRISM where it is only gently touched.
- Output loudness is matched to the input automatically (slow-rising, fast-falling gain), so changing the voice count, mixes, WIDE or DIMENSION does not change the overall level. The match is K-weighted (ITU-R BS.1770), so it follows perceived loudness, not raw power.

## Presets (title bar)
- Dot-matrix display between the title and the GA logo: one grid of square LEDs, the text is lit cells
  of it (5 x 7 per letter). The number and the name; a star at the end once you change anything.
- The arrows step through Factory, then User presets, and wrap around. Click the display for the list
  (each preset shows its Spacey mode), Save, Save As, Rename, Delete and Show Presets Folder.
- 16 factory presets, every Spacey mode at least twice. User presets: one `.hhdpreset` file each in
  `~/Music/GA/Hyper! Hyper! Dimension/Presets` (easy to share).
- A preset holds the sound only: oversampling, the anti-alias filter and Bypass stay as they are.

## Spacey (the widener) and Bass Mono
- SPACEY group: MODE (Comb, Twin Drive, Phase Smear, Freq Shift, Prism, Lo-Fi Twins), SPACING and AMOUNT (blend, 0-100 %, 0 = off).
- SPACING is mode specific, centre = the original setting: Comb = delay spacing of the peaks, Twin Drive = drive, Phase Smear = depth and speed, Freq Shift = shift in Hz (0.2-4.2), Prism = pan depth, Lo-Fi Twins = bit depth.

## Bottom strip (output and settings) and the correlation box
- DRY/WET (small dial): global mix of the whole effect. It sits before the loudness match, so every setting is equally loud.
- BASS MONO (small dial): everything below the cutoff is summed to mono (dry and wet). Fully left = off.
- BYPASS: real host-visible bypass, 20 ms crossfade, lit while the effect is active. The effect is loudness
  matched to the input, so bypassing compares at equal loudness; the dry signal is delayed by the
  oversampling latency so the crossfade stays in phase.
- Oversampling: Off / 2x / 4x / 8x (default 2x), then the anti-alias filter: Linear Phase (FIR, more latency)
  or Low Latency (IIR, minimal latency, non-linear phase), then the window size. All three are drop-downs.
  The whole engine runs at the higher rate, which keeps the harmonics of Twin Drive and Lo-Fi Twins from folding
  back as aliasing. The latency is reported to the host and shown in the oversampling tooltip. Not automatable (it changes the
  latency). Costs CPU: 8x is 8 times the work. Phase Smear is corrected to sound the same at every factor.
  At 4x/8x the Freq Shift 90-degree network gets less accurate in the lowest mids (~150-300 Hz).
- Window size: 75-200 %, or drag the (purple) bottom right corner.
- Correlation box (right): -1 .. +1 over about 150 ms; purple = in phase, red = out of phase. After ~200 ms of
  silence it glides slowly back to 0 (about 1.5 s) and greys out. Its button opens the goniometer right next to it (auto gain, mid up, L/R on the
  diagonals); open/closed is saved with the session.

## Source layout
- `HyperDimEngine.h`: the DSP (Hyper, Dimension, Spacey, dry/wet, bass mono, loudness match), no JUCE.
- `PluginProcessor.*`: parameters, oversampling, retrig, bypass crossfade, the meter feed, session state.
- `PresetManager.*`: factory presets, user preset files, loading and the edited star.
- `PresetBar.*`: the dot-matrix preset display with its arrows and menu.
- `ParamIDs.h`: every parameter ID in one place (never change them: sessions and hosts use them).
- `UiStyle.*`: colours, fonts, drawing primitives (knobs, LED, recess, panels, screws, the GA logo), the look and feel.
- `Controls.*`: Knob, StepBox, LampButton, SquareToggle, SectionLed, MenuBox and the right-click menu.
- `Meters.*`: correlation meter and goniometer.
- `MainPanel.*`: the faceplate layout at 100 %, section on/off and dimming, the meters' timer.
- `PluginEditor.*`: the resizable window that scales the faceplate.
