#include "data_model.h"
#include <cstdio>
#include <cstring>

namespace BardBeats {

static const int SCALE_INTERVALS[SCALE_COUNT][BB_GRID_ROWS] = {
    { 0, 2, 3, 5, 7, 9, 10, 12 },   // Dorian
    { 0, 3, 5, 7, 10, 12, 15, 17 }, // Minor Pentatonic
    { 0, 2, 4, 5, 7, 9, 11, 12 },   // Major
    { 0, 2, 3, 5, 7, 8, 10, 12 },   // Natural Minor
    { 0, 1, 2, 3, 4, 5, 6, 7 }      // Chromatic
};

static const uint8_t DRUM_ROW_NOTES[BB_GRID_ROWS] = {
    DRUM_KICK,
    DRUM_TOM,
    DRUM_SNARE,
    DRUM_CLAP,
    DRUM_CLOSED_HAT,
    DRUM_OPEN_HAT,
    DRUM_TAMB,
    DRUM_SHAKER
};

static const char* DRUM_ROW_NAMES[BB_GRID_ROWS] = {
    "Kick", "Tom", "Snare", "Clap", "CHat", "OHat", "Tamb", "Shak"
};

uint8_t getScaleNote(uint8_t scaleId, uint8_t rootNote, int octaveOffset, int degree) {
    if (scaleId >= SCALE_COUNT) scaleId = SCALE_DORIAN;
    if (degree < 0) degree = 0;
    if (degree >= BB_GRID_ROWS) degree = BB_GRID_ROWS - 1;
    int midi = 12 * (4 + octaveOffset) + (rootNote % 12) + SCALE_INTERVALS[scaleId][degree];
    if (midi < 24) midi = 24;
    if (midi > 108) midi = 108;
    return static_cast<uint8_t>(midi);
}

uint8_t getDrumPieceNote(int row) {
    if (row < 0) row = 0;
    if (row >= BB_GRID_ROWS) row = BB_GRID_ROWS - 1;
    return DRUM_ROW_NOTES[row];
}

const char* getDrumPieceName(int row) {
    if (row < 0 || row >= BB_GRID_ROWS) return "?";
    return DRUM_ROW_NAMES[row];
}

const char* getInstrumentName(uint8_t instId) {
    switch (instId) {
        case INST_TAVERN_KIT:   return "Tavern Kit";
        case INST_CHIP_KIT:     return "8-Bit Kit";
        case INST_UPRIGHT_BASS: return "Upright Bass";
        case INST_SUB_BASS:     return "Dungeon Sub";
        case INST_OCARINA:      return "Ocarina";
        case INST_PULSE_LEAD:   return "Chip Lead";
        case INST_LUTE:         return "Bard's Lute";
        case INST_HARP:         return "Minstrel Harp";
        default:                return "Instrument";
    }
}

const char* getInstrumentShortName(uint8_t instId) {
    switch (instId) {
        case INST_TAVERN_KIT:   return "TavKit";
        case INST_CHIP_KIT:     return "8BitKit";
        case INST_UPRIGHT_BASS: return "UpBass";
        case INST_SUB_BASS:     return "SubBass";
        case INST_OCARINA:      return "Ocarina";
        case INST_PULSE_LEAD:   return "ChipLd";
        case INST_LUTE:         return "Lute";
        case INST_HARP:         return "Harp";
        default:                return "Inst";
    }
}

const char* getScaleName(uint8_t scaleId) {
    switch (scaleId) {
        case SCALE_DORIAN:      return "Dorian";
        case SCALE_MINOR_PENTA: return "Penta";
        case SCALE_MAJOR:       return "Major";
        case SCALE_MINOR:       return "Minor";
        case SCALE_CHROMATIC:   return "Chrom";
        default:                return "Dorian";
    }
}

const char* getTrackRoleName(int trackIdx) {
    switch (trackIdx) {
        case TRACK_DRUMS:   return "Drums";
        case TRACK_BASS:    return "Bass";
        case TRACK_MELODY:  return "Melody";
        case TRACK_HARMONY: return "Harmony";
        default:            return "Track";
    }
}

void getNoteName(uint8_t midiNote, char* outBuf, int bufSize) {
    static const char* NOTE_NAMES[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    if (midiNote == 0) {
        snprintf(outBuf, bufSize, "--");
        return;
    }
    int octave = (midiNote / 12) - 1;
    snprintf(outBuf, bufSize, "%s%d", NOTE_NAMES[midiNote % 12], octave);
}

uint8_t cycleTrackInstrument(int trackIdx, uint8_t currentInstId) {
    switch (trackIdx) {
        case TRACK_DRUMS:
            return (currentInstId == INST_TAVERN_KIT) ? INST_CHIP_KIT : INST_TAVERN_KIT;
        case TRACK_BASS:
            return (currentInstId == INST_UPRIGHT_BASS) ? INST_SUB_BASS : INST_UPRIGHT_BASS;
        case TRACK_MELODY:
            return (currentInstId == INST_OCARINA) ? INST_PULSE_LEAD : INST_OCARINA;
        case TRACK_HARMONY:
            return (currentInstId == INST_LUTE) ? INST_HARP : INST_LUTE;
        default:
            return currentInstId;
    }
}

void clearStepCell(StepCell& cell) {
    std::memset(&cell, 0, sizeof(StepCell));
    cell.gateLength = 1;
}

bool stepHasNote(const StepCell& cell, uint8_t midiNote) {
    if (midiNote == 0) return false;
    for (int i = 0; i < BB_MAX_POLY_PER_STEP; ++i) {
        if (cell.notes[i] == midiNote) return true;
    }
    return false;
}

bool stepHasAnyNote(const StepCell& cell) {
    for (int i = 0; i < BB_MAX_POLY_PER_STEP; ++i) {
        if (cell.notes[i] != 0) return true;
    }
    return false;
}

void addStepNote(StepCell& cell, uint8_t midiNote, uint8_t velocity, int8_t bendQ4) {
    if (midiNote == 0) return;
    for (int i = 0; i < BB_MAX_POLY_PER_STEP; ++i) {
        if (cell.notes[i] == midiNote) {
            cell.velocity = velocity;
            cell.pitchBendSemisQ4 = bendQ4;
            return;
        }
    }
    for (int i = 0; i < BB_MAX_POLY_PER_STEP; ++i) {
        if (cell.notes[i] == 0) {
            cell.notes[i] = midiNote;
            cell.velocity = velocity;
            cell.pitchBendSemisQ4 = bendQ4;
            if (cell.gateLength == 0) cell.gateLength = 1;
            return;
        }
    }
    // Overwrite oldest slot if full
    cell.notes[0] = midiNote;
    cell.velocity = velocity;
    cell.pitchBendSemisQ4 = bendQ4;
}

void toggleStepNote(StepCell& cell, uint8_t midiNote, uint8_t velocity, int8_t bendQ4) {
    if (midiNote == 0) return;
    for (int i = 0; i < BB_MAX_POLY_PER_STEP; ++i) {
        if (cell.notes[i] == midiNote) {
            // Remove note and compact
            for (int j = i; j < BB_MAX_POLY_PER_STEP - 1; ++j) {
                cell.notes[j] = cell.notes[j + 1];
            }
            cell.notes[BB_MAX_POLY_PER_STEP - 1] = 0;
            if (!stepHasAnyNote(cell)) {
                cell.velocity = 0;
                cell.pitchBendSemisQ4 = 0;
            }
            return;
        }
    }
    addStepNote(cell, midiNote, velocity, bendQ4);
}

void initEmptyLoop(LoopFile& loop, uint16_t id, const char* name, uint16_t bpm) {
    std::memset(&loop, 0, sizeof(LoopFile));
    std::memcpy(loop.magic, "BBLP", 4);
    loop.version = 1;
    loop.loopId = id;
    std::snprintf(loop.name, sizeof(loop.name), "%s", name ? name : "New Loop");
    loop.bpm = bpm;
    loop.scaleId = SCALE_DORIAN;
    loop.rootNote = 2; // D
    loop.vocalVolume = 90;

    loop.tracks[TRACK_DRUMS].instrumentId   = INST_TAVERN_KIT;
    loop.tracks[TRACK_BASS].instrumentId    = INST_UPRIGHT_BASS;
    loop.tracks[TRACK_MELODY].instrumentId  = INST_OCARINA;
    loop.tracks[TRACK_HARMONY].instrumentId = INST_LUTE;

    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        loop.tracks[t].volume = 85;
        for (int s = 0; s < BB_STEPS_PER_LOOP; ++s) {
            clearStepCell(loop.tracks[t].steps[s]);
        }
    }
}

void initStarterLoop(LoopFile& loop, int presetIndex) {
    switch (presetIndex % 4) {
        case 0: {
            initEmptyLoop(loop, 1, "Tavern Bounce", 112);
            // Drums: lively tavern groove
            for (int s = 0; s < 16; s += 2) {
                addStepNote(loop.tracks[TRACK_DRUMS].steps[s], DRUM_TAMB, (s % 4 == 0) ? 115 : 80);
            }
            addStepNote(loop.tracks[TRACK_DRUMS].steps[0], DRUM_KICK, 120);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[6], DRUM_KICK, 105);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[8], DRUM_KICK, 120);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[4], DRUM_SNARE, 110);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[12], DRUM_SNARE, 115);

            // Bass (D Dorian, octave -1)
            addStepNote(loop.tracks[TRACK_BASS].steps[0], getScaleNote(SCALE_DORIAN, 2, -1, 0), 105);
            addStepNote(loop.tracks[TRACK_BASS].steps[6], getScaleNote(SCALE_DORIAN, 2, -1, 4), 95);
            addStepNote(loop.tracks[TRACK_BASS].steps[8], getScaleNote(SCALE_DORIAN, 2, -1, 0), 105);
            addStepNote(loop.tracks[TRACK_BASS].steps[12], getScaleNote(SCALE_DORIAN, 2, -1, 3), 95);
            addStepNote(loop.tracks[TRACK_BASS].steps[14], getScaleNote(SCALE_DORIAN, 2, -1, 4), 90);

            // Harmony (Lute chords)
            addStepNote(loop.tracks[TRACK_HARMONY].steps[0], getScaleNote(SCALE_DORIAN, 2, 0, 0), 90);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[0], getScaleNote(SCALE_DORIAN, 2, 0, 2), 90);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[4], getScaleNote(SCALE_DORIAN, 2, 0, 4), 85);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[8], getScaleNote(SCALE_DORIAN, 2, 0, 0), 90);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[12], getScaleNote(SCALE_DORIAN, 2, 0, 3), 85);

            // Melody (Ocarina hook)
            addStepNote(loop.tracks[TRACK_MELODY].steps[0], getScaleNote(SCALE_DORIAN, 2, 0, 0), 100);
            addStepNote(loop.tracks[TRACK_MELODY].steps[3], getScaleNote(SCALE_DORIAN, 2, 0, 2), 95);
            addStepNote(loop.tracks[TRACK_MELODY].steps[6], getScaleNote(SCALE_DORIAN, 2, 0, 4), 105);
            addStepNote(loop.tracks[TRACK_MELODY].steps[10], getScaleNote(SCALE_DORIAN, 2, 0, 5), 100);
            addStepNote(loop.tracks[TRACK_MELODY].steps[12], getScaleNote(SCALE_DORIAN, 2, 0, 4), 95);
            break;
        }
        case 1: {
            initEmptyLoop(loop, 2, "Fireball Cue", 136);
            loop.scaleId = SCALE_MINOR;
            loop.tracks[TRACK_DRUMS].instrumentId = INST_CHIP_KIT;
            loop.tracks[TRACK_BASS].instrumentId = INST_SUB_BASS;
            loop.tracks[TRACK_MELODY].instrumentId = INST_PULSE_LEAD;

            addStepNote(loop.tracks[TRACK_DRUMS].steps[0], DRUM_KICK, 127);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[3], DRUM_KICK, 110);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[4], DRUM_CLAP, 120);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[8], DRUM_KICK, 127);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[12], DRUM_SNARE, 125);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[14], DRUM_OPEN_HAT, 100);

            addStepNote(loop.tracks[TRACK_BASS].steps[0], getScaleNote(SCALE_MINOR, 2, -1, 0), 120);
            addStepNote(loop.tracks[TRACK_BASS].steps[4], getScaleNote(SCALE_MINOR, 2, -1, 0), 110);
            addStepNote(loop.tracks[TRACK_BASS].steps[8], getScaleNote(SCALE_MINOR, 2, -1, 5), 115);
            addStepNote(loop.tracks[TRACK_BASS].steps[12], getScaleNote(SCALE_MINOR, 2, -1, 4), 115);

            addStepNote(loop.tracks[TRACK_MELODY].steps[0], getScaleNote(SCALE_MINOR, 2, 0, 0), 115);
            addStepNote(loop.tracks[TRACK_MELODY].steps[2], getScaleNote(SCALE_MINOR, 2, 0, 4), 110);
            addStepNote(loop.tracks[TRACK_MELODY].steps[4], getScaleNote(SCALE_MINOR, 2, 0, 7), 120);
            addStepNote(loop.tracks[TRACK_MELODY].steps[8], getScaleNote(SCALE_MINOR, 2, 0, 5), 110);
            addStepNote(loop.tracks[TRACK_MELODY].steps[12], getScaleNote(SCALE_MINOR, 2, 0, 4), 110);
            break;
        }
        case 2: {
            initEmptyLoop(loop, 3, "Healing Hymn", 88);
            loop.scaleId = SCALE_MAJOR;
            loop.tracks[TRACK_HARMONY].instrumentId = INST_HARP;

            addStepNote(loop.tracks[TRACK_DRUMS].steps[0], DRUM_SHAKER, 75);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[8], DRUM_TAMB, 80);

            addStepNote(loop.tracks[TRACK_HARMONY].steps[0], getScaleNote(SCALE_MAJOR, 2, 0, 0), 90);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[2], getScaleNote(SCALE_MAJOR, 2, 0, 2), 85);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[4], getScaleNote(SCALE_MAJOR, 2, 0, 4), 95);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[8], getScaleNote(SCALE_MAJOR, 2, 0, 7), 100);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[12], getScaleNote(SCALE_MAJOR, 2, 0, 4), 90);

            addStepNote(loop.tracks[TRACK_MELODY].steps[0], getScaleNote(SCALE_MAJOR, 2, 1, 2), 95);
            addStepNote(loop.tracks[TRACK_MELODY].steps[8], getScaleNote(SCALE_MAJOR, 2, 1, 4), 100);
            break;
        }
        default: {
            initEmptyLoop(loop, 4, "Goblin March", 124);
            loop.scaleId = SCALE_MINOR_PENTA;
            addStepNote(loop.tracks[TRACK_DRUMS].steps[0], DRUM_KICK, 120);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[4], DRUM_TOM, 110);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[8], DRUM_KICK, 120);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[10], DRUM_TOM, 100);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[12], DRUM_SNARE, 115);

            addStepNote(loop.tracks[TRACK_BASS].steps[0], getScaleNote(SCALE_MINOR_PENTA, 2, -1, 0), 110);
            addStepNote(loop.tracks[TRACK_BASS].steps[4], getScaleNote(SCALE_MINOR_PENTA, 2, -1, 1), 105);
            addStepNote(loop.tracks[TRACK_BASS].steps[8], getScaleNote(SCALE_MINOR_PENTA, 2, -1, 2), 110);
            addStepNote(loop.tracks[TRACK_BASS].steps[12], getScaleNote(SCALE_MINOR_PENTA, 2, -1, 4), 110);
            break;
        }
    }
}

