#include "ui_common.h"
#include "audio_engine.h"
#include "mic_recorder.h"
#include <cstdio>

namespace BardBeats {

static C2D_TextBuf s_textBuf = nullptr;
static C2D_SpriteSheet s_spriteSheet = nullptr;
static C2D_SpriteSheet s_spriteSheet2 = nullptr;
static float s_3dOffset = 0.0f;

void uiInit() {
    s_textBuf = C2D_TextBufNew(4096);
    s_spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
    s_spriteSheet2 = C2D_SpriteSheetLoad("romfs:/gfx/sprites2.t3x");
}

void uiExit() {
    if (s_spriteSheet2) {
        C2D_SpriteSheetFree(s_spriteSheet2);
        s_spriteSheet2 = nullptr;
    }
    if (s_spriteSheet) {
        C2D_SpriteSheetFree(s_spriteSheet);
        s_spriteSheet = nullptr;
    }
    if (s_textBuf) {
        C2D_TextBufDelete(s_textBuf);
        s_textBuf = nullptr;
    }
}

C2D_SpriteSheet uiGetSpriteSheet() {
    return s_spriteSheet;
}

C2D_SpriteSheet uiGetSpriteSheet2() {
    return s_spriteSheet2;
}

void uiSet3DOffset(float offsetPx) {
    s_3dOffset = offsetPx;
}

float uiGet3DOffset() {
    return s_3dOffset;
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
    float d = s_3dOffset * 0.4f;
    C2D_DrawRectSolid(0, 0, 0.1f, 400.0f, 34.0f, Colors::BG_PANEL);
    C2D_DrawRectSolid(0, 34.0f, 0.2f, 400.0f, 2.0f, Colors::BORDER_WOOD);

    bool inSong = (state.activeView == VIEW_SONG);
    const char* title = inSong ? state.currentSong.name : state.currentLoop.name;
    uiDrawText(10.0f + d, 7.0f, 0.50f, 0.50f, Colors::GOLD_TEXT, title);

    char infoBuf[48];
    if (inSong) {
        std::snprintf(infoBuf, sizeof(infoBuf), "%u BPM | %d Bars",
                      state.currentSong.bpm, state.getTotalSongBars());
    } else {
        std::snprintf(infoBuf, sizeof(infoBuf), "%u BPM | %s%s",
                      state.currentLoop.bpm, getScaleName(state.currentLoop.scaleId),
                      state.currentLoop.numBars == 2 ? " 2B" : "");
    }
    uiDrawText(186.0f + d, 9.0f, 0.43f, 0.43f, Colors::CREAM_TEXT, infoBuf);

    if (state.isRecordingVocal) {
        float lvl = micRecorderGetLiveLevel();
        C2D_DrawRectSolid(314.0f + d, 6.0f, 0.25f, 78.0f, 22.0f, Colors::RED_REC);
        C2D_DrawRectSolid(316.0f + d, 24.0f, 0.3f, 74.0f * lvl, 3.0f, Colors::PLAYHEAD_LINE);
        uiDrawTextCentered({314.0f + d, 5.0f, 78.0f, 18.0f}, 0.38f, Colors::CREAM_TEXT, "MIC REC");
    } else {
        int step = audioEngineGetCurrentStep();
        char stepBuf[24];
        std::snprintf(stepBuf, sizeof(stepBuf), "STEP %02d", step + 1);
        uint32_t stepCol = state.isPlaying ? Colors::PLAYHEAD_LINE : Colors::MUTED_TEXT;
        uiDrawText(325.0f + d, 6.0f, 0.42f, 0.42f, stepCol, stepBuf);
    }

    // Live VU Pulse Meter (#17)
    float peak = audioEngineGetPeakLevel();
    if (peak > 1.0f) peak = 1.0f;
    if (peak < 0.0f) peak = 0.0f;
    C2D_DrawRectSolid(325.0f + d, 24.0f, 0.3f, 66.0f, 5.0f, Colors::BG_DARK);
    if (peak > 0.01f) {
        uint32_t vuCol = (peak > 0.82f) ? Colors::RED_REC : (peak > 0.45f ? Colors::GOLD_TEXT : Colors::GREEN_PLAY);
        C2D_DrawRectSolid(325.0f + d, 24.0f, 0.35f, 66.0f * peak, 5.0f, vuCol);
    }

    if (!audioEngineIsDspReady()) {
        C2D_DrawRectSolid(0, 218.0f, 0.8f, 400.0f, 22.0f, Colors::RED_REC);
        uiDrawText(12.0f, 222.0f, 0.42f, 0.42f, Colors::CREAM_TEXT,
                   "DSP firmware missing: place dspfirm.cdc in sdmc:/3ds/");
    } else if (state.toastFrames > 0) {
        C2D_DrawRectSolid(40.0f + d * 1.4f, 212.0f, 0.8f, 320.0f, 22.0f, Colors::BG_CARD_ALT);
        uiDrawTextCentered({40.0f + d * 1.4f, 212.0f, 320.0f, 22.0f}, 0.40f, Colors::GOLD_TEXT, state.toastMsg);
    }
}

void uiDrawTopFourTrackOverview(const AppState& state, float yStart, float height) {
    float d = s_3dOffset * 0.6f;
    float rowH = height / BB_NUM_TRACKS;
    float labelW = 76.0f;
    float gridW = 400.0f - labelW - 10.0f;
    int totalSteps = (state.currentLoop.numBars == 2) ? BB_MAX_STEPS : BB_STEPS_PER_LOOP;
    float stepW = gridW / static_cast<float>(totalSteps);
    int curStep = audioEngineGetCurrentStep();
    if (curStep >= totalSteps) curStep = 0;

    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        float y = yStart + t * rowH;
        bool isSel = (t == state.activeTrack);
        uint32_t rowBg = isSel ? Colors::BG_CARD_ALT : Colors::BG_PANEL;
        C2D_DrawRectSolid(4.0f, y + 2.0f, 0.1f, 392.0f, rowH - 4.0f, rowBg);

        uint32_t nameCol = state.currentLoop.tracks[t].muted ? Colors::MUTED_TEXT : Colors::TRACK_COLORS[t];
        uiDrawText(10.0f + d * 0.5f, y + 6.0f, 0.42f, 0.42f, nameCol, getTrackRoleName(t));
        uiDrawText(10.0f + d * 0.5f, y + 21.0f, 0.40f, 0.40f, Colors::MUTED_TEXT,
                   getInstrumentShortName(state.currentLoop.tracks[t].instrumentId));

        for (int s = 0; s < totalSteps; ++s) {
            float sx = labelW + s * stepW + d * 0.5f;
            bool hasNote = stepHasAnyNote(state.currentLoop.tracks[t].steps[s]);
            bool isHit = state.isPlaying && (s == curStep) && hasNote && !state.currentLoop.tracks[t].muted;
            uint32_t cellCol = isHit
                ? Colors::GOLD_TEXT
                : (hasNote
                    ? (state.currentLoop.tracks[t].muted ? Colors::MUTED_TEXT : Colors::TRACK_COLORS[t])
                    : ((s % 4 == 0) ? Colors::BG_CARD : Colors::BG_DARK));
            float inset = isHit ? 3.0f : 5.0f;
            C2D_DrawRectSolid(sx + 0.5f, y + inset, isHit ? 0.35f : 0.2f, stepW - 1.5f, rowH - inset * 2.0f, cellCol);
        }
    }

