#include "app_state.h"
#include "audio_engine.h"
#include "mic_recorder.h"
#include "wifi_midi.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace BardBeats {

void AppState::init() {
    storageLoadConfig(config);
    refreshSavedLoops();
    refreshSavedSongs();

    bool loopLoaded = false;
    if (config.lastLoopId > 0) {
        loopLoaded = storageLoadLoop(config.lastLoopId, currentLoop);
    }
    if (!loopLoaded && !savedLoops.empty()) {
        loopLoaded = storageLoadLoop(savedLoops[0].loopId, currentLoop);
    }
    if (!loopLoaded) {
        initStarterLoop(currentLoop, 0);
    }

    bool songLoaded = false;
    if (config.lastSongId > 0) {
        songLoaded = storageLoadSong(config.lastSongId, currentSong);
    }
    if (!songLoaded && !savedSongs.empty()) {
        songLoaded = storageLoadSong(savedSongs[0].songId, currentSong);
    }
    if (!songLoaded) {
        initStarterSong(currentSong, 0);
    }

    undoHead = 0;
    undoCount = 0;
    hasUndo = false;
    gridBarPage = 0;
    activeView = VIEW_GRID;
    activeTrack = TRACK_MELODY;
    octaveOffset = 0;
    isPlaying = false;
    isRecording = false;
    pendingCountInAction = 0;
    editingFromSong = false;
    showSongShelf = false;
    showMixDrawer = false;
    showInstPicker = false;
    showHelpOverlay = false;
    spellbookQuantizedCue = (config.spellbookQuantizedCue != 0);

    audioEngineSetSongMode(false);
    audioEnginePollHeadphones();
    syncMasterConfig();
    syncAudioLoop();
    syncSongToAudio();
    refreshSongSlotLoopCache();
    loadActiveVocalForCurrentMode();
}

void AppState::saveConfig() {
    config.lastLoopId = currentLoop.loopId;
    config.lastSongId = currentSong.songId;
    config.spellbookQuantizedCue = spellbookQuantizedCue ? 1 : 0;
    storageSaveConfig(config);
}

void AppState::syncMasterConfig() {
    audioEngineSetMasterConfig(config.masterVolume, config.audioOutMode, config.instTrimPct);
}

void AppState::refreshSavedLoops() {
    savedLoops = storageListLoops();
}

void AppState::refreshSavedSongs() {
    savedSongs = storageListSongs();
}

void AppState::refreshSongSlotLoopCache() {
    if (currentSong.numEntries > 0 && selectedSongSlot >= 0 && selectedSongSlot < currentSong.numEntries) {
        if (storageLoadLoop(currentSong.chain[selectedSongSlot].loopId, songSlotLoopCache)) {
            return;
        }
    }
    songSlotLoopCache = currentLoop;
}

void AppState::setToast(const char* msg, int frames) {
    std::snprintf(toastMsg, sizeof(toastMsg), "%s", msg);
    toastFrames = frames;
}

void AppState::switchView(ViewMode newView) {
    if (isRecordingVocal) {
        stopVocalRecording(true);
    }
    if (pendingCountInAction != 0) {
        pendingCountInAction = 0;
        audioEngineCancelCountIn();
    }
    showMixDrawer = false;
    showInstPicker = false;
    showHelpOverlay = false;
    if (activeView == newView) return;

    bool enteringSong = (newView == VIEW_SONG);
    bool leavingSong  = (activeView == VIEW_SONG);
    activeView = newView;

    if (enteringSong) {
        isRecording = false;
        editingFromSong = false;
        showSongShelf = false;
        syncSongToAudio();
        refreshSongSlotLoopCache();
        audioEngineSetSongMode(true);
        audioEngineResetPlayhead();
        loadActiveVocalForCurrentMode();
    } else if (leavingSong) {
        showSongShelf = false;
        audioEngineSetSongMode(false);
        audioEngineResetPlayhead();
        loadActiveVocalForCurrentMode();
    }
}

void AppState::snapshotForUndo(int trackIdx) {
    (void)trackIdx;
    undoRing[undoHead] = currentLoop;
    undoHead = (undoHead + 1) % BB_UNDO_LEVELS;
    if (undoCount < BB_UNDO_LEVELS) undoCount++;
    hasUndo = (undoCount > 0);
}

void AppState::performUndo() {
    if (activeView == VIEW_SONG && hasVocalUndo && undoVocalIsSong) {
        undoVocalTake();
        return;
    }
    if (undoCount <= 0) {
        if (hasVocalUndo && !undoVocalIsSong && undoVocalTargetId == currentLoop.loopId) {
            undoVocalTake();
            return;
        }
        setToast("Nothing to undo", 75);
        return;
    }
    undoHead = (undoHead + BB_UNDO_LEVELS - 1) % BB_UNDO_LEVELS;
    undoCount--;
    hasUndo = (undoCount > 0);
    currentLoop = undoRing[undoHead];
    syncAudioLoop();
    char buf[48];
    std::snprintf(buf, sizeof(buf), "Undid! (%d left)", undoCount);
    setToast(buf, 85);
}

void AppState::togglePlay() {
    if (pendingCountInAction != 0 || audioEngineIsCountingIn()) {
        pendingCountInAction = 0;
        audioEngineCancelCountIn();
        isRecording = false;
        setToast("Count-in cancelled", 70);
        return;
    }
    if (isRecordingVocal) {
        stopVocalRecording(true);
    }
    isPlaying = !isPlaying;
    if (isPlaying) {
        audioEngineResetPlayhead();
    } else {
        isRecording = false;
    }
    audioEngineSetPlaying(isPlaying);
}

