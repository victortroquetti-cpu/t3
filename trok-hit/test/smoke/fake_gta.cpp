// Smoke test do Trok Hitmarker fora do jogo (Wine 32 bits + Xvfb): um "GTA de mentira" que
//  - deixa os enderecos fixos do gta_sa.exe 1.0 US dentro da propria imagem (array gigante no .bss), com a marca de
//    versao do 1.0 US, CSprite::CalcScreenCoors e CPed::GetBonePosition de mentira (camera em (0, 0, 0.8) olhando para
//    +Y) e um CHud::DrawCrossHairs de mentira que desenha a mira redonda como o original: pede o raio ao
//    CPlayerPed::GetWeaponRadiusOnScreen e manda os 4 quartos ao CSprite2d::Draw (e o ponto ao DrawRect), todos pelos
//    enderecos do jogo, onde o .asi pode ter posto os ganchos dele;
//  - carrega o samp.dll falso (make_fake_samp.py) e o .asi antes de existir device, como o ASI Loader, e monta as
//    estruturas do SA-MP 0.3.7 R1 que o Trok Hitmarker le: CNetGame conectado em 127.0.0.1:7777, pools e 4 jogadores;
//  - faz o papel do SA-MP: chama o SendGiveDamage e o SendTakeDamage pelos enderecos do R1 (como quando um tiro
//    acerta), derruba a vida no "sync" logo depois, manda a mensagem de morte e confere o que aparece na tela.
//
// Uso: fake_gta.exe <asi> <cenario>
//   trok          padroes: a mira do Trok no lugar da do jogo, sinal de acerto, numeros, local do acerto, informer,
//                 seta e borda do dano levado, abate, dano sem queda de vida (~), menu do /trokhit
//   mira-do-jogo  Trok Hitmarker.ini com a mira do jogo, estatica e em 150%: os quartos chegam ao GTA no raio de quem nao
//                 esta atirando e 1,5x maiores
//   pasta-propria a pasta "Trok Hitmarker" ja existe com o que o usuario pos: acerto.mp3 e um acerto.png retangular
//                 (o resto vem de dentro do .asi); no meio do jogo uma mira.png nova e copiada por cima e vale na hora
//   desligado     ligado=0: a mira do jogo intacta e nada na tela
//   outra-versao  samp.dll com o ponto de entrada do R3: o .asi nao instala nada
//   gta-errado    sem a marca do 1.0 US: o .asi nao instala nada

#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// A imagem vai de 0x400000 ate ~0xD6x000 e cobre 0x58E020..0xC9C05C. Tem que ser o comeco do .bss: os outros buffers
// grandes vem do VirtualAlloc.
unsigned char g_gtaMemory[0x960000];

template <typename T> T& Mem(DWORD address) {
    return *reinterpret_cast<T*>(address);
}

template <typename T> void Put(void* base, DWORD offset, T value) {
    memcpy(static_cast<BYTE*>(base) + offset, &value, sizeof(T));
}

struct Vec3 {
    float x, y, z;
};
struct Rect {
    float x0, y0, x1, y1;
};

typedef void(__cdecl* CmdProc)(const char*);
typedef void(__cdecl* VoidFn)();
typedef float(__thiscall* RadiusFn)(void*);
typedef void(__thiscall* SpriteDrawFn)(void*, const Rect&, const BYTE*);
typedef void(__cdecl* DrawRectFn)(const Rect&, const BYTE*);
typedef void(__thiscall* DamageFn)(void*, int, float, int, int);
typedef void(__thiscall* DeathFn)(void*, const char*, const char*, DWORD, DWORD, char);

constexpr int W = 1600;
constexpr int H = 900;
constexpr D3DCOLOR CLEAR = D3DCOLOR_XRGB(20, 90, 40);
constexpr Vec3 CAMERA = {0.0f, 0.0f, 0.8f};
constexpr float FOCAL = W / 2; // 90 graus na horizontal

// gta_sa.exe 1.0 US
constexpr DWORD GTA_VERSION_MARK = 0x82457C;
constexpr DWORD GTA_DRAW_CROSSHAIRS = 0x58E020, GTA_SPRITE_DRAW = 0x728350, GTA_DRAW_RECT = 0x727B60;
constexpr DWORD GTA_WEAPON_RADIUS = 0x609CD0, GTA_CALC_SCREEN_COORS = 0x70CE30, GTA_GET_BONE_POSITION = 0x5E4280;
constexpr DWORD GTA_THE_CAMERA = 0xB6F028, GTA_PLAYER_PED = 0xB7CD98;
constexpr DWORD GTA_CHAIR_Y = 0xB6EC10, GTA_CHAIR_X = 0xB6EC14;

// samp.dll 0.3.7 R1 (os mesmos de src/samp.cpp)
constexpr DWORD NETGAME = 0x21A0F8, DIALOG = 0x21A0B8, SCOREBOARD = 0x21A0B4;
constexpr DWORD FN_SEND_GIVE = 0x6770, FN_SEND_TAKE = 0x6660, FN_DEATH_ADD = 0x66A10;

