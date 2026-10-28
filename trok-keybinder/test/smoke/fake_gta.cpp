// Smoke test do Trok KeyBinder fora do jogo (Wine 32 bits + Xvfb): um "GTA de mentira" que
//  - deixa os enderecos fixos do gta_sa.exe 1.0 US dentro da propria imagem (array gigante no .bss), com o
//    CSprite::CalcScreenCoors de mentira (camera em (0, 0, 0.8) olhando para +Y) e o CPed do jogador local;
//  - grava um Trok KeyBinder.txt com um preset para 127.0.0.1:7777, carrega o samp.dll falso
//    (make_fake_samp.py) e o .asi antes de existir device, como o ASI Loader, e monta o SA-MP 0.3.7 R1: CNetGame
//    conectado, o jogador local (Victor_Trok) e 3 remotos (Fulano_Silva na frente, no meio da tela; Lucas_Mendes
//    a 2 m; Camilla_Souza a 5 m);
//  - troca o CInput::Send por uma funcao que anota cada mensagem com a hora, e o CChat::AddEntry e o
//    CGame::SetCursorMode por funcoes que fazem o que o SA-MP faria;
//  - aperta teclas na janela (WM_KEYDOWN/UP, o caminho de verdade ate o ImGui) e confere o que o mod manda:
//    tecla (F5), chat aberto (nada), entrar no carro (/tranca e /cinto 1 s depois, com o anti-flood), mensagem
//    no chat (/atender 12, mesmo com cores no meio das palavras), ao redor (F7 -> /algemar 3), a bolinha no
//    Fulano, a lista presa nele (B, e a tecla 2 -> /rg 12), o circulo sem ninguem na mira (clique -> /motor) e o
//    /trokbinder abrindo o menu.
//
// Uso: fake_gta.exe <asi> <cenario>
//   direto        tudo acima
//   outra-versao  samp.dll com o ponto de entrada do R3: o .asi nao faz nada

#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h" // so os valores do ImGuiKey (o .asi grava as teclas assim)

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

struct Vec3 {
    float x, y, z;
};

typedef void(__cdecl* CmdProc)(const char*);
typedef void(__thiscall* AddEntryFn)(void*, int, const char*, const char*, DWORD, DWORD);

constexpr int W = 1600;
constexpr int H = 900;
constexpr D3DCOLOR CLEAR = D3DCOLOR_XRGB(20, 90, 40);
constexpr Vec3 CAMERA = {0.0f, 0.0f, 0.8f};
constexpr float FOCAL = W / 2; // 90 graus na horizontal

// gta_sa.exe 1.0 US
constexpr DWORD GTA_CALC_SCREEN_COORS = 0x70CE30, GTA_THE_CAMERA = 0xB6F028, GTA_PLAYER_PED = 0xB7CD98,
                GTA_CURR_AREA = 0xB72914;

// samp.dll 0.3.7 R1 (os mesmos de src/samp.cpp)
constexpr DWORD NETGAME = 0x21A0F8, DIALOG = 0x21A0B8, SCOREBOARD = 0x21A0B4, LABEL_DRAWER = 0x21A0C0;
constexpr DWORD FN_ADD_ENTRY = 0x64010, FN_SEND = 0x65C60, FN_SET_CURSOR = 0x9BD30;

// Campos do samp.dll falso (make_fake_samp.py), relativos a samp.dll + 0x300000.
constexpr DWORD F_CMD_NAME = 0, F_CMD_PROC = 4, F_NO_OBSTACLES = 0xC, F_OBJECT = 0x100;

int g_failures = 0;
IDirect3DDevice9* g_real = nullptr;
DWORD* g_shot = nullptr; // W * H
DWORD g_samp = 0;
HWND g_hwnd = nullptr;
DWORD g_start = 0;

DWORD& Field(DWORD offset) {
    return Mem<DWORD>(g_samp + 0x300000 + offset);
}

BYTE* Object() {
    return reinterpret_cast<BYTE*>(g_samp + 0x300000 + F_OBJECT);
}

void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHA", what);
    fflush(stdout);
    if (!ok) {
        ++g_failures;
    }
}

// ---------------------------------------------------------------- GTA e SA-MP de mentira

struct GameMatrix {
    float m[16]; // RwMatrix: a posicao fica em +0x30
    BYTE pad[0x20];
};

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

