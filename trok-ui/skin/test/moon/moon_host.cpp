// Teste do Trok Skin no imgui antigo do moonloader (moon_imgui 1.1.5: lib\imgui.lua + lib\MoonImGui.dll, com o
// Dear ImGui 1.52) no Wine. Como no moonloader, cada script roda no seu lua_State do LuaJIT (lua51.dll) e o
// imgui.lua de verdade desenha no evento onD3DPresent. O host imita so o que o imgui.lua usa da API do
// moonloader (moonloader\prelude.lua) e salva o retangulo de cada controle e capturas da tela.
//
// Uso: moon_host.exe <saida> [skin] [alternar|cliques]
//   skin      carrega o Trok Skin.asi antes (como o ASI Loader); a config vem do Trok Skin.ini ao lado
//   alternar  carrega tambem um samp.dll falso (0.3.7 R1) e, depois da primeira carga, digita /trokskin
//             duas vezes, salvando a tela depois de cada uma (out_desligado.bmp e out_religado.bmp)
//   cliques   carrega o samp.dll falso e, depois da primeira carga, usa mouse e teclado simulados (mensagens da
//             janela, como o moonloader manda ao onWindowMessage de cada script: o script que segura uma mensagem
//             esconde dos seguintes): no "Painel A", clica no interruptor, arrasta o slider e clica no botao
//             "Salvar"; clica no "Painel A2"
//             e aperta Esc; abre a lista do Painel A e aperta Esc (fecha a lista) e de novo (com o chat do SA-MP
//             aberto e fechado); clica fora e aperta Esc; clica no X do "Painel B". Grava os valores em
//             out_cliques.txt e as telas out_lista.bmp (lista aberta), out_lista_fechada.bmp e out_cliques.bmp
//             (no fim).
//
// Scripts (moonloader\*.lua), os mesmos papeis do teste do mimgui:
//   Trok_Painel_A.lua  "Painel A": tema claro do proprio script e cantos retos; "trok" no nome, sem o kit; abre
//                      tambem a janela "Painel A2", sem botao de fechar
//   painel_b.lua       "Painel B": tema padrao; fundo vermelho empurrado antes do Begin; botao e texto vermelhos
//   hud.lua            "HUD": fundo transparente, sem titulo
//   casa.lua           mod da casa com outro nome (usa resource\\trok, janela ##trokCasa): nada pode mudar nele
// Depois de desenhar, fecha os scripts e carrega de novo (como o Ctrl+R do moonloader) e desenha outra vez.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>

namespace {

constexpr int W = 1280;
constexpr int H = 800;
constexpr int LUA_GLOBALSINDEX = -10002;

struct lua_State;
typedef int(__cdecl* lua_CFunction)(lua_State*);

struct Lua {
    lua_State*(__cdecl* newstate)();
    void(__cdecl* openlibs)(lua_State*);
    void(__cdecl* close)(lua_State*);
    int(__cdecl* loadfile)(lua_State*, const char*);
    int(__cdecl* pcall)(lua_State*, int, int, int);
    const char*(__cdecl* tolstring)(lua_State*, int, size_t*);
    double(__cdecl* tonumber)(lua_State*, int);
    void(__cdecl* settop)(lua_State*, int);
    void(__cdecl* pushcclosure)(lua_State*, lua_CFunction, int);
    void(__cdecl* setfield)(lua_State*, int, const char*);
    void(__cdecl* getfield)(lua_State*, int, const char*);
    void(__cdecl* pushnumber)(lua_State*, double);
    void(__cdecl* pushstring)(lua_State*, const char*);
    int(__cdecl* toboolean)(lua_State*, int);
};

Lua lua;
FILE* g_rects = nullptr;
HWND g_hwnd = nullptr;
IDirect3DDevice9* g_device = nullptr;
bool g_record = false;
char g_fonts[MAX_PATH];

const char* kScripts[] = {"Trok_Painel_A.lua", "painel_b.lua", "hud.lua", "casa.lua"};
constexpr int kCount = 4;

template <typename T> void Get(HMODULE dll, const char* name, T& out) {
    out = reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(dll, name)));
    if (!out) {
        printf("FALHA: lua51.dll sem %s\n", name);
        ExitProcess(2);
    }
}

