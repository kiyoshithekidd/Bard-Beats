#include <3ds.h>
#include <citro2d.h>
#include "app_state.h"
#include "audio_engine.h"
#include "mic_recorder.h"
#include "storage_sd.h"
#include "ui_common.h"
#include "view_grid.h"
#include "view_instrument.h"
#include "view_pads.h"
#include "view_song.h"
#include "view_spellbook.h"
#include "wifi_midi.h"

static void renderTopEye(C3D_RenderTarget* target, const BardBeats::AppState& state, float offsetPx) {
    BardBeats::uiSet3DOffset(offsetPx);
    C2D_TargetClear(target, BardBeats::Colors::BG_DARK);
    C2D_SceneBegin(target);
    switch (state.activeView) {
        case BardBeats::VIEW_GRID:       BardBeats::viewGridDrawTop(state); break;
        case BardBeats::VIEW_PADS:       BardBeats::viewPadsDrawTop(state); break;
        case BardBeats::VIEW_INSTRUMENT: BardBeats::viewInstrumentDrawTop(state); break;
        case BardBeats::VIEW_SPELLBOOK:  BardBeats::viewSpellbookDrawTop(state); break;
        case BardBeats::VIEW_SONG:       BardBeats::viewSongDrawTop(state); break;
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    romfsInit();
    gfxInitDefault();
    gfxSet3D(true);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* topLeft  = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* topRight = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    C3D_RenderTarget* bot      = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    BardBeats::uiInit();
    BardBeats::storageInit();
    BardBeats::audioEngineInit();
    BardBeats::micRecorderInit();

    BardBeats::AppState state;
    state.init();

    bool wasTouching = false;
    float lastTouchX = 0.0f;
    float lastTouchY = 0.0f;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        u32 kUp   = hidKeysUp();

        if ((kHeld & KEY_L) && (kHeld & KEY_R) && (kDown & KEY_START)) {
            break;
        }

        circlePosition cpad;
        hidCircleRead(&cpad);
        state.handleHardwareInput(kDown, kHeld, kUp, cpad);

        touchPosition touch;
        hidTouchRead(&touch);
        bool isTouching = (kHeld & KEY_TOUCH) != 0;
        bool isJustPressed = (kDown & KEY_TOUCH) != 0;
        bool isReleased = (kUp & KEY_TOUCH) != 0 || (wasTouching && !isTouching);

        float tx = isTouching ? static_cast<float>(touch.px) : lastTouchX;
        float ty = isTouching ? static_cast<float>(touch.py) : lastTouchY;

        if (isJustPressed) {
            if (!BardBeats::uiHandleBottomChromeTouch(state, tx, ty)) {
                if (state.showMixDrawer) {
                    BardBeats::uiHandleMixDrawerTouch(state, tx, ty, true, true);
                } else {
                    switch (state.activeView) {
                        case BardBeats::VIEW_GRID:
                            BardBeats::viewGridHandleTouch(state, tx, ty, true, true, false);
                            break;
                        case BardBeats::VIEW_PADS:
                            BardBeats::viewPadsHandleTouch(state, tx, ty, true, false);
                            break;
                        case BardBeats::VIEW_INSTRUMENT:
                            BardBeats::viewInstrumentHandleTouch(state, tx, ty, true, true, false);
                            break;
                        case BardBeats::VIEW_SPELLBOOK:
                            BardBeats::viewSpellbookHandleTouch(state, tx, ty, true);
                            break;
                        case BardBeats::VIEW_SONG:
                            BardBeats::viewSongHandleTouch(state, tx, ty, true);
                            break;
                    }
                }
            }
        } else if (isTouching && wasTouching) {
            if (state.showMixDrawer) {
                BardBeats::uiHandleMixDrawerTouch(state, tx, ty, false, true);
            } else if (state.activeView == BardBeats::VIEW_GRID) {
                BardBeats::viewGridHandleTouch(state, tx, ty, false, true, false);
            } else if (state.activeView == BardBeats::VIEW_INSTRUMENT) {
                BardBeats::viewInstrumentHandleTouch(state, tx, ty, false, true, false);
            }
        } else if (isReleased) {
            if (state.activeView == BardBeats::VIEW_GRID) {
                BardBeats::viewGridHandleTouch(state, tx, ty, false, false, true);
            } else if (state.activeView == BardBeats::VIEW_PADS) {
                BardBeats::viewPadsHandleTouch(state, tx, ty, false, true);
            } else if (state.activeView == BardBeats::VIEW_INSTRUMENT) {
                BardBeats::viewInstrumentHandleTouch(state, tx, ty, false, false, true);
            }
        }

        wasTouching = isTouching;
        if (isTouching) {
            lastTouchX = tx;
            lastTouchY = ty;
        }

        float slider = osGet3DSliderState();
        float iod = slider * 3.5f;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        BardBeats::uiBeginFrame();

        // Top Screen (Left Eye + Right Eye if 3D slider > 0)
        renderTopEye(topLeft, state, -iod);
        if (slider > 0.02f) {
            renderTopEye(topRight, state, iod);
        }

        // Bottom Screen (320x240)
        C2D_TargetClear(bot, BardBeats::Colors::BG_DARK);
        C2D_SceneBegin(bot);
        BardBeats::uiDrawBottomChrome(state);
        if (state.showMixDrawer) {
            BardBeats::uiDrawMixDrawer(state);
        } else {
            switch (state.activeView) {
                case BardBeats::VIEW_GRID:       BardBeats::viewGridDrawBottom(state); break;
                case BardBeats::VIEW_PADS:       BardBeats::viewPadsDrawBottom(state); break;
                case BardBeats::VIEW_INSTRUMENT: BardBeats::viewInstrumentDrawBottom(state); break;
                case BardBeats::VIEW_SPELLBOOK:  BardBeats::viewSpellbookDrawBottom(state); break;
                case BardBeats::VIEW_SONG:       BardBeats::viewSongDrawBottom(state); break;
            }
        }

        C3D_FrameEnd(0);
    }

    BardBeats::wifiMidiExit();
    BardBeats::micRecorderExit();
    BardBeats::audioEngineExit();
    BardBeats::uiExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    romfsExit();
    return 0;
}
