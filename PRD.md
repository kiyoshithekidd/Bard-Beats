# Bard Beats, PRD

Date: November 19, 2026
Status: Draft. Name chosen: Bard Beats.

## What it is

A standalone loop maker for the Nintendo 3DS. A step sequencer and MIDI synth that lives entirely on the handheld: make loops, play live, chain clips into songs, and sing over them. No PC, no account, no ecosystem. GarageBand-simple, bard-flavored. The tagline: a bard's loop maker for your pocket.

## Who it is for

Anyone who wants to make music on a handheld, starting with the person building it. One player, one 3DS, no audience required. The app is for making loops and songs, not for performing them live for a crowd.

## Why this gap is open

- KORG M01D and KORG DSN-12 are official 3DS music apps, a full workstation and a synth, but both were eShop-only and the eShop closed in March 2023. They cannot be bought now unless already owned.
- μcor Handheld is a free homebrew pocket synth and drum machine with a 16-step sequencer, released November 12, 2026, but it works best as a companion to the μcor modular platform on a computer or tablet. The offline synth on the 3DS is the smaller standalone piece.
- Nothing on the 3DS is a simple standalone loop maker that starts and ends on the handheld. That is the gap.

## Platform

Nintendo 3DS homebrew, on a modded 3DS XL. Ships as .3dsx first, with a .cia build later so it can launch from the home menu without the homebrew launcher (same packaging path as the Tomodachi Life Save Editor and DM's Toolkit). Built from the same source either way.

## The core loop: make a loop

1. Pick an instrument and a view.
2. The loop plays, and you jam along live until a pattern clicks.
3. Hit record and the pattern lands on the grid.
4. Restart or undo if you hate it. No commitment until you like it.
5. Save the loop, copy it, tweak it into the next part, chain loops into a song.

## Note input

- Both ways: tap notes straight onto the grid, or play live on the 3DS itself.
- No physical keyboard connection is possible. The 3DS has no USB port and no MIDI input, only a charging port, an SD slot, a headphone jack, and a mic. Live play happens on the device.
- A Wi-Fi bridge through a computer to reach a physical piano is a later maybe, not v1.

## Views

The player picks how they play. Three views:

- The rendered instrument on screen. Choose which instrument you are playing, and it is drawn on screen.
- A MIDI sound pad view. Pads for notes, tap to play.
- The step sequencer grid view. The classic grid, tap to place notes.

Buttons play notes, and the analog stick (circle pad) bends pitch, ocarina-style. The model is playing the ocarina in the new Zelda game on Switch 2.

## Loop length

16 steps. One bar of 4/4 at 16th notes, the step sequencer standard.

## Saved loops

- Loops are saved with names, so a loop can be found and played on cue.
- A quick play list shows saved loops by name, one tap to play.
- This serves the Spellbook Bard persona directly: a shelf of named loops is their spellbook, a loop per spell, played at the table when they cast.

## Song mode

Clips chain together in the app to make a song.

- Copy a clip to a new slot so you can tweak it into the next part instead of starting from scratch.
- A song needs: clip order plus repeat counts, sections (intro, verse, chorus, outro, or whatever order the player builds), one tempo for the whole song, a saved arrangement, and playback start to finish.

## Vocals

- Record voice with the built-in mic or an attached headset mic through the headphone jack.
- Both options: per clip, and over the whole song.
- The vocal is a layer over the arrangement, not baked into the loop.
- 3DS mic quality is sketchpad-level: mono, a bit lo-fi. The app is honest about that. It is for sketching songs, not studio takes.

## Editing

The song never freezes. The arrangement stays live: you can keep editing on top after the song is put together, and go in and edit individual loops from the song view. The song is an arrangement pointing at loops, never a baked render.

## UI and 3DS notes

- Built for the 3DS screens: top screen 400x240 for the instrument or the grid, bottom touchscreen 320x240 for navigation, pads, and quick actions.
- Big tap targets. The app is played with thumbs and a stylus, often with the loop running, so nothing important hides behind a fiddly menu.
- Keep it usable with the loop playing. Starting, stopping, recording, and undoing are always one tap away.

## Content and IP rules

- Bard and fantasy flavor, generic terms only. Never use "Dungeons & Dragons", "D&D", the ampersand logo, "Dungeon Master" spelled out, "d20 System", Forgotten Realms, beholder, mind flayer, or named settings and characters.
- Bard, lute, tavern, ocarina, minstrel, and song are safe. All content in the app is original.
- The flavor lives in the names, the instruments, and the visuals, not in the workflow. It never gets in the way of making a loop.

## Out of scope for v1

- Wi-Fi bridge to a physical piano (later maybe).
- MIDI or audio export (open decision, see below).
- Anything that needs a PC, an account, or an internet connection.
- Sharing, multiplayer, or a song store.

## Open decisions

- Tracks per loop: how many tracks does one loop have? Drums, bass, and melody, plus a fourth? Undecided.
- Instruments at launch: which instruments ship in v1?
- Save and export format: loops and songs save as app files on the SD card. MIDI export, audio export, or neither is undecided.

## Next steps

1. Name confirmed: Bard Beats. v1 scope as above.
2. Repo created: github.com/kiyoshithekidd/Bard-Beats. Next: personas, then a starter prompt for the build.
3. First build slice: step sequencer grid plus live play plus loop save, since the loop is the priority.
4. Second slice: song mode and vocals.
