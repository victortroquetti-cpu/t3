// Trok Hitmarker (.asi) -- Victor_Trok
// samp.dll 0.3.7 R1. Enderecos da SAMP-API (MIT, LUCHARE / BlastHack), os mesmos que o Trok Tags confere:
//   CLocalPlayer::SendGiveDamage +0x6770 e SendTakeDamage +0x6660 (o pacote 115, o mesmo que o damage informer
//   antigo lia pelo SAMPFUNCS), CDeathWindow::AddMessage +0x66A10 (a mensagem de morte do servidor),
//   CRemotePlayer: colete +0x1B8, vida +0x1BC, estado +0x1D1 (2 = AFK); cor: CRemotePlayer::GetColorAsARGB +0x12A00.

#include "samp.h"

#include "MinHook.h"

#include <cstdio>
#include <cstring>

void HitLog(const char* fmt, ...); // main.cpp

namespace samp {
namespace {

constexpr DWORD NETGAME = 0x21A0F8;
constexpr DWORD DIALOG = 0x21A0B8;
constexpr DWORD SCOREBOARD = 0x21A0B4;
constexpr DWORD FN_SEND_GIVE_DAMAGE = 0x6770;
constexpr DWORD FN_SEND_TAKE_DAMAGE = 0x6660;
constexpr DWORD FN_DEATH_ADD_MESSAGE = 0x66A10;
constexpr DWORD FN_GET_COLOR_ARGB = 0x12A00;
constexpr DWORD FN_LOCAL_NAME = 0x13CD0; // CPlayerPool::GetLocalPlayerName

constexpr DWORD NG_HOST = 0x20, NG_PORT = 0x225, NG_STATE = 0x3BD, NG_POOLS = 0x3CD;
constexpr int GAME_CONNECTED = 14;
constexpr DWORD PL_PLAYER = 0x18;
constexpr int MAX_PLAYERS = 1004;
constexpr DWORD PP_LOCAL_ID = 0x04, PP_INFO = 0x2E, PP_NOT_EMPTY = 0xFDE;
constexpr DWORD PI_REMOTE = 0x00, PI_NAME = 0x0C, PI_NAME_SIZE = 0x1C, PI_NAME_CAP = 0x20;
constexpr DWORD RP_PED = 0x00, RP_ARMOUR = 0x1B8, RP_HEALTH = 0x1BC, RP_STATUS = 0x1D1;
constexpr int STATUS_PAUSED = 2;
constexpr DWORD SP_GAME_PED = 0x2A4;
constexpr DWORD DIALOG_ACTIVE = 0x28;

typedef void(__thiscall* DamageFn)(void*, int, float, int, int);
typedef void(__thiscall* DeathFn)(void*, const char*, const char*, DWORD, DWORD, char);
typedef DWORD(__thiscall* ColorFn)(void*);
typedef const char*(__thiscall* NameFn)(void*);

DWORD g_samp = 0;
Events g_events = {};
DamageFn o_give = nullptr, o_take = nullptr;
DeathFn o_death = nullptr;

template <class T>
T At(DWORD base, DWORD offset) {
    return *reinterpret_cast<T*>(base + offset);
}

bool Ptr(DWORD p) {
    return p >= 0x10000;
}

DWORD Players() {
    if (!g_samp) {
        return 0;
    }
    DWORD ng = At<DWORD>(g_samp, NETGAME);
    if (!Ptr(ng)) {
        return 0;
    }
    DWORD pools = At<DWORD>(ng, NG_POOLS);
    if (!Ptr(pools)) {
        return 0;
    }
    DWORD pool = At<DWORD>(pools, PL_PLAYER);
    return Ptr(pool) ? pool : 0;
}

DWORD Info(DWORD players, int id) {
    if (id < 0 || id >= MAX_PLAYERS || id == At<WORD>(players, PP_LOCAL_ID) || !At<BOOL>(players, PP_NOT_EMPTY + id * 4)) {
        return 0;
    }
    DWORD info = At<DWORD>(players, PP_INFO + id * 4);
    return Ptr(info) ? info : 0;
}

std::string RawName(DWORD info) {
    DWORD size = At<DWORD>(info, PI_NAME_SIZE), cap = At<DWORD>(info, PI_NAME_CAP);
    if (size == 0 || size > 64 || cap < size) {
        return std::string();
    }
    const char* data = cap >= 16 ? At<const char*>(info, PI_NAME) : reinterpret_cast<const char*>(info + PI_NAME);
    if (!Ptr(reinterpret_cast<DWORD>(data))) {
        return std::string();
    }
    return std::string(data, size);
}

std::string Utf8(const std::string& text) {
    wchar_t wide[256];
    int n = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), wide, 255);
    if (n <= 0) {
        return std::string();
    }
    char out[768];
    int m = WideCharToMultiByte(CP_UTF8, 0, wide, n, out, sizeof(out) - 1, nullptr, nullptr);
    return m > 0 ? std::string(out, m) : std::string();
}

// Os ganchos so avisam: o pacote segue para o servidor como sempre.
void __fastcall HookGive(void* self, void*, int id, float damage, int weapon, int part) {
    if (g_events.giveDamage) {
        g_events.giveDamage(id, damage, weapon, part);
    }
    o_give(self, id, damage, weapon, part);
}

void __fastcall HookTake(void* self, void*, int id, float damage, int weapon, int part) {
    if (g_events.takeDamage) {
        g_events.takeDamage(id, damage, weapon, part);
    }
    o_take(self, id, damage, weapon, part);
}

