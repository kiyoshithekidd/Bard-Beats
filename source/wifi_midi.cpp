#include "wifi_midi.h"
#include <3ds.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <malloc.h>
#include <sys/socket.h>
#include <unistd.h>

namespace BardBeats {

static constexpr uint32_t SOC_BUFFERSIZE = 0x100000; // 1 MB aligned to 0x1000
static constexpr uint16_t MIDI_UDP_PORT = 5004;

static struct {
    bool active = false;
    u32* socMem = nullptr;
    int  sock = -1;
    struct sockaddr_in bcastAddr{};
} s_midi;

bool wifiMidiInit() {
    if (s_midi.active) return true;

    s_midi.socMem = static_cast<u32*>(memalign(0x1000, SOC_BUFFERSIZE));
    if (!s_midi.socMem) return false;

    if (R_FAILED(socInit(s_midi.socMem, SOC_BUFFERSIZE))) {
        std::free(s_midi.socMem);
        s_midi.socMem = nullptr;
        return false;
    }

    s_midi.sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_midi.sock < 0) {
        socExit();
        std::free(s_midi.socMem);
        s_midi.socMem = nullptr;
        return false;
    }

    int yes = 1;
    setsockopt(s_midi.sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    int flags = fcntl(s_midi.sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(s_midi.sock, F_SETFL, flags | O_NONBLOCK);
    }

    std::memset(&s_midi.bcastAddr, 0, sizeof(s_midi.bcastAddr));
    s_midi.bcastAddr.sin_family = AF_INET;
    s_midi.bcastAddr.sin_port = htons(MIDI_UDP_PORT);
    s_midi.bcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    s_midi.active = true;
    return true;
}

void wifiMidiExit() {
    if (!s_midi.active) return;
    if (s_midi.sock >= 0) {
        close(s_midi.sock);
        s_midi.sock = -1;
    }
    socExit();
    if (s_midi.socMem) {
        std::free(s_midi.socMem);
        s_midi.socMem = nullptr;
    }
    s_midi.active = false;
}

bool wifiMidiIsActive() {
    return s_midi.active;
}

static void sendPacket3(uint8_t b0, uint8_t b1, uint8_t b2) {
    if (!s_midi.active || s_midi.sock < 0) return;
    uint8_t pkt[3] = { b0, b1, b2 };
    sendto(s_midi.sock, pkt, sizeof(pkt), 0,
           reinterpret_cast<struct sockaddr*>(&s_midi.bcastAddr), sizeof(s_midi.bcastAddr));
}

void wifiMidiSendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    sendPacket3(static_cast<uint8_t>(0x90 | (channel & 0x0F)), note & 0x7F, velocity & 0x7F);
}

void wifiMidiSendNoteOff(uint8_t channel, uint8_t note) {
    sendPacket3(static_cast<uint8_t>(0x80 | (channel & 0x0F)), note & 0x7F, 0);
}

void wifiMidiSendPitchBend(uint8_t channel, float bendSemis) {
    if (!s_midi.active) return;
    float norm = bendSemis / 2.0f;
    if (norm > 1.0f) norm = 1.0f;
    if (norm < -1.0f) norm = -1.0f;
    int val14 = 8192 + static_cast<int>(norm * 8191.0f);
    if (val14 < 0) val14 = 0;
    if (val14 > 16383) val14 = 16383;
    sendPacket3(static_cast<uint8_t>(0xE0 | (channel & 0x0F)),
                static_cast<uint8_t>(val14 & 0x7F),
                static_cast<uint8_t>((val14 >> 7) & 0x7F));
}

} // namespace BardBeats