void AppState::toggleRecord() {
    if (pendingCountInAction != 0 || audioEngineIsCountingIn()) {
        pendingCountInAction = 0;
        audioEngineCancelCountIn();
        setToast("Count-in cancelled", 70);
        return;
    }
    if (activeView == VIEW_SONG) {
        if (isRecordingVocal) {
            stopVocalRecording(true);
        } else {
            startVocalRecording(true);
        }
        return;
    }
    if (isRecording) {
        isRecording = false;
        setToast("JAM Mode: Live play only", 75);
        return;
    }
    snapshotForUndo(activeTrack);
    if (!isPlaying) {
        pendingCountInAction = 1;
        audioEngineResetPlayhead();
        audioEngineStartCountIn(4);
        setToast("Count-in: 4-3-2-1...", 120);
    } else {
        isRecording = true;
        setToast("REC ON: Play notes onto grid", 90);
    }
}

void AppState::syncAudioLoop() {
    audioEngineSetLoop(currentLoop);
}

void AppState::toggleLoopBars() {
    if (currentLoop.numBars == 1) {
        currentLoop.numBars = 2;
        // Copy bar 1 into bar 2 if bar 2 is empty so the groove continues immediately
        bool bar2Empty = true;
        for (int t = 0; t < BB_NUM_TRACKS && bar2Empty; ++t) {
            for (int s = 16; s < BB_MAX_STEPS; ++s) {
                if (stepHasAnyNote(currentLoop.tracks[t].steps[s])) {
                    bar2Empty = false;
                    break;
                }
            }
        }
        if (bar2Empty) {
            for (int t = 0; t < BB_NUM_TRACKS; ++t) {
                for (int s = 0; s < 16; ++s) {
                    currentLoop.tracks[t].steps[16 + s] = currentLoop.tracks[t].steps[s];
                }
            }
        }
        gridBarPage = 1;
        setToast("Loop: 2 Bars (32 steps)", 90);
    } else {
        currentLoop.numBars = 1;
        gridBarPage = 0;
        setToast("Loop: 1 Bar (16 steps)", 90);
    }
    syncAudioLoop();
}

void AppState::cycleLoopBarsAndPage() {
    if (currentLoop.numBars == 1) {
        toggleLoopBars();
        gridBarPage = 0;
        setToast("2 Bars: Viewing Bar 1 (1-16)", 80);
    } else if (gridBarPage == 0) {
        gridBarPage = 1;
        setToast("2 Bars: Viewing Bar 2 (17-32)", 80);
    } else {
        toggleLoopBars();
    }
}

void AppState::cycleSwing() {
    if (currentLoop.swingPercent < 54) {
        currentLoop.swingPercent = 58;
        setToast("Swing: 58% (Tavern Bounce)", 80);
    } else if (currentLoop.swingPercent < 62) {
        currentLoop.swingPercent = 64;
        setToast("Swing: 64% (Shuffle)", 80);
    } else {
        currentLoop.swingPercent = 50;
        setToast("Swing: 50% (Straight)", 80);
    }
    syncAudioLoop();
}

void AppState::cycleRootKey() {
    snapshotForUndo(activeTrack);
    transposeLoopRoot(currentLoop, 1);
    syncAudioLoop();
    char buf[48];
    std::snprintf(buf, sizeof(buf), "Key: %s", getRootNoteName(currentLoop.rootNote));
    setToast(buf, 80);
}

void AppState::cycleScaleSmart() {
    snapshotForUndo(activeTrack);
    uint8_t nextScale = (currentLoop.scaleId + 1) % SCALE_COUNT;
    remapLoopToScale(currentLoop, nextScale);
    syncAudioLoop();
    char buf[48];
    std::snprintf(buf, sizeof(buf), "Scale: %s (remapped)", getScaleName(currentLoop.scaleId));
    setToast(buf, 85);
}

void AppState::rollBardicDice() {
    snapshotForUndo(activeTrack);
    diceSeed ^= static_cast<uint32_t>(audioEngineGetCurrentStep() + 1) * 2654435761u;
    generateBardicDiceTrack(currentLoop, activeTrack, diceSeed);
    syncAudioLoop();
    char buf[48];
    std::snprintf(buf, sizeof(buf), "Bardic Dice: %s!", getTrackRoleName(activeTrack));
    setToast(buf, 85);
}

void AppState::toggleMetronome() {
    metronomeEnabled = !metronomeEnabled;
    audioEngineSetMetronome(metronomeEnabled);
    setToast(metronomeEnabled ? "Metronome: ON" : "Metronome: OFF", 75);
}

void AppState::cyclePerfMode() {
    perfMode = (perfMode + 1) % 3;
    arpHeldDegree = -1;
    lastArpStep = -1;
    arpCycleIdx = 0;
    if (perfMode == 0) setToast("Play Mode: NOTE", 70);
    else if (perfMode == 1) setToast("Play Mode: CHORD", 70);
    else setToast("Play Mode: ARP", 70);
}

void AppState::cycleAudioOutMode() {
    config.audioOutMode = (config.audioOutMode + 1) % AUDIO_OUT_COUNT;
    syncMasterConfig();
    saveConfig();
    char buf[64];
    if (config.audioOutMode == AUDIO_OUT_AUTO) {
        std::snprintf(buf, sizeof(buf), "Audio Out: AUTO (%s)",
                      audioEngineIsSpeakerVoicingActive() ? "3DS Speaker" : "Headphones");
    } else if (config.audioOutMode == AUDIO_OUT_SPEAKER) {
        std::snprintf(buf, sizeof(buf), "Audio Out: 3DS Speaker EQ");
    } else {
        std::snprintf(buf, sizeof(buf), "Audio Out: Headphones (Full Bass)");
    }
    setToast(buf, 95);
}

void AppState::cycleShoulderMode() {
    config.shoulderMode = (config.shoulderMode + 1) % SHOULDER_MODE_COUNT;
    saveConfig();
    if (config.shoulderMode == SHOULDER_OCTAVE_HOLD) {
        setToast("L/R: Momentary Octave Hold", 95);
    } else if (config.shoulderMode == SHOULDER_SEMITONE) {
        setToast("L/R: Flat (-1st) / Sharp (+1st)", 95);
    } else {
        setToast("L/R: Sustain Latch + 16th Roll", 95);
    }
}

