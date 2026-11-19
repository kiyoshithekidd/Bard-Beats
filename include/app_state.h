#pragma once

#include "data_model.h"
#include "storage_sd.h"
#include <3ds.h>
#include <cstdint>
#include <vector>

namespace BardBeats {

enum ViewMode : uint8_t {
    VIEW_GRID = 0,
    VIEW_PADS,
    VIEW_INSTRUMENT,
    VIEW_SPELLBOOK,
    VIEW_SONG
};

struct AppState {
    LoopFile currentLoop;
    TrackData undoSnapshot;
    int      undoTrackIdx = 0;
    bool     hasUndo = false;

    ViewMode activeView = VIEW_GRID;
    int      activeTrack = TRACK_MELODY;
    int      octaveOffset = 0; // -1..+1

    bool     isPlaying = false;
    bool     isRecording = false;

    // Live play telemetry for visualizers
    float    circleBendSemis = 0.0f;
    float    circleVibrato = 0.0f;
    uint8_t  lastLiveNote = 0;
    int      lastLiveDegree = -1;
    int      liveFlashFrames = 0;

    // Spellbook saved loops cache
    std::vector<LoopSummary> savedLoops;
    int      spellbookPage = 0;
    int      selectedSavedIdx = -1;

    // Status toast banner
    char     toastMsg[64] = {0};
    int      toastFrames = 0;

    void init();
    void refreshSavedLoops();
    void setToast(const char* msg, int frames = 120);

    void snapshotForUndo(int trackIdx);
    void performUndo();
    void togglePlay();
    void toggleRecord();
    void saveCurrentLoop();
    void copyCurrentLoop();
    void renameCurrentLoop();
    void loadLoopById(uint16_t loopId);
    void syncAudioLoop();

    // Live note input (from buttons, pads, or instrument touch)
    void playLiveDegree(int degree, uint8_t velocity = 100);
    void releaseLiveNotes();

    // Update hardware inputs (buttons, circle pad)
    void handleHardwareInput(u32 kDown, u32 kHeld, u32 kUp, const circlePosition& cpad);
};

} // namespace BardBeats
