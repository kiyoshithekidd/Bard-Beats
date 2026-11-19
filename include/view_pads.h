#pragma once

#include "app_state.h"

namespace BardBeats {

void viewPadsDrawTop(const AppState& state);
void viewPadsDrawBottom(const AppState& state);
void viewPadsHandleTouch(AppState& state, float tx, float ty, bool isJustPressed, bool isReleased);

} // namespace BardBeats
