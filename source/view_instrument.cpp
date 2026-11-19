#include "view_instrument.h"
#include "audio_engine.h"
#include "sprites.h"
#include "ui_common.h"
#include <cmath>
#include <cstdio>

namespace BardBeats {

static int s_lastStrumDeg = -1;

void viewInstrumentDrawTop(const AppState& state) {
    uiDrawTopHeader(state);

    uint8_t inst = state.currentLoop.tracks[state.activeTrack].instrumentId;
    uint32_t trkCol = Colors::TRACK_COLORS[state.activeTrack];

    // Stage background frame
    C2D_DrawRectSolid(8.0f, 40.0f, 0.05f, 384.0f, 152.0f, Colors::BG_PANEL);

    // Draw illustrated instrument sprite from romfs:/gfx/sprites.t3x
    C2D_SpriteSheet sheet = uiGetSpriteSheet();
    if (sheet) {
        size_t spriteIdx = sprites_lute_idx;
        if (inst == INST_OCARINA || inst == INST_PULSE_LEAD || inst == INST_SUB_BASS) {
            spriteIdx = sprites_ocarina_idx;
        } else if (inst == INST_HARP) {
            spriteIdx = sprites_harp_idx;
        } else if (inst == INST_TAVERN_KIT || inst == INST_CHIP_KIT) {
            spriteIdx = sprites_drums_idx;
        }
        C2D_Image img = C2D_SpriteSheetGetImage(sheet, spriteIdx);
        C2D_DrawImageAt(img, 8.0f, 40.0f, 0.10f, nullptr, 1.0f, 1.0f);
    }

    // Interactive live note glow overlay synced to buttons / touch strum
    if (state.liveFlashFrames > 0 && state.lastLiveDegree >= 0) {
        float pulse = static_cast<float>(state.liveFlashFrames) / 14.0f;
        uint8_t alpha = static_cast<uint8_t>(pulse * 185.0f);
        uint32_t glowCol = C2D_Color32(0xFF, 0xE0, 0x66, alpha);
        if (inst == INST_OCARINA || inst == INST_PULSE_LEAD || inst == INST_SUB_BASS) {
            float hx = 128.0f + (state.lastLiveDegree % 4) * 44.0f;
            float hy = (state.lastLiveDegree < 4) ? 104.0f : 136.0f;
            C2D_DrawCircleSolid(hx, hy, 0.25f, 13.0f + (1.0f - pulse) * 6.0f, glowCol);
        } else if (inst == INST_TAVERN_KIT || inst == INST_CHIP_KIT) {
            float dx = 92.0f + (state.lastLiveDegree % 3) * 108.0f;
            C2D_DrawCircleSolid(dx, 118.0f, 0.25f, 22.0f + (1.0f - pulse) * 10.0f, glowCol);
        } else {
            // Lute / Harp / Bass string vibration flash
            float sy = 86.0f + state.lastLiveDegree * 8.5f;
            C2D_DrawRectSolid(84.0f, sy, 0.25f, 260.0f, 3.0f, glowCol);
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
    uint32_t trkCol = Colors::TRACK_COLORS[state.activeTrack];
    for (int deg = 0; deg < 8; ++deg) {
        Rect r = { 4.0f + deg * 39.2f, 70.0f, 36.5f, 132.0f };
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

    if ((isJustPressed || isHeld) && ty >= 70.0f && ty < 202.0f) {
        int deg = static_cast<int>((tx - 4.0f) / 39.2f);
        if (deg >= 0 && deg < 8 && deg != s_lastStrumDeg) {
            s_lastStrumDeg = deg;
            state.playLiveDegree(deg, 105);
        }
    }
}

} // namespace BardBeats
