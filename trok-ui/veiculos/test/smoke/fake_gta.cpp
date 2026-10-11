// Smoke test do Trok Veiculos fora do jogo (Wine 32 bits + Xvfb): um "GTA de mentira" que
//  - deixa os enderecos fixos do gta_sa.exe 1.0 US dentro da propria imagem (array gigante no .bss), com o
//    CSprite::CalcScreenCoors de mentira (camera em (0, 0, 0.8) olhando para +Y) e a tabela de modelos (caixas);
//  - carrega o samp.dll falso (make_fake_samp.py) e o .asi antes de existir device, como o ASI Loader, e monta o
//    que o Trok Veiculos le do SA-MP 0.3.7 R1: CNetGame conectado em 127.0.0.1:7777, a CVehiclePool com 6 veiculos
//    e o personagem local (dentro do proprio carro); as funcoes do R1 que o .asi chama caem nas funcoes daqui;
//  - confere: placa embaixo de cada veiculo ate 20 m e nao alem, o seu escondido, o atras de parede escondido
//    (e mostrado com "como o /dl"), pneu furado em vermelho, blindagem em azul, fogo em vermelho, dialogo do servidor
//    esconde as placas, "ao dar dano" so mostra o que voce acertou e some depois, e o /trokveiculos abre o menu.
//
// Uso: fake_gta.exe <asi> <cenario>
//   direto        o padrao
//   ao-dar-dano   Trok Veiculos.ini com mostrar=1 e tempo_dano=3
//   como-o-dl     Trok Veiculos.ini com parede=1 e esconder_meu=0
//   outra-versao  samp.dll com o ponto de entrada do R3: o .asi nao faz nada

#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// A imagem vai de 0x400000 ate ~0xDAx000 e cobre 0x70CE30..0xC9C05C. Tem que ser o comeco do .bss: os outros
// buffers grandes vem do VirtualAlloc.
unsigned char g_gtaMemory[0x960000];

template <typename T> T& Mem(DWORD address) {
    return *reinterpret_cast<T*>(address);
}

template <typename T> void Put(void* base, DWORD offset, T value) {
    memcpy(static_cast<BYTE*>(base) + offset, &value, sizeof(T));
}

template <typename T> T Get(const void* base, DWORD offset) {
    T v;
    memcpy(&v, static_cast<const BYTE*>(base) + offset, sizeof(T));
    return v;
}

struct Vec3 {
    float x, y, z;
};

typedef void(__cdecl* CmdProc)(const char*);

constexpr int W = 1600;
constexpr int H = 900;
constexpr D3DCOLOR CLEAR = D3DCOLOR_XRGB(20, 90, 40);
constexpr Vec3 CAMERA = {0.0f, 0.0f, 0.8f};
constexpr float FOCAL = W / 2;

// gta_sa.exe 1.0 US
constexpr DWORD GTA_CALC_SCREEN_COORS = 0x70CE30;
constexpr DWORD GTA_MODEL_INFO = 0xA9B0C8;
constexpr DWORD VT_AUTOMOBILE = 0x871120;

// samp.dll 0.3.7 R1 (os mesmos de src/samp.cpp)
constexpr DWORD NETGAME = 0x21A0F8, DIALOG = 0x21A0B8, SCOREBOARD = 0x21A0B4, LABEL_DRAWER = 0x21A0C0;

// Campos do samp.dll falso (make_fake_samp.py), relativos a samp.dll + 0x300000.
constexpr DWORD F_CMD_NAME = 0, F_CMD_PROC = 4, F_CHAT_COUNT = 8, F_CURSOR_MODE = 0xC, F_OBJECT = 0x14, F_JUMPS = 0x2000;

int g_failures = 0;
IDirect3DDevice9* g_real = nullptr;
DWORD* g_shot = nullptr; // W * H
DWORD g_samp = 0;
int g_wallTests = 0;

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

// ---------------------------------------------------------------- GTA de mentira

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

