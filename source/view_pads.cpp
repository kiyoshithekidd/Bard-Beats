#include "view_pads.h"
#include "ui_common.h"
#include <cstdio>

namespace BardBeats {

static int s_heldPad = -1;

static Rect getPadRect(int idx) {
    int col = idx % 4;
    int row = idx / 4; // 0 = top row (deg 4..7), 1 = bottom row (deg 0..3)
    return { 4.0f + col * 79.0f, 89.0f + row * 57.0f, 75.0f, 54.0f };
}

static int padIndexToDegree(int idx) {
    int col = idx % 4;
    int row = idx / 4;
    return (row == 0) ? (4 + col) : col;
}

void viewPadsDrawTop(const AppState& state) {
    uiDrawTopHeader(state);
    uiDrawTopFourTrackOverview(state, 40.0f, 150.0f);

    char bendBuf[64];
    std::snprintf(bendBuf, sizeof(bendBuf), "Mode: %s | Circle Pad Bend: %+.1f st",
                  state.isRecording ? "RECORDING" : "LIVE JAM",
                  state.circleBendSemis);
    uiDrawText(10.0f, 200.0f, 0.42f, 0.42f,
               state.isRecording ? Colors::RED_REC : Colors::GOLD_TEXT, bendBuf);
}

void viewPadsDrawBottom(const AppState& state) {
    Rect btnMode = {   4.0f, 67.0f, 102.0f, 20.0f };
    Rect btnDice = { 109.0f, 67.0f,  52.0f, 20.0f };
    Rect btnOct  = { 164.0f, 67.0f,  68.0f, 20.0f };
    Rect btnMix  = { 235.0f, 67.0f,  52.0f, 20.0f };
    Rect btnHelp = { 290.0f, 67.0f,  26.0f, 20.0f };

    const char* modeStr = (state.perfMode == 1) ? "CHORD"
                                                : (state.perfMode == 2 ? "ARP" : "NOTE");
    char modeLbl[20];
    std::snprintf(modeLbl, sizeof(modeLbl), "MODE:%s", modeStr);
    uiDrawButton(btnMode, state.perfMode > 0 ? Colors::BLUE_ACTION : Colors::BG_CARD,
                 Colors::BORDER_WOOD, Colors::CREAM_TEXT, modeLbl, 0.38f);
    uiDrawButton(btnDice, Colors::PURPLE_SPELL, Colors::GOLD_TEXT, Colors::CREAM_TEXT, "DICE", 0.38f);
    const char* octStr = (state.octaveOffset < 0) ? "OCT:LO" : (state.octaveOffset > 0 ? "OCT:HI" : "OCT:MD");
    uiDrawButton(btnOct, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::GOLD_TEXT, octStr, 0.38f);
    uiDrawButton(btnMix, Colors::BG_CARD_ALT, Colors::GOLD_TEXT, Colors::GOLD_TEXT, "MIX", 0.38f);
    uiDrawButton(btnHelp, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::GOLD_TEXT, "?", 0.40f);

    uint32_t trkCol = Colors::TRACK_COLORS[state.activeTrack];
    int effOct = state.getEffectiveLiveOctaveOffset();
    for (int i = 0; i < 8; ++i) {
        int deg = padIndexToDegree(i);
        Rect r = getPadRect(i);
        bool lit = (s_heldPad == i) || (state.liveFlashFrames > 0 && state.lastLiveDegree == deg);
        uint32_t bg = lit ? trkCol : Colors::BG_CARD;
        uint32_t border = lit ? Colors::CREAM_TEXT : Colors::BORDER_WOOD;

        char lbl[16];
        if (state.activeTrack == TRACK_DRUMS) {
            std::snprintf(lbl, sizeof(lbl), "%s", getDrumPieceName(deg));
        } else {
            uint8_t note = getScaleNote(state.currentLoop.scaleId, state.currentLoop.rootNote, effOct, deg);
            getNoteName(note, lbl, sizeof(lbl));
        }
        uiDrawButton(r, bg, border, lit ? Colors::BG_DARK : Colors::CREAM_TEXT, lbl, 0.48f);

        // Symmetric 3DS button badge in top-left corner of each pad
        uiDrawText(r.x + 4.0f, r.y + 2.0f, 0.38f, 0.38f,
                   lit ? Colors::BG_DARK : Colors::GOLD_TEXT, getDegreeButtonBadge(deg));

        // Subtle accent zone line on lower 35% of pad
        C2D_DrawRectSolid(r.x + 6.0f, r.y + r.h - 12.0f, 0.2f, r.w - 12.0f, 1.0f,
                          C2D_Color32(0xFF, 0xFF, 0xFF, 0x28));
    }
}

void viewPadsHandleTouch(AppState& state, float tx, float ty, bool isJustPressed, bool isReleased) {
    if (isReleased) {
        if (s_heldPad >= 0) {
            s_heldPad = -1;
            state.releaseLiveNotes();
        }
        return;
    }

    if (isJustPressed) {
        if (ty >= 67.0f && ty < 88.0f) {
            Rect btnMode = {   4.0f, 67.0f, 102.0f, 20.0f };
            Rect btnDice = { 109.0f, 67.0f,  52.0f, 20.0f };
            Rect btnOct  = { 164.0f, 67.0f,  68.0f, 20.0f };
            Rect btnMix  = { 235.0f, 67.0f,  52.0f, 20.0f };
            Rect btnHelp = { 290.0f, 67.0f,  26.0f, 20.0f };
            if (btnMode.contains(tx, ty)) {
                state.cyclePerfMode();
            } else if (btnDice.contains(tx, ty)) {
                state.rollBardicDice();
            } else if (btnOct.contains(tx, ty)) {
                state.octaveOffset = (state.octaveOffset == 1) ? -1 : (state.octaveOffset + 1);
            } else if (btnMix.contains(tx, ty)) {
                state.showMixDrawer = true;
                state.showInstPicker = false;
                state.showHelpOverlay = false;
            } else if (btnHelp.contains(tx, ty)) {
                state.showHelpOverlay = true;
                state.showMixDrawer = false;
                state.showInstPicker = false;
            }
            return;
        }
        for (int i = 0; i < 8; ++i) {
            Rect r = getPadRect(i);
            if (r.contains(tx, ty)) {
                s_heldPad = i;
                int deg = padIndexToDegree(i);
                bool accent = (ty >= r.y + r.h * 0.65f);
                state.playLiveDegree(deg, accent ? 127 : 95);
                return;
            }
        }
    }
}

} // namespace BardBeats
