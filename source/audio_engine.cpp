#include "audio_engine.h"
#include <3ds.h>
#include <atomic>
#include <cmath>
#include <cstring>

namespace BardBeats {

static constexpr int NUM_WAVE_BUFS = 3;
static constexpr int SAMPLES_PER_BUF = 160; // 4.888 ms at 32728.5 Hz
static constexpr int BYTES_PER_BUF = SAMPLES_PER_BUF * 2 * sizeof(int16_t);

struct LiveCmd {
    uint8_t type; // 1 = noteOn, 2 = releaseTrack
    uint8_t trackIdx;
    uint8_t instrumentId;
    uint8_t midiNote;
    uint8_t velocity;
};

static constexpr int CMD_QUEUE_SIZE = 32;

static struct {
    bool dspReady = false;
    std::atomic<bool> running{false};
    std::atomic<bool> playing{false};
    std::atomic<int>  currentStep{0};
    std::atomic<float> stepFrac{0.0f};
    std::atomic<float> liveBendSemis{0.0f};
    std::atomic<float> liveVibrato{0.0f};
    std::atomic<float> peakLevel{0.0f};

    Thread audioThread = nullptr;
    LightEvent dspEvent;
    LightLock  loopLock;
    LightLock  cmdLock;

    LoopFile activeLoop;
    SynthVoices synth;

    int16_t* audioMem = nullptr;
    ndspWaveBuf waveBufs[NUM_WAVE_BUFS];

    LiveCmd cmdQueue[CMD_QUEUE_SIZE];
    int cmdHead = 0;
    int cmdTail = 0;

    double sampleAccum = 0.0;
    bool firstStepTriggered = false;
} s_eng;

static void ndspFrameCallback(void* userdata) {
    (void)userdata;
    LightEvent_Signal(&s_eng.dspEvent);
}

static void triggerStepNotes(const LoopFile& loop, int step, double samplesPerStep) {
    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        if (loop.tracks[t].muted) continue;
        const StepCell& cell = loop.tracks[t].steps[step];
        if (cell.flags & 1) continue; // step muted
        uint8_t vel = cell.velocity > 0 ? cell.velocity : 95;
        float bendSemis = static_cast<float>(cell.pitchBendSemisQ4) / 16.0f;
        int gateSamples = static_cast<int>(samplesPerStep * (cell.gateLength > 0 ? cell.gateLength : 1) * 0.85);
        for (int p = 0; p < BB_MAX_POLY_PER_STEP; ++p) {
            uint8_t note = cell.notes[p];
            if (note != 0) {
                s_eng.synth.triggerNote(t, loop.tracks[t].instrumentId, note, vel, bendSemis, gateSamples, false);
            }
        }
    }
}

static void processLiveCommands() {
    LightLock_Lock(&s_eng.cmdLock);
    while (s_eng.cmdHead != s_eng.cmdTail) {
        LiveCmd cmd = s_eng.cmdQueue[s_eng.cmdTail];
        s_eng.cmdTail = (s_eng.cmdTail + 1) % CMD_QUEUE_SIZE;
        if (cmd.type == 1) {
            s_eng.synth.triggerNote(cmd.trackIdx, cmd.instrumentId, cmd.midiNote, cmd.velocity, 0.0f, -1, true);
        } else if (cmd.type == 2) {
            s_eng.synth.releaseLiveNotes(cmd.trackIdx);
        }
    }
    LightLock_Unlock(&s_eng.cmdLock);
}

