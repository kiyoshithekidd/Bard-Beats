#pragma once

#include "data_model.h"
#include "synth_voices.h"
#include <cstdint>
#include <vector>

namespace BardBeats {

bool audioEngineInit();
void audioEngineExit();
bool audioEngineIsDspReady();

// Loop Mode state updates
void audioEngineSetLoop(const LoopFile& loop);
void audioEngineQueueNextBarLoop(const LoopFile& loop); // Switches cleanly on next bar downbeat
bool audioEngineHasQueuedLoop();
void audioEngineSetPlaying(bool playing);
bool audioEngineIsPlaying();
void audioEngineResetPlayhead();

// Song Mode state updates
void audioEngineSetSongMode(bool songMode);
bool audioEngineIsSongMode();
void audioEngineSetSongArrangement(const SongFile& song, const std::vector<LoopFile>& resolvedLoops);
int  audioEngineGetSongEntryIndex();
int  audioEngineGetSongRepeatIndex();

// Vocal Layer playback on dedicated ndsp Channel 1 (16,364 Hz Mono PCM16)
void audioEngineSetVocalLayer(const int16_t* samples, uint32_t sampleCount, bool looping,
                              uint8_t volume, bool muted, bool highPass = false);
void audioEngineClearVocalLayer();
void audioEngineSetVocalMix(uint8_t volume, bool muted);

// Live note triggers & Circle Pad
void audioEngineTriggerLiveNote(int trackIdx, uint8_t instrumentId, uint8_t midiNote, uint8_t velocity);
void audioEngineReleaseLiveNotes(int trackIdx);
void audioEngineSetCirclePad(float bendSemis, float vibrato);

// Read-only telemetry for UI rendering
int   audioEngineGetCurrentStep();
float audioEngineGetStepFraction();
float audioEngineGetPeakLevel();

// Metronome click & 4-beat count-in (v0.3.0 / v0.7.0)
void  audioEngineSetMetronome(bool enabled);
bool  audioEngineGetMetronome();
void  audioEngineStartCountIn(int beats = 4);
void  audioEngineCancelCountIn();
bool  audioEngineIsCountingIn();
int   audioEngineGetCountInBeatRemaining();
bool  audioEngineConsumeCountInFinished();

// Master volume, per-instrument trim, and Auto Speaker/Headphone EQ (v0.7.0)
void  audioEngineSetMasterConfig(uint8_t masterVolPct, uint8_t audioOutMode, const int8_t instTrimPct[INST_COUNT]);
void  audioEnginePollHeadphones();
bool  audioEngineIsHeadphonesInserted();
bool  audioEngineIsSpeakerVoicingActive();

} // namespace BardBeats
