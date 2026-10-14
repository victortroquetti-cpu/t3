// Trok Hitmarker (.asi) -- Victor_Trok
// gta_sa.exe 1.0 US. Enderecos do gta-reversed e do plugin-sdk (os mesmos dos mods da casa):
//   CHud::DrawCrossHairs 0x58E020: a mira redonda sao 4 quartos do sprite "site M16" (CSprite2d::Draw 0x728350) em
//     volta de (largura x CHairMultX, altura x CHairMultY); com o raio no minimo (0.2) vem tambem um ponto
//     (CSprite2d::DrawRect 0x727B60). Luneta, bazuca e camera vao por outro caminho (CSprite::RenderOneXLUSprite).
//   CPlayerPed::GetWeaponRadiusOnScreen 0x609CD0: so o HUD usa; o tamanho cresce com CPlayerData::m_fAttackButtonCounter
//     (+0x2C), entao zerar o contador so durante o desenho deixa a mira parada sem mexer no espalhamento dos tiros.
//   CPed::GetBonePosition 0x5E4280, CSprite::CalcScreenCoors 0x70CE30, TheCamera 0xB6F028.

#include "game.h"

#include "MinHook.h"

#include <cmath>
#include <cstring>

void HitLog(const char* fmt, ...); // main.cpp

namespace game {
namespace {

constexpr DWORD GTA_VERSION_MARK = 0x82457C; // 0x94BF no 1.0 US
constexpr DWORD FN_DRAW_CROSSHAIRS = 0x58E020;
constexpr DWORD FN_SPRITE_DRAW = 0x728350; // CSprite2d::Draw(CRect const&, CRGBA const&)
constexpr DWORD FN_DRAW_RECT = 0x727B60;   // CSprite2d::DrawRect(CRect const&, CRGBA const&)
constexpr DWORD FN_WEAPON_RADIUS = 0x609CD0;
constexpr DWORD FN_BONE_POSITION = 0x5E4280;
constexpr DWORD FN_CALC_SCREEN = 0x70CE30;

constexpr DWORD PLAYER_PED = 0xB7CD98; // CWorld::Players[0].m_pPed
constexpr DWORD SCREEN_W = 0xC17044, SCREEN_H = 0xC17048;
constexpr DWORD CHAIR_MULT_Y = 0xB6EC10, CHAIR_MULT_X = 0xB6EC14;
constexpr DWORD MENU_ACTIVE = 0xBA67A4;
constexpr DWORD PREFS_SHOW_HUD = 0xBA6769; // FrontEndMenuManager.m_bPrefsShowHud
constexpr DWORD SCRIPT_DISPLAY_HUD = 0xA444A0;
constexpr DWORD CAMERA = 0xB6F028;
constexpr DWORD CAM_WIDESCREEN = 0x3D, CAM_ACTIVE = 0x59, CAM_LIST = 0x174, CAM_SIZE = 0x238, CAM_TARGET = 0x21C;
constexpr DWORD ENT_MATRIX = 0x14, ENT_PLACEMENT = 0x04, ENT_RWOBJECT = 0x18;
constexpr DWORD MAT_FORWARD = 0x10, MAT_POS = 0x30;
constexpr DWORD PED_PLAYER_DATA = 0x480, PED_FLAGS = 0x46C, PED_STATE = 0x530, PED_HEALTH = 0x540, PED_ARMOUR = 0x548;
constexpr DWORD PED_VEHICLE = 0x58C, PED_WEAPONS = 0x5A0, PED_WEAPON_SLOT = 0x718, WEAPON_SIZE = 0x1C;
constexpr DWORD PD_ATTACK_COUNTER = 0x2C;
constexpr int PEDSTATE_DIE = 54, PEDSTATE_DEAD = 55;

struct Rect {
    float x0, y0, x1, y1; // como o CRect do GTA: esquerda, "baixo", direita, "cima" (y cresce para baixo na tela)
};
struct Rgba {
    BYTE r, g, b, a;
};

typedef void(__cdecl* VoidFn)();
typedef void(__thiscall* SpriteDrawFn)(void*, const Rect&, const Rgba&);
typedef void(__cdecl* DrawRectFn)(const Rect&, const Rgba&);
typedef float(__thiscall* RadiusFn)(void*);
typedef void(__thiscall* BoneFn)(void*, Vec3&, unsigned, bool);
typedef bool(__cdecl* CalcScreenFn)(const Vec3&, Vec3*, float*, float*, bool, bool);

VoidFn o_crosshairs = nullptr;
SpriteDrawFn o_spriteDraw = nullptr;
DrawRectFn o_drawRect = nullptr;
RadiusFn o_radius = nullptr;

// O que o Present pediu (lido dentro dos ganchos, na mesma thread).
bool g_hide = false, g_fixed = false;
float g_size = 1.0f;
DWORD g_asked = 0; // GetTickCount do ultimo pedido

// Dentro de um CHud::DrawCrossHairs.
bool g_inside = false;
bool g_hideNow = false;  // esta chamada: a mira do jogo sai da tela
bool g_haveCenter = false;
float g_cx = 0, g_cy = 0; // centro desta chamada (o canto do primeiro quarto, ou o meio do ponto)
float g_minX, g_minY, g_maxX, g_maxY;
bool g_any = false;

Aim g_aim; // o que os ganchos viram desde o ultimo TakeAim

template <class T>
T At(DWORD base, DWORD offset) {
    return *reinterpret_cast<T*>(base + offset);
}

bool Ptr(DWORD p) {
    return p >= 0x10000;
}

bool Readable(DWORD address, size_t size) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
        return false;
    }
    return address + size <= reinterpret_cast<DWORD>(mbi.BaseAddress) + mbi.RegionSize;
}

