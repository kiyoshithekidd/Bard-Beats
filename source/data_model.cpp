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

static const int SCALE_PERIOD[SCALE_COUNT] = {
    7, // Dorian (7 notes/oct)
    5, // Minor Pentatonic (5 notes/oct)
    7, // Major (7 notes/oct)
    7, // Natural Minor (7 notes/oct)
    12 // Chromatic (12 notes/oct)
};

uint8_t getScaleNote(uint8_t scaleId, uint8_t rootNote, int octaveOffset, int degree) {
    if (scaleId >= SCALE_COUNT) scaleId = SCALE_DORIAN;
    int semitone = 0;
    if (scaleId == SCALE_CHROMATIC) {
        semitone = degree;
    } else {
        int period = SCALE_PERIOD[scaleId];
        int octShift = degree / period;
        int degInOct = degree % period;
        if (degInOct < 0) {
            degInOct += period;
            octShift--;
        }
        semitone = octShift * 12 + SCALE_INTERVALS[scaleId][degInOct];
    }
    int midi = 12 * (4 + octaveOffset) + (rootNote % 12) + semitone;
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
        case INST_WAR_KIT:      return "War Drum Kit";
        case INST_LOFI_KIT:     return "Modern Kit";
        case INST_UPRIGHT_BASS: return "Upright Bass";
        case INST_SUB_BASS:     return "Dungeon Sub";
        case INST_ELEC_BASS:    return "Electric Bass";
        case INST_OCARINA:      return "Clay Ocarina";
        case INST_WOOD_FLUTE:   return "Wooden Flute";
        case INST_PULSE_LEAD:   return "Chip Lead";
        case INST_SYNTH_BRASS:  return "Synth Brass";
        case INST_LUTE:         return "Bard's Lute";
        case INST_HARP:         return "Minstrel Harp";
        case INST_ELEC_GUITAR:  return "Elec Guitar";
        case INST_TAVERN_PIANO: return "Tavern Piano";
        case INST_ELEC_KEYS:    return "Elec Keys";
        default:                return "Instrument";
    }
}

