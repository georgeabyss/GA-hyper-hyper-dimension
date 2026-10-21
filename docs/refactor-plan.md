# GA Framework – Refactor Plan

Extract a `ga-framework` repo from Hyper! Hyper! Dimension in 8 phases, each proven by a bit-identical audio test and an unchanged screenshot, and ship Hyper! Hyper! Dimension 0.9.1 on it before any Fase Phucker code exists.

## Ground rules

- **No behaviour change.** Every phase ends with the null test and the screenshot diff passing.
- **Parameter ID strings never change** (`mix`, `bassmono`, `bypass`, `os`, `osfilter`, `phaseguard`, `wide`, …). Hosts and saved sessions depend on them.
- **Plugin codes, bundle IDs and the preset folder stay** (`Gaau` / `Hhd1`, `com.ga.hyperhyperdimension`, `~/Music/GA/Hyper! Hyper! Dimension/Presets`, `.hhdpreset`).
- **One phase = one commit or PR,** so any regression points to a single step.
- Everything shared lives in `namespace ga`; plugin code keeps its own namespace (`hd`, later `fp`).

## Repo layout

A separate `ga-framework` repo, added to each plugin as a git submodule at `framework/`. It is pinned per plugin, editable in place, and Hyper! Hyper! Dimension keeps its repo, releases and history. A monorepo is the alternative if keeping the submodule in sync gets tedious.

```
ga-framework/
  cmake/GaPlugin.cmake        ga_add_plugin()
  ui/        LookAndFeel, Controls, Meters, Title, BottomStrip, EditorShell
  presets/   PresetManager, PresetBar
  dsp/       DspBlocks, OutputStage
  plugin/    ProcessorBase, SharedParams
  tests/     null-test harness
```

## Module map

| Framework module | Taken from | Change needed |
| --- | --- | --- |
| `ga::LookAndFeel`, `ga::ui` drawing | `UiStyle.*` | Rename `HyperLookAndFeel`; move `kWidth` / `kHeight` / `kScopeExtra` out (window size is per plugin) |
| Controls | `Controls.*` | Move as is: Knob, StepBox, LampButton, SquareToggle, SectionLed, LedToggle, MenuBox |
| Meters | `Meters.*` | Move as is: CorrelationMeter, Goniometer |
| Title + logo | `MainPanel::paintTitle` | Replace the hard-coded "HYPER! HYPER!" and "DIMENSION" with `ga::drawTitle(g, title, subtitle)` + `ga::drawLogo(g, area)` |
| `ga::BottomStrip` | `MainPanel` (`kOutX`, `kSetX`, `kCorX`, `kSd*`, attachments) | New component holding Bypass, Dry/Wet, Bass Mono, oversampling, filter, size, correlation, Phase Protect, scope; takes a width and lays out with today's proportions |
| `ga::EditorShell` | `PluginEditor.*` | Scaling, size menu, constraints, goniometer side panel; wraps a plugin panel that reports its base size |
| `ga::PresetManager` / `PresetBar` | `PresetManager.*`, `PresetBar.*` | Config struct: folder name, extension, preset parameter list, lockable IDs → properties, factory presets, list tag function (Spacey mode today) |
| `ga::dsp` | `DspBlocks.h` | Move as is: DelayLine, Biquad, Crossover, Allpass1/2, Hilbert |
| `ga::OutputStage` | `hd::Engine` (dryWet, bassMono, phaseProtect, loudnessMatch, K-weighting) | Takes dry + wet per sample block; owns the smoothing, Phase Protect and loudness state; exposes `protectActivity` |
| `ga::ProcessorBase` | `PluginProcessor.*` | Oversampling (both filter sets, latency, dry delay), equal-loudness bypass, safety mute, scope FIFO, state save/load with `prop` settings; plugin overrides `processWet()`, optional MIDI split hook for Retrig |
| `ga::SharedParams` | `ParamIDs.h`, `createParameterLayout` | `ga::pid` for the shared IDs + `ga::addSharedParameters(layout)` |
| `ga_add_plugin()` | `CMakeLists.txt` | Universal arch, macOS 11, company GA, `Gaau`, definitions, LTO, dead-strip, screenshot tool option |

Stays in each plugin: the engine, the main panel, the parameter layout of its own sections, factory presets, state migration (`setStateInformation`'s hyperon / unison fix-up).

## Phases

1. **Safety net.** Add a null-test tool: run 10 s of fixed audio (noise bursts, a kick, a sine sweep, silence) through every oversampling × filter combination and 3 presets, and save the outputs as reference files. Save reference screenshots with `tools/make_screenshots.sh`. Add both to CI.
2. **Build system.** Create `ga-framework` with `ga_add_plugin()`; Hyper! Hyper! Dimension builds through it. Check: same binary behaviour, plugin still scans in Logic and Ableton.
3. **UI building blocks.** Move LookAndFeel, Controls, Meters and the title/logo drawing. Check: screenshots identical.
4. **Bottom strip and editor shell.** Extract `ga::BottomStrip` and `ga::EditorShell` from MainPanel and PluginEditor. Check: screenshots identical at 75–200 %, goniometer open and closed.
5. **Presets.** Generalise PresetManager and PresetBar with the config struct. Check: factory and user presets load, locks persist, modified star appears.
6. **DSP blocks.** Move DspBlocks to `ga::dsp`. Check: null test bit-identical.
7. **Output stage.** Move dry/wet, Bass Mono, Phase Protect and loudness matching out of `hd::Engine` into `ga::OutputStage`. Check: null test within a tolerance set just above the measured difference (floating-point order may differ; see "Tolerances" below), Phase Protect LED still flashes.
8. **Processor shell.** Move oversampling, bypass, safety mute, scope and state into `ga::ProcessorBase`. Check: null test, latency reported unchanged, safety mute and resume work, old sessions open with all values.

### Tolerances

A fixed −120 dB is not good enough: in the Phase 1 test, multiplying the output by 1.000001 gave
differences of −116.9 to −120.4 dB, so three renders passed at −120 dB. When a phase cannot stay
bit-identical (Phase 7), measure the actual difference first and set the tolerance just above it,
never wider. The same goes for CI: it starts as strict as the local check (bit-identical audio,
`--pixel-tolerance 0`), and a tolerance is only added just above what the first run measures.

Then release Hyper! Hyper! Dimension 0.9.1 (Mac and Windows) from the framework, and only then create the Fase Phucker repo with the framework submodule.

## Additions for Fase Phucker

Built in the framework after 0.9.1, so Hyper! Hyper! Dimension can use them later:

- **Controls:** donut, modulation arc on `Knob`, orange solo button, band card, Stages box (StepBox with double/halve and drag).
- **Modulation:** a small `ga::ModMatrix` (source → parameter ID → depth) with depths stored as parameters.
- **Section header:** title + channel dropdown + dice + LED as one component.
- **Window size:** EditorShell already takes the base size, so 1260 × 726 needs no framework change.

## Fase Phucker parameter scheme

Decide before the first parameter is added; IDs can never change afterwards.

| Kind | Pattern | Example |
| --- | --- | --- |
| Per band, per channel | `b<band><ch>_<param>` | `b2s_freq` = band 2, Side, Freq |
| Per band | `b<band>_<param>` | `b2_on`, `b2_solo`, `b2_stereo` |
| Section link | `b<band>_<section>_link` | `b2_dist_link` |
| Modulation depth | `b<band>_mod_<section>_<param>` | `b2_mod_d_freq` |
| Global | plain | `bands`, `split1`, `split2` + the shared GA IDs |

About 200 parameters in total. Presets store only values that differ from the default, which `PresetManager` already supports ("missing = default").