struct Veh {
    int id;
    int model;
    Vec3 pos; // o centro (o chao fica 0.6 m abaixo)
    float health;
    BYTE wheels[4]; // FE, TE, FD, TD (como o GTA guarda)
    bool occupied, locked, mine;
};

// 0 Sultan (traseiro direito furado), 1 Huntley blindado, 2 Infernus pegando fogo, 3 Premier a 30 m, 4 Premier atras
// de parede (vazio e trancado) e 5 o seu (voce dirige).
Veh g_veh[] = {
    {12, 560, {0.0f, 10.0f, 0.6f}, 1000.0f, {0, 0, 0, 1}, true, false, false},
    {207, 579, {-6.0f, 14.0f, 0.7f}, 5000.0f, {0, 0, 0, 0}, true, false, false},
    {41, 411, {6.0f, 12.0f, 0.55f}, 230.0f, {0, 0, 0, 0}, true, false, false},
    {99, 426, {2.0f, 30.0f, 0.6f}, 1000.0f, {0, 0, 0, 0}, true, false, false},
    {77, 426, {-3.5f, 8.0f, 0.6f}, 900.0f, {0, 0, 0, 0}, false, true, false},
    {5, 560, {0.0f, 4.5f, 0.6f}, 1000.0f, {0, 0, 0, 0}, true, false, true},
};
constexpr int VEH_COUNT = 6;

struct Model {
    int id;
    Vec3 min, max;
};
const Model MODELS[] = {
    {560, {-1.0f, -2.3f, -0.6f}, {1.0f, 2.3f, 0.8f}},
    {579, {-1.1f, -2.5f, -0.7f}, {1.1f, 2.5f, 1.1f}},
    {411, {-1.0f, -2.3f, -0.55f}, {1.0f, 2.3f, 0.65f}},
    {426, {-1.0f, -2.4f, -0.6f}, {1.0f, 2.4f, 0.8f}},
};

BYTE* g_gameVeh[VEH_COUNT];
BYTE* g_sampVeh[VEH_COUNT];
float* g_matrices[VEH_COUNT];
BYTE* g_pool;
BYTE* g_modelInfo[4];
BYTE* g_colModel[4];
BYTE g_netGame[0x400], g_pools[0x40], g_dialog[0x100], g_scoreboard[0x40], g_labelDrawer[0x40];
BYTE g_localSampPed[0x300], g_localGamePed[0x800];

