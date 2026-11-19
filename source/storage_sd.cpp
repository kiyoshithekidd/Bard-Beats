#include "storage_sd.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace BardBeats {

static const char* ROOT_DIR    = "sdmc:/3ds/BardBeats";
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

bool storageInit() {
    ensureDir("sdmc:/3ds");
    ensureDir(ROOT_DIR);
    ensureDir(LOOPS_DIR);
    ensureDir(SONGS_DIR);
    ensureDir(VOCALS_DIR);
    ensureDir(EXPORTS_DIR);

    auto existing = storageListLoops();
    if (existing.empty()) {
        for (int i = 0; i < 4; ++i) {
            LoopFile starter;
            initStarterLoop(starter, i);
            storageSaveLoop(starter);
        }
    }

    auto existingSongs = storageListSongs();
    if (existingSongs.empty()) {
        SongFile starterSong;
        initStarterSong(starterSong);
        storageSaveSong(starterSong);
    }
    return true;
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
        FILE* f = std::fopen(fullPath, "rb");
        if (!f) continue;

        LoopFile hdr;
        size_t rd = std::fread(&hdr, 1, sizeof(LoopFile), f);
        std::fclose(f);
        if (rd == sizeof(LoopFile) && std::memcmp(hdr.magic, "BBLP", 4) == 0) {
            LoopSummary s{};
            s.loopId = hdr.loopId;
            s.bpm = hdr.bpm;
            s.scaleId = hdr.scaleId;
            s.hasVocal = hdr.hasVocalClip;
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
            FILE* f = std::fopen(path, "rb");
            if (!f) return false;
            size_t rd = std::fread(&outLoop, 1, sizeof(LoopFile), f);
            std::fclose(f);
            return rd == sizeof(LoopFile) && std::memcmp(outLoop.magic, "BBLP", 4) == 0;
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
    // MThd header: Format 1, 5 tracks (1 tempo + 4 instrument tracks), 480 PPQN
    out.insert(out.end(), {'M', 'T', 'h', 'd'});
    writeBE32(out, 6);
    writeBE16(out, 1);
    writeBE16(out, 1 + BB_NUM_TRACKS);
    writeBE16(out, 480);

    // Tempo track
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

    // 4 Instrumental tracks (120 ticks per 16th note at 480 PPQN)
    constexpr uint32_t TICKS_PER_STEP = 120;
    for (int t = 0; t < BB_NUM_TRACKS; ++t) {
        std::vector<uint8_t> trk;
        uint8_t ch = (t == TRACK_DRUMS) ? 9 : static_cast<uint8_t>(t);
        uint32_t lastTick = 0;

        struct MidiEv { uint32_t tick; uint8_t status; uint8_t d1; uint8_t d2; };
        std::vector<MidiEv> evs;
        for (int s = 0; s < BB_STEPS_PER_LOOP; ++s) {
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

// ---- Vocal WAV Persistence (16-bit mono RIFF WAV) ----

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
    hdr.audioFormat   = 1; // PCM
    hdr.numChannels   = 1; // Mono
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
    if (numSamples == 0 || numSamples > 16365 * 180) { // cap at 3 min
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

} // namespace BardBeats
