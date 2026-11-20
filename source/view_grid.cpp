#include "view_grid.h"
#include "audio_engine.h"
#include "ui_common.h"
#include <cstdio>

namespace BardBeats {

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
    uiDrawText(10.0f, 212.0f, 0.36f, 0.36f, Colors::MUTED_TEXT,
               "Tap/Drag Grid | A/B/X/Y/D-Pad play live | Circle Pad bends pitch");
}

void viewGridDrawBottom(const AppState& state) {
    if (state.showMixDrawer) {
        uiDrawMixDrawer(state);
        return;
    }

    // Sub-toolbar (y: 67..87)
    Rect btnBpmDn  = {  3.0f, 67.0f, 34.0f, 20.0f };
    Rect btnBpmUp  = { 39.0f, 67.0f, 34.0f, 20.0f };
    Rect btnScale  = { 75.0f, 67.0f, 62.0f, 20.0f };
    Rect btnBars   = {139.0f, 67.0f, 44.0f, 20.0f };
    Rect btnSwing  = {185.0f, 67.0f, 46.0f, 20.0f };
    Rect btnMix    = {233.0f, 67.0f, 40.0f, 20.0f };
    Rect btnClear  = {275.0f, 67.0f, 42.0f, 20.0f };

    uiDrawButton(btnBpmDn, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "-BPM", 0.32f);
    uiDrawButton(btnBpmUp, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "+BPM", 0.32f);

    char scaleBuf[20];
    std::snprintf(scaleBuf, sizeof(scaleBuf), "%s", getScaleName(state.currentLoop.scaleId));
    uiDrawButton(btnScale, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::GOLD_TEXT, scaleBuf, 0.32f);

    char barBuf[16];
    if (state.currentLoop.numBars == 2) {
        std::snprintf(barBuf, sizeof(barBuf), "2B:P%d", state.gridBarPage + 1);
    } else {
        std::snprintf(barBuf, sizeof(barBuf), "1B:16");
    }
    uiDrawButton(btnBars, state.currentLoop.numBars == 2 ? Colors::BLUE_ACTION : Colors::BG_CARD,
                 Colors::BORDER_WOOD, Colors::CREAM_TEXT, barBuf, 0.32f);

    char swBuf[16];
    std::snprintf(swBuf, sizeof(swBuf), "Sw%u", state.currentLoop.swingPercent);
    uiDrawButton(btnSwing, state.currentLoop.swingPercent > 50 ? Colors::AMBER_JAM : Colors::BG_CARD,
                 Colors::BORDER_WOOD,
                 state.currentLoop.swingPercent > 50 ? Colors::BG_DARK : Colors::CREAM_TEXT,
                 swBuf, 0.32f);

    uiDrawButton(btnMix,   Colors::BG_CARD_ALT, Colors::GOLD_TEXT, Colors::GOLD_TEXT, "MIX", 0.32f);
    uiDrawButton(btnClear, Colors::BG_CARD,     Colors::RED_REC,   Colors::RED_REC,   "CLR", 0.32f);

    const TrackData& trk = state.currentLoop.tracks[state.activeTrack];
    int curStep = audioEngineGetCurrentStep();
    int pageOffset = (state.currentLoop.numBars == 2 && state.gridBarPage == 1) ? 16 : 0;
    uint32_t trkCol = Colors::TRACK_COLORS[state.activeTrack];

    for (int row = 0; row < BB_GRID_ROWS; ++row) {
        int visualRow = (BB_GRID_ROWS - 1) - row;
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
            int absStep = pageOffset + step;
            float rx = GRID_X + step * CELL_W;
            bool active = stepHasNote(trk.steps[absStep], rowNote);
            bool beatStart = (step % 4 == 0);
            uint32_t bg = active ? trkCol : (beatStart ? Colors::BG_CARD_ALT : Colors::BG_PANEL);
            if (state.isPlaying && absStep == curStep) {
                bg = active ? Colors::PLAYHEAD_LINE : Colors::BG_CARD;
            }
            C2D_DrawRectSolid(rx + 1.0f, ry + 1.0f, 0.1f, CELL_W - 2.0f, CELL_H - 2.0f, bg);
        }
    }
}

void viewGridHandleTouch(AppState& state, float tx, float ty, bool isJustPressed, bool isHeld, bool isReleased) {
    if (state.showMixDrawer) {
        if (isJustPressed || isHeld) {
            uiHandleMixDrawerTouch(state, tx, ty, isJustPressed, isHeld);
        }
        return;
    }

    if (isReleased) {
        s_lastPaintStep = -1;
        s_lastPaintRow = -1;
        return;
    }

    if (isJustPressed && ty >= 67.0f && ty < 87.0f) {
        Rect btnBpmDn  = {  3.0f, 67.0f, 34.0f, 20.0f };
        Rect btnBpmUp  = { 39.0f, 67.0f, 34.0f, 20.0f };
        Rect btnScale  = { 75.0f, 67.0f, 62.0f, 20.0f };
        Rect btnBars   = {139.0f, 67.0f, 44.0f, 20.0f };
        Rect btnSwing  = {185.0f, 67.0f, 46.0f, 20.0f };
        Rect btnMix    = {233.0f, 67.0f, 40.0f, 20.0f };
        Rect btnClear  = {275.0f, 67.0f, 42.0f, 20.0f };

        if (btnBpmDn.contains(tx, ty)) {
            if (state.currentLoop.bpm > 60) state.currentLoop.bpm -= 4;
            state.syncAudioLoop();
        } else if (btnBpmUp.contains(tx, ty)) {
            if (state.currentLoop.bpm < 180) state.currentLoop.bpm += 4;
            state.syncAudioLoop();
        } else if (btnScale.contains(tx, ty)) {
            state.currentLoop.scaleId = (state.currentLoop.scaleId + 1) % SCALE_COUNT;
            state.syncAudioLoop();
        } else if (btnBars.contains(tx, ty)) {
            if (state.currentLoop.numBars == 2 && state.gridBarPage == 0) {
                state.gridBarPage = 1;
                state.setToast("Viewing Bar 2 (Steps 17-32)", 70);
            } else {
                state.toggleLoopBars();
            }
        } else if (btnSwing.contains(tx, ty)) {
            state.cycleSwing();
        } else if (btnMix.contains(tx, ty)) {
            state.showMixDrawer = true;
        } else if (btnClear.contains(tx, ty)) {
            state.snapshotForUndo(state.activeTrack);
            for (int s = 0; s < BB_MAX_STEPS; ++s) {
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
        int absStep = ((state.currentLoop.numBars == 2 && state.gridBarPage == 1) ? 16 : 0) + step;

        if (absStep == s_lastPaintStep && row == s_lastPaintRow) return;

        uint8_t note = (state.activeTrack == TRACK_DRUMS)
                           ? getDrumPieceNote(row)
                           : getScaleNote(state.currentLoop.scaleId, state.currentLoop.rootNote,
                                          (state.activeTrack == TRACK_BASS ? state.octaveOffset - 1 : state.octaveOffset), row);

        StepCell& cell = state.currentLoop.tracks[state.activeTrack].steps[absStep];
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

        s_lastPaintStep = absStep;
        s_lastPaintRow = row;
        state.syncAudioLoop();
    }
}

} // namespace BardBeats
