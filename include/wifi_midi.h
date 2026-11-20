#pragma once

#include <cstdint>

namespace BardBeats {

bool wifiMidiInit();
void wifiMidiExit();
bool wifiMidiIsActive();

void wifiMidiSendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
void wifiMidiSendNoteOff(uint8_t channel, uint8_t note);
void wifiMidiSendPitchBend(uint8_t channel, float bendSemis); // -2.0 .. +2.0

} // namespace BardBeats
