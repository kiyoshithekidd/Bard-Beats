#include "view_spellbook.h"
#include "audio_engine.h"
#include "ui_common.h"
#include <cstdio>

namespace BardBeats {

static constexpr int CARDS_PER_PAGE = 4;

static Rect getCardRect(int slot) {
    int col = slot % 2;
    int row = slot / 2;
    return { 6.0f + col * 156.0f, 70.0f + row * 50.0f, 152.0f, 46.0f };
}

void viewSpellbookDrawTop(const AppState& state) {
    uiDrawTopHeader(state);
    uiDrawTopFourTrackOverview(state, 40.0f, 148.0f);

    char info[64];
    std::snprintf(info, sizeof(info), "Spellbook: %zu saved loops | Tap any loop to play on cue!",
                  state.savedLoops.size());
    uiDrawText(10.0f, 198.0f, 0.42f, 0.42f, Colors::GOLD_TEXT, info);
}

void viewSpellbookDrawBottom(const AppState& state) {
    int total = static_cast<int>(state.savedLoops.size());
    int totalPages = (total + CARDS_PER_PAGE - 1) / CARDS_PER_PAGE;
    if (totalPages < 1) totalPages = 1;
    int page = state.spellbookPage;
    if (page >= totalPages) page = totalPages - 1;
    if (page < 0) page = 0;

    int startIdx = page * CARDS_PER_PAGE;
    for (int i = 0; i < CARDS_PER_PAGE; ++i) {
        int idx = startIdx + i;
        Rect r = getCardRect(i);
        if (idx < total) {
            const LoopSummary& item = state.savedLoops[idx];
            bool isCur = (item.loopId == state.currentLoop.loopId);
            uint32_t bg = isCur ? Colors::PURPLE_SPELL : Colors::BG_CARD;
            uint32_t border = isCur ? Colors::GOLD_TEXT : Colors::BORDER_WOOD;
            uiDrawButton(r, bg, border, Colors::CREAM_TEXT, nullptr);

            uiDrawText(r.x + 8.0f, r.y + 6.0f, 0.42f, 0.42f, Colors::GOLD_TEXT, item.name);
            char sub[32];
            std::snprintf(sub, sizeof(sub), "%u BPM | %s", item.bpm, getScaleName(item.scaleId));
            uiDrawText(r.x + 8.0f, r.y + 24.0f, 0.35f, 0.35f, Colors::CREAM_TEXT, sub);
        } else {
            uiDrawButton(r, Colors::BG_DARK, Colors::BG_PANEL, Colors::MUTED_TEXT, "(Empty)", 0.36f);
        }
    }

    // Bottom Action Row (y: 173..201)
    Rect btnRename = {  4.0f, 173.0f, 64.0f, 28.0f };
    Rect btnCopy   = { 71.0f, 173.0f, 54.0f, 28.0f };
    Rect btnMidi   = {128.0f, 173.0f, 64.0f, 28.0f };
    Rect btnDel    = {195.0f, 173.0f, 48.0f, 28.0f };
    Rect btnPrev   = {246.0f, 173.0f, 34.0f, 28.0f };
    Rect btnNext   = {283.0f, 173.0f, 33.0f, 28.0f };

    uiDrawButton(btnRename, Colors::BG_CARD_ALT, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "RENAME", 0.35f);
    uiDrawButton(btnCopy,   Colors::BG_CARD_ALT, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "COPY", 0.35f);
    uiDrawButton(btnMidi,   Colors::BLUE_ACTION, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "EXP.MID", 0.34f);
    uiDrawButton(btnDel,    Colors::BG_CARD,     Colors::RED_REC,     Colors::RED_REC,    "DEL", 0.35f);
    uiDrawButton(btnPrev,   Colors::BG_CARD,     Colors::BORDER_WOOD, Colors::GOLD_TEXT,  "<", 0.42f);
    uiDrawButton(btnNext,   Colors::BG_CARD,     Colors::BORDER_WOOD, Colors::GOLD_TEXT,  ">", 0.42f);
}

void viewSpellbookHandleTouch(AppState& state, float tx, float ty, bool isJustPressed) {
    if (!isJustPressed) return;

    int total = static_cast<int>(state.savedLoops.size());
    int totalPages = (total + CARDS_PER_PAGE - 1) / CARDS_PER_PAGE;
    if (totalPages < 1) totalPages = 1;

    // Loop Cue Cards
    for (int i = 0; i < CARDS_PER_PAGE; ++i) {
        int idx = state.spellbookPage * CARDS_PER_PAGE + i;
        if (idx < total && getCardRect(i).contains(tx, ty)) {
            state.loadLoopById(state.savedLoops[idx].loopId);
            if (!state.isPlaying) {
                state.isPlaying = true;
                audioEngineResetPlayhead();
                audioEngineSetPlaying(true);
            }
            return;
        }
    }

    // Action Buttons
    Rect btnRename = {  4.0f, 173.0f, 64.0f, 28.0f };
    Rect btnCopy   = { 71.0f, 173.0f, 54.0f, 28.0f };
    Rect btnMidi   = {128.0f, 173.0f, 64.0f, 28.0f };
    Rect btnDel    = {195.0f, 173.0f, 48.0f, 28.0f };
    Rect btnPrev   = {246.0f, 173.0f, 34.0f, 28.0f };
    Rect btnNext   = {283.0f, 173.0f, 33.0f, 28.0f };

    if (btnRename.contains(tx, ty)) {
        state.renameCurrentLoop();
    } else if (btnCopy.contains(tx, ty)) {
        state.copyCurrentLoop();
    } else if (btnMidi.contains(tx, ty)) {
        char path[256];
        if (storageExportLoopMidi(state.currentLoop, path, sizeof(path))) {
            state.setToast("Exported .mid to exports/", 110);
        } else {
            state.setToast("MIDI Export Failed!", 110);
        }
    } else if (btnDel.contains(tx, ty)) {
        if (state.savedLoops.size() > 1) {
            storageDeleteLoop(state.currentLoop.loopId);
            state.refreshSavedLoops();
            state.loadLoopById(state.savedLoops[0].loopId);
            state.setToast("Deleted loop", 90);
        } else {
            state.setToast("Keep at least 1 loop", 90);
        }
    } else if (btnPrev.contains(tx, ty)) {
        if (state.spellbookPage > 0) state.spellbookPage--;
    } else if (btnNext.contains(tx, ty)) {
        if (state.spellbookPage + 1 < totalPages) state.spellbookPage++;
    }
}

} // namespace BardBeats
