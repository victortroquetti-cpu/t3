// Trok KeyBinder -- samp.dll 0.3.7 R1 e gta_sa.exe 1.0 US.
//
// Enderecos e estruturas da SAMP-API (MIT, LUCHARE / BlastHack), as mesmas do Trok Tags e do Trok Chat:
//   - CChat::AddEntry (0x64010): toda mensagem do chat passa por aqui (o gancho chama o original antes e guarda uma
//     copia para os gatilhos; o Trok Chat faz o mesmo, os dois convivem na corrente);
//   - CInput::Send (0x65C60): o que a caixa do chat faz com o texto digitado (comando do cliente, do servidor ou fala);
//   - CPlayerPool: o local (id em +0x04, nick em +0x0A, CLocalPlayer em +0x22) e os remotos (+0x2E, +0xFDE);
//   - CLocalPlayer: m_bIsActive (+0x0C) e m_bIsWasted (+0x10) dizem se nasceu e se morreu;
//   - CGame (0x21A10C): m_nCursorMode em +0x55 (algum menu com o cursor do SA-MP);
//   - CNetGame: endereco (+0x20), nome do servidor (+0x121), porta (+0x225), estado (+0x3BD).
// Do GTA (gta-reversed / plugin-sdk): o CPed do jogador (0xB7CD98) com vida +0x540, colete +0x548, estado +0x530,
// "no veiculo" (bit 0 de +0x46D), veiculo +0x58C, armas +0x5A0 (0x1C cada) e o slot na mao +0x718; o veiculo com o
// motorista em +0x460 e a lataria em +0x4C0; CGame::currArea (0xB72914) e o interior.

#include "samp.h"

#include "MinHook.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

void BinderLog(const char* fmt, ...); // main.cpp

