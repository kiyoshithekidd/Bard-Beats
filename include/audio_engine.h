#pragma once

#include "data_model.h"
#include "synth_voices.h"
#include <cstdint>

namespace BardBeats {

bool audioEngineInit();
void audioEngineExit();
bool audioEngineIsDspReady();

// Thread-safe state updates from UI thread
void audioEngineSetLoop(const LoopFile& loop);
void audioEngineSetPlaying(bool playing);
bool audioEngineIsPlaying();
void audioEngineResetPlayhead();

void audioEngineTriggerLiveNote(int trackIdx, uint8_t instrumentId, uint8_t midiNote, uint8_t velocity);
void audioEngineReleaseLiveNotes(int trackIdx);
void audioEngineSetCirclePad(float bendSemis, float vibrato);

// Read-only telemetry for UI rendering
int   audioEngineGetCurrentStep();
float audioEngineGetStepFraction(); // 0.0..1.0 within current step (for live quantize)
float audioEngineGetPeakLevel();

} // namespace BardBeats