    if (state.isPlaying) {
        float px = labelW + curStep * stepW + d * 1.1f;
        C2D_DrawRectSolid(px, yStart, 0.4f, stepW, height, C2D_Color32(0xFF, 0xE0, 0x66, 0x44));
        C2D_DrawRectSolid(px, yStart, 0.5f, 2.0f, height, Colors::PLAYHEAD_LINE);
    }
}

static constexpr Rect BTN_PLAY  = {  4.0f, 2.0f, 74.0f, 32.0f };
static constexpr Rect BTN_REC   = { 82.0f, 2.0f, 76.0f, 32.0f };
static constexpr Rect BTN_UNDO  = {162.0f, 2.0f, 74.0f, 32.0f };
static constexpr Rect BTN_SAVE  = {240.0f, 2.0f, 76.0f, 32.0f };
static constexpr Rect BTN_MIC   = {264.0f, 38.0f, 52.0f, 26.0f };

void uiDrawBottomChrome(const AppState& state) {
    bool inSong = (state.activeView == VIEW_SONG);

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

    C2D_DrawRectSolid(0, 36.0f, 0.05f, 320.0f, 30.0f, Colors::BG_DARK);
    float tabW = 63.0f;
    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        Rect tr = { 4.0f + t * 65.0f, 38.0f, tabW, 26.0f };
        bool active = (t == state.activeTrack) && !inSong;
        bool muted = (state.currentLoop.tracks[t].muted != 0);
        uint32_t bg = active ? Colors::BG_CARD_ALT : Colors::BG_PANEL;
        uint32_t border = active ? Colors::TRACK_COLORS[t] : Colors::BORDER_WOOD;
        uint32_t txtCol = muted ? Colors::MUTED_TEXT : (active ? Colors::CREAM_TEXT : Colors::TRACK_COLORS[t]);

        uiDrawButton(tr, bg, border, txtCol, "", 0.38f);
        Rect muteBox = { tr.x + 2.0f, tr.y + 3.0f, 14.0f, 20.0f };
        C2D_DrawRectSolid(muteBox.x, muteBox.y, 0.16f, muteBox.w, muteBox.h,
                          muted ? Colors::RED_REC : Colors::BG_DARK);
        uiDrawTextCentered(muteBox, 0.38f, muted ? Colors::CREAM_TEXT : Colors::MUTED_TEXT, "M");
        Rect instBox = { tr.x + 16.0f, tr.y, tr.w - 17.0f, tr.h };
        uiDrawTextCentered(instBox, 0.38f, txtCol,
                           getInstrumentShortName(state.currentLoop.tracks[t].instrumentId));
    }

    bool hasVoc = inSong ? (state.currentSong.hasSongVocal != 0) : (state.currentLoop.hasVocalClip != 0);
    bool vocMuted = inSong ? (state.currentSong.songVocalMuted != 0) : (state.currentLoop.vocalMuted != 0);
    uint32_t micBg = state.isRecordingVocal ? Colors::RED_REC
                                            : (hasVoc ? (vocMuted ? Colors::BG_CARD : Colors::PURPLE_SPELL) : Colors::BG_PANEL);
    const char* micLbl = state.isRecordingVocal ? "STOP" : (hasVoc ? (vocMuted ? "VOC:M" : "VOC") : "MIC");
    uiDrawButton(BTN_MIC, micBg, hasVoc ? Colors::GOLD_TEXT : Colors::BORDER_WOOD, Colors::CREAM_TEXT, micLbl, 0.38f);

    C2D_DrawRectSolid(0, 206.0f, 0.05f, 320.0f, 34.0f, Colors::BG_PANEL);
    static const char* TAB_NAMES[5] = { "GRID", "PADS", "INST", "SPELL", "SONG" };
    for (int i = 0; i < 5; ++i) {
        Rect nr = { 2.0f + i * 63.5f, 209.0f, 61.5f, 29.0f };
        bool sel = (static_cast<int>(state.activeView) == i);
        uint32_t bg = sel ? (i == VIEW_SPELLBOOK ? Colors::PURPLE_SPELL : Colors::BLUE_ACTION) : Colors::BG_CARD;
        uiDrawButton(nr, bg, sel ? Colors::GOLD_TEXT : Colors::BORDER_WOOD,
                     Colors::CREAM_TEXT, TAB_NAMES[i], 0.42f);
    }
}

