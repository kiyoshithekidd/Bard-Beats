#pragma once

#include "app_state.h"

namespace BardBeats {

void viewSpellbookDrawTop(const AppState& state);
void viewSpellbookDrawBottom(const AppState& state);
void viewSpellbookHandleTouch(AppState& state, float tx, float ty, bool isJustPressed);

} // namespace BardBeats
