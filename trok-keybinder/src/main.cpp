// Trok KeyBinder (.asi) -- Victor_Trok
// Binds do SA-MP 0.3.7 R1 no estilo da casa: tecla (Padrao), condicao (Automatico), jogadores perto (Ao redor) e o
// jogador da mira (Na visao), com o menu circular e um preset por servidor. Liga o ImGui no device do GTA SA (a
// mesma base do Trok Tags: Present/Reset ORIGINAIS do d3d9 desviados com o MinHook, no fim de qualquer corrente),
// registra /trokbinder e roda o motor (engine.cpp) a cada quadro com o estado lido em samp.cpp.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "binds.h"
#include "engine.h"
#include "icons.h"
#include "menu.h"
#include "radial.h"
#include "samp.h"
#include "trok_ui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

void BinderLog(const char* fmt, ...); // Trok KeyBinder.log, ao lado do .asi

namespace {

// Enderecos do gta_sa.exe 1.0 US (os mesmos que os mods da casa usam).
constexpr DWORD GTA_DEVICE = 0xC97C28;      // IDirect3DDevice9* exposto (pode ser o proxy do SA-MP)
constexpr DWORD GTA_D3D_WINDOW = 0xC9C05C;  // hDeviceWindow dos D3DPRESENT_PARAMETERS do RenderWare
constexpr DWORD GTA_WINDOW = 0xC8CF88;      // PsGlobal.window
constexpr DWORD GTA_SCREEN_H = 0xC17048;    // RsGlobal.maximumHeight
constexpr DWORD GTA_MENU_ACTIVE = 0xBA67A4; // menu de pausa aberto

// samp.dll 0.3.7 R1.
constexpr DWORD SAMP_R1_ENTRY = 0x31DF13;
constexpr DWORD SAMP_INFO = 0x21A0F8;
constexpr DWORD SAMP_INPUT = 0x21A0E8;
constexpr DWORD SAMP_MISC = 0x21A10C;
constexpr DWORD SAMP_ADD_COMMAND = 0x65AD0;
constexpr DWORD SAMP_SET_CURSOR = 0x9BD30;
constexpr DWORD SAMP_UNLOCK_CAM = 0x9BC10;

typedef HRESULT(__stdcall* PresentFn)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT(__stdcall* ResetFn)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef IDirect3D9*(WINAPI* Direct3DCreate9Fn)(UINT);
typedef void(__cdecl* CmdProc)(const char*);
typedef void(__thiscall* AddCommandFn)(void*, const char*, CmdProc);
typedef void(__thiscall* SetCursorFn)(void*, int, BOOL);
typedef void(__thiscall* UnlockCamFn)(void*);

PresentFn g_present = nullptr; // trampolim do MinHook (ou slot antigo da vtable no modo reserva)
ResetFn g_reset = nullptr;
void* g_presentTarget = nullptr; // funcoes do d3d9 desviadas (para desligar so elas no modo reserva)
void* g_resetTarget = nullptr;
IDirect3DDevice9* volatile g_gameDevice = nullptr; // device que o GTA apresenta (o real, atras do proxy)
IDirect3DDevice9* g_otherDevice = nullptr;
IDirect3DDevice9* g_imguiDevice = nullptr; // device em que o backend DX9 do ImGui foi montado
WNDPROC g_wndProc = nullptr;
HWND g_window = nullptr;
HMODULE g_module = nullptr;
DWORD g_samp = 0;
volatile bool g_sampR1 = false;  // samp.dll 0.3.7 R1 conferido
volatile bool g_command = false; // /trokbinder registrado
bool g_ready = false;
bool g_initFailed = false;
bool g_cursorOn = false;
bool g_inPresent = false;
bool g_firstDraw = true;
const char* g_hookMode = "nenhum";
volatile LONG g_togglePending = 0;
DWORD g_sampKeyboardTick = 0;
volatile LONG g_presentCalls = 0;
volatile DWORD g_lastPresent = 0;
float g_fontScreenH = 0.0f;
char g_logPath[MAX_PATH] = {};
char g_dataPath[MAX_PATH] = {};

kb::Data g_data;
kb::Engine g_engine;
std::string g_presetKey = "\x01"; // servidor + preset em uso (trocou: o motor recomeca)
bool g_radialKeyWas = false;
kb::Player g_radialTarget; // o alvo de quando o circulo abriu
bool g_radialHasTarget = false;

HMODULE OwnerOf(const void* address) {
    HMODULE module = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCSTR>(address), &module);
    return module;
}

// Nome do modulo dono de um endereco (para o log dizer quem esta na corrente).
const char* ModuleOf(const void* address, char* out, size_t size) {
    HMODULE module = OwnerOf(address);
    if (module && GetModuleFileNameA(module, out, static_cast<DWORD>(size))) {
        const char* slash = strrchr(out, '\\');
        return slash ? slash + 1 : out;
    }
    snprintf(out, size, "fora de qualquer modulo");
    return out;
}

float ScreenHeight() {
    int h = *reinterpret_cast<int*>(GTA_SCREEN_H);
    return h > 1 ? static_cast<float>(h) : 1080.0f;
}