void LoadLua() {
    HMODULE dll = LoadLibraryA("lua51.dll");
    if (!dll) {
        printf("FALHA: lua51.dll nao carregou\n");
        ExitProcess(2);
    }
    Get(dll, "luaL_newstate", lua.newstate);
    Get(dll, "luaL_openlibs", lua.openlibs);
    Get(dll, "lua_close", lua.close);
    Get(dll, "luaL_loadfile", lua.loadfile);
    Get(dll, "lua_pcall", lua.pcall);
    Get(dll, "lua_tolstring", lua.tolstring);
    Get(dll, "lua_tonumber", lua.tonumber);
    Get(dll, "lua_settop", lua.settop);
    Get(dll, "lua_pushcclosure", lua.pushcclosure);
    Get(dll, "lua_setfield", lua.setfield);
    Get(dll, "lua_getfield", lua.getfield);
    Get(dll, "lua_pushnumber", lua.pushnumber);
    Get(dll, "lua_pushstring", lua.pushstring);
    Get(dll, "lua_toboolean", lua.toboolean);
}

// Onde o botao, o interruptor, o slider e a lista do "Painel A" ficaram no ultimo quadro (x0, y0, x1, y1), para os
// cliques.
float g_save[4], g_check[4], g_slider[4], g_combo[4];

// host_rect(script, controle, x0, y0, x1, y1): retangulo de um controle, so no quadro gravado.
int __cdecl HostRect(lua_State* L) {
    const char* script = lua.tolstring(L, 1, nullptr);
    const char* what = lua.tolstring(L, 2, nullptr);
    if (g_record) {
        fprintf(g_rects, "%s|%s %.2f %.2f %.2f %.2f\n", script, what, lua.tonumber(L, 3), lua.tonumber(L, 4),
                lua.tonumber(L, 5), lua.tonumber(L, 6));
    }
    float* r = nullptr;
    if (strcmp(script, "Painel A") == 0) {
        r = strcmp(what, "salvar") == 0     ? g_save
            : strcmp(what, "checkbox") == 0 ? g_check
            : strcmp(what, "slider") == 0   ? g_slider
            : strcmp(what, "combo") == 0    ? g_combo
                                            : nullptr;
    }
    if (r) {
        for (int i = 0; i < 4; ++i) {
            r[i] = static_cast<float>(lua.tonumber(L, 3 + i));
        }
    }
    return 0;
}

int __cdecl HostPrint(lua_State* L) {
    printf("lua: %s\n", lua.tolstring(L, 1, nullptr));
    fflush(stdout);
    return 0;
}

void Check(lua_State* L, int status, const char* what) {
    if (status != 0) {
        printf("FALHA: %s: %s\n", what, lua.tolstring(L, -1, nullptr));
        fflush(stdout);
        TerminateProcess(GetCurrentProcess(), 1);
    }
}

// Um script do moonloader: lua_State proprio, a API imitada e o arquivo do script.
lua_State* Open(const char* file) {
    lua_State* L = lua.newstate();
    lua.openlibs(L);
    lua.pushnumber(L, static_cast<double>(reinterpret_cast<DWORD>(g_hwnd)));
    lua.setfield(L, LUA_GLOBALSINDEX, "HOST_HWND");
    lua.pushnumber(L, static_cast<double>(reinterpret_cast<DWORD>(g_device)));
    lua.setfield(L, LUA_GLOBALSINDEX, "HOST_DEVICE");
    lua.pushstring(L, g_fonts);
    lua.setfield(L, LUA_GLOBALSINDEX, "HOST_FONTS");
    lua.pushstring(L, file);
    lua.setfield(L, LUA_GLOBALSINDEX, "SCRIPT_FILE");
    lua.pushcclosure(L, HostRect, 0);
    lua.setfield(L, LUA_GLOBALSINDEX, "host_rect");
    lua.pushcclosure(L, HostPrint, 0);
    lua.setfield(L, LUA_GLOBALSINDEX, "host_print");
    Check(L, lua.loadfile(L, "moonloader\\prelude.lua"), "prelude.lua");
    Check(L, lua.pcall(L, 0, 0, 0), "prelude.lua");
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "moonloader\\%s", file);
    Check(L, lua.loadfile(L, path), file);
    Check(L, lua.pcall(L, 0, 0, 0), file);
    return L;
}

void Frame(lua_State** states) {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        DispatchMessageA(&msg);
    }
    g_device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(20, 90, 40), 1.0f, 0);
    g_device->BeginScene();
    // O moonloader chama o onD3DPresent de cada script, na ordem em que carregaram.
    for (int i = 0; i < kCount; ++i) {
        lua.getfield(states[i], LUA_GLOBALSINDEX, "__host_frame");
        Check(states[i], lua.pcall(states[i], 0, 0, 0), kScripts[i]);
    }
    g_device->EndScene();
}

