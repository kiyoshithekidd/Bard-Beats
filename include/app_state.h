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
constexpr int BB_UNDO_LEVELS = 8;

struct AppState {
    // Persistent User Config (v0.7.0)
    AppConfig config;

    // Active Loop & 8-Step Multi-Track Undo Ring
    LoopFile currentLoop;
    LoopFile undoRing[BB_UNDO_LEVELS];
    int      undoHead = 0;
    int      undoCount = 0;
    bool     hasUndo = false;
    int      gridBarPage = 0; // 0 = steps 1..16, 1 = steps 17..32

    // Active Song & Drill-Down Context
    SongFile currentSong;
    LoopFile songSlotLoopCache; // Resolved loop for selectedSongSlot (for Song Track Bar & Mix)
    std::vector<SongSummary> savedSongs;
    int      selectedSongSlot = 0;
    int      songShelfPage = 0;
    bool     showSongShelf = false;
    bool     editingFromSong = false;
    int      returnSongSlot = 0;

    // Overlays: Mix Drawer (#4), Instrument Picker (#20), Help Scroll (v0.7.0), Wi-Fi MIDI (#6)
    bool     showMixDrawer = false;
    bool     showInstPicker = false;
    bool     showHelpOverlay = false;
    bool     wifiMidiEnabled = false;

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

    // 4-Beat Count-In pending action (0=none, 1=recGrid, 2=recClipVocal, 3=recSongVocal)
    int      pendingCountInAction = 0;

    // Tap Tempo timestamps (ms from osGetTime)
    uint64_t tapTimes[4] = {0};
    int      tapCount = 0;

    // Shoulder L/R runtime state (v0.7.0)
    bool     lHeld = false;
    bool     rHeld = false;
    int      rollHeldDegree = -1;
    int      lastRollStep = -1;
    int      selectHoldFrames = 0;
    bool     selectConsumedByHelp = false;
    int      hpPollCounter = 0;

    // Live play telemetry for visualizers
    float    circleBendSemis = 0.0f;
    float    circleVibrato = 0.0f;
    uint8_t  lastLiveNote = 0;
    int      lastLiveDegree = -1;
    int      liveFlashFrames = 0;

    // Spellbook saved loops cache & Quantized Cue (#5, #11)
    std::vector<LoopSummary> savedLoops;
    int      spellbookPage = 0;
    bool     spellbookQuantizedCue = false; // false = Instant, true = Next Bar
    bool     hasQueuedSpellbookLoop = false;
    LoopFile queuedSpellbookLoop;

    // Performance mode, live sustain recording, metronome, dice (#12, #13, #14, #15)
    int      perfMode = 0; // 0 = NOTE, 1 = CHORD, 2 = ARP
    int      arpHeldDegree = -1;
    int      lastArpStep = -1;
    int      arpCycleIdx = 0;
    int      recHeldStep = -1;
    uint8_t  recHeldNote = 0;
    bool     metronomeEnabled = false;
    uint32_t diceSeed = 0x13572468u;

    // Status toast banner
    char     toastMsg[64] = {0};
    int      toastFrames = 0;

    void init();
    void saveConfig();
    void syncMasterConfig();
    void refreshSavedLoops();
    void refreshSavedSongs();
    void refreshSongSlotLoopCache();
    void setToast(const char* msg, int frames = 120);

    // Navigation
    void switchView(ViewMode newView);

    // Loop operations
    void snapshotForUndo(int trackIdx = -1);
    void performUndo();
    void togglePlay();
    void toggleRecord();
    void saveCurrentLoop();
    void copyCurrentLoop();
    void renameCurrentLoop();
    void loadLoopById(uint16_t loopId);
    void cueLoopFromSpellbook(uint16_t loopId);
    void syncAudioLoop();
    void toggleLoopBars();
    void cycleLoopBarsAndPage();
    void cycleSwing();
    void cycleRootKey();
    void cycleScaleSmart();
    void rollBardicDice();
    void toggleMetronome();
    void cyclePerfMode();
    void cycleAudioOutMode();
    void cycleShoulderMode();
    void tapTempo();
    void adjustBpm(int delta);
    void exportCurrentLoopWav();

    // Song operations
    void syncSongToAudio();
    void saveCurrentSong();
    void renameCurrentSong();
    void createNewSong();
    void loadSongById(uint16_t songId);
    void deleteSongById(uint16_t songId);
    void addCurrentLoopToSong();
    void copyAndTweakSongSlot(int slotIdx);
    void editSongSlotLoop(int slotIdx);
    void cycleSongSlotLoop(int slotIdx, int dir);
    void toggleSongSlotTrackMute(int slotIdx, int trackIdx);
    void returnToSongFromEdit();
    void deleteSongSlot(int slotIdx);
    void exportCurrentSongWav();
    int  getTotalSongBars() const;
    int  getLoopNumBarsById(uint16_t loopId) const;
    const char* getLoopNameById(uint16_t loopId) const;

    // Vocal & Mix operations
    void loadActiveVocalForCurrentMode();
    void startVocalRecording(bool songScope);
    void beginVocalCaptureNow(bool songScope);
    void stopVocalRecording(bool commit);
    void pollVocalRecording();
    void undoVocalTake();
    void clearVocalTake(bool songScope);
    void toggleVocalMute(bool songScope);
    void toggleVocalHighPass();
    void toggleWifiMidi();

    // Live note input
    int  getEffectiveLiveOctaveOffset() const;
    float getEffectiveLiveBendSemis() const;
    void playLiveDegree(int degree, uint8_t velocity = 100);
    void releaseLiveNotes();

    // Hardware input
    void handleHardwareInput(u32 kDown, u32 kHeld, u32 kUp, const circlePosition& cpad);
};

} // namespace BardBeats