const char* getInstrumentShortName(uint8_t instId) {
    switch (instId) {
        case INST_TAVERN_KIT:   return "TavKit";
        case INST_CHIP_KIT:     return "8BitKt";
        case INST_WAR_KIT:      return "WarKit";
        case INST_LOFI_KIT:     return "ModKit";
        case INST_UPRIGHT_BASS: return "UpBass";
        case INST_SUB_BASS:     return "SubBas";
        case INST_ELEC_BASS:    return "EBass";
        case INST_OCARINA:      return "Ocarin";
        case INST_WOOD_FLUTE:   return "Flute";
        case INST_PULSE_LEAD:   return "ChipLd";
        case INST_SYNTH_BRASS:  return "Brass";
        case INST_LUTE:         return "Lute";
        case INST_HARP:         return "Harp";
        case INST_ELEC_GUITAR:  return "EGtr";
        case INST_TAVERN_PIANO: return "Piano";
        case INST_ELEC_KEYS:    return "EKeys";
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

const char* getDegreeButtonBadge(int degree) {
    static const char* kBadges[8] = {
        "<L", "^U", ">R", "vD", "Y", "X", "A", "B"
    };
    if (degree < 0 || degree >= 8) return "";
    return kBadges[degree];
}

const char* getShoulderModeName(uint8_t mode) {
    switch (mode) {
        case SHOULDER_OCTAVE_HOLD: return "OCT";
        case SHOULDER_SEMITONE:    return "b/#";
        case SHOULDER_SUS_ROLL:    return "ROLL";
        default:                   return "OCT";
    }
}

const char* getAudioOutModeName(uint8_t mode) {
    switch (mode) {
        case AUDIO_OUT_AUTO:      return "AUTO";
        case AUDIO_OUT_SPEAKER:   return "SPKR";
        case AUDIO_OUT_HEADPHONE: return "HDPH";
        default:                  return "AUTO";
    }
}

void initDefaultConfig(AppConfig& cfg) {
    std::memset(&cfg, 0, sizeof(AppConfig));
    std::memcpy(cfg.magic, "BBCF", 4);
    cfg.version = 1;
    cfg.masterVolume = 100;
    cfg.audioOutMode = AUDIO_OUT_AUTO;
    cfg.shoulderMode = SHOULDER_OCTAVE_HOLD;
    cfg.spellbookQuantizedCue = 0;
    cfg.lastLoopId = 1;
    cfg.lastSongId = 1;
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
    if (trackIdx == TRACK_DRUMS) {
        static const uint8_t kDrumKits[4] = {
            INST_TAVERN_KIT, INST_CHIP_KIT, INST_WAR_KIT, INST_LOFI_KIT
        };
        for (int i = 0; i < 4; ++i) {
            if (kDrumKits[i] == currentInstId) return kDrumKits[(i + 1) % 4];
        }
        return INST_TAVERN_KIT;
    }

    static const uint8_t kMelodicInsts[12] = {
        INST_UPRIGHT_BASS, INST_SUB_BASS, INST_ELEC_BASS,
        INST_OCARINA, INST_WOOD_FLUTE, INST_PULSE_LEAD, INST_SYNTH_BRASS,
        INST_LUTE, INST_HARP, INST_ELEC_GUITAR, INST_TAVERN_PIANO, INST_ELEC_KEYS
    };
    for (int i = 0; i < 12; ++i) {
        if (kMelodicInsts[i] == currentInstId) return kMelodicInsts[(i + 1) % 12];
    }
    return INST_LUTE;
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
    cell.notes[0] = midiNote;
    cell.velocity = velocity;
    cell.pitchBendSemisQ4 = bendQ4;
}

void toggleStepNote(StepCell& cell, uint8_t midiNote, uint8_t velocity, int8_t bendQ4) {
    if (midiNote == 0) return;
    for (int i = 0; i < BB_MAX_POLY_PER_STEP; ++i) {
        if (cell.notes[i] == midiNote) {
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
    loop.version = 2;
    loop.loopId = id;
    std::snprintf(loop.name, sizeof(loop.name), "%s", name ? name : "New Loop");
    loop.bpm = bpm;
    loop.scaleId = SCALE_DORIAN;
    loop.rootNote = 2; // D
    loop.vocalVolume = 90;
    loop.numBars = 1;
    loop.swingPercent = 50;
    loop.reverbMix = 38;
    loop.vocalHighPass = 1;

    loop.tracks[TRACK_DRUMS].instrumentId   = INST_TAVERN_KIT;
    loop.tracks[TRACK_BASS].instrumentId    = INST_UPRIGHT_BASS;
    loop.tracks[TRACK_MELODY].instrumentId  = INST_OCARINA;
    loop.tracks[TRACK_HARMONY].instrumentId = INST_LUTE;

    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        loop.tracks[t].volume = 85;
        for (int s = 0; s < BB_MAX_STEPS; ++s) {
            clearStepCell(loop.tracks[t].steps[s]);
        }
    }
}

void migrateLoopV1ToV2(const LoopFileV1& v1, LoopFile& v2) {
    initEmptyLoop(v2, v1.loopId, v1.name, v1.bpm);
    v2.scaleId = v1.scaleId;
    v2.rootNote = v1.rootNote;
    v2.hasVocalClip = v1.hasVocalClip;
    v2.vocalVolume = v1.vocalVolume;
    v2.vocalMuted = v1.vocalMuted;
    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        v2.tracks[t].instrumentId = v1.tracks[t].instrumentId;
        v2.tracks[t].volume       = v1.tracks[t].volume;
        v2.tracks[t].muted        = v1.tracks[t].muted;
        for (int s = 0; s < BB_STEPS_PER_LOOP; ++s) {
            v2.tracks[t].steps[s] = v1.tracks[t].steps[s];
        }
    }
}

void initStarterLoop(LoopFile& loop, int presetIndex) {
    switch (presetIndex % BB_NUM_STARTER_LOOPS) {
        case 0: {
            initEmptyLoop(loop, 1, "Tavern Bounce", 112);
            for (int s = 0; s < 16; s += 2) {
                addStepNote(loop.tracks[TRACK_DRUMS].steps[s], DRUM_TAMB, (s % 4 == 0) ? 115 : 80);
            }
            addStepNote(loop.tracks[TRACK_DRUMS].steps[0], DRUM_KICK, 120);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[6], DRUM_KICK, 105);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[8], DRUM_KICK, 120);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[4], DRUM_SNARE, 110);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[12], DRUM_SNARE, 115);

            addStepNote(loop.tracks[TRACK_BASS].steps[0], getScaleNote(SCALE_DORIAN, 2, -1, 0), 105);
            addStepNote(loop.tracks[TRACK_BASS].steps[6], getScaleNote(SCALE_DORIAN, 2, -1, 4), 95);
            addStepNote(loop.tracks[TRACK_BASS].steps[8], getScaleNote(SCALE_DORIAN, 2, -1, 0), 105);
            addStepNote(loop.tracks[TRACK_BASS].steps[12], getScaleNote(SCALE_DORIAN, 2, -1, 3), 95);
            addStepNote(loop.tracks[TRACK_BASS].steps[14], getScaleNote(SCALE_DORIAN, 2, -1, 4), 90);

            addStepNote(loop.tracks[TRACK_HARMONY].steps[0], getScaleNote(SCALE_DORIAN, 2, 0, 0), 90);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[0], getScaleNote(SCALE_DORIAN, 2, 0, 2), 90);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[4], getScaleNote(SCALE_DORIAN, 2, 0, 4), 85);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[8], getScaleNote(SCALE_DORIAN, 2, 0, 0), 90);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[12], getScaleNote(SCALE_DORIAN, 2, 0, 3), 85);

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
        case 3: {
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
        case 4: {
            // 2-Bar (32-step) Bardic Ballad
            initEmptyLoop(loop, 5, "Bardic Ballad", 96);
            loop.numBars = 2;
            loop.scaleId = SCALE_DORIAN;
            for (int bar = 0; bar < 2; ++bar) {
                int o = bar * 16;
                addStepNote(loop.tracks[TRACK_DRUMS].steps[o + 0], DRUM_KICK, 110);
                addStepNote(loop.tracks[TRACK_DRUMS].steps[o + 8], DRUM_TAMB, 90);
                addStepNote(loop.tracks[TRACK_DRUMS].steps[o + 12], DRUM_SHAKER, 80);

                addStepNote(loop.tracks[TRACK_HARMONY].steps[o + 0], getScaleNote(SCALE_DORIAN, 2, 0, bar == 0 ? 0 : 3), 95);
                addStepNote(loop.tracks[TRACK_HARMONY].steps[o + 4], getScaleNote(SCALE_DORIAN, 2, 0, 2), 85);
                addStepNote(loop.tracks[TRACK_HARMONY].steps[o + 8], getScaleNote(SCALE_DORIAN, 2, 0, 4), 90);
                addStepNote(loop.tracks[TRACK_HARMONY].steps[o + 12], getScaleNote(SCALE_DORIAN, 2, 0, 2), 85);
            }
            addStepNote(loop.tracks[TRACK_MELODY].steps[0], getScaleNote(SCALE_DORIAN, 2, 0, 4), 105);
            addStepNote(loop.tracks[TRACK_MELODY].steps[6], getScaleNote(SCALE_DORIAN, 2, 0, 5), 100);
            addStepNote(loop.tracks[TRACK_MELODY].steps[10], getScaleNote(SCALE_DORIAN, 2, 0, 4), 95);
            addStepNote(loop.tracks[TRACK_MELODY].steps[16], getScaleNote(SCALE_DORIAN, 2, 0, 3), 105);
            addStepNote(loop.tracks[TRACK_MELODY].steps[22], getScaleNote(SCALE_DORIAN, 2, 0, 2), 100);
            addStepNote(loop.tracks[TRACK_MELODY].steps[26], getScaleNote(SCALE_DORIAN, 2, 0, 0), 105);
            break;
        }
        case 5: {
            initEmptyLoop(loop, 6, "Shield Ward", 104);
            loop.scaleId = SCALE_MAJOR;
            loop.tracks[TRACK_HARMONY].instrumentId = INST_HARP;
            addStepNote(loop.tracks[TRACK_DRUMS].steps[0], DRUM_KICK, 115);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[8], DRUM_CLAP, 105);
            addStepNote(loop.tracks[TRACK_BASS].steps[0], getScaleNote(SCALE_MAJOR, 2, -1, 0), 110);
            addStepNote(loop.tracks[TRACK_BASS].steps[8], getScaleNote(SCALE_MAJOR, 2, -1, 4), 110);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[0], getScaleNote(SCALE_MAJOR, 2, 0, 0), 100);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[4], getScaleNote(SCALE_MAJOR, 2, 0, 4), 100);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[8], getScaleNote(SCALE_MAJOR, 2, 0, 7), 110);
            break;
        }
        case 6: {
            initEmptyLoop(loop, 7, "Shadow Step", 132);
            loop.scaleId = SCALE_MINOR;
            loop.swingPercent = 58;
            addStepNote(loop.tracks[TRACK_DRUMS].steps[0], DRUM_KICK, 110);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[4], DRUM_CLOSED_HAT, 95);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[6], DRUM_SHAKER, 85);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[8], DRUM_KICK, 105);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[12], DRUM_CLAP, 100);
            addStepNote(loop.tracks[TRACK_BASS].steps[0], getScaleNote(SCALE_MINOR, 2, -1, 0), 115);
            addStepNote(loop.tracks[TRACK_BASS].steps[3], getScaleNote(SCALE_MINOR, 2, -1, 2), 95);
            addStepNote(loop.tracks[TRACK_BASS].steps[10], getScaleNote(SCALE_MINOR, 2, -1, 5), 105);
            addStepNote(loop.tracks[TRACK_MELODY].steps[4], getScaleNote(SCALE_MINOR, 2, 0, 2), 95);
            addStepNote(loop.tracks[TRACK_MELODY].steps[12], getScaleNote(SCALE_MINOR, 2, 0, 4), 100);
            break;
        }
        default: {
            initEmptyLoop(loop, 8, "Victory Fanfare", 128);
            loop.scaleId = SCALE_MAJOR;
            addStepNote(loop.tracks[TRACK_DRUMS].steps[0], DRUM_KICK, 125);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[4], DRUM_SNARE, 115);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[8], DRUM_KICK, 125);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[12], DRUM_SNARE, 120);
            addStepNote(loop.tracks[TRACK_DRUMS].steps[14], DRUM_TAMB, 115);

            addStepNote(loop.tracks[TRACK_HARMONY].steps[0], getScaleNote(SCALE_MAJOR, 2, 0, 0), 100);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[4], getScaleNote(SCALE_MAJOR, 2, 0, 2), 100);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[8], getScaleNote(SCALE_MAJOR, 2, 0, 4), 105);
            addStepNote(loop.tracks[TRACK_HARMONY].steps[12], getScaleNote(SCALE_MAJOR, 2, 0, 7), 115);

            addStepNote(loop.tracks[TRACK_MELODY].steps[0], getScaleNote(SCALE_MAJOR, 2, 0, 0), 110);
            addStepNote(loop.tracks[TRACK_MELODY].steps[4], getScaleNote(SCALE_MAJOR, 2, 0, 4), 110);
            addStepNote(loop.tracks[TRACK_MELODY].steps[8], getScaleNote(SCALE_MAJOR, 2, 0, 7), 120);
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

