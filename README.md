# Organika

**Effect sequencer by NICEGUY** (VST3 / AU / Standalone), built with JUCE 8.

Organika makes a looped cymbal or hat stop sounding like a loop. Five effect lanes are sequenced in sixteenths, and the **LIFE** engine gives every detected hit a small variation in level, tone, stereo place, pitch and timing, so no two hits are the same.

## Main screen

- Five lanes, one under the other: **ENVELOPE, STEREO, PITCH, VOCODER, BLOOM** (also the order of the chain).
- **Length**: 1, 2 or 4 bars. All five lanes follow it.
- Each lane is groups of four sixteenths. The first sixteenth of every beat is the darker, stronger cell.
- **Click** a step to switch it on. **Hover** to raise its mix line, then **drag up or down** (or scroll) to set how much of the effect that sixteenth gets.
- Each lane has an on/off dot, an **AMOUNT** knob, and a settings button (or click its name) that opens its own screen.
- Header: presets (click the name for the library, arrows to step), **SAVE**, length, **LIFE**, mix and output.
- Footer: **hit fingerprint** (the last 28 hits, every bar different), **COMPARE: STATIC** (turns LIFE off so you can hear a plain loop), output meter.

The sequencer follows the host's transport. When the host is stopped it keeps running on its own clock, so the standalone app works too.

## Modules

| # | Module | What it does |
|---|--------|--------------|
| 1 | **Envelope** | Re-shapes every detected hit. **LONG**: attack, decay, sustain (held for one sixteenth), release. **SHORT**: attack and sustain locked at 0, one exponential decay set by **Snap** and **Curve**. **Drift** changes the length on every hit. |
| 2 | **Stereo** | 3-band EQ, then **SIDES** (pushes the hit outwards and flips side on every hit), **PAN** (places it, **Wander** moves it on every hit) or **MONO**. Width, Haas, mono below a frequency. Live goniometer. |
| 3 | **Pitch** | Three pitch voices (semitones, cents, level) layered on the dry hit. **Spread** pans them, **Scatter** re-detunes each voice per hit, **Glide**, Dry/Wet. |
| 4 | **Vocoder** | After Ableton's Vocoder: carrier **Noise / Tone / Modulator / Pitch Track**, 4 to 40 bands, range, BW, formant, attack/release, gate, unvoiced + sensitivity, depth, **Enhance**, **Precise / Retro**, Mono / Stereo / L-R channels, Dry/Wet. Output level follows the input automatically. |
| 5 | **Bloom** | A living modulation unit. An XY pad morphs **Chorus, Flanger, Phaser** (X) and depth (Y). **Wind** pushes the settings somewhere new on every hit, **Moss** is a dark filter that wanders slowly, **Grain** adds soft saturation. |

Each step's mix is how much of that lane's effect the sixteenth gets, scaled by the lane's AMOUNT.

## Presets

50 factory presets in 5 groups: **Water** (width and motion), **Soil** (short envelopes), **Wood** (pitch), **Leaf** (bloom), **Night** (vocoder). Each one sets the pattern and every module.

User presets (parameters plus steps) are stored in
`%APPDATA%\Organika\Presets` (Windows) or `~/Library/Organika/Presets` (macOS).

## Building with GitHub (no local tools needed)

1. Create a new repository on GitHub and upload the contents of this folder (Source, CMakeLists.txt, README.md and the `.github` folder).
   If the web upload skips the `.github` folder: Add file > Create new file, name it `.github/workflows/build.yml`, paste the contents of `BUILD-WORKFLOW.yml`, commit.
2. The build starts automatically. Open the **Actions** tab > latest run > download **Organika-Windows** (or macOS).
3. Copy `Organika.vst3` to `C:\Program Files\Common Files\VST3\` and rescan in your DAW.

## Building locally

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

`mockup/Organika_Mockup.html` is the interactive design prototype.

Fonts: Alegreya SC, Alegreya Sans, JetBrains Mono (SIL Open Font License), downloaded at configure time.
