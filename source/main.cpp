#include <3ds.h>
#include <citro2d.h>
#include "app_state.h"
#include "audio_engine.h"
#include "storage_sd.h"
#include "ui_common.h"
#include "view_grid.h"
#include "view_instrument.h"
#include "view_pads.h"
#include "view_song.h"
#include "view_spellbook.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    romfsInit();
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    BardBeats::uiInit();
    BardBeats::storageInit();
    BardBeats::audioEngineInit();

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

        // Exit shortcut for Homebrew Launcher
        if ((kHeld & KEY_L) && (kHeld & KEY_R) && (kDown & KEY_START)) {
            break;
        }

        circlePosition cpad;
        hidCircleRead(&cpad);
        state.handleHardwareInput(kDown, kHeld, kUp, cpad);

        // Touch Handling
        touchPosition touch;
        hidTouchRead(&touch);
        bool isTouching = (kHeld & KEY_TOUCH) != 0;
        bool isJustPressed = (kDown & KEY_TOUCH) != 0;
        bool isReleased = (kUp & KEY_TOUCH) != 0 || (wasTouching && !isTouching);

        float tx = isTouching ? static_cast<float>(touch.px) : lastTouchX;
        float ty = isTouching ? static_cast<float>(touch.py) : lastTouchY;

        if (isJustPressed) {
            if (!BardBeats::uiHandleBottomChromeTouch(state, tx, ty)) {
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
        } else if (isTouching && wasTouching) {
            if (state.activeView == BardBeats::VIEW_GRID) {
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

        // Render Frame
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        BardBeats::uiBeginFrame();

        // Top Screen (400x240)
        C2D_TargetClear(top, BardBeats::Colors::BG_DARK);
        C2D_SceneBegin(top);
        switch (state.activeView) {
            case BardBeats::VIEW_GRID:       BardBeats::viewGridDrawTop(state); break;
            case BardBeats::VIEW_PADS:       BardBeats::viewPadsDrawTop(state); break;
            case BardBeats::VIEW_INSTRUMENT: BardBeats::viewInstrumentDrawTop(state); break;
            case BardBeats::VIEW_SPELLBOOK:  BardBeats::viewSpellbookDrawTop(state); break;
            case BardBeats::VIEW_SONG:       BardBeats::viewSongDrawTop(state); break;
        }

        // Bottom Screen (320x240)
        C2D_TargetClear(bot, BardBeats::Colors::BG_DARK);
        C2D_SceneBegin(bot);
        BardBeats::uiDrawBottomChrome(state);
        switch (state.activeView) {
            case BardBeats::VIEW_GRID:       BardBeats::viewGridDrawBottom(state); break;
            case BardBeats::VIEW_PADS:       BardBeats::viewPadsDrawBottom(state); break;
            case BardBeats::VIEW_INSTRUMENT: BardBeats::viewInstrumentDrawBottom(state); break;
            case BardBeats::VIEW_SPELLBOOK:  BardBeats::viewSpellbookDrawBottom(state); break;
            case BardBeats::VIEW_SONG:       BardBeats::viewSongDrawBottom(state); break;
        }

        C3D_FrameEnd(0);
    }

    BardBeats::audioEngineExit();
    BardBeats::uiExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    romfsExit();
    return 0;
}
