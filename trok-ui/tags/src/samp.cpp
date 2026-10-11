// Trok Tags -- samp.dll 0.3.7 R1.
//
// Enderecos e estruturas conferidos com a SAMP-API (MIT, LUCHARE / BlastHack): os cabecalhos do R1 foram
// compilados e os deslocamentos impressos (vida em +0x1BC, colete em +0x1B8, AFK em +0x1D1, texto 3D de 29
// bytes...). Os baloes de fala nao estao mapeados no R1: o ponteiro vem da mesma posicao relativa do R3
// (0x21A0DC, entre o CNetStats e o CConfig, como no R3) e a estrutura e conferida antes de qualquer escrita;
// se nao conferir, os baloes do SA-MP ficam como estao.

#include "samp.h"

#include "MinHook.h"
#include "config.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

void TagsLog(const char* fmt, ...); // main.cpp

namespace samp {
namespace {

// ---------------------------------------------------------------- samp.dll 0.3.7 R1

constexpr DWORD NETGAME = 0x21A0F8;             // CNetGame*
constexpr DWORD DIALOG = 0x21A0B8;              // CDialog*
constexpr DWORD SCOREBOARD = 0x21A0B4;          // CScoreboard* (m_bIsEnabled no comeco)
constexpr DWORD LABEL_DRAWER = 0x21A0C0;        // CLabel* (o desenho de texto 3D do SA-MP)
constexpr DWORD CHAT_BUBBLE = 0x21A0DC;         // CChatBubble* (conferido em tempo de execucao)
constexpr DWORD LAST_PLAYERS_UPDATE = 0x104978; // ultimo pedido de pontos e pings (TICK)
constexpr DWORD FN_DRAW_LABEL = 0x686C0;        // CPlayerTags::DrawLabel
constexpr DWORD FN_DRAW_HEALTH_BAR = 0x689C0;   // CPlayerTags::DrawHealthBar
constexpr DWORD FN_LABEL_POOL_DRAW = 0x1340;    // CLabelPool::Draw
constexpr DWORD FN_HAS_NO_OBSTACLES = 0x67530;  // CLabel::HasNoObstacles (o teste de parede do SA-MP)
constexpr DWORD FN_UPDATE_PLAYERS = 0x8A10;     // CNetGame::UpdatePlayers (o pedido que o Tab faz)

constexpr DWORD NG_HOST = 0x20, NG_PORT = 0x225, NG_STATE = 0x3BD, NG_SETTINGS = 0x3C5, NG_POOLS = 0x3CD;
constexpr int GAME_CONNECTED = 14;
constexpr DWORD ST_NAMETAG_DIST = 0x27, ST_NAMETAG_LOS = 0x2F, ST_NAMETAGS = 0x38; // CNetGame::Settings
constexpr DWORD PL_LABEL = 0x0C, PL_PLAYER = 0x18;                                   // CNetGame::Pools
constexpr int MAX_PLAYERS = 1004;
constexpr DWORD PP_LOCAL_ID = 0x04, PP_INFO = 0x2E, PP_NOT_EMPTY = 0xFDE; // CPlayerPool
// CPlayerInfo: o nome e um std::string do MSVC (16 bytes de buffer, tamanho, capacidade).
constexpr DWORD PI_REMOTE = 0x00, PI_NAME = 0x0C, PI_NAME_SIZE = 0x1C, PI_NAME_CAP = 0x20, PI_PING = 0x28;
constexpr DWORD RP_PED = 0x00, RP_ARMOUR = 0x1B8, RP_HEALTH = 0x1BC, RP_STATUS = 0x1D1; // CRemotePlayer
constexpr int STATUS_PAUSED = 2;                                                       // PLAYER_STATUS_TIMEOUT
constexpr DWORD SP_GAME_PED = 0x2A4; // CPed do SA-MP -> CPed do GTA
constexpr int MAX_LABELS = 2048;
constexpr DWORD LABEL_SIZE = 0x1D, LB_TEXT = 0x00, LB_COLOR = 0x04, LB_POS = 0x08, LB_DIST = 0x14, LB_LOS = 0x18,
                LB_PLAYER = 0x19, LB_NOT_EMPTY = 0xE800; // CLabelPool / TextLabel
constexpr WORD INVALID_ID = 0xFFFF;
// CChatBubble::Player: existe, texto[256], criado, vida, cor, distancia, largura da linha.
constexpr DWORD BUBBLE_SIZE = 280, BB_TEXT = 4, BB_CREATED = 260, BB_LIFE = 264, BB_COLOR = 268, BB_DIST = 272;
constexpr DWORD DIALOG_ACTIVE = 0x28;

// ---------------------------------------------------------------- gta_sa.exe 1.0 US

constexpr DWORD GTA_CALC_SCREEN_COORS = 0x70CE30; // CSprite::CalcScreenCoors
constexpr DWORD GTA_GET_BONE_POSITION = 0x5E4280; // CPed::GetBonePosition
constexpr DWORD GTA_THE_CAMERA = 0xB6F028;
constexpr unsigned BONE_HEAD = 8;
constexpr float ANCHOR_ABOVE_HEAD = 0.25f; // m: onde a pilha comeca, um pouco acima da cabeca

struct Vec3 {
    float x, y, z;
};

typedef void(__thiscall* DrawLabelFn)(void*, Vec3*, const char*, DWORD, float, bool, int);
typedef void(__thiscall* DrawHealthBarFn)(void*, Vec3*, float, float, float);
typedef void(__thiscall* LabelPoolDrawFn)(void*);
typedef BOOL(__thiscall* HasNoObstaclesFn)(void*, Vec3);
typedef void(__thiscall* UpdatePlayersFn)(void*);
typedef bool(__cdecl* CalcScreenCoorsFn)(const Vec3&, Vec3*, float*, float*, bool, bool);
typedef void(__thiscall* GetBonePositionFn)(void*, Vec3&, unsigned, bool);

DWORD g_samp = 0;
DrawLabelFn o_drawLabel = nullptr;
DrawHealthBarFn o_drawHealthBar = nullptr;
LabelPoolDrawFn o_labelPoolDraw = nullptr;

// O que esta ligado neste quadro (Keep) e ate quando.
bool g_takeTags = false, g_takeFixed = false, g_takeBubbles = false;
DWORD g_keepUntil = 0;
DWORD g_frame = 1;

// Nametags que o SA-MP decidiu desenhar neste quadro (o gancho guarda e nao desenha).
struct Seen {
    DWORD frame;
    DWORD color; // ARGB
    float dist;
};
Seen g_seen[MAX_PLAYERS] = {};

struct StoredBubble {
    bool active;
    std::string text; // UTF-8
    DWORD color;      // ARGB
    float dist;
    DWORD created;
    int life;
    bool timeClock; // criado pelo timeGetTime (senao, GetTickCount)
};
StoredBubble g_bubbles[MAX_PLAYERS];

DWORD g_afkSince[MAX_PLAYERS] = {};
int g_bubbleState = 0; // 0 ainda nao conferido, 1 conferido, -1 recusado
DWORD g_bubbleChecked = 0;
DWORD g_bubblePtr = 0; // estrutura conferida (os baloes recolhidos voltam so para ela)
DWORD g_pingRequest = 0;

template <class T>
T At(DWORD base, DWORD offset) {
    return *reinterpret_cast<T*>(base + offset);
}

bool Ptr(DWORD p) {
    return p >= 0x10000;
}

bool Active() {
    return static_cast<LONG>(g_keepUntil - GetTickCount()) > 0;
}

DWORD NetGame() {
    if (!g_samp) {
        return 0;
    }
    DWORD ng = At<DWORD>(g_samp, NETGAME);
    return Ptr(ng) ? ng : 0;
}

DWORD Pool(DWORD which) {
    DWORD ng = NetGame();
    if (!ng) {
        return 0;
    }
    DWORD pools = At<DWORD>(ng, NG_POOLS);
    if (!Ptr(pools)) {
        return 0;
    }
    DWORD pool = At<DWORD>(pools, which);
    return Ptr(pool) ? pool : 0;
}

// CPlayerInfo de um jogador remoto conectado (nunca o local).
DWORD Info(DWORD players, int id) {
    if (id < 0 || id >= MAX_PLAYERS || id == At<WORD>(players, PP_LOCAL_ID) || !At<BOOL>(players, PP_NOT_EMPTY + id * 4)) {
        return 0;
    }
    DWORD info = At<DWORD>(players, PP_INFO + id * 4);
    return Ptr(info) ? info : 0;
}

DWORD RemoteOf(DWORD info) {
    DWORD remote = At<DWORD>(info, PI_REMOTE);
    return Ptr(remote) ? remote : 0;
}

DWORD PedOf(DWORD remote) {
    DWORD sampPed = At<DWORD>(remote, RP_PED);
    if (!Ptr(sampPed)) {
        return 0;
    }
    DWORD ped = At<DWORD>(sampPed, SP_GAME_PED);
    return Ptr(ped) ? ped : 0;
}

// Texto do servidor (pagina de codigo do Windows, a mesma que o SA-MP usa para desenhar) em UTF-8.
std::string Utf8(const char* text, int len) {
    wchar_t wide[1024];
    int n = MultiByteToWideChar(CP_ACP, 0, text, len, wide, 1023);
    if (n <= 0) {
        return std::string();
    }
    char out[3072];
    int m = WideCharToMultiByte(CP_UTF8, 0, wide, n, out, sizeof(out) - 1, nullptr, nullptr);
    return m > 0 ? std::string(out, m) : std::string();
}

std::string NameOf(DWORD info) {
    DWORD size = At<DWORD>(info, PI_NAME_SIZE), cap = At<DWORD>(info, PI_NAME_CAP);
    if (size == 0 || size > 64 || cap < size) {
        return std::string();
    }
    const char* data = cap >= 16 ? At<const char*>(info, PI_NAME) : reinterpret_cast<const char*>(info + PI_NAME);
    if (!Ptr(reinterpret_cast<DWORD>(data))) {
        return std::string();
    }
    return Utf8(data, static_cast<int>(size));
}

ImU32 FromArgb(DWORD c, bool opaque) {
    int a = opaque ? 255 : static_cast<int>(c >> 24);
    return IM_COL32((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, a);
}

// "Nome (ID)", como o SA-MP escreve a nametag.
int IdFromLabel(const char* text) {
    const char* open = strrchr(text, '(');
    if (!open) {
        return -1;
    }
    int id = 0, digits = 0;
    const char* p = open + 1;
    while (*p >= '0' && *p <= '9' && digits < 5) {
        id = id * 10 + (*p++ - '0');
        ++digits;
    }
    if (!digits || *p != ')' || id >= MAX_PLAYERS) {
        return -1;
    }
    return id;
}

Vec3 CameraPos() {
    DWORD matrix = At<DWORD>(GTA_THE_CAMERA, 0x14);
    if (Ptr(matrix)) {
        return At<Vec3>(matrix, 0x30);
    }
    return At<Vec3>(GTA_THE_CAMERA, 0x04);
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

Vec3 HeadPos(DWORD ped) {
    Vec3 body = PedPos(ped);
    Vec3 head = {0, 0, 0};
    reinterpret_cast<GetBonePositionFn>(GTA_GET_BONE_POSITION)(reinterpret_cast<void*>(ped), head, BONE_HEAD, false);
    // Osso ainda nao calculado (personagem acabou de aparecer): usa o corpo.
    if (Dist(head, body) > 3.0f) {
        head = {body.x, body.y, body.z + 0.75f};
    }
    return head;
}

bool Project(const Vec3& world, ImVec2* out) {
    Vec3 screen;
    float w, h;
    if (!reinterpret_cast<CalcScreenCoorsFn>(GTA_CALC_SCREEN_COORS)(world, &screen, &w, &h, false, false)) {
        return false;
    }
    *out = ImVec2(screen.x, screen.y);
    return true;
}

bool NoObstacles(const Vec3& pos) {
    void* drawer = At<void*>(g_samp, LABEL_DRAWER);
    if (!Ptr(reinterpret_cast<DWORD>(drawer))) {
        return true;
    }
    return reinterpret_cast<HasNoObstaclesFn>(g_samp + FN_HAS_NO_OBSTACLES)(drawer, pos) != FALSE;
}

// ---------------------------------------------------------------- ganchos

void __fastcall HookDrawLabel(void* self, void*, Vec3* pos, const char* text, DWORD color, float dist, bool drawStatus,
                              int status) {
    if (g_takeTags && Active() && text) {
        int id = IdFromLabel(text);
        if (id >= 0) {
            g_seen[id] = {g_frame, color, dist};
            return;
        }
    }
    o_drawLabel(self, pos, text, color, dist, drawStatus, status);
}

void __fastcall HookDrawHealthBar(void* self, void*, Vec3* pos, float health, float armour, float dist) {
    if (g_takeTags && Active()) {
        return;
    }
    o_drawHealthBar(self, pos, health, armour, dist);
}

// Textos 3D presos a jogadores visiveis saem da vez do SA-MP so durante o desenho dele (os dados continuam la
// para o resto do SA-MP e para o nosso desenho).
void __fastcall HookLabelPoolDraw(void* pool, void*) {
    if (!(g_takeFixed && Active())) {
        o_labelPoolDraw(pool);
        return;
    }
    DWORD base = reinterpret_cast<DWORD>(pool);
    DWORD players = Pool(PL_PLAYER);
    static short hidden[MAX_LABELS];
    int count = 0;
    if (players) {
        for (int i = 0; i < MAX_LABELS; ++i) {
            BOOL* used = reinterpret_cast<BOOL*>(base + LB_NOT_EMPTY + i * 4);
            if (!*used) {
                continue;
            }
            WORD player = At<WORD>(base, i * LABEL_SIZE + LB_PLAYER);
            if (player == INVALID_ID) {
                continue;
            }
            DWORD info = Info(players, player);
            DWORD remote = info ? RemoteOf(info) : 0;
            if (!remote || !PedOf(remote)) {
                continue;
            }
            *used = FALSE;
            hidden[count++] = static_cast<short>(i);
        }
    }
    o_labelPoolDraw(pool);
    for (int k = 0; k < count; ++k) {
        *reinterpret_cast<BOOL*>(base + LB_NOT_EMPTY + hidden[k] * 4) = TRUE;
    }
}

// Paginas da faixa toda gravaveis (a estrutura dos baloes tem 281 KB).
bool Writable(DWORD addr, DWORD size) {
    DWORD end = addr + size;
    while (addr < end) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
            (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !(mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY))) {
            return false;
        }
        addr = reinterpret_cast<DWORD>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return true;
}

// A estrutura dos baloes confere? Todo "existe" vale 0 ou 1 nos 1004 jogadores (uma estrutura diferente
// quase nunca passa nisso). Confere de novo a cada 5 s (e se o ponteiro mudar); uma falha desliga os baloes de
// vez nesta sessao.
bool BubblesUsable(DWORD bubbles) {
    if (g_bubbleState < 0 || !Ptr(bubbles)) {
        return false;
    }
    DWORD now = GetTickCount();
    if (g_bubbleState == 1 && bubbles == g_bubblePtr && now - g_bubbleChecked < 5000) {
        return true;
    }
    bool ok = Writable(bubbles, BUBBLE_SIZE * MAX_PLAYERS);
    for (int i = 0; ok && i < MAX_PLAYERS; ++i) {
        int exists = At<int>(bubbles, i * BUBBLE_SIZE);
        ok = exists == 0 || exists == 1;
    }
    if (!ok) {
        g_bubbleState = -1;
        TagsLog("baloes: a estrutura em samp.dll+0x%X nao conferiu -- os baloes do SA-MP ficam como estao", CHAT_BUBBLE);
        return false;
    }
    if (g_bubbleState == 0) {
        TagsLog("baloes: estrutura conferida (0x%08X) -- os baloes passam a ser do Trok Tags", bubbles);
    }
    g_bubbleState = 1;
    g_bubbleChecked = now;
    g_bubblePtr = bubbles;
    return true;
}

bool Expired(const StoredBubble& b) {
    DWORD clock = b.timeClock ? timeGetTime() : GetTickCount();
    return clock - b.created > static_cast<DWORD>(b.life);
}

// Os baloes recolhidos voltam para o SA-MP, que desenha o resto do tempo de cada um (o desenho do Trok Tags
// parou ou os baloes foram desligados no menu). So onde o SA-MP nao pos outro balao no lugar.
void GiveBackBubbles() {
    DWORD bubbles = g_bubbleState == 1 && At<DWORD>(g_samp, CHAT_BUBBLE) == g_bubblePtr ? g_bubblePtr : 0;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        StoredBubble& b = g_bubbles[i];
        if (!b.active) {
            continue;
        }
        b.active = false;
        DWORD e = bubbles + i * BUBBLE_SIZE;
        if (bubbles && !Expired(b) && At<int>(e, 0) == 0 && At<DWORD>(e, BB_CREATED) == b.created) {
            *reinterpret_cast<int*>(e) = 1;
        }
    }
}

} // namespace

bool Install(DWORD base) {
    g_samp = base;
    struct Target {
        DWORD offset;
        void* hook;
        void** original;
        const char* name;
    } targets[] = {
        {FN_DRAW_LABEL, reinterpret_cast<void*>(HookDrawLabel), reinterpret_cast<void**>(&o_drawLabel), "CPlayerTags::DrawLabel"},
        {FN_DRAW_HEALTH_BAR, reinterpret_cast<void*>(HookDrawHealthBar), reinterpret_cast<void**>(&o_drawHealthBar),
         "CPlayerTags::DrawHealthBar"},
        {FN_LABEL_POOL_DRAW, reinterpret_cast<void*>(HookLabelPoolDraw), reinterpret_cast<void**>(&o_labelPoolDraw),
         "CLabelPool::Draw"},
    };
    for (const Target& t : targets) {
        void* at = reinterpret_cast<void*>(base + t.offset);
        MH_STATUS created = MH_CreateHook(at, t.hook, t.original);
        MH_STATUS enabled = created == MH_OK ? MH_EnableHook(at) : created;
        if (enabled != MH_OK) {
            TagsLog("gancho %s (samp.dll+0x%X) recusado (%d) -- desligando o Trok Tags", t.name, t.offset, enabled);
            for (const Target& u : targets) {
                MH_RemoveHook(reinterpret_cast<void*>(base + u.offset));
            }
            g_samp = 0;
            return false;
        }
    }
    TagsLog("ganchos do samp.dll instalados (DrawLabel, DrawHealthBar, CLabelPool::Draw)");
    return true;
}

void Keep(bool nametags, bool fixed, bool bubbles) {
    g_takeTags = nametags;
    g_takeFixed = fixed;
    g_takeBubbles = bubbles;
    g_keepUntil = GetTickCount() + 300;
}

void BeginFrame() {
    if (!g_samp) {
        return;
    }
    if (!(g_takeBubbles && Active())) {
        GiveBackBubbles();
        return;
    }
    DWORD bubbles = At<DWORD>(g_samp, CHAT_BUBBLE);
    if (!BubblesUsable(bubbles)) {
        return;
    }
    DWORD players = Pool(PL_PLAYER);
    if (!players) {
        return;
    }
    DWORD tick = GetTickCount(), time = timeGetTime();
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        DWORD e = bubbles + i * BUBBLE_SIZE;
        if (At<int>(e, 0) != 1 || !Info(players, i)) {
            continue;
        }
        const char* text = reinterpret_cast<const char*>(e + BB_TEXT);
        size_t len = strnlen(text, 256);
        DWORD created = At<DWORD>(e, BB_CREATED);
        int life = At<int>(e, BB_LIFE);
        float dist = At<float>(e, BB_DIST);
        if (len == 0 || len >= 256 || life <= 0 || life > 600000 || !(dist > 0.0f && dist < 10000.0f)) {
            continue; // nao parece um balao: fica com o SA-MP
        }
        bool timeClock;
        if (tick - created <= static_cast<DWORD>(life) + 2000) {
            timeClock = false;
        } else if (time - created <= static_cast<DWORD>(life) + 2000) {
            timeClock = true;
        } else {
            continue;
        }
        g_bubbles[i] = {true, Utf8(text, static_cast<int>(len)), At<DWORD>(e, BB_COLOR), dist, created, life, timeClock};
        *reinterpret_cast<int*>(e) = 0; // o SA-MP nao desenha mais este; o nosso desenha
    }
}

