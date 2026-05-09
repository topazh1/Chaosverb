# Chaosverb

Generative reverb plugin built on JUCE 8 + WebView UI. Mutating dual-FDN reverb with parameter automation, BPM-synced mutation grid, per-section parameter locks, settings panel, and user-savable defaults.

**By Entropia Audio · v0.0.8b**

---

## Features

- **Dual-FDN crossfading reverb** — two independent feedback delay networks blend equal-power on each mutation
- **Mutation engine** — parameters auto-randomize at user-set intervals (BPM-sync OR free milliseconds)
- **Crossfade speed** — controls morph time between parameter sets (BPM-sync OR free ms)
- **Per-section parameter locks** — click any section title (Space/Spectral/Tone/Motion/Duck/Output) to freeze that group's params during mutations
- **Per-knob locks** — right-click any knob to lock individually
- **Sidechain ducking** — wet signal ducks against dry input transients
- **Stereo width** — Haas + M/S processing
- **Tape wow/flutter, chorus, saturation, spectral squash, tilt EQ**
- **Polar.sh license verification + 7-day trial system**
- **Settings panel** — gear icon (bottom-right): tooltips toggle, window size presets, brightness, save current as default UI preset
- **User-saved parameter defaults** — Cmd/Ctrl + click DEFAULT button to save current param state as cold-start defaults for new instances

## Build (macOS)

```bash
git clone https://github.com/juce-framework/JUCE.git --branch 8.0.4 --depth 1 ../JUCE
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Chaosverb_VST3 Chaosverb_AU Chaosverb_Standalone
```

Output: `build/plugins/Chaosverb/Chaosverb_artefacts/Release/`

## Build (Windows)

```bat
git clone https://github.com/juce-framework/JUCE.git --branch 8.0.4 --depth 1 ..\JUCE
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Chaosverb_VST3
```

## Install

**macOS:**
```bash
cp -R build/plugins/Chaosverb/Chaosverb_artefacts/Release/VST3/Chaosverb.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R build/plugins/Chaosverb/Chaosverb_artefacts/Release/AU/Chaosverb.component ~/Library/Audio/Plug-Ins/Components/
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Chaosverb.vst3 ~/Library/Audio/Plug-Ins/Components/Chaosverb.component
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Chaosverb.vst3
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/Components/Chaosverb.component
```

**Windows:** copy `Chaosverb.vst3` to `C:\Program Files\Common Files\VST3\` (admin).

## Architecture

```
plugins/Chaosverb/Source/
   PluginProcessor.*       Main DSP graph + parameters + user-defaults loader
   PluginEditor.*          WebView host + native ↔ JS bridges
   LicenseManager.h        Polar.sh API client + 7-day trial state machine
   ChaosverbFDN.h          Dual-FDN reverb core (Clouds-style Dattorro plate)
   ChaosverbEQ.*           Tilt + low/high cut + 200Hz damping bell
   ChaosverbChorus.*       Stereo chorus driven by mod rate
   ChaosverbSaturation.*   Padé tanh autogain
   ChaosverbSpectralSquash.* FDR multiband compressor (LUFS-normalized)
   ChaosverbWowFlutter.*   Tape pitch modulation
   clouds_*.h              Mutable Instruments grain engine port
   ui/public/              WebView UI (HTML/CSS/JS)
```

## License Engine

Plugin verifies licenses against [polar.sh](https://polar.sh) (Entropia Audio organization).

State machine:
- **Licensed** — verified key + activation_id on file → full audio
- **Trial Active** — within 7 days of trial start → full audio
- **Trial Expired** — silent
- **Unlicensed** — silent until activation or trial start

30-day offline grace period from last successful online validation.

Reset license + trial during testing:
- macOS: `rm ~/Library/Application\ Support/Chaosverb.license`
- Windows: delete `%APPDATA%\Chaosverb\Chaosverb.license`

## User Defaults

Plugin stores user-saved param defaults at:
- macOS: `~/Library/Application Support/Chaosverb.defaults`
- Windows: `%APPDATA%\Chaosverb\Chaosverb.defaults`

Hold **Cmd (Mac)** or **Ctrl (Win)** + click the **DEFAULT** button → label flips to "Save as Default" → click → snapshot current params as defaults. Plain click on DEFAULT restores those values. Fresh plugin instances also load with these values (host's project state still wins on project reload).

To revert to factory defaults: delete the `.defaults` file.

## Buy

[buy.polar.sh/polar_cl_uSz2Ytiofavyczc7hYX5687ljWevsRuYocdxK3s07V7](https://buy.polar.sh/polar_cl_uSz2Ytiofavyczc7hYX5687ljWevsRuYocdxK3s07V7)
