#include "storage_sd.h"
#include "synth_voices.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace BardBeats {

static const char* ROOT_DIR    = "sdmc:/3ds/BardBeats";
static const char* CONFIG_PATH = "sdmc:/3ds/BardBeats/config.bin";
static const char* LOOPS_DIR   = "sdmc:/3ds/BardBeats/loops";
static const char* SONGS_DIR   = "sdmc:/3ds/BardBeats/songs";
static const char* VOCALS_DIR  = "sdmc:/3ds/BardBeats/vocals";
static const char* EXPORTS_DIR = "sdmc:/3ds/BardBeats/exports";

static void ensureDir(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        mkdir(path, 0777);
    }
}

static void sanitizeName(const char* in, char* out, int maxLen) {
    int j = 0;
    for (int i = 0; in[i] != '\0' && j < maxLen - 1; ++i) {
        char c = in[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') {
            out[j++] = c;
        } else if (c == ' ') {
            out[j++] = '_';
        }
    }
    if (j == 0) {
        std::snprintf(out, maxLen, "Loop");
    } else {
        out[j] = '\0';
    }
}

static bool hasLoopId(const std::vector<LoopSummary>& list, uint16_t id) {
    for (const auto& s : list) {
        if (s.loopId == id) return true;
    }
    return false;
}

static bool hasSongId(const std::vector<SongSummary>& list, uint16_t id) {
    for (const auto& s : list) {
        if (s.songId == id) return true;
    }
    return false;
}

bool storageInit() {
    ensureDir("sdmc:/3ds");
    ensureDir(ROOT_DIR);
    ensureDir(LOOPS_DIR);
    ensureDir(SONGS_DIR);
    ensureDir(VOCALS_DIR);
    ensureDir(EXPORTS_DIR);

    auto existingLoops = storageListLoops();
    if (existingLoops.empty()) {
        for (int i = 0; i < BB_NUM_STARTER_LOOPS; ++i) {
            LoopFile starter;
            initStarterLoop(starter, i);
            storageSaveLoop(starter);
        }
    } else {
        // Seed newly added starter spells (IDs 5..8) if upgrading from v0.1.x
        for (int i = 4; i < BB_NUM_STARTER_LOOPS; ++i) {
            LoopFile starter;
            initStarterLoop(starter, i);
            if (!hasLoopId(existingLoops, starter.loopId)) {
                storageSaveLoop(starter);
            }
        }
    }

    auto existingSongs = storageListSongs();
    if (existingSongs.empty()) {
        for (int i = 0; i < BB_NUM_STARTER_SONGS; ++i) {
            SongFile starterSong;
            initStarterSong(starterSong, i);
            storageSaveSong(starterSong);
        }
    } else {
        for (int i = 1; i < BB_NUM_STARTER_SONGS; ++i) {
            SongFile starterSong;
            initStarterSong(starterSong, i);
            if (!hasSongId(existingSongs, starterSong.songId)) {
                storageSaveSong(starterSong);
            }
        }
    }
    return true;
}

bool storageLoadConfig(AppConfig& outCfg) {
    initDefaultConfig(outCfg);
    FILE* f = std::fopen(CONFIG_PATH, "rb");
    if (!f) return false;
    AppConfig loaded{};
    size_t rd = std::fread(&loaded, 1, sizeof(AppConfig), f);
    std::fclose(f);
    if (rd != sizeof(AppConfig) || std::memcmp(loaded.magic, "BBCF", 4) != 0) {
        return false;
    }
    if (loaded.masterVolume > 150) loaded.masterVolume = 100;
    if (loaded.audioOutMode >= AUDIO_OUT_COUNT) loaded.audioOutMode = AUDIO_OUT_AUTO;
    if (loaded.shoulderMode >= SHOULDER_MODE_COUNT) loaded.shoulderMode = SHOULDER_OCTAVE_HOLD;
    for (int i = 0; i < INST_COUNT; ++i) {
        if (loaded.instTrimPct[i] < -50) loaded.instTrimPct[i] = -50;
        if (loaded.instTrimPct[i] >  50) loaded.instTrimPct[i] =  50;
    }
    outCfg = loaded;
    return true;
}

