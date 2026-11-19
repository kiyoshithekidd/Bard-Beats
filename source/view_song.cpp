#include "view_song.h"
#include "ui_common.h"

namespace BardBeats {

void viewSongDrawTop(const AppState& state) {
    uiDrawTopHeader(state);
    C2D_DrawRectSolid(20.0f, 54.0f, 0.1f, 360.0f, 120.0f, Colors::BG_PANEL);
    uiDrawTextCentered({20.0f, 70.0f, 360.0f, 30.0f}, 0.52f, Colors::GOLD_TEXT,
                       "Song Mode & Vocals (Slice 2)");
    uiDrawTextCentered({20.0f, 108.0f, 360.0f, 24.0f}, 0.40f, Colors::CREAM_TEXT,
                       "Chain loops into songs and record vocals with the 3DS mic.");
    uiDrawTextCentered({20.0f, 134.0f, 360.0f, 24.0f}, 0.38f, Colors::MUTED_TEXT,
                       "Slice 1 focuses on the 16-step grid, live play, and Spellbook.");
}

void viewSongDrawBottom(const AppState& state) {
    (void)state;
    Rect card = { 16.0f, 80.0f, 288.0f, 110.0f };
    uiDrawButton(card, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::GOLD_TEXT,
                 "Tap SPELLBOOK for 1-Tap Loop Cues", 0.42f);
}

void viewSongHandleTouch(AppState& state, float tx, float ty, bool isJustPressed) {
    if (!isJustPressed) return;
    Rect card = { 16.0f, 80.0f, 288.0f, 110.0f };
    if (card.contains(tx, ty)) {
        state.activeView = VIEW_SPELLBOOK;
    }
}

} // namespace BardBeats
