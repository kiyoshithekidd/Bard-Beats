# Bard Beats

A standalone loop maker for the Nintendo 3DS. A step sequencer and MIDI synth that lives entirely on the handheld: make loops, play live, chain clips into songs, and sing over them. No PC, no account, no ecosystem.

## What it does

- **Step sequencer**: 1-bar (16-step) or 2-bar (32-step) loops (`1B` -> `2B:1` -> `2B:2`) with Root/Octave row highlighting, horizontal drag-sustain note lengths, Tap Tempo + `±1 BPM` steppers, adjustable Swing (`50%`, `58%`, `64%`), Smart Scale Remapping (zero ghost notes), 4 tracks (Drums, Bass, Melody, Harmony) with one-tap `[M]` mute and inline horizontal drag-volume (`0-200%`), **Bardic Dice** pattern generator (`DICE`), and **16 fantasy + modern instruments** (*Tavern Kit*, *8-Bit Kit*, *War Drum Kit*, *Modern Kit*, *Upright Bass*, *Dungeon Sub*, *Electric Bass*, *Clay Ocarina*, *Wooden Flute*, *Chip Lead*, *Synth Brass*, *Bard's Lute*, *Minstrel Harp*, *Elec Guitar*, *Tavern Piano*, *Elec Keys*). Tap the active track tab to open the **Instrument Picker** and layer any melodic instrument on Tracks 1-3!
- **Live play**: Play on the 3DS itself across Grid, 4x2 Sound Pads, and Rendered Instrument views (including a **Dual-Diamond Ocarina/Flute touch layout**, top-screen Stereoscopic 3D, and live VU pulse meter). Switch between **NOTE**, **CHORD** (octave-wrapped scale triads), and **ARP** (stepped arpeggiator) modes. Symmetric buttons (`D-Pad Left/Up/Right/Down` = Notes 1–4, `Y/X/A/B` = Notes 5–8) play notes, configurable **`L/R` Shoulder Modes** give you Momentary Octave Hold (`-1/+1/+2`), Chromatic `♭/♯` Accidentals, or Sustain + 16th Accent Roll, and the Circle Pad bends pitch ($\pm 2$ semitones on X) and adds vibrato (on Y), ocarina-style.
- **Jam and record**: The loop plays while you jam along (`JAM`), and tapping `REC` (with an automatic **4-beat woodblock count-in** when starting from stopped) quantizes your live notes and held sustain lengths onto the grid. An **8-step Multi-Track Undo Ring** (`UNDO`) reverts changes without stopping playback.
- **Saved loops (Spellbook)**: Ships with 8 starter bard spells (*Tavern Bounce*, *Fireball Cue*, *Healing Hymn*, *Goblin March*, *Bardic Ballad*, *Shield Ward*, *Shadow Step*, *Victory Fanfare*). Loops save to `sdmc:/3ds/BardBeats/loops/*.bbloop` with Instant (`Q:NOW`) or Next-Bar (`Q:BAR`) cue playback, persistent `config.bin` preferences, plus Standard MIDI (`.mid`) and Offline Stereo WAV (`.wav`) export.
- **Song mode & Multiple Songs**: Ships with 3 sample songs (*Ballad of the Inn*, *Tavern Encore*, *Quest Medley*). Chain up to 32 saved loops into `.bbsong` arrangements with 2-bar-aware timeline, **per-slot `[D][B][M][H]` track mute pills**, in-card `<LP`/`LP>` loop cycler, section tags, repeat counts (`1x..16x`), master Tap Tempo BPM, `COPY+TWK`, live `EDIT LP`, Song Shelf (`SONGS`), and full-song `.wav` bounce.
- **Vocal layer, Master Limiter & Mix Drawer**: Record voice with the built-in 3DS mic or an attached TRRS headset mic (`16,364 Hz` mono `.wav`, with 4-beat count-in and 22 ms latency compensation) per clip or over the whole song. Powered by a stereo-linked **Master Bus Compressor/Limiter** with **Auto Headphone Detection** (`3DS Speaker EQ` vs. `Headphone Full-Bass`), per-track `0-200%` sliders, **Master Volume (`0-150%`)**, **Per-Instrument Trim (`-50..+50%`)**, Root Key Transpose (`K`), Woodblock Metronome (`MET`), Vocal `HPF`, Tavern Reverb, Controls Cheat-Sheet (`?` / Hold `SELECT`), and optional Wi-Fi UDP MIDI bridge (`tools/midi_bridge.py`).
- **Zero-setup DSP audio**: Built with **OpenDSP** so audio works out of the box even without a dumped `dspfirm.cdc`.

## Status

- **v0.7.0 (Audio Studio Polish, Ergonomic 3DS Controls & Bardic Workflow — `#23`, `#24`, `#25`)**:
  - **Audio Overhaul (`#23`)**: Replaced hard-clamping master clipper with a stereo-linked **Master Bus Envelope Compressor/Limiter**, added **Auto Headphone Detection** (`DSP_GetHeadphoneStatus`) switching between **3DS Speaker Voicing** (`135 Hz` Butterworth HPF + `1.8 kHz` presence) and **Headphone Full-Bass**, added `2.5 ms` anti-click voice-steal crossfades, linear-interpolated sine LUT, pre-limiter metronome, **Per-Instrument Gain Trim (`-50..+50%`)**, **Master Volume (`0-150%`)**, and **inline horizontal drag-volume directly on Track Tabs** across every view.
  - **Controls & Ocarina Alignment (`#24`)**: Symmetric clockwise button mapping (`D-Pad Left/Up/Right/Down` = Notes 1–4, `Y/X/A/B` = Notes 5–8) with on-screen button badges, ordered 8 primary Ocarina finger holes + **Dual-Diamond Ocarina/Flute bottom touch layout**, configurable **`L`/`R` Shoulder Modes** (*Momentary Octave Hold*, *Flat/Sharp Semitones*, *Sustain Latch + 16th Accent Roll*), and fixed `getScaleNote()` octave wrapping for `CHORD` and `ARP`.
  - **Workflow & Sequencer Fixes (`#25`)**: **4-Beat Metronome Count-In** before `REC`/`MIC` from stopped, **Tap Tempo + `±1 BPM` steppers**, **Per-Slot `[D][B][M][H]` Track Mute pills** in Song Mode, **Smart Scale Remapping** (zero ghost notes), Grid Note-Off release fix, Root/Octave row highlighting, **8-step Multi-Track Undo Ring**, **Controls Cheat-Sheet Scroll** (`?` / Hold `SELECT`), and persistent `sdmc:/3ds/BardBeats/config.bin`.
- **v0.6.0**: Dedicated `384x152` illustrated sprite assets and pixel-aligned interactive note overlays for all 16 instruments across `sprites.t3x` and `sprites2.t3x` (`#21`).
- **v0.5.0**: Boosted `MASTER_GAIN`, normalized per-instrument synthesis loudness for 3DS speakers, and added `0-200%` per-track gain boost sliders in the Mix Drawer (`#22`).
- **v0.4.0**: Raised minimum UI text scale to `0.38f` across all views (`#18`), fixed Ocarina live-drag voice pileup and static clipping (`#19`), and expanded the instrument library to 16 fantasy + modern instruments with a visual Instrument Picker and cross-track layering (`#20`).
- **v0.3.0**: Fixed Track Mute (`#7`), 2-Bar Grid Cycling (`#8`), 2-Bar Song Timeline (`#9`), Song Slot Loop Cycler (`#10`), and Quantized Cue Sync (`#11`), and added Chord/Arp Mode (`#12`), Sustained Note Lengths (`#13`), Woodblock Metronome (`#14`), Bardic Dice (`#15`), Root Key Transpose (`#16`), and Live VU Pulse Meter (`#17`).

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
- `SELECT`: Tap to toggle Spellbook Quick Play | **Hold** to open Controls Cheat-Sheet Scroll
- `D-Pad (Left, Up, Right, Down)`: Play Scale Notes 1–4 live
- `Face Buttons (Y, X, A, B)`: Play Scale Notes 5–8 live (symmetric clockwise from 9 o'clock!)
- `Circle Pad`: Pitch bend ($\pm 2$ semitones Left/Right) and Vibrato (Up)
- `L / R`: Configurable in `MIX` — **OCT** (Momentary `-1 / +1 / +2` Octave Hold), **b/#** (Flat `-1st` / Sharp `+1st` chromatic accidentals), or **ROLL** (Hold `L` Sustain Latch / Hold `R` 16th Accent Roll)
- `Drag Track Tabs L/R`: Quick inline Track Volume (`0–200%`) from any view
- `3D Slider`: Adjust top-screen Stereoscopic 3D depth
- `L + R + START`: Exit to Homebrew Launcher

## Content and IP

All content is original. Bard and fantasy flavor, generic terms only: no "Dungeons & Dragons", "D&D", "Dungeon Master" spelled out, or other trademarks.