bool storageSaveConfig(const AppConfig& cfg) {
    char tmpPath[256];
    std::snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", CONFIG_PATH);
    FILE* f = std::fopen(tmpPath, "wb");
    if (!f) return false;
    size_t wr = std::fwrite(&cfg, 1, sizeof(AppConfig), f);
    std::fflush(f);
    std::fclose(f);
    if (wr != sizeof(AppConfig)) {
        std::remove(tmpPath);
        return false;
    }
    std::remove(CONFIG_PATH);
    return std::rename(tmpPath, CONFIG_PATH) == 0;
}

static bool readLoopFileAutoMigrate(const char* path, LoopFile& outLoop) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (sz == static_cast<long>(sizeof(LoopFile))) {
        size_t rd = std::fread(&outLoop, 1, sizeof(LoopFile), f);
        std::fclose(f);
        if (rd == sizeof(LoopFile) && std::memcmp(outLoop.magic, "BBLP", 4) == 0) {
            if (outLoop.numBars < 1 || outLoop.numBars > 2) outLoop.numBars = 1;
            if (outLoop.swingPercent < 50 || outLoop.swingPercent > 66) outLoop.swingPercent = 50;
            if (outLoop.reverbMix > 100) outLoop.reverbMix = 38;
            return true;
        }
        return false;
    } else if (sz == static_cast<long>(sizeof(LoopFileV1))) {
        LoopFileV1 v1{};
        size_t rd = std::fread(&v1, 1, sizeof(LoopFileV1), f);
        std::fclose(f);
        if (rd == sizeof(LoopFileV1) && std::memcmp(v1.magic, "BBLP", 4) == 0) {
            migrateLoopV1ToV2(v1, outLoop);
            return true;
        }
        return false;
    }

    std::fclose(f);
    return false;
}

std::vector<LoopSummary> storageListLoops() {
    std::vector<LoopSummary> list;
    DIR* dir = opendir(LOOPS_DIR);
    if (!dir) return list;

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        size_t len = std::strlen(ent->d_name);
        if (len < 8 || std::strcmp(ent->d_name + len - 7, ".bbloop") != 0) continue;

        char fullPath[512];
        std::snprintf(fullPath, sizeof(fullPath), "%s/%s", LOOPS_DIR, ent->d_name);
        LoopFile hdr;
        if (readLoopFileAutoMigrate(fullPath, hdr)) {
            LoopSummary s{};
            s.loopId = hdr.loopId;
            s.bpm = hdr.bpm;
            s.scaleId = hdr.scaleId;
            s.hasVocal = hdr.hasVocalClip;
            s.numBars = hdr.numBars;
            std::snprintf(s.name, sizeof(s.name), "%s", hdr.name);
            std::snprintf(s.filename, sizeof(s.filename), "%s", ent->d_name);
            list.push_back(s);
        }
    }
    closedir(dir);

    std::sort(list.begin(), list.end(), [](const LoopSummary& a, const LoopSummary& b) {
        return a.loopId < b.loopId;
    });
    return list;
}

uint16_t storageGetNextLoopId() {
    auto list = storageListLoops();
    uint16_t maxId = 0;
    for (const auto& s : list) {
        if (s.loopId > maxId) maxId = s.loopId;
    }
    return maxId + 1;
}

static void removeExistingLoopFilesForId(uint16_t loopId) {
    auto list = storageListLoops();
    for (const auto& s : list) {
        if (s.loopId == loopId) {
            char path[512];
            std::snprintf(path, sizeof(path), "%s/%s", LOOPS_DIR, s.filename);
            std::remove(path);
        }
    }
}

bool storageSaveLoop(const LoopFile& loop) {
    removeExistingLoopFilesForId(loop.loopId);

    char cleanName[32];
    sanitizeName(loop.name, cleanName, sizeof(cleanName));

    char tmpPath[256];
    char finalPath[256];
    std::snprintf(tmpPath, sizeof(tmpPath), "%s/%03u_%s.tmp", LOOPS_DIR, loop.loopId, cleanName);
    std::snprintf(finalPath, sizeof(finalPath), "%s/%03u_%s.bbloop", LOOPS_DIR, loop.loopId, cleanName);

    FILE* f = std::fopen(tmpPath, "wb");
    if (!f) return false;
    size_t wr = std::fwrite(&loop, 1, sizeof(LoopFile), f);
    std::fflush(f);
    std::fclose(f);

    if (wr != sizeof(LoopFile)) {
        std::remove(tmpPath);
        return false;
    }
    std::remove(finalPath);
    return std::rename(tmpPath, finalPath) == 0;
}