void AppState::tapTempo() {
    uint64_t now = osGetTime();
    if (tapCount > 0 && (now - tapTimes[tapCount - 1]) > 2200) {
        tapCount = 0;
    }
    if (tapCount < 4) {
        tapTimes[tapCount++] = now;
    } else {
        tapTimes[0] = tapTimes[1];
        tapTimes[1] = tapTimes[2];
        tapTimes[2] = tapTimes[3];
        tapTimes[3] = now;
    }
    if (tapCount >= 2) {
        uint64_t span = tapTimes[tapCount - 1] - tapTimes[0];
        if (span > 150) {
            double avgMs = static_cast<double>(span) / static_cast<double>(tapCount - 1);
            int bpm = static_cast<int>(std::lround(60000.0 / avgMs));
            if (bpm < 60) bpm = 60;
            if (bpm > 180) bpm = 180;
            if (activeView == VIEW_SONG) {
                currentSong.bpm = static_cast<uint16_t>(bpm);
                syncSongToAudio();
            } else {
                currentLoop.bpm = static_cast<uint16_t>(bpm);
                syncAudioLoop();
            }
            char buf[40];
            std::snprintf(buf, sizeof(buf), "Tap Tempo: %d BPM", bpm);
            setToast(buf, 70);
        }
    } else {
        setToast("Tap Tempo: keep tapping...", 60);
    }
}

void AppState::adjustBpm(int delta) {
    if (activeView == VIEW_SONG) {
        int v = static_cast<int>(currentSong.bpm) + delta;
        if (v < 60) v = 60;
        if (v > 180) v = 180;
        currentSong.bpm = static_cast<uint16_t>(v);
        syncSongToAudio();
    } else {
        int v = static_cast<int>(currentLoop.bpm) + delta;
        if (v < 60) v = 60;
        if (v > 180) v = 180;
        currentLoop.bpm = static_cast<uint16_t>(v);
        syncAudioLoop();
    }
}

void AppState::saveCurrentLoop() {
    if (storageSaveLoop(currentLoop)) {
        refreshSavedLoops();
        saveConfig();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Saved: %s", currentLoop.name);
        setToast(buf, 110);
    } else {
        setToast("SD Save Failed!", 120);
    }
}

void AppState::copyCurrentLoop() {
    uint16_t newId = storageGetNextLoopId();
    currentLoop.loopId = newId;
    char newName[32];
    std::snprintf(newName, sizeof(newName), "Loop %u", newId);
    std::snprintf(currentLoop.name, sizeof(currentLoop.name), "%s", newName);
    currentLoop.hasVocalClip = 0;
    if (storageSaveLoop(currentLoop)) {
        refreshSavedLoops();
        syncAudioLoop();
        saveConfig();
        loadActiveVocalForCurrentMode();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Copied to %s", currentLoop.name);
        setToast(buf, 110);
    }
}

void AppState::renameCurrentLoop() {
    SwkbdState swkbd;
    char buf[32] = {0};
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 31);
    swkbdSetInitialText(&swkbd, currentLoop.name);
    swkbdSetHintText(&swkbd, "Name your loop (e.g. Fireball)");
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    SwkbdButton btn = swkbdInputText(&swkbd, buf, sizeof(buf));
    if (btn == SWKBD_BUTTON_CONFIRM && buf[0] != '\0') {
        std::snprintf(currentLoop.name, sizeof(currentLoop.name), "%s", buf);
        saveCurrentLoop();
    }
}