int ActiveWeapon(DWORD ped) {
    BYTE slot = At<BYTE>(ped, PED_WEAPON_SLOT);
    return slot < 13 ? At<int>(ped, PED_WEAPONS + slot * WEAPON_SIZE) : 0;
}

bool InVehicle(DWORD ped) {
    return ((At<DWORD>(ped, PED_FLAGS) >> 8) & 1) && Ptr(At<DWORD>(ped, PED_VEHICLE));
}

void Grow(const Rect& r) {
    float xs[2] = {r.x0, r.x1}, ys[2] = {r.y0, r.y1};
    for (int i = 0; i < 2; ++i) {
        if (!g_any || xs[i] < g_minX) g_minX = xs[i];
        if (!g_any || xs[i] > g_maxX) g_maxX = xs[i];
        if (!g_any || ys[i] < g_minY) g_minY = ys[i];
        if (!g_any || ys[i] > g_maxY) g_maxY = ys[i];
        g_any = true;
    }
}

Rect Scaled(const Rect& r) {
    if (g_size == 1.0f || !g_haveCenter) {
        return r;
    }
    return {g_cx + (r.x0 - g_cx) * g_size, g_cy + (r.y0 - g_cy) * g_size, g_cx + (r.x1 - g_cx) * g_size,
            g_cy + (r.y1 - g_cy) * g_size};
}

void __cdecl HookCrosshairs() {
    DWORD ped = At<DWORD>(PLAYER_PED, 0);
    bool fresh = GetTickCount() - g_asked < 500; // o overlay esta vivo
    g_inside = true;
    g_hideNow = fresh && g_hide && Ptr(ped) && !InVehicle(ped) && !ExcludedWeapon(ActiveWeapon(ped));
    g_haveCenter = false;
    g_any = false;
    o_crosshairs();
    g_inside = false;
    if (g_any) {
        g_aim.drawn = true;
        g_aim.center = ImVec2((g_minX + g_maxX) * 0.5f, (g_minY + g_maxY) * 0.5f);
    }
}

// Os quartos da mira redonda: o primeiro (em cima a esquerda) termina exatamente no centro.
void __fastcall HookSpriteDraw(void* self, void*, const Rect& rect, const Rgba& color) {
    if (!g_inside) {
        o_spriteDraw(self, rect, color);
        return;
    }
    if (!g_haveCenter) {
        g_haveCenter = true;
        g_cx = rect.x1;
        g_cy = rect.y1;
    }
    Grow(rect);
    if (!g_hideNow) {
        o_spriteDraw(self, Scaled(rect), color);
    }
}

