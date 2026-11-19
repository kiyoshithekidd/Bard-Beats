#pragma once

#include "data_model.h"
#include <cstdint>

namespace BardBeats {

constexpr float BB_SAMPLE_RATE = 32728.498f;
constexpr int BB_MAX_VOICES = 16;

enum EnvStage : uint8_t {
    ENV_OFF = 0,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE,
    ENV_STEAL_FADE
};

struct Voice {
    bool     active;
    uint8_t  trackIdx;
    uint8_t  instrumentId;
    uint8_t  midiNote;
    uint8_t  velocity;
    bool     isLiveNote;

    EnvStage envStage;
    float    envLevel;
    float    gain;
    float    phase;
    float    phaseInc;
    float    baseFreq;
    float    notePitchBendSemis; // Per-note recorded bend
    float    samplePos;
    float    sampleInc;
    float    lpfState;
    uint32_t ageSamples;
    uint32_t lfsrState;
    int      samplesRemaining;   // Gate countdown for sequencer notes (-1 for held live)
};

class SynthVoices {
public:
    void init();
    void shutdown();

    void triggerNote(int trackIdx, uint8_t instrumentId, uint8_t midiNote,
                     uint8_t velocity, float stepBendSemis, int gateSamples, bool isLive);
    void releaseLiveNotes(int trackIdx);
    void releaseAllForTrack(int trackIdx);
    void stopAll();

    // Render numSamples stereo PCM16 frames (interleaved L, R)
    // liveBendSemis: Circle Pad X (-2.0 .. +2.0) applied to live notes
    // liveVibrato:   Circle Pad Y (0.0 .. 1.0) applied to live notes
    void render(int16_t* outStereo, int numSamples, float liveBendSemis, float liveVibrato,
                const uint8_t trackVolumes[BB_NUM_TRACKS], const uint8_t trackMuted[BB_NUM_TRACKS]);

    float getLastPeakLevel() const { return lastPeak_; }

private:
    Voice voices_[BB_MAX_VOICES];
    int16_t* luteTbl_ = nullptr;
    uint32_t luteLen_ = 0;
    int16_t* harpTbl_ = nullptr;
    uint32_t harpLen_ = 0;
    int16_t* bassTbl_ = nullptr;
    uint32_t bassLen_ = 0;

    float vibPhase_ = 0.0f;
    float smoothedLiveBend_ = 0.0f;
    float lastPeak_ = 0.0f;

    int allocateVoice(int trackIdx, uint8_t midiNote, uint8_t instrumentId);
    void loadWaveTable(const char* path, int16_t** outData, uint32_t* outLen);
    float renderVoiceSample(Voice& v, float liveBendSemis, float liveVibOffset);
};

} // namespace BardBeats