void AppState::loadLoopById(uint16_t loopId) {
    LoopFile loaded;
    if (storageLoadLoop(loopId, loaded)) {
        currentLoop = loaded;
        undoCount = 0;
        hasUndo = false;
        gridBarPage = 0;
        syncAudioLoop();
        saveConfig();
        if (activeView != VIEW_SONG) {
            loadActiveVocalForCurrentMode();
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Loaded: %s", currentLoop.name);
        setToast(buf, 90);
    }
}

void AppState::cueLoopFromSpellbook(uint16_t loopId) {
    LoopFile loaded;
    if (!storageLoadLoop(loopId, loaded)) return;

    if (spellbookQuantizedCue && isPlaying) {
        queuedSpellbookLoop = loaded;
        hasQueuedSpellbookLoop = true;
        audioEngineQueueNextBarLoop(loaded);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Queued next bar: %s", loaded.name);
        setToast(buf, 90);
    } else {
        hasQueuedSpellbookLoop = false;
        currentLoop = loaded;
        undoCount = 0;
        hasUndo = false;
        gridBarPage = 0;
        syncAudioLoop();
        saveConfig();
        loadActiveVocalForCurrentMode();
        if (!isPlaying) {
            isPlaying = true;
            audioEngineResetPlayhead();
            audioEngineSetPlaying(true);
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Playing: %s", currentLoop.name);
        setToast(buf, 90);
    }
}

void AppState::exportCurrentLoopWav() {
    setToast("Rendering .WAV to SD...", 60);
    char path[256];
    if (storageExportLoopWav(currentLoop, activeVocalSamples, path, sizeof(path), &config)) {
        setToast("Exported .wav to exports/", 120);
    } else {
        setToast("WAV Export Failed!", 120);
    }
}

// ---- Song Mode Operations ----

void AppState::syncSongToAudio() {
    std::vector<LoopFile> resolved;
    resolved.reserve(currentSong.numEntries);
    for (int i = 0; i < currentSong.numEntries; ++i) {
        LoopFile lf;
        if (!storageLoadLoop(currentSong.chain[i].loopId, lf)) {
            initEmptyLoop(lf, currentSong.chain[i].loopId, "Missing", currentSong.bpm);
        }
        resolved.push_back(lf);
    }
    audioEngineSetSongArrangement(currentSong, resolved);
}

void AppState::saveCurrentSong() {
    if (storageSaveSong(currentSong)) {
        refreshSavedSongs();
        syncSongToAudio();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Saved Song: %s", currentSong.name);
        setToast(buf, 110);
    } else {
        setToast("Song Save Failed!", 120);
    }
}

void AppState::renameCurrentSong() {
    SwkbdState swkbd;
    char buf[32] = {0};
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 31);
    swkbdSetInitialText(&swkbd, currentSong.name);
    swkbdSetHintText(&swkbd, "Name your song");
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    SwkbdButton btn = swkbdInputText(&swkbd, buf, sizeof(buf));
    if (btn == SWKBD_BUTTON_CONFIRM && buf[0] != '\0') {
        std::snprintf(currentSong.name, sizeof(currentSong.name), "%s", buf);
        saveCurrentSong();
    }
}

void AppState::createNewSong() {
    uint16_t newId = storageGetNextSongId();
    char newName[32];
    std::snprintf(newName, sizeof(newName), "Bard Song %u", newId);
    initEmptySong(currentSong, newId, newName, 112);
    // Start with the current active loop as slot 1
    storageSaveLoop(currentLoop);
    refreshSavedLoops();
    currentSong.numEntries = 1;
    currentSong.chain[0] = { currentLoop.loopId, 2, SEC_INTRO, 0, {0, 0, 0} };
    selectedSongSlot = 0;
    showSongShelf = false;
    saveCurrentSong();
    loadActiveVocalForCurrentMode();
}

void AppState::loadSongById(uint16_t songId) {
    SongFile loaded;
    if (storageLoadSong(songId, loaded)) {
        currentSong = loaded;
        selectedSongSlot = 0;
        showSongShelf = false;
        syncSongToAudio();
        refreshSongSlotLoopCache();
        saveConfig();
        audioEngineResetPlayhead();
        loadActiveVocalForCurrentMode();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Loaded Song: %s", currentSong.name);
        setToast(buf, 90);
    }
}

void AppState::deleteSongById(uint16_t songId) {
    if (savedSongs.size() <= 1) {
        setToast("Keep at least 1 song", 90);
        return;
    }
    storageDeleteSong(songId);
    refreshSavedSongs();
    if (!savedSongs.empty()) {
        loadSongById(savedSongs[0].songId);
    }
    setToast("Deleted song", 90);
}

void AppState::addCurrentLoopToSong() {
    if (currentSong.numEntries >= BB_MAX_SONG_CLIPS) {
        setToast("Song full (32 clips max)", 90);
        return;
    }
    storageSaveLoop(currentLoop);
    refreshSavedLoops();

    int idx = currentSong.numEntries;
    currentSong.chain[idx].loopId = currentLoop.loopId;
    currentSong.chain[idx].repeatCount = 2;
    currentSong.chain[idx].sectionTag = (idx == 0) ? SEC_INTRO : SEC_VERSE;
    currentSong.chain[idx].muteMask = 0;
    currentSong.numEntries++;
    selectedSongSlot = idx;
    saveCurrentSong();
    refreshSongSlotLoopCache();
}

void AppState::copyAndTweakSongSlot(int slotIdx) {
    if (slotIdx < 0 || slotIdx >= currentSong.numEntries) return;
    if (currentSong.numEntries >= BB_MAX_SONG_CLIPS) {
        setToast("Song full (32 clips max)", 90);
        return;
    }

    LoopFile src;
    if (!storageLoadLoop(currentSong.chain[slotIdx].loopId, src)) {
        setToast("Could not load clip to copy", 90);
        return;
    }

    uint16_t newId = storageGetNextLoopId();
    src.loopId = newId;
    src.bpm = currentSong.bpm;
    src.hasVocalClip = 0;
    char baseName[24];
    std::snprintf(baseName, sizeof(baseName), "%.22s", src.name);
    std::snprintf(src.name, sizeof(src.name), "%s B", baseName);
    storageSaveLoop(src);
    refreshSavedLoops();

    int insertPos = slotIdx + 1;
    for (int i = currentSong.numEntries; i > insertPos; --i) {
        currentSong.chain[i] = currentSong.chain[i - 1];
    }
    currentSong.chain[insertPos] = currentSong.chain[slotIdx];
    currentSong.chain[insertPos].loopId = newId;
    currentSong.numEntries++;
    selectedSongSlot = insertPos;
    saveCurrentSong();

    editSongSlotLoop(insertPos);
}

void AppState::editSongSlotLoop(int slotIdx) {
    if (slotIdx < 0 || slotIdx >= currentSong.numEntries) return;
    if (isRecordingVocal) stopVocalRecording(true);

    uint16_t targetLoopId = currentSong.chain[slotIdx].loopId;
    LoopFile loaded;
    if (!storageLoadLoop(targetLoopId, loaded)) {
        setToast("Loop file missing!", 90);
        return;
    }
    currentLoop = loaded;
    currentLoop.bpm = currentSong.bpm;
    undoCount = 0;
    hasUndo = false;
    gridBarPage = 0;
    editingFromSong = true;
    returnSongSlot = slotIdx;

    audioEngineSetSongMode(false);
    audioEngineResetPlayhead();
    syncAudioLoop();
    activeView = VIEW_GRID;
    loadActiveVocalForCurrentMode();

    char buf[64];
    std::snprintf(buf, sizeof(buf), "Editing: %s (Tap < SONG)", currentLoop.name);
    setToast(buf, 120);
}

void AppState::returnToSongFromEdit() {
    if (!editingFromSong) return;
    if (isRecordingVocal) stopVocalRecording(true);

    storageSaveLoop(currentLoop);
    refreshSavedLoops();
    editingFromSong = false;
    selectedSongSlot = returnSongSlot;

    syncSongToAudio();
    refreshSongSlotLoopCache();
    audioEngineSetSongMode(true);
    audioEngineResetPlayhead();
    activeView = VIEW_SONG;
    loadActiveVocalForCurrentMode();
    setToast("Returned to Song (Loop updated live)", 100);
}

void AppState::deleteSongSlot(int slotIdx) {
    if (slotIdx < 0 || slotIdx >= currentSong.numEntries) return;
    if (currentSong.numEntries <= 1) {
        setToast("Song needs at least 1 clip", 90);
        return;
    }
    for (int i = slotIdx; i < currentSong.numEntries - 1; ++i) {
        currentSong.chain[i] = currentSong.chain[i + 1];
    }
    currentSong.numEntries--;
    if (selectedSongSlot >= currentSong.numEntries) {
        selectedSongSlot = currentSong.numEntries - 1;
    }
    saveCurrentSong();
    refreshSongSlotLoopCache();
}

void AppState::exportCurrentSongWav() {
    setToast("Rendering Song .WAV to SD...", 60);
    char path[256];
    if (storageExportSongWav(currentSong, activeVocalSamples, path, sizeof(path), &config)) {
        setToast("Exported Song .wav to exports/", 120);
    } else {
        setToast("Song WAV Export Failed!", 120);
    }
}

void AppState::cycleSongSlotLoop(int slotIdx, int dir) {
    if (slotIdx < 0 || slotIdx >= currentSong.numEntries || savedLoops.empty()) return;
    uint16_t curId = currentSong.chain[slotIdx].loopId;
    int foundIdx = 0;
    for (int i = 0; i < static_cast<int>(savedLoops.size()); ++i) {
        if (savedLoops[i].loopId == curId) {
            foundIdx = i;
            break;
        }
    }
    int count = static_cast<int>(savedLoops.size());
    int nextIdx = (foundIdx + dir) % count;
    if (nextIdx < 0) nextIdx += count;
    currentSong.chain[slotIdx].loopId = savedLoops[nextIdx].loopId;
    saveCurrentSong();
    refreshSongSlotLoopCache();
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Slot %d -> %s", slotIdx + 1, savedLoops[nextIdx].name);
    setToast(buf, 85);
}

void AppState::toggleSongSlotTrackMute(int slotIdx, int trackIdx) {
    if (slotIdx < 0 || slotIdx >= currentSong.numEntries) return;
    if (trackIdx < 0 || trackIdx >= BB_NUM_TRACKS) return;
    currentSong.chain[slotIdx].muteMask ^= static_cast<uint8_t>(1u << trackIdx);
    syncSongToAudio();
    bool muted = (currentSong.chain[slotIdx].muteMask & (1u << trackIdx)) != 0;
    char buf[56];
    std::snprintf(buf, sizeof(buf), "Slot %d %s: %s", slotIdx + 1, getTrackRoleName(trackIdx), muted ? "MUTED" : "ON");
    setToast(buf, 75);
}

int AppState::getLoopNumBarsById(uint16_t loopId) const {
    for (const auto& s : savedLoops) {
        if (s.loopId == loopId) return (s.numBars == 2) ? 2 : 1;
    }
    return 1;
}

int AppState::getTotalSongBars() const {
    int total = 0;
    for (int i = 0; i < currentSong.numEntries; ++i) {
        int reps = (currentSong.chain[i].repeatCount > 0 ? currentSong.chain[i].repeatCount : 1);
        int bars = getLoopNumBarsById(currentSong.chain[i].loopId);
        total += reps * bars;
    }
    return total;
}

const char* AppState::getLoopNameById(uint16_t loopId) const {
    for (const auto& s : savedLoops) {
        if (s.loopId == loopId) return s.name;
    }
    return "Loop";
}

// ---- Vocal Layer & Mix Operations ----

void AppState::loadActiveVocalForCurrentMode() {
    bool isSong = (activeView == VIEW_SONG);
    char path[256];
    bool hasVoc = false;
    uint8_t vol = 95;
    bool muted = false;
    bool hpf = (currentLoop.vocalHighPass != 0);

    if (isSong) {
        storageGetSongVocalPath(currentSong.songId, path, sizeof(path));
        hasVoc = (currentSong.hasSongVocal != 0);
        vol = currentSong.songVocalVolume;
        muted = (currentSong.songVocalMuted != 0);
    } else {
        storageGetLoopVocalPath(currentLoop.loopId, path, sizeof(path));
        hasVoc = (currentLoop.hasVocalClip != 0);
        vol = currentLoop.vocalVolume;
        muted = (currentLoop.vocalMuted != 0);
    }

    activeVocalSamples.clear();
    if (hasVoc && storageLoadVocalWav(path, activeVocalSamples) && !activeVocalSamples.empty()) {
        micComputeWaveformBars(activeVocalSamples, vocalWaveBars, BB_WAVE_BARS);
        audioEngineSetVocalLayer(activeVocalSamples.data(),
                                 static_cast<uint32_t>(activeVocalSamples.size()),
                                 !isSong, vol, muted, hpf);
    } else {
        std::memset(vocalWaveBars, 0, sizeof(vocalWaveBars));
        audioEngineClearVocalLayer();
    }
}

void AppState::startVocalRecording(bool songScope) {
    if (!micRecorderIsAvailable()) {
        setToast("3DS Mic unavailable!", 110);
        return;
    }
    if (isPlaying) {
        isPlaying = false;
        audioEngineSetPlaying(false);
    }
    isRecording = false;
    audioEngineSetSongMode(songScope);
    audioEngineResetPlayhead();
    pendingCountInAction = songScope ? 3 : 2;
    audioEngineStartCountIn(4);
    setToast("Vocal Count-In: 4-3-2-1...", 120);
}

void AppState::beginVocalCaptureNow(bool songScope) {
    if (!micRecorderIsAvailable()) {
        setToast("3DS Mic unavailable!", 110);
        return;
    }

    undoVocalSamples = activeVocalSamples;
    undoVocalIsSong = songScope;
    undoVocalTargetId = songScope ? currentSong.songId : currentLoop.loopId;
    hasVocalUndo = true;

    audioEngineClearVocalLayer();

    uint16_t bpm = songScope ? currentSong.bpm : currentLoop.bpm;
    if (bpm < 60 || bpm > 180) bpm = 110;
    double samplesPerBar = (static_cast<double>(BB_MIC_SAMPLE_RATE) * 60.0 * 4.0) / static_cast<double>(bpm);
    int bars = songScope ? std::max(1, getTotalSongBars()) : (currentLoop.numBars == 2 ? 2 : 1);
    uint32_t targetSamples = static_cast<uint32_t>(std::round(samplesPerBar * bars));

    if (!micRecorderStart(targetSamples)) {
        setToast("Mic start failed!", 110);
        loadActiveVocalForCurrentMode();
        return;
    }

    isRecordingVocal = true;
    vocalRecIsSongScope = songScope;
    isRecording = false;

    audioEngineSetSongMode(songScope);
    audioEngineResetPlayhead();
    isPlaying = true;
    audioEngineSetPlaying(true);

    setToast(songScope ? "SINGING (SONG): Tap REC/STOP to finish"
                       : "SINGING (CLIP): Auto-stops at loop end", 120);
}

void AppState::stopVocalRecording(bool commit) {
    if (!isRecordingVocal) return;
    isRecordingVocal = false;

    std::vector<int16_t> newTake;
    micRecorderStop(commit, newTake);

    if (commit && !newTake.empty()) {
        char path[256];
        if (vocalRecIsSongScope) {
            storageGetSongVocalPath(currentSong.songId, path, sizeof(path));
            if (storageSaveVocalWav(path, newTake.data(), static_cast<uint32_t>(newTake.size()), BB_MIC_SAMPLE_RATE)) {
                currentSong.hasSongVocal = 1;
                currentSong.songVocalMuted = 0;
                storageSaveSong(currentSong);
                setToast("Song vocal saved! (UNDO to revert)", 120);
            }
        } else {
            storageGetLoopVocalPath(currentLoop.loopId, path, sizeof(path));
            if (storageSaveVocalWav(path, newTake.data(), static_cast<uint32_t>(newTake.size()), BB_MIC_SAMPLE_RATE)) {
                currentLoop.hasVocalClip = 1;
                currentLoop.vocalMuted = 0;
                storageSaveLoop(currentLoop);
                refreshSavedLoops();
                setToast("Clip vocal saved! (UNDO to revert)", 120);
            }
        }
    } else {
        setToast("Vocal take cancelled", 80);
    }

    loadActiveVocalForCurrentMode();
}

void AppState::pollVocalRecording() {
    if (!isRecordingVocal) return;
    if (micRecorderPoll()) {
        stopVocalRecording(true);
    }
}

void AppState::undoVocalTake() {
    if (!hasVocalUndo) {
        setToast("No vocal take to undo", 80);
        return;
    }

    char path[256];
    if (undoVocalIsSong) {
        storageGetSongVocalPath(undoVocalTargetId, path, sizeof(path));
        if (undoVocalSamples.empty()) {
            storageDeleteFile(path);
            currentSong.hasSongVocal = 0;
        } else {
            storageSaveVocalWav(path, undoVocalSamples.data(),
                                static_cast<uint32_t>(undoVocalSamples.size()), BB_MIC_SAMPLE_RATE);
            currentSong.hasSongVocal = 1;
        }
        storageSaveSong(currentSong);
    } else {
        storageGetLoopVocalPath(undoVocalTargetId, path, sizeof(path));
        if (undoVocalSamples.empty()) {
            storageDeleteFile(path);
            currentLoop.hasVocalClip = 0;
        } else {
            storageSaveVocalWav(path, undoVocalSamples.data(),
                                static_cast<uint32_t>(undoVocalSamples.size()), BB_MIC_SAMPLE_RATE);
            currentLoop.hasVocalClip = 1;
        }
        storageSaveLoop(currentLoop);
        refreshSavedLoops();
    }

    hasVocalUndo = false;
    loadActiveVocalForCurrentMode();
    setToast("Reverted vocal take!", 100);
}

void AppState::clearVocalTake(bool songScope) {
    undoVocalSamples = activeVocalSamples;
    undoVocalIsSong = songScope;
    undoVocalTargetId = songScope ? currentSong.songId : currentLoop.loopId;
    hasVocalUndo = !activeVocalSamples.empty();

    char path[256];
    if (songScope) {
        storageGetSongVocalPath(currentSong.songId, path, sizeof(path));
        storageDeleteFile(path);
        currentSong.hasSongVocal = 0;
        storageSaveSong(currentSong);
    } else {
        storageGetLoopVocalPath(currentLoop.loopId, path, sizeof(path));
        storageDeleteFile(path);
        currentLoop.hasVocalClip = 0;
        storageSaveLoop(currentLoop);
        refreshSavedLoops();
    }
    loadActiveVocalForCurrentMode();
    setToast("Vocal cleared (Loop untouched)", 100);
}

void AppState::toggleVocalMute(bool songScope) {
    if (songScope) {
        currentSong.songVocalMuted ^= 1;
        audioEngineSetVocalMix(currentSong.songVocalVolume, currentSong.songVocalMuted != 0);
        setToast(currentSong.songVocalMuted ? "Song vocal muted" : "Song vocal unmuted", 70);
    } else {
        currentLoop.vocalMuted ^= 1;
        audioEngineSetVocalMix(currentLoop.vocalVolume, currentLoop.vocalMuted != 0);
        setToast(currentLoop.vocalMuted ? "Clip vocal muted" : "Clip vocal unmuted", 70);
    }
}

void AppState::toggleVocalHighPass() {
    currentLoop.vocalHighPass ^= 1;
    loadActiveVocalForCurrentMode();
    setToast(currentLoop.vocalHighPass ? "Vocal HPF: ON (Clear)" : "Vocal HPF: OFF (Warm)", 80);
}

void AppState::toggleWifiMidi() {
    if (wifiMidiEnabled) {
        wifiMidiExit();
        wifiMidiEnabled = false;
        setToast("Wi-Fi MIDI: OFF", 80);
    } else {
        if (wifiMidiInit()) {
            wifiMidiEnabled = true;
            setToast("Wi-Fi MIDI: ON (UDP 5004)", 100);
        } else {
            setToast("Wi-Fi MIDI failed (Check Wi-Fi)", 110);
        }
    }
}

int AppState::getEffectiveLiveOctaveOffset() const {
    int oct = (activeTrack == TRACK_BASS ? octaveOffset - 1 : octaveOffset);
    if (config.shoulderMode == SHOULDER_OCTAVE_HOLD) {
        if (lHeld && rHeld) oct += 2;
        else if (lHeld) oct -= 1;
        else if (rHeld) oct += 1;
    }
    return oct;
}

float AppState::getEffectiveLiveBendSemis() const {
    float bend = circleBendSemis;
    if (config.shoulderMode == SHOULDER_SEMITONE && activeTrack != TRACK_DRUMS) {
        if (lHeld && !rHeld) bend -= 1.0f;
        else if (rHeld && !lHeld) bend += 1.0f;
    }
    return bend;
}

void AppState::playLiveDegree(int degree, uint8_t velocity) {
    if (degree < 0 || degree >= BB_GRID_ROWS) return;

    int oct = getEffectiveLiveOctaveOffset();
    uint8_t instId = currentLoop.tracks[activeTrack].instrumentId;

    int noteCount = 1;
    uint8_t notes[3] = {0, 0, 0};
    if (activeTrack == TRACK_DRUMS) {
        notes[0] = getDrumPieceNote(degree);
    } else if (perfMode == 1) {
        // Chord Strum: 3-note scale triad (now properly octave-wrapped!)
        noteCount = 3;
        notes[0] = getScaleNote(currentLoop.scaleId, currentLoop.rootNote, oct, degree);
        notes[1] = getScaleNote(currentLoop.scaleId, currentLoop.rootNote, oct, degree + 2);
        notes[2] = getScaleNote(currentLoop.scaleId, currentLoop.rootNote, oct, degree + 4);
    } else {
        notes[0] = getScaleNote(currentLoop.scaleId, currentLoop.rootNote, oct, degree);
        if (perfMode == 2) {
            arpHeldDegree = degree;
            lastArpStep = audioEngineGetCurrentStep();
            arpCycleIdx = 0;
        }
    }

    rollHeldDegree = degree;
    lastRollStep = audioEngineGetCurrentStep();

    for (int i = 0; i < noteCount; ++i) {
        audioEngineTriggerLiveNote(activeTrack, instId, notes[i], velocity);
        if (wifiMidiEnabled) {
            uint8_t ch = (activeTrack == TRACK_DRUMS) ? 9 : static_cast<uint8_t>(activeTrack);
            wifiMidiSendNoteOn(ch, notes[i], velocity);
        }
    }

    lastLiveNote = notes[0];
    lastLiveDegree = degree;
    liveFlashFrames = 14;

    if (isRecording && isPlaying && activeView != VIEW_SONG) {
        int totalSteps = (currentLoop.numBars == 2) ? BB_MAX_STEPS : BB_STEPS_PER_LOOP;
        int step = audioEngineGetCurrentStep();
        float frac = audioEngineGetStepFraction();
        int qStep = (frac >= 0.55f) ? ((step + 1) % totalSteps) : step;
        float effBend = getEffectiveLiveBendSemis();
        int8_t bendQ4 = static_cast<int8_t>(std::lround(effBend * 16.0f));
        if (bendQ4 > 32) bendQ4 = 32;
        if (bendQ4 < -32) bendQ4 = -32;
        for (int i = 0; i < noteCount; ++i) {
            addStepNote(currentLoop.tracks[activeTrack].steps[qStep], notes[i], velocity, bendQ4);
        }
        recHeldStep = qStep;
        recHeldNote = notes[0];
        syncAudioLoop();
    }
}

void AppState::releaseLiveNotes() {
    // In Sustain+Roll mode, holding L latches active notes until L is released
    if (config.shoulderMode == SHOULDER_SUS_ROLL && lHeld) {
        rollHeldDegree = -1;
        return;
    }
    audioEngineReleaseLiveNotes(activeTrack);
    if (wifiMidiEnabled && lastLiveNote > 0) {
        uint8_t ch = (activeTrack == TRACK_DRUMS) ? 9 : static_cast<uint8_t>(activeTrack);
        wifiMidiSendNoteOff(ch, lastLiveNote);
    }
    arpHeldDegree = -1;
    lastArpStep = -1;
    arpCycleIdx = 0;
    rollHeldDegree = -1;
    lastRollStep = -1;
    recHeldStep = -1;
    recHeldNote = 0;
}

void AppState::handleHardwareInput(u32 kDown, u32 kHeld, u32 kUp, const circlePosition& cpad) {
    if (toastFrames > 0) toastFrames--;
    if (liveFlashFrames > 0) liveFlashFrames--;

    // Periodic 3DS Headphone Jack Auto-Detection (~every 0.5s)
    if (++hpPollCounter >= 30) {
        hpPollCounter = 0;
        bool prevSpk = audioEngineIsSpeakerVoicingActive();
        audioEnginePollHeadphones();
        bool curSpk = audioEngineIsSpeakerVoicingActive();
        if (config.audioOutMode == AUDIO_OUT_AUTO && prevSpk != curSpk) {
            setToast(curSpk ? "Speakers detected (EQ ON)" : "Headphones detected (Full Bass)", 90);
        }
    }

    // Complete 4-beat Count-In on exact downbeat
    if (audioEngineConsumeCountInFinished()) {
        int act = pendingCountInAction;
        pendingCountInAction = 0;
        if (act == 1) {
            isRecording = true;
            isPlaying = true;
            audioEngineResetPlayhead();
            audioEngineSetPlaying(true);
            setToast("REC ON: Play notes onto grid!", 90);
        } else if (act == 2) {
            beginVocalCaptureNow(false);
        } else if (act == 3) {
            beginVocalCaptureNow(true);
        }
    }

    pollVocalRecording();

    // Promote quantized Spellbook cue once audio engine swaps at step 0 (#11)
    if (hasQueuedSpellbookLoop && !audioEngineHasQueuedLoop()) {
        currentLoop = queuedSpellbookLoop;
        hasQueuedSpellbookLoop = false;
        undoCount = 0;
        hasUndo = false;
        gridBarPage = 0;
        saveConfig();
        loadActiveVocalForCurrentMode();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Playing: %s", currentLoop.name);
        setToast(buf, 75);
    }

    bool prevL = lHeld;
    lHeld = (kHeld & KEY_L) != 0;
    rHeld = (kHeld & KEY_R) != 0;

    // Release latched sustain when L is let go in Sustain+Roll mode
    if (config.shoulderMode == SHOULDER_SUS_ROLL && prevL && !lHeld) {
        releaseLiveNotes();
    }

    // Ignore START when holding L+R exit combo
    if ((kDown & KEY_START) && !(lHeld && rHeld)) {
        togglePlay();
    }

    // SELECT: quick tap toggles Spellbook; hold > 25 frames toggles Controls Cheat-Sheet
    if (kHeld & KEY_SELECT) {
        selectHoldFrames++;
        if (selectHoldFrames == 25 && !selectConsumedByHelp) {
            selectConsumedByHelp = true;
            showHelpOverlay = !showHelpOverlay;
            showMixDrawer = false;
            showInstPicker = false;
        }
    }
    if (kUp & KEY_SELECT) {
        if (!selectConsumedByHelp) {
            if (showHelpOverlay) {
                showHelpOverlay = false;
            } else {
                switchView((activeView == VIEW_SPELLBOOK) ? VIEW_GRID : VIEW_SPELLBOOK);
            }
        }
        selectHoldFrames = 0;
        selectConsumedByHelp = false;
    }

    constexpr int DEADZONE = 20;
    float nx = 0.0f;
    if (cpad.dx > DEADZONE)       nx = static_cast<float>(cpad.dx - DEADZONE) / 136.0f;
    else if (cpad.dx < -DEADZONE) nx = static_cast<float>(cpad.dx + DEADZONE) / 136.0f;
    if (nx > 1.0f) nx = 1.0f;
    if (nx < -1.0f) nx = -1.0f;
    circleBendSemis = (0.4f * nx + 0.6f * nx * nx * nx) * 2.0f;

    float ny = 0.0f;
    if (cpad.dy > DEADZONE) ny = static_cast<float>(cpad.dy - DEADZONE) / 136.0f;
    if (ny > 1.0f) ny = 1.0f;
    circleVibrato = ny;

    float totalLiveBend = getEffectiveLiveBendSemis();
    audioEngineSetCirclePad(totalLiveBend, circleVibrato);
    if (wifiMidiEnabled && (std::fabs(totalLiveBend) > 0.02f)) {
        uint8_t ch = (activeTrack == TRACK_DRUMS) ? 9 : static_cast<uint8_t>(activeTrack);
        wifiMidiSendPitchBend(ch, totalLiveBend);
    }

    // Symmetric clockwise from 9 o'clock: D-Pad (Left, Up, Right, Down) + Face (Y, X, A, B) (#24)
    static const u32 NOTE_KEYS[8] = {
        KEY_DLEFT, KEY_DUP, KEY_DRIGHT, KEY_DDOWN,
        KEY_Y,     KEY_X,   KEY_A,      KEY_B
    };
    static constexpr u32 ALL_NOTE_MASK =
        KEY_DLEFT | KEY_DUP | KEY_DRIGHT | KEY_DDOWN | KEY_Y | KEY_X | KEY_A | KEY_B;

    if (!showHelpOverlay && (activeView == VIEW_PADS || activeView == VIEW_INSTRUMENT || activeView == VIEW_GRID)) {
        uint8_t vel = (config.shoulderMode == SHOULDER_SUS_ROLL && rHeld) ? 124 : 105;
        for (int deg = 0; deg < 8; ++deg) {
            if (kDown & NOTE_KEYS[deg]) {
                playLiveDegree(deg, vel);
            }
        }
        if ((kUp & ALL_NOTE_MASK) && !(kHeld & ALL_NOTE_MASK)) {
            releaseLiveNotes();
        }
    }

    // R-Hold 16th-Note Accent Roll in Sustain+Roll mode (great for drum rolls & lute tremolos)
    if (config.shoulderMode == SHOULDER_SUS_ROLL && rHeld && rollHeldDegree >= 0 && isPlaying) {
        int curStep = audioEngineGetCurrentStep();
        if (curStep != lastRollStep) {
            lastRollStep = curStep;
            playLiveDegree(rollHeldDegree, 122);
        }
    }

    // Arpeggiator stepping while note is held (#12)
    if (arpHeldDegree >= 0 && activeTrack != TRACK_DRUMS && perfMode == 2 && isPlaying) {
        int curStep = audioEngineGetCurrentStep();
        if (curStep != lastArpStep) {
            lastArpStep = curStep;
            static const int kArpOffsets[4] = { 0, 2, 4, 7 };
            arpCycleIdx = (arpCycleIdx + 1) & 3;
            int oct = getEffectiveLiveOctaveOffset();
            uint8_t note = getScaleNote(currentLoop.scaleId, currentLoop.rootNote, oct,
                                        arpHeldDegree + kArpOffsets[arpCycleIdx]);
            uint8_t instId = currentLoop.tracks[activeTrack].instrumentId;
            audioEngineReleaseLiveNotes(activeTrack);
            audioEngineTriggerLiveNote(activeTrack, instId, note, 98);
            lastLiveNote = note;
            liveFlashFrames = 8;
            if (isRecording && activeView != VIEW_SONG) {
                addStepNote(currentLoop.tracks[activeTrack].steps[curStep], note, 98, 0);
                syncAudioLoop();
            }
        }
    }

    // Live hold-sustain gateLength recording (#13)
    if (recHeldStep >= 0 && isRecording && isPlaying && activeView != VIEW_SONG && perfMode != 2) {
        int totalSteps = (currentLoop.numBars == 2) ? BB_MAX_STEPS : BB_STEPS_PER_LOOP;
        int curStep = audioEngineGetCurrentStep();
        int heldSteps = (curStep - recHeldStep + totalSteps) % totalSteps + 1;
        if (heldSteps > 16) heldSteps = 16;
        StepCell& cell = currentLoop.tracks[activeTrack].steps[recHeldStep];
        if (heldSteps > cell.gateLength && stepHasNote(cell, recHeldNote)) {
            cell.gateLength = static_cast<uint8_t>(heldSteps);
            syncAudioLoop();
        }
    }
}

} // namespace BardBeats
