#include "audio_engine.h"
#include "mic_recorder.h"
#include <3ds.h>
#include <atomic>
#include <cmath>
#include <cstring>

namespace BardBeats {

static constexpr int NUM_WAVE_BUFS = 3;
static constexpr int SAMPLES_PER_BUF = 160; // 4.888 ms at 32728.5 Hz
static constexpr int BYTES_PER_BUF = SAMPLES_PER_BUF * 2 * sizeof(int16_t);
static constexpr int VOCAL_CHN = 1;

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
    std::atomic<bool> songMode{false};
    std::atomic<int>  currentStep{0};
    std::atomic<int>  songEntryIdx{0};
    std::atomic<int>  songRepeatIdx{0};
    std::atomic<float> stepFrac{0.0f};
    std::atomic<float> liveBendSemis{0.0f};
    std::atomic<float> liveVibrato{0.0f};
    std::atomic<float> peakLevel{0.0f};

    Thread audioThread = nullptr;
    LightEvent dspEvent;
    LightLock  loopLock;
    LightLock  cmdLock;
    LightLock  vocalLock;

    LoopFile activeLoop;
    SongFile activeSong;
    LoopFile songLoops[BB_MAX_SONG_CLIPS];
    int      songLoopCount = 0;

    SynthVoices synth;

    int16_t* audioMem = nullptr;
    ndspWaveBuf waveBufs[NUM_WAVE_BUFS];

