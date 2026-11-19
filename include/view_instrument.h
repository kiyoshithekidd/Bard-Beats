#pragma once

#include "app_state.h"

namespace BardBeats {

void viewInstrumentDrawTop(const AppState& state);
void viewInstrumentDrawBottom(const AppState& state);
void viewInstrumentHandleTouch(AppState& state, float tx, float ty, bool isJustPressed, bool isHeld, bool isReleased);

} // namespace BardBeats