bool uiHandleBottomChromeTouch(AppState& state, float tx, float ty) {
    bool inSong = (state.activeView == VIEW_SONG);

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
                    if (tx < tr.x + 18.0f) {
                        state.currentLoop.tracks[t].muted = state.currentLoop.tracks[t].muted ? 0 : 1;
                        state.syncAudioLoop();
                        char buf[48];
                        std::snprintf(buf, sizeof(buf), "%s: %s",
                                      getTrackRoleName(t),
                                      state.currentLoop.tracks[t].muted ? "MUTED" : "UNMUTED");
                        state.setToast(buf, 70);
                    } else if (state.activeTrack == t) {
                        state.showInstPicker = !state.showInstPicker;
                        state.showMixDrawer = false;
                    } else {
                        state.activeTrack = t;
                    }
                    return true;
                }
            }
        }
    }

    if (ty >= 206.0f) {
        for (int i = 0; i < 5; ++i) {
            Rect nr = { 2.0f + i * 63.5f, 209.0f, 61.5f, 29.0f };
            if (nr.contains(tx, ty)) {
                state.showInstPicker = false;
                state.switchView(static_cast<ViewMode>(i));
                return true;
            }
        }
    }
    return false;
}

// ---- Mix & Settings Drawer (#4 & #6) ----

