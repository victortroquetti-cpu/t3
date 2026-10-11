// Smoke test do Trok Tags fora do jogo (Wine 32 bits + Xvfb): um "GTA de mentira" que
//  - deixa os enderecos fixos do gta_sa.exe 1.0 US dentro da propria imagem (array gigante no .bss), com
//    CSprite::CalcScreenCoors e CPed::GetBonePosition de mentira (camera em (0, 0, 0.8) olhando para +Y);
//  - carrega o samp.dll falso (make_fake_samp.py) e o .asi antes de existir device, como o ASI Loader, e
//    monta as estruturas do SA-MP 0.3.7 R1 que o Trok Tags le: CNetGame conectado em 127.0.0.1:7777, pools,
//    4 jogadores remotos, textos 3D e baloes de fala;
//  - a cada quadro faz o papel do SA-MP: BeginScene, CPlayerTags::DrawLabel e DrawHealthBar so de quem o
//    servidor deixa ver, CLabelPool::Draw, EndScene e Present;
//  - confere: as originais somem enquanto o overlay desenha (o texto 3D preso ao jogador sai so durante o
//    desenho do SA-MP e volta depois), o balao passa para o Trok Tags, o desenho aparece em cima das
//    cabecas e so de quem o SA-MP desenharia, as originais voltam sozinhas quando o Present para, o ping e
//    pedido, o dialogo do servidor esconde o overlay e o /troktags abre e fecha o menu.
//
// Uso: fake_gta.exe <asi> <cenario>
//   direto            o device do jogo exposto direto
//   samp-mods         o ponteiro do device e um proxy estilo SA-MP (BeginScene e Present passam por ele)
//   desligado         Trok Tags.ini com ligado=0: tudo fica com o SA-MP
//   outra-versao      samp.dll com o ponto de entrada do R3: o .asi nao instala nada
//   baloes-estranhos  a estrutura dos baloes nao confere: os baloes ficam com o SA-MP, o resto segue

#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// A imagem vai de 0x400000 ate ~0xDAx000 e cobre 0x5E4280..0xC9C05C. Tem que ser o comeco do .bss: os outros
// buffers grandes vem do VirtualAlloc.
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

typedef void(__cdecl* CmdProc)(const char*);
typedef void(__thiscall* DrawLabelFn)(void*, Vec3*, const char*, DWORD, float, bool, int);
typedef void(__thiscall* DrawHealthBarFn)(void*, Vec3*, float, float, float);
typedef void(__thiscall* LabelPoolDrawFn)(void*);

constexpr int W = 1600;
constexpr int H = 900;
constexpr D3DCOLOR CLEAR = D3DCOLOR_XRGB(20, 90, 40);
constexpr Vec3 CAMERA = {0.0f, 0.0f, 0.8f};
constexpr float FOCAL = W / 2; // 90 graus na horizontal
constexpr float HEAD_ABOVE_BODY = 0.65f;

// gta_sa.exe 1.0 US
constexpr DWORD GTA_CALC_SCREEN_COORS = 0x70CE30;
constexpr DWORD GTA_GET_BONE_POSITION = 0x5E4280;
constexpr DWORD GTA_THE_CAMERA = 0xB6F028;

// samp.dll 0.3.7 R1 (os mesmos de src/samp.cpp)
constexpr DWORD NETGAME = 0x21A0F8, DIALOG = 0x21A0B8, SCOREBOARD = 0x21A0B4, LABEL_DRAWER = 0x21A0C0,
                CHAT_BUBBLE = 0x21A0DC, LAST_PLAYERS_UPDATE = 0x104978;
constexpr DWORD FN_DRAW_LABEL = 0x686C0, FN_DRAW_HEALTH_BAR = 0x689C0, FN_LABEL_POOL_DRAW = 0x1340;
constexpr int MAX_PLAYERS = 1004, MAX_LABELS = 2048, BUBBLE_SIZE = 280;

