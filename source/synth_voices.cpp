#include "synth_voices.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace BardBeats {

static constexpr float TWO_PI = 6.28318530718f;

static inline float midiToFreq(uint8_t midiNote) {
    return 440.0f * std::pow(2.0f, (static_cast<int>(midiNote) - 69) / 12.0f);
}

static inline float fastSemisToMult(float semis) {
    return std::pow(2.0f, semis / 12.0f);
}

// Crisp, uncorrelated white noise using xorshift32
static inline float nextWhiteNoise(uint32_t& state) {
    uint32_t x = state ? state : 0x6D2B79F5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return (static_cast<float>(static_cast<int32_t>(x)) * (1.0f / 2147483648.0f));
}

// PolyBLEP anti-aliasing residual for clean, alias-free square/saw oscillators
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
    constexpr float fb = 0.68f;
    constexpr float damp = 0.35f;

    float dL1 = cL1[iL1];
    lpL1 = dL1 * (1.0f - damp) + lpL1 * damp;
    cL1[iL1] = inMono + lpL1 * fb;
    if (++iL1 >= COMB_L1) iL1 = 0;

    float dL2 = cL2[iL2];
    lpL2 = dL2 * (1.0f - damp) + lpL2 * damp;
    cL2[iL2] = inMono + lpL2 * (fb * 0.96f);
    if (++iL2 >= COMB_L2) iL2 = 0;

    float dR1 = cR1[iR1];
    lpR1 = dR1 * (1.0f - damp) + lpR1 * damp;
    cR1[iR1] = inMono + lpR1 * fb;
    if (++iR1 >= COMB_R1) iR1 = 0;

    float dR2 = cR2[iR2];
    lpR2 = dR2 * (1.0f - damp) + lpR2 * damp;
    cR2[iR2] = inMono + lpR2 * (fb * 0.96f);
    if (++iR2 >= COMB_R2) iR2 = 0;

    float sumL = (dL1 + dL2) * 0.5f;
    float sumR = (dR1 + dR2) * 0.5f;

    // 1-stage all-pass diffusion per channel
    float bufL = apL[iAp];
    outL = -sumL * 0.5f + bufL;
    apL[iAp] = sumL + bufL * 0.5f;

    float bufR = apR[iAp];
    outR = -sumR * 0.5f + bufR;
    apR[iAp] = sumR + bufR * 0.5f;

    if (++iAp >= AP_LEN) iAp = 0;
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
    std::memset(voices_, 0, sizeof(voices_));
    reverb_.reset();
    loadWaveTable("romfs:/audio/lute_tbl.bin", &luteTbl_, &luteLen_);
    loadWaveTable("romfs:/audio/harp_tbl.bin", &harpTbl_, &harpLen_);
    loadWaveTable("romfs:/audio/bass_tbl.bin", &bassTbl_, &bassLen_);
    vibPhase_ = 0.0f;
    smoothedLiveBend_ = 0.0f;
    lastPeak_ = 0.0f;
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