static void drawSliderRow(float y, const char* label, uint8_t value, uint32_t fillCol, bool muted = false, uint8_t maxVal = 100) {
    uiDrawText(8.0f, y + 1.0f, 0.38f, 0.38f, muted ? Colors::RED_REC : Colors::CREAM_TEXT, label);
    Rect bar = { 76.0f, y + 2.0f, 196.0f, 14.0f };
    C2D_DrawRectSolid(bar.x, bar.y, 0.1f, bar.w, bar.h, Colors::BG_DARK);
    uint8_t clamped = (value > maxVal) ? maxVal : value;
    float w = (static_cast<float>(clamped) / static_cast<float>(maxVal)) * bar.w;
    uint32_t barCol = muted ? Colors::MUTED_TEXT : ((maxVal > 100 && clamped > 100) ? Colors::GOLD_TEXT : fillCol);
    C2D_DrawRectSolid(bar.x, bar.y, 0.15f, w, bar.h, barCol);
    if (maxVal > 100) {
        // 100% unity gain reference tick at midpoint
        C2D_DrawRectSolid(bar.x + bar.w * 0.5f, bar.y, 0.2f, 1.5f, bar.h, Colors::CREAM_TEXT);
    }
    char valBuf[12];
    std::snprintf(valBuf, sizeof(valBuf), "%u%%", clamped);
    uiDrawText(276.0f, y + 1.0f, 0.38f, 0.38f, (maxVal > 100 && clamped > 100) ? Colors::GOLD_TEXT : Colors::CREAM_TEXT, valBuf);
}

void uiDrawMixDrawer(const AppState& state) {
    C2D_DrawRectSolid(2.0f, 67.0f, 0.08f, 316.0f, 137.0f, Colors::BG_PANEL);

    drawSliderRow(69.0f,  state.currentLoop.tracks[0].muted ? "[M]Drm" : "Drums",   state.currentLoop.tracks[0].volume, Colors::TRACK_COLORS[0], state.currentLoop.tracks[0].muted != 0, 200);
    drawSliderRow(86.0f,  state.currentLoop.tracks[1].muted ? "[M]Bas" : "Bass",    state.currentLoop.tracks[1].volume, Colors::TRACK_COLORS[1], state.currentLoop.tracks[1].muted != 0, 200);
    drawSliderRow(103.0f, state.currentLoop.tracks[2].muted ? "[M]Mel" : "Melody",  state.currentLoop.tracks[2].volume, Colors::TRACK_COLORS[2], state.currentLoop.tracks[2].muted != 0, 200);
    drawSliderRow(120.0f, state.currentLoop.tracks[3].muted ? "[M]Hrm" : "Harmony", state.currentLoop.tracks[3].volume, Colors::TRACK_COLORS[3], state.currentLoop.tracks[3].muted != 0, 200);
    bool vocMuted = (state.activeView == VIEW_SONG) ? (state.currentSong.songVocalMuted != 0) : (state.currentLoop.vocalMuted != 0);
    uint8_t vocVol = (state.activeView == VIEW_SONG) ? state.currentSong.songVocalVolume : state.currentLoop.vocalVolume;
    drawSliderRow(137.0f, vocMuted ? "[M]Voc" : "Vocal", vocVol, Colors::PURPLE_SPELL, vocMuted, 100);
    drawSliderRow(154.0f, "Reverb", state.currentLoop.reverbMix, Colors::BLUE_ACTION, false, 100);

    Rect btnKey   = {  6.0f, 175.0f, 66.0f, 26.0f };
    Rect btnMetro = { 75.0f, 175.0f, 54.0f, 26.0f };
    Rect btnHpf   = {132.0f, 175.0f, 54.0f, 26.0f };
    Rect btnMidi  = {189.0f, 175.0f, 58.0f, 26.0f };
    Rect btnClose = {250.0f, 175.0f, 64.0f, 26.0f };

    char keyLbl[16];
    std::snprintf(keyLbl, sizeof(keyLbl), "KEY:%s", getRootNoteName(state.currentLoop.rootNote));
    uiDrawButton(btnKey, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::GOLD_TEXT, keyLbl, 0.38f);
    uiDrawButton(btnMetro,
                 state.metronomeEnabled ? Colors::AMBER_JAM : Colors::BG_CARD,
                 Colors::BORDER_WOOD,
                 state.metronomeEnabled ? Colors::BG_DARK : Colors::CREAM_TEXT,
                 "MET", 0.38f);
    uiDrawButton(btnHpf,
                 state.currentLoop.vocalHighPass ? Colors::BLUE_ACTION : Colors::BG_CARD,
                 Colors::BORDER_WOOD, Colors::CREAM_TEXT,
                 "HPF", 0.38f);
    uiDrawButton(btnMidi,
                 state.wifiMidiEnabled ? Colors::GREEN_PLAY : Colors::BG_CARD,
                 Colors::BORDER_WOOD, Colors::CREAM_TEXT,
                 "MIDI", 0.38f);
    uiDrawButton(btnClose, Colors::BG_CARD_ALT, Colors::GOLD_TEXT, Colors::GOLD_TEXT, "DONE", 0.38f);
}

