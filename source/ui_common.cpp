#include "ui_common.h"
#include "audio_engine.h"
#include "mic_recorder.h"
#include <cstdio>

namespace BardBeats {

static C2D_TextBuf s_textBuf = nullptr;

void uiInit() {
    s_textBuf = C2D_TextBufNew(4096);
}

void uiExit() {
    if (s_textBuf) {
        C2D_TextBufDelete(s_textBuf);
        s_textBuf = nullptr;
    }
}

void uiBeginFrame() {
    if (s_textBuf) {
        C2D_TextBufClear(s_textBuf);
    }
}

void uiDrawText(float x, float y, float scaleX, float scaleY, uint32_t color, const char* text) {
    if (!s_textBuf || !text) return;
    C2D_Text t;
    C2D_TextParse(&t, s_textBuf, text);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, scaleX, scaleY, color);
}

void uiDrawTextCentered(const Rect& r, float scale, uint32_t color, const char* text) {
    if (!s_textBuf || !text) return;
    C2D_Text t;
    C2D_TextParse(&t, s_textBuf, text);
    C2D_TextOptimize(&t);
    float tw = 0.0f, th = 0.0f;
    C2D_TextGetDimensions(&t, scale, scale, &tw, &th);
    float tx = r.x + (r.w - tw) * 0.5f;
    float ty = r.y + (r.h - th) * 0.5f;
    C2D_DrawText(&t, C2D_WithColor, tx, ty, 0.5f, scale, scale, color);
}

void uiDrawButton(const Rect& r, uint32_t bgColor, uint32_t borderColor,
                  uint32_t textColor, const char* label, float textScale) {
    C2D_DrawRectSolid(r.x, r.y, 0.1f, r.w, r.h, bgColor);
    C2D_DrawRectSolid(r.x, r.y, 0.2f, r.w, 1.0f, borderColor);
    C2D_DrawRectSolid(r.x, r.y + r.h - 1.0f, 0.2f, r.w, 1.0f, borderColor);
    C2D_DrawRectSolid(r.x, r.y, 0.2f, 1.0f, r.h, borderColor);
    C2D_DrawRectSolid(r.x + r.w - 1.0f, r.y, 0.2f, 1.0f, r.h, borderColor);
    if (label) {
        uiDrawTextCentered(r, textScale, textColor, label);
    }
}

void uiDrawTopHeader(const AppState& state) {
    C2D_DrawRectSolid(0, 0, 0.1f, 400.0f, 34.0f, Colors::BG_PANEL);
    C2D_DrawRectSolid(0, 34.0f, 0.2f, 400.0f, 2.0f, Colors::BORDER_WOOD);

    bool inSong = (state.activeView == VIEW_SONG);
    const char* title = inSong ? state.currentSong.name : state.currentLoop.name;
    uiDrawText(10.0f, 7.0f, 0.50f, 0.50f, Colors::GOLD_TEXT, title);

    char infoBuf[48];
    if (inSong) {
        std::snprintf(infoBuf, sizeof(infoBuf), "%u BPM | %d Bars",
                      state.currentSong.bpm, state.getTotalSongBars());
    } else {
        std::snprintf(infoBuf, sizeof(infoBuf), "%u BPM | %s",
                      state.currentLoop.bpm, getScaleName(state.currentLoop.scaleId));
    }
    uiDrawText(186.0f, 9.0f, 0.43f, 0.43f, Colors::CREAM_TEXT, infoBuf);

    // Current Step or Mic Rec indicator
    if (state.isRecordingVocal) {
        float lvl = micRecorderGetLiveLevel();
        C2D_DrawRectSolid(314.0f, 6.0f, 0.25f, 78.0f, 22.0f, Colors::RED_REC);
        C2D_DrawRectSolid(316.0f, 24.0f, 0.3f, 74.0f * lvl, 3.0f, Colors::PLAYHEAD_LINE);
        uiDrawTextCentered({314.0f, 5.0f, 78.0f, 18.0f}, 0.38f, Colors::CREAM_TEXT, "MIC REC");
    } else {
        int step = audioEngineGetCurrentStep();
        char stepBuf[16];
        std::snprintf(stepBuf, sizeof(stepBuf), "STEP %02d", step + 1);
        uint32_t stepCol = state.isPlaying ? Colors::PLAYHEAD_LINE : Colors::MUTED_TEXT;
        uiDrawText(325.0f, 9.0f, 0.45f, 0.45f, stepCol, stepBuf);
    }

    // Toast or DSP Warning Banner
    if (!audioEngineIsDspReady()) {
        C2D_DrawRectSolid(0, 218.0f, 0.8f, 400.0f, 22.0f, Colors::RED_REC);
        uiDrawText(12.0f, 222.0f, 0.42f, 0.42f, Colors::CREAM_TEXT,
                   "DSP firmware missing: place dspfirm.cdc in sdmc:/3ds/");
    } else if (state.toastFrames > 0) {
        C2D_DrawRectSolid(40.0f, 212.0f, 0.8f, 320.0f, 22.0f, Colors::BG_CARD_ALT);
        uiDrawTextCentered({40.0f, 212.0f, 320.0f, 22.0f}, 0.40f, Colors::GOLD_TEXT, state.toastMsg);
    }
}