// Campos do samp.dll falso (make_fake_samp.py), relativos a samp.dll + 0x300000.
constexpr DWORD F_CMD_NAME = 0, F_CMD_PROC = 4, F_CHAT_COUNT = 8, F_CURSOR_MODE = 0xC;
constexpr DWORD F_LABELS_DRAWN = 0x2000, F_BARS_DRAWN = 0x2004, F_POOL_DRAWS = 0x2008, F_OBSTACLE_CALLS = 0x200C,
                F_NO_OBSTACLES = 0x2010, F_UPDATE_PLAYERS = 0x2014, F_SEEN = 0x2040;

int g_failures = 0;
IDirect3DDevice9* g_real = nullptr;
DWORD* g_shot = nullptr; // W * H
DWORD g_samp = 0;

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
    float m[16]; // RwMatrix: a posicao fica em +0x30
    BYTE pad[0x20];
};

struct GamePed {
    BYTE raw[0x800]; // CPlaceable: posicao em +0x04, CMatrix* em +0x14
};

GameMatrix g_cameraMatrix;

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

// CPed::GetBonePosition (thiscall): a cabeca fica 0.65 m acima do corpo.
void __fastcall FakeGetBonePosition(void* ped, void*, Vec3& out, unsigned bone, bool) {
    Vec3 body = PedBody(ped);
    out = {body.x, body.y, body.z + (bone == 8 ? HEAD_ABOVE_BODY : 0.0f)};
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

// ---------------------------------------------------------------- SA-MP de mentira

struct Player {
    int id;
    const char* name;
    DWORD color; // ARGB, como o SA-MP passa para o DrawLabel
    float x, y;  // no chao, a camera fica em (0, 0)
    bool streamed;    // tem personagem no jogo
    bool tagByServer; // o SA-MP desenharia a nametag (distancia, parede, ShowPlayerNameTagForPlayer)
    float health, armour;
    int status; // 2 = AFK
    int ping;
};

const Player PLAYERS[] = {
    {1, "Joao_Silva", 0xFF2ECC71, 0.0f, 10.0f, true, true, 100.0f, 60.0f, 0, 80},
    {2, "Maria_Souza_Lima", 0xFFF39C12, -6.0f, 16.0f, true, true, 22.0f, 0.0f, 2, 320}, // nome de 16: fora do SSO
    {3, "Pedro_Lima", 0xFF3498DB, 0.0f, 0.0f, false, false, 100.0f, 0.0f, 0, 60},     // fora de alcance
    {4, "Ana_Costa", 0xFFE74C3C, 8.0f, 16.0f, true, false, 100.0f, 0.0f, 0, 50},      // servidor esconde a nametag
};
constexpr int PLAYER_COUNT = 4;

alignas(16) BYTE g_netGame[0x400];
alignas(16) BYTE g_settings[0x100];
alignas(16) BYTE g_pools[0x40];
alignas(16) BYTE g_playerPool[0x2000];
alignas(16) BYTE g_infos[PLAYER_COUNT][0x100];
alignas(16) BYTE g_remotes[PLAYER_COUNT][0x200];
alignas(16) BYTE g_sampPeds[PLAYER_COUNT][0x300];
GamePed g_peds[PLAYER_COUNT];
GameMatrix g_pedMatrices[PLAYER_COUNT];
BYTE* g_labelPool = nullptr; // MAX_LABELS * 0x1D + MAX_LABELS * 4 (texto 3D + m_bNotEmpty)
BYTE* g_bubbles = nullptr;   // MAX_PLAYERS * BUBBLE_SIZE
alignas(16) BYTE g_dialog[0x100];
alignas(16) BYTE g_scoreboard[0x40];
alignas(16) BYTE g_labelDrawer[0x40];
alignas(16) BYTE g_playerTags[0x40];
char g_longNames[PLAYER_COUNT][32];

void SetMatrixPos(GameMatrix& m, float x, float y, float z) {
    memset(&m, 0, sizeof(m));
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    m.m[12] = x;
    m.m[13] = y;
    m.m[14] = z;
}

void AddLabel(int index, const char* text, DWORD argb, Vec3 pos, float dist, bool los, WORD player) {
    BYTE* l = g_labelPool + index * 0x1D;
    Put<const char*>(l, 0x00, text);
    Put<DWORD>(l, 0x04, argb);
    Put<Vec3>(l, 0x08, pos);
    Put<float>(l, 0x14, dist);
    Put<BYTE>(l, 0x18, los ? 1 : 0);
    Put<WORD>(l, 0x19, player);
    Put<WORD>(l, 0x1B, 0xFFFF);
    Put<BOOL>(g_labelPool, 0xE800 + index * 4, TRUE);
}

BOOL LabelUsed(int index) {
    BOOL v;
    memcpy(&v, g_labelPool + 0xE800 + index * 4, sizeof(v));
    return v;
}

void SetBubble(int id, const char* text, DWORD argb, int life, float dist) {
    BYTE* b = g_bubbles + id * BUBBLE_SIZE;
    memset(b, 0, BUBBLE_SIZE);
    Put<int>(b, 0, 1);
    strncpy(reinterpret_cast<char*>(b + 4), text, 255);
    Put<DWORD>(b, 260, GetTickCount());
    Put<int>(b, 264, life);
    Put<DWORD>(b, 268, argb);
    Put<float>(b, 272, dist);
}

int BubbleExists(int id) {
    int v;
    memcpy(&v, g_bubbles + id * BUBBLE_SIZE, sizeof(v));
    return v;
}

void BuildSamp(bool strangeBubbles) {
    strcpy(reinterpret_cast<char*>(g_netGame + 0x20), "127.0.0.1");
    Put<int>(g_netGame, 0x225, 7777);
    Put<int>(g_netGame, 0x3BD, 14); // GAMESTATE_CONNECTED
    Put<BYTE*>(g_netGame, 0x3C5, g_settings);
    Put<BYTE*>(g_netGame, 0x3CD, g_pools);
    Put<float>(g_settings, 0x27, 40.0f); // distancia da nametag do servidor
    Put<BYTE>(g_settings, 0x2F, 1);      // nametag some atras de parede
    Put<BYTE>(g_settings, 0x38, 1);      // ShowNameTags
    Put<BYTE*>(g_pools, 0x0C, g_labelPool);
    Put<BYTE*>(g_pools, 0x18, g_playerPool);
    Put<WORD>(g_playerPool, 0x04, 0); // o jogador local e o 0

    for (int k = 0; k < PLAYER_COUNT; ++k) {
        const Player& p = PLAYERS[k];
        BYTE* info = g_infos[k];
        BYTE* remote = g_remotes[k];
        Put<BYTE*>(g_playerPool, 0x2E + p.id * 4, info);
        Put<BOOL>(g_playerPool, 0xFDE + p.id * 4, TRUE);
        Put<BYTE*>(info, 0x00, remote);
        // std::string do MSVC: ate 15 letras dentro, mais que isso num ponteiro
        size_t len = strlen(p.name);
        if (len < 16) {
            memcpy(info + 0x0C, p.name, len + 1);
            Put<DWORD>(info, 0x20, 15);
        } else {
            strcpy(g_longNames[k], p.name);
            Put<char*>(info, 0x0C, g_longNames[k]);
            Put<DWORD>(info, 0x20, 31);
        }
        Put<DWORD>(info, 0x1C, static_cast<DWORD>(len));
        Put<int>(info, 0x28, p.ping);
        Put<float>(remote, 0x1B8, p.armour);
        Put<float>(remote, 0x1BC, p.health);
        Put<int>(remote, 0x1D1, p.status);
        if (p.streamed) {
            SetMatrixPos(g_pedMatrices[k], p.x, p.y, 0.0f);
            Put<GameMatrix*>(g_peds[k].raw, 0x14, &g_pedMatrices[k]);
            Put<Vec3>(g_peds[k].raw, 0x04, Vec3{p.x, p.y, 0.0f});
            Put<GamePed*>(g_sampPeds[k], 0x2A4, &g_peds[k]);
            Put<BYTE*>(remote, 0x00, g_sampPeds[k]);
        }
    }

    // 0: preso ao Joao (no lugar do nome, como o "RG" do Horizonte), com teste de parede
    // 1: texto 3D do mundo (fica com o SA-MP)
    // 2: preso ao Pedro, que esta fora de alcance (fica com o SA-MP)
    // 3: segunda linha presa ao Joao, mais alta
    AddLabel(0, "{5AA2FF}RG: {FFFFFF}48213", 0xFFFFFFFF, Vec3{0.0f, 0.0f, 0.45f}, 20.0f, true, 1);
    AddLabel(1, "Prefeitura", 0xFFFFFFFF, Vec3{0.0f, 12.0f, 1.0f}, 30.0f, false, 0xFFFF);
    AddLabel(2, "[VIP]", 0xFFFFD27A, Vec3{0.0f, 0.0f, 0.5f}, 20.0f, false, 3);
    AddLabel(3, "[ADMIN] Moderador", 0xFFFF6B6B, Vec3{0.0f, 0.0f, 0.6f}, 20.0f, false, 1);
    Field(F_NO_OBSTACLES) = 1;

    if (strangeBubbles) {
        Put<int>(g_bubbles, 900 * BUBBLE_SIZE, 5); // um "existe" que nao e 0 nem 1: nao e a estrutura
    }

    Mem<BYTE*>(g_samp + DIALOG) = g_dialog;
    Mem<BYTE*>(g_samp + SCOREBOARD) = g_scoreboard;
    Mem<BYTE*>(g_samp + LABEL_DRAWER) = g_labelDrawer;
    Mem<BYTE*>(g_samp + CHAT_BUBBLE) = g_bubbles;
    Mem<BYTE*>(g_samp + NETGAME) = g_netGame; // por ultimo: com ele o .asi registra o /troktags
}

// O que o SA-MP desenha dentro da cena do jogo (com um dialogo aberto tambem: o dialogo cobre depois).
void SampDraws() {
    for (const Player& p : PLAYERS) {
        if (!p.streamed || !p.tagByServer) {
            continue;
        }
        Vec3 pos = {p.x, p.y, HEAD_ABOVE_BODY + 0.2f};
        float dx = p.x - CAMERA.x, dy = p.y - CAMERA.y;
        float dist = sqrtf(dx * dx + dy * dy);
        char text[64];
        snprintf(text, sizeof(text), "%s (%d)", p.name, p.id);
        reinterpret_cast<DrawLabelFn>(g_samp + FN_DRAW_LABEL)(g_playerTags, &pos, text, p.color, dist, p.status == 2,
                                                              p.status);
        reinterpret_cast<DrawHealthBarFn>(g_samp + FN_DRAW_HEALTH_BAR)(g_playerTags, &pos, p.health, p.armour, dist);
    }
    reinterpret_cast<LabelPoolDrawFn>(g_samp + FN_LABEL_POOL_DRAW)(g_labelPool);
}

// ---------------------------------------------------------------- proxy estilo SA-MP

void* g_proxyVtable[119];
void* g_proxyObject[1] = {g_proxyVtable};

// Cada metodo do proxy e um thunk que troca o "this" pelo device real e salta pela vtable ao vivo dele.
IDirect3DDevice9* MakeProxy(IDirect3DDevice9* real) {
    BYTE* code = static_cast<BYTE*>(VirtualAlloc(nullptr, 119 * 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    for (int i = 0; i < 119; ++i) {
        BYTE* p = code + i * 32;
        g_proxyVtable[i] = p;
        *p++ = 0xB8; // mov eax, real
        *reinterpret_cast<DWORD*>(p) = reinterpret_cast<DWORD>(real);
        p += 4;
        *p++ = 0x89; // mov [esp+4], eax
        *p++ = 0x44;
        *p++ = 0x24;
        *p++ = 0x04;
        *p++ = 0x8B; // mov eax, [eax]
        *p++ = 0x00;
        *p++ = 0xFF; // jmp [eax + i*4]
        *p++ = 0xA0;
        *reinterpret_cast<DWORD*>(p) = i * 4;
    }
    return reinterpret_cast<IDirect3DDevice9*>(g_proxyObject);
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
    int covered = 0;
    int total = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            DWORD p = g_shot[y * W + x] & 0xFFFFFF;
            covered += p != (CLEAR & 0xFFFFFF);
            ++total;
        }
    }
    return total ? double(covered) / total : 0.0;
}

// Ponto da tela logo acima da cabeca (onde a pilha do Trok Tags comeca).
POINT Anchor(const Player& p) {
    Vec3 at = {p.x, p.y, HEAD_ABOVE_BODY + 0.25f}, screen;
    float w, h;
    FakeCalcScreenCoors(at, &screen, &w, &h, false, false);
    return {static_cast<LONG>(screen.x), static_cast<LONG>(screen.y)};
}

double AboveHead(const Player& p, int halfWidth, int height) {
    POINT a = Anchor(p);
    return Covered(a.x - halfWidth, a.y - height, a.x + halfWidth, a.y - 1);
}

bool LogHas(const char* text) {
    FILE* f = fopen("Trok Tags.log", "rb");
    if (!f) {
        return false;
    }
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    return strstr(buf, text) != nullptr;
}

void CheckPct(bool ok, const char* what, double value) {
    char text[200];
    snprintf(text, sizeof(text), "%s (%.1f%%)", what, value * 100);
    Check(ok, text);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("uso: fake_gta.exe <asi> <direto|samp-mods|desligado|outra-versao|baloes-estranhos>\n");
        return 2;
    }
    const char* scenario = argv[2];
    bool proxy = strcmp(scenario, "samp-mods") == 0;
    bool off = strcmp(scenario, "desligado") == 0;
    bool otherVersion = strcmp(scenario, "outra-versao") == 0;
    bool strangeBubbles = strcmp(scenario, "baloes-estranhos") == 0;
    bool takesOver = !off && !otherVersion;        // o Trok Tags assume nametags e textos fixos
    bool takesBubbles = takesOver && !strangeBubbles; // e os baloes

    DWORD lo = reinterpret_cast<DWORD>(g_gtaMemory);
    DWORD hi = lo + sizeof(g_gtaMemory);
    if (lo > GTA_GET_BONE_POSITION || hi < 0xC9C060) {
        printf("FALHA: memoria falsa do GTA fora do lugar (0x%08lX..0x%08lX)\n", lo, hi);
        return 2;
    }
    Check(Redirect(GTA_CALC_SCREEN_COORS, reinterpret_cast<void*>(FakeCalcScreenCoors)) &&
              Redirect(GTA_GET_BONE_POSITION, reinterpret_cast<void*>(FakeGetBonePosition)),
          "CalcScreenCoors e GetBonePosition de mentira no lugar");
    SetMatrixPos(g_cameraMatrix, CAMERA.x, CAMERA.y, CAMERA.z);
    Mem<GameMatrix*>(GTA_THE_CAMERA + 0x14) = &g_cameraMatrix;

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
    g_labelPool = static_cast<BYTE*>(Alloc(MAX_LABELS * 0x1D + MAX_LABELS * 4));
    g_bubbles = static_cast<BYTE*>(Alloc(MAX_PLAYERS * BUBBLE_SIZE));
    BuildSamp(strangeBubbles);
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
    IDirect3DDevice9* exposed = proxy ? MakeProxy(g_real) : g_real;
    Mem<IDirect3DDevice9*>(0xC97C28) = exposed;

    const Player& joao = PLAYERS[0];
    const Player& maria = PLAYERS[1];
    const Player& ana = PLAYERS[3];
    DWORD start = GetTickCount();
    int step = 0;
    bool present = true;
    DWORD labels = 0, bars = 0, chatBefore = 0;
    for (;;) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        DWORD t = GetTickCount() - start;

        // Antes do quadro: muda o "servidor" no tempo certo.
        if (step == 0 && t >= 4600) {
            const char* name = reinterpret_cast<const char*>(Field(F_CMD_NAME));
            if (otherVersion) {
                Check(name == nullptr, "outro samp.dll: nada registrado");
            } else {
                Check(name && strcmp(name, "troktags") == 0, "/troktags registrado no CInput do samp.dll");
            }
            SetBubble(joao.id, "Bom dia [VIP] galera, (( alguem no discord? ))", 0xFFFFFFFF, 30000, 30.0f);
            Mem<DWORD>(g_samp + LAST_PLAYERS_UPDATE) = GetTickCount() - 20000; // o Tab nao pede ha 20 s
            labels = Field(F_LABELS_DRAWN);
            bars = Field(F_BARS_DRAWN);
            ++step;
        }
        if (step == 2 && t >= 5500) {
            present = false; // o desenho do Trok Tags para: as originais tem que voltar sozinhas
            labels = Field(F_LABELS_DRAWN);
            ++step;
        }
        if (step == 3 && t >= 5600) {
            SetBubble(maria.id, "(( ja volto ))", 0xFFC0C0C0, 30000, 30.0f);
            ++step;
        }
        if (step == 5 && t >= 6100) {
            present = true;
            ++step;
        }
        if (step == 6 && t >= 6300) {
            labels = Field(F_LABELS_DRAWN);
            ++step;
        }
        if (step == 8 && t >= 6600) {
            Put<int>(g_dialog, 0x28, 1);
            ++step;
        }
        if (step == 10 && t >= 7000) {
            Put<int>(g_dialog, 0x28, 0);
            chatBefore = Field(F_CHAT_COUNT);
            if (Field(F_CMD_PROC)) {
                reinterpret_cast<CmdProc>(Field(F_CMD_PROC))("");
            }
            ++step;
        }

        // O quadro do jogo, com o SA-MP desenhando dentro da cena.
        exposed->Clear(0, nullptr, D3DCLEAR_TARGET, CLEAR, 1.0f, 0);
        if (SUCCEEDED(exposed->BeginScene())) {
            SampDraws();
            exposed->EndScene();
        }
        if (present) {
            exposed->Present(nullptr, nullptr, nullptr, nullptr);
        }
        for (int i = 0; i < 4; ++i) {
            if (!LabelUsed(i)) {
                Check(false, "texto 3D ficou marcado como vazio depois do desenho do SA-MP");
                Put<BOOL>(g_labelPool, 0xE800 + i * 4, TRUE);
            }
        }

        // Depois do quadro: confere.
        if (step == 1 && t >= 5400) {
            if (takesOver) {
                Check(Field(F_LABELS_DRAWN) == labels, "nametags do SA-MP escondidas (DrawLabel original nao chamado)");
                Check(Field(F_BARS_DRAWN) == bars, "barras de vida do SA-MP escondidas");
                Check(Field(F_SEEN) == 0 && Field(F_SEEN + 12) == 0,
                      "textos 3D presos ao jogador saem do desenho do SA-MP");
                Check(Field(F_OBSTACLE_CALLS) > 0, "teste de parede do SA-MP usado no texto 3D que pede");
            } else {
                Check(Field(F_LABELS_DRAWN) > labels, "nametags com o SA-MP (DrawLabel original chamado)");
                Check(Field(F_BARS_DRAWN) > bars, "barras de vida com o SA-MP");
                Check(Field(F_SEEN) == 1 && Field(F_SEEN + 12) == 1, "textos 3D presos ao jogador com o SA-MP");
            }
            Check(Field(F_SEEN + 4) == 1, "texto 3D do mundo continua com o SA-MP");
            Check(Field(F_SEEN + 8) == 1, "texto 3D de jogador fora de alcance continua com o SA-MP");
            Check(Field(F_POOL_DRAWS) > 0, "CLabelPool::Draw original sempre chamado");
            if (takesBubbles) {
                Check(BubbleExists(joao.id) == 0, "balao passou para o Trok Tags (o SA-MP nao desenha)");
            } else {
                Check(BubbleExists(joao.id) == 1, "balao continua com o SA-MP");
            }
            if (takesOver) {
                Check(Field(F_UPDATE_PLAYERS) == 1, "ping pedido uma vez (o Tab nao pedia ha 20 s)");
            } else {
                Check(Field(F_UPDATE_PLAYERS) == 0, "ping nao pedido");
            }

            Check(Capture("cena.bmp"), "back buffer salvo (cena.bmp)");
            double j = AboveHead(joao, 60, 120), m = AboveHead(maria, 60, 40), a = AboveHead(ana, 100, 160);
            if (takesOver) {
                CheckPct(j > 0.15, "Joao: nametag, textos fixos e balao desenhados em cima da cabeca", j);
                CheckPct(m > 0.15, "Maria: nametag desenhada em cima da cabeca", m);
            } else {
                CheckPct(j < 0.002 && m < 0.002, "nada desenhado em cima das cabecas", j + m);
            }
            CheckPct(a < 0.002, "Ana: nada (o servidor esconde a nametag dela)", a);
            double below = Covered(300, H / 2 + 30, W - 300, H / 2 + 150);
            CheckPct(below < 0.002, "nada abaixo das cabecas", below);
            ++step;
        }
        if (step == 4 && t >= 6000) {
            if (takesOver) {
                Check(Field(F_LABELS_DRAWN) > labels, "sem o Present, as nametags do SA-MP voltam sozinhas");
                Check(Field(F_SEEN) == 1, "sem o Present, os textos 3D presos voltam para o SA-MP");
            }
            Check(BubbleExists(joao.id) == 1 && BubbleExists(maria.id) == 1,
                  takesBubbles ? "sem o Present, os baloes recolhidos voltam para o SA-MP" : "baloes continuam com o SA-MP");
            ++step;
        }
        if (step == 7 && t >= 6500) {
            if (takesOver) {
                Check(Field(F_LABELS_DRAWN) == labels, "Present de volta: nametags do SA-MP escondidas de novo");
            }
            if (takesBubbles) {
                Check(BubbleExists(joao.id) == 0 && BubbleExists(maria.id) == 0,
                      "Present de volta: os baloes passam de novo para o Trok Tags");
            }
            ++step;
        }
        if (step == 9 && t >= 6900) {
            Check(Capture("dialogo.bmp"), "back buffer salvo (dialogo.bmp)");
            double j = AboveHead(joao, 60, 120);
            CheckPct(j < 0.002, "dialogo do servidor aberto: nada em cima das cabecas", j);
            ++step;
        }
        if (step == 11 && t >= 7500) {
            if (otherVersion) {
                ++step;
            } else {
                Check(Capture("menu.bmp"), "back buffer salvo (menu.bmp)");
                double menu = Covered(W / 2 - 150, H / 2 + 30, W / 2 + 150, H / 2 + 150);
                CheckPct(menu > 0.95, "menu aberto no centro da tela", menu);
                Check(int(Field(F_CURSOR_MODE)) == 2, "cursor do SA-MP no modo 2 com o menu aberto");
                Check(Field(F_CHAT_COUNT) == chatBefore, "nenhum aviso de 'desenho nao ativo' no chat");
                reinterpret_cast<CmdProc>(Field(F_CMD_PROC))("");
                ++step;
            }
        }
        if (step == 12 && t >= 8000) {
            if (!otherVersion) {
                Check(Capture("menu_fechado.bmp"), "back buffer salvo (menu_fechado.bmp)");
                double menu = Covered(W / 2 - 150, H / 2 + 30, W / 2 + 150, H / 2 + 150);
                CheckPct(menu < 0.002, "menu fechado no segundo /troktags", menu);
                Check(int(Field(F_CURSOR_MODE)) == 0, "cursor do SA-MP devolvido (modo 0)");
            }
            ++step;
        }
        if (step == 13) {
            break;
        }
        if (t > 20000) {
            Check(false, "tempo esgotado");
            break;
        }
        Sleep(5);
    }

    if (otherVersion) {
        Check(LogHas("nao e o 0.3.7 R1"), "log: samp.dll de outra versao recusado");
    } else {
        Check(LogHas("ganchos do samp.dll instalados"), "log: ganchos do samp.dll instalados");
        Check(LogHas("perfil: 127.0.0.1:7777"), "log: perfil do servidor 127.0.0.1:7777 carregado");
    }
    if (takesBubbles) {
        Check(LogHas("baloes: estrutura conferida"), "log: estrutura dos baloes conferida");
    }
    if (strangeBubbles) {
        Check(LogHas("nao conferiu"), "log: estrutura dos baloes recusada");
    }
    printf(g_failures ? "FALHOU (%d)\n" : "PASSOU\n", g_failures);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), g_failures ? 1 : 0); // sem descarregar o .asi (o jogo tambem nao)
    return 0;
}