bool PauseMenuOpen() {
    return *reinterpret_cast<BYTE*>(GTA_MENU_ACTIVE) != 0;
}

// Janela do jogo, na mesma ordem do Trok Dialogs: a do device do RenderWare, a da plataforma e,
// por ultimo, a classe da janela do GTA.
HWND GameWindow() {
    HWND candidates[] = {*reinterpret_cast<HWND*>(GTA_D3D_WINDOW), *reinterpret_cast<HWND*>(GTA_WINDOW)};
    for (HWND window : candidates) {
        if (window && IsWindow(window)) {
            return window;
        }
    }
    HWND window = FindWindowA("Grand theft auto San Andreas", nullptr);
    return window && IsWindow(window) ? window : nullptr;
}

bool GameInForeground() {
    HWND game = GameWindow();
    return game && GetForegroundWindow() == game && !IsIconic(game);
}

void SampCursor(bool on) {
    if (!g_command) {
        return;
    }
    void* misc = *reinterpret_cast<void**>(g_samp + SAMP_MISC);
    if (!misc) {
        return;
    }
    // Modo 2 = cursor visivel com camera e controles travados (o mesmo do Trok Dialogs).
    reinterpret_cast<SetCursorFn>(g_samp + SAMP_SET_CURSOR)(misc, on ? 2 : 0, on ? FALSE : TRUE);
    if (!on) {
        reinterpret_cast<UnlockCamFn>(g_samp + SAMP_UNLOCK_CAM)(misc);
    }
}

void __cdecl CmdTrokBinder(const char*) {
    InterlockedExchange(&g_togglePending, 1);
    BinderLog("/trokbinder digitado (imgui %s, %ld quadros, ultimo ha %lu ms, gancho: %s)",
              g_ready ? "pronto" : "NAO pronto", g_presentCalls, GetTickCount() - g_lastPresent, g_hookMode);
    if (!g_ready || GetTickCount() - g_lastPresent > 2000) {
        samp::Notice("{FF8A8A}[Trok KeyBinder]{FFFFFF} o desenho ainda n\xC3\xA3o est\xC3\xA1 ativo. Me envie o arquivo "
                     "{FFD27A}Trok KeyBinder.log{FFFFFF} (mesma pasta do .asi).");
    }
}

// O teclado e do SA-MP? (chat aberto ou dialogo do servidor na tela). Vale tambem por 150 ms depois de o
// chat fechar: o Enter que manda a mensagem nao pode cair no menu nem disparar uma bind.
bool SampOwnsKeyboard() {
    if (g_sampR1 && (samp::ChatOpen() || samp::DialogOpen())) {
        g_sampKeyboardTick = GetTickCount();
        return true;
    }
    return g_sampKeyboardTick && GetTickCount() - g_sampKeyboardTick < 150;
}

bool CapturesInput() {
    return menu::IsOpen() || radial::IsOpen();
}

// O WindowsMouse da casa le esta propriedade: 1 = maozinha, 2 = I de texto, ausente = seta.
void PublishCursor(int request) {
    static int published = -1;
    if (!g_window || request == published) {
        return;
    }
    published = request;
    if (request > 0) {
        SetPropA(g_window, "TrokCursor.Pedido", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(request)));
    } else {
        RemovePropA(g_window, "TrokCursor.Pedido");
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_ready) {
        // Com o chat ou um dialogo do SA-MP aberto, o ImGui nao ve as teclas apertadas (nem as binds): as soltas
        // sempre chegam, para nenhuma tecla ficar presa.
        bool pressed = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_CHAR;
        bool sampKeys = pressed && SampOwnsKeyboard();
        // O Tab e do SA-MP (abre o placar): o ImGui nunca o ve e ele sempre segue para o jogo.
        bool tabKey = wParam == VK_TAB && (pressed || msg == WM_KEYUP || msg == WM_SYSKEYUP);
        if (!sampKeys && !tabKey) {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
        }

        // Com o menu ou o circulo aberto o jogo nao recebe teclas nem cliques. Teclas soltas sempre passam.
        // T e F6 (e o "t" que vem junto) seguem para o SA-MP abrir o chat; o Tab (placar) segue sempre.
        bool typing = ImGui::GetIO().WantTextInput || tui::CapturingKey();
        bool chatKey = (msg == WM_KEYDOWN && (wParam == 'T' || wParam == VK_F6)) ||
                       (msg == WM_CHAR && (wParam == 't' || wParam == 'T'));
        if (CapturesInput() && !PauseMenuOpen() && !sampKeys && !tabKey && !(chatKey && !typing)) {
            switch (msg) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_CHAR:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MOUSEWHEEL:
                return 0;
            case WM_SETCURSOR:
                return TRUE;
            default:
                break;
            }
        }
    }
    return CallWindowProcA(g_wndProc, hwnd, msg, wParam, lParam);
}