// Campos do samp.dll falso (make_fake_samp.py), relativos a samp.dll + 0x300000.
constexpr DWORD F_CMD_NAME = 0, F_CMD_PROC = 4, F_CHAT_COUNT = 8, F_CURSOR_MODE = 0xC, F_GIVE_COUNT = 0x10,
                F_GIVE_ID = 0x14, F_TAKE_COUNT = 0x18, F_TAKE_ID = 0x1C, F_DEATH_COUNT = 0x20;

int g_failures = 0;
IDirect3DDevice9* g_real = nullptr;
DWORD* g_shot = nullptr; // W * H
DWORD g_samp = 0;
char g_text[300];

DWORD& Field(DWORD offset) {
    return Mem<DWORD>(g_samp + 0x300000 + offset);
}

void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHA", what);
    fflush(stdout);
    if (!ok) {
        ++g_failures;
    }
}

// ---------------------------------------------------------------- GTA de mentira

struct GameMatrix {
    float m[16]; // CMatrix: direita, frente, cima, posicao (+0x30)
    BYTE pad[0x20];
};

struct GamePed {
    BYTE raw[0x800]; // CPlaceable: posicao em +0x04, CMatrix* em +0x14, RwObject em +0x18
};

GameMatrix g_cameraMatrix;
GamePed g_local;
GameMatrix g_localMatrix;
BYTE g_localData[0x100]; // CPlayerData: m_fAttackButtonCounter em +0x2C
BYTE g_vehicle[0x100];
BYTE g_sprite[0x10];

bool g_aiming = false;
int g_crosshairCalls = 0, g_spriteDraws = 0, g_rectDraws = 0, g_drawsThisCall = 0;
Rect g_firstRect = {};

Vec3 PedBody(void* ped) {
    GameMatrix* matrix = *reinterpret_cast<GameMatrix**>(static_cast<BYTE*>(ped) + 0x14);
    return {matrix->m[12], matrix->m[13], matrix->m[14]};
}

// CSprite::CalcScreenCoors: camera fixa olhando para +Y.
bool __cdecl FakeCalcScreenCoors(const Vec3& in, Vec3* out, float* w, float* h, bool, bool) {
    float depth = in.y - CAMERA.y;
    if (depth < 0.2f) {
        return false;
    }
    out->x = W / 2 + (in.x - CAMERA.x) / depth * FOCAL;
    out->y = H / 2 - (in.z - CAMERA.z) / depth * FOCAL;
    out->z = depth;
    *w = *h = FOCAL / depth;
    return true;
}

// CPed::GetBonePosition (thiscall): a cabeca (8) fica 0.65 m acima do corpo, o resto no corpo.
void __fastcall FakeGetBonePosition(void* ped, void*, Vec3& out, unsigned bone, bool) {
    Vec3 body = PedBody(ped);
    out = {body.x, body.y, body.z + (bone == 8 ? 0.65f : 0.0f)};
}

// CPlayerPed::GetWeaponRadiusOnScreen: 0.3 parado, abrindo com o contador de tiros (como a Deagle).
float __fastcall FakeRadius(void* ped, void*) {
    BYTE* data = *reinterpret_cast<BYTE**>(static_cast<BYTE*>(ped) + 0x480);
    float counter;
    memcpy(&counter, data + 0x2C, 4);
    float r = (counter * 0.5f + 1.0f) * 0.3f;
    return r > 0.2f ? r : 0.2f;
}

void __fastcall FakeSpriteDraw(void*, void*, const Rect& rect, const BYTE*) {
    if (g_drawsThisCall++ == 0) {
        g_firstRect = rect;
    }
    ++g_spriteDraws;
}

void __cdecl FakeDrawRect(const Rect&, const BYTE*) {
    ++g_rectDraws;
}

// CHud::DrawCrossHairs: a mira redonda do 1.0 US (MODE_AIMWEAPON), pelos enderecos do jogo.
void __cdecl FakeDrawCrossHairs() {
    ++g_crosshairCalls;
    g_drawsThisCall = 0;
    if (!g_aiming) {
        return;
    }
    float cx = W * Mem<float>(GTA_CHAIR_X), cy = H * Mem<float>(GTA_CHAIR_Y);
    float r = reinterpret_cast<RadiusFn>(GTA_WEAPON_RADIUS)(&g_local);
    static const BYTE white[4] = {255, 255, 255, 255};
    Rect rect;
    if (r == 0.2f) {
        rect = {cx - 1, cy - 1, cx + 1, cy + 1};
        reinterpret_cast<DrawRectFn>(GTA_DRAW_RECT)(rect, white);
    }
    float sx = 64.0f * r / 2.0f * W / 640.0f, sy = 64.0f * r / 2.0f * H / 448.0f;
    auto draw = [&]() { reinterpret_cast<SpriteDrawFn>(GTA_SPRITE_DRAW)(g_sprite, rect, white); };
    rect.x0 = cx - sx;
    rect.y0 = cy - sy;
    rect.x1 = rect.x0 + sx;
    rect.y1 = rect.y0 + sy;
    draw(); // em cima a esquerda
    rect.x0 = cx + sx;
    draw(); // em cima a direita
    rect.x0 = cx - sx;
    rect.y0 += 2 * sy;
    draw(); // embaixo a esquerda
    rect.x0 = cx + sx;
    draw(); // embaixo a direita
}

