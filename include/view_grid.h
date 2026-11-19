#pragma once

#include "app_state.h"

namespace BardBeats {

void viewGridDrawTop(const AppState& state);
void viewGridDrawBottom(const AppState& state);
void viewGridHandleTouch(AppState& state, float tx, float ty, bool isJustPressed, bool isHeld, bool isReleased);

} // namespace BardBeats