void SynthVoices::triggerNote(int trackIdx, uint8_t instrumentId, uint8_t midiNote,
                              uint8_t velocity, float stepBendSemis, int gateSamples, bool isLive) {
    if (midiNote == 0 || velocity == 0) return;

    int idx = allocateVoice(trackIdx, midiNote, instrumentId);
    Voice& v = voices_[idx];
    std::memset(&v, 0, sizeof(Voice));
    v.active = true;
    v.trackIdx = static_cast<uint8_t>(trackIdx);
    v.instrumentId = instrumentId;
    v.midiNote = midiNote;
    v.velocity = velocity;
    v.isLiveNote = isLive;
    v.envStage = ENV_ATTACK;
    v.envLevel = 0.0f;
    v.gain = (static_cast<float>(velocity) / 127.0f) * 0.44f;
    v.baseFreq = midiToFreq(midiNote);
    v.phaseInc = v.baseFreq / BB_SAMPLE_RATE;
    v.notePitchBendSemis = stepBendSemis;
    v.rngState = 0x9E3779B9u ^ (static_cast<uint32_t>(idx + 1) * 2654435761u) ^ (midiNote * 7919u);
    v.samplesRemaining = gateSamples;

    if (instrumentId == INST_LUTE && luteTbl_) {
        v.sampleInc = v.baseFreq / 146.832f; // D3
    } else if (instrumentId == INST_HARP && harpTbl_) {
        v.sampleInc = v.baseFreq / 293.665f; // D4
    } else if (instrumentId == INST_UPRIGHT_BASS && bassTbl_) {
        v.sampleInc = v.baseFreq / 73.416f;  // D2
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
}

float SynthVoices::renderVoiceSample(Voice& v, float liveBendSemis, float liveVibOffset) {
    float tSec = static_cast<float>(v.ageSamples) / BB_SAMPLE_RATE;
    float totalBend = v.notePitchBendSemis + (v.isLiveNote ? (liveBendSemis + liveVibOffset) : 0.0f);

    bool isDrum = (v.instrumentId == INST_TAVERN_KIT || v.instrumentId == INST_CHIP_KIT);
    if (isDrum) {
        bool chip = (v.instrumentId == INST_CHIP_KIT);
        float raw = 0.0f;
        switch (v.midiNote) {
            case DRUM_KICK: {
                // Handheld-tuned warm punchy kick (beater click + 175Hz -> 62Hz body + soft warmth)
                float fStart = chip ? 240.0f : 175.0f;
                float fEnd   = chip ? 56.0f  : 62.0f;
                float sweepFreq = fEnd + (fStart - fEnd) * std::exp(-tSec * (chip ? 38.0f : 30.0f));
                v.phase += sweepFreq / BB_SAMPLE_RATE;
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                float body = chip ? (1.0f - 4.0f * std::fabs(v.phase - 0.5f))
                                  : std::sin(v.phase * TWO_PI);
                // Add subtle 2nd harmonic warmth so it punches on small 3DS speakers
                body = std::tanh(body * 1.45f) * std::exp(-tSec * (chip ? 14.0f : 10.5f));
                // Short wood-mallet click transient (5ms)
                float click = (tSec < 0.008f)
                    ? (nextWhiteNoise(v.rngState) * std::exp(-tSec * 380.0f) * 0.35f)
                    : 0.0f;
                raw = (body + click) * 1.25f;
                if (tSec > 0.30f) v.active = false;
                break;
            }
            case DRUM_TOM: {
                float sweepFreq = 105.0f + 100.0f * std::exp(-tSec * 24.0f);
                v.phase += sweepFreq / BB_SAMPLE_RATE;
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                float skin = std::sin(v.phase * TWO_PI) + 0.25f * std::sin(v.phase * TWO_PI * 1.59f);
                raw = skin * std::exp(-tSec * 11.5f) * 0.9f;
                if (tSec > 0.26f) v.active = false;
                break;
            }
            case DRUM_SNARE: {
                float bodyFreq = (chip ? 220.0f : 185.0f) + 75.0f * std::exp(-tSec * 45.0f);
                v.phase += bodyFreq / BB_SAMPLE_RATE;
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                float body = std::sin(v.phase * TWO_PI) * std::exp(-tSec * 22.0f);
                // Crisp band-shaped snare wire noise
                float n = nextWhiteNoise(v.rngState);
                v.lpfState  += 0.55f * (n - v.lpfState);
                v.lpfState2 += 0.14f * (v.lpfState - v.lpfState2);
                float wires = (v.lpfState - v.lpfState2) * std::exp(-tSec * (chip ? 19.0f : 14.5f));
                raw = 0.50f * body + 0.85f * wires;
                if (tSec > 0.24f) v.active = false;
                break;
            }
            case DRUM_CLAP: {
                // Multi-tap tavern handclap (3 quick staggered bursts + warm room tail)
                float burst = 1.0f;
                if (tSec < 0.030f) {
                    float m = std::fmod(tSec, 0.010f);
                    burst = std::exp(-m * 240.0f);
                }
                float n = nextWhiteNoise(v.rngState);
                v.lpfState  += 0.42f * (n - v.lpfState);
                v.lpfState2 += 0.16f * (v.lpfState - v.lpfState2);
                float bp = v.lpfState - v.lpfState2;
                raw = bp * (0.45f + 0.95f * burst) * std::exp(-tSec * 13.0f) * 1.3f;
                if (tSec > 0.26f) v.active = false;
                break;
            }
            case DRUM_CLOSED_HAT:
            case DRUM_OPEN_HAT: {
                float n = nextWhiteNoise(v.rngState);
                v.lpfState += 0.55f * (n - v.lpfState);
                float hp = n - v.lpfState;
                // Subtle metallic shimmer ring
                v.phase  += 6420.0f / BB_SAMPLE_RATE; if (v.phase >= 1.0f) v.phase -= 1.0f;
                v.phase2 += 8130.0f / BB_SAMPLE_RATE; if (v.phase2 >= 1.0f) v.phase2 -= 1.0f;
                float metal = 0.22f * ((v.phase < 0.5f ? 1.0f : -1.0f) * (v.phase2 < 0.5f ? 1.0f : -1.0f));
                float decayRate = (v.midiNote == DRUM_OPEN_HAT) ? 7.5f : 38.0f;
                raw = (0.78f * hp + metal) * std::exp(-tSec * decayRate) * 0.72f;
                if (tSec > ((v.midiNote == DRUM_OPEN_HAT) ? 0.42f : 0.09f)) v.active = false;
                break;
            }
            case DRUM_TAMB: {
                float n = nextWhiteNoise(v.rngState);
                v.lpfState += 0.45f * (n - v.lpfState);
                float hp = n - v.lpfState;
                v.phase  += 5280.0f / BB_SAMPLE_RATE; if (v.phase >= 1.0f) v.phase -= 1.0f;
                v.phase2 += 7410.0f / BB_SAMPLE_RATE; if (v.phase2 >= 1.0f) v.phase2 -= 1.0f;
                float jingle = 0.35f * (std::sin(v.phase * TWO_PI) + std::sin(v.phase2 * TWO_PI));
                raw = (0.65f * hp + jingle) * std::exp(-tSec * 15.0f) * 0.70f;
                if (tSec > 0.25f) v.active = false;
                break;
            }
            default: { // DRUM_SHAKER
                float n = nextWhiteNoise(v.rngState);
                v.lpfState += 0.50f * (n - v.lpfState);
                float hp = n - v.lpfState;
                float swell = (tSec < 0.018f) ? (tSec / 0.018f) : 1.0f;
                raw = hp * swell * std::exp(-tSec * 24.0f) * 0.60f;
                if (tSec > 0.16f) v.active = false;
                break;
            }
        }
        v.ageSamples++;
        return raw * v.gain;
    }

    // ---- Melodic & Harmonic Instruments ----

    // Gate countdown for sequenced notes
    if (!v.isLiveNote && v.samplesRemaining > 0) {
        v.samplesRemaining--;
        if (v.samplesRemaining == 0 && v.envStage != ENV_RELEASE) {
            v.envStage = ENV_RELEASE;
        }
    }

    // Smooth ADSR envelope tuned per instrument
    switch (v.envStage) {
        case ENV_ATTACK: {
            float atkStep = 0.018f;
            if (v.instrumentId == INST_OCARINA) atkStep = 0.0014f; // ~22ms gentle breath swell
            else if (v.instrumentId == INST_HARP) atkStep = 0.025f;
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
                // Sample tables already carry their natural acoustic decay
                sus = 0.95f;
                decStep = 0.00008f;
            } else if (v.instrumentId == INST_PULSE_LEAD) {
                sus = 0.62f;
                decStep = 0.00035f;
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
            if (v.instrumentId == INST_LUTE || v.instrumentId == INST_HARP) relStep = 0.00028f; // Let strings ring!
            else if (v.instrumentId == INST_OCARINA) relStep = 0.00065f; // Gentle breath taper
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
    float pitchMult = (totalBend != 0.0f) ? fastSemisToMult(totalBend) : 1.0f;

    // 1. Acoustic Sample-Table Instruments (Lute, Harp, Upright Bass)
    const int16_t* tbl = nullptr;
    uint32_t tblLen = 0;
    if (v.instrumentId == INST_LUTE)         { tbl = luteTbl_; tblLen = luteLen_; }
    else if (v.instrumentId == INST_HARP)    { tbl = harpTbl_; tblLen = harpLen_; }
    else if (v.instrumentId == INST_UPRIGHT_BASS) { tbl = bassTbl_; tblLen = bassLen_; }

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
        // Gentle brightness polish on Lute/Bass
        if (v.instrumentId == INST_UPRIGHT_BASS) {
            raw *= 1.18f;
        }
        v.samplePos += v.sampleInc * pitchMult;
    } else if (v.instrumentId == INST_OCARINA) {
        // Expressive Clay Ocarina: subtle pitch scoop on attack + natural delayed vibrato + warm breath chiff
        float attackScoop = (tSec < 0.045f) ? (-0.22f * (1.0f - tSec / 0.045f)) : 0.0f;
        float autoVibDepth = (tSec > 0.16f) ? std::min(0.14f, (tSec - 0.16f) * 0.45f) : 0.0f;
        float autoVib = autoVibDepth * std::sin(tSec * 5.2f * TWO_PI);
        float ocMult = fastSemisToMult(totalBend + attackScoop + autoVib);

        v.phase += v.phaseInc * ocMult;
        if (v.phase >= 1.0f) v.phase -= std::floor(v.phase);

        float f1 = std::sin(v.phase * TWO_PI);
        float f2 = 0.22f * std::sin(v.phase * TWO_PI * 2.0f);
        float f3 = 0.05f * std::sin(v.phase * TWO_PI * 3.0f);

        // Soft band-limited breath chiff (stronger on attack, gentle whisper during sustain)
        float n = nextWhiteNoise(v.rngState);
        v.lpfState += 0.28f * (n - v.lpfState);
        v.lpfState2 += 0.09f * (v.lpfState - v.lpfState2);
        float breathAmt = 0.035f + ((tSec < 0.06f) ? (0.12f * (1.0f - tSec / 0.06f)) : 0.0f);
        float breath = (v.lpfState - v.lpfState2) * breathAmt;

        raw = (f1 + f2 + f3) * 0.78f + breath;
    } else if (v.instrumentId == INST_PULSE_LEAD) {
        // Clean PolyBLEP Chiptune Pulse Lead + subtle detuned 2nd oscillator + filter pluck
        float dt1 = v.phaseInc * pitchMult;
        float dt2 = dt1 * 1.0035f; // +6 cents warm chorus detune
        v.phase  += dt1; if (v.phase >= 1.0f) v.phase -= std::floor(v.phase);
        v.phase2 += dt2; if (v.phase2 >= 1.0f) v.phase2 -= std::floor(v.phase2);

        auto blepPulse = [](float p, float dt, float duty) {
            float val = (p < duty) ? 1.0f : -1.0f;
            val += polyBlep(p, dt);
            float p2 = p - duty;
            if (p2 < 0.0f) p2 += 1.0f;
            val -= polyBlep(p2, dt);
            return val;
        };

        float osc = 0.55f * blepPulse(v.phase, dt1, 0.25f) + 0.35f * blepPulse(v.phase2, dt2, 0.50f);
        // Warm 2-pole low-pass filter sweep on attack
        float cutoff = 0.22f + 0.45f * std::exp(-tSec * 9.0f);
        v.lpfState  += cutoff * (osc - v.lpfState);
        v.lpfState2 += cutoff * (v.lpfState - v.lpfState2);
        raw = v.lpfState2 * 0.85f;
    } else {
        // INST_SUB_BASS: Fat PolyBLEP Saw + Sub Sine through warm resonant filter envelope
        float dt = v.phaseInc * pitchMult;
        v.phase += dt;
        if (v.phase >= 1.0f) v.phase -= std::floor(v.phase);

        float saw = (2.0f * v.phase - 1.0f) - polyBlep(v.phase, dt);
        float sub = std::sin(v.phase * TWO_PI);
        float mix = 0.55f * saw + 0.55f * sub;
        float cutoff = 0.14f + 0.32f * std::exp(-tSec * 12.0f);
        v.lpfState  += cutoff * (mix - v.lpfState);
        v.lpfState2 += cutoff * (v.lpfState - v.lpfState2);
        raw = std::tanh(v.lpfState2 * 1.4f) * 0.95f;
    }

    v.ageSamples++;
    return raw * v.envLevel * v.gain;
}

void SynthVoices::render(int16_t* outStereo, int numSamples, float liveBendSemis, float liveVibrato,
                         const uint8_t trackVolumes[BB_NUM_TRACKS], const uint8_t trackMuted[BB_NUM_TRACKS]) {
    float peak = 0.0f;
    const float vibFreqInc = 5.5f / BB_SAMPLE_RATE;

    // Subtle stereo panning per track (L, R gains) + reverb send level
    static constexpr float PAN_L[BB_NUM_TRACKS]  = { 1.00f, 1.00f, 0.86f, 1.12f };
    static constexpr float PAN_R[BB_NUM_TRACKS]  = { 1.00f, 1.00f, 1.12f, 0.86f };
    static constexpr float REV_SEND[BB_NUM_TRACKS] = { 0.14f, 0.04f, 0.28f, 0.26f };

    for (int s = 0; s < numSamples; ++s) {
        smoothedLiveBend_ += 0.015f * (liveBendSemis - smoothedLiveBend_);

        vibPhase_ += vibFreqInc;
        if (vibPhase_ >= 1.0f) vibPhase_ -= 1.0f;
        float vibOffset = liveVibrato * 0.35f * std::sin(vibPhase_ * TWO_PI);

        float dryL = 0.0f;
        float dryR = 0.0f;
        float revIn = 0.0f;

        for (int i = 0; i < BB_MAX_VOICES; ++i) {
            Voice& v = voices_[i];
            if (!v.active) continue;
            if (trackMuted[v.trackIdx]) {
                renderVoiceSample(v, smoothedLiveBend_, vibOffset);
                continue;
            }
            float vol = static_cast<float>(trackVolumes[v.trackIdx]) * 0.01f;
            float smp = renderVoiceSample(v, smoothedLiveBend_, vibOffset) * vol;
            dryL  += smp * PAN_L[v.trackIdx];
            dryR  += smp * PAN_R[v.trackIdx];
            revIn += smp * REV_SEND[v.trackIdx];
        }

        float wetL = 0.0f, wetR = 0.0f;
        reverb_.process(revIn, wetL, wetR);

        float mixL = dryL + wetL * 0.42f;
        float mixR = dryR + wetR * 0.42f;

        // Warm soft-clip limiter
        if (mixL > 0.90f || mixL < -0.90f) mixL = std::tanh(mixL);
        if (mixR > 0.90f || mixR < -0.90f) mixR = std::tanh(mixR);

        float absMax = std::max(std::fabs(mixL), std::fabs(mixR));
        if (absMax > peak) peak = absMax;

        outStereo[s * 2 + 0] = static_cast<int16_t>(mixL * 32767.0f);
        outStereo[s * 2 + 1] = static_cast<int16_t>(mixR * 32767.0f);
    }

    lastPeak_ = peak;
}

} // namespace BardBeats
