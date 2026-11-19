#include "view_instrument.h"
#include "audio_engine.h"
#include "ui_common.h"
#include <cmath>
#include <cstdio>

namespace BardBeats {

static int s_lastStrumDeg = -1;

void viewInstrumentDrawTop(const AppState& state) {
    uiDrawTopHeader(state);

    uint8_t inst = state.currentLoop.tracks[state.activeTrack].instrumentId;
    uint32_t trkCol = Colors::TRACK_COLORS[state.activeTrack];

    // Stage background
    C2D_DrawRectSolid(8.0f, 40.0f, 0.05f, 384.0f, 152.0f, Colors::BG_PANEL);

    if (inst == INST_LUTE || inst == INST_HARP || inst == INST_UPRIGHT_BASS) {
        // Rendered Stringed Instrument (Lute / Harp / Upright Bass)
        uint32_t bodyCol = (inst == INST_HARP) ? C2D_Color32(0xC8, 0x9B, 0x3C, 0xFF)
                                               : C2D_Color32(0x8C, 0x4E, 0x26, 0xFF);
        C2D_DrawCircleSolid(140.0f, 116.0f, 0.1f, 54.0f, bodyCol);
        C2D_DrawCircleSolid(140.0f, 116.0f, 0.15f, 20.0f, Colors::BG_DARK);
        // Neck
        C2D_DrawRectSolid(180.0f, 92.0f, 0.1f, 195.0f, 48.0f, C2D_Color32(0x5E, 0x34, 0x1A, 0xFF));
        // 8 Glowing Strings
        for (int s = 0; s < 8; ++s) {
            float sy = 76.0f + s * 11.0f;
            bool lit = (state.liveFlashFrames > 0 && state.lastLiveDegree == s);
            uint32_t sCol = lit ? Colors::PLAYHEAD_LINE : C2D_Color32(0xE8, 0xD8, 0xB8, 0xBB);
            C2D_DrawRectSolid(96.0f, sy, 0.25f, 276.0f, lit ? 3.0f : 1.5f, sCol);
        }
    } else if (inst == INST_OCARINA || inst == INST_PULSE_LEAD || inst == INST_SUB_BASS) {
        // Rendered Ocarina / Wind Synth with 8 glowing tone holes
        uint32_t shellCol = (inst == INST_OCARINA) ? C2D_Color32(0x2E, 0x6B, 0x9E, 0xFF)
                                                   : C2D_Color32(0x56, 0x3E, 0x7C, 0xFF);
        C2D_DrawCircleSolid(160.0f, 116.0f, 0.1f, 52.0f, shellCol);
        C2D_DrawCircleSolid(230.0f, 116.0f, 0.1f, 42.0f, shellCol);
        C2D_DrawRectSolid(160.0f, 74.0f, 0.1f, 72.0f, 84.0f, shellCol);
        // Mouthpiece
        C2D_DrawRectSolid(84.0f, 106.0f, 0.1f, 34.0f, 22.0f, shellCol);

        // 8 Tone Holes
        for (int h = 0; h < 8; ++h) {
            float hx = 142.0f + (h % 4) * 26.0f;
            float hy = (h < 4) ? 98.0f : 132.0f;
            bool lit = (state.liveFlashFrames > 0 && state.lastLiveDegree == h);
            C2D_DrawCircleSolid(hx, hy, 0.2f, 9.0f, lit ? Colors::PLAYHEAD_LINE : Colors::BG_DARK);
        }
    } else {
        // Rendered Tavern Drum Kit / 8-Bit Kit
        C2D_DrawCircleSolid(120.0f, 128.0f, 0.1f, 38.0f, C2D_Color32(0x9E, 0x58, 0x2E, 0xFF));
        C2D_DrawCircleSolid(200.0f, 116.0f, 0.1f, 28.0f, C2D_Color32(0xC8, 0xB4, 0x92, 0xFF));
        C2D_DrawCircleSolid(275.0f, 104.0f, 0.1f, 24.0f, Colors::GOLD_TEXT);
        if (state.liveFlashFrames > 0) {
            C2D_DrawCircleSolid(120.0f + (state.lastLiveDegree % 3) * 76.0f, 116.0f, 0.25f, 18.0f, trkCol);
        }
    }

    // Instrument Title & Live Pitch Bend Gauge
    char instBuf[64];
    std::snprintf(instBuf, sizeof(instBuf), "Playing: %s (%s)",
                  getInstrumentName(inst), getTrackRoleName(state.activeTrack));
    uiDrawText(16.0f, 44.0f, 0.45f, 0.45f, Colors::GOLD_TEXT, instBuf);

    // Circle Pad Pitch Bend Bar (bottom of stage)
    float gaugeX = 120.0f;
    float gaugeW = 160.0f;
    C2D_DrawRectSolid(gaugeX, 174.0f, 0.2f, gaugeW, 8.0f, Colors::BG_DARK);
    float thumbX = gaugeX + (gaugeW * 0.5f) + (state.circleBendSemis / 2.0f) * (gaugeW * 0.45f);
    C2D_DrawRectSolid(thumbX - 4.0f, 171.0f, 0.3f, 8.0f, 14.0f, Colors::PLAYHEAD_LINE);
    uiDrawText(16.0f, 170.0f, 0.36f, 0.36f, Colors::MUTED_TEXT, "Pitch Bend:");

    // Compact 16-step beat progress bar at y: 196
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
    // 8 interactive vertical strings / tone keys across the bottom workspace (y: 70..202)
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
