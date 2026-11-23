#include "view_song.h"
#include "audio_engine.h"
#include "mic_recorder.h"
#include "ui_common.h"
#include <cstdio>

namespace BardBeats {

static constexpr uint32_t SECTION_COLORS[6] = {
    C2D_Color32(0x3E, 0x8E, 0xB8, 0xFF),
    C2D_Color32(0x52, 0xA8, 0x58, 0xFF),
    C2D_Color32(0xD9, 0x62, 0x48, 0xFF),
    C2D_Color32(0x9A, 0x5E, 0xC4, 0xFF),
    C2D_Color32(0xC8, 0x9B, 0x3C, 0xFF),
    C2D_Color32(0x6E, 0x62, 0x58, 0xFF)
};

static constexpr int SONGS_PER_PAGE = 4;

static Rect getSongShelfCardRect(int slot) {
    int col = slot % 2;
    int row = slot / 2;
    return { 6.0f + col * 156.0f, 70.0f + row * 50.0f, 152.0f, 46.0f };
}

void viewSongDrawTop(const AppState& state) {
    uiDrawTopHeader(state);
    float d = uiGet3DOffset() * 0.8f;

    C2D_DrawRectSolid(8.0f, 44.0f, 0.05f, 384.0f, 84.0f, Colors::BG_PANEL);
    uiDrawText(14.0f + d * 0.4f, 48.0f, 0.38f, 0.38f, Colors::MUTED_TEXT, "SONG ARRANGEMENT (LIVE CLIPS)");

    int totalBars = state.getTotalSongBars();
    if (totalBars < 1) totalBars = 1;
    int playEntry = audioEngineGetSongEntryIndex();
    int playRep   = audioEngineGetSongRepeatIndex();

    float curX = 14.0f + d;
    float timelineW = 372.0f;

    for (int i = 0; i < state.currentSong.numEntries; ++i) {
        const SongChainEntry& e = state.currentSong.chain[i];
        int reps = (e.repeatCount > 0 ? e.repeatCount : 1);
        int loopBars = state.getLoopNumBarsById(e.loopId);
        int entryBars = reps * loopBars;
        float segW = timelineW * (static_cast<float>(entryBars) / static_cast<float>(totalBars));
        if (segW < 10.0f) segW = 10.0f;

        uint32_t col = SECTION_COLORS[static_cast<int>(e.sectionTag) % 6];
        bool isPlayingSlot = state.isPlaying && (i == playEntry);
        bool isSelected    = (i == state.selectedSongSlot);

        Rect sr = { curX + 1.0f, 66.0f, segW - 2.0f, 54.0f };
        C2D_DrawRectSolid(sr.x, sr.y, 0.1f, sr.w, sr.h, col);
        if (isSelected || isPlayingSlot) {
            uint32_t bCol = isPlayingSlot ? Colors::PLAYHEAD_LINE : Colors::CREAM_TEXT;
            C2D_DrawRectSolid(sr.x, sr.y, 0.25f, sr.w, 2.0f, bCol);
            C2D_DrawRectSolid(sr.x, sr.y + sr.h - 2.0f, 0.25f, sr.w, 2.0f, bCol);
            C2D_DrawRectSolid(sr.x, sr.y, 0.25f, 2.0f, sr.h, bCol);
            C2D_DrawRectSolid(sr.x + sr.w - 2.0f, sr.y, 0.25f, 2.0f, sr.h, bCol);
        }

        if (segW >= 34.0f) {
            char lbl[20];
            std::snprintf(lbl, sizeof(lbl), "%s x%d", getSectionTagName(e.sectionTag), reps);
            uiDrawText(sr.x + 4.0f, sr.y + 6.0f, 0.38f, 0.38f, Colors::CREAM_TEXT, lbl);
            uiDrawText(sr.x + 4.0f, sr.y + 24.0f, 0.38f, 0.38f, Colors::BG_DARK,
                       state.getLoopNameById(e.loopId));
        }

        if (isPlayingSlot) {
            float barFrac = (static_cast<float>(playRep) +
                             static_cast<float>(audioEngineGetCurrentStep()) / static_cast<float>(loopBars * 16)) /
                            static_cast<float>(reps);
            float px = sr.x + sr.w * barFrac;
            C2D_DrawRectSolid(px, sr.y, 0.4f, 2.5f, sr.h, Colors::PLAYHEAD_LINE);
        }
        curX += segW;
    }

    // Vocal Layer Waveform Ribbon
    C2D_DrawRectSolid(8.0f, 136.0f, 0.05f, 384.0f, 66.0f, Colors::BG_PANEL);
    const char* vocStatus = state.isRecordingVocal
        ? "VOCAL LAYER: RECORDING LIVE..."
        : (state.currentSong.hasSongVocal
               ? (state.currentSong.songVocalMuted ? "VOCAL LAYER (MUTED)" : "VOCAL LAYER (ACTIVE OVER SONG)")
               : "VOCAL LAYER: EMPTY (Tap REC VOC to sing over song)");
    uiDrawText(14.0f + d * 0.4f, 140.0f, 0.38f, 0.38f,
               state.isRecordingVocal ? Colors::RED_REC : Colors::GOLD_TEXT, vocStatus);

    float waveX = 14.0f + d * 0.6f;
    float waveW = 372.0f;
    float barW  = waveW / BB_WAVE_BARS;
    float midY  = 178.0f;

    if (state.isRecordingVocal) {
        float prog = micRecorderGetProgress();
        C2D_DrawRectSolid(waveX, 166.0f, 0.1f, waveW, 20.0f, Colors::BG_DARK);
        C2D_DrawRectSolid(waveX, 166.0f, 0.2f, waveW * prog, 20.0f, Colors::RED_REC);
    } else if (state.currentSong.hasSongVocal) {
        uint32_t wCol = state.currentSong.songVocalMuted ? Colors::MUTED_TEXT : Colors::PURPLE_SPELL;
        for (int b = 0; b < BB_WAVE_BARS; ++b) {
            float h = state.vocalWaveBars[b] * 38.0f;
            if (h < 2.0f) h = 2.0f;
            C2D_DrawRectSolid(waveX + b * barW + 1.0f, midY - h * 0.5f, 0.15f, barW - 2.0f, h, wCol);
        }
    } else {
        C2D_DrawRectSolid(waveX, midY, 0.1f, waveW, 1.0f, Colors::BORDER_WOOD);
    }
}

void viewSongDrawBottom(const AppState& state) {
    if (state.showMixDrawer) {
        uiDrawMixDrawer(state);
        return;
    }

    if (state.showSongShelf) {
        // Multiple Songs Shelf (#2)
        int total = static_cast<int>(state.savedSongs.size());
        int totalPages = (total + SONGS_PER_PAGE - 1) / SONGS_PER_PAGE;
        if (totalPages < 1) totalPages = 1;
        int page = state.songShelfPage;
        if (page >= totalPages) page = totalPages - 1;
        if (page < 0) page = 0;

        int startIdx = page * SONGS_PER_PAGE;
        for (int i = 0; i < SONGS_PER_PAGE; ++i) {
            int idx = startIdx + i;
            Rect r = getSongShelfCardRect(i);
            if (idx < total) {
                const SongSummary& s = state.savedSongs[idx];
                bool isCur = (s.songId == state.currentSong.songId);
                uiDrawButton(r, isCur ? Colors::BLUE_ACTION : Colors::BG_CARD,
                             isCur ? Colors::GOLD_TEXT : Colors::BORDER_WOOD, Colors::CREAM_TEXT, nullptr);
                uiDrawText(r.x + 8.0f, r.y + 6.0f, 0.40f, 0.40f, Colors::GOLD_TEXT, s.name);
                char sub[32];
                std::snprintf(sub, sizeof(sub), "%u BPM | %u Clips", s.bpm, s.numEntries);
                uiDrawText(r.x + 8.0f, r.y + 24.0f, 0.38f, 0.38f, Colors::CREAM_TEXT, sub);
            } else {
                uiDrawButton(r, Colors::BG_DARK, Colors::BG_PANEL, Colors::MUTED_TEXT, "(Empty)", 0.38f);
            }
        }

        Rect btnNew  = {  4.0f, 173.0f, 64.0f, 28.0f };
        Rect btnWav  = { 72.0f, 173.0f, 68.0f, 28.0f };
        Rect btnDel  = {144.0f, 173.0f, 48.0f, 28.0f };
        Rect btnPrev = {196.0f, 173.0f, 28.0f, 28.0f };
        Rect btnNext = {228.0f, 173.0f, 28.0f, 28.0f };
        Rect btnBack = {260.0f, 173.0f, 56.0f, 28.0f };

        uiDrawButton(btnNew,  Colors::GREEN_PLAY,  Colors::BORDER_WOOD, Colors::CREAM_TEXT, "+NEW", 0.38f);
        uiDrawButton(btnWav,  Colors::BLUE_ACTION, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "EXP.WAV", 0.38f);
        uiDrawButton(btnDel,  Colors::BG_CARD,     Colors::RED_REC,     Colors::RED_REC,    "DEL", 0.38f);
        uiDrawButton(btnPrev, Colors::BG_CARD,     Colors::BORDER_WOOD, Colors::GOLD_TEXT,  "<", 0.42f);
        uiDrawButton(btnNext, Colors::BG_CARD,     Colors::BORDER_WOOD, Colors::GOLD_TEXT,  ">", 0.42f);
        uiDrawButton(btnBack, Colors::BG_CARD_ALT, Colors::GOLD_TEXT,   Colors::GOLD_TEXT,  "BACK", 0.38f);
        return;
    }

    // Row 1: Master Tempo, Songs Shelf, Rename, Vocal Mute/Clear, Mix (y: 68..92)
    Rect btnBpmDn   = {  4.0f, 68.0f, 34.0f, 24.0f };
    Rect btnBpmUp   = { 40.0f, 68.0f, 34.0f, 24.0f };
    Rect btnSongs   = { 76.0f, 68.0f, 52.0f, 24.0f };
    Rect btnRename  = {130.0f, 68.0f, 44.0f, 24.0f };
    Rect btnVocMute = {176.0f, 68.0f, 54.0f, 24.0f };
    Rect btnVocClr  = {232.0f, 68.0f, 44.0f, 24.0f };
    Rect btnMix     = {278.0f, 68.0f, 38.0f, 24.0f };

    uiDrawButton(btnBpmDn, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "-BM", 0.38f);
    uiDrawButton(btnBpmUp, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "+BM", 0.38f);
    uiDrawButton(btnSongs, Colors::PURPLE_SPELL, Colors::GOLD_TEXT, Colors::CREAM_TEXT, "SONGS", 0.38f);
    uiDrawButton(btnRename, Colors::BG_CARD_ALT, Colors::BORDER_WOOD, Colors::GOLD_TEXT, "NAME", 0.38f);
    uiDrawButton(btnVocMute,
                 state.currentSong.songVocalMuted ? Colors::RED_REC : Colors::BG_CARD,
                 Colors::BORDER_WOOD, Colors::CREAM_TEXT,
                 state.currentSong.songVocalMuted ? "UNMUTE" : "MUTE V", 0.38f);
    uiDrawButton(btnVocClr, Colors::BG_CARD, Colors::RED_REC, Colors::RED_REC, "CLRV", 0.38f);
    uiDrawButton(btnMix, Colors::BG_CARD_ALT, Colors::GOLD_TEXT, Colors::GOLD_TEXT, "MIX", 0.38f);

    // Row 2: Selected Clip Card + Prev/Next Slot Selector (y: 96..158)
    Rect btnPrevSlot = {  4.0f, 98.0f, 36.0f, 58.0f };
    Rect cardRect    = { 44.0f, 98.0f, 232.0f, 58.0f };
    Rect btnNextSlot = {280.0f, 98.0f, 36.0f, 58.0f };

    uiDrawButton(btnPrevSlot, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::GOLD_TEXT, "<", 0.55f);
    uiDrawButton(btnNextSlot, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::GOLD_TEXT, ">", 0.55f);

    if (state.currentSong.numEntries > 0 && state.selectedSongSlot < state.currentSong.numEntries) {
        const SongChainEntry& e = state.currentSong.chain[state.selectedSongSlot];
        uint32_t secCol = SECTION_COLORS[static_cast<int>(e.sectionTag) % 6];
        uiDrawButton(cardRect, Colors::BG_CARD_ALT, secCol, Colors::CREAM_TEXT, nullptr);

        char header[48];
        std::snprintf(header, sizeof(header), "Slot %d/%d: [%s] x%d",
                      state.selectedSongSlot + 1, state.currentSong.numEntries,
                      getSectionTagName(e.sectionTag), e.repeatCount);
        uiDrawText(cardRect.x + 8.0f, cardRect.y + 4.0f, 0.38f, 0.38f, Colors::GOLD_TEXT, header);

        char loopLbl[48];
        std::snprintf(loopLbl, sizeof(loopLbl), "Loop: %s", state.getLoopNameById(e.loopId));
        uiDrawText(cardRect.x + 8.0f, cardRect.y + 18.0f, 0.38f, 0.38f, Colors::CREAM_TEXT, loopLbl);

        Rect btnLpDn  = { cardRect.x +   8.0f, cardRect.y + 34.0f, 44.0f, 20.0f };
        Rect btnLpUp  = { cardRect.x +  56.0f, cardRect.y + 34.0f, 44.0f, 20.0f };
        Rect btnRepDn = { cardRect.x + 150.0f, cardRect.y + 34.0f, 34.0f, 20.0f };
        Rect btnRepUp = { cardRect.x + 188.0f, cardRect.y + 34.0f, 34.0f, 20.0f };
        uiDrawButton(btnLpDn,  Colors::BG_DARK, Colors::BORDER_WOOD, Colors::GOLD_TEXT,  "<LP", 0.38f);
        uiDrawButton(btnLpUp,  Colors::BG_DARK, Colors::BORDER_WOOD, Colors::GOLD_TEXT,  "LP>", 0.38f);
        uiDrawButton(btnRepDn, Colors::BG_DARK, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "-x",  0.38f);
        uiDrawButton(btnRepUp, Colors::BG_DARK, Colors::BORDER_WOOD, Colors::CREAM_TEXT, "+x",  0.38f);
    } else {
        uiDrawButton(cardRect, Colors::BG_PANEL, Colors::BORDER_WOOD, Colors::MUTED_TEXT,
                     "No clips yet - Tap +ADD", 0.40f);
    }

    // Row 3: Clip Actions (y: 164..200)
    Rect btnAdd   = {  4.0f, 164.0f, 54.0f, 36.0f };
    Rect btnCopy  = { 61.0f, 164.0f, 76.0f, 36.0f };
    Rect btnEdit  = {140.0f, 164.0f, 70.0f, 36.0f };
    Rect btnSec   = {213.0f, 164.0f, 56.0f, 36.0f };
    Rect btnDel   = {272.0f, 164.0f, 44.0f, 36.0f };

    uiDrawButton(btnAdd,  Colors::GREEN_PLAY,   Colors::BORDER_WOOD, Colors::CREAM_TEXT, "+ADD", 0.38f);
    uiDrawButton(btnCopy, Colors::AMBER_JAM,    Colors::BORDER_WOOD, Colors::BG_DARK,    "COPY+TWK", 0.38f);
    uiDrawButton(btnEdit, Colors::BLUE_ACTION,  Colors::GOLD_TEXT,   Colors::CREAM_TEXT, "EDIT LP", 0.38f);
    uiDrawButton(btnSec,  Colors::BG_CARD_ALT,  Colors::BORDER_WOOD, Colors::GOLD_TEXT,  "SECT", 0.38f);
    uiDrawButton(btnDel,  Colors::BG_CARD,      Colors::RED_REC,     Colors::RED_REC,    "DEL", 0.38f);
}

void viewSongHandleTouch(AppState& state, float tx, float ty, bool isJustPressed) {
    if (state.showMixDrawer) {
        uiHandleMixDrawerTouch(state, tx, ty, isJustPressed, false);
        return;
    }
    if (!isJustPressed) return;

    if (state.showSongShelf) {
        int total = static_cast<int>(state.savedSongs.size());
        int totalPages = (total + SONGS_PER_PAGE - 1) / SONGS_PER_PAGE;
        if (totalPages < 1) totalPages = 1;

        for (int i = 0; i < SONGS_PER_PAGE; ++i) {
            int idx = state.songShelfPage * SONGS_PER_PAGE + i;
            if (idx < total && getSongShelfCardRect(i).contains(tx, ty)) {
                state.loadSongById(state.savedSongs[idx].songId);
                return;
            }
        }

        Rect btnNew  = {  4.0f, 173.0f, 64.0f, 28.0f };
        Rect btnWav  = { 72.0f, 173.0f, 68.0f, 28.0f };
        Rect btnDel  = {144.0f, 173.0f, 48.0f, 28.0f };
        Rect btnPrev = {196.0f, 173.0f, 28.0f, 28.0f };
        Rect btnNext = {228.0f, 173.0f, 28.0f, 28.0f };
        Rect btnBack = {260.0f, 173.0f, 56.0f, 28.0f };

        if (btnNew.contains(tx, ty))       state.createNewSong();
        else if (btnWav.contains(tx, ty))  state.exportCurrentSongWav();
        else if (btnDel.contains(tx, ty))  state.deleteSongById(state.currentSong.songId);
        else if (btnPrev.contains(tx, ty)) { if (state.songShelfPage > 0) state.songShelfPage--; }
        else if (btnNext.contains(tx, ty)) { if (state.songShelfPage + 1 < totalPages) state.songShelfPage++; }
        else if (btnBack.contains(tx, ty)) state.showSongShelf = false;
        return;
    }

    Rect btnBpmDn   = {  4.0f, 68.0f, 34.0f, 24.0f };
    Rect btnBpmUp   = { 40.0f, 68.0f, 34.0f, 24.0f };
    Rect btnSongs   = { 76.0f, 68.0f, 52.0f, 24.0f };
    Rect btnRename  = {130.0f, 68.0f, 44.0f, 24.0f };
    Rect btnVocMute = {176.0f, 68.0f, 54.0f, 24.0f };
    Rect btnVocClr  = {232.0f, 68.0f, 44.0f, 24.0f };
    Rect btnMix     = {278.0f, 68.0f, 38.0f, 24.0f };

    if (btnBpmDn.contains(tx, ty)) {
        if (state.currentSong.bpm > 60) state.currentSong.bpm -= 4;
        state.syncSongToAudio();
        return;
    }
    if (btnBpmUp.contains(tx, ty)) {
        if (state.currentSong.bpm < 180) state.currentSong.bpm += 4;
        state.syncSongToAudio();
        return;
    }
    if (btnSongs.contains(tx, ty)) {
        state.refreshSavedSongs();
        state.showSongShelf = true;
        return;
    }
    if (btnRename.contains(tx, ty)) {
        state.renameCurrentSong();
        return;
    }
    if (btnVocMute.contains(tx, ty)) {
        state.toggleVocalMute(true);
        return;
    }
    if (btnVocClr.contains(tx, ty)) {
        state.clearVocalTake(true);
        return;
    }
    if (btnMix.contains(tx, ty)) {
        state.showMixDrawer = true;
        return;
    }

    Rect btnPrevSlot = {  4.0f, 98.0f, 36.0f, 58.0f };
    Rect cardRect    = { 44.0f, 98.0f, 232.0f, 58.0f };
    Rect btnNextSlot = {280.0f, 98.0f, 36.0f, 58.0f };
    Rect btnLpDn     = { cardRect.x +   8.0f, cardRect.y + 34.0f, 44.0f, 20.0f };
    Rect btnLpUp     = { cardRect.x +  56.0f, cardRect.y + 34.0f, 44.0f, 20.0f };
    Rect btnRepDn    = { cardRect.x + 150.0f, cardRect.y + 34.0f, 34.0f, 20.0f };
    Rect btnRepUp    = { cardRect.x + 188.0f, cardRect.y + 34.0f, 34.0f, 20.0f };

    if (btnPrevSlot.contains(tx, ty) && state.selectedSongSlot > 0) {
        state.selectedSongSlot--;
        return;
    }
    if (btnNextSlot.contains(tx, ty) && state.selectedSongSlot + 1 < state.currentSong.numEntries) {
        state.selectedSongSlot++;
        return;
    }
    if (state.currentSong.numEntries > 0 && state.selectedSongSlot < state.currentSong.numEntries) {
        SongChainEntry& e = state.currentSong.chain[state.selectedSongSlot];
        if (btnLpDn.contains(tx, ty)) {
            state.cycleSongSlotLoop(state.selectedSongSlot, -1);
            return;
        }
        if (btnLpUp.contains(tx, ty)) {
            state.cycleSongSlotLoop(state.selectedSongSlot, 1);
            return;
        }
        if (btnRepDn.contains(tx, ty)) {
            if (e.repeatCount > 1) e.repeatCount--;
            state.syncSongToAudio();
            return;
        }
        if (btnRepUp.contains(tx, ty)) {
            if (e.repeatCount < 16) e.repeatCount++;
            state.syncSongToAudio();
            return;
        }
    }

    Rect btnAdd   = {  4.0f, 164.0f, 54.0f, 36.0f };
    Rect btnCopy  = { 61.0f, 164.0f, 76.0f, 36.0f };
    Rect btnEdit  = {140.0f, 164.0f, 70.0f, 36.0f };
    Rect btnSec   = {213.0f, 164.0f, 56.0f, 36.0f };
    Rect btnDel   = {272.0f, 164.0f, 44.0f, 36.0f };

    if (btnAdd.contains(tx, ty))       state.addCurrentLoopToSong();
    else if (btnCopy.contains(tx, ty)) state.copyAndTweakSongSlot(state.selectedSongSlot);
    else if (btnEdit.contains(tx, ty)) state.editSongSlotLoop(state.selectedSongSlot);
    else if (btnSec.contains(tx, ty)) {
        if (state.currentSong.numEntries > 0 && state.selectedSongSlot < state.currentSong.numEntries) {
            SongChainEntry& e = state.currentSong.chain[state.selectedSongSlot];
            e.sectionTag = static_cast<SectionTag>((static_cast<int>(e.sectionTag) + 1) % 6);
            state.syncSongToAudio();
        }
    } else if (btnDel.contains(tx, ty)) state.deleteSongSlot(state.selectedSongSlot);
}

} // namespace BardBeats
