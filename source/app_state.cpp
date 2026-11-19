#include "app_state.h"
#include "audio_engine.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace BardBeats {

void AppState::init() {
    refreshSavedLoops();
    if (!savedLoops.empty()) {
        if (!storageLoadLoop(savedLoops[0].loopId, currentLoop)) {
            initStarterLoop(currentLoop, 0);
        }
    } else {
        initStarterLoop(currentLoop, 0);
    }
    hasUndo = false;
    activeView = VIEW_GRID;
    activeTrack = TRACK_MELODY;
    octaveOffset = 0;
    isPlaying = false;
    isRecording = false;
    syncAudioLoop();
}

void AppState::refreshSavedLoops() {
    savedLoops = storageListLoops();
}

void AppState::setToast(const char* msg, int frames) {
    std::snprintf(toastMsg, sizeof(toastMsg), "%s", msg);
    toastFrames = frames;
}

void AppState::snapshotForUndo(int trackIdx) {
    if (trackIdx < 0 || trackIdx >= BB_NUM_TRACKS) return;
    undoSnapshot = currentLoop.tracks[trackIdx];
    undoTrackIdx = trackIdx;
    hasUndo = true;
}

void AppState::performUndo() {
    if (!hasUndo) {
        setToast("Nothing to undo", 75);
        return;
    }
    currentLoop.tracks[undoTrackIdx] = undoSnapshot;
    hasUndo = false;
    syncAudioLoop();
    setToast("Undid last change!", 90);
}

void AppState::togglePlay() {
    isPlaying = !isPlaying;
    if (isPlaying) {
        audioEngineResetPlayhead();
    } else {
        isRecording = false;
    }
    audioEngineSetPlaying(isPlaying);
}

void AppState::toggleRecord() {
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
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Loaded: %s", currentLoop.name);
        setToast(buf, 90);
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

    if (isRecording && isPlaying) {
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

    // Global transport & navigation shortcuts
    if (kDown & KEY_START) {
        togglePlay();
    }
    if (kDown & KEY_SELECT) {
        activeView = (activeView == VIEW_SPELLBOOK) ? VIEW_GRID : VIEW_SPELLBOOK;
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
    // Smooth cubic curve for fine center control
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