// O device que chega no Present e o do jogo? (o Present desviado atende qualquer device do processo)
bool IsGameDevice(IDirect3DDevice9* device) {
    if (device == g_gameDevice) {
        return true;
    }
    if (device == g_otherDevice) {
        return false;
    }
    HWND game = GameWindow();
    if (!game) {
        return false; // a janela ainda nao existe: decide no proximo quadro
    }
    bool mine = device == *reinterpret_cast<IDirect3DDevice9**>(GTA_DEVICE);
    D3DDEVICE_CREATION_PARAMETERS cp = {};
    D3DPRESENT_PARAMETERS pp = {};
    if (!mine && SUCCEEDED(device->GetCreationParameters(&cp)) && cp.hFocusWindow == game) {
        mine = true;
    }
    if (!mine) {
        IDirect3DSwapChain9* chain = nullptr;
        if (SUCCEEDED(device->GetSwapChain(0, &chain)) && chain) {
            mine = SUCCEEDED(chain->GetPresentParameters(&pp)) && pp.hDeviceWindow == game;
            chain->Release();
        }
    }
    if (mine) {
        g_gameDevice = device;
        BinderLog("device do jogo agora e 0x%08X (exposto em 0xC97C28: 0x%08X)", reinterpret_cast<DWORD>(device),
                  *reinterpret_cast<DWORD*>(GTA_DEVICE));
    } else {
        static int ignoredLogs = 0;
        g_otherDevice = device;
        if (ignoredLogs < 5) {
            ++ignoredLogs;
            BinderLog("device 0x%08X ignorado (foco 0x%08X, janela 0x%08X; a do jogo e 0x%08X)",
                      reinterpret_cast<DWORD>(device), reinterpret_cast<DWORD>(cp.hFocusWindow),
                      reinterpret_cast<DWORD>(pp.hDeviceWindow), reinterpret_cast<DWORD>(game));
        }
    }
    return mine;
}

void GameDir(char* out) {
    GetModuleFileNameA(nullptr, out, MAX_PATH);
    char* slash = strrchr(out, '\\');
    if (slash) {
        slash[1] = 0;
    } else {
        out[0] = 0;
    }
}

void BuildFonts(float screenH) {
    char dir[MAX_PATH] = {};
    GameDir(dir);
    tui::BuildFonts(screenH, dir);
    ImGui::GetIO().Fonts->Build();
}

void InitImGui(IDirect3DDevice9* device) {
    g_window = GameWindow();
    D3DDEVICE_CREATION_PARAMETERS cp = {};
    if (!g_window && SUCCEEDED(device->GetCreationParameters(&cp)) && cp.hFocusWindow && IsWindow(cp.hFocusWindow)) {
        g_window = cp.hFocusWindow;
    }
    if (!g_window) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            BinderLog("imgui: janela do jogo ainda nao encontrada -- tento de novo a cada quadro");
        }
        return;
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.MouseDrawCursor = false;

    if (!ImGui_ImplWin32_Init(g_window) || !ImGui_ImplDX9_Init(device)) {
        BinderLog("imgui: backend falhou -- desistindo");
        g_initFailed = true;
        return;
    }
    g_imguiDevice = device;

    g_fontScreenH = ScreenHeight();
    BuildFonts(g_fontScreenH);

    g_wndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    if (!g_wndProc) {
        BinderLog("imgui: SetWindowLongPtr falhou -- sem entrada");
        g_initFailed = true;
        return;
    }

    g_ready = true;
    BinderLog("imgui pronto (device 0x%08X, janela 0x%08X, tela de %.0f px)", reinterpret_cast<DWORD>(device),
              reinterpret_cast<DWORD>(g_window), g_fontScreenH);
}

void Save() {
    if (g_dataPath[0] && !kb::SaveFile(g_dataPath, g_data)) {
        BinderLog("nao consegui gravar %s", g_dataPath);
    }
}

// O preset em uso: o do servidor (criado com o nome dele na primeira vez) ou, fora de um servidor, o ultimo.
int CurrentPreset(const samp::ServerInfo& server) {
    if (server.connected && !server.address.empty()) {
        size_t before = g_data.presets.size();
        int p = kb::LinkServer(g_data, server.address, server.hostname);
        if (g_data.presets.size() != before || g_data.lastPreset != g_data.presets[p].name) {
            g_data.lastPreset = g_data.presets[p].name;
            Save();
            if (g_data.presets.size() != before) {
                BinderLog("servidor novo %s (%s): preset \"%s\" criado", server.address.c_str(), server.hostname.c_str(),
                          g_data.presets[p].name.c_str());
            }
        }
        return p;
    }
    int p = kb::FindPreset(g_data, g_data.lastPreset);
    if (p < 0) {
        if (g_data.presets.empty()) {
            kb::NewPreset(g_data, "Meu preset", false, -1);
        }
        p = 0;
    }
    return p;
}