void* Alloc(DWORD size) {
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); // ja vem zerado
}

bool Redirect(DWORD at, void* to) {
    DWORD old;
    if (!VirtualProtect(reinterpret_cast<void*>(at & ~0xFFFu), 0x2000, PAGE_EXECUTE_READWRITE, &old)) {
        return false;
    }
    Mem<BYTE>(at) = 0xE9;
    Mem<DWORD>(at + 1) = reinterpret_cast<DWORD>(to) - (at + 5);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(at), 5);
    return true;
}

void SetMatrixPos(GameMatrix& m, float x, float y, float z) {
    memset(&m, 0, sizeof(m));
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    m.m[12] = x;
    m.m[13] = y;
    m.m[14] = z;
}

void BuildGame(bool version) {
    if (version) {
        Mem<DWORD>(GTA_VERSION_MARK) = 0x94BF;
    }
    Mem<float>(GTA_CHAIR_X) = 0.53f;
    Mem<float>(GTA_CHAIR_Y) = 0.4f;
    Mem<BYTE>(0xBA6769) = 1;   // HUD ligado nas opcoes
    Mem<BYTE>(0xA444A0) = 1;   // HUD ligado pelo script
    SetMatrixPos(g_cameraMatrix, CAMERA.x, CAMERA.y, CAMERA.z); // frente = (0, 1, 0)
    Mem<GameMatrix*>(GTA_THE_CAMERA + 0x14) = &g_cameraMatrix;
    Mem<GamePed*>(GTA_THE_CAMERA + 0x174 + 0x21C) = &g_local; // a camera segue o jogador

    SetMatrixPos(g_localMatrix, 0, 0, 0);
    Put<GameMatrix*>(g_local.raw, 0x14, &g_localMatrix);
    Put<BYTE*>(g_local.raw, 0x480, g_localData);
    Put<int>(g_local.raw, 0x530, 1);           // PEDSTATE_IDLE
    Put<float>(g_local.raw, 0x540, 100.0f);    // vida
    Put<float>(g_local.raw, 0x548, 0.0f);      // colete
    Put<BYTE>(g_local.raw, 0x718, 2);          // slot da pistola
    Put<int>(g_local.raw, 0x5A0 + 2 * 0x1C, 24); // Deagle
    Mem<GamePed*>(GTA_PLAYER_PED) = &g_local;
}

void SetFiring(float counter) {
    Put<float>(g_localData, 0x2C, counter);
}

void SetInVehicle(bool in) {
    DWORD flags;
    memcpy(&flags, g_local.raw + 0x46C, 4);
    flags = in ? flags | 0x100 : flags & ~0x100u;
    Put<DWORD>(g_local.raw, 0x46C, flags);
    Put<BYTE*>(g_local.raw, 0x58C, in ? g_vehicle : nullptr);
}

// ---------------------------------------------------------------- SA-MP de mentira

struct Player {
    int id;
    const char* name;
    DWORD color;
    float x, y;
    float health, armour;
};

const Player PLAYERS[] = {
    {1, "Joao_Silva", 0xFF2ECC71, 0.0f, 10.0f, 100.0f, 0.0f},
    {2, "Maria_Souza", 0xFFF39C12, -3.0f, 12.0f, 30.0f, 0.0f},
    {3, "Pedro_Lima", 0xFF3498DB, 0.0f, -8.0f, 100.0f, 0.0f}, // atras da camera
    {4, "Ana_Costa", 0xFFE74C3C, 4.0f, 14.0f, 100.0f, 0.0f},  // o servidor esconde a vida (nunca cai)
};
constexpr int PLAYER_COUNT = 4;

alignas(16) BYTE g_netGame[0x400];
alignas(16) BYTE g_pools[0x40];
alignas(16) BYTE g_playerPool[0x2000];
alignas(16) BYTE g_infos[PLAYER_COUNT][0x100];
alignas(16) BYTE g_remotes[PLAYER_COUNT][0x200];
alignas(16) BYTE g_sampPeds[PLAYER_COUNT][0x300];
GamePed g_peds[PLAYER_COUNT];
GameMatrix g_pedMatrices[PLAYER_COUNT];
alignas(16) BYTE g_dialog[0x100];
alignas(16) BYTE g_scoreboard[0x40];
alignas(16) BYTE g_localPlayer[0x100];
alignas(16) BYTE g_deathWindow[0x100];

void SetHealth(int k, float health) {
    Put<float>(g_remotes[k], 0x1BC, health);
}