namespace samp {
namespace {

constexpr DWORD NETGAME = 0x21A0F8, CHAT = 0x21A0E4, INPUT = 0x21A0E8, DIALOG = 0x21A0B8, SCOREBOARD = 0x21A0B4,
                GAME = 0x21A10C, LABEL_DRAWER = 0x21A0C0;
constexpr DWORD FN_ADD_ENTRY = 0x64010, FN_SEND = 0x65C60, FN_HAS_NO_OBSTACLES = 0x67530;
constexpr DWORD INPUT_ENABLED = 0x14E0, DIALOG_ACTIVE = 0x28, GAME_CURSOR_MODE = 0x55;
constexpr DWORD NG_HOST = 0x20, NG_HOSTNAME = 0x121, NG_PORT = 0x225, NG_STATE = 0x3BD, NG_POOLS = 0x3CD;
constexpr int GAME_CONNECTED = 14;
constexpr DWORD PL_PLAYER = 0x18;
constexpr int MAX_PLAYERS = 1004;
constexpr DWORD PP_LOCAL_ID = 0x04, PP_LOCAL_NAME = 0x0A, PP_LOCAL_PLAYER = 0x22, PP_INFO = 0x2E, PP_NOT_EMPTY = 0xFDE;
constexpr DWORD PI_REMOTE = 0x00, PI_NAME = 0x0C;
constexpr DWORD RP_PED = 0x00, SP_GAME_PED = 0x2A4;
constexpr DWORD LP_ACTIVE = 0x0C, LP_WASTED = 0x10;
constexpr int CHAT_PLAYER = 2, CHAT_INFO = 4;
constexpr size_t MAX_SEND = 128;

constexpr DWORD GTA_PLAYER_PED = 0xB7CD98, GTA_CURR_AREA = 0xB72914, GTA_THE_CAMERA = 0xB6F028;
constexpr DWORD GTA_CALC_SCREEN_COORS = 0x70CE30;
constexpr DWORD PED_FLAGS_1 = 0x46D, PED_STATE = 0x530, PED_HEALTH = 0x540, PED_ARMOUR = 0x548, PED_VEHICLE = 0x58C,
                PED_WEAPONS = 0x5A0, PED_WEAPON_SIZE = 0x1C, PED_ACTIVE_SLOT = 0x718;
constexpr DWORD VEH_DRIVER = 0x460, VEH_HEALTH = 0x4C0;
constexpr int PEDSTATE_DIE = 54, PEDSTATE_DEAD = 55;

struct Vec3 {
    float x, y, z;
};

typedef void(__thiscall* AddEntryFn)(void*, int, const char*, const char*, DWORD, DWORD);
typedef void(__thiscall* SendFn)(void*, const char*);
typedef BOOL(__thiscall* HasNoObstaclesFn)(void*, Vec3);
typedef bool(__cdecl* CalcScreenCoorsFn)(const Vec3&, Vec3*, float*, float*, bool, bool);

DWORD g_samp = 0;
AddEntryFn o_addEntry = nullptr;
bool g_selfNotice = false;
CRITICAL_SECTION g_chatLock;
std::vector<kb::ChatLine> g_chat;

struct Cached {
    int id;
    std::string nick;
    DWORD info;
};
std::vector<Cached> g_players;
DWORD g_playersAt = 0;
bool g_playersOnce = false;

template <class T>
T At(DWORD base, DWORD offset) {
    return *reinterpret_cast<T*>(base + offset);
}

bool Ptr(DWORD p) {
    return p >= 0x10000;
}

std::string Convert(const char* text, int len, UINT from, UINT to) {
    if (!text || len == 0 || !*text) {
        return std::string();
    }
    int wide = MultiByteToWideChar(from, 0, text, len, nullptr, 0);
    if (wide <= 0) {
        return std::string();
    }
    std::wstring w(wide, L'\0');
    MultiByteToWideChar(from, 0, text, len, &w[0], wide);
    int bytes = WideCharToMultiByte(to, 0, w.c_str(), wide, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return std::string();
    }
    std::string out(bytes, '\0');
    WideCharToMultiByte(to, 0, w.c_str(), wide, &out[0], bytes, nullptr, nullptr);
    while (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return out;
}

DWORD NetGame() {
    if (!g_samp) {
        return 0;
    }
    DWORD ng = At<DWORD>(g_samp, NETGAME);
    return Ptr(ng) ? ng : 0;
}

DWORD PlayerPool() {
    DWORD ng = NetGame();
    if (!ng) {
        return 0;
    }
    DWORD pools = At<DWORD>(ng, NG_POOLS);
    if (!Ptr(pools)) {
        return 0;
    }
    DWORD pool = At<DWORD>(pools, PL_PLAYER);
    return Ptr(pool) ? pool : 0;
}

// std::string do MSVC (16 bytes de buffer, tamanho, capacidade) em UTF-8.
std::string MsvcString(DWORD at) {
    DWORD size = At<DWORD>(at, 0x10), cap = At<DWORD>(at, 0x14);
    if (size == 0 || size > 64 || cap < size) {
        return std::string();
    }
    const char* data = cap >= 16 ? At<const char*>(at, 0) : reinterpret_cast<const char*>(at);
    if (!Ptr(reinterpret_cast<DWORD>(data))) {
        return std::string();
    }
    return Convert(data, static_cast<int>(size), CP_ACP, CP_UTF8);
}

DWORD PedOfInfo(DWORD info) {
    DWORD remote = At<DWORD>(info, PI_REMOTE);
    if (!Ptr(remote)) {
        return 0;
    }
    DWORD sampPed = At<DWORD>(remote, RP_PED);
    if (!Ptr(sampPed)) {
        return 0;
    }
    DWORD ped = At<DWORD>(sampPed, SP_GAME_PED);
    return Ptr(ped) ? ped : 0;
}

Vec3 PedPos(DWORD ped) {
    DWORD matrix = At<DWORD>(ped, 0x14);
    if (Ptr(matrix)) {
        return At<Vec3>(matrix, 0x30);
    }
    return At<Vec3>(ped, 0x04);
}

float Dist(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

DWORD LocalPed() {
    DWORD ped = At<DWORD>(GTA_PLAYER_PED, 0);
    return Ptr(ped) ? ped : 0;
}

// O CPlayerInfo de id ainda e o mesmo da lista? (quem saiu do servidor tem o CPlayerInfo apagado)
DWORD LiveInfo(DWORD pool, const Cached& c) {
    if (!At<BOOL>(pool, PP_NOT_EMPTY + c.id * 4)) {
        return 0;
    }
    DWORD info = At<DWORD>(pool, PP_INFO + c.id * 4);
    return info == c.info ? info : 0;
}

void Refresh(DWORD now) {
    if (g_playersOnce && now - g_playersAt < 250) {
        return;
    }
    g_playersOnce = true;
    g_playersAt = now;
    g_players.clear();
    DWORD pool = PlayerPool();
    if (!pool) {
        return;
    }
    int local = At<WORD>(pool, PP_LOCAL_ID);
    for (int id = 0; id < MAX_PLAYERS; ++id) {
        if (id == local || !At<BOOL>(pool, PP_NOT_EMPTY + id * 4)) {
            continue;
        }
        DWORD info = At<DWORD>(pool, PP_INFO + id * 4);
        if (!Ptr(info)) {
            continue;
        }
        std::string nick = MsvcString(info + PI_NAME);
        if (!nick.empty()) {
            g_players.push_back({id, nick, info});
        }
    }
}

bool Project(const Vec3& world, ImVec2* out, float* scale) {
    Vec3 screen;
    float w, h;
    if (!reinterpret_cast<CalcScreenCoorsFn>(GTA_CALC_SCREEN_COORS)(world, &screen, &w, &h, false, false)) {
        return false;
    }
    *out = ImVec2(screen.x, screen.y);
    *scale = h;
    return true;
}

bool NoObstacles(const Vec3& pos) {
    void* drawer = At<void*>(g_samp, LABEL_DRAWER);
    if (!Ptr(reinterpret_cast<DWORD>(drawer))) {
        return true;
    }
    return reinterpret_cast<HasNoObstaclesFn>(g_samp + FN_HAS_NO_OBSTACLES)(drawer, pos) != FALSE;
}

void __fastcall HookAddEntry(void* chat, void*, int type, const char* text, const char* prefix, DWORD textColor,
                             DWORD prefixColor) {
    o_addEntry(chat, type, text, prefix, textColor, prefixColor);
    if (g_selfNotice || !text || (type != CHAT_PLAYER && type != CHAT_INFO)) {
        return;
    }
    kb::ChatLine line;
    line.text = Convert(text, static_cast<int>(strnlen(text, 512)), CP_ACP, CP_UTF8);
    if (type == CHAT_PLAYER && prefix) {
        line.author = Convert(prefix, static_cast<int>(strnlen(prefix, 64)), CP_ACP, CP_UTF8);
        if (!line.author.empty() && line.author.back() == ':') {
            line.author.pop_back();
        }
        if (kb::SameNick(line.author, LocalName())) {
            return; // a propria fala nunca dispara nada
        }
    }
    if (line.text.empty()) {
        return;
    }
    EnterCriticalSection(&g_chatLock);
    if (g_chat.size() < 64) {
        g_chat.push_back(line);
    }
    LeaveCriticalSection(&g_chatLock);
}

} // namespace

bool Install(DWORD base) {
    g_samp = base;
    InitializeCriticalSection(&g_chatLock);
    void* target = reinterpret_cast<void*>(base + FN_ADD_ENTRY);
    MH_STATUS a = MH_CreateHook(target, reinterpret_cast<void*>(HookAddEntry), reinterpret_cast<void**>(&o_addEntry));
    MH_STATUS b = a == MH_OK ? MH_EnableHook(target) : MH_UNKNOWN;
    if (b != MH_OK) {
        BinderLog("gancho do chat recusado (%d, %d) -- a condicao \"mensagem no chat\" fica sem efeito", a, b);
        if (a == MH_OK) {
            MH_RemoveHook(target);
        }
        o_addEntry = nullptr;
        return true; // o resto funciona sem o gancho
    }
    BinderLog("gancho do chat ligado (CChat::AddEntry)");
    return true;
}

ServerInfo Server() {
    ServerInfo s;
    DWORD ng = NetGame();
    if (!ng) {
        return s;
    }
    s.connected = At<int>(ng, NG_STATE) == GAME_CONNECTED;
    char addr[300];
    snprintf(addr, sizeof(addr), "%.*s:%d", 256, reinterpret_cast<const char*>(ng + NG_HOST), At<int>(ng, NG_PORT));
    s.address = addr;
    const char* host = reinterpret_cast<const char*>(ng + NG_HOSTNAME);
    s.hostname = Convert(host, static_cast<int>(strnlen(host, 256)), CP_ACP, CP_UTF8);
    return s;
}

bool InputReady() {
    return g_samp && Ptr(At<DWORD>(g_samp, INPUT));
}

bool ChatOpen() {
    DWORD input = g_samp ? At<DWORD>(g_samp, INPUT) : 0;
    return Ptr(input) && At<int>(input, INPUT_ENABLED) != 0;
}

bool DialogOpen() {
    DWORD dialog = g_samp ? At<DWORD>(g_samp, DIALOG) : 0;
    return Ptr(dialog) && At<int>(dialog, DIALOG_ACTIVE) != 0;
}

bool ScoreboardOpen() {
    DWORD board = g_samp ? At<DWORD>(g_samp, SCOREBOARD) : 0;
    return Ptr(board) && At<BOOL>(board, 0) != FALSE;
}

int CursorMode() {
    DWORD game = g_samp ? At<DWORD>(g_samp, GAME) : 0;
    return Ptr(game) ? At<int>(game, GAME_CURSOR_MODE) : 0;
}

void Send(const std::string& utf8) {
    DWORD input = g_samp ? At<DWORD>(g_samp, INPUT) : 0;
    if (!Ptr(input)) {
        return;
    }
    std::string text = Convert(utf8.c_str(), static_cast<int>(utf8.size()), CP_UTF8, CP_ACP);
    if (text.size() > MAX_SEND) {
        // Corta sem partir um caractere de dois bytes (paginas de codigo asiaticas).
        size_t cut = 0;
        while (cut < text.size()) {
            size_t next = cut + (IsDBCSLeadByte(static_cast<BYTE>(text[cut])) ? 2 : 1);
            if (next > MAX_SEND) {
                break;
            }
            cut = next;
        }
        text.resize(cut);
    }
    if (!text.empty()) {
        reinterpret_cast<SendFn>(g_samp + FN_SEND)(reinterpret_cast<void*>(input), text.c_str());
    }
}

void Notice(const char* utf8) {
    DWORD chat = g_samp ? At<DWORD>(g_samp, CHAT) : 0;
    if (!Ptr(chat)) {
        return;
    }
    std::string text = Convert(utf8, -1, CP_UTF8, CP_ACP);
    g_selfNotice = true; // o nosso gancho deixa passar sem guardar
    reinterpret_cast<AddEntryFn>(g_samp + FN_ADD_ENTRY)(reinterpret_cast<void*>(chat), 8, text.c_str(), nullptr,
                                                        0xFFFFFFFF, 0);
    g_selfNotice = false;
}

kb::Me ReadMe(bool connected) {
    kb::Me m;
    m.connected = connected;
    DWORD ped = LocalPed();
    if (!ped) {
        return m;
    }
    m.health = At<float>(ped, PED_HEALTH);
    m.armour = At<float>(ped, PED_ARMOUR);
    int state = At<int>(ped, PED_STATE);
    m.dead = state == PEDSTATE_DIE || state == PEDSTATE_DEAD || m.health <= 0.0f;
    DWORD vehicle = At<DWORD>(ped, PED_VEHICLE);
    if ((At<BYTE>(ped, PED_FLAGS_1) & 1) && Ptr(vehicle)) {
        m.inVehicle = true;
        m.vehicle = vehicle;
        m.driver = At<DWORD>(vehicle, VEH_DRIVER) == ped;
        m.vehicleHealth = At<float>(vehicle, VEH_HEALTH);
    }
    BYTE slot = At<BYTE>(ped, PED_ACTIVE_SLOT);
    if (slot < 13) {
        m.weapon = At<int>(ped, PED_WEAPONS + slot * PED_WEAPON_SIZE);
    }
    m.interior = At<int>(GTA_CURR_AREA, 0);
    m.spawned = !m.dead;
    DWORD pool = PlayerPool();
    DWORD local = pool ? At<DWORD>(pool, PP_LOCAL_PLAYER) : 0;
    if (Ptr(local)) {
        bool active = At<BOOL>(local, LP_ACTIVE) != FALSE;
        bool wasted = At<BOOL>(local, LP_WASTED) != FALSE;
        m.spawned = active && !wasted && !m.dead;
        m.dead = m.dead || wasted;
    }
    return m;
}

void Players(DWORD now, std::vector<kb::Player>* out) {
    out->clear();
    Refresh(now);
    DWORD pool = PlayerPool();
    DWORD me = LocalPed();
    if (!pool) {
        return;
    }
    Vec3 mine = me ? PedPos(me) : Vec3{0, 0, 0};
    for (const Cached& c : g_players) {
        DWORD info = LiveInfo(pool, c);
        if (!info) {
            continue;
        }
        kb::Player p;
        p.id = c.id;
        p.nick = c.nick;
        DWORD ped = PedOfInfo(info);
        if (ped && me) {
            p.distance = std::max(0.01f, Dist(PedPos(ped), mine));
        }
        out->push_back(p);
    }
}

std::string LocalName() {
    DWORD pool = PlayerPool();
    return pool ? MsvcString(pool + PP_LOCAL_NAME) : std::string();
}

int LocalId() {
    DWORD pool = PlayerPool();
    return pool ? At<WORD>(pool, PP_LOCAL_ID) : -1;
}

bool FindTarget(float maxDistance, float screenW, float screenH, kb::Player* out, ImVec2* dot) {
    DWORD pool = PlayerPool();
    DWORD me = LocalPed();
    if (!pool || !me) {
        return false;
    }
    Vec3 mine = PedPos(me);
    ImVec2 center(screenW * 0.5f, screenH * 0.5f);
    float bestScore = 1.0f, bestDist = 0;
    bool found = false;
    for (const Cached& c : g_players) {
        DWORD info = LiveInfo(pool, c);
        DWORD ped = info ? PedOfInfo(info) : 0;
        if (!ped) {
            continue;
        }
        Vec3 body = PedPos(ped);
        float d = Dist(body, mine);
        if (d > maxDistance || d < 0.3f) {
            continue;
        }
        ImVec2 at;
        float scale;
        if (!Project(body, &at, &scale) || at.x < 0 || at.y < 0 || at.x > screenW || at.y > screenH) {
            continue;
        }
        // Cabe na "linha reta": ate a largura do corpo (0,6 m na distancia dele) ou 4,5% da tela, o que for maior.
        float reach = std::max(screenH * 0.045f, 0.6f * scale);
        float dx = at.x - center.x, dy = at.y - center.y;
        float score = std::sqrt(dx * dx + dy * dy) / reach;
        if (score >= 1.0f || (found && score > bestScore + 0.15f) ||
            (found && std::fabs(score - bestScore) <= 0.15f && d >= bestDist)) {
            continue;
        }
        if (!NoObstacles(body)) {
            continue;
        }
        bestScore = score;
        bestDist = d;
        found = true;
        out->id = c.id;
        out->nick = c.nick;
        out->distance = d;
        *dot = at;
    }
    return found;
}

bool PlayerDot(int id, float maxDistance, float screenW, float screenH, ImVec2* dot) {
    DWORD pool = PlayerPool();
    DWORD me = LocalPed();
    if (!pool || !me) {
        return false;
    }
    for (const Cached& c : g_players) {
        if (c.id != id) {
            continue;
        }
        DWORD info = LiveInfo(pool, c);
        DWORD ped = info ? PedOfInfo(info) : 0;
        if (!ped) {
            return false;
        }
        Vec3 body = PedPos(ped);
        float scale;
        return Dist(body, PedPos(me)) <= maxDistance && Project(body, dot, &scale) && dot->x >= 0 && dot->y >= 0 &&
               dot->x <= screenW && dot->y <= screenH;
    }
    return false;
}

void TakeChat(std::vector<kb::ChatLine>* out) {
    out->clear();
    if (!g_samp || !o_addEntry) {
        return;
    }
    EnterCriticalSection(&g_chatLock);
    out->swap(g_chat);
    LeaveCriticalSection(&g_chatLock);
}

} // namespace samp