// As teclas para o motor: o estado que o ImGui ja recebe (o WndProc entrega tudo, menos com o chat aberto).
class ImGuiKeys : public kb::Keys {
public:
    bool Down(int key) const override {
        return key > 0 && ImGui::IsKeyDown(static_cast<ImGuiKey>(key));
    }
    bool ModsOk(const kb::Hotkey& h) const override {
        ImGuiKey k = static_cast<ImGuiKey>(h.key);
        if (k >= ImGuiKey_LeftCtrl && k <= ImGuiKey_RightSuper) {
            return true; // o proprio modificador como tecla
        }
        ImGuiIO& io = ImGui::GetIO();
        int now = (io.KeyCtrl ? kb::HK_CTRL : 0) | (io.KeyShift ? kb::HK_SHIFT : 0) | (io.KeyAlt ? kb::HK_ALT : 0);
        return now == h.mods;
    }
};

bool HasViewBinds(const kb::Preset& p) {
    for (const kb::Bind& b : p.binds) {
        if (b.cls == kb::CLASS_VIEW && b.enabled && (b.inMenu || !b.key.Empty())) {
            return true;
        }
    }
    return false;
}

// As binds da visao marcadas "No menu" (a lista presa no jogador).
std::vector<radial::Item> ViewItems(const kb::Preset& p) {
    std::vector<radial::Item> items;
    for (size_t i = 0; i < p.binds.size(); ++i) {
        const kb::Bind& b = p.binds[i];
        if (b.enabled && b.inMenu && b.cls == kb::CLASS_VIEW) {
            radial::Item it;
            it.label = b.name.empty() ? kb::Lines(b.text).empty() ? std::string() : kb::Lines(b.text)[0] : b.name;
            it.bind = static_cast<int>(i);
            it.icon = icons::Find(b.icon);
            items.push_back(it);
        }
    }
    return items;
}

std::vector<radial::Item> RadialItems(const kb::Preset& p, bool target) {
    std::vector<radial::Item> items;
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t i = 0; i < p.binds.size(); ++i) {
            const kb::Bind& b = p.binds[i];
            if (!b.enabled || !b.inMenu || b.cls == kb::CLASS_AUTO) {
                continue;
            }
            bool view = b.cls == kb::CLASS_VIEW;
            if (view != (pass == 0) || (view && !target)) {
                continue;
            }
            radial::Item it;
            it.label = b.name.empty() ? kb::Lines(b.text).empty() ? std::string() : kb::Lines(b.text)[0] : b.name;
            it.tag = view ? std::string() : b.cls == kb::CLASS_AROUND ? std::string("Ao redor") : std::string();
            it.bind = static_cast<int>(i);
            it.icon = icons::Find(b.icon);
            items.push_back(it);
        }
    }
    return items;
}