void BuildSamp() {
    strcpy(reinterpret_cast<char*>(g_netGame + 0x20), "127.0.0.1");
    Put<int>(g_netGame, 0x225, 7777);
    Put<int>(g_netGame, 0x3BD, 14); // GAMESTATE_CONNECTED
    Put<BYTE*>(g_netGame, 0x3CD, g_pools);
    Put<BYTE*>(g_pools, 0x18, g_playerPool);
    Put<WORD>(g_playerPool, 0x04, 0); // o jogador local e o 0

    for (int k = 0; k < PLAYER_COUNT; ++k) {
        const Player& p = PLAYERS[k];
        BYTE* info = g_infos[k];
        BYTE* remote = g_remotes[k];
        Put<BYTE*>(g_playerPool, 0x2E + p.id * 4, info);
        Put<BOOL>(g_playerPool, 0xFDE + p.id * 4, TRUE);
        Put<BYTE*>(info, 0x00, remote);
        size_t len = strlen(p.name); // todos com menos de 16 letras: dentro do std::string
        memcpy(info + 0x0C, p.name, len + 1);
        Put<DWORD>(info, 0x1C, static_cast<DWORD>(len));
        Put<DWORD>(info, 0x20, 15);
        Put<float>(remote, 0x1B8, p.armour);
        Put<float>(remote, 0x1BC, p.health);
        Put<DWORD>(remote, 0x1F8, p.color); // o GetColorAsARGB falso le daqui
        SetMatrixPos(g_pedMatrices[k], p.x, p.y, 0.0f);
        Put<GameMatrix*>(g_peds[k].raw, 0x14, &g_pedMatrices[k]);
        Put<Vec3>(g_peds[k].raw, 0x04, Vec3{p.x, p.y, 0.0f});
        Put<void*>(g_peds[k].raw, 0x18, &g_peds[k]); // RwObject: tem modelo
        Put<GamePed*>(g_sampPeds[k], 0x2A4, &g_peds[k]);
        Put<BYTE*>(remote, 0x00, g_sampPeds[k]);
    }
    Mem<BYTE*>(g_samp + DIALOG) = g_dialog;
    Mem<BYTE*>(g_samp + SCOREBOARD) = g_scoreboard;
    Mem<BYTE*>(g_samp + NETGAME) = g_netGame; // por ultimo: com ele o .asi registra o /trokhit
}

void Give(int id, float damage, int weapon, int part) {
    reinterpret_cast<DamageFn>(g_samp + FN_SEND_GIVE)(g_localPlayer, id, damage, weapon, part);
}

void Take(int id, float damage, int weapon, int part) {
    reinterpret_cast<DamageFn>(g_samp + FN_SEND_TAKE)(g_localPlayer, id, damage, weapon, part);
}

// ---------------------------------------------------------------- captura

bool Capture(const char* path) {
    IDirect3DSurface9* back = nullptr;
    IDirect3DSurface9* sys = nullptr;
    bool ok = false;
    if (SUCCEEDED(g_real->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) &&
        SUCCEEDED(g_real->CreateOffscreenPlainSurface(W, H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, nullptr)) &&
        SUCCEEDED(g_real->GetRenderTargetData(back, sys))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            for (int y = 0; y < H; ++y) {
                memcpy(&g_shot[y * W], static_cast<BYTE*>(lr.pBits) + y * lr.Pitch, W * 4);
            }
            sys->UnlockRect();
            ok = true;
        }
    }
    if (sys) {
        sys->Release();
    }
    if (back) {
        back->Release();
    }
    if (!ok) {
        return false;
    }
    FILE* f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    BITMAPFILEHEADER fh = {};
    BITMAPINFOHEADER ih = {};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + W * H * 3;
    ih.biSize = sizeof(ih);
    ih.biWidth = W;
    ih.biHeight = H;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    static BYTE row[W * 3];
    for (int y = H - 1; y >= 0; --y) {
        for (int x = 0; x < W; ++x) {
            DWORD p = g_shot[y * W + x];
            row[x * 3] = p & 0xFF;
            row[x * 3 + 1] = (p >> 8) & 0xFF;
            row[x * 3 + 2] = (p >> 16) & 0xFF;
        }
        fwrite(row, sizeof(row), 1, f);
    }
    fclose(f);
    return true;
}

// Fracao dos pixels de um retangulo que nao sao a cor de fundo do "jogo".
double Covered(int x0, int y0, int x1, int y1) {
    int covered = 0, total = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            covered += (g_shot[y * W + x] & 0xFFFFFF) != (CLEAR & 0xFFFFFF);
            ++total;
        }
    }
    return total ? double(covered) / total : 0.0;
}

// Pixels bem vermelhos (sinal de acerto, abate, seta).
int Red(int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            DWORD p = g_shot[y * W + x];
            int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
            n += r > 170 && r > g + 80 && r > b + 80;
        }
    }
    return n;
}

// Pixels puxados para o vermelho em cima do fundo verde (a borda e translucida).
int Tinted(int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            DWORD p = g_shot[y * W + x];
            int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF;
            n += r >= 60 && r > g + 10;
        }
    }
    return n;
}

int Bright(int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            DWORD p = g_shot[y * W + x];
            n += ((p >> 16) & 0xFF) > 200 && ((p >> 8) & 0xFF) > 200 && (p & 0xFF) > 200;
        }
    }
    return n;
}

POINT Screen(float x, float y, float z) {
    Vec3 in = {x, y, z}, out;
    float w, h;
    FakeCalcScreenCoors(in, &out, &w, &h, false, false);
    return {static_cast<LONG>(out.x), static_cast<LONG>(out.y)};
}

int LogCount(const char* text) {
    FILE* f = fopen("Trok Hitmarker.log", "rb");
    if (!f) {
        return 0;
    }
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    int count = 0;
    for (const char* p = strstr(buf, text); p; p = strstr(p + 1, text)) {
        ++count;
    }
    return count;
}

