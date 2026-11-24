#include "synth_voices.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace BardBeats {

static constexpr float TWO_PI = 6.28318530718f;
static constexpr int SIN_LUT_SIZE = 2048;
static float s_sinLut[SIN_LUT_SIZE];
static bool  s_lutInit = false;

static void initSinLut() {
    if (s_lutInit) return;
    for (int i = 0; i < SIN_LUT_SIZE; ++i) {
        s_sinLut[i] = std::sin((static_cast<float>(i) / static_cast<float>(SIN_LUT_SIZE)) * TWO_PI);
    }
    s_lutInit = true;
}

static inline float fastSin(float phase01) {
    phase01 -= std::floor(phase01);
    float fIdx = phase01 * static_cast<float>(SIN_LUT_SIZE);
    int idx0 = static_cast<int>(fIdx) & (SIN_LUT_SIZE - 1);
    int idx1 = (idx0 + 1) & (SIN_LUT_SIZE - 1);
    float frac = fIdx - std::floor(fIdx);
    return s_sinLut[idx0] + frac * (s_sinLut[idx1] - s_sinLut[idx0]);
}

// Smooth asymptotic saturation (never hard-clamps flat like polynomial > 1.5)
static inline float fastSoftClip(float x) {
    return std::tanh(x);
}

static inline float midiToFreq(uint8_t midiNote) {
    return 440.0f * std::pow(2.0f, (static_cast<int>(midiNote) - 69) / 12.0f);
}

static inline float nextWhiteNoise(uint32_t& state) {
    uint32_t x = state ? state : 0x6D2B79F5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return (static_cast<float>(static_cast<int32_t>(x)) * (1.0f / 2147483648.0f));
}

static inline float polyBlep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    } else if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// ---- Tavern Room Stereo Reverb ----

void TavernReverb::reset() {
    std::memset(this, 0, sizeof(TavernReverb));
}

void TavernReverb::process(float inMono, float& outL, float& outR) {
    constexpr float fb = 0.65f;
    constexpr float damp = 0.38f;

    float dL1 = cL1[iL1];
    lpL1 = dL1 * (1.0f - damp) + lpL1 * damp;
    cL1[iL1] = inMono + lpL1 * fb;
    if (++iL1 >= COMB_L1) iL1 = 0;

    float dL2 = cL2[iL2];
    lpL2 = dL2 * (1.0f - damp) + lpL2 * damp;
    cL2[iL2] = inMono + lpL2 * (fb * 0.95f);
    if (++iL2 >= COMB_L2) iL2 = 0;

    float dR1 = cR1[iR1];
    lpR1 = dR1 * (1.0f - damp) + lpR1 * damp;
    cR1[iR1] = inMono + lpR1 * fb;
    if (++iR1 >= COMB_R1) iR1 = 0;

    float dR2 = cR2[iR2];
    lpR2 = dR2 * (1.0f - damp) + lpR2 * damp;
    cR2[iR2] = inMono + lpR2 * (fb * 0.95f);
    if (++iR2 >= COMB_R2) iR2 = 0;

    float sumL = (dL1 + dL2) * 0.5f;
    float sumR = (dR1 + dR2) * 0.5f;

    float bufL = apL[iAp];
    outL = -sumL * 0.5f + bufL;
    apL[iAp] = sumL + bufL * 0.5f;

    float bufR = apR[iAp];
    outR = -sumR * 0.5f + bufR;
    apR[iAp] = sumR + bufR * 0.5f;

    if (++iAp >= AP_LEN) iAp = 0;
}

// ---- Stereo Biquad EQ (3DS Speaker Voicing) ----

void StereoBiquad::resetState() {
    z1L = z2L = z1R = z2R = 0.0f;
}

void StereoBiquad::setHighPass(float freqHz, float q, float sampleRate) {
    float w0 = TWO_PI * freqHz / sampleRate;
    float cosw = std::cos(w0);
    float sinw = std::sin(w0);
    float alpha = sinw / (2.0f * q);
    float invA0 = 1.0f / (1.0f + alpha);
    b0 = ((1.0f + cosw) * 0.5f) * invA0;
    b1 = (-(1.0f + cosw)) * invA0;
    b2 = ((1.0f + cosw) * 0.5f) * invA0;
    a1 = (-2.0f * cosw) * invA0;
    a2 = (1.0f - alpha) * invA0;
    resetState();
}

void StereoBiquad::setPeaking(float freqHz, float q, float gainDb, float sampleRate) {
    float w0 = TWO_PI * freqHz / sampleRate;
    float cosw = std::cos(w0);
    float sinw = std::sin(w0);
    float alpha = sinw / (2.0f * q);
    float A = std::pow(10.0f, gainDb / 40.0f);
    float invA0 = 1.0f / (1.0f + alpha / A);
    b0 = (1.0f + alpha * A) * invA0;
    b1 = (-2.0f * cosw) * invA0;
    b2 = (1.0f - alpha * A) * invA0;
    a1 = (-2.0f * cosw) * invA0;
    a2 = (1.0f - alpha / A) * invA0;
    resetState();
}