const Model* ModelOf(int id) {
    for (const Model& m : MODELS) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

float DistanceTo(const Vec3& p) {
    return sqrtf(p.x * p.x + p.y * p.y + (p.z - CAMERA.z) * (p.z - CAMERA.z));
}

// As funcoes do samp.dll R1 que o .asi chama (thiscall; aqui fastcall, que arruma a pilha do mesmo jeito).
float __fastcall FakeDistance(BYTE* sv, void*) { return Get<float>(sv, 0x200); }
BOOL __fastcall FakeOccupied(BYTE* sv, void*) { return Get<BOOL>(sv, 0x204); }
BOOL __fastcall FakeWrecked(BYTE* sv, void*) { return Get<BOOL>(sv, 0x208); }
char __fastcall FakeSeats(BYTE* sv, void*) { return Get<char>(sv, 0x20C); }
BOOL __fastcall FakeNoObstacles(void*, void*, Vec3 pos) {
    ++g_wallTests;
    // Uma parede na frente do Premier estacionado a esquerda (x de -4.6 a -2.4, ate 11 m).
    return !(pos.x > -4.6f && pos.x < -2.4f && pos.y > 5.0f && pos.y < 11.0f);
}
BOOL __fastcall FakeInVehicle(BYTE* ped, void*) { return Get<BOOL>(ped, 0x100); }
void* __fastcall FakePedVehicle(BYTE* ped, void*) { return Get<void*>(ped, 0x104); }

void BuildWorld() {
    for (int k = 0; k < 4; ++k) {
        g_colModel[k] = static_cast<BYTE*>(Alloc(0x40));
        g_modelInfo[k] = static_cast<BYTE*>(Alloc(0x40));
        Put<Vec3>(g_colModel[k], 0x00, MODELS[k].min);
        Put<Vec3>(g_colModel[k], 0x0C, MODELS[k].max);
        Put<BYTE*>(g_modelInfo[k], 0x14, g_colModel[k]);
        Mem<BYTE*>(GTA_MODEL_INFO + MODELS[k].id * 4) = g_modelInfo[k];
    }
    g_pool = static_cast<BYTE*>(Alloc(0x7000));
    for (int k = 0; k < VEH_COUNT; ++k) {
        const Veh& v = g_veh[k];
        BYTE* gv = g_gameVeh[k] = static_cast<BYTE*>(Alloc(0x800));
        BYTE* sv = g_sampVeh[k] = static_cast<BYTE*>(Alloc(0x300));
        float* m = g_matrices[k] = static_cast<float*>(Alloc(0x48));
        m[0] = 1.0f;  // right
        m[5] = 1.0f;  // forward
        m[10] = 1.0f; // up
        m[12] = v.pos.x;
        m[13] = v.pos.y;
        m[14] = v.pos.z;
        Put<DWORD>(gv, 0x00, VT_AUTOMOBILE);
        Put<float*>(gv, 0x14, m);
        Put<WORD>(gv, 0x22, static_cast<WORD>(v.model));
        Put<float>(gv, 0x4C0, v.health);
        for (int w = 0; w < 4; ++w) {
            Put<BYTE>(gv, 0x5A5 + w, v.wheels[w]);
        }
        Put<BYTE*>(sv, 0x4C, gv);
        Put<BOOL>(sv, 0x60, v.locked);
        Put<float>(sv, 0x200, DistanceTo(v.pos));
        Put<BOOL>(sv, 0x204, v.occupied);
        Put<BOOL>(sv, 0x208, FALSE);
        Put<char>(sv, 0x20C, 3);
        Put<BYTE*>(g_pool, 0x1134 + v.id * 4, sv);
        Put<BOOL>(g_pool, 0x3074 + v.id * 4, TRUE);
        Put<BYTE*>(g_pool, 0x4FB4 + v.id * 4, gv);
        if (v.mine) {
            Put<BOOL>(g_localSampPed, 0x100, TRUE);
            Put<BYTE*>(g_localSampPed, 0x104, gv);
        }
    }
    Put<BYTE*>(g_localSampPed, 0x2A4, g_localGamePed);
    Put<BYTE>(g_localGamePed, 0x718, 2); // pistola na mao

    strcpy(reinterpret_cast<char*>(g_netGame + 0x20), "127.0.0.1");
    Put<int>(g_netGame, 0x225, 7777);
    Put<int>(g_netGame, 0x3BD, 14); // GAMESTATE_CONNECTED
    Put<BYTE*>(g_netGame, 0x3CD, g_pools);
    Put<BYTE*>(g_pools, 0x1C, g_pool);

    void* fns[] = {reinterpret_cast<void*>(FakeDistance),   reinterpret_cast<void*>(FakeOccupied),
                   reinterpret_cast<void*>(FakeWrecked),    reinterpret_cast<void*>(FakeSeats),
                   reinterpret_cast<void*>(FakeNoObstacles), reinterpret_cast<void*>(FakeInVehicle),
                   reinterpret_cast<void*>(FakePedVehicle)};
    for (int i = 0; i < 7; ++i) {
        Field(F_JUMPS + i * 4) = reinterpret_cast<DWORD>(fns[i]);
    }
    Field(F_OBJECT + 8) = reinterpret_cast<DWORD>(g_localSampPed); // CGame::m_pPlayerPed
    Mem<BYTE*>(g_samp + DIALOG) = g_dialog;
    Mem<BYTE*>(g_samp + SCOREBOARD) = g_scoreboard;
    Mem<BYTE*>(g_samp + LABEL_DRAWER) = g_labelDrawer;
    Mem<BYTE*>(g_samp + NETGAME) = g_netGame; // por ultimo: com ele o .asi registra o /trokveiculos
}

void SetHealth(int k, float health) {
    Put<float>(g_gameVeh[k], 0x4C0, health);
}

void SetFiring(bool on) {
    BYTE slot = Get<BYTE>(g_localGamePed, 0x718);
    Put<int>(g_localGamePed, 0x5A0 + slot * 0x1C + 4, on ? 1 : 0);
}

// Onde a placa do veiculo k tem que aparecer: o meio da borda de baixo da caixa projetada.
POINT Anchor(int k) {
    const Model* m = ModelOf(g_veh[k].model);
    float lx = 1e9f, hx = -1e9f, hy = -1e9f;
    for (int i = 0; i < 8; ++i) {
        Vec3 c = {g_veh[k].pos.x + (i & 1 ? m->max.x : m->min.x), g_veh[k].pos.y + (i & 2 ? m->max.y : m->min.y),
                  g_veh[k].pos.z + (i & 4 ? m->max.z : m->min.z)};
        Vec3 s;
        float w, h;
        if (FakeCalcScreenCoors(c, &s, &w, &h, false, false)) {
            lx = fminf(lx, s.x);
            hx = fmaxf(hx, s.x);
            hy = fmaxf(hy, s.y);
        }
    }
    return {static_cast<LONG>((lx + hx) * 0.5f), static_cast<LONG>(hy)};
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
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < H; ++y) {
        for (int x = x0 < 0 ? 0 : x0; x < x1 && x < W; ++x) {
            covered += (g_shot[y * W + x] & 0xFFFFFF) != (CLEAR & 0xFFFFFF);
            ++total;
        }
    }
    return total ? double(covered) / total : 0.0;
}