bool Capture(const char* path) {
    IDirect3DSurface9* back = nullptr;
    IDirect3DSurface9* sys = nullptr;
    if (FAILED(g_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) ||
        FAILED(g_device->CreateOffscreenPlainSurface(W, H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, nullptr)) ||
        FAILED(g_device->GetRenderTargetData(back, sys))) {
        return false;
    }
    D3DLOCKED_RECT lr;
    sys->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    FILE* f = fopen(path, "wb");
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
        const DWORD* src = reinterpret_cast<const DWORD*>(static_cast<BYTE*>(lr.pBits) + y * lr.Pitch);
        for (int x = 0; x < W; ++x) {
            row[x * 3] = src[x] & 0xFF;
            row[x * 3 + 1] = (src[x] >> 8) & 0xFF;
            row[x * 3 + 2] = (src[x] >> 16) & 0xFF;
        }
        fwrite(row, sizeof(row), 1, f);
    }
    fclose(f);
    sys->UnlockRect();
    sys->Release();
    back->Release();
    return true;
}

// Uma mensagem da janela do jogo, como o moonloader: o onWindowMessage de cada script, na ordem em que carregaram,
// ate um segurar (consumeWindowMessage). Devolve o script que segurou ou "jogo".
const char* Message(lua_State** states, UINT msg, WPARAM wParam, LPARAM lParam) {
    for (int i = 0; i < kCount; ++i) {
        lua_State* L = states[i];
        lua.getfield(L, LUA_GLOBALSINDEX, "__host_message");
        lua.pushnumber(L, msg);
        lua.pushnumber(L, static_cast<double>(wParam));
        lua.pushnumber(L, static_cast<double>(static_cast<DWORD>(lParam)));
        Check(L, lua.pcall(L, 3, 1, 0), kScripts[i]);
        bool consumed = lua.toboolean(L, -1) != 0;
        lua.settop(L, 0);
        if (consumed) {
            return kScripts[i];
        }
    }
    return "jogo";
}

// Valores de um script (HOST_VALUES dele, ou outra funcao HOST_* que ele definiu).
void Values(FILE* f, lua_State* L, const char* when, const char* which = nullptr) {
    lua.getfield(L, LUA_GLOBALSINDEX, "__host_values");
    if (which) {
        lua.pushstring(L, which);
    }
    Check(L, lua.pcall(L, which ? 1 : 0, 1, 0), "__host_values");
    fprintf(f, "%s %s\n", when, lua.tolstring(L, -1, nullptr));
    lua.settop(L, 0);
}

// Mouse em (x, y) com o botao esquerdo como pedido (WM_MOUSEMOVE e, se mudou, WM_LBUTTON*), e um quadro.
bool g_mouseDown = false;
void MouseAt(lua_State** states, int x, int y, bool down) {
    SetCursorPos(x, y);
    LPARAM pos = MAKELPARAM(x, y);
    Message(states, WM_MOUSEMOVE, g_mouseDown ? MK_LBUTTON : 0, pos);
    if (down != g_mouseDown) {
        Message(states, down ? WM_LBUTTONDOWN : WM_LBUTTONUP, down ? MK_LBUTTON : 0, pos);
        g_mouseDown = down;
    }
    Frame(states);
}

void Click(lua_State** states, int x, int y) {
    MouseAt(states, x, y, false);
    MouseAt(states, x, y, true);
    MouseAt(states, x, y, false);
}

constexpr LPARAM KEY_DOWN = 0x00010001, KEY_REPEAT = 0x40010001, KEY_UP = static_cast<LPARAM>(0xC0010001);