void StereoBiquad::process(float& l, float& r) {
    float outL = b0 * l + z1L;
    z1L = b1 * l - a1 * outL + z2L;
    z2L = b2 * l - a2 * outL;
    l = outL;

    float outR = b0 * r + z1R;
    z1R = b1 * r - a1 * outR + z2R;
    z2R = b2 * r - a2 * outR;
    r = outR;
}

// ---- SynthVoices ----

void SynthVoices::loadWaveTable(const char* path, int16_t** outData, uint32_t* outLen) {
    *outData = nullptr;
    *outLen = 0;
    FILE* f = std::fopen(path, "rb");
    if (!f) return;
    uint32_t count = 0;
    if (std::fread(&count, sizeof(uint32_t), 1, f) == 1 && count > 0 && count < 200000) {
        int16_t* buf = static_cast<int16_t*>(std::malloc(count * sizeof(int16_t)));
        if (buf) {
            size_t rd = std::fread(buf, sizeof(int16_t), count, f);
            if (rd == count) {
                *outData = buf;
                *outLen = count;
            } else {
                std::free(buf);
            }
        }
    }
    std::fclose(f);
}

void SynthVoices::init() {
    initSinLut();
    std::memset(voices_, 0, sizeof(voices_));
    reverb_.reset();
    speakerHpf_.setHighPass(135.0f, 0.707f, BB_SAMPLE_RATE);
    speakerPresence_.setPeaking(1800.0f, 1.1f, 2.8f, BB_SAMPLE_RATE);
    masterVol_ = 1.0f;
    speakerMode_ = true;
    std::memset(instTrimPct_, 0, sizeof(instTrimPct_));
    compEnv_ = 0.0f;
    metroPhase_ = 0.0f;
    metroFreq_ = 1040.0f;
    metroEnv_ = 0.0f;
    loadWaveTable("romfs:/audio/lute_tbl.bin", &luteTbl_, &luteLen_);
    loadWaveTable("romfs:/audio/harp_tbl.bin", &harpTbl_, &harpLen_);
    loadWaveTable("romfs:/audio/bass_tbl.bin", &bassTbl_, &bassLen_);
    vibPhase_ = 0.0f;
    smoothedLiveBend_ = 0.0f;
    lastPeak_ = 0.0f;
}

void SynthVoices::setMasterConfig(uint8_t masterVolPct, bool speakerMode, const int8_t instTrimPct[INST_COUNT]) {
    if (masterVolPct > 150) masterVolPct = 150;
    masterVol_ = static_cast<float>(masterVolPct) * 0.01f;
    if (speakerMode_ != speakerMode) {
        speakerMode_ = speakerMode;
        speakerHpf_.resetState();
        speakerPresence_.resetState();
    }
    if (instTrimPct) {
        std::memcpy(instTrimPct_, instTrimPct, INST_COUNT);
    }
}

void SynthVoices::triggerMetronome(bool accented) {
    metroPhase_ = 0.0f;
    metroFreq_ = accented ? 1480.0f : 1040.0f;
    metroEnv_  = accented ? 0.42f : 0.28f;
}

void SynthVoices::shutdown() {
    std::free(luteTbl_); luteTbl_ = nullptr; luteLen_ = 0;
    std::free(harpTbl_); harpTbl_ = nullptr; harpLen_ = 0;
    std::free(bassTbl_); bassTbl_ = nullptr; bassLen_ = 0;
}

int SynthVoices::allocateVoice(int trackIdx, uint8_t midiNote, uint8_t instrumentId) {
    (void)instrumentId;
    if (trackIdx == TRACK_DRUMS) {
        for (int i = 0; i < BB_MAX_VOICES; ++i) {
            if (voices_[i].active && voices_[i].trackIdx == TRACK_DRUMS) {
                if (voices_[i].midiNote == midiNote ||
                    (midiNote == DRUM_CLOSED_HAT && voices_[i].midiNote == DRUM_OPEN_HAT)) {
                    return i;
                }
            }
        }
    }

    for (int i = 0; i < BB_MAX_VOICES; ++i) {
        if (!voices_[i].active) return i;
    }

    int bestSame = -1;
    uint32_t maxAgeSame = 0;
    int bestAny = 0;
    uint32_t maxAgeAny = 0;
    for (int i = 0; i < BB_MAX_VOICES; ++i) {
        if (voices_[i].trackIdx == trackIdx && voices_[i].ageSamples >= maxAgeSame) {
            maxAgeSame = voices_[i].ageSamples;
            bestSame = i;
        }
        if (voices_[i].ageSamples >= maxAgeAny) {
            maxAgeAny = voices_[i].ageSamples;
            bestAny = i;
        }
    }
    return (bestSame >= 0) ? bestSame : bestAny;
}

// Base RMS-balanced gain per instrument (calibrated for 3DS speakers & headphones)
static constexpr float INST_BASE_GAIN[INST_COUNT] = {
    0.72f, // INST_TAVERN_KIT
    0.68f, // INST_CHIP_KIT
    0.84f, // INST_UPRIGHT_BASS
    0.76f, // INST_SUB_BASS
    0.92f, // INST_OCARINA (warm presence boost so solo melodies cut cleanly)
    0.66f, // INST_PULSE_LEAD
    0.86f, // INST_LUTE
    0.90f, // INST_HARP
    0.72f, // INST_WAR_KIT
    0.70f, // INST_LOFI_KIT
    0.80f, // INST_ELEC_BASS
    0.90f, // INST_WOOD_FLUTE
    0.68f, // INST_SYNTH_BRASS
    0.78f, // INST_ELEC_GUITAR
    0.84f, // INST_TAVERN_PIANO
    0.82f  // INST_ELEC_KEYS
};