static void fillAudioBuffer(int16_t* outStereo, int numSamples) {
    processLiveCommands();

    LoopFile loopCopy;
    LightLock_Lock(&s_eng.loopLock);
    loopCopy = s_eng.activeLoop;
    LightLock_Unlock(&s_eng.loopLock);

    uint8_t trackVols[BB_NUM_TRACKS];
    uint8_t trackMuted[BB_NUM_TRACKS];
    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        trackVols[t] = loopCopy.tracks[t].volume;
        trackMuted[t] = loopCopy.tracks[t].muted;
    }

    float bend = s_eng.liveBendSemis.load(std::memory_order_relaxed);
    float vib  = s_eng.liveVibrato.load(std::memory_order_relaxed);
    bool isPlaying = s_eng.playing.load(std::memory_order_relaxed);

    if (!isPlaying) {
        s_eng.firstStepTriggered = false;
        s_eng.synth.render(outStereo, numSamples, bend, vib, trackVols, trackMuted);
        s_eng.peakLevel.store(s_eng.synth.getLastPeakLevel(), std::memory_order_relaxed);
        return;
    }

    uint16_t bpm = (loopCopy.bpm >= 60 && loopCopy.bpm <= 180) ? loopCopy.bpm : 110;
    double samplesPerStep = (static_cast<double>(BB_SAMPLE_RATE) * 60.0) / (static_cast<double>(bpm) * 4.0);

    int rendered = 0;
    while (rendered < numSamples) {
        if (!s_eng.firstStepTriggered) {
            s_eng.firstStepTriggered = true;
            s_eng.sampleAccum = 0.0;
            int step = s_eng.currentStep.load(std::memory_order_relaxed);
            triggerStepNotes(loopCopy, step, samplesPerStep);
        }

        double samplesToNext = samplesPerStep - s_eng.sampleAccum;
        int chunk = numSamples - rendered;
        bool stepBoundaryReached = false;
        if (samplesToNext <= static_cast<double>(chunk)) {
            chunk = static_cast<int>(std::ceil(samplesToNext));
            if (chunk < 0) chunk = 0;
            if (chunk > numSamples - rendered) chunk = numSamples - rendered;
            stepBoundaryReached = true;
        }

        if (chunk > 0) {
            s_eng.synth.render(&outStereo[rendered * 2], chunk, bend, vib, trackVols, trackMuted);
            rendered += chunk;
            s_eng.sampleAccum += static_cast<double>(chunk);
        }

        if (stepBoundaryReached) {
            s_eng.sampleAccum -= samplesPerStep;
            if (s_eng.sampleAccum < 0.0) s_eng.sampleAccum = 0.0;
            int nextStep = (s_eng.currentStep.load(std::memory_order_relaxed) + 1) % BB_STEPS_PER_LOOP;
            s_eng.currentStep.store(nextStep, std::memory_order_relaxed);
            triggerStepNotes(loopCopy, nextStep, samplesPerStep);
        }
    }

    s_eng.stepFrac.store(static_cast<float>(s_eng.sampleAccum / samplesPerStep), std::memory_order_relaxed);
    s_eng.peakLevel.store(s_eng.synth.getLastPeakLevel(), std::memory_order_relaxed);
}

static void audioThreadMain(void* arg) {
    (void)arg;
    while (s_eng.running.load(std::memory_order_relaxed)) {
        for (int i = 0; i < NUM_WAVE_BUFS; ++i) {
            if (s_eng.waveBufs[i].status == NDSP_WBUF_DONE ||
                s_eng.waveBufs[i].status == NDSP_WBUF_FREE) {
                fillAudioBuffer(s_eng.waveBufs[i].data_pcm16, SAMPLES_PER_BUF);
                DSP_FlushDataCache(s_eng.waveBufs[i].data_pcm16, BYTES_PER_BUF);
                ndspChnWaveBufAdd(0, &s_eng.waveBufs[i]);
            }
        }
        LightEvent_Wait(&s_eng.dspEvent);
    }
}

bool audioEngineInit() {
    LightEvent_Init(&s_eng.dspEvent, RESET_ONESHOT);
    LightLock_Init(&s_eng.loopLock);
    LightLock_Init(&s_eng.cmdLock);
    s_eng.synth.init();
    initStarterLoop(s_eng.activeLoop, 0);

    Result rc = ndspInit();
    if (R_FAILED(rc)) {
        s_eng.dspReady = false;
        return false;
    }

    s_eng.dspReady = true;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, BB_SAMPLE_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    float mix[12] = {0};
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(0, mix);

    s_eng.audioMem = static_cast<int16_t*>(linearAlloc(NUM_WAVE_BUFS * BYTES_PER_BUF));
    std::memset(s_eng.audioMem, 0, NUM_WAVE_BUFS * BYTES_PER_BUF);
    std::memset(s_eng.waveBufs, 0, sizeof(s_eng.waveBufs));

    for (int i = 0; i < NUM_WAVE_BUFS; ++i) {
        s_eng.waveBufs[i].data_vaddr = &s_eng.audioMem[i * SAMPLES_PER_BUF * 2];
        s_eng.waveBufs[i].nsamples = SAMPLES_PER_BUF;
        fillAudioBuffer(s_eng.waveBufs[i].data_pcm16, SAMPLES_PER_BUF);
        DSP_FlushDataCache(s_eng.waveBufs[i].data_pcm16, BYTES_PER_BUF);
        ndspChnWaveBufAdd(0, &s_eng.waveBufs[i]);
    }

    ndspSetCallback(ndspFrameCallback, nullptr);
    s_eng.running.store(true);

    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    s_eng.audioThread = threadCreate(audioThreadMain, nullptr, 16 * 1024, prio - 2, -2, false);
    return true;
}

