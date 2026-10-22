# Changelog

## 0.9.2 (beta)

**A bigger window.** At 100 % the window is now as large as 117 % used to be (940 x 225 pixels), the
size of the GA family's next plug-in. Sessions saved at 100 % open at the new 100 %; other sizes
grow by the same step. The sound is unchanged: every render was checked against 0.9.1.

- **Bypassed, everything dims:** the section titles and the Unison, Retrig and Mode names now dim
  with the rest (they used to stay bright).
- **Under the hood:** the sections are built from the GA framework's new parts, shared with the next
  GA plug-ins.

## 0.9.1 (beta)

**Same plug-in, new foundation.** Hyper! Hyper! Dimension now runs on the GA framework, the shared
base for the next GA plug-ins. Nothing changes for you: every sound and every pixel was checked
against 0.9.0 and is identical, and your sessions, presets and locks load as before.

- **Windows:** the VST3 (Windows 10 or 11, 64-bit) is on the release page from now on, next to the Mac
  installer.

## 0.9.0 (beta)

**Now also for Windows** (VST3, Windows 10 or 11, 64-bit). The Windows build is new: feedback welcome.

- **Phase Protect** (formerly Phase Guard) has its own switch in the phase box. Its LED flashes orange
  while it audibly narrows the sound; small corrections happen quietly. Your old setting is kept.
- **Safety mute:** absurd levels (above +24 dBFS) or broken audio silence the plug-in instantly, like
  REAPER does. The display shows *OUTPUT MUTED*; click it to resume.
- **Lock Dry/Wet and Bass Mono:** click the name. A padlock appears, and the value stays put while you
  browse presets.
- **Bottom row in three boxes:** output, settings, phase. Bypass comes first.
- **Bypass at Dry/Wet 0 %** can't be clicked, so turning Dry/Wet up always brings the effect back.
- **Goniometer** scales to the signal's peak and never draws outside its frame.
- **Prism** splits its bands in phase, so panning them apart no longer cancels in mono.
- **Smaller download:** 9 MB instead of 29 MB on the Mac. The standalone app is gone.

## 0.8.0 (beta)

First release: Hyper (up to 17 voices, Retrig), Dimension, Spacey with six modes, Dry/Wet, Bass Mono,
loudness-matched output and bypass, Phase Guard, oversampling, 16 factory presets, correlation meter,
goniometer, resizable window. macOS (VST3, Audio Unit).