void SynthVoices::triggerNote(int trackIdx, uint8_t instrumentId, uint8_t midiNote,
                              uint8_t velocity, float stepBendSemis, int gateSamples, bool isLive) {
    if (midiNote == 0 || velocity == 0) return;

    int idx = allocateVoice(trackIdx, midiNote, instrumentId);
    Voice& v = voices_[idx];
    float prevOut = v.active ? v.lastSample : 0.0f;
    std::memset(&v, 0, sizeof(Voice));
    v.active = true;
    v.trackIdx = static_cast<uint8_t>(trackIdx);
    v.instrumentId = instrumentId;
    v.midiNote = midiNote;
    v.velocity = velocity;
    v.isLiveNote = isLive;
    v.envStage = ENV_ATTACK;
    v.envLevel = 0.0f;
    float baseInstGain = (instrumentId < INST_COUNT) ? INST_BASE_GAIN[instrumentId] : 0.78f;
    int8_t trimPct = (instrumentId < INST_COUNT) ? instTrimPct_[instrumentId] : 0;
    float trimMult = 1.0f + static_cast<float>(trimPct) * 0.01f;
    if (trimMult < 0.25f) trimMult = 0.25f;
    if (trimMult > 1.75f) trimMult = 1.75f;
    v.gain = (static_cast<float>(velocity) / 127.0f) * baseInstGain * trimMult;
    v.baseFreq = midiToFreq(midiNote);
    v.phaseInc = v.baseFreq / BB_SAMPLE_RATE;
    v.notePitchMult = (stepBendSemis != 0.0f) ? std::pow(2.0f, stepBendSemis / 12.0f) : 1.0f;
    v.rngState = 0x9E3779B9u ^ (static_cast<uint32_t>(idx + 1) * 2654435761u) ^ (midiNote * 7919u);
    v.samplesRemaining = gateSamples;
    v.stealPrevSample = prevOut;
    v.stealFade = (std::fabs(prevOut) > 0.0001f) ? 1.0f : 0.0f;

    // Precompute multiplicative per-sample decay coefficients (zero std::exp in render loop!)
    v.drumAmpEnv   = 1.0f;
    v.drumPitchEnv = 1.0f;
    v.drumNoiseEnv = 1.0f;
    v.filterEnv    = 1.0f;
    v.filterDecay  = std::exp(-10.0f / BB_SAMPLE_RATE);

    bool chip = (instrumentId == INST_CHIP_KIT);
    bool war  = (instrumentId == INST_WAR_KIT);
    bool lofi = (instrumentId == INST_LOFI_KIT);
    if (instrumentId == INST_TAVERN_KIT || chip || war || lofi) {
        switch (midiNote) {
            case DRUM_KICK:
                v.drumAmpDecay   = std::exp(-(chip ? 14.0f : (war ? 7.8f : (lofi ? 12.5f : 10.5f))) / BB_SAMPLE_RATE);
                v.drumPitchDecay = std::exp(-(chip ? 38.0f : (war ? 22.0f : 30.0f)) / BB_SAMPLE_RATE);
                v.drumNoiseDecay = std::exp(-360.0f / BB_SAMPLE_RATE);
                break;
            case DRUM_TOM:
                v.drumAmpDecay   = std::exp(-(war ? 7.5f : 11.5f) / BB_SAMPLE_RATE);
                v.drumPitchDecay = std::exp(-(war ? 18.0f : 24.0f) / BB_SAMPLE_RATE);
                break;
            case DRUM_SNARE:
                v.drumAmpDecay   = std::exp(-(lofi ? 26.0f : 22.0f) / BB_SAMPLE_RATE);
                v.drumPitchDecay = std::exp(-45.0f / BB_SAMPLE_RATE);
                v.drumNoiseDecay = std::exp(-(chip ? 19.0f : (war ? 11.5f : 14.5f)) / BB_SAMPLE_RATE);
                break;
            case DRUM_CLAP:
                v.drumAmpDecay   = std::exp(-(lofi ? 17.0f : 13.0f) / BB_SAMPLE_RATE);
                break;
            case DRUM_CLOSED_HAT:
                v.drumAmpDecay   = std::exp(-(lofi ? 48.0f : 38.0f) / BB_SAMPLE_RATE);
                break;
            case DRUM_OPEN_HAT:
                v.drumAmpDecay   = std::exp(-7.5f / BB_SAMPLE_RATE);
                break;
            case DRUM_TAMB:
                v.drumAmpDecay   = std::exp(-15.0f / BB_SAMPLE_RATE);
                break;
            default:
                v.drumAmpDecay   = std::exp(-24.0f / BB_SAMPLE_RATE);
                break;
        }
    }

    if (instrumentId == INST_LUTE && luteTbl_) {
        v.sampleInc = v.baseFreq / 146.832f;
    } else if (instrumentId == INST_HARP && harpTbl_) {
        v.sampleInc = v.baseFreq / 293.665f;
    } else if (instrumentId == INST_UPRIGHT_BASS && bassTbl_) {
        v.sampleInc = v.baseFreq / 73.416f;
    } else {
        v.sampleInc = 1.0f;
    }
}