void audioEngineExit() {
    if (s_eng.dspReady) {
        s_eng.running.store(false);
        LightEvent_Signal(&s_eng.dspEvent);
        if (s_eng.audioThread) {
            threadJoin(s_eng.audioThread, U64_MAX);
            threadFree(s_eng.audioThread);
            s_eng.audioThread = nullptr;
        }
        ndspSetCallback(nullptr, nullptr);
        ndspChnWaveBufClear(0);
        ndspExit();
        if (s_eng.audioMem) {
            linearFree(s_eng.audioMem);
            s_eng.audioMem = nullptr;
        }
        s_eng.dspReady = false;
    }
    s_eng.synth.shutdown();
}

bool audioEngineIsDspReady() {
    return s_eng.dspReady;
}

void audioEngineSetLoop(const LoopFile& loop) {
    LightLock_Lock(&s_eng.loopLock);
    s_eng.activeLoop = loop;
    LightLock_Unlock(&s_eng.loopLock);
}

void audioEngineSetPlaying(bool playing) {
    if (!playing) {
        s_eng.synth.stopAll();
        s_eng.firstStepTriggered = false;
    }
    s_eng.playing.store(playing, std::memory_order_relaxed);
}

bool audioEngineIsPlaying() {
    return s_eng.playing.load(std::memory_order_relaxed);
}

void audioEngineResetPlayhead() {
    s_eng.currentStep.store(0, std::memory_order_relaxed);
    s_eng.sampleAccum = 0.0;
    s_eng.firstStepTriggered = false;
}

void audioEngineTriggerLiveNote(int trackIdx, uint8_t instrumentId, uint8_t midiNote, uint8_t velocity) {
    LightLock_Lock(&s_eng.cmdLock);
    int nextHead = (s_eng.cmdHead + 1) % CMD_QUEUE_SIZE;
    if (nextHead != s_eng.cmdTail) {
        s_eng.cmdQueue[s_eng.cmdHead] = {1, static_cast<uint8_t>(trackIdx), instrumentId, midiNote, velocity};
        s_eng.cmdHead = nextHead;
    }
    LightLock_Unlock(&s_eng.cmdLock);
}

void audioEngineReleaseLiveNotes(int trackIdx) {
    LightLock_Lock(&s_eng.cmdLock);
    int nextHead = (s_eng.cmdHead + 1) % CMD_QUEUE_SIZE;
    if (nextHead != s_eng.cmdTail) {
        s_eng.cmdQueue[s_eng.cmdHead] = {2, static_cast<uint8_t>(trackIdx), 0, 0, 0};
        s_eng.cmdHead = nextHead;
    }
    LightLock_Unlock(&s_eng.cmdLock);
}

void audioEngineSetCirclePad(float bendSemis, float vibrato) {
    s_eng.liveBendSemis.store(bendSemis, std::memory_order_relaxed);
    s_eng.liveVibrato.store(vibrato, std::memory_order_relaxed);
}

int audioEngineGetCurrentStep() {
    return s_eng.currentStep.load(std::memory_order_relaxed);
}

float audioEngineGetStepFraction() {
    return s_eng.stepFrac.load(std::memory_order_relaxed);
}

float audioEngineGetPeakLevel() {
    return s_eng.peakLevel.load(std::memory_order_relaxed);
}

} // namespace BardBeats