bool storageLoadLoop(uint16_t loopId, LoopFile& outLoop) {
    auto list = storageListLoops();
    for (const auto& s : list) {
        if (s.loopId == loopId) {
            char path[512];
            std::snprintf(path, sizeof(path), "%s/%s", LOOPS_DIR, s.filename);
            return readLoopFileAutoMigrate(path, outLoop);
        }
    }
    return false;
}

bool storageDeleteLoop(uint16_t loopId) {
    removeExistingLoopFilesForId(loopId);
    return true;
}

static void writeBE32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back((v >> 24) & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >> 8) & 0xFF);
    buf.push_back(v & 0xFF);
}

static void writeBE16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back((v >> 8) & 0xFF);
    buf.push_back(v & 0xFF);
}

static void writeVarLen(std::vector<uint8_t>& buf, uint32_t val) {
    uint8_t bytes[4];
    int n = 0;
    bytes[n++] = val & 0x7F;
    while (val >>= 7) {
        bytes[n++] = (val & 0x7F) | 0x80;
    }
    for (int i = n - 1; i >= 0; --i) {
        buf.push_back(bytes[i]);
    }
}

bool storageExportLoopMidi(const LoopFile& loop, char* outPathBuf, int outPathBufSize) {
    char cleanName[32];
    sanitizeName(loop.name, cleanName, sizeof(cleanName));
    char path[256];
    std::snprintf(path, sizeof(path), "%s/%03u_%s.mid", EXPORTS_DIR, loop.loopId, cleanName);

    std::vector<uint8_t> out;
    out.insert(out.end(), {'M', 'T', 'h', 'd'});
    writeBE32(out, 6);
    writeBE16(out, 1);
    writeBE16(out, 1 + BB_NUM_TRACKS);
    writeBE16(out, 480);

    std::vector<uint8_t> tempoTrk;
    uint32_t usPerQuarter = 60000000u / (loop.bpm > 0 ? loop.bpm : 110);
    writeVarLen(tempoTrk, 0);
    tempoTrk.push_back(0xFF);
    tempoTrk.push_back(0x51);
    tempoTrk.push_back(0x03);
    tempoTrk.push_back((usPerQuarter >> 16) & 0xFF);
    tempoTrk.push_back((usPerQuarter >> 8) & 0xFF);
    tempoTrk.push_back(usPerQuarter & 0xFF);
    writeVarLen(tempoTrk, 0);
    tempoTrk.insert(tempoTrk.end(), {0xFF, 0x2F, 0x00});
    out.insert(out.end(), {'M', 'T', 'r', 'k'});
    writeBE32(out, static_cast<uint32_t>(tempoTrk.size()));
    out.insert(out.end(), tempoTrk.begin(), tempoTrk.end());

    constexpr uint32_t TICKS_PER_STEP = 120;
    int totalSteps = (loop.numBars == 2) ? BB_MAX_STEPS : BB_STEPS_PER_LOOP;
    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        std::vector<uint8_t> trk;
        uint8_t ch = (t == TRACK_DRUMS) ? 9 : static_cast<uint8_t>(t);
        uint32_t lastTick = 0;

        struct MidiEv { uint32_t tick; uint8_t status; uint8_t d1; uint8_t d2; };
        std::vector<MidiEv> evs;
        for (int s = 0; s < totalSteps; ++s) {
            const StepCell& cell = loop.tracks[t].steps[s];
            uint32_t onTick = static_cast<uint32_t>(s) * TICKS_PER_STEP;
            uint32_t offTick = onTick + (cell.gateLength > 0 ? cell.gateLength : 1) * TICKS_PER_STEP - 10;
            uint8_t vel = cell.velocity > 0 ? cell.velocity : 95;
            for (int p = 0; p < BB_MAX_POLY_PER_STEP; ++p) {
                uint8_t note = cell.notes[p];
                if (note == 0) continue;
                evs.push_back({onTick, static_cast<uint8_t>(0x90 | ch), note, vel});
                evs.push_back({offTick, static_cast<uint8_t>(0x80 | ch), note, 0});
            }
        }
        std::sort(evs.begin(), evs.end(), [](const MidiEv& a, const MidiEv& b) {
            return a.tick < b.tick;
        });
        for (const auto& ev : evs) {
            writeVarLen(trk, ev.tick - lastTick);
            lastTick = ev.tick;
            trk.push_back(ev.status);
            trk.push_back(ev.d1);
            trk.push_back(ev.d2);
        }
        writeVarLen(trk, 0);
        trk.insert(trk.end(), {0xFF, 0x2F, 0x00});
        out.insert(out.end(), {'M', 'T', 'r', 'k'});
        writeBE32(out, static_cast<uint32_t>(trk.size()));
        out.insert(out.end(), trk.begin(), trk.end());
    }

    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    size_t wr = std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (outPathBuf && outPathBufSize > 0) {
        std::snprintf(outPathBuf, outPathBufSize, "%s", path);
    }
    return wr == out.size();
}

