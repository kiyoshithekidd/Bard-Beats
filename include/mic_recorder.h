#pragma once

#include <cstdint>
#include <vector>

namespace BardBeats {

constexpr uint32_t BB_MIC_SAMPLE_RATE = 16364; // Hardware MICU_SAMPLE_RATE_16360 (16364.479 Hz)

bool micRecorderInit();
void micRecorderExit();
bool micRecorderIsAvailable();

// Begin capturing into scratch buffer up to maxSamples (e.g., 1 bar for clip, or song duration)
bool micRecorderStart(uint32_t targetSamples);
// Poll incoming samples from mic:u shared memory ring buffer (call every frame while recording)
// Returns true if targetSamples was reached and recording auto-finished.
bool micRecorderPoll();
// Stop capture; if commit==true, applies 22ms latency compensation + fade and fills outSamples
void micRecorderStop(bool commit, std::vector<int16_t>& outSamples);
bool micRecorderIsRecording();

float micRecorderGetProgress(); // 0.0..1.0 of targetSamples
float micRecorderGetLiveLevel(); // 0.0..1.0 live mic meter

// Compute downsampled waveform envelope bars (0.0..1.0) for UI display
void micComputeWaveformBars(const std::vector<int16_t>& samples, float* outBars, int numBars);

} // namespace BardBeats
