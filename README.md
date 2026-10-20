# Hyper! Hyper! Dimension

![Hyper! Hyper! Dimension](docs/interface.png)

A unison, dimension and stereo widening effect for macOS (VST3 and Audio Unit).

It is a take on **Hyper/Dimension**, the effect from **Xfer Records' Serum**. All credit for the
original idea goes to Xfer Records; this is an independent re-implementation and not affiliated
with or endorsed by them. The twist:

- **Up to 17 unison voices** instead of 7, with Retrig on every MIDI note.
- **Spacey**, a third stage: a stereo widener with six very different modes.
- **Never louder, never out of phase:** the output is matched to the input's loudness, and
  Phase Protect keeps the effect mono-compatible.
- **Built for real sessions:** presets, oversampling, a correlation meter and goniometer, and a
  resizable interface.

**[Download the latest release](https://github.com/georgeabyss/GA-hyper-hyper-dimension/releases)**:
macOS 11 or newer, Apple Silicon and Intel.

---

## What it does

The signal runs through three sections, left to right. Each one ends with its amount: at 0 % the
section is switched off and costs no CPU.

### HYPER!: unison
Up to 17 copies of the signal, each with its own slowly drifting pitch, beating against each other
like a stack of detuned oscillators.

| Control | |
|---|---|
| **Unison** | number of voices, 1–17 |
| **Retrig** | restarts all voices in tune on every MIDI note, so each note starts focused and then spreads out ("laser zap"). Needs MIDI routed to the plug-in. |
| **Rate** | how fast the voices drift |
| **Detune** | how far they drift, in cents |
| **Spread** | how far the voices are spread across the stereo field |
| **Mix** | how much of the unison you hear |

### DIMENSION: space
Short, slowly moving delays, one per side and in opposite polarity: the classic chorus-ensemble
width. **Size** sets the delay times, **Mix** the amount.

### SPACEY: the widener
Six ways to make a signal wide. **Spacing** is the mode's main character control. Its centre
position is each mode's default, and the knob briefly holds there as you drag past.
**Amount** blends the effect in.

| Mode | What it does | Spacing |
|---|---|---|
| **Comb** | Two short delays, added to the left and subtracted from the right: comb filter peaks that differ per side. The mono sum stays exactly the same. | delay times |
| **Twin Drive** | A different saturation on each side (soft clipping left, wave folding right). The bass stays mono. | drive |
| **Phase Smear** | Two different, slowly moving phase shifters. Width without changing the tone. | depth and speed |
| **Freq Shift** | Shifts the left side up and the right side down by a few hertz: a slow, rotating beat. | shift, 0.2–4.2 Hz |
| **Prism** | Splits the signal into 8 bands and moves each one around the stereo field on its own LFO. | pan depth |
| **Lo-Fi Twins** | A different bit depth and sample rate on each side. The bass stays mono. | crush depth |

### Output
- **Bypass**: a click-free crossfade to the untouched dry signal.
- **Dry/Wet**: the whole effect against the dry signal. At 0 % it is bypassed, and the Bypass button
  can't be clicked then, so turning Dry/Wet up always brings the effect back.
- **Bass Mono**: everything below the chosen frequency is summed to mono. Fully left is off.
- **Phase Protect**: keeps the effect mono-compatible (see below).
- **Locks**: click the name *Dry/Wet* or *Bass Mono* to lock it. A padlock appears, and the value
  stays where it is when you switch presets.

### Under the hood
- **Equal loudness.** The output is continuously matched to the input's loudness, measured the way
  the ear hears it (K-weighted, ITU-R BS.1770). More voices, more width or more drive never make it
  louder, and Bypass compares at the same level. So you judge the sound, not the volume.
- **Phase Protect** (on by default). If the effect pushes the stereo correlation below +0.2, it pulls
  back what the effect changed until the signal is mono-compatible again, and the correlation meter
  flashes orange while it does. It never touches your input: width the source already has stays, and
  with all sections off the output is the input, sample for sample.
- **Safety mute.** Absurd levels (above +24 dBFS) or broken audio (NaN) silence the plug-in at once,
  like REAPER does. The display shows *OUTPUT MUTED*; click it to resume.
- **Oversampling** Off / 2× / 4× / 8× (default 2×) keeps Twin Drive and Lo-Fi Twins free of
  aliasing. The anti-alias filter is either **Linear Phase** (cleanest) or **Low Latency**. The
  added latency is reported to your DAW, which compensates for it.
- **Presets.** 16 factory presets, from subtle to absurd, covering every Spacey mode. Your own
  presets are single files in `~/Music/GA/Hyper! Hyper! Dimension/Presets`, easy to share.

---

## The interface

![The interface, numbered](docs/interface-annotated.png)

1. **Preset browser.** ◀ ▶ step through all presets. Click the display for the list, plus Save,
   Rename and Delete. A star appears once you change anything.
2. **Section LEDs.** Switch a section on or off without moving its knobs. Dark while the section
   is off, either through its LED or because its amount is at 0. A section that is off is dimmed.
3. **HYPER!**: Unison, Retrig, Rate, Detune, Spread, Mix.
4. **DIMENSION**: Size, Mix.
5. **SPACEY**: Mode, Spacing, Amount.
6. **Bypass**, at equal loudness. Bypass and Dry/Wet at 0 % dim the whole interface.
7. **Dry/Wet** for the whole effect. Click the name to lock it (padlock, as shown).
8. **Bass Mono**: mono below this frequency. Click the name to lock it.
9. **Phase Protect** on / off.
10. **Oversampling.** The tooltip shows the current latency.
11. **Anti-alias filter**: Linear Phase or Low Latency.
12. **Window size**, 75–200 %. You can also drag the bottom right corner.
13. **Correlation meter.** +1 is mono and 0 is very wide. Below 0 (red) means parts of the
    signal cancel in mono. Flashes orange while Phase Protect works (as in the picture).
14. **Goniometer.** Opens the stereo scope next to the meter:

![With the goniometer open](docs/goniometer.png)

**Handling**

| | |
|---|---|
| Drag up / down | change a knob or number (hold Shift for fine steps) |
| Double-click or Alt-click | back to the default |
| Right-click | type a value, reset, or your DAW's automation and MIDI-learn options |
| Mouse wheel | small steps |

---

## Installing

Download the zip from the [releases](https://github.com/georgeabyss/GA-hyper-hyper-dimension/releases),
unzip it and follow **READ ME FIRST.txt**. The installer is not signed with a paid Apple certificate,
so macOS asks you to allow it once. Then pick VST3 and/or Audio Unit. In Logic, the plug-in is under
Audio Units → GA.

To uninstall, delete `Hyper! Hyper! Dimension.vst3` from `/Library/Audio/Plug-Ins/VST3` and
`Hyper! Hyper! Dimension.component` from `/Library/Audio/Plug-Ins/Components`.

---

## Building from source

Needs CMake, Ninja, the Xcode command line tools and [JUCE](https://juce.com) (expected in
`~/Downloads/JUCE`; otherwise pass `-DJUCE_DIR=...`).

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./install.sh                  # copies the plug-ins to /Library/Audio/Plug-Ins (asks for your password)
./make_installer.sh           # dist/HyperHyperDimension-<version>.zip, ready to share
./tools/make_screenshots.sh   # re-renders the images in docs/ from the real interface
```

The version number is set in `CMakeLists.txt` (`project(... VERSION ...)`).

| Source | |
|---|---|
| `HyperDimEngine.*`, `DspBlocks.h` | the DSP, without JUCE: one function per stage and per Spacey mode |
| `PluginProcessor.*`, `ParamIDs.h` | parameters, oversampling, bypass, session state |
| `PresetManager.*` | factory and user presets |
| `MainPanel.*`, `PresetBar.*`, `Controls.*`, `Meters.*`, `UiStyle.*` | the interface |
| `PluginEditor.*` | the resizable window |

---

## Credits

- The Hyper/Dimension idea: [Xfer Records](https://xferrecords.com), from Serum.
- Built with [JUCE](https://juce.com).
- Made by GA.
