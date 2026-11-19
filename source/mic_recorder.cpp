#include "mic_recorder.h"
#include <3ds.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

namespace BardBeats {

static constexpr uint32_t MIC_SHARED_SIZE = 0x30000; // 192 KB, 4KB-aligned
static constexpr uint32_t LATENCY_COMP_SAMPLES = 360; // ~22 ms at 16,364 Hz
static constexpr uint32_t POP_BLANK_SAMPLES = 250;    // ~15 ms initial ADC pop blanking
static constexpr uint32_t MAX_RECORD_SAMPLES = BB_MIC_SAMPLE_RATE * 120; // 2 min max

static struct {
    bool initialized = false;
    bool recording = false;
    uint8_t* sharedBuf = nullptr;
    uint32_t sharedDataSize = 0;
    uint32_t readBytePos = 0;

    uint32_t targetSamples = 0;
    std::vector<int16_t> scratch;
    float liveLevel = 0.0f;
} s_mic;

bool micRecorderInit() {
    if (s_mic.initialized) return true;
    s_mic.sharedBuf = static_cast<uint8_t*>(memalign(0x1000, MIC_SHARED_SIZE));
    if (!s_mic.sharedBuf) return false;
    std::memset(s_mic.sharedBuf, 0, MIC_SHARED_SIZE);

    if (R_FAILED(micInit(s_mic.sharedBuf, MIC_SHARED_SIZE))) {
        std::free(s_mic.sharedBuf);
        s_mic.sharedBuf = nullptr;
        return false;
    }

    s_mic.sharedDataSize = micGetSampleDataSize();
    MICU_SetClamp(true);
    s_mic.initialized = true;
    return true;
}

void micRecorderExit() {
    if (s_mic.recording) {
        MICU_StopSampling();
        s_mic.recording = false;
    }
    if (s_mic.initialized) {
        micExit();
        std::free(s_mic.sharedBuf);
        s_mic.sharedBuf = nullptr;
        s_mic.initialized = false;
    }
    s_mic.scratch.clear();
}

bool micRecorderIsAvailable() {
    return s_mic.initialized;
}

bool micRecorderStart(uint32_t targetSamples) {
    if (!s_mic.initialized) return false;
    if (s_mic.recording) {
        MICU_StopSampling();
        s_mic.recording = false;
    }

    if (targetSamples == 0 || targetSamples > MAX_RECORD_SAMPLES) {
        targetSamples = MAX_RECORD_SAMPLES;
    }
    s_mic.targetSamples = targetSamples;
    // Capture extra LATENCY_COMP_SAMPLES so after shifting earlier we still have full length
    s_mic.scratch.clear();
    s_mic.scratch.reserve(targetSamples + LATENCY_COMP_SAMPLES + 512);
    s_mic.readBytePos = 0;
    s_mic.liveLevel = 0.0f;

    Result rc = MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_16360,
                                   0, s_mic.sharedDataSize, true);
    if (R_FAILED(rc)) return false;

    s_mic.recording = true;
    return true;
}

bool micRecorderPoll() {
    if (!s_mic.initialized || !s_mic.recording) return false;

    uint32_t writeBytePos = micGetLastSampleOffset();
    // Ensure even 2-byte alignment for int16 samples
    writeBytePos &= ~1u;
    uint32_t dataSize = s_mic.sharedDataSize & ~1u;
    if (dataSize == 0) return false;

    float framePeak = 0.0f;
    uint32_t maxCapture = s_mic.targetSamples + LATENCY_COMP_SAMPLES;

    while (s_mic.readBytePos != writeBytePos && s_mic.scratch.size() < maxCapture) {
        int16_t sample = 0;
        std::memcpy(&sample, &s_mic.sharedBuf[s_mic.readBytePos], sizeof(int16_t));
        s_mic.scratch.push_back(sample);

        float a = std::fabs(static_cast<float>(sample)) / 32768.0f;
        if (a > framePeak) framePeak = a;

        s_mic.readBytePos = (s_mic.readBytePos + 2) % dataSize;
    }

    s_mic.liveLevel = framePeak;
    return (s_mic.scratch.size() >= maxCapture);
}

void micRecorderStop(bool commit, std::vector<int16_t>& outSamples) {
    if (s_mic.initialized && s_mic.recording) {
        MICU_StopSampling();
    }
    s_mic.recording = false;
    s_mic.liveLevel = 0.0f;

    if (!commit || s_mic.scratch.size() <= POP_BLANK_SAMPLES + 16) {
        s_mic.scratch.clear();
        return;
    }

    // Apply 22ms latency compensation (drop leading ~LATENCY_COMP_SAMPLES)
    uint32_t startOffset = std::min<uint32_t>(LATENCY_COMP_SAMPLES,
                                              static_cast<uint32_t>(s_mic.scratch.size() / 4));
    uint32_t avail = static_cast<uint32_t>(s_mic.scratch.size()) - startOffset;
    uint32_t finalLen = std::min<uint32_t>(avail, s_mic.targetSamples);

    outSamples.resize(finalLen);
    std::memcpy(outSamples.data(), &s_mic.scratch[startOffset], finalLen * sizeof(int16_t));

    // Apply gentle 10ms (160 samples) fade-in and fade-out to eliminate loop boundary clicks
    uint32_t fadeLen = std::min<uint32_t>(160, finalLen / 4);
    for (uint32_t i = 0; i < fadeLen; ++i) {
        float g = static_cast<float>(i) / static_cast<float>(fadeLen);
        outSamples[i] = static_cast<int16_t>(outSamples[i] * g);
        outSamples[finalLen - 1 - i] = static_cast<int16_t>(outSamples[finalLen - 1 - i] * g);
    }

    s_mic.scratch.clear();
}

bool micRecorderIsRecording() {
    return s_mic.recording;
}

float micRecorderGetProgress() {
    if (!s_mic.recording || s_mic.targetSamples == 0) return 0.0f;
    float p = static_cast<float>(s_mic.scratch.size()) / static_cast<float>(s_mic.targetSamples);
    return (p > 1.0f) ? 1.0f : p;
}

float micRecorderGetLiveLevel() {
    return s_mic.liveLevel;
}

void micComputeWaveformBars(const std::vector<int16_t>& samples, float* outBars, int numBars) {
    if (!outBars || numBars <= 0) return;
    if (samples.empty()) {
        for (int i = 0; i < numBars; ++i) outBars[i] = 0.0f;
        return;
    }
    size_t total = samples.size();
    for (int b = 0; b < numBars; ++b) {
        size_t s0 = (total * static_cast<size_t>(b)) / static_cast<size_t>(numBars);
        size_t s1 = (total * static_cast<size_t>(b + 1)) / static_cast<size_t>(numBars);
        if (s1 <= s0) s1 = std::min(total, s0 + 1);
        float peak = 0.0f;
        size_t stride = std::max<size_t>(1, (s1 - s0) / 32);
        for (size_t i = s0; i < s1; i += stride) {
            float v = std::fabs(static_cast<float>(samples[i])) / 32768.0f;
            if (v > peak) peak = v;
        }
        outBars[b] = peak;
    }
}

} // namespace BardBeats