void initStarterSong(SongFile& song, int presetIndex) {
    switch (presetIndex % BB_NUM_STARTER_SONGS) {
        case 0:
            initEmptySong(song, 1, "Ballad of the Inn", 112);
            song.numEntries = 3;
            song.chain[0] = { 1, 2, SEC_INTRO,  0, {0, 0, 0} };
            song.chain[1] = { 3, 2, SEC_VERSE,  0, {0, 0, 0} };
            song.chain[2] = { 2, 2, SEC_CHORUS, 0, {0, 0, 0} };
            break;
        case 1:
            initEmptySong(song, 2, "Tavern Encore", 116);
            song.numEntries = 3;
            song.chain[0] = { 5, 1, SEC_INTRO,  0, {0, 0, 0} };
            song.chain[1] = { 7, 2, SEC_VERSE,  0, {0, 0, 0} };
            song.chain[2] = { 8, 2, SEC_OUTRO,  0, {0, 0, 0} };
            break;
        default:
            initEmptySong(song, 3, "Quest Medley", 124);
            song.numEntries = 3;
            song.chain[0] = { 6, 2, SEC_INTRO,  0, {0, 0, 0} };
            song.chain[1] = { 4, 2, SEC_BRIDGE, 0, {0, 0, 0} };
            song.chain[2] = { 8, 2, SEC_CHORUS, 0, {0, 0, 0} };
            break;
    }
}