enum Tint { RED, BLUE, PINK };

int Tinted(int x0, int y0, int x1, int y1, Tint tint) {
    int n = 0;
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < H; ++y) {
        for (int x = x0 < 0 ? 0 : x0; x < x1 && x < W; ++x) {
            DWORD p = g_shot[y * W + x];
            int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
            if (tint == RED) {
                n += r > 190 && g < 120 && b < 120;
            } else if (tint == BLUE) {
                n += b > 200 && r < 190 && b > r + 50;
            } else {
                n += r > 140 && r > g + 40 && r > b + 40; // o -120, rosado e com borda suavizada
            }
        }
    }
    return n;
}

// A placa do veiculo k (o retangulo logo abaixo dele).
double Card(int k) {
    POINT a = Anchor(k);
    return Covered(a.x - 55, a.y + 6, a.x + 55, a.y + 26);
}

int CardTint(int k, Tint tint) {
    POINT a = Anchor(k);
    return Tinted(a.x - 70, a.y + 2, a.x + 70, a.y + 34, tint);
}

void CheckCard(int k, bool expected, const char* what) {
    char text[200];
    double c = Card(k);
    snprintf(text, sizeof(text), "%s (%.0f%% da placa)", what, c * 100);
    Check(expected ? c > 0.25 : c < 0.01, text);
}

bool LogHas(const char* text) {
    FILE* f = fopen("Trok Veiculos.log", "rb");
    if (!f) {
        return false;
    }
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    return strstr(buf, text) != nullptr;
}

