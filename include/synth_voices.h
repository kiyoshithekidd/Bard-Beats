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
    float    phase2;
    float    phaseInc;
    float    baseFreq;
    float    notePitchMult;      // Precomputed pow(2, notePitchBendSemis/12)
    float    samplePos;
    float    sampleInc;
    float    lpfState;
    float    lpfState2;
    // Fast multiplicative envelopes (zero std::exp per sample!)
    float    drumAmpEnv;
    float    drumAmpDecay;
    float    drumPitchEnv;
    float    drumPitchDecay;
    float    drumNoiseEnv;
    float    drumNoiseDecay;
    float    filterEnv;
    float    filterDecay;

    uint32_t ageSamples;
    uint32_t rngState;
    int      samplesRemaining;
};

// Lightweight Tavern Room Stereo Reverb (2 comb + 1 all-pass per channel, fast on ARM11)
struct TavernReverb {
    static constexpr int COMB_L1 = 743;
    static constexpr int COMB_L2 = 907;
    static constexpr int COMB_R1 = 787;
    static constexpr int COMB_R2 = 953;
    static constexpr int AP_LEN  = 223;

    float cL1[COMB_L1];
    float cL2[COMB_L2];
    float cR1[COMB_R1];
    float cR2[COMB_R2];
    float apL[AP_LEN];
    float apR[AP_LEN];
    float lpL1, lpL2, lpR1, lpR2;
    int   iL1, iL2, iR1, iR2, iAp;

    void reset();
    void process(float inMono, float& outL, float& outR);
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

    void render(int16_t* outStereo, int numSamples, float liveBendSemis, float liveVibrato,
                const uint8_t trackVolumes[BB_NUM_TRACKS], const uint8_t trackMuted[BB_NUM_TRACKS],
                float reverbMix = 0.38f);

    float getLastPeakLevel() const { return lastPeak_; }

private:
    Voice voices_[BB_MAX_VOICES];
    int16_t* luteTbl_ = nullptr;
    uint32_t luteLen_ = 0;
    int16_t* harpTbl_ = nullptr;
    uint32_t harpLen_ = 0;
    int16_t* bassTbl_ = nullptr;
    uint32_t bassLen_ = 0;

    TavernReverb reverb_;
    float vibPhase_ = 0.0f;
    float smoothedLiveBend_ = 0.0f;
    float lastPeak_ = 0.0f;

    int allocateVoice(int trackIdx, uint8_t midiNote, uint8_t instrumentId);
    void loadWaveTable(const char* path, int16_t** outData, uint32_t* outLen);
    float renderVoiceSample(Voice& v, float livePitchMult);
};

} // namespace BardBeats