const char* getRootNoteName(uint8_t rootNote) {
    static const char* kNames[12] = {
        "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
    };
    return kNames[rootNote % 12];
}

void transposeLoopRoot(LoopFile& loop, int semitoneDelta) {
    if (semitoneDelta == 0) return;
    int newRoot = (static_cast<int>(loop.rootNote) + semitoneDelta) % 12;
    if (newRoot < 0) newRoot += 12;
    loop.rootNote = static_cast<uint8_t>(newRoot);

    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        if (t == TRACK_DRUMS) continue; // Never transpose percussion MIDI triggers
        for (int s = 0; s < BB_MAX_STEPS; ++s) {
            StepCell& cell = loop.tracks[t].steps[s];
            for (int n = 0; n < BB_MAX_POLY_PER_STEP; ++n) {
                if (cell.notes[n] == 0) continue;
                int shifted = static_cast<int>(cell.notes[n]) + semitoneDelta;
                if (shifted < 24) shifted += 12;
                if (shifted > 108) shifted -= 12;
                cell.notes[n] = static_cast<uint8_t>(shifted);
            }
        }
    }
}

void remapLoopToScale(LoopFile& loop, uint8_t newScaleId) {
    if (newScaleId >= SCALE_COUNT) newScaleId = static_cast<uint8_t>(SCALE_DORIAN);
    uint8_t oldScale = (loop.scaleId < SCALE_COUNT) ? loop.scaleId : static_cast<uint8_t>(SCALE_DORIAN);
    if (oldScale == newScaleId) return;

    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        if (t == TRACK_DRUMS) continue;
        int baseOct = (t == TRACK_BASS) ? -1 : 0;
        for (int s = 0; s < BB_MAX_STEPS; ++s) {
            StepCell& cell = loop.tracks[t].steps[s];
            for (int n = 0; n < BB_MAX_POLY_PER_STEP; ++n) {
                uint8_t oldNote = cell.notes[n];
                if (oldNote == 0) continue;

                // Find closest degree in old scale across octave offsets -1..+1
                int bestDeg = 0;
                int bestOct = baseOct;
                int bestDist = 999;
                for (int oct = baseOct - 1; oct <= baseOct + 1; ++oct) {
                    for (int deg = 0; deg < BB_GRID_ROWS; ++deg) {
                        int cand = static_cast<int>(getScaleNote(oldScale, loop.rootNote, oct, deg));
                        int dist = cand - static_cast<int>(oldNote);
                        if (dist < 0) dist = -dist;
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestDeg = deg;
                            bestOct = oct;
                        }
                    }
                }
                cell.notes[n] = getScaleNote(newScaleId, loop.rootNote, bestOct, bestDeg);
            }
        }
    }
    loop.scaleId = newScaleId;
}