bool LogHas(const char* text) {
    return LogCount(text) > 0;
}

bool FileExists(const char* path) {
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

void CheckN(bool ok, const char* what, int value) {
    snprintf(g_text, sizeof(g_text), "%s (%d)", what, value);
    Check(ok, g_text);
}

void CheckPct(bool ok, const char* what, double value) {
    snprintf(g_text, sizeof(g_text), "%s (%.1f%%)", what, value * 100);
    Check(ok, g_text);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("uso: fake_gta.exe <asi> <trok|mira-do-jogo|desligado|outra-versao|gta-errado>\n");
        return 2;
    }
    const char* scenario = argv[2];
    bool own = strcmp(scenario, "pasta-propria") == 0;
    bool trok = strcmp(scenario, "trok") == 0 || own;
    bool gameAim = strcmp(scenario, "mira-do-jogo") == 0;
    bool wrongGta = strcmp(scenario, "gta-errado") == 0;
    bool otherSamp = strcmp(scenario, "outra-versao") == 0;
    bool active = trok || gameAim; // o Trok Hitmarker liga (trok tambem vale para a pasta-propria)

    DWORD lo = reinterpret_cast<DWORD>(g_gtaMemory);
    DWORD hi = lo + sizeof(g_gtaMemory);
    if (lo > GTA_DRAW_CROSSHAIRS || hi < 0xC9C060) {
        printf("FALHA: memoria falsa do GTA fora do lugar (0x%08lX..0x%08lX)\n", lo, hi);
        return 2;
    }
    Check(Redirect(GTA_CALC_SCREEN_COORS, reinterpret_cast<void*>(FakeCalcScreenCoors)) &&
              Redirect(GTA_GET_BONE_POSITION, reinterpret_cast<void*>(FakeGetBonePosition)) &&
              Redirect(GTA_DRAW_CROSSHAIRS, reinterpret_cast<void*>(FakeDrawCrossHairs)) &&
              Redirect(GTA_SPRITE_DRAW, reinterpret_cast<void*>(FakeSpriteDraw)) &&
              Redirect(GTA_DRAW_RECT, reinterpret_cast<void*>(FakeDrawRect)) &&
              Redirect(GTA_WEAPON_RADIUS, reinterpret_cast<void*>(FakeRadius)),
          "funcoes de mentira do GTA no lugar (mira, raio, sprites, projecao, ossos)");
    BuildGame(!wrongGta);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "Grand theft auto San Andreas";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "GTA: San Andreas", WS_POPUP | WS_VISIBLE, 0, 0, W, H, nullptr,
                                nullptr, wc.hInstance, nullptr);
    SetForegroundWindow(hwnd);
    Mem<int>(0xC17044) = W;
    Mem<int>(0xC17048) = H;
    Mem<HWND>(0xC9C05C) = hwnd;
    Mem<HWND>(0xC8CF88) = hwnd;

    HMODULE samp = LoadLibraryA("samp.dll");
    Check(samp != nullptr, "samp.dll falso carregado");
    HMODULE asi = LoadLibraryA(argv[1]);
    Check(asi != nullptr, "asi carregado");
    if (!samp || !asi) {
        return 1;
    }
    g_samp = reinterpret_cast<DWORD>(samp);
    g_shot = static_cast<DWORD*>(Alloc(W * H * 4));
    BuildSamp();
    Sleep(300);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth = W;
    pp.BackBufferHeight = H;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.hDeviceWindow = hwnd;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    HRESULT hr = d3d ? d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                         &pp, &g_real)
                     : E_FAIL;
    Check(SUCCEEDED(hr) && g_real, "device d3d9 criado");
    if (!g_real) {
        return 1;
    }
    Mem<IDirect3DDevice9*>(0xC97C28) = g_real;

    const POINT aim = {static_cast<LONG>(W * 0.53f), static_cast<LONG>(H * 0.4f)};
    const POINT joao = Screen(0.0f, 10.0f, 0.0f);
    const POINT ring = {W / 2, H / 2 + static_cast<LONG>(165 * H / 1080.0f)}; // a seta de quem atira de tras
    DWORD start = GetTickCount();
    int step = 0;
    int calls = 0, sprites = 0;
    DWORD gives = 0, deaths = 0, chatBefore = 0;
    int frame = 0, dropFrame = 0, takeFrame = 0;
    for (;;) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        DWORD t = GetTickCount() - start;

        // Antes do quadro: o "servidor" e o "jogador" no tempo certo.
        if (step == 0 && t >= 4600) {
            const char* name = reinterpret_cast<const char*>(Field(F_CMD_NAME));
            if (active || strcmp(scenario, "desligado") == 0) {
                Check(name && strcmp(name, "trokhit") == 0, "/trokhit registrado no CInput do samp.dll");
            } else {
                Check(name == nullptr, "nada registrado no samp.dll");
            }
            g_aiming = true;
            SetFiring(gameAim || !active ? 4.0f : 0.0f);
            calls = g_crosshairCalls;
            sprites = g_spriteDraws;
            ++step;
        }
        if (step == 2 && t >= 4900) {
            gives = Field(F_GIVE_COUNT);
            Give(1, 46.2f, 24, 3); // Joao, no tronco
            Check(Field(F_GIVE_COUNT) == gives + 1 && Field(F_GIVE_ID) == 1,
                  "SendGiveDamage original continua sendo chamado (o pacote vai ao servidor)");
            ++step;
        }
        if (step == 3 && t >= 4930) {
            SetHealth(0, 54.0f); // o sync do Joao mostra a queda
            dropFrame = frame;
            ++step;
        }
        if (step == 5 && t >= 5100) {
            Give(2, 46.2f, 24, 9); // Maria, na cabeca
            ++step;
        }
        if (step == 6 && t >= 5160) {
            SetHealth(1, 0.0f);
            deaths = Field(F_DEATH_COUNT);
            reinterpret_cast<DeathFn>(g_samp + FN_DEATH_ADD)(g_deathWindow, "Victor_Trok", "Maria_Souza", 0, 0, 24);
            Check(Field(F_DEATH_COUNT) == deaths + 1, "mensagem de morte do SA-MP continua sendo mostrada");
            ++step;
        }
        if (step == 8 && t >= 5400) {
            Take(3, 33.0f, 31, 3); // Pedro, de tras
            Put<float>(g_local.raw, 0x540, 67.0f);
            takeFrame = frame;
            ++step;
        }
        if (step == 10 && t >= 5600) {
            Give(4, 46.2f, 24, 3); // Ana: a vida dela nunca cai
            ++step;
        }
        if (step == 12 && t >= 6900) {
            chatBefore = Field(F_CHAT_COUNT);
            if (Field(F_CMD_PROC)) {
                reinterpret_cast<CmdProc>(Field(F_CMD_PROC))("");
            }
            ++step;
        }
        if (step == 14 && t >= 7400) {
            if (Field(F_CMD_PROC)) {
                reinterpret_cast<CmdProc>(Field(F_CMD_PROC))("");
            }
            ++step;
        }
        if (step == 16 && t >= 7900) {
            g_aiming = false; // a pe, sem mirar: a do Trok fica (sempre visivel)
            if (own) {
                Check(CopyFileA("nova_mira.png", "Trok Hitmarker\\mira\\mira.png", FALSE) != 0,
                      "mira.png nova copiada para a pasta com o jogo rodando");
            }
            ++step;
        }
        if (step == 18 && t >= 9200) {
            SetInVehicle(true);
            ++step;
        }

        // O quadro do jogo, com o HUD desenhando a mira dentro da cena.
        g_real->Clear(0, nullptr, D3DCLEAR_TARGET, CLEAR, 1.0f, 0);
        if (SUCCEEDED(g_real->BeginScene())) {
            reinterpret_cast<VoidFn>(GTA_DRAW_CROSSHAIRS)();
            g_real->EndScene();
        }
        g_real->Present(nullptr, nullptr, nullptr, nullptr);
        ++frame;

        // Depois do quadro: confere.
        if (step == 1 && t >= 4850) {
            Check(g_crosshairCalls > calls, "CHud::DrawCrossHairs chamado pelo endereco do jogo");
            int drawn = g_spriteDraws - sprites;
            Check(Capture("mira.bmp"), "back buffer salvo (mira.bmp)");
            if (trok) {
                CheckN(drawn == 0, "mira do Trok: os quartos da mira do jogo nao chegam ao GTA", drawn);
                CheckN(Bright(aim.x - 12, aim.y - 12, aim.x + 12, aim.y + 12) > 20,
                       "o ponto do Trok (mira.png) no lugar da mira do jogo", Bright(aim.x - 12, aim.y - 12, aim.x + 12, aim.y + 12));
            } else {
                CheckN(drawn > 0, "a mira do jogo chega ao GTA", drawn);
                CheckPct(Covered(0, 0, W, H) < 0.0005, "nada do Trok Hitmarker na tela", Covered(0, 0, W, H));
            }
            float half = g_firstRect.x1 - g_firstRect.x0;
            if (gameAim) {
                // estatica: raio 0.3 (contador zerado so no desenho) -> 24 px; 150% -> 36 px
                snprintf(g_text, sizeof(g_text),
                         "mira do jogo estatica e em 150%%: primeiro quarto com %.1f px (esperado 36) e canto no centro "
                         "(%.1f, %.1f)", half, g_firstRect.x1, g_firstRect.y1);
                Check(std::fabs(half - 36.0f) < 0.6f && std::fabs(g_firstRect.x1 - aim.x) < 1.5f &&
                          std::fabs(g_firstRect.y1 - aim.y) < 1.5f,
                      g_text);
                float counter;
                memcpy(&counter, g_localData + 0x2C, 4);
                Check(counter == 4.0f, "o contador de tiros do jogador volta ao valor dele depois do desenho");
            } else if (!trok) {
                snprintf(g_text, sizeof(g_text), "mira do jogo intacta: primeiro quarto com %.1f px (esperado 72)", half);
                Check(std::fabs(half - 72.0f) < 0.6f, g_text);
            }
            ++step;
        }
        if (step == 4 && frame >= dropFrame + 2) { // logo depois: o sinal de acerto dura 0,35 s
            Check(Capture("acerto.bmp"), "back buffer salvo (acerto.bmp)");
            if (active) {
                int red = Red(aim.x - 20, aim.y - 20, aim.x + 20, aim.y + 20);
                CheckN(red > 30, "sinal de acerto (acerto.png, vermelho) na mira", red);
                double num = Covered(joao.x + 5, joao.y - 80, joao.x + 90, joao.y + 10);
                CheckPct(num > 0.04, "numero de dano subindo ao lado do Joao", num);
                double body = Covered(joao.x - 18, joao.y - 18, joao.x + 18, joao.y + 18);
                CheckPct(body > 0.05, "local do acerto (corpo.png) no tronco do Joao", body);
            } else {
                CheckPct(Covered(0, 0, W, H) < 0.0005, "nada na tela depois do tiro", Covered(0, 0, W, H));
            }
            ++step;
        }
        if (step == 7 && t >= 5300) {
            Check(Capture("abate.bmp"), "back buffer salvo (abate.bmp)");
            if (active) {
                int red = Red(aim.x - 20, aim.y - 20, aim.x + 20, aim.y + 20);
                CheckN(red > 20, "efeito de abate (abate.png) na mira", red);
                double informer = Covered(W * 60 / 100, H * 47 / 100 - 120, W * 60 / 100 + 300, H * 47 / 100 + 40);
                CheckPct(informer > 0.01, "informer classico do dano dado a direita do centro", informer);
            }
            ++step;
        }
        if (step == 9 && frame >= takeFrame + 2) { // a borda apaga em 0,6 s
            Check(Capture("levado.bmp"), "back buffer salvo (levado.bmp)");
            if (active) {
                int edge = Tinted(0, H / 3, 40, H * 2 / 3);
                CheckN(edge > 300, "borda de dano levado (vida: vermelha) nas laterais", edge);
                int arrow = Red(ring.x - 40, ring.y - 25, ring.x + 40, ring.y + 30);
                CheckN(arrow > 60, "seta vermelha embaixo do centro (o tiro veio de tras)", arrow);
                double informer = Covered(W * 40 / 100 - 300, H * 47 / 100 - 120, W * 40 / 100, H * 47 / 100 + 40);
                CheckPct(informer > 0.01, "informer do dano levado a esquerda do centro", informer);
            }
            ++step;
        }
        if (step == 11 && t >= 6800) {
            if (active) {
                Check(LogHas("acerto: id 4, parte 3, arma 24, GTA 46.2 -> a vida nao caiu em 1 s, vale o do GTA (~46)"),
                      "log: sem queda de vida em 1 s, o numero e o do GTA (~46)");
            }
            ++step;
        }
        if (step == 13 && t >= 7300) {
            if (active) {
                Check(Capture("menu.bmp"), "back buffer salvo (menu.bmp)");
                double menu = Covered(W / 2 - 150, H / 2 + 30, W / 2 + 150, H / 2 + 150);
                CheckPct(menu > 0.95, "menu do /trokhit aberto no centro da tela", menu);
                Check(int(Field(F_CURSOR_MODE)) == 2, "cursor do SA-MP no modo 2 com o menu aberto");
                Check(Field(F_CHAT_COUNT) == chatBefore, "nenhum aviso de 'desenho nao ativo' no chat");
            }
            ++step;
        }
        if (step == 15 && t >= 7800) {
            if (active) {
                Check(int(Field(F_CURSOR_MODE)) == 0, "menu fechado no segundo /trokhit (cursor devolvido)");
            }
            ++step;
        }
        if (step == 17 && t >= 9100) { // a pasta e conferida 1x por segundo
            Check(Capture("sem_mirar.bmp"), "back buffer salvo (sem_mirar.bmp)");
            int b = Bright(aim.x - 12, aim.y - 12, aim.x + 12, aim.y + 12);
            if (own) {
                int magenta = 0;
                for (int y = aim.y - 12; y < aim.y + 12; ++y) {
                    for (int x = aim.x - 12; x < aim.x + 12; ++x) {
                        DWORD p = g_shot[y * W + x];
                        magenta += ((p >> 16) & 0xFF) > 200 && ((p >> 8) & 0xFF) < 60 && (p & 0xFF) > 200;
                    }
                }
                CheckN(magenta > 50, "a mira.png nova (magenta) ja aparece no lugar da antiga", magenta);
            } else if (trok) {
                CheckN(b > 20, "a pe sem mirar: o ponto do Trok continua (sempre visivel)", b);
            } else {
                CheckN(b == 0, "sem mirar: nenhuma mira do Trok", b);
            }
            ++step;
        }
        if (step == 19 && t >= 9400) {
            Check(Capture("no_carro.bmp"), "back buffer salvo (no_carro.bmp)");
            int b = Bright(aim.x - 12, aim.y - 12, aim.x + 12, aim.y + 12);
            CheckN(b == 0, "no carro: o ponto do Trok some", b);
            ++step;
        }
        // O menu pelo teclado: abrir e fechar nao grava nada; a seta muda o tamanho da mira e o INI grava depois.
        if (step == 20) {
            if (!trok) {
                step = 30;
            } else if (t >= 9500) {
                Check(!FileExists("Trok Hitmarker.ini"), "abrir e fechar o menu nao grava nada no INI");
                SetInVehicle(false);
                reinterpret_cast<CmdProc>(Field(F_CMD_PROC))("");
                ++step;
            }
        }
        if (step == 21 && t >= 9800) {
            SendMessageA(hwnd, WM_KEYDOWN, VK_DOWN, 1); // linha 2: Tamanho (mira e efeitos)
            ++step;
        }
        if (step == 22 && t >= 9850) {
            SendMessageA(hwnd, WM_KEYUP, VK_DOWN, 0xC0000001);
            ++step;
        }
        if (step == 23 && t >= 9900) {
            SendMessageA(hwnd, WM_KEYDOWN, VK_RIGHT, 1); // +10%
            ++step;
        }
        if (step == 24 && t >= 9950) {
            SendMessageA(hwnd, WM_KEYUP, VK_RIGHT, 0xC0000001);
            ++step;
        }
        if (step == 25 && t >= 10050) {
            reinterpret_cast<CmdProc>(Field(F_CMD_PROC))("");
            ++step;
        }
        if (step == 26 && t >= 11200) {
            char path[MAX_PATH];
            GetFullPathNameA("Trok Hitmarker.ini", MAX_PATH, path, nullptr);
            int all = GetPrivateProfileIntA("padrao", "mira_tamanho", -1, path);
            int server = GetPrivateProfileIntA("127.0.0.1:7777", "mira_tamanho", -1, path);
            snprintf(g_text, sizeof(g_text), "seta para a direita no Tamanho: INI gravado com 110 (padrao %d, servidor %d)",
                     all, server);
            Check(all == 110 && server == 110, g_text);
            step = 30;
        }
        if (step == 30) {
            break;
        }
        if (t > 25000) {
            Check(false, "tempo esgotado");
            break;
        }
        Sleep(5);
    }

    if (active) {
        Check(LogHas("gta_sa.exe 1.0 US confirmado"), "log: gta_sa.exe 1.0 US confirmado");
        Check(LogHas("ganchos do samp.dll instalados"), "log: ganchos do samp.dll instalados");
        Check(LogHas("ganchos da mira do GTA instalados"), "log: ganchos da mira do GTA instalados");
        Check(LogHas("perfil: 127.0.0.1:7777"), "log: perfil do servidor 127.0.0.1:7777 carregado");
        Check(LogHas("acerto: id 1, parte 3, arma 24, GTA 46.2 -> real 46"), "log: dano real do Joao (46)");
        Check(LogHas("acerto: id 2, parte 9, arma 24, GTA 46.2 -> real 30"),
              "log: headshot na Maria tirou o que ela tinha (30)");
        // a mensagem de morte chega antes da vida zerada aparecer no quadro seguinte: o abate conta uma vez so
        CheckN(LogCount("abate: id 2 (") == 1, "log: abate da Maria contado uma vez (vida zerada e mensagem de morte)",
               LogCount("abate: id 2 ("));
        Check(LogHas("dano levado: id 3, GTA 33.0 -> real 33"), "log: dano levado do Pedro (33)");
        Check(LogHas("sons prontos") || LogHas("DirectSoundCreate8 falhou"), "log: sons iniciados (ou sem placa de som)");
        if (own) {
            Check(!FileExists("Trok Hitmarker\\mira\\corpo.png") && !FileExists("Trok Hitmarker\\sons\\morte.wav"),
                  "pasta que ja existia fica como o usuario deixou (nada gravado por cima)");
            Check(LogHas("acerto.mp3 (0.2"), "log: acerto.mp3 da pasta decodificado");
            Check(LogHas("imagem acerto.png: 300x200 da pasta"), "log: acerto.png retangular (300x200) da pasta");
            Check(LogHas("imagem mira.png: 64x64 da pasta"), "log: mira.png nova recarregada com o jogo rodando");
        } else {
            Check(FileExists("Trok Hitmarker\\mira\\mira.png") && FileExists("Trok Hitmarker\\mira\\corpo.png"),
                  "pasta Trok Hitmarker\\mira criada com as imagens padrao");
            Check(FileExists("Trok Hitmarker\\sons\\acerto.wav") && FileExists("Trok Hitmarker\\sons\\morte.wav"),
                  "pasta Trok Hitmarker\\sons criada com os sons padrao");
        }
        if (LogHas("sons prontos")) {
            Check(LogHas("som acerto tocado (volume 80%): 0x00000000"), "log: som de acerto tocado no DirectSound");
            Check(LogHas("som dano_levado tocado"), "log: som de dano levado tocado");
        }
    } else if (wrongGta) {
        Check(LogHas("nao e o 1.0 US"), "log: gta_sa.exe de outra versao recusado");
        Check(!LogHas("ganchos"), "log: nenhum gancho instalado");
    } else if (otherSamp) {
        Check(LogHas("nao e o 0.3.7 R1"), "log: samp.dll de outra versao recusado");
        Check(!LogHas("ganchos da mira"), "log: a mira do GTA nao foi tocada");
    } else {
        Check(!LogHas("acerto: id"), "log: desligado, nenhum acerto contado");
    }
    printf(g_failures ? "FALHOU (%d)\n" : "PASSOU\n", g_failures);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), g_failures ? 1 : 0); // sem descarregar o .asi (o jogo tambem nao)
    return 0;
}