void SynthVoices::releaseLiveNotes(int trackIdx) {
    for (int i = 0; i < BB_MAX_VOICES; ++i) {
        Voice& v = voices_[i];
        if (v.active && v.isLiveNote && (trackIdx < 0 || v.trackIdx == trackIdx)) {
            if (v.envStage != ENV_OFF && v.envStage != ENV_RELEASE) {
                v.envStage = ENV_RELEASE;
            }
        }
    }
}

void SynthVoices::releaseAllForTrack(int trackIdx) {
    for (int i = 0; i < BB_MAX_VOICES; ++i) {
        if (voices_[i].active && voices_[i].trackIdx == trackIdx) {
            voices_[i].envStage = ENV_RELEASE;
        }
    }
}

void SynthVoices::stopAll() {
    for (int i = 0; i < BB_MAX_VOICES; ++i) {
        voices_[i].active = false;
        voices_[i].envStage = ENV_OFF;
    }
    compEnv_ = 0.0f;
    metroEnv_ = 0.0f;
}

float SynthVoices::renderVoiceSample(Voice& v, float livePitchMult) {
    bool isDrum = (v.instrumentId == INST_TAVERN_KIT || v.instrumentId == INST_CHIP_KIT ||
                   v.instrumentId == INST_WAR_KIT || v.instrumentId == INST_LOFI_KIT);
    if (isDrum) {
        bool chip = (v.instrumentId == INST_CHIP_KIT);
        bool war  = (v.instrumentId == INST_WAR_KIT);
        bool lofi = (v.instrumentId == INST_LOFI_KIT);
        float raw = 0.0f;
        switch (v.midiNote) {
            case DRUM_KICK: {
                float fStart = chip ? 240.0f : (war ? 145.0f : (lofi ? 195.0f : 175.0f));
                float fEnd   = chip ? 56.0f  : (war ? 44.0f  : (lofi ? 52.0f  : 62.0f));
                float sweepFreq = fEnd + (fStart - fEnd) * v.drumPitchEnv;
                v.phase += sweepFreq * (1.0f / BB_SAMPLE_RATE);
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                float osc = chip ? (1.0f - 4.0f * std::fabs(v.phase - 0.5f))
                                 : fastSin(v.phase);
                float body = fastSoftClip(osc * (war ? 1.65f : 1.45f)) * v.drumAmpEnv;
                float click = (v.ageSamples < 260) ? (nextWhiteNoise(v.rngState) * v.drumNoiseEnv * (lofi ? 0.45f : 0.35f)) : 0.0f;
                raw = (body + click) * 1.25f;
                v.drumAmpEnv   *= v.drumAmpDecay;
                v.drumPitchEnv *= v.drumPitchDecay;
                v.drumNoiseEnv *= v.drumNoiseDecay;
                if (v.drumAmpEnv < 0.002f) v.active = false;
                break;
            }
            case DRUM_TOM: {
                float sweepFreq = (war ? 78.0f : 105.0f) + (war ? 75.0f : 100.0f) * v.drumPitchEnv;
                v.phase  += sweepFreq * (1.0f / BB_SAMPLE_RATE);
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                v.phase2 += (sweepFreq * 1.59f) * (1.0f / BB_SAMPLE_RATE);
                if (v.phase2 >= 1.0f) v.phase2 -= 1.0f;
                float skin = fastSin(v.phase) + (war ? 0.34f : 0.22f) * fastSin(v.phase2);
                raw = skin * v.drumAmpEnv * 0.90f;
                v.drumAmpEnv   *= v.drumAmpDecay;
                v.drumPitchEnv *= v.drumPitchDecay;
                if (v.drumAmpEnv < 0.002f) v.active = false;
                break;
            }
            case DRUM_SNARE: {
                float bodyFreq = (chip ? 220.0f : (war ? 150.0f : 185.0f)) + 75.0f * v.drumPitchEnv;
                v.phase += bodyFreq * (1.0f / BB_SAMPLE_RATE);
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                float body = fastSin(v.phase) * v.drumAmpEnv;
                float n = nextWhiteNoise(v.rngState);
                v.lpfState  += 0.55f * (n - v.lpfState);
                v.lpfState2 += 0.14f * (v.lpfState - v.lpfState2);
                float wires = (v.lpfState - v.lpfState2) * v.drumNoiseEnv;
                raw = 0.50f * body + 0.85f * wires;
                v.drumAmpEnv   *= v.drumAmpDecay;
                v.drumPitchEnv *= v.drumPitchDecay;
                v.drumNoiseEnv *= v.drumNoiseDecay;
                if (v.drumNoiseEnv < 0.002f && v.drumAmpEnv < 0.002f) v.active = false;
                break;
            }
            case DRUM_CLAP: {
                float burst = 1.0f;
                if (v.ageSamples < 980) {
                    uint32_t m = v.ageSamples % 327;
                    burst = (m < 140) ? 1.25f : 0.30f;
                }
                float n = nextWhiteNoise(v.rngState);
                v.lpfState  += 0.42f * (n - v.lpfState);
                v.lpfState2 += 0.16f * (v.lpfState - v.lpfState2);
                raw = (v.lpfState - v.lpfState2) * burst * v.drumAmpEnv * 1.25f;
                v.drumAmpEnv *= v.drumAmpDecay;
                if (v.drumAmpEnv < 0.002f) v.active = false;
                break;
            }
            case DRUM_CLOSED_HAT:
            case DRUM_OPEN_HAT: {
                float n = nextWhiteNoise(v.rngState);
                v.lpfState += 0.55f * (n - v.lpfState);
                float hp = n - v.lpfState;
                v.phase  += 6420.0f * (1.0f / BB_SAMPLE_RATE); if (v.phase >= 1.0f) v.phase -= 1.0f;
                v.phase2 += 8130.0f * (1.0f / BB_SAMPLE_RATE); if (v.phase2 >= 1.0f) v.phase2 -= 1.0f;
                float metal = 0.20f * ((v.phase < 0.5f ? 1.0f : -1.0f) * (v.phase2 < 0.5f ? 1.0f : -1.0f));
                raw = (0.78f * hp + metal) * v.drumAmpEnv * 0.72f;
                v.drumAmpEnv *= v.drumAmpDecay;
                if (v.drumAmpEnv < 0.002f) v.active = false;
                break;
            }
            case DRUM_TAMB: {
                float n = nextWhiteNoise(v.rngState);
                v.lpfState += 0.45f * (n - v.lpfState);
                float hp = n - v.lpfState;
                v.phase  += 5280.0f * (1.0f / BB_SAMPLE_RATE); if (v.phase >= 1.0f) v.phase -= 1.0f;
                v.phase2 += 7410.0f * (1.0f / BB_SAMPLE_RATE); if (v.phase2 >= 1.0f) v.phase2 -= 1.0f;
                float jingle = 0.32f * (fastSin(v.phase) + fastSin(v.phase2));
                raw = (0.65f * hp + jingle) * v.drumAmpEnv * 0.70f;
                v.drumAmpEnv *= v.drumAmpDecay;
                if (v.drumAmpEnv < 0.002f) v.active = false;
                break;
            }
            default: { // DRUM_SHAKER
                float n = nextWhiteNoise(v.rngState);
                v.lpfState += 0.50f * (n - v.lpfState);
                float hp = n - v.lpfState;
                float swell = (v.ageSamples < 580) ? (static_cast<float>(v.ageSamples) * (1.0f / 580.0f)) : 1.0f;
                raw = hp * swell * v.drumAmpEnv * 0.60f;
                v.drumAmpEnv *= v.drumAmpDecay;
                if (v.drumAmpEnv < 0.002f) v.active = false;
                break;
            }
        }
        v.ageSamples++;
        float out = raw * v.gain;
        if (v.stealFade > 0.0f) {
            out += v.stealPrevSample * v.stealFade;
            v.stealFade -= (1.0f / 82.0f); // 2.5 ms linear fade-out
            if (v.stealFade < 0.0f) v.stealFade = 0.0f;
        }
        v.lastSample = out;
        return out;
    }

    // ---- Melodic & Harmonic Instruments ----

    if (!v.isLiveNote && v.samplesRemaining > 0) {
        v.samplesRemaining--;
        if (v.samplesRemaining == 0 && v.envStage != ENV_RELEASE) {
            v.envStage = ENV_RELEASE;
        }
    }

    switch (v.envStage) {
        case ENV_ATTACK: {
            float atkStep = 0.020f;
            if (v.instrumentId == INST_OCARINA || v.instrumentId == INST_WOOD_FLUTE) atkStep = 0.0025f;
            else if (v.instrumentId == INST_SYNTH_BRASS) atkStep = 0.0032f;
            else if (v.instrumentId == INST_HARP || v.instrumentId == INST_TAVERN_PIANO) atkStep = 0.028f;
            v.envLevel += atkStep;
            if (v.envLevel >= 1.0f) {
                v.envLevel = 1.0f;
                v.envStage = ENV_DECAY;
            }
            break;
        }
        case ENV_DECAY: {
            float sus = 0.78f;
            float decStep = 0.00018f;
            if (v.instrumentId == INST_LUTE || v.instrumentId == INST_HARP || v.instrumentId == INST_UPRIGHT_BASS) {
                sus = 0.96f;
                decStep = 0.00005f;
            } else if (v.instrumentId == INST_TAVERN_PIANO || v.instrumentId == INST_ELEC_GUITAR) {
                sus = 0.42f;
                decStep = 0.00014f;
            } else if (v.instrumentId == INST_ELEC_KEYS || v.instrumentId == INST_ELEC_BASS) {
                sus = 0.58f;
                decStep = 0.00012f;
            } else if (v.instrumentId == INST_PULSE_LEAD) {
                sus = 0.65f;
                decStep = 0.00030f;
            }
            v.envLevel -= decStep;
            if (v.envLevel <= sus) {
                v.envLevel = sus;
                v.envStage = ENV_SUSTAIN;
            }
            break;
        }
        case ENV_SUSTAIN:
            break;
        case ENV_RELEASE: {
            float relStep = 0.0009f;
            if (v.instrumentId == INST_LUTE || v.instrumentId == INST_HARP || v.instrumentId == INST_TAVERN_PIANO) relStep = 0.00028f;
            else if (v.instrumentId == INST_OCARINA || v.instrumentId == INST_WOOD_FLUTE) relStep = 0.0011f;
            v.envLevel -= relStep;
            if (v.envLevel <= 0.0f) {
                v.envLevel = 0.0f;
                v.envStage = ENV_OFF;
                v.active = false;
            }
            break;
        }
        case ENV_STEAL_FADE:
        case ENV_OFF:
            v.active = false;
            return 0.0f;
    }

    float raw = 0.0f;
    float pitchMult = v.notePitchMult * (v.isLiveNote ? livePitchMult : 1.0f);

    const int16_t* tbl = nullptr;
    uint32_t tblLen = 0;
    if (v.instrumentId == INST_LUTE)             { tbl = luteTbl_; tblLen = luteLen_; }
    else if (v.instrumentId == INST_HARP)        { tbl = harpTbl_; tblLen = harpLen_; }
    else if (v.instrumentId == INST_UPRIGHT_BASS){ tbl = bassTbl_; tblLen = bassLen_; }

    if (tbl && tblLen > 4) {
        uint32_t idx0 = static_cast<uint32_t>(v.samplePos);
        if (idx0 >= tblLen - 2) {
            v.active = false;
            return 0.0f;
        }
        float frac = v.samplePos - static_cast<float>(idx0);
        float s0 = static_cast<float>(tbl[idx0]) * (1.0f / 32768.0f);
        float s1 = static_cast<float>(tbl[idx0 + 1]) * (1.0f / 32768.0f);
        raw = s0 + frac * (s1 - s0);
        if (v.instrumentId == INST_UPRIGHT_BASS) raw *= 1.18f;
        v.samplePos += v.sampleInc * pitchMult;
    } else if (v.instrumentId == INST_OCARINA) {
        // Clean Helmholtz Clay Ocarina: pure sine-rich tone, gentle scoop, brief 27ms attack chiff (zero continuous hiss!)
        float scoop = (v.ageSamples < 1100)
            ? (1.0f - 0.010f * (1.0f - static_cast<float>(v.ageSamples) * (1.0f / 1100.0f)))
            : 1.0f;
        v.phase2 += 5.0f * (1.0f / BB_SAMPLE_RATE);
        if (v.phase2 >= 1.0f) v.phase2 -= 1.0f;
        float vib = (v.ageSamples > 4800) ? (1.0f + 0.0055f * fastSin(v.phase2)) : 1.0f;

        v.phase += v.phaseInc * pitchMult * scoop * vib;
        if (v.phase >= 1.0f) v.phase -= static_cast<float>(static_cast<int>(v.phase));

        float f1 = fastSin(v.phase);
        float f2 = 0.07f * fastSin(v.phase * 2.0f);
        float f3 = 0.015f * fastSin(v.phase * 3.0f);

        float chiff = 0.0f;
        if (v.ageSamples < 900) {
            float n = nextWhiteNoise(v.rngState);
            v.lpfState  += 0.20f * (n - v.lpfState);
            v.lpfState2 += 0.08f * (v.lpfState - v.lpfState2);
            float tChiff = 1.0f - static_cast<float>(v.ageSamples) * (1.0f / 900.0f);
            chiff = (v.lpfState - v.lpfState2) * (0.07f * tChiff);
        }
        raw = (f1 + f2 + f3) * 0.80f + chiff;
    } else if (v.instrumentId == INST_WOOD_FLUTE) {
        // Airy Medieval Wooden Flute: polyphonic-friendly gain + gentle 2nd/3rd harmonics + soft breath chiff
        v.phase2 += 5.4f * (1.0f / BB_SAMPLE_RATE);
        if (v.phase2 >= 1.0f) v.phase2 -= 1.0f;
        float vib = (v.ageSamples > 4200) ? (1.0f + 0.006f * fastSin(v.phase2)) : 1.0f;

        v.phase += v.phaseInc * pitchMult * vib;
        if (v.phase >= 1.0f) v.phase -= static_cast<float>(static_cast<int>(v.phase));

        float tri = 1.0f - 4.0f * std::fabs(v.phase - 0.5f);
        float tone = 0.68f * fastSin(v.phase) + 0.22f * tri + 0.12f * fastSin(v.phase * 2.0f) + 0.05f * fastSin(v.phase * 3.0f);
        float n = nextWhiteNoise(v.rngState);
        v.lpfState  += 0.24f * (n - v.lpfState);
        v.lpfState2 += 0.10f * (v.lpfState - v.lpfState2);
        float breathAmt = (v.ageSamples < 1400) ? (0.09f * (1.0f - static_cast<float>(v.ageSamples) * (1.0f / 1400.0f))) : 0.012f;
        raw = tone * 0.80f + (v.lpfState - v.lpfState2) * breathAmt;
    } else if (v.instrumentId == INST_PULSE_LEAD) {
        float dt1 = v.phaseInc * pitchMult;
        float dt2 = dt1 * 1.0035f;
        v.phase  += dt1; if (v.phase >= 1.0f) v.phase -= static_cast<float>(static_cast<int>(v.phase));
        v.phase2 += dt2; if (v.phase2 >= 1.0f) v.phase2 -= static_cast<float>(static_cast<int>(v.phase2));

        auto blepPulse = [](float p, float dt, float duty) {
            float val = (p < duty) ? 1.0f : -1.0f;
            val += polyBlep(p, dt);
            float p2 = p - duty;
            if (p2 < 0.0f) p2 += 1.0f;
            val -= polyBlep(p2, dt);
            return val;
        };

        float osc = 0.55f * blepPulse(v.phase, dt1, 0.25f) + 0.35f * blepPulse(v.phase2, dt2, 0.50f);
        float cutoff = 0.22f + 0.45f * v.filterEnv;
        v.filterEnv *= v.filterDecay;
        v.lpfState  += cutoff * (osc - v.lpfState);
        v.lpfState2 += cutoff * (v.lpfState - v.lpfState2);
        raw = v.lpfState2 * 0.85f;
    } else if (v.instrumentId == INST_SYNTH_BRASS) {
        // Modern detuned dual-saw Synth Brass with warm filter swell
        float dt1 = v.phaseInc * pitchMult;
        float dt2 = dt1 * 1.005f;
        v.phase  += dt1; if (v.phase >= 1.0f) v.phase -= static_cast<float>(static_cast<int>(v.phase));
        v.phase2 += dt2; if (v.phase2 >= 1.0f) v.phase2 -= static_cast<float>(static_cast<int>(v.phase2));
        float saw1 = (2.0f * v.phase - 1.0f) - polyBlep(v.phase, dt1);
        float saw2 = (2.0f * v.phase2 - 1.0f) - polyBlep(v.phase2, dt2);
        float osc = 0.50f * (saw1 + saw2);
        float cutoff = 0.12f + 0.42f * v.envLevel;
        v.lpfState  += cutoff * (osc - v.lpfState);
        v.lpfState2 += cutoff * (v.lpfState - v.lpfState2);
        raw = v.lpfState2 * 0.85f;
    } else if (v.instrumentId == INST_ELEC_BASS) {
        // Punchy modern fingered/picked Electric Bass
        float dt = v.phaseInc * pitchMult;
        v.phase += dt; if (v.phase >= 1.0f) v.phase -= static_cast<float>(static_cast<int>(v.phase));
        float f = fastSin(v.phase) + 0.45f * fastSin(v.phase * 2.0f) + 0.20f * v.filterEnv * fastSin(v.phase * 3.0f);
        v.filterEnv *= v.filterDecay;
        raw = fastSoftClip(f * 0.85f) * 0.90f;
    } else if (v.instrumentId == INST_ELEC_GUITAR) {
        // Clean/warm Electric Guitar with harmonic pluck & gentle amp warmth
        float dt = v.phaseInc * pitchMult;
        v.phase += dt; if (v.phase >= 1.0f) v.phase -= static_cast<float>(static_cast<int>(v.phase));
        float h = v.filterEnv;
        v.filterEnv *= v.filterDecay;
        float str = fastSin(v.phase)
                  + (0.48f * h + 0.14f) * fastSin(v.phase * 2.0f)
                  + (0.30f * h) * fastSin(v.phase * 3.0f)
                  + (0.16f * h) * fastSin(v.phase * 4.0f);
        raw = fastSoftClip(str * 0.75f) * 0.84f;
    } else if (v.instrumentId == INST_TAVERN_PIANO) {
        // Acoustic Upright Piano: hammer transient + warm decaying upper partials
        float dt = v.phaseInc * pitchMult;
        v.phase += dt; if (v.phase >= 1.0f) v.phase -= static_cast<float>(static_cast<int>(v.phase));
        float h = v.filterEnv;
        v.filterEnv *= v.filterDecay;
        float body = fastSin(v.phase)
                   + (0.38f * h + 0.08f) * fastSin(v.phase * 2.0f)
                   + (0.22f * h) * fastSin(v.phase * 3.0f)
                   + (0.12f * h * h) * fastSin(v.phase * 4.0f);
        raw = body * 0.84f;
    } else if (v.instrumentId == INST_ELEC_KEYS) {
        // Warm Rhodes-style Electric Piano: fundamental + bell tine on attack
        float dt = v.phaseInc * pitchMult;
        v.phase += dt; if (v.phase >= 1.0f) v.phase -= static_cast<float>(static_cast<int>(v.phase));
        float h = v.filterEnv;
        v.filterEnv *= v.filterDecay;
        float tine = 0.22f * h * h * fastSin(v.phase * 7.0f);
        float body = fastSin(v.phase) + 0.18f * h * fastSin(v.phase * 2.0f) + tine;
        raw = body * 0.84f;
    } else {
        float dt = v.phaseInc * pitchMult;
        v.phase += dt;
        if (v.phase >= 1.0f) v.phase -= static_cast<float>(static_cast<int>(v.phase));

        float saw = (2.0f * v.phase - 1.0f) - polyBlep(v.phase, dt);
        float sub = fastSin(v.phase);
        float mix = 0.55f * saw + 0.55f * sub;
        float cutoff = 0.14f + 0.32f * v.filterEnv;
        v.filterEnv *= v.filterDecay;
        v.lpfState  += cutoff * (mix - v.lpfState);
        v.lpfState2 += cutoff * (v.lpfState - v.lpfState2);
        raw = fastSoftClip(v.lpfState2 * 1.4f) * 0.95f;
    }

    v.ageSamples++;
    float out = raw * v.envLevel * v.gain;
    if (v.stealFade > 0.0f) {
        out += v.stealPrevSample * v.stealFade;
        v.stealFade -= (1.0f / 82.0f); // 2.5 ms linear fade-out
        if (v.stealFade < 0.0f) v.stealFade = 0.0f;
    }
    v.lastSample = out;
    return out;
}