// ---- Song (.bbsong) Persistence ----

std::vector<SongSummary> storageListSongs() {
    std::vector<SongSummary> list;
    DIR* dir = opendir(SONGS_DIR);
    if (!dir) return list;

    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        size_t len = std::strlen(ent->d_name);
        if (len < 8 || std::strcmp(ent->d_name + len - 7, ".bbsong") != 0) continue;

        char fullPath[512];
        std::snprintf(fullPath, sizeof(fullPath), "%s/%s", SONGS_DIR, ent->d_name);
        FILE* f = std::fopen(fullPath, "rb");
        if (!f) continue;

        SongFile hdr;
        size_t rd = std::fread(&hdr, 1, sizeof(SongFile), f);
        std::fclose(f);
        if (rd == sizeof(SongFile) && std::memcmp(hdr.magic, "BBSG", 4) == 0) {
            SongSummary s{};
            s.songId = hdr.songId;
            s.bpm = hdr.bpm;
            s.numEntries = hdr.numEntries;
            s.hasVocal = hdr.hasSongVocal;
            std::snprintf(s.name, sizeof(s.name), "%s", hdr.name);
            std::snprintf(s.filename, sizeof(s.filename), "%s", ent->d_name);
            list.push_back(s);
        }
    }
    closedir(dir);

    std::sort(list.begin(), list.end(), [](const SongSummary& a, const SongSummary& b) {
        return a.songId < b.songId;
    });
    return list;
}

uint16_t storageGetNextSongId() {
    auto list = storageListSongs();
    uint16_t maxId = 0;
    for (const auto& s : list) {
        if (s.songId > maxId) maxId = s.songId;
    }
    return maxId + 1;
}

static void removeExistingSongFilesForId(uint16_t songId) {
    auto list = storageListSongs();
    for (const auto& s : list) {
        if (s.songId == songId) {
            char path[512];
            std::snprintf(path, sizeof(path), "%s/%s", SONGS_DIR, s.filename);
            std::remove(path);
        }
    }
}

bool storageSaveSong(const SongFile& song) {
    removeExistingSongFilesForId(song.songId);

    char cleanName[32];
    sanitizeName(song.name, cleanName, sizeof(cleanName));

    char tmpPath[256];
    char finalPath[256];
    std::snprintf(tmpPath, sizeof(tmpPath), "%s/%03u_%s.tmp", SONGS_DIR, song.songId, cleanName);
    std::snprintf(finalPath, sizeof(finalPath), "%s/%03u_%s.bbsong", SONGS_DIR, song.songId, cleanName);

    FILE* f = std::fopen(tmpPath, "wb");
    if (!f) return false;
    size_t wr = std::fwrite(&song, 1, sizeof(SongFile), f);
    std::fflush(f);
    std::fclose(f);

    if (wr != sizeof(SongFile)) {
        std::remove(tmpPath);
        return false;
    }
    std::remove(finalPath);
    return std::rename(tmpPath, finalPath) == 0;
}