void RenderOverlay(IDirect3DDevice9* device) {
    if (!g_ready && !g_initFailed) {
        InitImGui(device);
    }
    if (!g_ready) {
        return;
    }
    if (device != g_imguiDevice) {
        BinderLog("device trocou (0x%08X -> 0x%08X) -- remontando o ImGui", reinterpret_cast<DWORD>(g_imguiDevice),
                  reinterpret_cast<DWORD>(device));
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplDX9_Init(device);
        g_imguiDevice = device;
    }
    if (device->TestCooperativeLevel() != D3D_OK) {
        return;
    }
    // Todo quadro, para a folga de 150 ms valer mesmo se o Enter que fecha o chat vier depois de uma pausa.
    bool sampKeyboard = SampOwnsKeyboard();
    DWORD now = GetTickCount();

    samp::ServerInfo server = g_sampR1 ? samp::Server() : samp::ServerInfo();
    int presetIndex = CurrentPreset(server);
    std::string key = server.address + '\x1f' + g_data.presets[presetIndex].name;
    if (key != g_presetKey) {
        g_presetKey = key;
        g_engine.Reset();
        radial::Close();
        BinderLog("preset em uso: \"%s\" (%s)", g_data.presets[presetIndex].name.c_str(),
                  server.connected ? server.address.c_str() : "fora de um servidor");
    }

    if (InterlockedExchange(&g_togglePending, 0)) {
        if (PauseMenuOpen()) {
            BinderLog("/trokbinder ignorado: menu de pausa aberto");
        } else {
            menu::Toggle();
            radial::Close();
            BinderLog("menu %s", menu::IsOpen() ? "aberto" : "fechado");
        }
    }

    float screenH = ScreenHeight();
    if (screenH != g_fontScreenH) {
        BinderLog("resolucao mudou (%.0f -> %.0f px) -- remontando as fontes", g_fontScreenH, screenH);
        g_fontScreenH = screenH;
        ImGui_ImplDX9_InvalidateDeviceObjects();
        BuildFonts(screenH);
        ImGui_ImplDX9_CreateDeviceObjects();
    }

    bool paused = PauseMenuOpen();
    if (paused) {
        radial::Close();
    }
    bool wantCursor = CapturesInput() && !paused;
    if (wantCursor) {
        SampCursor(true); // todo quadro: chat e outros mods tentam devolver o cursor
    } else if (g_cursorOn) {
        SampCursor(false);
    }
    g_cursorOn = wantCursor;

    // O ImGui roda todo quadro (mesmo sem nada na tela) para a fila de entrada nao acumular e as teclas valerem.
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    float time = static_cast<float>(ImGui::GetTime());
    bool foreground = GameInForeground();
    bool draw = false;

    // O mundo deste quadro.
    const kb::Preset& preset = g_data.presets[presetIndex];
    const kb::Settings& set = g_data.settings;
    kb::World w;
    w.now = now;
    w.sendReady = g_sampR1 && server.connected && samp::InputReady();
    if (g_sampR1) {
        w.me = samp::ReadMe(server.connected);
        samp::Players(now, &w.players);
        samp::TakeChat(&w.chat);
    }
    bool otherCursor = g_sampR1 && !wantCursor && samp::CursorMode() != 0; // menu de outro mod ou textdraw
    bool covered = sampKeyboard || paused || otherCursor || (g_sampR1 && samp::ScoreboardOpen());
    w.blocked = covered || CapturesInput() || !foreground;
    ImVec2 dot;
    if (w.sendReady && set.enabled && !w.me.dead && HasViewBinds(preset)) {
        w.hasTarget = samp::FindTarget(static_cast<float>(set.viewDistance), io.DisplaySize.x, io.DisplaySize.y,
                                       &w.target, &dot);
    }

    // O circulo: o atalho abre (fora de chat, dialogo, pausa e menus); de novo, fecha.
    ImGuiKeys keys;
    bool radialDown = set.radial.key > 0 && keys.Down(set.radial.key);
    bool radialPressed = radialDown && !g_radialKeyWas && keys.ModsOk(set.radial);
    g_radialKeyWas = radialDown;
    if (radialPressed && !radial::IsOpen() && !menu::IsOpen() && !covered && foreground && set.enabled) {
        g_radialHasTarget = w.hasTarget;
        g_radialTarget = w.target;
        std::string title = w.hasTarget ? w.target.nick : std::string("Binds");
        std::string sub = w.hasTarget ? std::to_string(w.target.id) : std::string(); // so o numero, ao lado do nick
        std::vector<radial::Item> view = ViewItems(preset);
        if (w.hasTarget && set.viewList && !view.empty()) {
            radial::OpenList(view, title, sub, time); // presa no jogador, saindo da bolinha
        } else {
            radial::Open(RadialItems(preset, w.hasTarget), title, sub, time);
        }
        radialPressed = false;
    }

    g_engine.Frame(preset, set, w, foreground ? &keys : nullptr);

    if (radial::IsOpen()) {
        float size = static_cast<float>(set.radialSize);
        int chosen;
        if (radial::ListOpen()) {
            ImVec2 anchor;
            bool visible = g_sampR1 && samp::PlayerDot(g_radialTarget.id, set.viewDistance * 1.5f, io.DisplaySize.x,
                                                         io.DisplaySize.y, &anchor);
            chosen = radial::ListFrame(ImGui::GetForegroundDrawList(), anchor, visible, time, radialDown, radialPressed,
                                       size);
        } else {
            ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            chosen = radial::Frame(ImGui::GetForegroundDrawList(), center, time, radialDown, radialPressed, size);
        }
        if (chosen >= 0 && chosen < static_cast<int>(preset.binds.size())) {
            const kb::Bind& b = preset.binds[chosen];
            BinderLog("menu de binds: \"%s\"", b.name.c_str());
            g_engine.Run(b, w, b.cls == kb::CLASS_VIEW && g_radialHasTarget ? &g_radialTarget : nullptr);
        }
        draw = true;
    } else if (w.hasTarget && set.viewDot && !menu::IsOpen() && !covered) {
        radial::DrawDot(ImGui::GetBackgroundDrawList(), dot, time);
        draw = true;
    }

    // A fila: uma mensagem por vez, no intervalo (nunca com a pausa aberta).
    std::string message;
    if (!paused && w.sendReady && g_engine.Next(now, set.flood, &message)) {
        samp::Send(message);
    }
    for (const kb::Notice& n : g_engine.TakeNotices()) {
        tui::Toast(n.title.c_str(), n.text.c_str());
    }

    if (menu::IsOpen() && !paused) {
        menu::Context c;
        c.data = &g_data;
        c.preset = presetIndex;
        c.connected = server.connected;
        c.address = server.address;
        c.hostname = server.hostname;
        c.players = w.players;
        if (menu::Frame(c)) {
            Save();
            g_engine.ResetKeys();
        }
        draw = true;
    }
    if (tui::ToastsVisible() && !paused) {
        tui::DrawToasts();
        draw = true;
    }
    ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
    ImGui::EndFrame();
    if (draw) {
        ImGui::Render();
        // No Present a cena do jogo ja foi fechada: o desenho do ImGui precisa da sua propria cena.
        HRESULT scene = device->BeginScene();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        if (SUCCEEDED(scene)) {
            device->EndScene();
        }
        if (g_firstDraw) {
            g_firstDraw = false;
            BinderLog("primeiro quadro desenhado (%d listas, BeginScene 0x%08X)", ImGui::GetDrawData()->CmdListsCount,
                      static_cast<unsigned>(scene));
        }
    }
    PublishCursor(!wantCursor ? 0 : cursor == ImGuiMouseCursor_Hand ? 1 : cursor == ImGuiMouseCursor_TextInput ? 2 : 0);
}

