#pragma once

#include "app_state.h"

namespace BardBeats {

void viewSongDrawTop(const AppState& state);
void viewSongDrawBottom(const AppState& state);
void viewSongHandleTouch(AppState& state, float tx, float ty, bool isJustPressed);

} // namespace BardBeats
