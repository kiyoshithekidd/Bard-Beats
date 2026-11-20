#include "ui_common.h"
#include "audio_engine.h"
#include "mic_recorder.h"
#include <cstdio>

namespace BardBeats {

static C2D_TextBuf s_textBuf = nullptr;
static C2D_SpriteSheet s_spriteSheet = nullptr;
static float s_3dOffset = 0.0f;

void uiInit() {
    s_textBuf = C2D_TextBufNew(4096);
    s_spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
}

void uiExit() {
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
        char stepBuf[16];
        std::snprintf(stepBuf, sizeof(stepBuf), "STEP %02d", step + 1);
        uint32_t stepCol = state.isPlaying ? Colors::PLAYHEAD_LINE : Colors::MUTED_TEXT;
        uiDrawText(325.0f + d, 9.0f, 0.45f, 0.45f, stepCol, stepBuf);
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
        uiDrawText(10.0f + d * 0.5f, y + 22.0f, 0.36f, 0.36f, Colors::MUTED_TEXT,
                   getInstrumentShortName(state.currentLoop.tracks[t].instrumentId));

        for (int s = 0; s < totalSteps; ++s) {
            float sx = labelW + s * stepW + d * 0.5f;
            bool hasNote = stepHasAnyNote(state.currentLoop.tracks[t].steps[s]);
            uint32_t cellCol = hasNote
                ? (state.currentLoop.tracks[t].muted ? Colors::MUTED_TEXT : Colors::TRACK_COLORS[t])
                : ((s % 4 == 0) ? Colors::BG_CARD : Colors::BG_DARK);
            C2D_DrawRectSolid(sx + 0.5f, y + 5.0f, 0.2f, stepW - 1.5f, rowH - 10.0f, cellCol);
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

        char label[20];
        std::snprintf(label, sizeof(label), "%s%s",
                      muted ? "M:" : "",
                      getInstrumentShortName(state.currentLoop.tracks[t].instrumentId));
        uiDrawButton(tr, bg, border, txtCol, label, 0.35f);
    }

    bool hasVoc = inSong ? (state.currentSong.hasSongVocal != 0) : (state.currentLoop.hasVocalClip != 0);
    bool vocMuted = inSong ? (state.currentSong.songVocalMuted != 0) : (state.currentLoop.vocalMuted != 0);
    uint32_t micBg = state.isRecordingVocal ? Colors::RED_REC
                                            : (hasVoc ? (vocMuted ? Colors::BG_CARD : Colors::PURPLE_SPELL) : Colors::BG_PANEL);
    const char* micLbl = state.isRecordingVocal ? "STOP" : (hasVoc ? (vocMuted ? "VOC:M" : "VOC") : "MIC");
    uiDrawButton(BTN_MIC, micBg, hasVoc ? Colors::GOLD_TEXT : Colors::BORDER_WOOD, Colors::CREAM_TEXT, micLbl, 0.36f);

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

// ---- Mix & Settings Drawer (#4 & #6) ----

static void drawSliderRow(float y, const char* label, uint8_t value, uint32_t fillCol) {
    uiDrawText(8.0f, y + 2.0f, 0.36f, 0.36f, Colors::CREAM_TEXT, label);
    Rect bar = { 74.0f, y + 2.0f, 204.0f, 14.0f };
    C2D_DrawRectSolid(bar.x, bar.y, 0.1f, bar.w, bar.h, Colors::BG_DARK);
    float w = (static_cast<float>(value) / 100.0f) * bar.w;
    C2D_DrawRectSolid(bar.x, bar.y, 0.15f, w, bar.h, fillCol);
    char valBuf[8];
    std::snprintf(valBuf, sizeof(valBuf), "%u", value);
    uiDrawText(284.0f, y + 2.0f, 0.36f, 0.36f, Colors::GOLD_TEXT, valBuf);
}

void uiDrawMixDrawer(const AppState& state) {
    C2D_DrawRectSolid(2.0f, 67.0f, 0.08f, 316.0f, 137.0f, Colors::BG_PANEL);

    drawSliderRow(69.0f,  "Drums",   state.currentLoop.tracks[0].volume, Colors::TRACK_COLORS[0]);
    drawSliderRow(86.0f,  "Bass",    state.currentLoop.tracks[1].volume, Colors::TRACK_COLORS[1]);
    drawSliderRow(103.0f, "Melody",  state.currentLoop.tracks[2].volume, Colors::TRACK_COLORS[2]);
    drawSliderRow(120.0f, "Harmony", state.currentLoop.tracks[3].volume, Colors::TRACK_COLORS[3]);
    uint8_t vocVol = (state.activeView == VIEW_SONG) ? state.currentSong.songVocalVolume : state.currentLoop.vocalVolume;
    drawSliderRow(137.0f, "Vocal",   vocVol, Colors::PURPLE_SPELL);
    drawSliderRow(154.0f, "Reverb",  state.currentLoop.reverbMix, Colors::BLUE_ACTION);

    Rect btnHpf   = {  6.0f, 175.0f, 96.0f, 26.0f };
    Rect btnMidi  = {106.0f, 175.0f, 114.0f, 26.0f };
    Rect btnClose = {224.0f, 175.0f, 90.0f, 26.0f };

    uiDrawButton(btnHpf,
                 state.currentLoop.vocalHighPass ? Colors::BLUE_ACTION : Colors::BG_CARD,
                 Colors::BORDER_WOOD, Colors::CREAM_TEXT,
                 state.currentLoop.vocalHighPass ? "HPF: ON" : "HPF: OFF", 0.36f);
    uiDrawButton(btnMidi,
                 state.wifiMidiEnabled ? Colors::GREEN_PLAY : Colors::BG_CARD,
                 Colors::BORDER_WOOD, Colors::CREAM_TEXT,
                 state.wifiMidiEnabled ? "MIDI: ON" : "MIDI: OFF", 0.36f);
    uiDrawButton(btnClose, Colors::BG_CARD_ALT, Colors::GOLD_TEXT, Colors::GOLD_TEXT, "DONE", 0.38f);
}

bool uiHandleMixDrawerTouch(AppState& state, float tx, float ty, bool isJustPressed, bool isHeld) {
    if (!state.showMixDrawer) return false;

    if ((isJustPressed || isHeld) && tx >= 70.0f && tx <= 284.0f && ty >= 69.0f && ty < 171.0f) {
        int pct = static_cast<int>(((tx - 74.0f) / 204.0f) * 100.0f);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        uint8_t val = static_cast<uint8_t>(pct);

        int row = static_cast<int>((ty - 69.0f) / 17.0f);
        if (row >= 0 && row < 4) {
            state.currentLoop.tracks[row].volume = val;
            state.syncAudioLoop();
        } else if (row == 4) {
            if (state.activeView == VIEW_SONG) {
                state.currentSong.songVocalVolume = val;
                audioEngineSetVocalMix(val, state.currentSong.songVocalMuted != 0);
            } else {
                state.currentLoop.vocalVolume = val;
                audioEngineSetVocalMix(val, state.currentLoop.vocalMuted != 0);
            }
        } else if (row == 5) {
            state.currentLoop.reverbMix = val;
            state.syncAudioLoop();
        }
        return true;
    }

    if (isJustPressed && ty >= 175.0f && ty <= 202.0f) {
        Rect btnHpf   = {  6.0f, 175.0f, 96.0f, 26.0f };
        Rect btnMidi  = {106.0f, 175.0f, 114.0f, 26.0f };
        Rect btnClose = {224.0f, 175.0f, 90.0f, 26.0f };

        if (btnHpf.contains(tx, ty)) {
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

} // namespace BardBeats