HRESULT __stdcall HookPresent(IDirect3DDevice9* device, const RECT* src, const RECT* dst, HWND wnd, const RGNDATA* dirty) {
    // g_inPresent: se outra camada nos chamar de novo dentro do mesmo Present, nao roda duas vezes.
    if (!g_inPresent && IsGameDevice(device)) {
        g_inPresent = true;
        if (InterlockedIncrement(&g_presentCalls) == 1) {
            BinderLog("primeiro Present recebido (device 0x%08X, gancho: %s)", reinterpret_cast<DWORD>(device), g_hookMode);
        }
        g_lastPresent = GetTickCount();
        RenderOverlay(device);
        g_inPresent = false;
    }
    return g_present(device, src, dst, wnd, dirty);
}

HRESULT __stdcall HookReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
    bool mine = g_ready && device == g_imguiDevice;
    if (mine) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
    HRESULT result = g_reset(device, params);
    if (mine && SUCCEEDED(result)) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    return result;
}

// Endereco ORIGINAL de um slot da vtable, lido do arquivo em disco do modulo dono dela. Os outros mods
// trocam o slot na memoria, entao o valor ao vivo pode ser o gancho de alguem.
void* PristineSlot(HMODULE live, void** slot) {
    BYTE* base = reinterpret_cast<BYTE*>(live);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    DWORD size = nt->OptionalHeader.SizeOfImage;
    DWORD rva = static_cast<DWORD>(reinterpret_cast<BYTE*>(slot) - base);
    if (rva + sizeof(DWORD) > size) {
        return nullptr;
    }
    char path[MAX_PATH];
    if (!GetModuleFileNameA(live, path, MAX_PATH)) {
        return nullptr;
    }
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    void* result = nullptr;
    HANDLE map = CreateFileMappingA(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    if (map) {
        BYTE* view = static_cast<BYTE*>(MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0));
        if (view) {
            auto vnt = reinterpret_cast<IMAGE_NT_HEADERS*>(view + reinterpret_cast<IMAGE_DOS_HEADER*>(view)->e_lfanew);
            DWORD value = *reinterpret_cast<DWORD*>(view + rva);
            DWORD liveBase = reinterpret_cast<DWORD>(base);
            DWORD viewBase = reinterpret_cast<DWORD>(view);
            DWORD preferred = vnt->OptionalHeader.ImageBase;
            // A copia pode vir realocada para a base ao vivo (ASLR do sistema), para a propria vista ou
            // nao realocada (base preferida). Em todo caso o destino tem que cair dentro do modulo.
            if (value >= liveBase && value < liveBase + size) {
                result = reinterpret_cast<void*>(value);
            } else if (value >= viewBase && value < viewBase + size) {
                result = reinterpret_cast<void*>(liveBase + (value - viewBase));
            } else if (value >= preferred && value < preferred + size) {
                result = reinterpret_cast<void*>(liveBase + (value - preferred));
            }
            UnmapViewOfFile(view);
        }
        CloseHandle(map);
    }
    CloseHandle(file);
    return result;
}

// O d3d9.dll do sistema (num GTA 32 bits o Windows redireciona para o SysWOW64).
HMODULE SystemD3D9() {
    char path[MAX_PATH];
    UINT n = GetSystemDirectoryA(path, MAX_PATH - 10);
    if (!n || n >= MAX_PATH - 10) {
        return nullptr;
    }
    strcat(path, "\\d3d9.dll");
    return LoadLibraryA(path);
}

// O device de verdade atras de proxies (o SA-MP troca o ponteiro do jogo por um proxy): o back buffer
// nao passa pelo proxy e devolve o device real. E o mesmo caminho que o Trok Dialogs usa.
IDirect3DDevice9* RealDevice(IDirect3DDevice9* exposed) {
    IDirect3DSurface9* surface = nullptr;
    IDirect3DDevice9* real = nullptr;
    if (SUCCEEDED(exposed->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface)) && surface) {
        if (SUCCEEDED(surface->GetDevice(&real)) && real) {
            real->Release();
        }
        surface->Release();
    }
    return real ? real : exposed;
}

// Present (17) e Reset (16) originais de uma vtable que mora no modulo indicado.
bool SlotsFromVtable(HMODULE owner, void** vtable, void** present, void** reset) {
    *present = PristineSlot(owner, &vtable[17]);
    *reset = PristineSlot(owner, &vtable[16]);
    if ((!*present || !*reset) && OwnerOf(vtable[17]) == owner && OwnerOf(vtable[16]) == owner) {
        // Sem o arquivo: usa o valor ao vivo, que ainda e do proprio modulo.
        *present = vtable[17];
        *reset = vtable[16];
    }
    return *present && *reset;
}

