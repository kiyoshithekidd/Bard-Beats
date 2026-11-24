#pragma once

#include "app_state.h"
#include <citro2d.h>
#include <cstdint>

namespace BardBeats {

struct Rect {
    float x, y, w, h;
    bool contains(float px, float py) const {
        return px >= x && px < (x + w) && py >= y && py < (y + h);
    }
};

// Warm Tavern & Parchment Color Palette (ABGR / C2D_Color32)
namespace Colors {
    constexpr uint32_t BG_DARK        = C2D_Color32(0x18, 0x12, 0x10, 0xFF);
    constexpr uint32_t BG_PANEL       = C2D_Color32(0x2A, 0x20, 0x1B, 0xFF);
    constexpr uint32_t BG_CARD        = C2D_Color32(0x38, 0x2B, 0x23, 0xFF);
    constexpr uint32_t BG_CARD_ALT    = C2D_Color32(0x46, 0x36, 0x2C, 0xFF);
    constexpr uint32_t BORDER_WOOD    = C2D_Color32(0x6E, 0x52, 0x3C, 0xFF);
    constexpr uint32_t GOLD_TEXT      = C2D_Color32(0xF2, 0xD2, 0x8B, 0xFF);
    constexpr uint32_t CREAM_TEXT     = C2D_Color32(0xF5, 0xEC, 0xDC, 0xFF);
    constexpr uint32_t MUTED_TEXT     = C2D_Color32(0xA8, 0x98, 0x84, 0xFF);

    constexpr uint32_t GREEN_PLAY     = C2D_Color32(0x4C, 0xAF, 0x50, 0xFF);
    constexpr uint32_t RED_REC        = C2D_Color32(0xD9, 0x43, 0x43, 0xFF);
    constexpr uint32_t AMBER_JAM      = C2D_Color32(0xE8, 0x9E, 0x32, 0xFF);
    constexpr uint32_t BLUE_ACTION    = C2D_Color32(0x3E, 0x8E, 0xB8, 0xFF);
    constexpr uint32_t PURPLE_SPELL   = C2D_Color32(0x8C, 0x58, 0xB8, 0xFF);
    constexpr uint32_t PLAYHEAD       = C2D_Color32(0xFF, 0xE0, 0x66, 0xCC);
    constexpr uint32_t PLAYHEAD_LINE  = C2D_Color32(0xFF, 0xD5, 0x4F, 0xFF);

    constexpr uint32_t TRACK_COLORS[4] = {
        C2D_Color32(0xE0, 0x6C, 0x50, 0xFF), // Drums: Ember Coral
        C2D_Color32(0x52, 0x9E, 0xD6, 0xFF), // Bass: Deep Azure
        C2D_Color32(0x64, 0xC4, 0x66, 0xFF), // Melody: Forest Emerald
        C2D_Color32(0xE8, 0xB0, 0x48, 0xFF)  // Harmony: Lute Gold
    };
}

void uiInit();
void uiExit();
void uiBeginFrame();
C2D_SpriteSheet uiGetSpriteSheet();
C2D_SpriteSheet uiGetSpriteSheet2();
void uiSet3DOffset(float offsetPx);
float uiGet3DOffset();

void uiDrawText(float x, float y, float scaleX, float scaleY, uint32_t color, const char* text);
void uiDrawTextCentered(const Rect& r, float scale, uint32_t color, const char* text);
void uiDrawButton(const Rect& r, uint32_t bgColor, uint32_t borderColor,
                  uint32_t textColor, const char* label, float textScale = 0.45f);

// Top-screen shared header & 4-track mini-map
void uiDrawTopHeader(const AppState& state);
void uiDrawTopFourTrackOverview(const AppState& state, float yStart, float height);

// Bottom-screen persistent bars (Transport y:0..34, Track Bar y:36..66, Nav Tabs y:206..240)
void uiDrawBottomChrome(const AppState& state);
bool uiHandleBottomChromeTouch(AppState& state, float tx, float ty);
bool uiHandleBottomChromeDrag(AppState& state, float tx, float ty);
void uiResetBottomChromeDrag();

// Mix & Settings Drawer (y: 66..206)
void uiDrawMixDrawer(const AppState& state);
bool uiHandleMixDrawerTouch(AppState& state, float tx, float ty, bool isJustPressed, bool isHeld);

// Instrument Picker Drawer (y: 66..206) (#20)
void uiDrawInstPicker(const AppState& state);
bool uiHandleInstPickerTouch(AppState& state, float tx, float ty);

// Controls Cheat-Sheet Scroll Overlay (v0.7.0)
void uiDrawHelpOverlay(const AppState& state);
bool uiHandleHelpOverlayTouch(AppState& state, float tx, float ty);

} // namespace BardBeats