void __fastcall HookDeath(void* self, void*, const char* killer, const char* victim, DWORD kc, DWORD vc, char weapon) {
    if (g_events.death) {
        g_events.death(killer, victim, static_cast<unsigned char>(weapon));
    }
    o_death(self, killer, victim, kc, vc, weapon);
}

} // namespace

bool Install(DWORD base, const Events& events) {
    g_samp = base;
    g_events = events;
    struct Target {
        DWORD offset;
        void* hook;
        void** original;
        const char* name;
    } targets[] = {
        {FN_SEND_GIVE_DAMAGE, reinterpret_cast<void*>(HookGive), reinterpret_cast<void**>(&o_give), "SendGiveDamage"},
        {FN_SEND_TAKE_DAMAGE, reinterpret_cast<void*>(HookTake), reinterpret_cast<void**>(&o_take), "SendTakeDamage"},
        {FN_DEATH_ADD_MESSAGE, reinterpret_cast<void*>(HookDeath), reinterpret_cast<void**>(&o_death),
         "CDeathWindow::AddMessage"},
    };
    for (const Target& t : targets) {
        void* at = reinterpret_cast<void*>(base + t.offset);
        MH_STATUS created = MH_CreateHook(at, t.hook, t.original);
        MH_STATUS enabled = created == MH_OK ? MH_EnableHook(at) : created;
        if (enabled != MH_OK) {
            HitLog("gancho %s (samp.dll+0x%X) recusado (%d) -- o Trok Hitmarker fica desligado", t.name, t.offset, enabled);
            for (const Target& u : targets) {
                MH_DisableHook(reinterpret_cast<void*>(base + u.offset));
                MH_RemoveHook(reinterpret_cast<void*>(base + u.offset));
            }
            g_samp = 0;
            return false;
        }
    }
    HitLog("ganchos do samp.dll instalados (SendGiveDamage, SendTakeDamage, mensagem de morte)");
    return true;
}

ServerInfo Server() {
    ServerInfo s;
    if (!g_samp) {
        return s;
    }
    DWORD ng = At<DWORD>(g_samp, NETGAME);
    if (!Ptr(ng)) {
        return s;
    }
    s.connected = At<int>(ng, NG_STATE) == GAME_CONNECTED;
    const char* host = reinterpret_cast<const char*>(ng + NG_HOST);
    snprintf(s.address, sizeof(s.address), "%.*s:%d", 256, host, At<int>(ng, NG_PORT));
    return s;
}

bool Covered() {
    if (!g_samp) {
        return false;
    }
    DWORD dialog = At<DWORD>(g_samp, DIALOG);
    DWORD scoreboard = At<DWORD>(g_samp, SCOREBOARD);
    return (Ptr(dialog) && At<int>(dialog, DIALOG_ACTIVE) != 0) || (Ptr(scoreboard) && At<BOOL>(scoreboard, 0) != FALSE);
}

bool GetPlayer(int id, Player* out) {
    DWORD players = Players();
    DWORD info = players ? Info(players, id) : 0;
    if (!info) {
        return false;
    }
    DWORD remote = At<DWORD>(info, PI_REMOTE);
    if (!Ptr(remote)) {
        return false;
    }
    out->raw = RawName(info);
    out->nick = Utf8(out->raw);
    DWORD argb = reinterpret_cast<ColorFn>(g_samp + FN_GET_COLOR_ARGB)(reinterpret_cast<void*>(remote));
    out->color = IM_COL32((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, 255);
    out->afk = At<BYTE>(remote, RP_STATUS) == STATUS_PAUSED;
    out->health = At<float>(remote, RP_HEALTH);
    out->armour = At<float>(remote, RP_ARMOUR);
    out->ped = 0;
    DWORD sampPed = At<DWORD>(remote, RP_PED);
    if (Ptr(sampPed)) {
        DWORD ped = At<DWORD>(sampPed, SP_GAME_PED);
        out->ped = Ptr(ped) ? ped : 0;
    }
    return true;
}

static DWORD Remote(int id) {
    DWORD players = Players();
    DWORD info = players ? Info(players, id) : 0;
    DWORD remote = info ? At<DWORD>(info, PI_REMOTE) : 0;
    return Ptr(remote) ? remote : 0;
}

bool Vitals(int id, float* health, float* armour) {
    DWORD remote = Remote(id);
    if (!remote) {
        return false;
    }
    *health = At<float>(remote, RP_HEALTH);
    *armour = At<float>(remote, RP_ARMOUR);
    return true;
}

DWORD PedOf(int id) {
    DWORD remote = Remote(id);
    DWORD sampPed = remote ? At<DWORD>(remote, RP_PED) : 0;
    DWORD ped = Ptr(sampPed) ? At<DWORD>(sampPed, SP_GAME_PED) : 0;
    return Ptr(ped) ? ped : 0;
}

std::string LocalNameRaw() {
    DWORD players = Players();
    if (!players) {
        return std::string();
    }
    const char* name = reinterpret_cast<NameFn>(g_samp + FN_LOCAL_NAME)(reinterpret_cast<void*>(players));
    return name && Ptr(reinterpret_cast<DWORD>(name)) ? std::string(name) : std::string();
}

int FindByName(const char* raw) {
    DWORD players = Players();
    if (!players || !raw || !raw[0]) {
        return -1;
    }
    for (int id = 0; id < MAX_PLAYERS; ++id) {
        DWORD info = Info(players, id);
        if (info && RawName(info) == raw) {
            return id;
        }
    }
    return -1;
}

} // namespace samp
