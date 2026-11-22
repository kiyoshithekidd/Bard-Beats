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
    refreshSavedLoops();
    refreshSavedSongs();

    if (!savedLoops.empty()) {
        if (!storageLoadLoop(savedLoops[0].loopId, currentLoop)) {
            initStarterLoop(currentLoop, 0);
        }
    } else {
        initStarterLoop(currentLoop, 0);
    }

    if (!savedSongs.empty()) {
        if (!storageLoadSong(savedSongs[0].songId, currentSong)) {
            initStarterSong(currentSong, 0);
        }
    } else {
        initStarterSong(currentSong, 0);
    }

    hasUndo = false;
    gridBarPage = 0;
    activeView = VIEW_GRID;
    activeTrack = TRACK_MELODY;
    octaveOffset = 0;
    isPlaying = false;
    isRecording = false;
    editingFromSong = false;
    showSongShelf = false;
    showMixDrawer = false;

    audioEngineSetSongMode(false);
    syncAudioLoop();
    syncSongToAudio();
    loadActiveVocalForCurrentMode();
}

void AppState::refreshSavedLoops() {
    savedLoops = storageListLoops();
}

void AppState::refreshSavedSongs() {
    savedSongs = storageListSongs();
}

void AppState::setToast(const char* msg, int frames) {
    std::snprintf(toastMsg, sizeof(toastMsg), "%s", msg);
    toastFrames = frames;
}