void uiDrawTopFourTrackOverview(const AppState& state, float yStart, float height) {
    float rowH = height / BB_NUM_TRACKS;
    float labelW = 76.0f;
    float gridW = 400.0f - labelW - 10.0f;
    float stepW = gridW / BB_STEPS_PER_LOOP;
    int curStep = audioEngineGetCurrentStep();

    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        float y = yStart + t * rowH;
        bool isSel = (t == state.activeTrack);
        uint32_t rowBg = isSel ? Colors::BG_CARD_ALT : Colors::BG_PANEL;
        C2D_DrawRectSolid(4.0f, y + 2.0f, 0.1f, 392.0f, rowH - 4.0f, rowBg);

        uint32_t nameCol = state.currentLoop.tracks[t].muted ? Colors::MUTED_TEXT : Colors::TRACK_COLORS[t];
        uiDrawText(10.0f, y + 6.0f, 0.42f, 0.42f, nameCol, getTrackRoleName(t));
        uiDrawText(10.0f, y + 22.0f, 0.36f, 0.36f, Colors::MUTED_TEXT,
                   getInstrumentShortName(state.currentLoop.tracks[t].instrumentId));

        for (int s = 0; s < BB_STEPS_PER_LOOP; ++s) {
            float sx = labelW + s * stepW;
            bool hasNote = stepHasAnyNote(state.currentLoop.tracks[t].steps[s]);
            uint32_t cellCol = hasNote
                ? (state.currentLoop.tracks[t].muted ? Colors::MUTED_TEXT : Colors::TRACK_COLORS[t])
                : ((s % 4 == 0) ? Colors::BG_CARD : Colors::BG_DARK);
            C2D_DrawRectSolid(sx + 1.0f, y + 5.0f, 0.2f, stepW - 2.0f, rowH - 10.0f, cellCol);
        }
    }

    if (state.isPlaying) {
        float px = labelW + curStep * stepW;
        C2D_DrawRectSolid(px, yStart, 0.4f, stepW, height, C2D_Color32(0xFF, 0xE0, 0x66, 0x44));
        C2D_DrawRectSolid(px, yStart, 0.5f, 2.0f, height, Colors::PLAYHEAD_LINE);
    }
}

// Persistent Bottom-Screen Layout Constants
static constexpr Rect BTN_PLAY  = {  4.0f, 2.0f, 74.0f, 32.0f };
static constexpr Rect BTN_REC   = { 82.0f, 2.0f, 76.0f, 32.0f };
static constexpr Rect BTN_UNDO  = {162.0f, 2.0f, 74.0f, 32.0f };
static constexpr Rect BTN_SAVE  = {240.0f, 2.0f, 76.0f, 32.0f };
static constexpr Rect BTN_MIC   = {264.0f, 38.0f, 52.0f, 26.0f };

