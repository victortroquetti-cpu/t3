// Trok Hitmarker (.asi) -- Victor_Trok
// Sons: o .mp3 (minimp3, CC0) ou o .wav da pasta vira PCM 16 bits e um buffer do DirectSound, com 4 copias para o
// mesmo som tocar por cima dele mesmo (rajada de M4). O DirectSound e o do Windows, o mesmo que o GTA usa; o mod abre o
// dele, no modo normal (nao mexe no formato do som do jogo) e preso a janela do GTA (minimizado, fica mudo).

#include "sound.h"

#include "res.h"

#include <mmreg.h>
#include <dsound.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#include "minimp3_ex.h"

void HitLog(const char* fmt, ...); // main.cpp

namespace sound {

const char* const NAMES[COUNT] = {"acerto", "cabeca", "dano_levado", "abate", "abate_cabeca", "morte"};

namespace {

constexpr int VOICES = 4;
constexpr int MAX_SECONDS = 10;

struct Pcm {
    std::vector<short> samples; // intercalado
    int channels = 1, rate = 44100;
};

struct Sound {
    IDirectSoundBuffer* voices[VOICES] = {};
    unsigned long long stamp = 1; // 1 = ainda nao carregado
    bool custom = false;
};

IDirectSound8* g_ds = nullptr;
Sound g_sounds[COUNT];
char g_folder[MAX_PATH] = "";
int g_volume = 80;
float g_check = 0;
std::vector<int> g_queue;
float g_queueWait = 0;

short Clip(float v) {
    return static_cast<short>(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

// Le um .wav: PCM de 8, 16, 24 ou 32 bits, ou float de 32 bits (tambem no formato "extensible").
bool DecodeWav(const unsigned char* d, size_t n, Pcm* out) {
    if (n < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) {
        return false;
    }
    int format = 0, channels = 0, rate = 0, bits = 0;
    const unsigned char* data = nullptr;
    size_t dataSize = 0;
    for (size_t p = 12; p + 8 <= n;) {
        unsigned size = d[p + 4] | d[p + 5] << 8 | d[p + 6] << 16 | static_cast<unsigned>(d[p + 7]) << 24;
        const unsigned char* body = d + p + 8;
        size_t avail = n - (p + 8);
        if (!memcmp(d + p, "fmt ", 4) && size >= 16 && avail >= 16) {
            format = body[0] | body[1] << 8;
            channels = body[2] | body[3] << 8;
            rate = body[4] | body[5] << 8 | body[6] << 16 | body[7] << 24;
            bits = body[14] | body[15] << 8;
            if (format == 0xFFFE && size >= 26 && avail >= 26) {
                format = body[24] | body[25] << 8; // o subformato (1 PCM, 3 float)
            }
        } else if (!memcmp(d + p, "data", 4)) {
            data = body;
            dataSize = size < avail ? size : avail;
        }
        p += 8 + size + (size & 1);
    }
    if (!data || channels < 1 || channels > 8 || rate < 4000 || rate > 192000 ||
        !((format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (format == 3 && bits == 32))) {
        return false;
    }
    size_t bytes = bits / 8, frames = dataSize / (bytes * channels);
    frames = frames < static_cast<size_t>(rate) * MAX_SECONDS ? frames : static_cast<size_t>(rate) * MAX_SECONDS;
    int outCh = channels >= 2 ? 2 : 1;
    out->channels = outCh;
    out->rate = rate;
    out->samples.resize(frames * outCh);
    for (size_t f = 0; f < frames; ++f) {
        for (int c = 0; c < outCh; ++c) {
            const unsigned char* s = data + (f * channels + c) * bytes;
            float v;
            if (format == 3) {
                float x;
                memcpy(&x, s, 4);
                v = x * 32767.0f;
            } else if (bits == 8) {
                v = (s[0] - 128) * 256.0f;
            } else if (bits == 16) {
                v = static_cast<short>(s[0] | s[1] << 8);
            } else if (bits == 24) {
                int x = (s[0] << 8 | s[1] << 16 | s[2] << 24) >> 8;
                v = x / 256.0f;
            } else {
                int x = static_cast<int>(s[0] | s[1] << 8 | s[2] << 16 | static_cast<unsigned>(s[3]) << 24);
                v = x / 65536.0f;
            }
            out->samples[f * outCh + c] = Clip(v);
        }
    }
    return !out->samples.empty();
}

bool DecodeMp3(const unsigned char* d, size_t n, Pcm* out) {
    static mp3dec_t dec;
    mp3dec_file_info_t info = {};
    if (mp3dec_load_buf(&dec, d, n, &info, nullptr, nullptr) || !info.buffer || info.samples == 0 || info.channels < 1 ||
        info.hz < 4000) {
        free(info.buffer);
        return false;
    }
    int ch = info.channels;
    size_t frames = info.samples / ch;
    frames = frames < static_cast<size_t>(info.hz) * MAX_SECONDS ? frames : static_cast<size_t>(info.hz) * MAX_SECONDS;
    int outCh = ch >= 2 ? 2 : 1;
    out->channels = outCh;
    out->rate = info.hz;
    out->samples.resize(frames * outCh);
    for (size_t f = 0; f < frames; ++f) {
        for (int c = 0; c < outCh; ++c) {
            out->samples[f * outCh + c] = info.buffer[f * ch + c];
        }
    }
    free(info.buffer);
    return !out->samples.empty();
}

void Free(Sound& s) {
    for (IDirectSoundBuffer*& v : s.voices) {
        if (v) {
            v->Stop();
            v->Release();
            v = nullptr;
        }
    }
}

bool Upload(Sound& s, const Pcm& pcm) {
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = static_cast<WORD>(pcm.channels);
    wfx.nSamplesPerSec = pcm.rate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = static_cast<WORD>(2 * pcm.channels);
    wfx.nAvgBytesPerSec = pcm.rate * wfx.nBlockAlign;
    DWORD bytes = static_cast<DWORD>(pcm.samples.size() * sizeof(short));
    DSBUFFERDESC desc = {};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2;
    desc.dwBufferBytes = bytes < DSBSIZE_MIN ? DSBSIZE_MIN : bytes;
    desc.lpwfxFormat = &wfx;
    IDirectSoundBuffer* buf = nullptr;
    HRESULT hr = g_ds->CreateSoundBuffer(&desc, &buf, nullptr);
    if (FAILED(hr) || !buf) {
        HitLog("som: CreateSoundBuffer falhou (0x%08X)", static_cast<unsigned>(hr));
        return false;
    }
    void *a = nullptr, *b = nullptr;
    DWORD na = 0, nb = 0;
    if (FAILED(buf->Lock(0, desc.dwBufferBytes, &a, &na, &b, &nb, 0))) {
        buf->Release();
        return false;
    }
    memset(a, 0, na);
    memcpy(a, pcm.samples.data(), bytes < na ? bytes : na);
    buf->Unlock(a, na, b, nb);
    Free(s);
    s.voices[0] = buf;
    for (int i = 1; i < VOICES; ++i) {
        if (FAILED(g_ds->DuplicateSoundBuffer(buf, &s.voices[i]))) {
            s.voices[i] = nullptr;
        }
    }
    return true;
}

// O arquivo do usuario (mp3 antes do wav) ou o padrao de dentro do .asi.
void Load(int id) {
    Sound& s = g_sounds[id];
    char mp3[MAX_PATH], wav[MAX_PATH];
    snprintf(mp3, sizeof(mp3), "%s\\%s.mp3", g_folder, NAMES[id]);
    snprintf(wav, sizeof(wav), "%s\\%s.wav", g_folder, NAMES[id]);
    unsigned long long sm = res::Stamp(mp3), sw = res::Stamp(wav);
    unsigned long long stamp = (sm * 31) ^ sw ^ 2; // nunca 1
    if (stamp == s.stamp) {
        return;
    }
    s.stamp = stamp;
    Pcm pcm;
    std::vector<unsigned char> file;
    const char* used = nullptr;
    if (sm && res::ReadAll(mp3, &file) && DecodeMp3(file.data(), file.size(), &pcm)) {
        used = mp3;
    } else if (sw && res::ReadAll(wav, &file) && DecodeWav(file.data(), file.size(), &pcm)) {
        used = wav;
    } else {
        if (sm || sw) {
            HitLog("som %s: arquivo da pasta nao abriu (mp3 ou wav PCM) -- usando o padrao", NAMES[id]);
        }
        const res::File& def = res::SOUNDS[id];
        if (!DecodeWav(def.data, def.size, &pcm)) {
            return;
        }
    }
    if (Upload(s, pcm)) {
        if (used || s.custom) {
            HitLog("som %s: %s (%.2f s, %d Hz)", NAMES[id], used ? used : "padrao",
                   pcm.samples.size() / static_cast<float>(pcm.channels * pcm.rate), pcm.rate);
        }
        s.custom = used != nullptr;
    }
}

LONG Volume() {
    if (g_volume <= 0) {
        return DSBVOLUME_MIN;
    }
    LONG v = static_cast<LONG>(2000.0 * std::log10(g_volume / 100.0));
    return v < DSBVOLUME_MIN ? DSBVOLUME_MIN : v > DSBVOLUME_MAX ? DSBVOLUME_MAX : v;
}

} // namespace

bool Init(HWND window, const char* folder) {
    if (g_ds) {
        return true;
    }
    snprintf(g_folder, sizeof(g_folder), "%s", folder);
    res::Unpack(folder, res::SOUNDS, COUNT);
    HRESULT hr = DirectSoundCreate8(nullptr, &g_ds, nullptr);
    if (FAILED(hr) || !g_ds) {
        HitLog("som: DirectSoundCreate8 falhou (0x%08X) -- sem sons", static_cast<unsigned>(hr));
        g_ds = nullptr;
        return false;
    }
    hr = g_ds->SetCooperativeLevel(window, DSSCL_NORMAL);
    if (FAILED(hr)) {
        HitLog("som: SetCooperativeLevel falhou (0x%08X) -- sem sons", static_cast<unsigned>(hr));
        g_ds->Release();
        g_ds = nullptr;
        return false;
    }
    for (int i = 0; i < COUNT; ++i) {
        Load(i);
    }
    HitLog("sons prontos (%s)", g_folder);
    return true;
}

void Update(float dt) {
    if (!g_ds) {
        return;
    }
    g_check += dt;
    if (g_check >= 1.0f) {
        g_check = 0;
        for (int i = 0; i < COUNT; ++i) {
            Load(i);
        }
    }
    if (!g_queue.empty()) {
        g_queueWait -= dt;
        if (g_queueWait <= 0) {
            Play(static_cast<Id>(g_queue.front()));
            g_queue.erase(g_queue.begin());
            g_queueWait = 0.75f;
        }
    }
}

void SetVolume(int percent) {
    g_volume = percent < 0 ? 0 : percent > 100 ? 100 : percent;
}

void Play(Id id) {
    if (!g_ds || id < 0 || id >= COUNT || g_volume <= 0) {
        return;
    }
    Sound& s = g_sounds[id];
    IDirectSoundBuffer* pick = nullptr;
    DWORD oldest = 0;
    for (IDirectSoundBuffer* v : s.voices) {
        if (!v) {
            continue;
        }
        DWORD status = 0, pos = 0;
        v->GetStatus(&status);
        if (status & DSBSTATUS_BUFFERLOST) {
            v->Restore();
        }
        if (!(status & DSBSTATUS_PLAYING)) {
            pick = v;
            break;
        }
        v->GetCurrentPosition(&pos, nullptr);
        if (!pick || pos > oldest) { // todos tocando: recomeca o que esta mais adiantado
            pick = v;
            oldest = pos;
        }
    }
    if (!pick) {
        return;
    }
    pick->Stop();
    pick->SetCurrentPosition(0);
    pick->SetVolume(Volume());
    HRESULT hr = pick->Play(0, 0, 0);
    static bool logged[COUNT] = {};
    if (!logged[id]) { // a primeira vez de cada som (para conferir no log)
        logged[id] = true;
        HitLog("som %s tocado (volume %d%%): 0x%08X", NAMES[id], g_volume, static_cast<unsigned>(hr));
    }
}

void PlayAll() {
    g_queue.clear();
    for (int i = 0; i < COUNT; ++i) {
        g_queue.push_back(i);
    }
    g_queueWait = 0;
}

} // namespace sound