void SynthVoices::render(int16_t* outStereo, int numSamples, float liveBendSemis, float liveVibrato,
                         const uint8_t trackVolumes[BB_NUM_TRACKS], const uint8_t trackMuted[BB_NUM_TRACKS],
                         float reverbMix) {
    float peak = 0.0f;
    const float vibFreqInc = 5.5f / BB_SAMPLE_RATE;

    static constexpr float PAN_L[BB_NUM_TRACKS]    = { 1.00f, 1.00f, 0.86f, 1.12f };
    static constexpr float PAN_R[BB_NUM_TRACKS]    = { 1.00f, 1.00f, 1.12f, 0.86f };
    static constexpr float REV_SEND[BB_NUM_TRACKS] = { 0.14f, 0.04f, 0.26f, 0.24f };

    // Master bus compressor/limiter constants (~0.8 ms attack, ~65 ms release at 32.7 kHz)
    constexpr float COMP_ATK   = 0.038f;
    constexpr float COMP_REL   = 0.00048f;
    constexpr float COMP_THR   = 0.56f;
    constexpr float COMP_RATIO = 4.5f;
    const float preGain = 1.32f * masterVol_;

    float livePitchMult = 1.0f;

    for (int s = 0; s < numSamples; ++s) {
        // Update live pitch-bend multiplier at control rate (every 32 samples = ~1022 Hz)
        if ((s & 31) == 0) {
            smoothedLiveBend_ += 0.35f * (liveBendSemis - smoothedLiveBend_);
            vibPhase_ += vibFreqInc * 32.0f;
            if (vibPhase_ >= 1.0f) vibPhase_ -= 1.0f;
            float vibOffset = liveVibrato * 0.35f * fastSin(vibPhase_);
            float totalSemis = smoothedLiveBend_ + vibOffset;
            livePitchMult = (totalSemis != 0.0f) ? std::pow(2.0f, totalSemis / 12.0f) : 1.0f;
        }

        float dryL = 0.0f;
        float dryR = 0.0f;
        float revIn = 0.0f;

        for (int i = 0; i < BB_MAX_VOICES; ++i) {
            Voice& v = voices_[i];
            if (!v.active) continue;
            if (trackMuted[v.trackIdx]) {
                renderVoiceSample(v, livePitchMult);
                continue;
            }
            float vol = static_cast<float>(std::min<uint8_t>(trackVolumes[v.trackIdx], 200)) * 0.01f;
            float smp = renderVoiceSample(v, livePitchMult) * vol;
            dryL  += smp * PAN_L[v.trackIdx];
            dryR  += smp * PAN_R[v.trackIdx];
            revIn += smp * REV_SEND[v.trackIdx];
        }

        float wetL = 0.0f, wetR = 0.0f;
        reverb_.process(revIn, wetL, wetR);

        float mixL = dryL + wetL * reverbMix;
        float mixR = dryR + wetR * reverbMix;

        // Pre-limiter woodblock metronome (never hard-clips integer PCM)
        if (metroEnv_ > 0.001f) {
            float clk = fastSin(metroPhase_) * metroEnv_;
            metroPhase_ += metroFreq_ * (1.0f / BB_SAMPLE_RATE);
            if (metroPhase_ >= 1.0f) metroPhase_ -= 1.0f;
            metroEnv_ *= 0.991f;
            mixL += clk;
            mixR += clk;
        }

        // 3DS Speaker Voicing: 135 Hz Butterworth HPF (eliminates cone rattle) + 1.8 kHz presence
        if (speakerMode_) {
            speakerHpf_.process(mixL, mixR);
            speakerPresence_.process(mixL, mixR);
        }

        mixL *= preGain;
        mixR *= preGain;

        // Stereo-linked Master Bus Envelope Compressor / Limiter
        float instPeak = std::max(std::fabs(mixL), std::fabs(mixR));
        float coeff = (instPeak > compEnv_) ? COMP_ATK : COMP_REL;
        compEnv_ += coeff * (instPeak - compEnv_);

        float compGain = 1.0f;
        if (compEnv_ > COMP_THR) {
            float over = compEnv_ - COMP_THR;
            float target = COMP_THR + over / COMP_RATIO;
            compGain = target / compEnv_;
        }

        // Smooth asymptotic safety ceiling (~0.96 max, zero flat-top clipping)
        mixL = std::tanh(mixL * compGain * 1.05f) * 0.96f;
        mixR = std::tanh(mixR * compGain * 1.05f) * 0.96f;

        float absMax = std::max(std::fabs(mixL), std::fabs(mixR));
        if (absMax > peak) peak = absMax;

        outStereo[s * 2 + 0] = static_cast<int16_t>(mixL * 32767.0f);
        outStereo[s * 2 + 1] = static_cast<int16_t>(mixR * 32767.0f);
    }

    lastPeak_ = peak;
}

} // namespace BardBeats