struct Sent {
    std::string text;
    DWORD at;
};
std::vector<Sent> g_sent;
std::vector<std::string> g_chat;
int g_cursorMode = 0;

void __fastcall FakeSend(void*, void*, const char* text) {
    g_sent.push_back({text, GetTickCount() - g_start});
    printf("   enviado em %5lu ms: %s\n", g_sent.back().at, text);
    fflush(stdout);
}

void __fastcall FakeAddEntry(void*, void*, int, const char* text, const char*, DWORD, DWORD) {
    g_chat.push_back(text ? text : "");
}

void __fastcall FakeSetCursor(void* game, void*, int mode, BOOL) {
    Put<int>(game, 0x55, mode);
    g_cursorMode = mode;
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

struct Remote {
    int id;
    const char* name;
    Vec3 pos;
};
const Remote REMOTES[] = {
    {12, "Fulano_Silva", {0.0f, 8.0f, 0.8f}},   // na frente, no meio da tela
    {3, "Lucas_Mendes", {1.5f, 1.5f, 0.8f}},    // a 2,3 m
    {7, "Camilla_Souza", {-4.0f, 3.0f, 0.8f}},  // a 5 m
};
constexpr int REMOTE_COUNT = 3;

GameMatrix g_cameraMatrix, g_myMatrix, g_matrices[REMOTE_COUNT];
alignas(16) BYTE g_me[0x800];
alignas(16) BYTE g_vehicle[0x600];
alignas(16) BYTE g_peds[REMOTE_COUNT][0x800];
alignas(16) BYTE g_netGame[0x400];
alignas(16) BYTE g_pools[0x40];
alignas(16) BYTE g_playerPool[0x2000];
alignas(16) BYTE g_localPlayer[0x200];
alignas(16) BYTE g_infos[REMOTE_COUNT][0x100];
alignas(16) BYTE g_remotes[REMOTE_COUNT][0x200];
alignas(16) BYTE g_sampPeds[REMOTE_COUNT][0x300];
alignas(16) BYTE g_dialog[0x100];
alignas(16) BYTE g_scoreboard[0x40];
alignas(16) BYTE g_labelDrawer[0x40];

void SetMatrixPos(GameMatrix& m, Vec3 p) {
    memset(&m, 0, sizeof(m));
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    m.m[12] = p.x;
    m.m[13] = p.y;
    m.m[14] = p.z;
}

void BuildGame() {
    SetMatrixPos(g_myMatrix, Vec3{0, 0, 0});
    Put<GameMatrix*>(g_me, 0x14, &g_myMatrix);
    Put<float>(g_me, 0x540, 100.0f); // vida
    Put<float>(g_me, 0x548, 0.0f);   // colete
    Put<int>(g_me, 0x530, 1);        // PEDSTATE_IDLE
    Mem<BYTE*>(GTA_PLAYER_PED) = g_me;
    Mem<int>(GTA_CURR_AREA) = 0;
    Put<float>(g_vehicle, 0x4C0, 1000.0f);
}

void SetInVehicle(bool in) {
    BYTE flags = g_me[0x46D];
    g_me[0x46D] = in ? (flags | 1) : (flags & ~1);
    Put<BYTE*>(g_me, 0x58C, in ? g_vehicle : nullptr);
    Put<BYTE*>(g_vehicle, 0x460, in ? g_me : nullptr);
}

void BuildSamp() {
    strcpy(reinterpret_cast<char*>(g_netGame + 0x20), "127.0.0.1");
    strcpy(reinterpret_cast<char*>(g_netGame + 0x121), "Horizonte Roleplay | Servidor 1");
    Put<int>(g_netGame, 0x225, 7777);
    Put<int>(g_netGame, 0x3BD, 14); // GAMESTATE_CONNECTED
    Put<BYTE*>(g_netGame, 0x3CD, g_pools);
    Put<BYTE*>(g_pools, 0x18, g_playerPool);
    Put<WORD>(g_playerPool, 0x04, 0); // o jogador local e o 0
    memcpy(g_playerPool + 0x0A, "Victor_Trok", 12);
    Put<DWORD>(g_playerPool, 0x1A, 11);
    Put<DWORD>(g_playerPool, 0x1E, 15);
    Put<BYTE*>(g_playerPool, 0x22, g_localPlayer);
    Put<BOOL>(g_localPlayer, 0x0C, TRUE); // nasceu
    Put<BOOL>(g_localPlayer, 0x10, FALSE);
    for (int k = 0; k < REMOTE_COUNT; ++k) {
        const Remote& r = REMOTES[k];
        BYTE* info = g_infos[k];
        Put<BYTE*>(g_playerPool, 0x2E + r.id * 4, info);
        Put<BOOL>(g_playerPool, 0xFDE + r.id * 4, TRUE);
        Put<BYTE*>(info, 0x00, g_remotes[k]);
        memcpy(info + 0x0C, r.name, strlen(r.name) + 1);
        Put<DWORD>(info, 0x1C, static_cast<DWORD>(strlen(r.name)));
        Put<DWORD>(info, 0x20, 15);
        SetMatrixPos(g_matrices[k], r.pos);
        Put<GameMatrix*>(g_peds[k], 0x14, &g_matrices[k]);
        Put<BYTE*>(g_sampPeds[k], 0x2A4, g_peds[k]);
        Put<BYTE*>(g_remotes[k], 0x00, g_sampPeds[k]);
    }
    Field(F_NO_OBSTACLES) = 1;
    Mem<BYTE*>(g_samp + DIALOG) = g_dialog;
    Mem<BYTE*>(g_samp + SCOREBOARD) = g_scoreboard;
    Mem<BYTE*>(g_samp + LABEL_DRAWER) = g_labelDrawer;
    Mem<BYTE*>(g_samp + NETGAME) = g_netGame; // por ultimo: com ele o .asi registra o /trokbinder
}

// O arquivo de binds que o jogador ja teria: um preset ligado ao servidor de teste.
void WriteBinds() {
    FILE* f = fopen("Trok KeyBinder.txt", "wb");
    fprintf(f, "[geral]\nligado=1\nmenu_tecla=%d\nintervalo=1200\n\n", static_cast<int>(ImGuiKey_B));
    fprintf(f, "[preset]\nnome=Horizonte Roleplay | Servidor 1\n\n");
    fprintf(f, "[bind]\nclasse=padrao\nnome=Sauda\xC3\xA7\xC3\xA3o\ntexto=oi galera, tudo bem?\ntecla=%d\nmodo=0\n\n",
            static_cast<int>(ImGuiKey_F5));
    fprintf(f, "[bind]\nclasse=padrao\nnome=Motor\ntexto=/motor\nmenu=1\n\n");
    fprintf(f, "[bind]\nclasse=auto\nnome=Trancar\ntexto=/tranca\ncondicao=0\natraso=0\n\n");
    fprintf(f, "[bind]\nclasse=auto\nnome=Cinto\ntexto=/cinto\ncondicao=0\nassento=1\natraso=1000\n\n");
    fprintf(f, "[bind]\nclasse=auto\nnome=Atender\ntexto=/atender (id)\ncondicao=11\n"
               "gatilho=est\xC3\xA1 precisando de atendimento\nespera=5000\n\n");
    fprintf(f, "[bind]\nclasse=redor\nnome=Algemar\ntexto=/algemar (id)\ntecla=%d\nraio=3\nalvo=0\n\n",
            static_cast<int>(ImGuiKey_F7));
    fprintf(f, "[bind]\nclasse=visao\nnome=Revistar\ntexto=/revistar (id)\nmenu=1\n\n");
    fprintf(f, "[bind]\nclasse=visao\nnome=Documentos\ntexto=/rg (id)\nmenu=1\n\n");
    fprintf(f, "[servidor]\nendereco=127.0.0.1:7777\nnome=Horizonte Roleplay | Servidor 1\n"
               "preset=Horizonte Roleplay | Servidor 1\n");
    fclose(f);
}

void Key(WPARAM vk, bool down) {
    UINT scan = MapVirtualKeyA(static_cast<UINT>(vk), 0);
    LPARAM lp = 1 | (scan << 16) | (down ? 0 : (3u << 30));
    PostMessageA(g_hwnd, down ? WM_KEYDOWN : WM_KEYUP, vk, lp);
}

void Mouse(int x, int y, UINT msg) {
    POINT p = {x, y};
    ClientToScreen(g_hwnd, &p);
    SetCursorPos(p.x, p.y);
    PostMessageA(g_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
    if (msg) {
        PostMessageA(g_hwnd, msg, msg == WM_LBUTTONDOWN ? MK_LBUTTON : 0, MAKELPARAM(x, y));
    }
}

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
    if (sys) sys->Release();
    if (back) back->Release();
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

// Fracao de pixels fora do fundo no retangulo; white = so os quase brancos.
double Covered(int x0, int y0, int x1, int y1, bool white = false) {
    int covered = 0, total = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            DWORD p = g_shot[y * W + x] & 0xFFFFFF;
            int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
            covered += white ? (r > 200 && g > 200 && b > 200) : p != (CLEAR & 0xFFFFFF);
            ++total;
        }
    }
    return total ? double(covered) / total : 0.0;
}