    // Vocal playback on ndsp Channel 1
    int16_t* vocalLinearMem = nullptr;
    uint32_t vocalSampleCount = 0;
    bool     vocalLooping = false;
    uint8_t  vocalVolume = 95;
    bool     vocalMuted = false;
    ndspWaveBuf vocalWaveBuf;

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

static void startVocalChannelLocked() {
    if (!s_eng.dspReady) return;
    ndspChnWaveBufClear(VOCAL_CHN);
    if (!s_eng.vocalLinearMem || s_eng.vocalSampleCount == 0) return;

    float g = s_eng.vocalMuted ? 0.0f : (static_cast<float>(s_eng.vocalVolume) / 100.0f);
    float mix[12] = {0};
    mix[0] = g;
    mix[1] = g;
    ndspChnSetMix(VOCAL_CHN, mix);

    std::memset(&s_eng.vocalWaveBuf, 0, sizeof(ndspWaveBuf));
    s_eng.vocalWaveBuf.data_vaddr = s_eng.vocalLinearMem;
    s_eng.vocalWaveBuf.nsamples   = s_eng.vocalSampleCount;
    s_eng.vocalWaveBuf.looping    = s_eng.vocalLooping;
    ndspChnWaveBufAdd(VOCAL_CHN, &s_eng.vocalWaveBuf);
}

static void stopVocalChannelLocked() {
    if (!s_eng.dspReady) return;
    ndspChnWaveBufClear(VOCAL_CHN);
}

static void triggerStepNotes(const LoopFile& loop, int step, double samplesPerStep, uint8_t extraMuteMask) {
    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        if (loop.tracks[t].muted || (extraMuteMask & (1u << t))) continue;
        const StepCell& cell = loop.tracks[t].steps[step];
        if (cell.flags & 1) continue;
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

    bool isSong = s_eng.songMode.load(std::memory_order_relaxed);
    LoopFile loopCopy;
    uint16_t bpm = 110;
    uint8_t muteMask = 0;
    int numEntries = 0;
    uint8_t curEntryRepeats = 1;

    LightLock_Lock(&s_eng.loopLock);
    if (isSong && s_eng.activeSong.numEntries > 0 && s_eng.songLoopCount > 0) {
        numEntries = s_eng.activeSong.numEntries;
        int eIdx = s_eng.songEntryIdx.load(std::memory_order_relaxed);
        if (eIdx < 0 || eIdx >= numEntries) eIdx = 0;
        if (eIdx >= s_eng.songLoopCount) eIdx = s_eng.songLoopCount - 1;
        loopCopy = s_eng.songLoops[eIdx];
        bpm = s_eng.activeSong.bpm;
        muteMask = s_eng.activeSong.chain[eIdx].muteMask;
        curEntryRepeats = s_eng.activeSong.chain[eIdx].repeatCount;
        if (curEntryRepeats < 1) curEntryRepeats = 1;
    } else {
        isSong = false;
        loopCopy = s_eng.activeLoop;
        bpm = loopCopy.bpm;
    }
    LightLock_Unlock(&s_eng.loopLock);

    uint8_t trackVols[BB_NUM_TRACKS];
    uint8_t trackMuted[BB_NUM_TRACKS];
    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        trackVols[t] = loopCopy.tracks[t].volume;
        trackMuted[t] = (loopCopy.tracks[t].muted || (muteMask & (1u << t))) ? 1 : 0;
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

    if (bpm < 60 || bpm > 180) bpm = 110;
    double samplesPerStep = (static_cast<double>(BB_SAMPLE_RATE) * 60.0) / (static_cast<double>(bpm) * 4.0);

    int rendered = 0;
    while (rendered < numSamples) {
        if (!s_eng.firstStepTriggered) {
            s_eng.firstStepTriggered = true;
            s_eng.sampleAccum = 0.0;
            int step = s_eng.currentStep.load(std::memory_order_relaxed);
            triggerStepNotes(loopCopy, step, samplesPerStep, muteMask);
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
            int prevStep = s_eng.currentStep.load(std::memory_order_relaxed);
            int nextStep = (prevStep + 1) % BB_STEPS_PER_LOOP;
            s_eng.currentStep.store(nextStep, std::memory_order_relaxed);

            // Handle bar wrap in Song Mode
            if (isSong && nextStep == 0 && numEntries > 0) {
                int rep = s_eng.songRepeatIdx.load(std::memory_order_relaxed) + 1;
                if (rep >= curEntryRepeats) {
                    rep = 0;
                    int nextEntry = (s_eng.songEntryIdx.load(std::memory_order_relaxed) + 1) % numEntries;
                    s_eng.songEntryIdx.store(nextEntry, std::memory_order_relaxed);
                    // Refresh loopCopy for the newly entered clip
                    LightLock_Lock(&s_eng.loopLock);
                    if (nextEntry < s_eng.songLoopCount) {
                        loopCopy = s_eng.songLoops[nextEntry];
                        muteMask = s_eng.activeSong.chain[nextEntry].muteMask;
                        curEntryRepeats = s_eng.activeSong.chain[nextEntry].repeatCount;
                        if (curEntryRepeats < 1) curEntryRepeats = 1;
                    }
                    LightLock_Unlock(&s_eng.loopLock);
                    // Restart whole-song vocal when song loops back to slot 0
                    if (nextEntry == 0) {
                        LightLock_Lock(&s_eng.vocalLock);
                        startVocalChannelLocked();
                        LightLock_Unlock(&s_eng.vocalLock);
                    }
                }
                s_eng.songRepeatIdx.store(rep, std::memory_order_relaxed);
            }

            triggerStepNotes(loopCopy, nextStep, samplesPerStep, muteMask);
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
    LightLock_Init(&s_eng.vocalLock);
    s_eng.synth.init();
    initStarterLoop(s_eng.activeLoop, 0);
    initStarterSong(s_eng.activeSong);

    Result rc = ndspInit();
    if (R_FAILED(rc)) {
        s_eng.dspReady = false;
        return false;
    }

    s_eng.dspReady = true;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    // Channel 0: 16-voice software synth & drum mixer
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, BB_SAMPLE_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
    float mix[12] = {0};
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(0, mix);

    // Channel 1: Dedicated Vocal Layer stream (16,364 Hz Mono PCM16)
    ndspChnReset(VOCAL_CHN);
    ndspChnSetInterp(VOCAL_CHN, NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(VOCAL_CHN, static_cast<float>(BB_MIC_SAMPLE_RATE));
    ndspChnSetFormat(VOCAL_CHN, NDSP_FORMAT_MONO_PCM16);
    ndspChnSetMix(VOCAL_CHN, mix);

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
        ndspChnWaveBufClear(VOCAL_CHN);
        ndspExit();
        if (s_eng.audioMem) {
            linearFree(s_eng.audioMem);
            s_eng.audioMem = nullptr;
        }
        if (s_eng.vocalLinearMem) {
            linearFree(s_eng.vocalLinearMem);
            s_eng.vocalLinearMem = nullptr;
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

void audioEngineSetSongMode(bool songMode) {
    s_eng.songMode.store(songMode, std::memory_order_relaxed);
}

bool audioEngineIsSongMode() {
    return s_eng.songMode.load(std::memory_order_relaxed);
}

void audioEngineSetSongArrangement(const SongFile& song, const std::vector<LoopFile>& resolvedLoops) {
    LightLock_Lock(&s_eng.loopLock);
    s_eng.activeSong = song;
    int count = static_cast<int>(resolvedLoops.size());
    if (count > BB_MAX_SONG_CLIPS) count = BB_MAX_SONG_CLIPS;
    s_eng.songLoopCount = count;
    for (int i = 0; i < count; ++i) {
        s_eng.songLoops[i] = resolvedLoops[i];
    }
    if (s_eng.songEntryIdx.load(std::memory_order_relaxed) >= s_eng.activeSong.numEntries) {
        s_eng.songEntryIdx.store(0, std::memory_order_relaxed);
        s_eng.songRepeatIdx.store(0, std::memory_order_relaxed);
    }
    LightLock_Unlock(&s_eng.loopLock);
}

int audioEngineGetSongEntryIndex() {
    return s_eng.songEntryIdx.load(std::memory_order_relaxed);
}

int audioEngineGetSongRepeatIndex() {
    return s_eng.songRepeatIdx.load(std::memory_order_relaxed);
}

void audioEngineSetVocalLayer(const int16_t* samples, uint32_t sampleCount, bool looping,
                              uint8_t volume, bool muted) {
    if (!s_eng.dspReady) return;
    LightLock_Lock(&s_eng.vocalLock);
    stopVocalChannelLocked();
    if (s_eng.vocalLinearMem) {
        linearFree(s_eng.vocalLinearMem);
        s_eng.vocalLinearMem = nullptr;
        s_eng.vocalSampleCount = 0;
    }
    if (samples && sampleCount > 0) {
        size_t bytes = static_cast<size_t>(sampleCount) * sizeof(int16_t);
        s_eng.vocalLinearMem = static_cast<int16_t*>(linearAlloc(bytes));
        if (s_eng.vocalLinearMem) {
            std::memcpy(s_eng.vocalLinearMem, samples, bytes);
            DSP_FlushDataCache(s_eng.vocalLinearMem, bytes);
            s_eng.vocalSampleCount = sampleCount;
            s_eng.vocalLooping = looping;
            s_eng.vocalVolume = volume;
            s_eng.vocalMuted = muted;
        }
    }
    if (s_eng.playing.load(std::memory_order_relaxed)) {
        startVocalChannelLocked();
    }
    LightLock_Unlock(&s_eng.vocalLock);
}

void audioEngineClearVocalLayer() {
    if (!s_eng.dspReady) return;
    LightLock_Lock(&s_eng.vocalLock);
    stopVocalChannelLocked();
    if (s_eng.vocalLinearMem) {
        linearFree(s_eng.vocalLinearMem);
        s_eng.vocalLinearMem = nullptr;
        s_eng.vocalSampleCount = 0;
    }
    LightLock_Unlock(&s_eng.vocalLock);
}

void audioEngineSetVocalMix(uint8_t volume, bool muted) {
    if (!s_eng.dspReady) return;
    LightLock_Lock(&s_eng.vocalLock);
    s_eng.vocalVolume = volume;
    s_eng.vocalMuted = muted;
    float g = muted ? 0.0f : (static_cast<float>(volume) / 100.0f);
    float mix[12] = {0};
    mix[0] = g;
    mix[1] = g;
    ndspChnSetMix(VOCAL_CHN, mix);
    LightLock_Unlock(&s_eng.vocalLock);
}

void audioEngineSetPlaying(bool playing) {
    LightLock_Lock(&s_eng.vocalLock);
    if (!playing) {
        s_eng.synth.stopAll();
        s_eng.firstStepTriggered = false;
        stopVocalChannelLocked();
    } else {
        startVocalChannelLocked();
    }
    LightLock_Unlock(&s_eng.vocalLock);
    s_eng.playing.store(playing, std::memory_order_relaxed);
}

bool audioEngineIsPlaying() {
    return s_eng.playing.load(std::memory_order_relaxed);
}

void audioEngineResetPlayhead() {
    s_eng.currentStep.store(0, std::memory_order_relaxed);
    s_eng.songEntryIdx.store(0, std::memory_order_relaxed);
    s_eng.songRepeatIdx.store(0, std::memory_order_relaxed);
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