bool storageLoadSong(uint16_t songId, SongFile& outSong) {
    auto list = storageListSongs();
    for (const auto& s : list) {
        if (s.songId == songId) {
            char path[512];
            std::snprintf(path, sizeof(path), "%s/%s", SONGS_DIR, s.filename);
            FILE* f = std::fopen(path, "rb");
            if (!f) return false;
            size_t rd = std::fread(&outSong, 1, sizeof(SongFile), f);
            std::fclose(f);
            return rd == sizeof(SongFile) && std::memcmp(outSong.magic, "BBSG", 4) == 0;
        }
    }
    return false;
}

bool storageDeleteSong(uint16_t songId) {
    removeExistingSongFilesForId(songId);
    return true;
}

// ---- Vocal WAV & Offline WAV Bounce ----

void storageGetLoopVocalPath(uint16_t loopId, char* outPath, int outSize) {
    std::snprintf(outPath, outSize, "%s/loop_%03u_vocal.wav", VOCALS_DIR, loopId);
}

void storageGetSongVocalPath(uint16_t songId, char* outPath, int outSize) {
    std::snprintf(outPath, outSize, "%s/song_%03u_vocal.wav", VOCALS_DIR, songId);
}

#pragma pack(push, 1)
struct WavHeader {
    char     riff[4];
    uint32_t chunkSize;
    char     wave[4];
    char     fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char     data[4];
    uint32_t dataSize;
};
#pragma pack(pop)

bool storageSaveVocalWav(const char* path, const int16_t* samples, uint32_t sampleCount, uint32_t sampleRate) {
    if (!path || !samples || sampleCount == 0) return false;
    char tmpPath[520];
    std::snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

    WavHeader hdr{};
    std::memcpy(hdr.riff, "RIFF", 4);
    std::memcpy(hdr.wave, "WAVE", 4);
    std::memcpy(hdr.fmt,  "fmt ", 4);
    hdr.fmtSize       = 16;
    hdr.audioFormat   = 1;
    hdr.numChannels   = 1;
    hdr.sampleRate    = sampleRate;
    hdr.bitsPerSample = 16;
    hdr.blockAlign    = 2;
    hdr.byteRate      = sampleRate * 2;
    std::memcpy(hdr.data, "data", 4);
    hdr.dataSize      = sampleCount * sizeof(int16_t);
    hdr.chunkSize     = 36 + hdr.dataSize;

    FILE* f = std::fopen(tmpPath, "wb");
    if (!f) return false;
    size_t w1 = std::fwrite(&hdr, 1, sizeof(WavHeader), f);
    size_t w2 = std::fwrite(samples, sizeof(int16_t), sampleCount, f);
    std::fflush(f);
    std::fclose(f);

    if (w1 != sizeof(WavHeader) || w2 != sampleCount) {
        std::remove(tmpPath);
        return false;
    }
    std::remove(path);
    return std::rename(tmpPath, path) == 0;
}

bool storageLoadVocalWav(const char* path, std::vector<int16_t>& outSamples) {
    outSamples.clear();
    if (!path) return false;
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;

    WavHeader hdr{};
    if (std::fread(&hdr, 1, sizeof(WavHeader), f) != sizeof(WavHeader) ||
        std::memcmp(hdr.riff, "RIFF", 4) != 0 ||
        std::memcmp(hdr.wave, "WAVE", 4) != 0 ||
        hdr.audioFormat != 1 || hdr.numChannels != 1 || hdr.bitsPerSample != 16) {
        std::fclose(f);
        return false;
    }

    uint32_t numSamples = hdr.dataSize / sizeof(int16_t);
    if (numSamples == 0 || numSamples > 16365 * 180) {
        std::fclose(f);
        return false;
    }
    outSamples.resize(numSamples);
    size_t rd = std::fread(outSamples.data(), sizeof(int16_t), numSamples, f);
    std::fclose(f);
    if (rd != numSamples) {
        outSamples.clear();
        return false;
    }
    return true;
}

bool storageDeleteFile(const char* path) {
    if (!path) return false;
    return std::remove(path) == 0;
}