bool LogHas(const char* text) {
    FILE* f = fopen("Trok KeyBinder.log", "rb");
    if (!f) {
        return false;
    }
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    return strstr(buf, text) != nullptr;
}

int SentCount(const char* text) {
    int n = 0;
    for (const Sent& s : g_sent) {
        n += s.text == text;
    }
    return n;
}

const Sent* Find(const char* text) {
    for (const Sent& s : g_sent) {
        if (s.text == text) {
            return &s;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("uso: fake_gta.exe <asi> <direto|outra-versao>\n");
        return 2;
    }
    bool otherVersion = strcmp(argv[2], "outra-versao") == 0;
    DWORD lo = reinterpret_cast<DWORD>(g_gtaMemory);
    DWORD hi = lo + sizeof(g_gtaMemory);
    if (lo > GTA_CALC_SCREEN_COORS || hi < 0xC9C060) {
        printf("FALHA: memoria falsa do GTA fora do lugar (0x%08lX..0x%08lX)\n", lo, hi);
        return 2;
    }
    Check(Redirect(GTA_CALC_SCREEN_COORS, reinterpret_cast<void*>(FakeCalcScreenCoors)),
          "CalcScreenCoors de mentira no lugar");
    SetMatrixPos(g_cameraMatrix, CAMERA);
    Mem<GameMatrix*>(GTA_THE_CAMERA + 0x14) = &g_cameraMatrix;
    BuildGame();
    WriteBinds();

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "Grand theft auto San Andreas";
    RegisterClassA(&wc);
    g_hwnd = CreateWindowExA(0, wc.lpszClassName, "GTA: San Andreas", WS_POPUP | WS_VISIBLE, 0, 0, W, H, nullptr,
                             nullptr, wc.hInstance, nullptr);
    SetForegroundWindow(g_hwnd);
    SetFocus(g_hwnd);
    Mem<int>(0xC17044) = W;
    Mem<int>(0xC17048) = H;
    Mem<HWND>(0xC9C05C) = g_hwnd;
    Mem<HWND>(0xC8CF88) = g_hwnd;

    HMODULE samp = LoadLibraryA("samp.dll");
    Check(samp != nullptr, "samp.dll falso carregado");
    if (!samp) {
        return 1;
    }
    g_samp = reinterpret_cast<DWORD>(samp);
    Check(Redirect(g_samp + FN_SEND, reinterpret_cast<void*>(FakeSend)) &&
              Redirect(g_samp + FN_ADD_ENTRY, reinterpret_cast<void*>(FakeAddEntry)) &&
              Redirect(g_samp + FN_SET_CURSOR, reinterpret_cast<void*>(FakeSetCursor)),
          "CInput::Send, CChat::AddEntry e CGame::SetCursorMode de mentira no lugar");
    HMODULE asi = LoadLibraryA(argv[1]);
    Check(asi != nullptr, "asi carregado");
    if (!asi) {
        return 1;
    }
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
    pp.hDeviceWindow = g_hwnd;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    HRESULT hr = d3d ? d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                         &pp, &g_real)
                     : E_FAIL;
    Check(SUCCEEDED(hr) && g_real, "device d3d9 criado");
    if (!g_real) {
        return 1;
    }
    Mem<IDirect3DDevice9*>(0xC97C28) = g_real;

    g_start = GetTickCount();
    int step = 0;
    DWORD mark = 0;
    size_t sentBefore = 0;
    float u = H / 1080.0f * 0.7225f;
    for (;;) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        DWORD t = GetTickCount() - g_start;
        auto at = [&](DWORD ms) { return t >= mark + ms; };
        auto next = [&]() {
            ++step;
            mark = t;
        };

        if (step == 0 && t >= 4600) {
            const char* name = reinterpret_cast<const char*>(Field(F_CMD_NAME));
            if (otherVersion) {
                Check(name == nullptr, "outro samp.dll: nada registrado");
                step = 100;
            } else {
                Check(name && strcmp(name, "trokbinder") == 0, "/trokbinder registrado no CInput do samp.dll");
                Check(LogHas("preset em uso: \"Horizonte Roleplay | Servidor 1\" (127.0.0.1:7777)"),
                      "log: o preset do servidor 127.0.0.1:7777 em uso");
                next();
            }
        }
        // Tecla: F5 -> a frase.
        if (step == 1) { Key(VK_F5, true); next(); }
        if (step == 2 && at(120)) { Key(VK_F5, false); next(); }
        if (step == 3 && at(400)) {
            Check(SentCount("oi galera, tudo bem?") == 1, "F5 mandou \"oi galera, tudo bem?\" uma vez");
            Put<int>(Object(), 0x14E0, 1); // o chat abre
            next();
        }
        // Com o chat aberto, F5 nao dispara (nem quando o chat fecha com ela segura).
        if (step == 4 && at(100)) { Key(VK_F5, true); next(); }
        if (step == 5 && at(120)) { Key(VK_F5, false); next(); }
        if (step == 6 && at(200)) {
            Put<int>(Object(), 0x14E0, 0);
            next();
        }
        if (step == 7 && at(800)) {
            Check(SentCount("oi galera, tudo bem?") == 1, "com o chat aberto a tecla nao dispara");
            sentBefore = g_sent.size();
            SetInVehicle(true); // entra no carro como motorista
            next();
        }
        if (step == 8 && at(3000)) {
            const Sent* tranca = Find("/tranca");
            const Sent* cinto = Find("/cinto");
            Check(tranca && cinto && g_sent.size() == sentBefore + 2, "entrar no carro: /tranca e /cinto");
            if (tranca && cinto) {
                char text[120];
                snprintf(text, sizeof(text), "/cinto depois do /tranca, com o anti-flood (%lu ms)", cinto->at - tranca->at);
                Check(cinto->at > tranca->at && cinto->at - tranca->at >= 1150, text);
            }
            // Mensagem do servidor com cores no meio das palavras e sem acento.
            void* chat = reinterpret_cast<void*>(Mem<DWORD>(g_samp + 0x21A0E4));
            reinterpret_cast<AddEntryFn>(g_samp + FN_ADD_ENTRY)(
                chat, 4, "{FF9900}[SOS]{FFFFFF} Fulano_Silva (12) esta preci{33AA33}sando de atendimento", nullptr,
                0xFFFFFFFF, 0);
            next();
        }
        if (step == 9 && at(1500)) {
            Check(SentCount("/atender 12") == 1, "mensagem no chat: /atender 12");
            Check(!g_chat.empty(), "o AddEntry original continua sendo chamado (o chat do SA-MP recebe)");
            Key(VK_F7, true);
            next();
        }
        if (step == 10 && at(120)) { Key(VK_F7, false); next(); }
        if (step == 11 && at(1500)) {
            Check(SentCount("/algemar 3") == 1, "ao redor (F7, 3 m): /algemar 3 (o Lucas, a 2,3 m)");
            Check(Capture("bolinha.bmp"), "back buffer salvo (bolinha.bmp)");
            double dot = Covered(W / 2 - 5, H / 2 - 5, W / 2 + 5, H / 2 + 5, true);
            double around = Covered(W / 2 - 60, H / 2 + 20, W / 2 + 60, H / 2 + 80);
            char text[120];
            snprintf(text, sizeof(text), "bolinha branca no meio do Fulano (%.0f%%), nada em volta (%.1f%%)", dot * 100,
                     around * 100);
            Check(dot > 0.25 && around < 0.002, text);
            Key('B', true); // com o Fulano na mira: a lista presa nele (toque rapido: fica aberta)
            next();
        }
        if (step == 12 && at(100)) { Key('B', false); next(); }
        if (step == 13 && at(500)) {
            Check(Capture("lista.bmp"), "back buffer salvo (lista.bmp)");
            double panel = Covered(W / 2 + static_cast<int>(40 * u), H / 2 - 10, W / 2 + static_cast<int>(150 * u), H / 2 + 40);
            double other = Covered(W / 2 - 200, H / 2 - 10, W / 2 - 40, H / 2 + 40);
            char text[160];
            snprintf(text, sizeof(text), "lista presa no Fulano, do lado direito da bolinha (%.0f%% / %.1f%% do outro lado)",
                     panel * 100, other * 100);
            Check(panel > 0.85 && other < 0.002, text);
            Check(g_cursorMode == 2, "cursor do SA-MP no modo 2 com a lista aberta");
            Key('2', true); // o numero sutil do segundo item
            next();
        }
        if (step == 14 && at(100)) { Key('2', false); next(); }
        if (step == 15 && at(1500)) {
            Check(SentCount("/rg 12") == 1, "lista: a tecla 2 mandou /rg 12 (Documentos, no Fulano)");
            Check(g_cursorMode == 0, "cursor do SA-MP devolvido (modo 0)");
            Field(F_NO_OBSTACLES) = 0; // uma parede na frente: ninguem na mira
            Key('B', true);
            next();
        }
        if (step == 16 && at(100)) { Key('B', false); next(); }
        if (step == 17 && at(500)) {
            Check(Capture("circulo.bmp"), "back buffer salvo (circulo.bmp)");
            double ring = Covered(W / 2 - 120, H / 2 - 30, W / 2 + 120, H / 2 + 30);
            char text[120];
            snprintf(text, sizeof(text), "sem ninguem na mira: o circulo no meio da tela (%.0f%%)", ring * 100);
            Check(ring > 0.6, text);
            Mouse(W / 2, static_cast<int>(H / 2 - 139 * u), 0); // a fatia de cima: o Motor
            next();
        }
        if (step == 18 && at(200)) { Mouse(W / 2, static_cast<int>(H / 2 - 139 * u), WM_LBUTTONDOWN); next(); }
        if (step == 19 && at(100)) { Mouse(W / 2, static_cast<int>(H / 2 - 139 * u), WM_LBUTTONUP); next(); }
        if (step == 20 && at(1500)) {
            Check(SentCount("/motor") == 1 && SentCount("/revistar 12") == 0,
                  "circulo: o clique na fatia mandou /motor (as binds da visao ficam de fora sem alvo)");
            Check(g_cursorMode == 0, "cursor do SA-MP devolvido (modo 0)");
            reinterpret_cast<CmdProc>(Field(F_CMD_PROC))(""); // /trokbinder
            next();
        }
        if (step == 21 && at(600)) {
            Check(Capture("menu.bmp"), "back buffer salvo (menu.bmp)");
            double menu = Covered(W / 2 - 150, H / 2 - 60, W / 2 + 150, H / 2 + 60);
            char text[120];
            snprintf(text, sizeof(text), "menu aberto no centro da tela (%.0f%%)", menu * 100);
            Check(menu > 0.9, text);
            Check(g_cursorMode == 2, "cursor do SA-MP no modo 2 com o menu aberto");
            sentBefore = g_sent.size();
            Key(VK_F5, true); // com o menu aberto, nenhuma bind dispara
            next();
        }
        if (step == 22 && at(120)) { Key(VK_F5, false); next(); }
        if (step == 23 && at(800)) {
            Check(g_sent.size() == sentBefore, "com o menu aberto a tecla nao dispara");
            reinterpret_cast<CmdProc>(Field(F_CMD_PROC))("");
            next();
        }
        if (step == 24 && at(500)) {
            Check(g_cursorMode == 0, "menu fechado no segundo /trokbinder (cursor modo 0)");
            break;
        }
        if (step == 100 && t >= 6000) {
            Check(g_sent.empty(), "outro samp.dll: nada mandado");
            break;
        }

        // O quadro do jogo.
        g_real->Clear(0, nullptr, D3DCLEAR_TARGET, CLEAR, 1.0f, 0);
        if (SUCCEEDED(g_real->BeginScene())) {
            g_real->EndScene();
        }
        g_real->Present(nullptr, nullptr, nullptr, nullptr);
        if (t > 40000) {
            Check(false, "tempo esgotado");
            break;
        }
        Sleep(5);
    }

    if (otherVersion) {
        Check(LogHas("nao e o 0.3.7 R1"), "log: samp.dll de outra versao recusado");
    } else {
        Check(LogHas("gancho do chat ligado"), "log: gancho do chat ligado");
    }
    printf(g_failures ? "FALHOU (%d)\n" : "PASSOU\n", g_failures);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), g_failures ? 1 : 0); // sem descarregar o .asi (o jogo tambem nao)
    return 0;
}