void __cdecl HookDrawRect(const Rect& rect, const Rgba& color) {
    if (!g_inside) {
        o_drawRect(rect, color);
        return;
    }
    if (!g_haveCenter) { // o ponto vem antes dos quartos: o meio dele e o centro
        g_haveCenter = true;
        g_cx = (rect.x0 + rect.x1) * 0.5f;
        g_cy = (rect.y0 + rect.y1) * 0.5f;
    }
    Grow(rect);
    if (!g_hideNow) {
        o_drawRect(Scaled(rect), color);
    }
}

// Mira estatica: durante o desenho, o raio e o de quem nao esta atirando.
float __fastcall HookRadius(void* self, void*) {
    DWORD data = g_inside && g_fixed ? At<DWORD>(reinterpret_cast<DWORD>(self), PED_PLAYER_DATA) : 0;
    if (!Ptr(data)) {
        return o_radius(self);
    }
    float* counter = reinterpret_cast<float*>(data + PD_ATTACK_COUNTER);
    float saved = *counter;
    *counter = 0.0f;
    float radius = o_radius(self);
    *counter = saved;
    return radius;
}

} // namespace

bool VersionOk() {
    return Readable(GTA_VERSION_MARK, 4) && At<DWORD>(GTA_VERSION_MARK, 0) == 0x94BF;
}

bool Install() {
    struct Target {
        DWORD at;
        void* hook;
        void** original;
        const char* name;
    } targets[] = {
        {FN_DRAW_CROSSHAIRS, reinterpret_cast<void*>(HookCrosshairs), reinterpret_cast<void**>(&o_crosshairs),
         "CHud::DrawCrossHairs"},
        {FN_SPRITE_DRAW, reinterpret_cast<void*>(HookSpriteDraw), reinterpret_cast<void**>(&o_spriteDraw),
         "CSprite2d::Draw"},
        {FN_DRAW_RECT, reinterpret_cast<void*>(HookDrawRect), reinterpret_cast<void**>(&o_drawRect),
         "CSprite2d::DrawRect"},
        {FN_WEAPON_RADIUS, reinterpret_cast<void*>(HookRadius), reinterpret_cast<void**>(&o_radius),
         "CPlayerPed::GetWeaponRadiusOnScreen"},
    };
    int made = 0;
    for (const Target& t : targets) {
        void* at = reinterpret_cast<void*>(t.at);
        MH_STATUS created = MH_CreateHook(at, t.hook, t.original);
        if (created != MH_OK) {
            HitLog("gancho %s (0x%06X) recusado (%d) -- a mira do jogo fica como esta e o sinal vai no lugar padrao",
                   t.name, t.at, created);
            for (int i = 0; i < made; ++i) {
                MH_RemoveHook(reinterpret_cast<void*>(targets[i].at));
            }
            return false;
        }
        ++made;
    }
    for (const Target& t : targets) {
        MH_STATUS enabled = MH_EnableHook(reinterpret_cast<void*>(t.at));
        if (enabled != MH_OK) {
            HitLog("gancho %s (0x%06X) nao ligou (%d) -- a mira do jogo fica como esta", t.name, t.at, enabled);
            for (const Target& u : targets) {
                MH_DisableHook(reinterpret_cast<void*>(u.at));
                MH_RemoveHook(reinterpret_cast<void*>(u.at));
            }
            return false;
        }
    }
    HitLog("ganchos da mira do GTA instalados (DrawCrossHairs, CSprite2d::Draw/DrawRect, raio da mira)");
    return true;
}

void SetCrosshair(bool hide, bool fixed, float size) {
    g_hide = hide;
    g_fixed = fixed;
    g_size = size;
    g_asked = GetTickCount();
}

Aim TakeAim() {
    Aim a = g_aim;
    g_aim = Aim();
    return a;
}

ImVec2 DefaultAim(ImVec2 screen) {
    float mx = At<float>(CHAIR_MULT_X, 0), my = At<float>(CHAIR_MULT_Y, 0);
    if (!(mx > 0.05f && mx < 0.95f)) mx = 0.53f;
    if (!(my > 0.05f && my < 0.95f)) my = 0.4f;
    return ImVec2(screen.x * mx, screen.y * my);
}

