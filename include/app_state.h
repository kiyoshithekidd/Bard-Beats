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

constexpr int BB_WAVE_BARS = 40;

struct AppState {
    // Active Loop & Undo
    LoopFile currentLoop;
    TrackData undoSnapshot;
    int      undoTrackIdx = 0;
    bool     hasUndo = false;

    // Active Song & Drill-Down Context
    SongFile currentSong;
    std::vector<SongSummary> savedSongs;
    int      selectedSongSlot = 0;
    int      songScrollOffset = 0;
    bool     editingFromSong = false;
    int      returnSongSlot = 0;

    // Vocal Layer & Non-Destructive Retake Shadow Buffer
    std::vector<int16_t> activeVocalSamples;
    std::vector<int16_t> undoVocalSamples;
    bool     hasVocalUndo = false;
    bool     undoVocalIsSong = false;
    uint16_t undoVocalTargetId = 0;
    bool     isRecordingVocal = false;
    bool     vocalRecIsSongScope = false;
    float    vocalWaveBars[BB_WAVE_BARS] = {0};

    // View & Transport
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
    void refreshSavedSongs();
    void setToast(const char* msg, int frames = 120);

    // Navigation
    void switchView(ViewMode newView);

    // Loop operations
    void snapshotForUndo(int trackIdx);
    void performUndo();
    void togglePlay();
    void toggleRecord();
    void saveCurrentLoop();
    void copyCurrentLoop();
    void renameCurrentLoop();
    void loadLoopById(uint16_t loopId);
    void syncAudioLoop();

    // Song operations
    void syncSongToAudio();
    void saveCurrentSong();
    void renameCurrentSong();
    void addCurrentLoopToSong();
    void copyAndTweakSongSlot(int slotIdx);
    void editSongSlotLoop(int slotIdx);
    void returnToSongFromEdit();
    void deleteSongSlot(int slotIdx);
    int  getTotalSongBars() const;
    const char* getLoopNameById(uint16_t loopId) const;

    // Vocal Layer operations (per-clip and whole-song)
    void loadActiveVocalForCurrentMode();
    void startVocalRecording(bool songScope);
    void stopVocalRecording(bool commit);
    void pollVocalRecording();
    void undoVocalTake();
    void clearVocalTake(bool songScope);
    void toggleVocalMute(bool songScope);

    // Live note input (from buttons, pads, or instrument touch)
    void playLiveDegree(int degree, uint8_t velocity = 100);
    void releaseLiveNotes();

    // Update hardware inputs (buttons, circle pad)
    void handleHardwareInput(u32 kDown, u32 kHeld, u32 kUp, const circlePosition& cpad);
};

} // namespace BardBeats
