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

bool storageInit();
uint16_t storageGetNextLoopId();
bool storageSaveLoop(const LoopFile& loop);
bool storageLoadLoop(uint16_t loopId, LoopFile& outLoop);
bool storageDeleteLoop(uint16_t loopId);
std::vector<LoopSummary> storageListLoops();
bool storageExportLoopMidi(const LoopFile& loop, char* outPathBuf, int outPathBufSize);

} // namespace BardBeats
