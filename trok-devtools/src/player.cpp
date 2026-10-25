// Trok DevTools (.asi) -- Victor_Trok

#include "player.h"

#include "sfx.h"

#include <windows.h>
#include <mmsystem.h>

#include <string>
#include <vector>

void DevToolsLog(const char* fmt, ...);

namespace player {
namespace {

HWAVEOUT g_out = nullptr;
WAVEHDR g_hdr = {};
std::vector<short> g_pcm;
int g_rate = 22050;
int g_bank = -1, g_sound = -1;
bool g_repeat = false, g_playing = false;
std::string g_error;

void Close() {
    if (!g_out) {
        return;
    }
    waveOutReset(g_out);
    if (g_hdr.dwFlags & WHDR_PREPARED) {
        waveOutUnprepareHeader(g_out, &g_hdr, sizeof(g_hdr));
    }
    waveOutClose(g_out);
    g_out = nullptr;
    g_hdr = {};
    g_playing = false;
}

} // namespace

bool Play(int bank, int sound) {
    Close();
    g_bank = bank;
    g_sound = sound;
    g_error.clear();
    if (!sfx::ReadPcm(bank, sound, &g_pcm, &g_rate) || g_pcm.empty()) {
        g_error = "n\xC3\xA3o deu para ler o som do pacote";
        return false;
    }
    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = static_cast<DWORD>(g_rate);
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * 2;
    MMRESULT r = waveOutOpen(&g_out, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL);
    if (r != MMSYSERR_NOERROR) {
        g_out = nullptr;
        g_error = "sem sa\xC3\xAD" "da de som (waveOut)";
        DevToolsLog("player: waveOutOpen falhou (%u)", static_cast<unsigned>(r));
        return false;
    }
    g_hdr.lpData = reinterpret_cast<LPSTR>(g_pcm.data());
    g_hdr.dwBufferLength = static_cast<DWORD>(g_pcm.size() * 2);
    if (waveOutPrepareHeader(g_out, &g_hdr, sizeof(g_hdr)) != MMSYSERR_NOERROR ||
        waveOutWrite(g_out, &g_hdr, sizeof(g_hdr)) != MMSYSERR_NOERROR) {
        Close();
        g_error = "o waveOut recusou o som";
        return false;
    }
    g_playing = true;
    return true;
}

void Stop() {
    Close();
}

void Update() {
    if (!g_out || !(g_hdr.dwFlags & WHDR_DONE)) {
        return;
    }
    if (g_repeat) {
        // De novo do comeco: o mesmo bloco, escrito outra vez
        waveOutReset(g_out);
        g_hdr.dwFlags &= ~WHDR_DONE;
        if (waveOutWrite(g_out, &g_hdr, sizeof(g_hdr)) == MMSYSERR_NOERROR) {
            return;
        }
    }
    Close();
}

void SetRepeat(bool on) {
    g_repeat = on;
}

bool Repeat() {
    return g_repeat;
}

bool Playing() {
    return g_playing && g_out;
}

int Bank() {
    return g_bank;
}

int Sound() {
    return g_sound;
}

float Position() {
    if (!g_out || g_rate <= 0 || g_pcm.empty()) {
        return 0;
    }
    MMTIME t = {};
    t.wType = TIME_SAMPLES;
    if (waveOutGetPosition(g_out, &t, sizeof(t)) != MMSYSERR_NOERROR) {
        return 0;
    }
    DWORD samples = t.wType == TIME_SAMPLES ? t.u.sample : t.wType == TIME_BYTES ? t.u.cb / 2 : 0;
    return static_cast<float>(samples % g_pcm.size()) / g_rate;
}

float Length() {
    return g_rate > 0 ? static_cast<float>(g_pcm.size()) / g_rate : 0;
}

const char* Error() {
    return g_error.c_str();
}

} // namespace player
