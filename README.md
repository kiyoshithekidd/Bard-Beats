# Bard Beats

A standalone loop maker for the Nintendo 3DS. A step sequencer and MIDI synth that lives entirely on the handheld: make loops, play live, chain clips into songs, and sing over them. No PC, no account, no ecosystem.

## What it does

- **Step sequencer**: 1-bar (16-step) or 2-bar (32-step) loops with adjustable Swing (`50%`, `58%`, `64%`), 4 tracks (Drums, Bass, Melody, Harmony), and 8 launch instruments (*Tavern Kit*, *8-Bit Kit*, *Upright Bass*, *Dungeon Sub*, *Ocarina*, *Chip Lead*, *Bard's Lute*, *Minstrel Harp*).
- **Live play**: Play on the 3DS itself across Grid, 4x2 Sound Pads, and Rendered Instrument views (with top-screen Stereoscopic 3D). Buttons (`A/B/X/Y/D-Pad`) play notes, `L/R` shifts octave, and the Circle Pad bends pitch ($\pm 2$ semitones on X) and adds vibrato (on Y), ocarina-style.
- **Jam and record**: The loop plays while you jam along (`JAM`), and tapping `REC` quantizes your live notes onto the grid. `UNDO` reverts your last pass without stopping playback.
- **Saved loops (Spellbook)**: Ships with 8 starter bard spells (*Tavern Bounce*, *Fireball Cue*, *Healing Hymn*, *Goblin March*, *Bardic Ballad*, *Shield Ward*, *Shadow Step*, *Victory Fanfare*). Loops save to `sdmc:/3ds/BardBeats/loops/*.bbloop` with Instant (`Q:NOW`) or Next-Bar (`Q:BAR`) cue playback, plus Standard MIDI (`.mid`) and Offline Stereo WAV (`.wav`) export.
- **Song mode & Multiple Songs**: Ships with 3 sample songs (*Ballad of the Inn*, *Tavern Encore*, *Quest Medley*). Chain up to 32 saved loops into `.bbsong` arrangements with section tags, repeat counts (`1x..16x`), one master BPM, `COPY+TWK`, live `EDIT LP`, Song Shelf (`SONGS`), and full-song `.wav` bounce.
- **Vocal layer & Mix Drawer**: Record voice with the built-in 3DS mic or an attached TRRS headset mic (`16,364 Hz` mono `.wav`), per clip or over the whole song, with 22 ms latency compensation, optional Vocal High-Pass Filter (`HPF`), per-track volume sliders, Tavern Reverb slider (`MIX`), and optional Wi-Fi UDP MIDI bridge (`tools/midi_bridge.py`).
- **Zero-setup DSP audio**: Built with **OpenDSP** so audio works out of the box even without a dumped `dspfirm.cdc`.

## Status

- **v0.2.0 (Complete & Building on `main`)**: Implements all 6 roadmap enhancements (#1–#6) — Offline WAV Bounce, Multiple Songs Shelf, Swing & 2-Bar (32-step) Loops, Live Mix Drawer, Spellbook Quantized Cue & Stereoscopic 3D, OpenDSP zero-setup audio, optional Wi-Fi MIDI Bridge, plus 8 starter bard spells and 3 sample songs.

See [PRD.md](PRD.md), [PERSONAS.md](PERSONAS.md), and [ARCHITECTURE.md](ARCHITECTURE.md) for the full spec and technical plan.

## Platform & Building

Nintendo 3DS homebrew (`.3dsx` for Homebrew Launcher and `.cia` for HOME Menu).

- **CI Builds**: Every push to `main` builds `BardBeats.3dsx` and `BardBeats.cia` via GitHub Actions.
- **Local Build** (requires devkitPro / devkitARM + `libctru` + `citro2d`, optionally `OpenDSP`):
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
- `3D Slider`: Adjust top-screen Stereoscopic 3D depth
- `L + R + START`: Exit to Homebrew Launcher

## Content and IP

All content is original. Bard and fantasy flavor, generic terms only: no "Dungeons & Dragons", "D&D", "Dungeon Master" spelled out, or other trademarks.