// Device NULLREF descartavel no d3d9.dll do sistema: so quando a vtable do device do jogo nao da para
// ler de um arquivo (o device real ficou escondido atras de um wrapper).
bool SlotsFromDummy(HMODULE d3d9, void** present, void** reset) {
    auto create = reinterpret_cast<Direct3DCreate9Fn>(reinterpret_cast<void*>(GetProcAddress(d3d9, "Direct3DCreate9")));
    IDirect3D9* d3d = create ? create(D3D_SDK_VERSION) : nullptr;
    if (!d3d) {
        BinderLog("Direct3DCreate9 do sistema falhou");
        return false;
    }
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = g_module;
    wc.lpszClassName = "TrokKeyBinderD3D";
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, g_module, nullptr);

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.hDeviceWindow = hwnd;
    IDirect3DDevice9* device = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hwnd,
                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &pp, &device);
    bool ok = false;
    if (SUCCEEDED(hr) && device) {
        void** vtable = *reinterpret_cast<void***>(device);
        ok = OwnerOf(vtable) == d3d9 && SlotsFromVtable(d3d9, vtable, present, reset);
        device->Release();
    } else {
        BinderLog("device descartavel do d3d9 falhou (0x%08X)", static_cast<unsigned>(hr));
    }
    d3d->Release();
    if (hwnd) {
        DestroyWindow(hwnd);
    }
    UnregisterClassA(wc.lpszClassName, g_module);
    return ok;
}

bool HookD3D9(IDirect3DDevice9* real) {
    void** vtable = *reinterpret_cast<void***>(real);
    HMODULE owner = OwnerOf(vtable);
    void* present = nullptr;
    void* reset = nullptr;
    bool found = owner && SlotsFromVtable(owner, vtable, &present, &reset);
    if (!found) {
        HMODULE d3d9 = SystemD3D9();
        BinderLog("a vtable do device real nao veio de um arquivo -- procurando com um device descartavel");
        found = d3d9 && SlotsFromDummy(d3d9, &present, &reset);
    }
    if (!found) {
        BinderLog("Present original nao encontrado");
        return false;
    }
    MH_STATUS a = MH_CreateHook(present, reinterpret_cast<void*>(HookPresent), reinterpret_cast<void**>(&g_present));
    MH_STATUS b = MH_CreateHook(reset, reinterpret_cast<void*>(HookReset), reinterpret_cast<void**>(&g_reset));
    g_hookMode = "Present desviado"; // antes de ligar: o primeiro Present pode chegar antes do proximo log
    MH_STATUS d = MH_UNKNOWN;
    if (a == MH_OK && b == MH_OK) {
        d = MH_EnableHook(present);
        if (d == MH_OK) {
            d = MH_EnableHook(reset);
        }
    }
    if (d != MH_OK) {
        BinderLog("MinHook recusou o desvio (Present %d, Reset %d, ligar %d)", a, b, d);
        MH_RemoveHook(present);
        MH_RemoveHook(reset);
        g_hookMode = "nenhum";
        g_present = nullptr;
        g_reset = nullptr;
        return false;
    }
    g_presentTarget = present;
    g_resetTarget = reset;
    char who[MAX_PATH];
    BinderLog("Present e Reset desviados em %s (Present 0x%08X)", ModuleOf(present, who, sizeof(who)),
              reinterpret_cast<DWORD>(present));
    return true;
}

// Reserva, se o Present desviado nunca for chamado: troca os slots da vtable do device exposto. So os
// desvios do d3d9 saem (os do samp.dll continuam).
bool HookVtable(IDirect3DDevice9* device) {
    for (void* target : {g_presentTarget, g_resetTarget}) {
        if (target) {
            MH_DisableHook(target);
            MH_RemoveHook(target);
        }
    }
    g_presentTarget = g_resetTarget = nullptr;
    void** vtable = *reinterpret_cast<void***>(device);
    DWORD old;
    if (!VirtualProtect(vtable, sizeof(void*) * 18, PAGE_EXECUTE_READWRITE, &old)) {
        BinderLog("vtable nao pode ser alterada -- sem desenho");
        return false;
    }
    g_reset = reinterpret_cast<ResetFn>(vtable[16]);
    g_present = reinterpret_cast<PresentFn>(vtable[17]);
    g_gameDevice = device;
    vtable[16] = reinterpret_cast<void*>(HookReset);
    vtable[17] = reinterpret_cast<void*>(HookPresent);
    VirtualProtect(vtable, sizeof(void*) * 18, old, &old);
    g_hookMode = "vtable";
    BinderLog("reserva: Present e Reset trocados na vtable do device 0x%08X", reinterpret_cast<DWORD>(device));
    return true;
}

bool IsSampR1(DWORD base) {
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return nt->OptionalHeader.AddressOfEntryPoint == SAMP_R1_ENTRY;
}

