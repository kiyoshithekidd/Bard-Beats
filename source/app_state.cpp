#include "app_state.h"
#include "audio_engine.h"
#include "mic_recorder.h"
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
            initStarterSong(currentSong);
        }
    } else {
        initStarterSong(currentSong);
    }

    hasUndo = false;
    activeView = VIEW_GRID;
    activeTrack = TRACK_MELODY;
    octaveOffset = 0;
    isPlaying = false;
    isRecording = false;
    editingFromSong = false;

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
    if (activeView == newView) return;

    bool enteringSong = (newView == VIEW_SONG);
    bool leavingSong  = (activeView == VIEW_SONG);
    activeView = newView;

    if (enteringSong) {
        isRecording = false;
        editingFromSong = false;
        syncSongToAudio();
        audioEngineSetSongMode(true);
        audioEngineResetPlayhead();
        loadActiveVocalForCurrentMode();
    } else if (leavingSong) {
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
        syncAudioLoop();
        if (activeView != VIEW_SONG) {
            loadActiveVocalForCurrentMode();
        }
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Loaded: %s", currentLoop.name);
        setToast(buf, 90);
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

void AppState::addCurrentLoopToSong() {
    if (currentSong.numEntries >= BB_MAX_SONG_CLIPS) {
        setToast("Song full (32 clips max)", 90);
        return;
    }
    // Ensure current loop is persisted before referencing it
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

    // Insert right after slotIdx
    int insertPos = slotIdx + 1;
    for (int i = currentSong.numEntries; i > insertPos; --i) {
        currentSong.chain[i] = currentSong.chain[i - 1];
    }
    currentSong.chain[insertPos] = currentSong.chain[slotIdx];
    currentSong.chain[insertPos].loopId = newId;
    currentSong.numEntries++;
    selectedSongSlot = insertPos;
    saveCurrentSong();

    // Immediately drill down to tweak the new clip
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
    currentLoop.bpm = currentSong.bpm; // Edit at song's master tempo
    hasUndo = false;
    editingFromSong = true;
    returnSongSlot = slotIdx;

    audioEngineSetSongMode(false);
    audioEngineResetPlayhead();
    syncAudioLoop();
    activeView = VIEW_GRID;
    loadActiveVocalForCurrentMode();

    char buf[64];
    std::snprintf(buf, sizeof(buf), "Editing: %s (Tap BACK TO SONG)", currentLoop.name);
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

int AppState::getTotalSongBars() const {
    int total = 0;
    for (int i = 0; i < currentSong.numEntries; ++i) {
        total += (currentSong.chain[i].repeatCount > 0 ? currentSong.chain[i].repeatCount : 1);
    }
    return total;
}

const char* AppState::getLoopNameById(uint16_t loopId) const {
    for (const auto& s : savedLoops) {
        if (s.loopId == loopId) return s.name;
    }
    return "Loop";
}

// ---- Vocal Layer Operations ----

void AppState::loadActiveVocalForCurrentMode() {
    bool isSong = (activeView == VIEW_SONG);
    char path[256];
    bool hasVoc = false;
    uint8_t vol = 95;
    bool muted = false;

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
                                 !isSong, // Loop vocal loops per bar; song vocal plays start-to-finish
                                 vol, muted);
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

    // Save current active vocal into shadow undo buffer so bad takes never ruin anything
    undoVocalSamples = activeVocalSamples;
    undoVocalIsSong = songScope;
    undoVocalTargetId = songScope ? currentSong.songId : currentLoop.loopId;
    hasVocalUndo = true;

    // Mute existing vocal during retake while keeping instrumental backing playing
    audioEngineClearVocalLayer();

    uint16_t bpm = songScope ? currentSong.bpm : currentLoop.bpm;
    if (bpm < 60 || bpm > 180) bpm = 110;
    double samplesPerBar = (static_cast<double>(BB_MIC_SAMPLE_RATE) * 60.0 * 4.0) / static_cast<double>(bpm);
    uint32_t targetSamples = songScope
        ? static_cast<uint32_t>(std::round(samplesPerBar * std::max(1, getTotalSongBars())))
        : static_cast<uint32_t>(std::round(samplesPerBar));

    if (!micRecorderStart(targetSamples)) {
        setToast("Mic start failed!", 110);
        loadActiveVocalForCurrentMode();
        return;
    }

    isRecordingVocal = true;
    vocalRecIsSongScope = songScope;
    isRecording = false; // Don't write grid notes while singing

    // Restart playback from bar 1 so vocals align cleanly
    audioEngineSetSongMode(songScope);
    audioEngineResetPlayhead();
    isPlaying = true;
    audioEngineSetPlaying(true);

    setToast(songScope ? "SINGING (SONG): Tap REC/STOP to finish"
                       : "SINGING (1-BAR CLIP): Auto-stops at bar end", 120);
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
    bool reachedTarget = micRecorderPoll();
    if (reachedTarget) {
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

void AppState::playLiveDegree(int degree, uint8_t velocity) {
    if (degree < 0 || degree >= BB_GRID_ROWS) return;

    uint8_t note = (activeTrack == TRACK_DRUMS)
                       ? getDrumPieceNote(degree)
                       : getScaleNote(currentLoop.scaleId, currentLoop.rootNote,
                                      (activeTrack == TRACK_BASS ? octaveOffset - 1 : octaveOffset), degree);
    uint8_t instId = currentLoop.tracks[activeTrack].instrumentId;

    audioEngineTriggerLiveNote(activeTrack, instId, note, velocity);
    lastLiveNote = note;
    lastLiveDegree = degree;
    liveFlashFrames = 14;

    if (isRecording && isPlaying && activeView != VIEW_SONG) {
        int step = audioEngineGetCurrentStep();
        float frac = audioEngineGetStepFraction();
        int qStep = (frac >= 0.55f) ? ((step + 1) % BB_STEPS_PER_LOOP) : step;
        int8_t bendQ4 = static_cast<int8_t>(std::lround(circleBendSemis * 16.0f));
        if (bendQ4 > 32) bendQ4 = 32;
        if (bendQ4 < -32) bendQ4 = -32;
        addStepNote(currentLoop.tracks[activeTrack].steps[qStep], note, velocity, bendQ4);
        syncAudioLoop();
    }
}

void AppState::releaseLiveNotes() {
    audioEngineReleaseLiveNotes(activeTrack);
}

void AppState::handleHardwareInput(u32 kDown, u32 kHeld, u32 kUp, const circlePosition& cpad) {
    if (toastFrames > 0) toastFrames--;
    if (liveFlashFrames > 0) liveFlashFrames--;

    pollVocalRecording();

    // Global transport & navigation shortcuts
    if (kDown & KEY_START) {
        togglePlay();
    }
    if (kDown & KEY_SELECT) {
        switchView((activeView == VIEW_SPELLBOOK) ? VIEW_GRID : VIEW_SPELLBOOK);
    }

    // Octave shift on L / R
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

    // Circle Pad Pitch Bend (+/- 2 semitones on X) & Vibrato (on Y)
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

    // Physical 8-note live play buttons (Ocarina-style A/B/X/Y + D-Pad)
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
}

} // namespace BardBeats
