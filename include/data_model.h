#pragma once

#include <cstdint>

namespace BardBeats {

constexpr int BB_NUM_TRACKS = 4;        // 0: Drums, 1: Bass, 2: Melody, 3: Harmony
constexpr int BB_STEPS_PER_LOOP = 16;   // 1 bar of 4/4 at 16th notes
constexpr int BB_MAX_POLY_PER_STEP = 4; // Up to 4 simultaneous notes/drums per step
constexpr int BB_MAX_SONG_CLIPS = 32;   // Up to 32 chained clip slots in a song
constexpr int BB_GRID_ROWS = 8;         // 8 scale degrees (or 8 drum pieces) in grid/pads

enum TrackRole : uint8_t {
    TRACK_DRUMS = 0,
    TRACK_BASS = 1,
    TRACK_MELODY = 2,
    TRACK_HARMONY = 3
};

enum InstrumentId : uint8_t {
    INST_TAVERN_KIT = 0, // Bodhran kick, woodblock/clap snare, tambourine, shaker
    INST_CHIP_KIT   = 1, // 8-bit triangle kick, LFSR noise snare, chip hi-hats
    INST_UPRIGHT_BASS = 2, // Karplus-Strong warm plucked double bass
    INST_SUB_BASS     = 3, // Resonant triangle/saw synth bass
    INST_OCARINA      = 4, // Sweet vessel flute (built for Circle Pad pitch bend)
    INST_PULSE_LEAD   = 5, // Classic chiptune 25%/50% duty pulse lead
    INST_LUTE         = 6, // Karplus-Strong plucked bard's lute
    INST_HARP         = 7, // Celtic minstrel harp
    INST_COUNT        = 8
};

enum ScaleId : uint8_t {
    SCALE_DORIAN      = 0, // Tavern Dorian: 0, 2, 3, 5, 7, 9, 10, 12
    SCALE_MINOR_PENTA = 1, // Folk Pentatonic: 0, 3, 5, 7, 10, 12, 15, 17
    SCALE_MAJOR       = 2, // Minstrel Major: 0, 2, 4, 5, 7, 9, 11, 12
    SCALE_MINOR       = 3, // Dungeon Minor: 0, 2, 3, 5, 7, 8, 10, 12
    SCALE_CHROMATIC   = 4, // Chiptune Chromatic (8 semitones from root)
    SCALE_COUNT       = 5
};

// Drum piece MIDI note mapping for Track 0 (Drums)
enum DrumPieceNote : uint8_t {
    DRUM_KICK       = 36, // Row 0 (bottom)
    DRUM_TOM        = 41, // Row 1
    DRUM_SNARE      = 38, // Row 2
    DRUM_CLAP       = 39, // Row 3
    DRUM_CLOSED_HAT = 42, // Row 4
    DRUM_OPEN_HAT   = 46, // Row 5
    DRUM_TAMB       = 54, // Row 6
    DRUM_SHAKER     = 70  // Row 7 (top)
};

#pragma pack(push, 1)
struct StepCell {
    uint8_t notes[BB_MAX_POLY_PER_STEP]; // MIDI note numbers (0 = empty)
    uint8_t velocity;                    // 0 = off, 90 = normal, 127 = accented
    int8_t  pitchBendSemisQ4;            // Pitch bend in 1/16th semitones (-32..+32)
    uint8_t gateLength;                  // Note length in 16th steps (1..16)
    uint8_t flags;                       // Bit 0: muted, Bit 1: slide
};

struct TrackData {
    uint8_t  instrumentId;
    uint8_t  volume;                     // 0..100
    uint8_t  muted;                      // 0 or 1
    uint8_t  reserved;
    StepCell steps[BB_STEPS_PER_LOOP];
};

struct LoopFile {
    char      magic[4];                  // "BBLP"
    uint16_t  version;                   // 1
    uint16_t  loopId;                    // Unique numeric ID (1..999)
    char      name[32];                  // Null-terminated loop name
    uint16_t  bpm;                       // 60..180
    uint8_t   scaleId;                   // ScaleId
    uint8_t   rootNote;                  // 0..11 (0=C, 2=D, etc.)
    uint8_t   hasVocalClip;              // 0 or 1 (Slice 2)
    uint8_t   vocalVolume;               // 0..100
    uint8_t   vocalMuted;                // 0 or 1
    uint8_t   reserved[17];
    TrackData tracks[BB_NUM_TRACKS];
};

enum SectionTag : uint8_t {
    SEC_INTRO = 0,
    SEC_VERSE = 1,
    SEC_CHORUS = 2,
    SEC_BRIDGE = 3,
    SEC_OUTRO = 4,
    SEC_CUSTOM = 5
};

struct SongChainEntry {
    uint16_t   loopId;
    uint8_t    repeatCount;
    SectionTag sectionTag;
    uint8_t    muteMask;
    uint8_t    reserved[3];
};

struct SongFile {
    char           magic[4];             // "BBSG"
    uint16_t       version;              // 1
    uint16_t       songId;
    char           name[32];
    uint16_t       bpm;
    uint8_t        numEntries;
    uint8_t        hasSongVocal;
    uint8_t        songVocalVolume;
    uint8_t        songVocalMuted;
    uint8_t        reserved[22];
    SongChainEntry chain[BB_MAX_SONG_CLIPS];
};
#pragma pack(pop)

// Helper functions
uint8_t getScaleNote(uint8_t scaleId, uint8_t rootNote, int octaveOffset, int degree);
uint8_t getDrumPieceNote(int row);
const char* getDrumPieceName(int row);
const char* getInstrumentName(uint8_t instId);
const char* getInstrumentShortName(uint8_t instId);
const char* getScaleName(uint8_t scaleId);
const char* getTrackRoleName(int trackIdx);
void getNoteName(uint8_t midiNote, char* outBuf, int bufSize);
uint8_t cycleTrackInstrument(int trackIdx, uint8_t currentInstId);

void initEmptyLoop(LoopFile& loop, uint16_t id, const char* name, uint16_t bpm = 110);
void initStarterLoop(LoopFile& loop, int presetIndex);

const char* getSectionTagName(SectionTag tag);
void initEmptySong(SongFile& song, uint16_t id, const char* name, uint16_t bpm = 112);
void initStarterSong(SongFile& song);

// Step editing helpers
void toggleStepNote(StepCell& cell, uint8_t midiNote, uint8_t velocity = 95, int8_t bendQ4 = 0);
void addStepNote(StepCell& cell, uint8_t midiNote, uint8_t velocity = 95, int8_t bendQ4 = 0);
bool stepHasNote(const StepCell& cell, uint8_t midiNote);
bool stepHasAnyNote(const StepCell& cell);
void clearStepCell(StepCell& cell);

} // namespace BardBeats
