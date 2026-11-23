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

    char info[80];
    std::snprintf(info, sizeof(info), "Spellbook: %zu loops | Cue: %s%s",
                  state.savedLoops.size(),
                  state.spellbookQuantizedCue ? "Next Bar" : "Instant",
                  audioEngineHasQueuedLoop() ? " (QUEUED)" : "");
    uiDrawText(10.0f, 198.0f, 0.40f, 0.40f, Colors::GOLD_TEXT, info);
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
            char sub[36];
            std::snprintf(sub, sizeof(sub), "%u BPM | %s%s",
                          item.bpm, getScaleName(item.scaleId),
                          item.numBars == 2 ? " 2B" : "");
            uiDrawText(r.x + 8.0f, r.y + 24.0f, 0.38f, 0.38f, Colors::CREAM_TEXT, sub);
        } else {
            uiDrawButton(r, Colors::BG_DARK, Colors::BG_PANEL, Colors::MUTED_TEXT, "(Empty)", 0.38f);
        }
    }

    // Bottom Action Row (y: 173..201)
    Rect btnRename = {  4.0f, 173.0f, 48.0f, 28.0f };
    Rect btnCopy   = { 54.0f, 173.0f, 38.0f, 28.0f };
    Rect btnCue    = { 94.0f, 173.0f, 52.0f, 28.0f };
    Rect btnMidi   = {148.0f, 173.0f, 44.0f, 28.0f };
    Rect btnWav    = {194.0f, 173.0f, 46.0f, 28.0f };
    Rect btnDel    = {242.0f, 173.0f, 28.0f, 28.0f };
    Rect btnPrev   = {272.0f, 173.0f, 22.0f, 28.0f };
    Rect btnNext   = {296.0f, 173.0f, 21.0f, 28.0f };

    uiDrawButton(btnRename, Colors::BG_CARD_ALT, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "NAME", 0.38f);
    uiDrawButton(btnCopy,   Colors::BG_CARD_ALT, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "COPY", 0.38f);
    uiDrawButton(btnCue,
                 state.spellbookQuantizedCue ? Colors::AMBER_JAM : Colors::PURPLE_SPELL,
                 Colors::BORDER_WOOD,
                 state.spellbookQuantizedCue ? Colors::BG_DARK : Colors::CREAM_TEXT,
                 state.spellbookQuantizedCue ? "Q:BAR" : "Q:NOW", 0.38f);
    uiDrawButton(btnMidi,   Colors::BLUE_ACTION, Colors::BORDER_WOOD, Colors::CREAM_TEXT, ".MID", 0.38f);
    uiDrawButton(btnWav,    Colors::GREEN_PLAY,  Colors::BORDER_WOOD, Colors::CREAM_TEXT, ".WAV", 0.38f);
    uiDrawButton(btnDel,    Colors::BG_CARD,     Colors::RED_REC,     Colors::RED_REC,    "DEL", 0.38f);
    uiDrawButton(btnPrev,   Colors::BG_CARD,     Colors::BORDER_WOOD, Colors::GOLD_TEXT,  "<", 0.40f);
    uiDrawButton(btnNext,   Colors::BG_CARD,     Colors::BORDER_WOOD, Colors::GOLD_TEXT,  ">", 0.40f);
}

void viewSpellbookHandleTouch(AppState& state, float tx, float ty, bool isJustPressed) {
    if (!isJustPressed) return;

    int total = static_cast<int>(state.savedLoops.size());
    int totalPages = (total + CARDS_PER_PAGE - 1) / CARDS_PER_PAGE;
    if (totalPages < 1) totalPages = 1;

    for (int i = 0; i < CARDS_PER_PAGE; ++i) {
        int idx = state.spellbookPage * CARDS_PER_PAGE + i;
        if (idx < total && getCardRect(i).contains(tx, ty)) {
            state.cueLoopFromSpellbook(state.savedLoops[idx].loopId);
            return;
        }
    }

    Rect btnRename = {  4.0f, 173.0f, 48.0f, 28.0f };
    Rect btnCopy   = { 54.0f, 173.0f, 38.0f, 28.0f };
    Rect btnCue    = { 94.0f, 173.0f, 52.0f, 28.0f };
    Rect btnMidi   = {148.0f, 173.0f, 44.0f, 28.0f };
    Rect btnWav    = {194.0f, 173.0f, 46.0f, 28.0f };
    Rect btnDel    = {242.0f, 173.0f, 28.0f, 28.0f };
    Rect btnPrev   = {272.0f, 173.0f, 22.0f, 28.0f };
    Rect btnNext   = {296.0f, 173.0f, 21.0f, 28.0f };

    if (btnRename.contains(tx, ty)) {
        state.renameCurrentLoop();
    } else if (btnCopy.contains(tx, ty)) {
        state.copyCurrentLoop();
    } else if (btnCue.contains(tx, ty)) {
        state.spellbookQuantizedCue = !state.spellbookQuantizedCue;
        state.setToast(state.spellbookQuantizedCue ? "Cue: Switches on Next Bar" : "Cue: Instant Trigger", 80);
    } else if (btnMidi.contains(tx, ty)) {
        char path[256];
        if (storageExportLoopMidi(state.currentLoop, path, sizeof(path))) {
            state.setToast("Exported .mid to exports/", 110);
        } else {
            state.setToast("MIDI Export Failed!", 110);
        }
    } else if (btnWav.contains(tx, ty)) {
        state.exportCurrentLoopWav();
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