void uiDrawBottomChrome(const AppState& state) {
    bool inSong = (state.activeView == VIEW_SONG);

    // Top Transport Bar (y: 0..36)
    C2D_DrawRectSolid(0, 0, 0.05f, 320.0f, 36.0f, Colors::BG_PANEL);

    uiDrawButton(BTN_PLAY,
                 state.isPlaying ? Colors::GREEN_PLAY : Colors::BG_CARD,
                 Colors::BORDER_WOOD, Colors::CREAM_TEXT,
                 state.isPlaying ? "STOP" : "PLAY", 0.46f);

    if (inSong) {
        uiDrawButton(BTN_REC,
                     state.isRecordingVocal ? Colors::RED_REC : Colors::PURPLE_SPELL,
                     Colors::BORDER_WOOD, Colors::CREAM_TEXT,
                     state.isRecordingVocal ? "STOP MIC" : "REC VOC", 0.40f);
    } else {
        uiDrawButton(BTN_REC,
                     state.isRecording ? Colors::RED_REC : Colors::AMBER_JAM,
                     Colors::BORDER_WOOD, Colors::CREAM_TEXT,
                     state.isRecording ? "REC ON" : "JAM", 0.45f);
    }

    bool canUndo = state.hasUndo || state.hasVocalUndo;
    uiDrawButton(BTN_UNDO,
                 canUndo ? Colors::BLUE_ACTION : Colors::BG_DARK,
                 Colors::BORDER_WOOD,
                 canUndo ? Colors::CREAM_TEXT : Colors::MUTED_TEXT,
                 "UNDO", 0.45f);

    if (state.editingFromSong && !inSong) {
        uiDrawButton(BTN_SAVE, Colors::PURPLE_SPELL, Colors::GOLD_TEXT, Colors::GOLD_TEXT, "< SONG", 0.42f);
    } else {
        uiDrawButton(BTN_SAVE, Colors::BG_CARD_ALT, Colors::GOLD_TEXT, Colors::GOLD_TEXT, "SAVE", 0.45f);
    }

    // Track Selector Bar + Per-Clip MIC Button (y: 36..66)
    C2D_DrawRectSolid(0, 36.0f, 0.05f, 320.0f, 30.0f, Colors::BG_DARK);
    float tabW = 63.0f;
    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        Rect tr = { 4.0f + t * 65.0f, 38.0f, tabW, 26.0f };
        bool active = (t == state.activeTrack) && !inSong;
        bool muted = (state.currentLoop.tracks[t].muted != 0);
        uint32_t bg = active ? Colors::BG_CARD_ALT : Colors::BG_PANEL;
        uint32_t border = active ? Colors::TRACK_COLORS[t] : Colors::BORDER_WOOD;
        uint32_t txtCol = muted ? Colors::MUTED_TEXT : (active ? Colors::CREAM_TEXT : Colors::TRACK_COLORS[t]);

        char label[20];
        std::snprintf(label, sizeof(label), "%s%s",
                      muted ? "M:" : "",
                      getInstrumentShortName(state.currentLoop.tracks[t].instrumentId));
        uiDrawButton(tr, bg, border, txtCol, label, 0.35f);
    }

    // Right-most Vocal / Mic button in Track Bar
    bool hasVoc = inSong ? (state.currentSong.hasSongVocal != 0) : (state.currentLoop.hasVocalClip != 0);
    bool vocMuted = inSong ? (state.currentSong.songVocalMuted != 0) : (state.currentLoop.vocalMuted != 0);
    uint32_t micBg = state.isRecordingVocal ? Colors::RED_REC
                                            : (hasVoc ? (vocMuted ? Colors::BG_CARD : Colors::PURPLE_SPELL) : Colors::BG_PANEL);
    const char* micLbl = state.isRecordingVocal ? "STOP" : (hasVoc ? (vocMuted ? "VOC:M" : "VOC") : "MIC");
    uiDrawButton(BTN_MIC, micBg, hasVoc ? Colors::GOLD_TEXT : Colors::BORDER_WOOD, Colors::CREAM_TEXT, micLbl, 0.36f);

    // Bottom Navigation Tabs (y: 206..240)
    C2D_DrawRectSolid(0, 206.0f, 0.05f, 320.0f, 34.0f, Colors::BG_PANEL);
    static const char* TAB_NAMES[5] = { "GRID", "PADS", "INST", "SPELL", "SONG" };
    for (int i = 0; i < 5; ++i) {
        Rect nr = { 2.0f + i * 63.5f, 209.0f, 61.5f, 29.0f };
        bool sel = (static_cast<int>(state.activeView) == i);
        uint32_t bg = sel ? (i == VIEW_SPELLBOOK ? Colors::PURPLE_SPELL : Colors::BLUE_ACTION) : Colors::BG_CARD;
        uiDrawButton(nr, bg, sel ? Colors::GOLD_TEXT : Colors::BORDER_WOOD,
                     Colors::CREAM_TEXT, TAB_NAMES[i], 0.40f);
    }
}

bool uiHandleBottomChromeTouch(AppState& state, float tx, float ty) {
    bool inSong = (state.activeView == VIEW_SONG);

    // Transport Bar
    if (BTN_PLAY.contains(tx, ty)) {
        state.togglePlay();
        return true;
    }
    if (BTN_REC.contains(tx, ty)) {
        state.toggleRecord();
        return true;
    }
    if (BTN_UNDO.contains(tx, ty)) {
        state.performUndo();
        return true;
    }
    if (BTN_SAVE.contains(tx, ty)) {
        if (state.editingFromSong && !inSong) {
            state.returnToSongFromEdit();
        } else if (inSong) {
            state.saveCurrentSong();
        } else {
            state.saveCurrentLoop();
        }
        return true;
    }

    // Track Selector Bar + MIC Button (y: 36..66)
    if (ty >= 36.0f && ty < 66.0f) {
        if (BTN_MIC.contains(tx, ty)) {
            if (state.isRecordingVocal) {
                state.stopVocalRecording(true);
            } else {
                state.startVocalRecording(inSong);
            }
            return true;
        }
        if (!inSong) {
            for (int t = 0; t < BB_NUM_TRACKS; ++t) {
                Rect tr = { 4.0f + t * 65.0f, 38.0f, 63.0f, 26.0f };
                if (tr.contains(tx, ty)) {
                    if (state.activeTrack == t) {
                        uint8_t nextInst = cycleTrackInstrument(t, state.currentLoop.tracks[t].instrumentId);
                        state.currentLoop.tracks[t].instrumentId = nextInst;
                        state.syncAudioLoop();
                        char buf[48];
                        std::snprintf(buf, sizeof(buf), "%s: %s", getTrackRoleName(t), getInstrumentName(nextInst));
                        state.setToast(buf, 80);
                    } else {
                        state.activeTrack = t;
                    }
                    return true;
                }
            }
        }
    }

    // Bottom Navigation Bar (y: 206..240)
    if (ty >= 206.0f) {
        for (int i = 0; i < 5; ++i) {
            Rect nr = { 2.0f + i * 63.5f, 209.0f, 61.5f, 29.0f };
            if (nr.contains(tx, ty)) {
                state.switchView(static_cast<ViewMode>(i));
                return true;
            }
        }
    }
    return false;
}

} // namespace BardBeats
