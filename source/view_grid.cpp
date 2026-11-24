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
static int s_dragStartStep = -1;
static int s_dragStartRow = -1;
static bool s_paintAddMode = true;

void viewGridDrawTop(const AppState& state) {
    uiDrawTopHeader(state);
    uiDrawTopFourTrackOverview(state, 40.0f, 166.0f);
    uiDrawText(10.0f, 212.0f, 0.38f, 0.38f, Colors::MUTED_TEXT,
               "Tap/Drag Grid (Drag right=Sustain) | A/B/X/Y/D-Pad play live");
}

void viewGridDrawBottom(const AppState& state) {
    if (state.showMixDrawer) {
        uiDrawMixDrawer(state);
        return;
    }

    // Sub-toolbar (y: 67..87): -1 | TAP | +1 | Scale | 1B/2B | Swing | DICE | MIX | CLR
    Rect btnBpmDn  = {  2.0f, 67.0f, 22.0f, 20.0f };
    Rect btnTap    = { 25.0f, 67.0f, 34.0f, 20.0f };
    Rect btnBpmUp  = { 60.0f, 67.0f, 22.0f, 20.0f };
    Rect btnScale  = { 84.0f, 67.0f, 48.0f, 20.0f };
    Rect btnBars   = {134.0f, 67.0f, 36.0f, 20.0f };
    Rect btnSwing  = {172.0f, 67.0f, 34.0f, 20.0f };
    Rect btnDice   = {208.0f, 67.0f, 36.0f, 20.0f };
    Rect btnMix    = {246.0f, 67.0f, 36.0f, 20.0f };
    Rect btnClear  = {284.0f, 67.0f, 34.0f, 20.0f };

    uiDrawButton(btnBpmDn, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "-", 0.42f);
    uiDrawButton(btnTap,   Colors::BG_CARD_ALT, Colors::GOLD_TEXT, Colors::GOLD_TEXT, "TAP", 0.38f);
    uiDrawButton(btnBpmUp, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "+", 0.42f);

    char scaleBuf[20];
    std::snprintf(scaleBuf, sizeof(scaleBuf), "%s", getScaleName(state.currentLoop.scaleId));
    uiDrawButton(btnScale, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::GOLD_TEXT, scaleBuf, 0.38f);

    char barBuf[16];
    if (state.currentLoop.numBars == 2) {
        std::snprintf(barBuf, sizeof(barBuf), "2B:%d", state.gridBarPage + 1);
    } else {
        std::snprintf(barBuf, sizeof(barBuf), "1B");
    }
    uiDrawButton(btnBars, state.currentLoop.numBars == 2 ? Colors::BLUE_ACTION : Colors::BG_CARD,
                 Colors::BORDER_WOOD, Colors::CREAM_TEXT, barBuf, 0.38f);

    char swBuf[16];
    std::snprintf(swBuf, sizeof(swBuf), "S%u", state.currentLoop.swingPercent);
    uiDrawButton(btnSwing, state.currentLoop.swingPercent > 50 ? Colors::AMBER_JAM : Colors::BG_CARD,
                 Colors::BORDER_WOOD,
                 state.currentLoop.swingPercent > 50 ? Colors::BG_DARK : Colors::CREAM_TEXT,
                 swBuf, 0.38f);

    uiDrawButton(btnDice,  Colors::PURPLE_SPELL, Colors::GOLD_TEXT, Colors::CREAM_TEXT, "DICE", 0.38f);
    uiDrawButton(btnMix,   Colors::BG_CARD_ALT,  Colors::GOLD_TEXT, Colors::GOLD_TEXT,  "MIX",  0.38f);
    uiDrawButton(btnClear, Colors::BG_CARD,      Colors::RED_REC,   Colors::RED_REC,    "CLR",  0.38f);

    const TrackData& trk = state.currentLoop.tracks[state.activeTrack];
    int curStep = audioEngineGetCurrentStep();
    int pageOffset = (state.currentLoop.numBars == 2 && state.gridBarPage == 1) ? 16 : 0;
    uint32_t trkCol = Colors::TRACK_COLORS[state.activeTrack];
    bool isDiatonic = (state.currentLoop.scaleId == SCALE_DORIAN ||
                       state.currentLoop.scaleId == SCALE_MAJOR ||
                       state.currentLoop.scaleId == SCALE_MINOR);

    for (int row = 0; row < BB_GRID_ROWS; ++row) {
        int visualRow = (BB_GRID_ROWS - 1) - row;
        float ry = GRID_Y + visualRow * CELL_H;
        bool isRootRow = (state.activeTrack != TRACK_DRUMS) &&
                         (row == 0 || (isDiatonic && row == 7));

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
        uiDrawText(1.0f, ry + 1.0f, 0.38f, 0.38f,
                   isRootRow ? Colors::GOLD_TEXT : Colors::MUTED_TEXT, lbl);

        for (int step = 0; step < BB_STEPS_PER_LOOP; ++step) {
            int absStep = pageOffset + step;
            float rx = GRID_X + step * CELL_W;
            bool active = stepHasNote(trk.steps[absStep], rowNote);
            bool beatStart = (step % 4 == 0);
            uint32_t emptyCol = isRootRow
                ? (beatStart ? C2D_Color32(0x52, 0x41, 0x2F, 0xFF) : C2D_Color32(0x36, 0x2A, 0x20, 0xFF))
                : (beatStart ? Colors::BG_CARD_ALT : Colors::BG_PANEL);
            uint32_t bg = active ? trkCol : emptyCol;
            if (state.isPlaying && absStep == curStep) {
                bg = active ? Colors::PLAYHEAD_LINE : Colors::BG_CARD;
            }
            C2D_DrawRectSolid(rx + 1.0f, ry + 1.0f, 0.1f, CELL_W - 2.0f, CELL_H - 2.0f, bg);

            if (active && state.activeTrack != TRACK_DRUMS && trk.steps[absStep].gateLength > 1) {
                int span = trk.steps[absStep].gateLength;
                if (step + span > BB_STEPS_PER_LOOP) span = BB_STEPS_PER_LOOP - step;
                if (span > 1) {
                    float tieW = static_cast<float>(span) * CELL_W - 4.0f;
                    C2D_DrawRectSolid(rx + 2.0f, ry + 3.0f, 0.25f, tieW, CELL_H - 6.0f, trkCol);
                }
            }
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
        if (s_lastPaintStep >= 0) {
            state.releaseLiveNotes();
        }
        s_lastPaintStep = -1;
        s_lastPaintRow = -1;
        s_dragStartStep = -1;
        s_dragStartRow = -1;
        return;
    }

    if (isJustPressed && ty >= 67.0f && ty < 87.0f) {
        Rect btnBpmDn  = {  2.0f, 67.0f, 22.0f, 20.0f };
        Rect btnTap    = { 25.0f, 67.0f, 34.0f, 20.0f };
        Rect btnBpmUp  = { 60.0f, 67.0f, 22.0f, 20.0f };
        Rect btnScale  = { 84.0f, 67.0f, 48.0f, 20.0f };
        Rect btnBars   = {134.0f, 67.0f, 36.0f, 20.0f };
        Rect btnSwing  = {172.0f, 67.0f, 34.0f, 20.0f };
        Rect btnDice   = {208.0f, 67.0f, 36.0f, 20.0f };
        Rect btnMix    = {246.0f, 67.0f, 36.0f, 20.0f };
        Rect btnClear  = {284.0f, 67.0f, 34.0f, 20.0f };

        if (btnBpmDn.contains(tx, ty)) {
            state.adjustBpm(-1);
        } else if (btnTap.contains(tx, ty)) {
            state.tapTempo();
        } else if (btnBpmUp.contains(tx, ty)) {
            state.adjustBpm(1);
        } else if (btnScale.contains(tx, ty)) {
            state.cycleScaleSmart();
        } else if (btnBars.contains(tx, ty)) {
            state.cycleLoopBarsAndPage();
        } else if (btnSwing.contains(tx, ty)) {
            state.cycleSwing();
        } else if (btnDice.contains(tx, ty)) {
            state.rollBardicDice();
        } else if (btnMix.contains(tx, ty)) {
            state.showMixDrawer = true;
            state.showInstPicker = false;
            state.showHelpOverlay = false;
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
            s_dragStartStep = absStep;
            s_dragStartRow = row;
        }

        if (isHeld && !isJustPressed && s_paintAddMode && s_dragStartStep >= 0 &&
            row == s_dragStartRow && absStep > s_dragStartStep && state.activeTrack != TRACK_DRUMS) {
            int gate = (absStep - s_dragStartStep) + 1;
            if (gate > 16) gate = 16;
            state.currentLoop.tracks[state.activeTrack].steps[s_dragStartStep].gateLength = static_cast<uint8_t>(gate);
        } else if (s_paintAddMode) {
            addStepNote(cell, note, 100, 0);
            audioEngineReleaseLiveNotes(state.activeTrack);
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
