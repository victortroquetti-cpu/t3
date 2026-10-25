// Trok DevTools (.asi) -- Victor_Trok
// CAESound (gta-reversed, 0x74 bytes): slot +0 (int16), som +2 (int16), evento +0xC, posicao +0x24, flags +0x56
// (bit 0 = frontend). CAEBankLoader (0x6E4): slots +0 (CAEBankSlot, 0x12D4 cada, o banco em +0x10), quantos slots
// +0xC, quantos bancos +0xE, quantos pacotes +0x10. O ponteiro para ele fica no AEAudioHardware (0xB5F8B8); o deslocamento
// e procurado (perto de +0xD98) e so vale se os numeros baterem com os arquivos do jogo.
// O gancho roda na thread do jogo, a mesma do menu.

#include "audiospy.h"

#include "gta.h"
#include "sfx.h"

#include "MinHook.h"

void DevToolsLog(const char* fmt, ...);

namespace audiospy {
namespace {

constexpr DWORD FN_REQUEST_NEW_SOUND = 0x4EFB10;
constexpr DWORD AE_AUDIO_HARDWARE = 0xB5F8B8;
constexpr DWORD SLOT_SIZE = 0x12D4;
constexpr size_t HISTORY_MAX = 100;
constexpr DWORD SAME_SOUND_MS = 1500;

using RequestFn = void*(__fastcall*)(void* self, void* edx, BYTE* sound);
RequestFn o_request = nullptr;

bool g_capture = true, g_ignoreFeet = true;
std::deque<Played> g_history;
unsigned g_serial = 0;
DWORD g_loader = 0;
int g_loaderState = 0; // 0 nao procurado, 1 achado, 2 nao achado

bool Readable(DWORD p, size_t n) {
    return p >= 0x10000 && gta::Readable(reinterpret_cast<void*>(p), n);
}

// Procura o carregador de bancos no AEAudioHardware: o ponteiro cujo alvo tem o numero de bancos e de pacotes dos
// arquivos (precisa do sfx lido).
void FindLoader() {
    if (g_loaderState != 0 || sfx::BankCount() == 0) {
        return;
    }
    g_loaderState = 2;
    int banks = sfx::BankCount(), packs = static_cast<int>(sfx::Packs().size());
    for (DWORD off = 0xC00; off < 0x1000; off += 4) {
        if (!Readable(AE_AUDIO_HARDWARE + off, 4)) {
            continue;
        }
        DWORD p = *reinterpret_cast<DWORD*>(AE_AUDIO_HARDWARE + off);
        if (!Readable(p, 0x6E4)) {
            continue;
        }
        WORD slots = *reinterpret_cast<WORD*>(p + 0xC), lkups = *reinterpret_cast<WORD*>(p + 0xE),
             paks = *reinterpret_cast<WORD*>(p + 0x10);
        DWORD slotArray = *reinterpret_cast<DWORD*>(p);
        if (lkups == banks && paks == packs && slots > 0 && slots < 200 && Readable(slotArray, slots * SLOT_SIZE)) {
            g_loader = p;
            g_loaderState = 1;
            DevToolsLog("audio: carregador de bancos em AEAudioHardware+0x%X (%d slots, %d bancos)", off, slots, lkups);
            return;
        }
    }
    DevToolsLog("audio: carregador de bancos nao achado (os sons aparecem so com slot e numero)");
}

int BankInSlot(int slot) {
    if (g_loaderState != 1 || slot < 0 || slot >= *reinterpret_cast<WORD*>(g_loader + 0xC)) {
        return -1;
    }
    DWORD slots = *reinterpret_cast<DWORD*>(g_loader);
    return *reinterpret_cast<short*>(slots + slot * SLOT_SIZE + 0x10);
}

void Record(BYTE* sound) {
    FindLoader();
    Played p;
    p.slot = *reinterpret_cast<short*>(sound);
    p.sound = *reinterpret_cast<short*>(sound + 2);
    p.event = *reinterpret_cast<int*>(sound + 0xC);
    p.x = *reinterpret_cast<float*>(sound + 0x24);
    p.y = *reinterpret_cast<float*>(sound + 0x28);
    p.z = *reinterpret_cast<float*>(sound + 0x2C);
    p.frontend = (*reinterpret_cast<WORD*>(sound + 0x56) & 1) != 0;
    p.bank = BankInSlot(p.slot);
    const sfx::Bank* b = sfx::FindBank(p.bank);
    if (g_ignoreFeet && b && b->kind == sfx::T_FEET) {
        return;
    }
    DWORD now = GetTickCount();
    for (Played& h : g_history) {
        if (now - h.last > SAME_SOUND_MS) {
            break;
        }
        if (h.bank == p.bank && h.sound == p.sound && h.slot == p.slot) {
            h.last = now;
            ++h.count;
            ++g_serial;
            return;
        }
    }
    p.first = p.last = now;
    GetLocalTime(&p.time);
    static bool logged = false;
    if (!logged) {
        logged = true; // a prova de que o gancho pega os sons (e de que o banco do slot bate)
        DevToolsLog("audio: primeiro som: %s %s sound_%03d (slot %d, evento %d)",
                    b ? sfx::Packs()[b->pack].name.c_str() : "?", b ? b->name.c_str() : "?", p.sound + 1, p.slot, p.event);
    }
    g_history.push_front(p);
    while (g_history.size() > HISTORY_MAX) {
        g_history.pop_back();
    }
    ++g_serial;
}

void* __fastcall HookRequestNewSound(void* self, void* edx, BYTE* sound) {
    void* result = o_request(self, edx, sound);
    if (g_capture && sound && Readable(reinterpret_cast<DWORD>(sound), 0x74)) {
        Record(sound);
    }
    return result;
}

} // namespace

bool Install() {
    void* target = reinterpret_cast<void*>(FN_REQUEST_NEW_SOUND);
    MH_STATUS s = MH_CreateHook(target, reinterpret_cast<void*>(HookRequestNewSound), reinterpret_cast<void**>(&o_request));
    if (s == MH_OK) {
        s = MH_EnableHook(target);
    }
    DevToolsLog(s == MH_OK ? "CAESoundManager::RequestNewSound desviado (audio tocando agora)"
                           : "CAESoundManager::RequestNewSound recusado (%d) -- sem audio tocando agora",
                static_cast<int>(s));
    return s == MH_OK;
}

void SetCapture(bool on) {
    g_capture = on;
}

void SetIgnoreFeet(bool on) {
    g_ignoreFeet = on;
}

const std::deque<Played>& History() {
    return g_history;
}

unsigned Serial() {
    return g_serial;
}

void Clear() {
    g_history.clear();
    ++g_serial;
}

bool BanksKnown() {
    return g_loaderState == 1;
}

} // namespace audiospy