// Modo cliques: o "Painel A" usado com mouse e teclado; o "Painel B" fechado pelo X. Valores em <saida>_cliques.txt.
void Clicks(lua_State** states, HMODULE samp, const char* out) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s_cliques.txt", out);
    FILE* f = fopen(path, "w");
    lua_State* a = states[0];
    Values(f, a, "antes");
    // Interruptor (ou caixa de marcar): clique perto do comeco do item, no meio da altura.
    int cy = static_cast<int>((g_check[1] + g_check[3]) * 0.5f);
    Click(states, static_cast<int>(g_check[0]) + 10, cy);
    Values(f, a, "interruptor");
    // Slider: aperta na ponta esquerda (vai ao minimo), arrasta ate a ponta direita (maximo) e solta.
    int sy = static_cast<int>((g_slider[1] + g_slider[3]) * 0.5f);
    int left = static_cast<int>(g_slider[0]) + 2, right = static_cast<int>(g_slider[2]) - 2;
    MouseAt(states, left, sy, false);
    MouseAt(states, left, sy, true);
    Values(f, a, "slider_apertado");
    MouseAt(states, right, sy, true);
    MouseAt(states, right, sy, false);
    Values(f, a, "slider_solto");
    // Botao: o clique chega ao mod (o botao do kit e o do ImGui com outras medidas).
    Click(states, static_cast<int>((g_save[0] + g_save[2]) * 0.5f), static_cast<int>((g_save[1] + g_save[3]) * 0.5f));
    Values(f, a, "botao", "HOST_SAVED");
    // Clique na janela sem X do mesmo script (o mais recente agora), com o Painel B ainda em foco no ImGui do script
    // dele (o clique no Painel A nunca chegou la): o Esc nao e de nenhum dos dois e vai para o jogo.
    Click(states, 240, 675);
    Frame(states); // no quadro em que o arraste da janela termina, o ImGui ainda segura o teclado
    fprintf(f, "esc_outro_script %s\n", Message(states, WM_KEYDOWN, VK_ESCAPE, KEY_DOWN));
    Frame(states);
    Message(states, WM_KEYUP, VK_ESCAPE, KEY_UP);
    Frame(states);
    Values(f, states[1], "esc_outro_script painel_b");
    // Lista suspensa: o clique abre a lista; o Esc fecha so a lista (a janela fica) e some para o jogo.
    Click(states, static_cast<int>(g_combo[0]) + 20, static_cast<int>((g_combo[1] + g_combo[3]) * 0.5f));
    Frame(states);
    snprintf(path, sizeof(path), "%s_lista.bmp", out);
    Capture(path);
    fprintf(f, "esc_lista %s\n", Message(states, WM_KEYDOWN, VK_ESCAPE, KEY_DOWN));
    Frame(states);
    Message(states, WM_KEYUP, VK_ESCAPE, KEY_UP);
    Frame(states);
    snprintf(path, sizeof(path), "%s_lista_fechada.bmp", out);
    Capture(path);
    Values(f, a, "esc_lista");
    // Esc com o chat do SA-MP aberto (CInput do samp.dll falso): a tecla e do SA-MP, a janela fica.
    BYTE* input = *reinterpret_cast<BYTE**>(reinterpret_cast<BYTE*>(samp) + 0x21A0E8);
    *reinterpret_cast<int*>(input + 0x14E0) = 1;
    Frame(states);
    fprintf(f, "esc_chat %s\n", Message(states, WM_KEYDOWN, VK_ESCAPE, KEY_DOWN));
    Frame(states);
    Message(states, WM_KEYUP, VK_ESCAPE, KEY_UP);
    Frame(states);
    Values(f, a, "esc_chat");
    // Chat fechado e passada a folga de 150 ms: o Esc fecha o Painel A (em foco desde os cliques) e some para o
    // jogo, a repeticao tambem; a tecla solta passa.
    *reinterpret_cast<int*>(input + 0x14E0) = 0;
    Frame(states);
    Sleep(250);
    Frame(states);
    fprintf(f, "esc %s\n", Message(states, WM_KEYDOWN, VK_ESCAPE, KEY_DOWN));
    Frame(states);
    Values(f, a, "esc");
    fprintf(f, "esc_repeticao %s\n", Message(states, WM_KEYDOWN, VK_ESCAPE, KEY_REPEAT));
    Frame(states);
    fprintf(f, "esc_solto %s\n", Message(states, WM_KEYUP, VK_ESCAPE, KEY_UP));
    Frame(states);
    // Clique fora de todas as janelas (nenhuma fica em foco): o Esc vai para o jogo (o menu de pausa abre).
    Click(states, 700, 700);
    fprintf(f, "esc_sem_foco %s\n", Message(states, WM_KEYDOWN, VK_ESCAPE, KEY_DOWN));
    Frame(states);
    Message(states, WM_KEYUP, VK_ESCAPE, KEY_UP);
    Frame(states);
    // X do Painel B (janela em 480,40 de 400x520): cabecalho de 44, X de 28 a 18 da borda, vezes a escala da tela.
    float u = H / 1080.0f * 0.7225f; // escala da casa
    u = u < 0.4675f ? 0.4675f : u;
    Click(states, static_cast<int>(480 + 400 - 26 * u + 0.5f), static_cast<int>(40 + 22 * u + 0.5f));
    Frame(states);
    Values(f, states[1], "x_painel_b");
    fclose(f);
    snprintf(path, sizeof(path), "%s_cliques.bmp", out);
    Capture(path);
}