void Collect(std::vector<tags::Stack>& out, float screenW, float screenH, float userDistance) {
    out.clear();
    DWORD ng = NetGame();
    if (!ng || At<int>(ng, NG_STATE) != GAME_CONNECTED) {
        return;
    }
    DWORD players = Pool(PL_PLAYER);
    if (!players) {
        return;
    }
    DWORD settings = At<DWORD>(ng, NG_SETTINGS);
    float serverDist = Ptr(settings) ? At<float>(settings, ST_NAMETAG_DIST) : 70.0f;
    if (!(serverDist > 0.0f && serverDist < 10000.0f)) {
        serverDist = 70.0f;
    }
    bool serverLos = Ptr(settings) && At<BYTE>(settings, ST_NAMETAG_LOS) != 0;
    float limit = userDistance > 0.0f ? std::fmin(userDistance, serverDist) : serverDist;
    Vec3 cam = CameraPos();
    DWORD now = GetTickCount();

    // Textos 3D presos a jogadores: uma passada pelos 2048.
    struct Attached {
        int player;
        DWORD label;
    };
    static std::vector<Attached> attached;
    attached.clear();
    DWORD labels = tags::cfg.enabled && tags::cfg.fixed ? Pool(PL_LABEL) : 0;
    if (labels) {
        for (int i = 0; i < MAX_LABELS; ++i) {
            if (!At<BOOL>(labels, LB_NOT_EMPTY + i * 4)) {
                continue;
            }
            DWORD label = labels + i * LABEL_SIZE;
            WORD player = At<WORD>(label, LB_PLAYER);
            if (player != INVALID_ID && player < MAX_PLAYERS && Ptr(At<DWORD>(label, LB_TEXT))) {
                attached.push_back({player, label});
            }
        }
    }

    for (int id = 0; id < MAX_PLAYERS; ++id) {
        DWORD info = Info(players, id);
        DWORD remote = info ? RemoteOf(info) : 0;
        DWORD ped = remote ? PedOf(remote) : 0;
        if (!info) {
            g_bubbles[id].active = false; // o ID pode ir para outro jogador
        }
        if (!ped) {
            g_afkSince[id] = 0;
            continue;
        }
        bool paused = At<int>(remote, RP_STATUS) == STATUS_PAUSED;
        if (!paused) {
            g_afkSince[id] = 0;
        } else if (!g_afkSince[id]) {
            g_afkSince[id] = now;
        }

        bool tagSeen = g_takeTags && g_seen[id].frame == g_frame;
        bool anyLabel = false;
        for (const Attached& a : attached) {
            if (a.player == id) {
                anyLabel = true;
                break;
            }
        }
        StoredBubble& bubble = g_bubbles[id];
        if (bubble.active && Expired(bubble)) {
            bubble.active = false;
        }
        if (!tagSeen && !anyLabel && !bubble.active) {
            continue;
        }

        Vec3 head = HeadPos(ped);
        Vec3 anchor = {head.x, head.y, head.z + ANCHOR_ABOVE_HEAD};
        tags::Stack st;
        if (!Project(anchor, &st.at) || st.at.x < -300 || st.at.x > screenW + 300 || st.at.y < -300 || st.at.y > screenH + 300) {
            continue;
        }
        float dist = Dist(cam, head);
        st.distance = tagSeen ? g_seen[id].dist : dist;
        st.limit = limit;

        if (tagSeen && st.distance <= limit) {
            st.hasTag = true;
            tags::Nametag& tag = st.tag;
            tag.name = NameOf(info);
            tag.id = id;
            tag.color = FromArgb(g_seen[id].color, true);
            tag.health = At<float>(remote, RP_HEALTH);
            tag.armour = At<float>(remote, RP_ARMOUR);
            tag.afk = paused;
            tag.afkSeconds = paused ? (now - g_afkSince[id]) / 1000.0f : -1.0f;
            tag.ping = static_cast<int>(At<DWORD>(info, PI_PING));
        }

        if (anyLabel) {
            Vec3 body = PedPos(ped);
            struct Item {
                float z;
                tags::FixedText text;
            };
            Item items[8];
            int count = 0;
            for (const Attached& a : attached) {
                if (a.player != id || count == 8) {
                    continue;
                }
                Vec3 offset = At<Vec3>(a.label, LB_POS);
                Vec3 at = {body.x + offset.x, body.y + offset.y, body.z + offset.z};
                if (Dist(cam, at) > At<float>(a.label, LB_DIST)) {
                    continue;
                }
                if (At<BYTE>(a.label, LB_LOS) && !NoObstacles(at)) {
                    continue;
                }
                const char* text = At<const char*>(a.label, LB_TEXT);
                DWORD color = At<DWORD>(a.label, LB_COLOR);
                if (!(color >> 24)) {
                    continue; // texto transparente: o SA-MP tambem nao mostra
                }
                items[count].z = offset.z;
                items[count].text.text = Utf8(text, static_cast<int>(strnlen(text, 1024)));
                items[count].text.color = FromArgb(color, true);
                ++count;
            }
            // O mais alto (maior altura que o servidor deu) fica em cima.
            for (int i = 1; i < count; ++i) {
                for (int j = i; j > 0 && items[j].z > items[j - 1].z; --j) {
                    std::swap(items[j], items[j - 1]);
                }
            }
            for (int i = 0; i < count; ++i) {
                st.fixed.push_back(items[i].text);
            }
        }

        if (bubble.active && dist <= bubble.dist && (!serverLos || NoObstacles(head))) {
            bool typing = tags::IsTypingText(bubble.text.c_str());
            if (typing && st.hasTag && tags::cfg.typing) {
                st.tag.typing = true;
            } else {
                st.hasBubble = true;
                st.bubble.text = bubble.text;
                st.bubble.color = FromArgb(bubble.color, true);
                st.bubble.typing = typing;
                DWORD clock = bubble.timeClock ? timeGetTime() : now;
                float left = (bubble.life - static_cast<int>(clock - bubble.created)) / 1000.0f;
                st.bubble.alpha = left < 0.4f ? std::fmax(0.0f, left / 0.4f) : 1.0f;
            }
        }

        if (st.hasTag || !st.fixed.empty() || st.hasBubble) {
            out.push_back(st);
        }
    }

    // Ping: o SA-MP so atualiza pontos e pings quando o Tab pede. Com o indicador ligado, pede a cada 10 s se
    // ninguem pediu (o mesmo pedido do Tab). O tique do ultimo pedido tem que parecer um tique de verdade.
    if (tags::cfg.enabled && tags::cfg.ping) {
        DWORD last = At<DWORD>(g_samp, LAST_PLAYERS_UPDATE);
        if (now - last < 86400000u && now - last > 10000 && now - g_pingRequest > 10000) {
            g_pingRequest = now;
            reinterpret_cast<UpdatePlayersFn>(g_samp + FN_UPDATE_PLAYERS)(reinterpret_cast<void*>(ng));
        }
    }
}

void EndFrame() {
    ++g_frame;
}

ServerInfo Server() {
    ServerInfo s;
    DWORD ng = NetGame();
    if (!ng) {
        return s;
    }
    s.connected = At<int>(ng, NG_STATE) == GAME_CONNECTED;
    const char* host = reinterpret_cast<const char*>(ng + NG_HOST);
    snprintf(s.address, sizeof(s.address), "%.*s:%d", 256, host, At<int>(ng, NG_PORT));
    DWORD settings = At<DWORD>(ng, NG_SETTINGS);
    if (Ptr(settings)) {
        s.nameTags = At<BYTE>(settings, ST_NAMETAGS) != 0;
        s.nameTagDistance = At<float>(settings, ST_NAMETAG_DIST);
        s.nameTagLos = At<BYTE>(settings, ST_NAMETAG_LOS) != 0;
    }
    s.bubbles = g_bubbleState;
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

} // namespace samp