bool uiHandleMixDrawerTouch(AppState& state, float tx, float ty, bool isJustPressed, bool isHeld) {
    if (!state.showMixDrawer) return false;

    if (isJustPressed && tx < 70.0f && ty >= 69.0f && ty < 154.0f) {
        int row = static_cast<int>((ty - 69.0f) / 17.0f);
        if (row >= 0 && row < 4) {
            state.currentLoop.tracks[row].muted = state.currentLoop.tracks[row].muted ? 0 : 1;
            state.syncAudioLoop();
        } else if (row == 4) {
            state.toggleVocalMute(state.activeView == VIEW_SONG);
        }
        return true;
    }

    if ((isJustPressed || isHeld) && tx >= 70.0f && tx <= 278.0f && ty >= 69.0f && ty < 171.0f) {
        float norm = (tx - 76.0f) / 196.0f;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;

        int row = static_cast<int>((ty - 69.0f) / 17.0f);
        if (row >= 0 && row < 4) {
            int pct = static_cast<int>(norm * 200.0f);
            state.currentLoop.tracks[row].volume = static_cast<uint8_t>(pct);
            state.syncAudioLoop();
        } else if (row == 4) {
            uint8_t val = static_cast<uint8_t>(norm * 100.0f);
            if (state.activeView == VIEW_SONG) {
                state.currentSong.songVocalVolume = val;
                audioEngineSetVocalMix(val, state.currentSong.songVocalMuted != 0);
            } else {
                state.currentLoop.vocalVolume = val;
                audioEngineSetVocalMix(val, state.currentLoop.vocalMuted != 0);
            }
        } else if (row == 5) {
            state.currentLoop.reverbMix = static_cast<uint8_t>(norm * 100.0f);
            state.syncAudioLoop();
        }
        return true;
    }

    if (isJustPressed && ty >= 175.0f && ty <= 202.0f) {
        Rect btnKey   = {  6.0f, 175.0f, 66.0f, 26.0f };
        Rect btnMetro = { 75.0f, 175.0f, 54.0f, 26.0f };
        Rect btnHpf   = {132.0f, 175.0f, 54.0f, 26.0f };
        Rect btnMidi  = {189.0f, 175.0f, 58.0f, 26.0f };
        Rect btnClose = {250.0f, 175.0f, 64.0f, 26.0f };

        if (btnKey.contains(tx, ty)) {
            state.cycleRootKey();
        } else if (btnMetro.contains(tx, ty)) {
            state.toggleMetronome();
        } else if (btnHpf.contains(tx, ty)) {
            state.toggleVocalHighPass();
        } else if (btnMidi.contains(tx, ty)) {
            state.toggleWifiMidi();
        } else if (btnClose.contains(tx, ty)) {
            state.showMixDrawer = false;
        }
        return true;
    }
    return true;
}

// ---- Instrument Picker Drawer (#20) ----

static const uint8_t PICKER_DRUMS[4] = {
    INST_TAVERN_KIT, INST_CHIP_KIT, INST_WAR_KIT, INST_LOFI_KIT
};