void Run(lua_State** states) {
    for (int i = 0; i < kCount; ++i) {
        states[i] = Open(kScripts[i]);
    }
    // O primeiro quadro carrega a fonte padrao (Trebuchet Bold 14 com cirilico); os seguintes ja usam.
    for (int f = 0; f < 6; ++f) {
        g_record = f == 5;
        Frame(states);
    }
    g_record = false;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("uso: moon_host.exe <saida> [skin] [alternar]\n");
        return 2;
    }
    const char* out = argv[1];
    bool toggle = argc > 3 && strcmp(argv[3], "alternar") == 0;
    bool clicks = argc > 3 && strcmp(argv[3], "cliques") == 0;
    HMODULE samp = toggle || clicks ? LoadLibraryA("samp.dll") : nullptr;
    if ((toggle || clicks) && !samp) {
        printf("FALHA: samp.dll falso nao carregou\n");
        return 1;
    }
    if (argc > 2 && strcmp(argv[2], "skin") == 0) {
        if (!LoadLibraryA("Trok Skin.asi")) {
            printf("FALHA: Trok Skin.asi nao carregou\n");
            return 1;
        }
        Sleep(500); // o ASI Loader carrega bem antes do moonloader rodar os scripts
    }

    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "MoonHost";
    RegisterClassA(&wc);
    g_hwnd = CreateWindowExA(0, wc.lpszClassName, "moon", WS_POPUP | WS_VISIBLE, 0, 0, W, H, nullptr, nullptr,
                             wc.hInstance, nullptr);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth = W;
    pp.BackBufferHeight = H;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.hDeviceWindow = g_hwnd;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp,
                                 &g_device))) {
        printf("FALHA: device\n");
        return 1;
    }
    GetWindowsDirectoryA(g_fonts, MAX_PATH);
    strcat(g_fonts, "\\Fonts");

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s_rects.txt", out);
    g_rects = fopen(path, "w");

    LoadLua();
    lua_State* states[kCount];
    Run(states);
    snprintf(path, sizeof(path), "%s.bmp", out);
    Capture(path);
    g_device->Present(nullptr, nullptr, nullptr, nullptr);

    if (toggle || clicks) {
        // Campos do samp.dll falso (make_fake_samp.py): nome e callback do comando registrado. Registrado = a skin
        // ja conferiu a versao do SA-MP (e passa a ver o chat e os dialogos dele).
        typedef void(__cdecl * CmdProc)(const char*);
        BYTE* data = reinterpret_cast<BYTE*>(samp) + 0x300000;
        for (int i = 0; i < 100 && !*reinterpret_cast<CmdProc*>(data + 4); ++i) {
            Sleep(50);
        }
        CmdProc proc = *reinterpret_cast<CmdProc*>(data + 4);
        const char* name = *reinterpret_cast<const char**>(data);
        if (!proc || !name || strcmp(name, "trokskin") != 0) {
            printf("FALHA: /trokskin nao registrado\n");
            return 1;
        }
        if (clicks) {
            Clicks(states, samp, out);
        }
    }
    if (toggle) {
        typedef void(__cdecl * CmdProc)(const char*);
        CmdProc proc = *reinterpret_cast<CmdProc*>(reinterpret_cast<BYTE*>(samp) + 0x300004);
        const char* shots[] = {"desligado", "religado"};
        for (const char* shot : shots) {
            proc("");
            for (int f = 0; f < 3; ++f) {
                Frame(states);
            }
            snprintf(path, sizeof(path), "%s_%s.bmp", out, shot);
            Capture(path);
        }
    }

    // Ctrl+R do moonloader: todos os scripts fecham (o LuaJIT solta o MoonImGui.dll) e carregam de novo.
    for (lua_State* L : states) {
        lua.close(L);
    }
    fprintf(g_rects, "--- recarregado\n");
    Run(states);
    snprintf(path, sizeof(path), "%s_recarregado.bmp", out);
    Capture(path);
    fclose(g_rects);
    printf("ok: %s\n", out);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}