double MenuArea() {
    return Covered(640, 300, 760, 380); // dentro do menu, longe das placas
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("uso: fake_gta.exe <asi> <direto|ao-dar-dano|como-o-dl|outra-versao>\n");
        return 2;
    }
    const char* scenario = argv[2];
    bool onDamage = strcmp(scenario, "ao-dar-dano") == 0;
    bool likeDl = strcmp(scenario, "como-o-dl") == 0;
    bool otherVersion = strcmp(scenario, "outra-versao") == 0;

    DWORD lo = reinterpret_cast<DWORD>(g_gtaMemory);
    DWORD hi = lo + sizeof(g_gtaMemory);
    if (lo > GTA_CALC_SCREEN_COORS || hi < 0xC9C060) {
        printf("FALHA: memoria falsa do GTA fora do lugar (0x%08lX..0x%08lX)\n", lo, hi);
        return 2;
    }
    Check(Redirect(GTA_CALC_SCREEN_COORS, reinterpret_cast<void*>(FakeCalcScreenCoors)), "CalcScreenCoors de mentira no lugar");

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
    BuildWorld();
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

    DWORD start = GetTickCount();
    int step = 0;
    DWORD chatBefore = 0;
    for (;;) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        DWORD t = GetTickCount() - start;

        // Antes do quadro: muda o "jogo" no tempo certo.
        if (step == 0 && t >= 4600) {
            const char* name = reinterpret_cast<const char*>(Field(F_CMD_NAME));
            if (otherVersion) {
                Check(name == nullptr, "outro samp.dll: nada registrado");
            } else {
                Check(name && strcmp(name, "trokveiculos") == 0, "/trokveiculos registrado no CInput do samp.dll");
            }
            ++step;
        }
        if (onDamage) {
            if (step == 2 && t >= 4700) {
                SetFiring(true); // a mira (centro da tela) esta no Sultan
                ++step;
            }
            if (step == 3 && t >= 4800) {
                SetHealth(0, 880.0f);
                ++step;
            }
            if (step == 5 && t >= 5100) {
                SetFiring(false);
                SetHealth(1, 4800.0f); // alguem bateu no Huntley: nao foi voce
                ++step;
            }
        } else if (!otherVersion && !likeDl) {
            if (step == 2 && t >= 5200) {
                Put<int>(g_dialog, 0x28, 1);
                ++step;
            }
            if (step == 4 && t >= 5600) {
                Put<int>(g_dialog, 0x28, 0);
                chatBefore = Field(F_CHAT_COUNT);
                reinterpret_cast<CmdProc>(Field(F_CMD_PROC))("");
                ++step;
            }
        }

        g_real->Clear(0, nullptr, D3DCLEAR_TARGET, CLEAR, 1.0f, 0);
        if (SUCCEEDED(g_real->BeginScene())) {
            g_real->EndScene();
        }
        g_real->Present(nullptr, nullptr, nullptr, nullptr);

        // Depois do quadro: confere.
        if (onDamage) {
            if (step == 1 && t >= 4600) {
                Check(Capture("antes.bmp"), "back buffer salvo (antes.bmp)");
                CheckCard(0, false, "ao dar dano: Sultan sem placa antes do tiro");
                CheckCard(1, false, "ao dar dano: Huntley sem placa");
                ++step;
            }
            if (step == 4 && t >= 5000) {
                Check(Capture("acerto.bmp"), "back buffer salvo (acerto.bmp)");
                CheckCard(0, true, "o seu tiro no Sultan abre a placa dele");
                POINT a = Anchor(0);
                char text[160];
                int pink = Tinted(a.x - 10, a.y - 30, a.x + 80, a.y + 2, PINK);
                snprintf(text, sizeof(text), "o -120 aparece em cima da placa (%d pixels)", pink);
                Check(pink > 8, text);
                CheckCard(1, false, "Huntley continua sem placa");
                ++step;
            }
            if (step == 6 && t >= 5500) {
                Check(Capture("dano_dos_outros.bmp"), "back buffer salvo (dano_dos_outros.bmp)");
                CheckCard(1, false, "dano que voce nao deu nao abre a placa do Huntley");
                ++step;
            }
            if (step == 7 && t >= 8600) {
                Check(Capture("depois.bmp"), "back buffer salvo (depois.bmp)");
                CheckCard(0, false, "3 s sem dano novo: a placa do Sultan some");
                ++step;
            }
            if (step == 8) {
                break;
            }
        } else if (otherVersion || likeDl) {
            if (step == 1 && t >= 5000) {
                Check(Capture("cena.bmp"), "back buffer salvo (cena.bmp)");
                if (otherVersion) {
                    for (int k = 0; k < VEH_COUNT; ++k) {
                        char text[80];
                        snprintf(text, sizeof(text), "outro samp.dll: veiculo %d sem placa", g_veh[k].id);
                        CheckCard(k, false, text);
                    }
                } else {
                    CheckCard(4, true, "como o /dl: o Premier atras da parede ganha placa");
                    CheckCard(5, true, "esconder o meu desligado: o seu carro ganha placa");
                    CheckCard(3, false, "a 30 m continua sem placa (passou dos 20 m)");
                }
                ++step;
            }
            if (step == 2) {
                break;
            }
        } else {
            if (step == 1 && t >= 5000) {
                Check(Capture("cena.bmp"), "back buffer salvo (cena.bmp)");
                CheckCard(0, true, "Sultan a 10 m: placa embaixo dele");
                CheckCard(1, true, "Huntley a 15 m: placa embaixo dele");
                CheckCard(2, true, "Infernus a 13 m: placa embaixo dele");
                CheckCard(3, false, "Premier a 30 m: sem placa (passou dos 20 m do /dl)");
                CheckCard(4, false, "Premier atras da parede: sem placa");
                CheckCard(5, false, "o seu carro: sem placa (esconder o meu)");
                char text[160];
                int red = CardTint(0, RED), blue = CardTint(1, BLUE), fire = CardTint(2, RED), clean = CardTint(0, BLUE);
                snprintf(text, sizeof(text), "Sultan: pneu traseiro direito furado em vermelho (%d pixels)", red);
                Check(red > 3 && red < 60, text);
                snprintf(text, sizeof(text), "Huntley: blindagem em azul (%d pixels)", blue);
                Check(blue > 40, text);
                snprintf(text, sizeof(text), "Infernus: barra vermelha do fogo (%d pixels)", fire);
                Check(fire > 30, text);
                snprintf(text, sizeof(text), "Sultan sem blindagem: nada azul (%d pixels)", clean);
                Check(clean == 0, text);
                Check(g_wallTests > 0, "teste de parede do SA-MP usado");
                ++step;
            }
            if (step == 3 && t >= 5500) {
                Check(Capture("dialogo.bmp"), "back buffer salvo (dialogo.bmp)");
                CheckCard(0, false, "dialogo do servidor aberto: sem placas");
                ++step;
            }
            if (step == 5 && t >= 6100) {
                Check(Capture("menu.bmp"), "back buffer salvo (menu.bmp)");
                double menu = MenuArea();
                char text[120];
                snprintf(text, sizeof(text), "menu aberto no centro da tela (%.0f%%)", menu * 100);
                Check(menu > 0.95, text);
                Check(int(Field(F_CURSOR_MODE)) == 2, "cursor do SA-MP no modo 2 com o menu aberto");
                Check(Field(F_CHAT_COUNT) == chatBefore, "nenhum aviso de 'desenho nao ativo' no chat");
                reinterpret_cast<CmdProc>(Field(F_CMD_PROC))("");
                ++step;
            }
            if (step == 6 && t >= 6600) {
                Check(Capture("menu_fechado.bmp"), "back buffer salvo (menu_fechado.bmp)");
                double menu = MenuArea();
                char text[120];
                snprintf(text, sizeof(text), "menu fechado no segundo /trokveiculos (%.1f%%)", menu * 100);
                Check(menu < 0.002, text);
                Check(int(Field(F_CURSOR_MODE)) == 0, "cursor do SA-MP devolvido (modo 0)");
                ++step;
            }
            if (step == 7) {
                break;
            }
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
        Check(LogHas("/trokveiculos registrado"), "log: /trokveiculos registrado");
        Check(LogHas("perfil: 127.0.0.1:7777"), "log: perfil do servidor 127.0.0.1:7777 carregado");
    }
    printf(g_failures ? "FALHOU (%d)\n" : "PASSOU\n", g_failures);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), g_failures ? 1 : 0); // sem descarregar o .asi (o jogo tambem nao)
    return 0;
}