static const uint8_t PICKER_MELODIC[12] = {
    INST_UPRIGHT_BASS, INST_SUB_BASS,    INST_ELEC_BASS,
    INST_OCARINA,      INST_WOOD_FLUTE,  INST_PULSE_LEAD,
    INST_SYNTH_BRASS,  INST_LUTE,        INST_HARP,
    INST_ELEC_GUITAR,  INST_TAVERN_PIANO,INST_ELEC_KEYS
};

void uiDrawInstPicker(const AppState& state) {
    C2D_DrawRectSolid(2.0f, 67.0f, 0.08f, 316.0f, 137.0f, Colors::BG_PANEL);

    char hdr[48];
    std::snprintf(hdr, sizeof(hdr), "Pick %s Instrument:", getTrackRoleName(state.activeTrack));
    uiDrawText(8.0f, 70.0f, 0.40f, 0.40f, Colors::GOLD_TEXT, hdr);
    Rect btnClose = { 252.0f, 68.0f, 62.0f, 19.0f };
    uiDrawButton(btnClose, Colors::BG_CARD_ALT, Colors::GOLD_TEXT, Colors::GOLD_TEXT, "DONE", 0.38f);

    uint8_t curInst = state.currentLoop.tracks[state.activeTrack].instrumentId;
    if (state.activeTrack == TRACK_DRUMS) {
        for (int i = 0; i < 4; ++i) {
            int col = i % 2;
            int row = i / 2;
            Rect r = { 8.0f + col * 154.0f, 92.0f + row * 54.0f, 148.0f, 48.0f };
            bool sel = (PICKER_DRUMS[i] == curInst);
            uiDrawButton(r, sel ? Colors::TRACK_COLORS[0] : Colors::BG_CARD,
                         sel ? Colors::GOLD_TEXT : Colors::BORDER_WOOD,
                         sel ? Colors::BG_DARK : Colors::CREAM_TEXT,
                         getInstrumentName(PICKER_DRUMS[i]), 0.44f);
        }
        return;
    }

    for (int i = 0; i < 12; ++i) {
        int col = i % 3;
        int row = i / 3;
        Rect r = { 6.0f + col * 104.0f, 90.0f + row * 28.0f, 100.0f, 25.0f };
        bool sel = (PICKER_MELODIC[i] == curInst);
        uint32_t famCol = (i < 3) ? Colors::TRACK_COLORS[1]
                                  : (i < 7 ? Colors::TRACK_COLORS[2] : Colors::TRACK_COLORS[3]);
        uiDrawButton(r, sel ? famCol : Colors::BG_CARD,
                     sel ? Colors::CREAM_TEXT : Colors::BORDER_WOOD,
                     sel ? Colors::BG_DARK : Colors::CREAM_TEXT,
                     getInstrumentName(PICKER_MELODIC[i]), 0.38f);
    }
}

bool uiHandleInstPickerTouch(AppState& state, float tx, float ty) {
    if (!state.showInstPicker) return false;

    Rect btnClose = { 252.0f, 68.0f, 62.0f, 19.0f };
    if (btnClose.contains(tx, ty)) {
        state.showInstPicker = false;
        return true;
    }

    uint8_t picked = 255;
    if (state.activeTrack == TRACK_DRUMS) {
        for (int i = 0; i < 4; ++i) {
            int col = i % 2;
            int row = i / 2;
            Rect r = { 8.0f + col * 154.0f, 92.0f + row * 54.0f, 148.0f, 48.0f };
            if (r.contains(tx, ty)) {
                picked = PICKER_DRUMS[i];
                break;
            }
        }
    } else {
        for (int i = 0; i < 12; ++i) {
            int col = i % 3;
            int row = i / 3;
            Rect r = { 6.0f + col * 104.0f, 90.0f + row * 28.0f, 100.0f, 25.0f };
            if (r.contains(tx, ty)) {
                picked = PICKER_MELODIC[i];
                break;
            }
        }
    }

    if (picked < INST_COUNT) {
        state.currentLoop.tracks[state.activeTrack].instrumentId = picked;
        state.syncAudioLoop();
        state.showInstPicker = false;
        state.playLiveDegree(0, 95);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%s: %s", getTrackRoleName(state.activeTrack), getInstrumentName(picked));
        state.setToast(buf, 80);
    }
    return true;
}

} // namespace BardBeats
