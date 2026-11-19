#include "synth_voices.h"
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

static inline float nextNoise(uint32_t& lfsr) {
    uint32_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 15);
    return (static_cast<float>(lfsr & 0xFFFF) / 32768.0f) - 1.0f;
}

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
    // 1. Retrigger same drum piece in place to avoid stacking identical drums
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

    // 2. Find a free voice
    for (int i = 0; i < BB_MAX_VOICES; ++i) {
        if (!voices_[i].active) return i;
    }

    // 3. Steal oldest voice on the same track first, or oldest overall
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
    v.gain = (static_cast<float>(velocity) / 127.0f) * 0.42f;
    v.baseFreq = midiToFreq(midiNote);
    v.phaseInc = v.baseFreq / BB_SAMPLE_RATE;
    v.notePitchBendSemis = stepBendSemis;
    v.lfsrState = 0xACE1u + static_cast<uint32_t>(idx * 131 + midiNote);
    v.samplesRemaining = gateSamples;

    // Set sample playback increment relative to wavetable base pitch
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
    float pitchMult = (totalBend != 0.0f) ? fastSemisToMult(totalBend) : 1.0f;

    float raw = 0.0f;
    bool isDrum = (v.instrumentId == INST_TAVERN_KIT || v.instrumentId == INST_CHIP_KIT);

    if (isDrum) {
        bool chip = (v.instrumentId == INST_CHIP_KIT);
        switch (v.midiNote) {
            case DRUM_KICK: {
                float sweepFreq = chip ? (52.0f + 180.0f * std::exp(-tSec * 42.0f))
                                       : (48.0f + 115.0f * std::exp(-tSec * 28.0f));
                v.phase += sweepFreq / BB_SAMPLE_RATE;
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                float osc = chip ? (1.0f - 4.0f * std::fabs(v.phase - 0.5f))
                                 : std::sin(v.phase * TWO_PI);
                raw = osc * std::exp(-tSec * (chip ? 16.0f : 11.0f)) * 1.35f;
                if (tSec > 0.28f) v.active = false;
                break;
            }
            case DRUM_TOM: {
                float sweepFreq = 92.0f + 95.0f * std::exp(-tSec * 22.0f);
                v.phase += sweepFreq / BB_SAMPLE_RATE;
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                raw = std::sin(v.phase * TWO_PI) * std::exp(-tSec * 12.0f);
                if (tSec > 0.25f) v.active = false;
                break;
            }
            case DRUM_SNARE: {
                float bodyFreq = chip ? 210.0f : 175.0f;
                v.phase += bodyFreq / BB_SAMPLE_RATE;
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                float body = std::sin(v.phase * TWO_PI) * std::exp(-tSec * 24.0f);
                float n = nextNoise(v.lfsrState) * std::exp(-tSec * (chip ? 18.0f : 15.0f));
                raw = 0.45f * body + 0.65f * n;
                if (tSec > 0.22f) v.active = false;
                break;
            }
            case DRUM_CLAP: {
                float burst = (tSec < 0.032f) ? (std::fmod(tSec, 0.011f) < 0.005f ? 1.0f : 0.25f) : 1.0f;
                float n = nextNoise(v.lfsrState);
                v.lpfState += 0.35f * (n - v.lpfState);
                raw = (n - 0.5f * v.lpfState) * burst * std::exp(-tSec * 14.0f);
                if (tSec > 0.24f) v.active = false;
                break;
            }
            case DRUM_CLOSED_HAT:
            case DRUM_OPEN_HAT: {
                float n = nextNoise(v.lfsrState);
                float hp = n - v.lpfState;
                v.lpfState += 0.65f * (n - v.lpfState);
                float decayRate = (v.midiNote == DRUM_OPEN_HAT) ? 8.5f : 38.0f;
                raw = hp * std::exp(-tSec * decayRate) * 0.85f;
                if (tSec > ((v.midiNote == DRUM_OPEN_HAT) ? 0.38f : 0.09f)) v.active = false;
                break;
            }
            case DRUM_TAMB: {
                float n = nextNoise(v.lfsrState);
                v.phase += 4300.0f / BB_SAMPLE_RATE;
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                float ring = std::sin(v.phase * TWO_PI);
                raw = (0.55f * n + 0.45f * ring) * std::exp(-tSec * 16.0f) * 0.7f;
                if (tSec > 0.22f) v.active = false;
                break;
            }
            default: { // DRUM_SHAKER
                float n = nextNoise(v.lfsrState);
                float hp = n - v.lpfState;
                v.lpfState += 0.5f * (n - v.lpfState);
                raw = hp * std::exp(-tSec * 22.0f) * 0.65f;
                if (tSec > 0.14f) v.active = false;
                break;
            }
        }
        v.ageSamples++;
        return raw * v.gain;
    }

    // Melodic & Harmonic Instruments
    // Gate countdown for sequenced notes
    if (!v.isLiveNote && v.samplesRemaining > 0) {
        v.samplesRemaining--;
        if (v.samplesRemaining == 0 && v.envStage != ENV_RELEASE) {
            v.envStage = ENV_RELEASE;
        }
    }

    // Envelope generator
    switch (v.envStage) {
        case ENV_ATTACK: {
            float atkStep = (v.instrumentId == INST_OCARINA) ? 0.0028f : 0.012f;
            v.envLevel += atkStep;
            if (v.envLevel >= 1.0f) {
                v.envLevel = 1.0f;
                v.envStage = ENV_DECAY;
            }
            break;
        }
        case ENV_DECAY: {
            float sus = (v.instrumentId == INST_OCARINA || v.instrumentId == INST_PULSE_LEAD || v.instrumentId == INST_SUB_BASS) ? 0.75f : 0.45f;
            v.envLevel -= 0.00025f;
            if (v.envLevel <= sus) {
                v.envLevel = sus;
                v.envStage = ENV_SUSTAIN;
            }
            break;
        }
        case ENV_SUSTAIN:
            break;
        case ENV_RELEASE: {
            float relStep = (v.instrumentId == INST_LUTE || v.instrumentId == INST_HARP) ? 0.00045f : 0.0012f;
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

    // Sample-table instruments (Lute, Harp, Upright Bass)
    const int16_t* tbl = nullptr;
    uint32_t tblLen = 0;
    if (v.instrumentId == INST_LUTE) { tbl = luteTbl_; tblLen = luteLen_; }
    else if (v.instrumentId == INST_HARP) { tbl = harpTbl_; tblLen = harpLen_; }
    else if (v.instrumentId == INST_UPRIGHT_BASS) { tbl = bassTbl_; tblLen = bassLen_; }

    if (tbl && tblLen > 2) {
        uint32_t idx0 = static_cast<uint32_t>(v.samplePos);
        if (idx0 >= tblLen - 1) {
            v.active = false;
            return 0.0f;
        }
        float frac = v.samplePos - static_cast<float>(idx0);
        float s0 = static_cast<float>(tbl[idx0]) / 32768.0f;
        float s1 = static_cast<float>(tbl[idx0 + 1]) / 32768.0f;
        raw = s0 + frac * (s1 - s0);
        v.samplePos += v.sampleInc * pitchMult;
    } else {
        // Procedural synthesis (Ocarina, Pulse Lead, Sub Bass, or table fallback)
        v.phase += v.phaseInc * pitchMult;
        if (v.phase >= 1.0f) v.phase -= std::floor(v.phase);

        if (v.instrumentId == INST_OCARINA) {
            // Pure vessel flute + gentle 2nd harmonic + breath attack puff
            float s1 = std::sin(v.phase * TWO_PI);
            float s2 = 0.14f * std::sin(v.phase * TWO_PI * 2.0f);
            float breath = (tSec < 0.045f) ? (nextNoise(v.lfsrState) * 0.08f * (1.0f - tSec / 0.045f)) : 0.0f;
            raw = (s1 + s2 + breath) * 0.9f;
        } else if (v.instrumentId == INST_PULSE_LEAD) {
            raw = (v.phase < 0.25f) ? 0.65f : -0.65f;
            v.lpfState += 0.45f * (raw - v.lpfState);
            raw = v.lpfState;
        } else if (v.instrumentId == INST_SUB_BASS) {
            float tri = 1.0f - 4.0f * std::fabs(v.phase - 0.5f);
            float saw = 2.0f * v.phase - 1.0f;
            float mix = 0.65f * tri + 0.35f * saw;
            v.lpfState += 0.14f * (mix - v.lpfState);
            raw = v.lpfState * 1.25f;
        } else {
            // Karplus-less fallback triangle
            raw = 1.0f - 4.0f * std::fabs(v.phase - 0.5f);
        }
    }

    v.ageSamples++;
    return raw * v.envLevel * v.gain;
}

void SynthVoices::render(int16_t* outStereo, int numSamples, float liveBendSemis, float liveVibrato,
                         const uint8_t trackVolumes[BB_NUM_TRACKS], const uint8_t trackMuted[BB_NUM_TRACKS]) {
    float peak = 0.0f;
    const float vibFreqInc = 5.5f / BB_SAMPLE_RATE;

    for (int s = 0; s < numSamples; ++s) {
        // Smooth Circle Pad pitch bend (5ms one-pole filter)
        smoothedLiveBend_ += 0.015f * (liveBendSemis - smoothedLiveBend_);

        vibPhase_ += vibFreqInc;
        if (vibPhase_ >= 1.0f) vibPhase_ -= 1.0f;
        float vibOffset = liveVibrato * 0.35f * std::sin(vibPhase_ * TWO_PI);

        float mix = 0.0f;
        for (int i = 0; i < BB_MAX_VOICES; ++i) {
            Voice& v = voices_[i];
            if (!v.active) continue;
            if (trackMuted[v.trackIdx]) {
                // Still advance age/envelope silently or skip
                renderVoiceSample(v, smoothedLiveBend_, vibOffset);
                continue;
            }
            float vol = static_cast<float>(trackVolumes[v.trackIdx]) / 100.0f;
            mix += renderVoiceSample(v, smoothedLiveBend_, vibOffset) * vol;
        }

        // Soft limiter / tanh saturation to prevent harsh digital clipping
        if (mix > 0.95f || mix < -0.95f) {
            mix = std::tanh(mix);
        }
        float absMix = std::fabs(mix);
        if (absMix > peak) peak = absMix;

        int16_t pcm = static_cast<int16_t>(mix * 32767.0f);
        outStereo[s * 2 + 0] = pcm;
        outStereo[s * 2 + 1] = pcm;
    }

    lastPeak_ = peak;
}

} // namespace BardBeats
