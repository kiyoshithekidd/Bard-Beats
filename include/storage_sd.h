#pragma once

#include "data_model.h"
#include <cstdint>
#include <vector>

namespace BardBeats {

struct LoopSummary {
    uint16_t loopId;
    uint16_t bpm;
    uint8_t  scaleId;
    uint8_t  hasVocal;
    char     name[32];
    char     filename[260];
};

struct SongSummary {
    uint16_t songId;
    uint16_t bpm;
    uint8_t  numEntries;
    uint8_t  hasVocal;
    char     name[32];
    char     filename[260];
};

bool storageInit();

// Loops (.bbloop)
uint16_t storageGetNextLoopId();
bool storageSaveLoop(const LoopFile& loop);
bool storageLoadLoop(uint16_t loopId, LoopFile& outLoop);
bool storageDeleteLoop(uint16_t loopId);
std::vector<LoopSummary> storageListLoops();
bool storageExportLoopMidi(const LoopFile& loop, char* outPathBuf, int outPathBufSize);

// Songs (.bbsong)
uint16_t storageGetNextSongId();
bool storageSaveSong(const SongFile& song);
bool storageLoadSong(uint16_t songId, SongFile& outSong);
bool storageDeleteSong(uint16_t songId);
std::vector<SongSummary> storageListSongs();

// Vocal WAV files (16-bit mono PCM at 16,364 Hz)
void storageGetLoopVocalPath(uint16_t loopId, char* outPath, int outSize);
void storageGetSongVocalPath(uint16_t songId, char* outPath, int outSize);
bool storageSaveVocalWav(const char* path, const int16_t* samples, uint32_t sampleCount, uint32_t sampleRate);
bool storageLoadVocalWav(const char* path, std::vector<int16_t>& outSamples);
bool storageDeleteFile(const char* path);

} // namespace BardBeats