void AppState::switchView(ViewMode newView) {
    if (isRecordingVocal) {
        stopVocalRecording(true);
    }
    showMixDrawer = false;
    if (activeView == newView) return;

    bool enteringSong = (newView == VIEW_SONG);
    bool leavingSong  = (activeView == VIEW_SONG);
    activeView = newView;

    if (enteringSong) {
        isRecording = false;
        editingFromSong = false;
        showSongShelf = false;
        syncSongToAudio();
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
    if (trackIdx < 0 || trackIdx >= BB_NUM_TRACKS) return;
    undoSnapshot = currentLoop.tracks[trackIdx];
    undoTrackIdx = trackIdx;
    hasUndo = true;
}

void AppState::performUndo() {
    if (activeView == VIEW_SONG && hasVocalUndo && undoVocalIsSong) {
        undoVocalTake();
        return;
    }
    if (!hasUndo) {
        if (hasVocalUndo && !undoVocalIsSong && undoVocalTargetId == currentLoop.loopId) {
            undoVocalTake();
            return;
        }
        setToast("Nothing to undo", 75);
        return;
    }
    currentLoop.tracks[undoTrackIdx] = undoSnapshot;
    hasUndo = false;
    syncAudioLoop();
    setToast("Undid last change!", 90);
}

void AppState::togglePlay() {
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
    if (activeView == VIEW_SONG) {
        if (isRecordingVocal) {
            stopVocalRecording(true);
        } else {
            startVocalRecording(true);
        }
        return;
    }
    isRecording = !isRecording;
    if (isRecording) {
        snapshotForUndo(activeTrack);
        if (!isPlaying) {
            isPlaying = true;
            audioEngineResetPlayhead();
            audioEngineSetPlaying(true);
        }
        setToast("REC ON: Play notes onto grid", 90);
    } else {
        setToast("JAM Mode: Live play only", 75);
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

void AppState::saveCurrentLoop() {
    if (storageSaveLoop(currentLoop)) {
        refreshSavedLoops();
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
        hasUndo = false;
        gridBarPage = 0;
        syncAudioLoop();
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
        hasUndo = false;
        gridBarPage = 0;
        syncAudioLoop();
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
    if (storageExportLoopWav(currentLoop, activeVocalSamples, path, sizeof(path))) {
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
}

void AppState::exportCurrentSongWav() {
    setToast("Rendering Song .WAV to SD...", 60);
    char path[256];
    if (storageExportSongWav(currentSong, activeVocalSamples, path, sizeof(path))) {
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
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Slot %d -> %s", slotIdx + 1, savedLoops[nextIdx].name);
    setToast(buf, 85);
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

void AppState::playLiveDegree(int degree, uint8_t velocity) {
    if (degree < 0 || degree >= BB_GRID_ROWS) return;

    int oct = (activeTrack == TRACK_BASS ? octaveOffset - 1 : octaveOffset);
    uint8_t instId = currentLoop.tracks[activeTrack].instrumentId;

    int noteCount = 1;
    uint8_t notes[3] = {0, 0, 0};
    if (activeTrack == TRACK_DRUMS) {
        notes[0] = getDrumPieceNote(degree);
    } else if (perfMode == 1) {
        // Chord Strum: 3-note scale triad
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
        int8_t bendQ4 = static_cast<int8_t>(std::lround(circleBendSemis * 16.0f));
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
    audioEngineReleaseLiveNotes(activeTrack);
    if (wifiMidiEnabled && lastLiveNote > 0) {
        uint8_t ch = (activeTrack == TRACK_DRUMS) ? 9 : static_cast<uint8_t>(activeTrack);
        wifiMidiSendNoteOff(ch, lastLiveNote);
    }
    arpHeldDegree = -1;
    lastArpStep = -1;
    arpCycleIdx = 0;
    recHeldStep = -1;
    recHeldNote = 0;
}

void AppState::handleHardwareInput(u32 kDown, u32 kHeld, u32 kUp, const circlePosition& cpad) {
    if (toastFrames > 0) toastFrames--;
    if (liveFlashFrames > 0) liveFlashFrames--;

    pollVocalRecording();

    // Promote quantized Spellbook cue once audio engine swaps at step 0 (#11)
    if (hasQueuedSpellbookLoop && !audioEngineHasQueuedLoop()) {
        currentLoop = queuedSpellbookLoop;
        hasQueuedSpellbookLoop = false;
        hasUndo = false;
        gridBarPage = 0;
        loadActiveVocalForCurrentMode();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Playing: %s", currentLoop.name);
        setToast(buf, 75);
    }

    if (kDown & KEY_START) {
        togglePlay();
    }
    if (kDown & KEY_SELECT) {
        switchView((activeView == VIEW_SPELLBOOK) ? VIEW_GRID : VIEW_SPELLBOOK);
    }

    if (kDown & KEY_L) {
        if (octaveOffset > -1) {
            octaveOffset--;
            setToast(octaveOffset == -1 ? "Octave: Low" : "Octave: Mid", 60);
        }
    }
    if (kDown & KEY_R) {
        if (octaveOffset < 1) {
            octaveOffset++;
            setToast(octaveOffset == 1 ? "Octave: High" : "Octave: Mid", 60);
        }
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

    audioEngineSetCirclePad(circleBendSemis, circleVibrato);
    if (wifiMidiEnabled && (std::fabs(circleBendSemis) > 0.02f)) {
        uint8_t ch = (activeTrack == TRACK_DRUMS) ? 9 : static_cast<uint8_t>(activeTrack);
        wifiMidiSendPitchBend(ch, circleBendSemis);
    }

    static const u32 NOTE_KEYS[8] = {
        KEY_DDOWN, KEY_DRIGHT, KEY_DUP, KEY_DLEFT,
        KEY_B, KEY_A, KEY_Y, KEY_X
    };
    static constexpr u32 ALL_NOTE_MASK =
        KEY_DDOWN | KEY_DRIGHT | KEY_DUP | KEY_DLEFT | KEY_B | KEY_A | KEY_Y | KEY_X;

    if (activeView == VIEW_PADS || activeView == VIEW_INSTRUMENT || activeView == VIEW_GRID) {
        for (int deg = 0; deg < 8; ++deg) {
            if (kDown & NOTE_KEYS[deg]) {
                playLiveDegree(deg, 105);
            }
        }
        if ((kUp & ALL_NOTE_MASK) && !(kHeld & ALL_NOTE_MASK)) {
            releaseLiveNotes();
        }
    }

    // Arpeggiator stepping while note is held (#12)
    if (arpHeldDegree >= 0 && activeTrack != TRACK_DRUMS && perfMode == 2 && isPlaying) {
        int curStep = audioEngineGetCurrentStep();
        if (curStep != lastArpStep) {
            lastArpStep = curStep;
            static const int kArpOffsets[4] = { 0, 2, 4, 7 };
            arpCycleIdx = (arpCycleIdx + 1) & 3;
            int oct = (activeTrack == TRACK_BASS ? octaveOffset - 1 : octaveOffset);
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