// Espera o samp.dll, confere a versao, liga o gancho do chat e registra o /trokbinder.
void SetupSamp() {
    for (int i = 0; i < 300 && !g_samp; ++i) {
        g_samp = reinterpret_cast<DWORD>(GetModuleHandleA("samp.dll"));
        if (!g_samp) {
            Sleep(100);
        }
    }
    if (!g_samp) {
        BinderLog("samp.dll nao apareceu (single player?) -- o Trok KeyBinder so funciona no SA-MP");
        return;
    }
    if (!IsSampR1(g_samp)) {
        BinderLog("samp.dll nao e o 0.3.7 R1 -- o Trok KeyBinder fica desligado");
        return;
    }
    if (!samp::Install(g_samp)) {
        return;
    }
    g_sampR1 = true;
    for (int i = 0; i < 1200; ++i) {
        void* input = *reinterpret_cast<void**>(g_samp + SAMP_INPUT);
        void* info = *reinterpret_cast<void**>(g_samp + SAMP_INFO);
        if (input && info) {
            reinterpret_cast<AddCommandFn>(g_samp + SAMP_ADD_COMMAND)(input, "trokbinder", CmdTrokBinder);
            g_command = true;
            BinderLog("/trokbinder registrado (CInput 0x%08X)", reinterpret_cast<DWORD>(input));
            return;
        }
        Sleep(100);
    }
    BinderLog("CInput do SA-MP nao apareceu -- sem /trokbinder");
}

DWORD WINAPI Boot(LPVOID) {
    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        BinderLog("MinHook nao iniciou (%d) -- desistindo", init);
        return 0;
    }
    IDirect3DDevice9* exposed = nullptr;
    for (int i = 0; i < 1200 && !exposed; ++i) {
        exposed = *reinterpret_cast<IDirect3DDevice9**>(GTA_DEVICE);
        if (!exposed) {
            Sleep(100);
        }
    }
    if (!exposed) {
        BinderLog("o render nao subiu em 2 minutos -- desistindo");
        return 0;
    }
    // Como o Trok Dialogs: da tempo para o SA-MP e o moonloader montarem os proxies e ganchos deles.
    Sleep(1500);
    exposed = *reinterpret_cast<IDirect3DDevice9**>(GTA_DEVICE);
    IDirect3DDevice9* real = RealDevice(exposed);
    g_gameDevice = real;
    char a[MAX_PATH], b[MAX_PATH], c[MAX_PATH];
    void** realVtable = *reinterpret_cast<void***>(real);
    BinderLog("device exposto 0x%08X (vtable em %s), real 0x%08X (vtable em %s, Present ao vivo em %s)",
              reinterpret_cast<DWORD>(exposed), ModuleOf(*reinterpret_cast<void**>(exposed), a, sizeof(a)),
              reinterpret_cast<DWORD>(real), ModuleOf(realVtable, b, sizeof(b)), ModuleOf(realVtable[17], c, sizeof(c)));

    bool detoured = HookD3D9(real);
    if (!detoured) {
        HookVtable(exposed);
    }

    SetupSamp();

    // Vigia: se o Present desviado nao estiver no caminho, troca para a vtable.
    // So conta o tempo com o jogo em primeiro plano (minimizado o GTA nao apresenta quadros).
    DWORD foreground = 0;
    bool silentLogged = false;
    for (;;) {
        Sleep(1000);
        LONG calls = g_presentCalls;
        if (GameInForeground()) {
            foreground += 1000;
        }
        if (detoured && calls == 0 && foreground >= 10000) {
            BinderLog("o Present desviado nao foi chamado em 10 s de jogo -- usando a vtable do device");
            detoured = false;
            IDirect3DDevice9* now = *reinterpret_cast<IDirect3DDevice9**>(GTA_DEVICE);
            if (now) {
                HookVtable(now);
            }
        }
        if (calls > 0 && !silentLogged && GetTickCount() - g_lastPresent > 3000 && GameInForeground() && !PauseMenuOpen()) {
            BinderLog("vigia: o Present nao chama ha 3 s (%ld quadros ate agora, gancho: %s)", calls, g_hookMode);
            silentLogged = true;
        } else if (silentLogged && GetTickCount() - g_lastPresent < 1000) {
            BinderLog("vigia: o Present voltou");
            silentLogged = false;
        }
    }
}

} // namespace

void BinderLog(const char* fmt, ...) {
    if (!g_logPath[0]) {
        return;
    }
    FILE* file = fopen(g_logPath, "a");
    if (!file) {
        return;
    }
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(file, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list args;
    va_start(args, fmt);
    vfprintf(file, fmt, args);
    va_end(args);
    fputs("\n", file);
    fclose(file);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        GetModuleFileNameA(module, g_logPath, MAX_PATH);
        snprintf(g_dataPath, sizeof(g_dataPath), "%s", g_logPath);
        char* slash = strrchr(g_logPath, '\\');
        char* dataSlash = strrchr(g_dataPath, '\\');
        if (slash && dataSlash) {
            strcpy(slash + 1, "Trok KeyBinder.log");
            strcpy(dataSlash + 1, "Trok KeyBinder.txt");
        } else {
            g_logPath[0] = 0;
            g_dataPath[0] = 0;
        }
        BinderLog("Trok KeyBinder .asi v%s -- esperando o render e o samp.dll", menu::Version());
        if (g_dataPath[0] && kb::LoadFile(g_dataPath, &g_data)) {
            BinderLog("%d presets e %d servidores lidos de Trok KeyBinder.txt", static_cast<int>(g_data.presets.size()),
                      static_cast<int>(g_data.servers.size()));
        }
        CreateThread(nullptr, 0, Boot, nullptr, 0, nullptr);
    }
    return TRUE;
}
