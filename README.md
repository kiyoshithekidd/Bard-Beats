# Bard Beats

A standalone loop maker for the Nintendo 3DS. A step sequencer and MIDI synth that lives entirely on the handheld: make loops, play live, chain clips into songs, and sing over them. No PC, no account, no ecosystem.

## What it does

- **Step sequencer**: 16-step loops, one bar of 4/4 at 16th notes, with 4 tracks (Drums, Bass, Melody, Harmony) and 8 launch instruments (*Tavern Kit*, *8-Bit Kit*, *Upright Bass*, *Dungeon Sub*, *Ocarina*, *Chip Lead*, *Bard's Lute*, *Minstrel Harp*).
- **Live play**: Play on the 3DS itself across Grid, 4x2 Sound Pads, and Rendered Instrument views. Buttons (`A/B/X/Y/D-Pad`) play notes, `L/R` shifts octave, and the Circle Pad bends pitch ($\pm 2$ semitones on X) and adds vibrato (on Y), ocarina-style.
- **Jam and record**: The loop plays while you jam along (`JAM`), and tapping `REC` quantizes your live notes onto the 16-step grid. `UNDO` reverts your last pass without stopping playback.
- **Saved loops (Spellbook)**: Loops save with names to `sdmc:/3ds/BardBeats/loops/*.bbloop` and play back on cue with one tap, plus optional Standard MIDI (`.mid`) export.
- **Song mode**: Chain up to 32 saved loops into live songs (`sdmc:/3ds/BardBeats/songs/*.bbsong`) with section tags (*Intro*, *Verse*, *Chorus*, *Bridge*, *Outro*), repeat counts (`1x..16x`), one master BPM, one-tap `COPY+TWK` variations, and live `EDIT LP` drill-down editing.
- **Vocal layer**: Record voice with the built-in 3DS mic or an attached TRRS headset mic (`16,364 Hz` mono `.wav` in `sdmc:/3ds/BardBeats/vocals/`), per clip or over the whole song, with 22 ms latency compensation and non-destructive `UNDO` / `MUTE` / `CLEAR` that never touches the loop underneath.

## Status

- **Slice 1 (Complete)**: 16-step sequencer grid, 4x2 Pads, Rendered Instrument view, 8-instrument `ndsp` audio engine, live play with Circle Pad pitch bend, `UNDO`, and Spellbook SD save/load/MIDI export.
- **Slice 2 (Complete & Building on `main`)**: Live Song Mode clip chaining, `COPY+TWK`, drill-down loop editing (`< SONG`), and `mic:u` vocal layer recording/playback on `ndsp` Channel 1.

See [PRD.md](PRD.md), [PERSONAS.md](PERSONAS.md), and [ARCHITECTURE.md](ARCHITECTURE.md) for the full spec and technical plan.

## Platform & Building

Nintendo 3DS homebrew (`.3dsx` for Homebrew Launcher and `.cia` for HOME Menu).

- **CI Builds**: Every push to `main` builds `BardBeats.3dsx` and `BardBeats.cia` via GitHub Actions.
- **Local Build** (requires devkitPro / devkitARM + `libctru` + `citro2d`):
  ```sh
  python3 tools/gen_assets.py --audio-only
  make        # builds BardBeats.3dsx
  make cia    # builds BardBeats.cia (requires bannertool and makerom)
  ```

## Controls

- `START`: Play / Stop
- `SELECT`: Toggle Spellbook Quick Play
- `A / B / X / Y / D-Pad`: Play 8 scale notes live
- `Circle Pad`: Pitch bend (Left/Right) and Vibrato (Up)
- `L / R`: Octave Down / Up
- `L + R + START`: Exit to Homebrew Launcher

## Content and IP

All content is original. Bard and fantasy flavor, generic terms only: no "Dungeons & Dragons", "D&D", "Dungeon Master" spelled out, or other trademarks.