// Offline Stereo WAV Bounce (streams directly to SD in 1024-sample chunks)
static bool bounceArrangementToWav(const char* path, uint16_t bpm,
                                   const std::vector<LoopFile>& loops,
                                   const std::vector<uint8_t>& repeats,
                                   const std::vector<uint8_t>& muteMasks,
                                   const std::vector<int16_t>& vocalSamples,
                                   uint8_t vocalVol, bool vocalMuted,
                                   const AppConfig* cfg) {
    if (loops.empty()) return false;
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;

    WavHeader hdr{};
    std::memcpy(hdr.riff, "RIFF", 4);
    std::memcpy(hdr.wave, "WAVE", 4);
    std::memcpy(hdr.fmt,  "fmt ", 4);
    hdr.fmtSize       = 16;
    hdr.audioFormat   = 1;
    hdr.numChannels   = 2;
    hdr.sampleRate    = 32728;
    hdr.bitsPerSample = 16;
    hdr.blockAlign    = 4;
    hdr.byteRate      = 32728 * 4;
    std::memcpy(hdr.data, "data", 4);
    std::fwrite(&hdr, 1, sizeof(WavHeader), f);

    SynthVoices synth;
    synth.init();
    // Always export WAV in full-range Headphone/Studio mode (no 135 Hz speaker HPF)
    if (cfg) {
        synth.setMasterConfig(cfg->masterVolume, false, cfg->instTrimPct);
    } else {
        int8_t zeroTrims[INST_COUNT] = {0};
        synth.setMasterConfig(100, false, zeroTrims);
    }

    if (bpm < 60 || bpm > 180) bpm = 110;
    double baseStepSamples = (static_cast<double>(BB_SAMPLE_RATE) * 60.0) / (static_cast<double>(bpm) * 4.0);
    constexpr int CHUNK = 1024;
    std::vector<int16_t> chunkBuf(CHUNK * 2);
    uint32_t totalStereoFrames = 0;
    uint32_t vocalPosQ1 = 0; // Fixed-point 1x -> 2x upsample index (16,364 Hz -> 32,728 Hz)
    float vocGain = (!vocalMuted && !vocalSamples.empty()) ? (static_cast<float>(vocalVol) * 0.01f) : 0.0f;

    for (size_t e = 0; e < loops.size(); ++e) {
        const LoopFile& lp = loops[e];
        uint8_t reps = (e < repeats.size() && repeats[e] > 0) ? repeats[e] : 1;
        uint8_t mMask = (e < muteMasks.size()) ? muteMasks[e] : 0;
        int numSteps = (lp.numBars == 2) ? BB_MAX_STEPS : BB_STEPS_PER_LOOP;
        double swing = (lp.swingPercent >= 50 && lp.swingPercent <= 66) ? lp.swingPercent : 50.0;
        float revMix = (lp.reverbMix <= 100) ? (static_cast<float>(lp.reverbMix) * 0.01f) : 0.38f;

        uint8_t trackVols[BB_NUM_TRACKS];
        uint8_t trackMuted[BB_NUM_TRACKS];
        for (int t = 0; t < BB_NUM_TRACKS; ++t) {
            trackVols[t] = lp.tracks[t].volume;
            trackMuted[t] = (lp.tracks[t].muted || (mMask & (1u << t))) ? 1 : 0;
        }

        for (int r = 0; r < reps; ++r) {
            for (int s = 0; s < numSteps; ++s) {
                double stepMult = (s % 2 == 0) ? (swing / 50.0) : ((100.0 - swing) / 50.0);
                int stepSamples = static_cast<int>(std::round(baseStepSamples * stepMult));

                for (int t = 0; t < BB_NUM_TRACKS; ++t) {
                    if (trackMuted[t]) continue;
                    const StepCell& cell = lp.tracks[t].steps[s];
                    if (cell.flags & 1) continue;
                    uint8_t vel = cell.velocity > 0 ? cell.velocity : 95;
                    float bendSemis = static_cast<float>(cell.pitchBendSemisQ4) / 16.0f;
                    uint8_t instId = lp.tracks[t].instrumentId;
                    double gateMult = (instId == INST_LUTE || instId == INST_HARP) ? 3.5
                                    : (instId == INST_UPRIGHT_BASS ? 2.0 : 0.85);
                    int gateSamples = static_cast<int>(baseStepSamples * (cell.gateLength > 0 ? cell.gateLength : 1) * gateMult);
                    for (int p = 0; p < BB_MAX_POLY_PER_STEP; ++p) {
                        if (cell.notes[p] != 0) {
                            synth.triggerNote(t, instId, cell.notes[p], vel, bendSemis, gateSamples, false);
                        }
                    }
                }

                int rem = stepSamples;
                while (rem > 0) {
                    int n = (rem > CHUNK) ? CHUNK : rem;
                    synth.render(chunkBuf.data(), n, 0.0f, 0.0f, trackVols, trackMuted, revMix);
                    if (vocGain > 0.0f) {
                        for (int i = 0; i < n; ++i) {
                            uint32_t vIdx = vocalPosQ1 >> 1;
                            if (vIdx < vocalSamples.size()) {
                                int32_t vSmp = static_cast<int32_t>(vocalSamples[vIdx] * vocGain);
                                int32_t l  = std::clamp<int32_t>(static_cast<int32_t>(chunkBuf[i * 2 + 0]) + vSmp, -32767, 32767);
                                int32_t r2 = std::clamp<int32_t>(static_cast<int32_t>(chunkBuf[i * 2 + 1]) + vSmp, -32767, 32767);
                                chunkBuf[i * 2 + 0] = static_cast<int16_t>(l);
                                chunkBuf[i * 2 + 1] = static_cast<int16_t>(r2);
                                vocalPosQ1++;
                            }
                        }
                    }
                    std::fwrite(chunkBuf.data(), sizeof(int16_t), static_cast<size_t>(n) * 2, f);
                    totalStereoFrames += static_cast<uint32_t>(n);
                    rem -= n;
                }
            }
        }
    }

    synth.shutdown();

    hdr.dataSize = totalStereoFrames * 4;
    hdr.chunkSize = 36 + hdr.dataSize;
    std::fseek(f, 0, SEEK_SET);
    std::fwrite(&hdr, 1, sizeof(WavHeader), f);
    std::fclose(f);
    return true;
}