const char* getSectionTagName(SectionTag tag) {
    switch (tag) {
        case SEC_INTRO:  return "Intro";
        case SEC_VERSE:  return "Verse";
        case SEC_CHORUS: return "Chorus";
        case SEC_BRIDGE: return "Bridge";
        case SEC_OUTRO:  return "Outro";
        case SEC_CUSTOM: return "Part";
        default:         return "Part";
    }
}

void initEmptySong(SongFile& song, uint16_t id, const char* name, uint16_t bpm) {
    std::memset(&song, 0, sizeof(SongFile));
    std::memcpy(song.magic, "BBSG", 4);
    song.version = 1;
    song.songId = id;
    std::snprintf(song.name, sizeof(song.name), "%s", name ? name : "New Song");
    song.bpm = bpm;
    song.numEntries = 0;
    song.hasSongVocal = 0;
    song.songVocalVolume = 95;
    song.songVocalMuted = 0;
}

void initStarterSong(SongFile& song) {
    initEmptySong(song, 1, "Ballad of the Inn", 112);
    song.numEntries = 3;
    song.chain[0] = { 1, 2, SEC_INTRO,  0, {0, 0, 0} };
    song.chain[1] = { 3, 2, SEC_VERSE,  0, {0, 0, 0} };
    song.chain[2] = { 2, 2, SEC_CHORUS, 0, {0, 0, 0} };
}

} // namespace BardBeats