bool ExcludedWeapon(int weapon) {
    return weapon == 34 || weapon == 35 || weapon == 36 || weapon == 43;
}

Local LocalPlayer() {
    Local l;
    DWORD ped = At<DWORD>(PLAYER_PED, 0);
    if (!Ptr(ped)) {
        return l;
    }
    l.ped = ped;
    l.health = At<float>(ped, PED_HEALTH);
    l.armour = At<float>(ped, PED_ARMOUR);
    int state = At<int>(ped, PED_STATE);
    l.alive = l.health > 0.0f && state != PEDSTATE_DIE && state != PEDSTATE_DEAD;
    l.inVehicle = InVehicle(ped);
    l.weapon = ActiveWeapon(ped);
    PedPos(ped, &l.pos);
    BYTE active = At<BYTE>(CAMERA, CAM_ACTIVE);
    if (active < 3) {
        DWORD target = At<DWORD>(CAMERA, CAM_LIST + active * CAM_SIZE + CAM_TARGET);
        l.followed = target == ped || (l.inVehicle && target == At<DWORD>(ped, PED_VEHICLE));
    }
    return l;
}

bool PedPos(DWORD ped, Vec3* out) {
    if (!Ptr(ped)) {
        return false;
    }
    DWORD matrix = At<DWORD>(ped, ENT_MATRIX);
    *out = Ptr(matrix) ? At<Vec3>(matrix, MAT_POS) : At<Vec3>(ped, ENT_PLACEMENT);
    return true;
}

bool Project(const Vec3& world, ImVec2* out) {
    Vec3 screen;
    float w, h;
    if (!reinterpret_cast<CalcScreenFn>(FN_CALC_SCREEN)(world, &screen, &w, &h, false, false)) {
        return false;
    }
    *out = ImVec2(screen.x, screen.y);
    return true;
}

bool PartOnScreen(DWORD ped, int part, ImVec2* out) {
    // Partes do SA-MP -> ossos do GTA: tronco, virilha, braco e antebraco de cada lado, perna (joelho), cabeca.
    static const unsigned bones[] = {3, 1, 33, 23, 42, 52, 8};
    Vec3 body;
    if (!PedPos(ped, &body) || !Ptr(At<DWORD>(ped, ENT_RWOBJECT))) {
        return false;
    }
    Vec3 at = body;
    if (part >= 3 && part <= 9) {
        Vec3 bone = {0, 0, 0};
        reinterpret_cast<BoneFn>(FN_BONE_POSITION)(reinterpret_cast<void*>(ped), bone, bones[part - 3], false);
        float dx = bone.x - body.x, dy = bone.y - body.y, dz = bone.z - body.z;
        if (dx * dx + dy * dy + dz * dz < 9.0f) { // osso ja calculado (perto do corpo)
            at = bone;
        }
    }
    return Project(at, out);
}

float AngleFromCamera(const Vec3& from, const Vec3& target) {
    DWORD matrix = At<DWORD>(CAMERA, ENT_MATRIX);
    if (!Ptr(matrix)) {
        return 0.0f;
    }
    Vec3 f = At<Vec3>(matrix, MAT_FORWARD);
    float fl = std::sqrt(f.x * f.x + f.y * f.y);
    float dx = target.x - from.x, dy = target.y - from.y;
    if (fl < 1e-4f || dx * dx + dy * dy < 1e-4f) {
        return 0.0f;
    }
    float fx = f.x / fl, fy = f.y / fl;
    float ahead = dx * fx + dy * fy;
    float right = dx * fy - dy * fx; // (fy, -fx) e a direita de quem olha para (fx, fy) com o z para cima
    return std::atan2(right, ahead);
}

bool Paused() {
    return At<BYTE>(MENU_ACTIVE, 0) != 0;
}

bool HudHidden() {
    return At<BYTE>(CAMERA, CAM_WIDESCREEN) != 0 || At<BYTE>(PREFS_SHOW_HUD, 0) == 0 ||
           At<BYTE>(SCRIPT_DISPLAY_HUD, 0) == 0;
}

} // namespace game
