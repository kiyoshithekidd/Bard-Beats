#include "view_grid.h"
#include "audio_engine.h"
#include "ui_common.h"
#include <cstdio>

namespace BardBeats {

// Bottom Workspace: y = 66 .. 206 (140 px tall)
// Top sub-toolbar: y = 68 .. 88 (BPM -, BPM +, Scale, Mute, Clear)
// 16x8 Grid: x = 32 .. 320 (18px per step = 288px), y = 90 .. 202 (14px per row = 112px)
static constexpr float GRID_X = 32.0f;
static constexpr float GRID_Y = 90.0f;
static constexpr float CELL_W = 18.0f;
static constexpr float CELL_H = 14.0f;

static int s_lastPaintStep = -1;
static int s_lastPaintRow = -1;
static bool s_paintAddMode = true;

void viewGridDrawTop(const AppState& state) {
    uiDrawTopHeader(state);
    uiDrawTopFourTrackOverview(state, 40.0f, 166.0f);
    uiDrawText(10.0f, 212.0f, 0.38f, 0.38f, Colors::MUTED_TEXT,
               "Tap/Drag Grid to place notes | A/B/X/Y/D-Pad play live | Circle Pad bends pitch");
}

void viewGridDrawBottom(const AppState& state) {
    // Sub-toolbar (y: 67..87)
    Rect btnBpmDn  = {  4.0f, 67.0f, 34.0f, 20.0f };
    Rect btnBpmUp  = { 40.0f, 67.0f, 34.0f, 20.0f };
    Rect btnScale  = { 78.0f, 67.0f, 82.0f, 20.0f };
    Rect btnMute   = {164.0f, 67.0f, 74.0f, 20.0f };
    Rect btnClear  = {242.0f, 67.0f, 74.0f, 20.0f };

    char bpmBuf[24];
    std::snprintf(bpmBuf, sizeof(bpmBuf), "-BPM");
    uiDrawButton(btnBpmDn, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::CREAM_TEXT, bpmBuf, 0.34f);
    uiDrawButton(btnBpmUp, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "+BPM", 0.34f);

    char scaleBuf[24];
    std::snprintf(scaleBuf, sizeof(scaleBuf), "Sc:%s", getScaleName(state.currentLoop.scaleId));
    uiDrawButton(btnScale, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::GOLD_TEXT, scaleBuf, 0.34f);

    bool muted = (state.currentLoop.tracks[state.activeTrack].muted != 0);
    uiDrawButton(btnMute, muted ? Colors::RED_REC : Colors::BG_CARD, Colors::BORDER_WOOD,
                 Colors::CREAM_TEXT, muted ? "UNMUTE" : "MUTE TRK", 0.34f);
    uiDrawButton(btnClear, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::RED_REC, "CLR TRK", 0.34f);

    const TrackData& trk = state.currentLoop.tracks[state.activeTrack];
    int curStep = audioEngineGetCurrentStep();
    uint32_t trkCol = Colors::TRACK_COLORS[state.activeTrack];

    // Row labels (left 30px) + 16x8 Grid
    for (int row = 0; row < BB_GRID_ROWS; ++row) {
        int visualRow = (BB_GRID_ROWS - 1) - row; // Higher pitch at top
        float ry = GRID_Y + visualRow * CELL_H;

        char lbl[12];
        uint8_t rowNote = (state.activeTrack == TRACK_DRUMS)
                              ? getDrumPieceNote(row)
                              : getScaleNote(state.currentLoop.scaleId, state.currentLoop.rootNote,
                                             (state.activeTrack == TRACK_BASS ? state.octaveOffset - 1 : state.octaveOffset), row);
        if (state.activeTrack == TRACK_DRUMS) {
            std::snprintf(lbl, sizeof(lbl), "%s", getDrumPieceName(row));
        } else {
            getNoteName(rowNote, lbl, sizeof(lbl));
        }
        uiDrawText(2.0f, ry + 1.0f, 0.32f, 0.32f, Colors::MUTED_TEXT, lbl);

        for (int step = 0; step < BB_STEPS_PER_LOOP; ++step) {
            float rx = GRID_X + step * CELL_W;
            bool active = stepHasNote(trk.steps[step], rowNote);
            bool beatStart = (step % 4 == 0);
            uint32_t bg = active ? trkCol : (beatStart ? Colors::BG_CARD_ALT : Colors::BG_PANEL);
            if (state.isPlaying && step == curStep) {
                bg = active ? Colors::PLAYHEAD_LINE : Colors::BG_CARD;
            }
            C2D_DrawRectSolid(rx + 1.0f, ry + 1.0f, 0.1f, CELL_W - 2.0f, CELL_H - 2.0f, bg);
        }
    }
}

void viewGridHandleTouch(AppState& state, float tx, float ty, bool isJustPressed, bool isHeld, bool isReleased) {
    if (isReleased) {
        s_lastPaintStep = -1;
        s_lastPaintRow = -1;
        return;
    }

    if (isJustPressed && ty >= 67.0f && ty < 87.0f) {
        Rect btnBpmDn = {  4.0f, 67.0f, 34.0f, 20.0f };
        Rect btnBpmUp = { 40.0f, 67.0f, 34.0f, 20.0f };
        Rect btnScale = { 78.0f, 67.0f, 82.0f, 20.0f };
        Rect btnMute  = {164.0f, 67.0f, 74.0f, 20.0f };
        Rect btnClear = {242.0f, 67.0f, 74.0f, 20.0f };

        if (btnBpmDn.contains(tx, ty)) {
            if (state.currentLoop.bpm > 60) state.currentLoop.bpm -= 4;
            state.syncAudioLoop();
        } else if (btnBpmUp.contains(tx, ty)) {
            if (state.currentLoop.bpm < 180) state.currentLoop.bpm += 4;
            state.syncAudioLoop();
        } else if (btnScale.contains(tx, ty)) {
            state.currentLoop.scaleId = (state.currentLoop.scaleId + 1) % SCALE_COUNT;
            state.syncAudioLoop();
        } else if (btnMute.contains(tx, ty)) {
            state.currentLoop.tracks[state.activeTrack].muted ^= 1;
            state.syncAudioLoop();
        } else if (btnClear.contains(tx, ty)) {
            state.snapshotForUndo(state.activeTrack);
            for (int s = 0; s < BB_STEPS_PER_LOOP; ++s) {
                clearStepCell(state.currentLoop.tracks[state.activeTrack].steps[s]);
            }
            state.syncAudioLoop();
            state.setToast("Cleared track (UNDO to restore)", 90);
        }
        return;
    }

    if ((isJustPressed || isHeld) && tx >= GRID_X && ty >= GRID_Y && ty < (GRID_Y + BB_GRID_ROWS * CELL_H)) {
        int step = static_cast<int>((tx - GRID_X) / CELL_W);
        int visualRow = static_cast<int>((ty - GRID_Y) / CELL_H);
        if (step < 0 || step >= BB_STEPS_PER_LOOP || visualRow < 0 || visualRow >= BB_GRID_ROWS) return;
        int row = (BB_GRID_ROWS - 1) - visualRow;

        if (step == s_lastPaintStep && row == s_lastPaintRow) return;

        uint8_t note = (state.activeTrack == TRACK_DRUMS)
                           ? getDrumPieceNote(row)
                           : getScaleNote(state.currentLoop.scaleId, state.currentLoop.rootNote,
                                          (state.activeTrack == TRACK_BASS ? state.octaveOffset - 1 : state.octaveOffset), row);

        StepCell& cell = state.currentLoop.tracks[state.activeTrack].steps[step];
        if (isJustPressed) {
            state.snapshotForUndo(state.activeTrack);
            s_paintAddMode = !stepHasNote(cell, note);
        }

        if (s_paintAddMode) {
            addStepNote(cell, note, 100, 0);
            audioEngineTriggerLiveNote(state.activeTrack, state.currentLoop.tracks[state.activeTrack].instrumentId, note, 100);
        } else if (stepHasNote(cell, note)) {
            toggleStepNote(cell, note);
        }

        s_lastPaintStep = step;
        s_lastPaintRow = row;
        state.syncAudioLoop();
    }
}

} // namespace BardBeats