bool storageExportLoopWav(const LoopFile& loop, const std::vector<int16_t>& vocalSamples,
                          char* outPathBuf, int outPathBufSize, const AppConfig* cfg) {
    char cleanName[32];
    sanitizeName(loop.name, cleanName, sizeof(cleanName));
    char path[256];
    std::snprintf(path, sizeof(path), "%s/%03u_%s.wav", EXPORTS_DIR, loop.loopId, cleanName);

    std::vector<LoopFile> loops = { loop };
    std::vector<uint8_t> reps   = { 2 }; // Export 2 passes so ring-out tail is captured
    std::vector<uint8_t> masks  = { 0 };
    bool ok = bounceArrangementToWav(path, loop.bpm, loops, reps, masks, vocalSamples,
                                     loop.vocalVolume, loop.vocalMuted != 0, cfg);
    if (ok && outPathBuf && outPathBufSize > 0) {
        std::snprintf(outPathBuf, outPathBufSize, "%s", path);
    }
    return ok;
}

bool storageExportSongWav(const SongFile& song, const std::vector<int16_t>& vocalSamples,
                          char* outPathBuf, int outPathBufSize, const AppConfig* cfg) {
    if (song.numEntries == 0) return false;
    char cleanName[32];
    sanitizeName(song.name, cleanName, sizeof(cleanName));
    char path[256];
    std::snprintf(path, sizeof(path), "%s/song_%03u_%s.wav", EXPORTS_DIR, song.songId, cleanName);

    std::vector<LoopFile> loops;
    std::vector<uint8_t> reps;
    std::vector<uint8_t> masks;
    for (int i = 0; i < song.numEntries; ++i) {
        LoopFile lf;
        if (!storageLoadLoop(song.chain[i].loopId, lf)) {
            initEmptyLoop(lf, song.chain[i].loopId, "Missing", song.bpm);
        }
        loops.push_back(lf);
        reps.push_back(song.chain[i].repeatCount > 0 ? song.chain[i].repeatCount : 1);
        masks.push_back(song.chain[i].muteMask);
    }
    bool ok = bounceArrangementToWav(path, song.bpm, loops, reps, masks, vocalSamples,
                                     song.songVocalVolume, song.songVocalMuted != 0, cfg);
    if (ok && outPathBuf && outPathBufSize > 0) {
        std::snprintf(outPathBuf, outPathBufSize, "%s", path);
    }
    return ok;
}

} // namespace BardBeats