static uint32_t nextDiceRand(uint32_t& seed) {
    if (seed == 0) seed = 0x9E3779B9u;
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

void generateBardicDiceTrack(LoopFile& loop, int trackIdx, uint32_t& rngSeed) {
    if (trackIdx < 0 || trackIdx >= BB_NUM_TRACKS) return;
    TrackData& tr = loop.tracks[trackIdx];
    for (int s = 0; s < BB_MAX_STEPS; ++s) {
        clearStepCell(tr.steps[s]);
    }

    int numSteps = (loop.numBars == 2) ? BB_MAX_STEPS : 16;

    if (trackIdx == TRACK_DRUMS) {
        uint32_t grooveStyle = nextDiceRand(rngSeed) % 3;
        for (int s = 0; s < numSteps; ++s) {
            int stepInBar = s % 16;
            if (stepInBar == 0 || (grooveStyle != 1 && stepInBar == 8) ||
                (grooveStyle == 1 && stepInBar == 10) ||
                (grooveStyle == 2 && stepInBar == 6) ||
                ((nextDiceRand(rngSeed) % 100) < 18 && (stepInBar % 2 == 0))) {
                addStepNote(tr.steps[s], DRUM_KICK, 118);
            }
            if (stepInBar == 4 || stepInBar == 12 ||
                (grooveStyle == 2 && stepInBar == 15 && (nextDiceRand(rngSeed) % 2 == 0))) {
                addStepNote(tr.steps[s], DRUM_SNARE, 110);
            }
            if ((stepInBar % 2 == 0 && (nextDiceRand(rngSeed) % 100) < 75) ||
                (stepInBar % 2 == 1 && (nextDiceRand(rngSeed) % 100) < 30)) {
                addStepNote(tr.steps[s], DRUM_SHAKER, static_cast<uint8_t>(68 + (nextDiceRand(rngSeed) % 28)));
            }
            if ((stepInBar == 2 || stepInBar == 10 || stepInBar == 14) && (nextDiceRand(rngSeed) % 100) < 40) {
                addStepNote(tr.steps[s], DRUM_TAMB, 92);
            }
        }
        return;
    }

    if (trackIdx == TRACK_BASS) {
        static const int kBassDegrees[6] = { 0, 0, 2, 3, 4, 5 };
        for (int s = 0; s < numSteps; ++s) {
            int stepInBar = s % 16;
            bool hit = (stepInBar == 0 || stepInBar == 8);
            if (!hit && (stepInBar % 2 == 0)) hit = (nextDiceRand(rngSeed) % 100) < 55;
            if (!hit && (stepInBar % 2 == 1)) hit = (nextDiceRand(rngSeed) % 100) < 22;
            if (hit) {
                int deg = (stepInBar == 0) ? 0 : kBassDegrees[nextDiceRand(rngSeed) % 6];
                uint8_t note = getScaleNote(loop.scaleId, loop.rootNote, -1, deg);
                uint8_t vel = static_cast<uint8_t>(92 + (nextDiceRand(rngSeed) % 24));
                addStepNote(tr.steps[s], note, vel);
                tr.steps[s].gateLength = (stepInBar % 4 == 0 && (nextDiceRand(rngSeed) % 2 == 0)) ? 2 : 1;
            }
        }
        return;
    }

    if (trackIdx == TRACK_HARMONY) {
        static const int kChordRoots[4] = { 0, 3, 4, 2 };
        int barChord = kChordRoots[nextDiceRand(rngSeed) % 4];
        for (int s = 0; s < numSteps; ++s) {
            int stepInBar = s % 16;
            if (stepInBar == 0) barChord = (s == 0) ? 0 : kChordRoots[nextDiceRand(rngSeed) % 4];
            if (stepInBar == 0 || stepInBar == 6 || stepInBar == 8 || stepInBar == 12 ||
                ((nextDiceRand(rngSeed) % 100) < 20)) {
                int deg = (stepInBar == 0) ? barChord : (barChord + static_cast<int>(nextDiceRand(rngSeed) % 3) * 2);
                uint8_t n1 = getScaleNote(loop.scaleId, loop.rootNote, 0, deg);
                uint8_t n2 = getScaleNote(loop.scaleId, loop.rootNote, 0, deg + 2);
                addStepNote(tr.steps[s], n1, 88);
                if (stepInBar == 0 || stepInBar == 8) {
                    addStepNote(tr.steps[s], n2, 82);
                    tr.steps[s].gateLength = 3;
                } else {
                    tr.steps[s].gateLength = 2;
                }
            }
        }
        return;
    }

    // TRACK_MELODY: lyrical 8th/16th motif
    int curDeg = 0;
    for (int s = 0; s < numSteps; ++s) {
        int stepInBar = s % 16;
        bool place = (stepInBar == 0) ||
                     ((stepInBar % 2 == 0) && (nextDiceRand(rngSeed) % 100) < 62) ||
                     ((stepInBar % 2 == 1) && (nextDiceRand(rngSeed) % 100) < 26);
        if (place) {
            int delta = static_cast<int>(nextDiceRand(rngSeed) % 5) - 2; // -2..+2 step motion
            curDeg += delta;
            if (curDeg < -2) curDeg = 0;
            if (curDeg > 9) curDeg = 7;
            uint8_t note = getScaleNote(loop.scaleId, loop.rootNote, 0, curDeg);
            uint8_t vel = static_cast<uint8_t>(90 + (nextDiceRand(rngSeed) % 28));
            addStepNote(tr.steps[s], note, vel);
            tr.steps[s].gateLength = (stepInBar % 4 == 0 && (nextDiceRand(rngSeed) % 100) < 45) ? 2 : 1;
        }
    }
}

} // namespace BardBeats
