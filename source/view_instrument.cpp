#include "view_instrument.h"
#include "audio_engine.h"
#include "sprites.h"
#include "ui_common.h"
#include <cmath>
#include <cstdio>

namespace BardBeats {

static int s_lastStrumDeg = -1;

// Exact feature coordinates inside the 384x152 instrument sprites (drawn at screen x=8, y=40)
static constexpr float STAGE_X = 8.0f;
static constexpr float STAGE_Y = 40.0f;

// 8 dark tone holes on gfx/instruments/ocarina.png (ordered left-to-right)
static constexpr float OCARINA_HOLES[8][2] = {
    { 102.0f,  90.0f }, // Deg 0
    { 132.0f,  82.0f }, // Deg 1
    { 138.0f, 100.0f }, // Deg 2
    { 162.0f,  72.0f }, // Deg 3
    { 188.0f,  55.0f }, // Deg 4
    { 190.0f, 108.0f }, // Deg 5
    { 248.0f,  79.0f }, // Deg 6
    { 279.0f,  77.0f }  // Deg 7
};

// 8 vertical harp strings on gfx/instruments/harp.png: { xTop, yTop, xBot, yBot }
static constexpr float HARP_STRINGS[8][4] = {
    { 162.0f,  8.0f, 180.0f, 134.0f }, // Deg 0 (longest, lowest)
    { 172.0f, 12.0f, 188.0f, 124.0f }, // Deg 1
    { 182.0f, 18.0f, 196.0f, 114.0f }, // Deg 2
    { 192.0f, 24.0f, 204.0f, 102.0f }, // Deg 3
    { 202.0f, 30.0f, 212.0f,  90.0f }, // Deg 4
    { 212.0f, 35.0f, 220.0f,  78.0f }, // Deg 5
    { 222.0f, 38.0f, 228.0f,  68.0f }, // Deg 6
    { 232.0f, 38.0f, 236.0f,  58.0f }  // Deg 7 (shortest, highest)
};

void viewInstrumentDrawTop(const AppState& state) {
    uiDrawTopHeader(state);

    uint8_t inst = state.currentLoop.tracks[state.activeTrack].instrumentId;
    uint32_t trkCol = Colors::TRACK_COLORS[state.activeTrack];

    float d = uiGet3DOffset();
    float sx = STAGE_X + d;

    // Stage background frame
    C2D_DrawRectSolid(STAGE_X, STAGE_Y, 0.05f, 384.0f, 152.0f, Colors::BG_PANEL);

    // Draw illustrated instrument sprite from romfs:/gfx/sprites.t3x
    C2D_SpriteSheet sheet = uiGetSpriteSheet();
    if (sheet) {
        size_t spriteIdx = sprites_lute_idx;
        if (inst == INST_OCARINA || inst == INST_PULSE_LEAD) {
            spriteIdx = sprites_ocarina_idx;
        } else if (inst == INST_HARP) {
            spriteIdx = sprites_harp_idx;
        } else if (inst == INST_TAVERN_KIT || inst == INST_CHIP_KIT) {
            spriteIdx = sprites_drums_idx;
        } else if (inst == INST_UPRIGHT_BASS || inst == INST_SUB_BASS) {
            spriteIdx = sprites_bass_idx;
        }
        C2D_Image img = C2D_SpriteSheetGetImage(sheet, spriteIdx);
        C2D_DrawImageAt(img, sx, STAGE_Y, 0.10f, nullptr, 1.0f, 1.0f);
    }

    // Pixel-aligned interactive hit overlay synced to live notes
    if (state.liveFlashFrames > 0 && state.lastLiveDegree >= 0 && state.lastLiveDegree < 8) {
        int deg = state.lastLiveDegree;
        float pulse = static_cast<float>(state.liveFlashFrames) / 14.0f;
        uint8_t alpha = static_cast<uint8_t>(pulse * 220.0f);
        uint32_t glowCol = C2D_Color32(0xFF, 0xE4, 0x70, alpha);
        uint32_t coreCol = C2D_Color32(0xFF, 0xFF, 0xEE, alpha);

        if (inst == INST_HARP) {
            float x0 = sx + HARP_STRINGS[deg][0];
            float y0 = STAGE_Y + HARP_STRINGS[deg][1];
            float x1 = sx + HARP_STRINGS[deg][2];
            float y1 = STAGE_Y + HARP_STRINGS[deg][3];
            C2D_DrawLine(x0, y0, glowCol, x1, y1, glowCol, 4.5f, 0.24f);
            C2D_DrawLine(x0, y0, coreCol, x1, y1, coreCol, 1.8f, 0.26f);
            C2D_DrawCircleSolid((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, 0.27f, 6.0f + (1.0f - pulse) * 4.0f, glowCol);
        } else if (inst == INST_OCARINA || inst == INST_PULSE_LEAD) {
            float hx = sx + OCARINA_HOLES[deg][0];
            float hy = STAGE_Y + OCARINA_HOLES[deg][1];
            C2D_DrawCircleSolid(hx, hy, 0.25f, 10.0f + (1.0f - pulse) * 5.0f, glowCol);
            C2D_DrawCircleSolid(hx, hy, 0.27f, 5.0f, coreCol);
        } else if (inst == INST_TAVERN_KIT || inst == INST_CHIP_KIT) {
            float dx = sx + 82.0f;
            float dy = STAGE_Y + 66.0f;
            float rad = 32.0f;
            if (deg == 2 || deg == 3) {
                dx = sx + 204.0f;
                dy = STAGE_Y + 56.0f;
                rad = 22.0f;
            } else if (deg >= 4) {
                dx = sx + 318.0f;
                dy = STAGE_Y + 76.0f;
                rad = 26.0f;
            }
            C2D_DrawCircleSolid(dx, dy, 0.25f, rad + (1.0f - pulse) * 10.0f,
                                C2D_Color32(0xFF, 0xE0, 0x66, static_cast<uint8_t>(pulse * 140.0f)));
            C2D_DrawCircleSolid(dx, dy, 0.27f, rad * 0.45f, glowCol);
        } else if (inst == INST_UPRIGHT_BASS || inst == INST_SUB_BASS) {
            // 4 thick horizontal bass strings aligned across fingerboard -> bridge on bass.png
            int strIdx = deg / 2;
            float sy = STAGE_Y + 71.0f + static_cast<float>(strIdx) * 4.3f;
            C2D_DrawRectSolid(sx + 64.0f, sy - 1.5f, 0.24f, 228.0f, 3.8f, glowCol);
            C2D_DrawRectSolid(sx + 64.0f, sy - 0.5f, 0.26f, 228.0f, 1.4f, coreCol);
            C2D_DrawCircleSolid(sx + 245.0f, sy, 0.27f, 7.0f + (1.0f - pulse) * 4.0f, glowCol);
        } else {
            float sy = STAGE_Y + 64.0f + static_cast<float>(deg) * 3.4f;
            C2D_DrawRectSolid(sx + 80.0f, sy - 1.5f, 0.24f, 240.0f, 3.5f, glowCol);
            C2D_DrawRectSolid(sx + 80.0f, sy - 0.5f, 0.26f, 240.0f, 1.2f, coreCol);
            C2D_DrawCircleSolid(sx + 258.0f, sy, 0.27f, 6.0f + (1.0f - pulse) * 4.0f, glowCol);
        }
    }

    // Subtle dark header pill for instrument title legibility
    C2D_DrawRectSolid(12.0f, 42.0f, 0.20f, 232.0f, 20.0f, C2D_Color32(0x18, 0x12, 0x10, 0xB8));
    char instBuf[64];
    std::snprintf(instBuf, sizeof(instBuf), "Playing: %s (%s)",
                  getInstrumentName(inst), getTrackRoleName(state.activeTrack));
    uiDrawText(16.0f, 44.0f, 0.42f, 0.42f, Colors::GOLD_TEXT, instBuf);

    // Circle Pad Pitch Bend Gauge pill (bottom of stage)
    C2D_DrawRectSolid(12.0f, 168.0f, 0.20f, 276.0f, 20.0f, C2D_Color32(0x18, 0x12, 0x10, 0xC8));
    float gaugeX = 114.0f;
    float gaugeW = 166.0f;
    C2D_DrawRectSolid(gaugeX, 174.0f, 0.25f, gaugeW, 8.0f, Colors::BG_DARK);
    float thumbX = gaugeX + (gaugeW * 0.5f) + (state.circleBendSemis / 2.0f) * (gaugeW * 0.45f);
    C2D_DrawRectSolid(thumbX - 4.0f, 171.0f, 0.30f, 8.0f, 14.0f, Colors::PLAYHEAD_LINE);
    uiDrawText(16.0f, 170.0f, 0.36f, 0.36f, Colors::CREAM_TEXT, "Pitch Bend:");

    // Compact 16-step beat progress bar at y: 198
    int curStep = audioEngineGetCurrentStep();
    for (int s = 0; s < BB_STEPS_PER_LOOP; ++s) {
        float sx = 12.0f + s * 23.5f;
        bool active = stepHasAnyNote(state.currentLoop.tracks[state.activeTrack].steps[s]);
        uint32_t col = (state.isPlaying && s == curStep)
                           ? Colors::PLAYHEAD_LINE
                           : (active ? trkCol : Colors::BG_CARD);
        C2D_DrawRectSolid(sx, 198.0f, 0.2f, 20.0f, 12.0f, col);
    }
}

void viewInstrumentDrawBottom(const AppState& state) {
    Rect btnMode = {   4.0f, 67.0f, 154.0f, 19.0f };
    Rect btnDice = { 162.0f, 67.0f,  76.0f, 19.0f };
    Rect btnOct  = { 242.0f, 67.0f,  74.0f, 19.0f };

    const char* modeStr = (state.perfMode == 1) ? "MODE: CHORD"
                                                : (state.perfMode == 2 ? "MODE: ARP" : "MODE: NOTE");
    uiDrawButton(btnMode, state.perfMode > 0 ? Colors::BLUE_ACTION : Colors::BG_CARD,
                 Colors::BORDER_WOOD, Colors::CREAM_TEXT, modeStr, 0.34f);
    uiDrawButton(btnDice, Colors::PURPLE_SPELL, Colors::GOLD_TEXT, Colors::CREAM_TEXT, "DICE", 0.34f);
    const char* octStr = (state.octaveOffset < 0) ? "OCT: LOW" : (state.octaveOffset > 0 ? "OCT: HI" : "OCT: MID");
    uiDrawButton(btnOct, Colors::BG_CARD, Colors::BORDER_WOOD, Colors::GOLD_TEXT, octStr, 0.32f);

    uint32_t trkCol = Colors::TRACK_COLORS[state.activeTrack];
    for (int deg = 0; deg < 8; ++deg) {
        Rect r = { 4.0f + deg * 39.2f, 89.0f, 36.5f, 113.0f };
        bool lit = (s_lastStrumDeg == deg) || (state.liveFlashFrames > 0 && state.lastLiveDegree == deg);
        uint32_t bg = lit ? trkCol : ((deg % 2 == 0) ? Colors::BG_CARD : Colors::BG_CARD_ALT);

        char lbl[12];
        if (state.activeTrack == TRACK_DRUMS) {
            std::snprintf(lbl, sizeof(lbl), "%s", getDrumPieceName(deg));
        } else {
            uint8_t note = getScaleNote(state.currentLoop.scaleId, state.currentLoop.rootNote,
                                        (state.activeTrack == TRACK_BASS ? state.octaveOffset - 1 : state.octaveOffset), deg);
            getNoteName(note, lbl, sizeof(lbl));
        }
        uiDrawButton(r, bg, lit ? Colors::GOLD_TEXT : Colors::BORDER_WOOD,
                     lit ? Colors::BG_DARK : Colors::CREAM_TEXT, lbl, 0.42f);
    }
}

void viewInstrumentHandleTouch(AppState& state, float tx, float ty, bool isJustPressed, bool isHeld, bool isReleased) {
    if (isReleased) {
        s_lastStrumDeg = -1;
        state.releaseLiveNotes();
        return;
    }

    if (isJustPressed && ty >= 67.0f && ty < 87.0f) {
        Rect btnMode = {   4.0f, 67.0f, 154.0f, 19.0f };
        Rect btnDice = { 162.0f, 67.0f,  76.0f, 19.0f };
        Rect btnOct  = { 242.0f, 67.0f,  74.0f, 19.0f };
        if (btnMode.contains(tx, ty)) {
            state.cyclePerfMode();
        } else if (btnDice.contains(tx, ty)) {
            state.rollBardicDice();
        } else if (btnOct.contains(tx, ty)) {
            state.octaveOffset = (state.octaveOffset == 1) ? -1 : (state.octaveOffset + 1);
        }
        return;
    }

    if ((isJustPressed || isHeld) && ty >= 89.0f && ty < 202.0f) {
        int deg = static_cast<int>((tx - 4.0f) / 39.2f);
        if (deg >= 0 && deg < 8 && deg != s_lastStrumDeg) {
            s_lastStrumDeg = deg;
            state.playLiveDegree(deg, 105);
        }
    }
}

} // namespace BardBeats
