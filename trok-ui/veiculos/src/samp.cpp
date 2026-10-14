// Trok Veiculos -- samp.dll 0.3.7 R1 e gta_sa.exe 1.0 US.
//
// Enderecos e estruturas do samp.dll conferidos com a SAMP-API (MIT, LUCHARE / BlastHack): os cabecalhos do R1
// foram compilados e os deslocamentos impressos (CVehiclePool: veiculos em +0x1134, ocupados em +0x3074, os do GTA
// em +0x4FB4). Do GTA, so o que o proprio samp.dll le: vida em +0x4C0, pneus em +0x5A5 (carro) e +0x65C (moto),
// o tipo pela vtable. O alcance de 20 m e a mesma conta do /dl (CEntity::GetDistanceToLocalPlayer).

#include "samp.h"

#include "config.h"
#include "names.h"

#include <cmath>
#include <cstdio>
#include <cstring>

void VeicLog(const char* fmt, ...); // main.cpp

namespace samp {
namespace {

// ---------------------------------------------------------------- samp.dll 0.3.7 R1

constexpr DWORD NETGAME = 0x21A0F8;      // CNetGame*
constexpr DWORD DIALOG = 0x21A0B8;       // CDialog*
constexpr DWORD SCOREBOARD = 0x21A0B4;   // CScoreboard* (m_bIsEnabled no comeco)
constexpr DWORD LABEL_DRAWER = 0x21A0C0; // CLabel* (o teste de parede dos textos 3D)
constexpr DWORD GAME = 0x21A10C;         // CGame*
constexpr DWORD FN_DISTANCE_TO_LOCAL = 0x9A700; // CEntity::GetDistanceToLocalPlayer (a conta do /dl)
constexpr DWORD FN_IS_OCCUPIED = 0xB0BE0;       // CVehicle::IsOccupied
constexpr DWORD FN_IS_WRECKED = 0xB0E80;        // CVehicle::IsWrecked
constexpr DWORD FN_MAX_PASSENGERS = 0xB1DE0;    // CVehicle::GetMaxPassengers (o PassengerSeats do /dl)
constexpr DWORD FN_HAS_NO_OBSTACLES = 0x67530;  // CLabel::HasNoObstacles (camera ate o ponto; so predios e objetos)
constexpr DWORD FN_PED_IN_VEHICLE = 0xA6730;    // CPed::IsInVehicle
constexpr DWORD FN_PED_VEHICLE = 0xA68C0;       // CPed::GetVehicle (o do GTA)

constexpr DWORD NG_HOST = 0x20, NG_PORT = 0x225, NG_STATE = 0x3BD, NG_POOLS = 0x3CD;
constexpr int GAME_CONNECTED = 14;
constexpr DWORD PL_VEHICLE = 0x1C;                                    // CNetGame::Pools
constexpr DWORD VP_OBJECT = 0x1134, VP_NOT_EMPTY = 0x3074, VP_GAME = 0x4FB4; // CVehiclePool
constexpr int MAX_VEHICLES = 2000;
constexpr DWORD SV_GAME_VEHICLE = 0x4C, SV_LOCKED = 0x60; // CVehicle do SA-MP
constexpr DWORD GM_PLAYER_PED = 0x08;                     // CGame::m_pPlayerPed
constexpr DWORD SP_GAME_PED = 0x2A4;                      // CPed do SA-MP -> CPed do GTA
constexpr DWORD DIALOG_ACTIVE = 0x28;

// ---------------------------------------------------------------- gta_sa.exe 1.0 US

constexpr DWORD GTA_CALC_SCREEN_COORS = 0x70CE30; // CSprite::CalcScreenCoors
constexpr DWORD GTA_MODEL_INFO = 0xA9B0C8;        // CModelInfo::ms_modelInfoPtrs
constexpr DWORD MI_COL_MODEL = 0x14;              // CBaseModelInfo::m_pColModel (caixa: min em +0, max em +0xC)
constexpr DWORD ENT_MATRIX = 0x14, ENT_MODEL = 0x22;
constexpr DWORD VEH_HEALTH = 0x4C0;
constexpr DWORD VT_AUTOMOBILE = 0x871120, VT_BIKE = 0x871360; // as vtables que o samp.dll compara
constexpr DWORD AUTO_WHEELS = 0x5A5;                          // CDamageManager: FE, TE, FD, TD (0 inteiro)
constexpr DWORD BIKE_WHEELS = 0x65C;                          // frente, tras
constexpr DWORD PED_WEAPONS = 0x5A0, PED_WEAPON_SLOT = 0x718, WEAPON_SIZE = 0x1C, WEAPON_STATE = 0x04;
constexpr int WEAPON_FIRING = 1;

// Tolerancias da deducao do "dano seu": a vida dos outros chega pelo sync, um pouco depois do tiro ou da batida.
constexpr float SHOT_WINDOW = 1.5f, TOUCH_WINDOW = 0.8f;

struct Vec3 {
    float x, y, z;
};

typedef float(__thiscall* DistanceFn)(void*);
typedef BOOL(__thiscall* BoolFn)(void*);
typedef char(__thiscall* CharFn)(void*);
typedef void*(__thiscall* PtrFn)(void*);
typedef BOOL(__thiscall* HasNoObstaclesFn)(void*, Vec3);
typedef bool(__cdecl* CalcScreenCoorsFn)(const Vec3&, Vec3*, float*, float*, bool, bool);

DWORD g_samp = 0;

// O que o mod lembra de cada veiculo (pelo ID do SA-MP).
struct Track {
    DWORD game = 0;          // o veiculo do GTA nesse ID (outro ponteiro: outro veiculo, comeca de novo)
    float health = 0.0f;
    float maxHealth = 0.0f;  // maior vida vista (o teto da blindagem "maior visto")
    float ghost = 0.0f;      // vida antes do dano que esta escorrendo
    float ghostAt = -100.0f;
    int damage = 0;          // o seu dano somado (os acertos em sequencia somam)
    float damageAt = -100.0f;
    float aimedFireAt = -100.0f; // ultimo instante em que voce atirava com a mira nele
    float touchAt = -100.0f;     // ultimo instante em que o seu veiculo encostava nele
    float mineUntil = -100.0f;   // "ao dar dano": ate quando a placa aparece
    float wallAt = -100.0f;      // o teste de parede vale por 100 ms
    bool visible = true;
};
Track g_track[MAX_VEHICLES];

template <class T>
T At(DWORD base, DWORD offset) {
    return *reinterpret_cast<T*>(base + offset);
}

bool Ptr(DWORD p) {
    return p >= 0x10000;
}

bool Finite(float v) {
    return v > -1e7f && v < 1e7f;
}

DWORD NetGame() {
    if (!g_samp) {
        return 0;
    }
    DWORD ng = At<DWORD>(g_samp, NETGAME);
    return Ptr(ng) ? ng : 0;
}

DWORD VehiclePool() {
    DWORD ng = NetGame();
    if (!ng) {
        return 0;
    }
    DWORD pools = At<DWORD>(ng, NG_POOLS);
    if (!Ptr(pools)) {
        return 0;
    }
    DWORD pool = At<DWORD>(pools, PL_VEHICLE);
    return Ptr(pool) ? pool : 0;
}

// O personagem local (o CPed do SA-MP).
DWORD LocalPed() {
    DWORD game = At<DWORD>(g_samp, GAME);
    if (!Ptr(game)) {
        return 0;
    }
    DWORD ped = At<DWORD>(game, GM_PLAYER_PED);
    return Ptr(ped) ? ped : 0;
}

// O veiculo do GTA em que voce esta (dirigindo ou de carona), ou 0.
DWORD LocalVehicle(DWORD ped) {
    if (!ped || !reinterpret_cast<BoolFn>(g_samp + FN_PED_IN_VEHICLE)(reinterpret_cast<void*>(ped))) {
        return 0;
    }
    DWORD vehicle = reinterpret_cast<DWORD>(reinterpret_cast<PtrFn>(g_samp + FN_PED_VEHICLE)(reinterpret_cast<void*>(ped)));
    return Ptr(vehicle) ? vehicle : 0;
}

// Voce esta atirando? (a arma na mao do personagem do GTA no estado "atirando")
bool Firing(DWORD ped) {
    DWORD gamePed = ped ? At<DWORD>(ped, SP_GAME_PED) : 0;
    if (!Ptr(gamePed)) {
        return false;
    }
    BYTE slot = At<BYTE>(gamePed, PED_WEAPON_SLOT);
    if (slot > 12) {
        return false;
    }
    return At<int>(gamePed, PED_WEAPONS + slot * WEAPON_SIZE + WEAPON_STATE) == WEAPON_FIRING;
}

struct Box {
    Vec3 min, max;
};

// A caixa do modelo (do arquivo de colisao), em metros, no espaco do veiculo.
bool ModelBox(DWORD vehicle, Box* box) {
    WORD model = At<WORD>(vehicle, ENT_MODEL);
    if (model < 400 || model > 611) {
        return false;
    }
    DWORD info = At<DWORD>(GTA_MODEL_INFO, model * 4);
    DWORD col = Ptr(info) ? At<DWORD>(info, MI_COL_MODEL) : 0;
    if (!Ptr(col)) {
        return false;
    }
    *box = {At<Vec3>(col, 0x00), At<Vec3>(col, 0x0C)};
    const float* v = &box->min.x;
    for (int i = 0; i < 6; ++i) {
        if (!(v[i] > -60.0f && v[i] < 60.0f)) {
            return false;
        }
    }
    return box->min.x < box->max.x && box->min.y < box->max.y && box->min.z < box->max.z;
}

struct Pose {
    Vec3 right, forward, up, pos;
};

bool VehiclePose(DWORD vehicle, Pose* pose) {
    DWORD matrix = At<DWORD>(vehicle, ENT_MATRIX);
    if (!Ptr(matrix)) {
        return false;
    }
    pose->right = At<Vec3>(matrix, 0x00);
    pose->forward = At<Vec3>(matrix, 0x10);
    pose->up = At<Vec3>(matrix, 0x20);
    pose->pos = At<Vec3>(matrix, 0x30);
    return Finite(pose->pos.x) && Finite(pose->pos.y) && Finite(pose->pos.z);
}

Vec3 World(const Pose& p, float x, float y, float z) {
    return {p.pos.x + p.right.x * x + p.forward.x * y + p.up.x * z, p.pos.y + p.right.y * x + p.forward.y * y + p.up.y * z,
            p.pos.z + p.right.z * x + p.forward.z * y + p.up.z * z};
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

// O retangulo do veiculo na tela: os 8 cantos da caixa projetados (cantos atras da camera ficam de fora).
bool ScreenRect(const Pose& pose, const Box& box, ImVec2* lo, ImVec2* hi) {
    int seen = 0;
    for (int i = 0; i < 8; ++i) {
        Vec3 corner = World(pose, i & 1 ? box.max.x : box.min.x, i & 2 ? box.max.y : box.min.y, i & 4 ? box.max.z : box.min.z);
        ImVec2 p;
        if (!Project(corner, &p)) {
            continue;
        }
        if (seen++ == 0) {
            *lo = *hi = p;
        } else {
            lo->x = std::fmin(lo->x, p.x);
            lo->y = std::fmin(lo->y, p.y);
            hi->x = std::fmax(hi->x, p.x);
            hi->y = std::fmax(hi->y, p.y);
        }
    }
    return seen >= 4;
}

float Dist(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float Radius(const Box& b) {
    float dx = b.max.x - b.min.x, dy = b.max.y - b.min.y;
    return 0.5f * std::sqrt(dx * dx + dy * dy);
}

bool NoObstacles(const Vec3& pos) {
    void* drawer = At<void*>(g_samp, LABEL_DRAWER);
    if (!Ptr(reinterpret_cast<DWORD>(drawer))) {
        return true;
    }
    return reinterpret_cast<HasNoObstaclesFn>(g_samp + FN_HAS_NO_OBSTACLES)(drawer, pos) != FALSE;
}

// Da para ver o veiculo? O teto ou o meio dele sem predio nem objeto no caminho (o mesmo teste dos textos 3D).
bool Visible(const Pose& pose, const Box& box) {
    float cx = (box.min.x + box.max.x) * 0.5f, cy = (box.min.y + box.max.y) * 0.5f;
    return NoObstacles(World(pose, cx, cy, box.max.z + 0.1f)) || NoObstacles(World(pose, cx, cy, (box.min.z + box.max.z) * 0.5f));
}

int KindOf(DWORD vehicle) {
    DWORD vtable = At<DWORD>(vehicle, 0);
    return vtable == VT_AUTOMOBILE ? veic::KIND_CAR : vtable == VT_BIKE ? veic::KIND_BIKE : veic::KIND_OTHER;
}

// Pneus furados no formato do SA-MP (o mesmo que o samp.dll monta lendo o GTA).
unsigned FlatTires(DWORD vehicle, int kind) {
    unsigned bits = 0;
    if (kind == veic::KIND_CAR) {
        const unsigned order[4] = {veic::TIRE_FL, veic::TIRE_RL, veic::TIRE_FR, veic::TIRE_RR};
        for (int i = 0; i < 4; ++i) {
            if (At<BYTE>(vehicle, AUTO_WHEELS + i)) {
                bits |= order[i];
            }
        }
    } else if (kind == veic::KIND_BIKE) {
        if (At<BYTE>(vehicle, BIKE_WHEELS)) {
            bits |= veic::TIRE_FRONT;
        }
        if (At<BYTE>(vehicle, BIKE_WHEELS + 1)) {
            bits |= veic::TIRE_REAR;
        }
    }
    return bits;
}

// A mira esta em cima do veiculo? (o centro da tela ou a mira de terceira pessoa do GTA, com folga)
bool Aimed(const ImVec2& lo, const ImVec2& hi, float screenW, float screenH) {
    float pad = screenW * 0.06f;
    const ImVec2 cross[2] = {ImVec2(screenW * 0.5f, screenH * 0.5f), ImVec2(screenW * 0.53f, screenH * 0.4f)};
    for (const ImVec2& c : cross) {
        if (c.x >= lo.x - pad && c.x <= hi.x + pad && c.y >= lo.y - pad && c.y <= hi.y + pad) {
            return true;
        }
    }
    return false;
}

} // namespace

void Install(DWORD base) {
    g_samp = base;
    for (Track& t : g_track) {
        t = Track();
    }
}

void Collect(std::vector<veic::Card>& out, float screenW, float screenH, float now) {
    out.clear();
    DWORD ng = NetGame();
    if (!ng || At<int>(ng, NG_STATE) != GAME_CONNECTED) {
        return;
    }
    DWORD pool = VehiclePool();
    if (!pool) {
        return;
    }
    using veic::cfg;
    float limit = cfg.distance > 0 ? std::fmin(static_cast<float>(cfg.distance), DL_RANGE) : DL_RANGE;
    DWORD ped = LocalPed();
    DWORD mine = LocalVehicle(ped);
    bool firing = Firing(ped);
    Pose minePose;
    Box mineBox;
    bool mineGeom = mine && VehiclePose(mine, &minePose) && ModelBox(mine, &mineBox);

    for (int id = 0; id < MAX_VEHICLES; ++id) {
        Track& t = g_track[id];
        DWORD sv = At<BOOL>(pool, VP_NOT_EMPTY + id * 4) ? At<DWORD>(pool, VP_OBJECT + id * 4) : 0;
        DWORD gv = Ptr(sv) ? At<DWORD>(pool, VP_GAME + id * 4) : 0;
        if (!Ptr(sv) || !Ptr(gv) || At<DWORD>(sv, SV_GAME_VEHICLE) != gv) {
            t = Track();
            continue;
        }
        float health = At<float>(gv, VEH_HEALTH);
        if (!Finite(health)) {
            continue;
        }
        if (t.game != gv) {
            t = Track();
            t.game = gv;
            t.health = t.maxHealth = health;
        }
        float before = t.health;
        t.health = health;
        t.maxHealth = std::fmax(t.maxHealth, health);
        bool hit = before - health > 0.5f;
        if (hit) {
            // Varios acertos seguidos: a parte branca comeca da vida de antes do primeiro.
            if (now - t.ghostAt > 0.75f || t.ghost <= health) {
                t.ghost = before;
            }
            t.ghostAt = now;
        }

        if (reinterpret_cast<BoolFn>(g_samp + FN_IS_WRECKED)(reinterpret_cast<void*>(sv))) {
            continue;
        }
        float distance = reinterpret_cast<DistanceFn>(g_samp + FN_DISTANCE_TO_LOCAL)(reinterpret_cast<void*>(sv));
        if (!(distance >= 0.0f && distance <= limit)) {
            continue;
        }
        Pose pose;
        Box box;
        ImVec2 lo, hi;
        if (!VehiclePose(gv, &pose) || !ModelBox(gv, &box) || !ScreenRect(pose, box, &lo, &hi)) {
            continue;
        }

        // O seu dano: tiro com a mira nele ou o seu veiculo encostado nele, pouco antes da vida cair.
        if (firing && Aimed(lo, hi, screenW, screenH)) {
            t.aimedFireAt = now;
        }
        if (mineGeom && gv != mine && Dist(minePose.pos, pose.pos) < Radius(mineBox) + Radius(box) + 0.3f) {
            t.touchAt = now;
        }
        if (hit && gv != mine && (now - t.aimedFireAt < SHOT_WINDOW || now - t.touchAt < TOUCH_WINDOW)) {
            int dealt = static_cast<int>(before - health + 0.5f);
            t.damage = now - t.damageAt < 0.8f ? t.damage + dealt : dealt;
            t.damageAt = now;
            t.mineUntil = now + static_cast<float>(cfg.damageHold);
        }

        if (cfg.hideMine && gv == mine) {
            continue;
        }
        bool occupied = reinterpret_cast<BoolFn>(g_samp + FN_IS_OCCUPIED)(reinterpret_cast<void*>(sv)) != FALSE;
        if (!occupied && !cfg.empty) {
            continue;
        }
        float alpha = 1.0f;
        if (cfg.show == 1) {
            float left = t.mineUntil - now;
            if (left <= 0.0f) {
                continue;
            }
            alpha = left < 0.6f ? left / 0.6f : 1.0f;
        }
        if (hi.x < 0 || lo.x > screenW || hi.y < 0 || lo.y > screenH) {
            continue;
        }
        if (cfg.walls == 0) {
            if (now - t.wallAt > 0.1f || now < t.wallAt) {
                t.visible = Visible(pose, box);
                t.wallAt = now;
            }
            if (!t.visible) {
                continue;
            }
        }

        veic::Card c;
        c.at = ImVec2((lo.x + hi.x) * 0.5f, cfg.position == 1 ? lo.y : hi.y);
        c.distance = distance;
        c.limit = limit;
        WORD model = At<WORD>(gv, ENT_MODEL);
        const char* name = veic::ModelName(model);
        c.name = name ? name : "";
        c.id = id;
        c.model = model;
        c.seats = cfg.detailSeats ? reinterpret_cast<CharFn>(g_samp + FN_MAX_PASSENGERS)(reinterpret_cast<void*>(sv)) : 0;
        c.health = health;
        float top = cfg.armourCap == 1 ? static_cast<float>(cfg.armourFixed) : t.maxHealth - 1000.0f;
        c.armourMax = (health > 1000.0f || t.maxHealth > 1000.0f) && top > 0.0f ? std::fmax(top, health - 1000.0f) : 0.0f;
        c.kind = KindOf(gv);
        c.tires = FlatTires(gv, c.kind);
        c.locked = At<BOOL>(sv, SV_LOCKED) != FALSE;
        c.empty = !occupied;
        if (t.ghost > health && now - t.ghostAt < 0.75f) {
            c.ghost = t.ghost;
            c.ghostAge = now - t.ghostAt;
        }
        if (t.damage > 0 && now - t.damageAt < 1.0f) {
            c.damage = t.damage;
            c.damageAge = now - t.damageAt;
        }
        c.alpha = alpha;
        out.push_back(c);
    }
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
