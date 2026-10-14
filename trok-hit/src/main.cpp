// Trok Hitmarker (.asi) -- Victor_Trok
// Indicador de dano para o SA-MP 0.3.7 R1: sinal de acerto na mira (imagens PNG editaveis), local do acerto no corpo,
// numeros de dano real, informer com combo, seta e borda do dano levado, e sons por evento. Liga o ImGui no device
// do GTA SA (a mesma base dos mods da casa: Present/Reset ORIGINAIS do d3d9 desviados com o MinHook, no fim de
// qualquer corrente), registra /trokhit e entrega o desenho para hud.cpp e menu.cpp. O lado do SA-MP fica em samp.cpp,
// o do GTA (mira, ossos, camera) em game.cpp e as contas do dano em logic.cpp.

#include <windows.h>
#include <d3d9.h>
#include <shellapi.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "MinHook.h"
#include "config.h"
#include "game.h"
#include "hud.h"
#include "images.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "logic.h"
#include "menu.h"
#include "options.h"
#include "samp.h"
#include "sound.h"
#include "trok_ui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

void HitLog(const char* fmt, ...); // Trok Hitmarker.log, ao lado do .asi

namespace {

// Enderecos do gta_sa.exe 1.0 US (os mesmos que os mods da casa usam).
constexpr DWORD GTA_DEVICE = 0xC97C28;      // IDirect3DDevice9* exposto (pode ser o proxy do SA-MP)
constexpr DWORD GTA_D3D_WINDOW = 0xC9C05C;  // hDeviceWindow dos D3DPRESENT_PARAMETERS do RenderWare
constexpr DWORD GTA_WINDOW = 0xC8CF88;      // PsGlobal.window
constexpr DWORD GTA_SCREEN_H = 0xC17048;    // RsGlobal.maximumHeight

// samp.dll 0.3.7 R1.
constexpr DWORD SAMP_R1_ENTRY = 0x31DF13;
constexpr DWORD SAMP_INFO = 0x21A0F8;
constexpr DWORD SAMP_CHAT = 0x21A0E4;
constexpr DWORD SAMP_INPUT = 0x21A0E8;
constexpr DWORD SAMP_DIALOG = 0x21A0B8;
constexpr DWORD SAMP_MISC = 0x21A10C;
constexpr DWORD SAMP_INPUT_ENABLED = 0x14E0; // CInput: chat aberto
constexpr DWORD SAMP_DIALOG_ACTIVE = 0x28;   // CDialog: dialogo do servidor na tela
constexpr DWORD SAMP_CHAT_ADD_ENTRY = 0x64010;
constexpr DWORD SAMP_ADD_COMMAND = 0x65AD0;
constexpr DWORD SAMP_SET_CURSOR = 0x9BD30;
constexpr DWORD SAMP_UNLOCK_CAM = 0x9BC10;

typedef HRESULT(__stdcall* PresentFn)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT(__stdcall* ResetFn)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef IDirect3D9*(WINAPI* Direct3DCreate9Fn)(UINT);
typedef void(__cdecl* CmdProc)(const char*);
typedef void(__thiscall* AddCommandFn)(void*, const char*, CmdProc);
typedef void(__thiscall* AddEntryFn)(void*, int, const char*, const char*, D3DCOLOR, D3DCOLOR);
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
volatile bool g_sampR1 = false;  // samp.dll 0.3.7 R1 com os ganchos instalados
volatile bool g_command = false; // /trokhit registrado
bool g_ready = false;
bool g_initFailed = false;
bool g_cursorOn = false;
bool g_inPresent = false;
bool g_firstDraw = true;
bool g_soundTried = false;
const char* g_hookMode = "nenhum";
volatile LONG g_togglePending = 0;
DWORD g_sampKeyboardTick = 0;
volatile LONG g_presentCalls = 0;
volatile DWORD g_lastPresent = 0;
float g_fontScreenH = 0.0f;
char g_logPath[MAX_PATH] = {};
char g_iniPath[MAX_PATH] = {};
char g_imagesDir[MAX_PATH] = {};
char g_soundsDir[MAX_PATH] = {};
char g_profile[300] = "\x01"; // servidor cujo perfil esta carregado ("\x01" = nenhum ainda)
bool g_dirty = false;         // mudou no menu e ainda nao foi gravado
DWORD g_dirtyTick = 0;
hud::State g_state;

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

// Janela do jogo, na mesma ordem do Trok Dialogs: a do device do RenderWare, a da plataforma e, por ultimo, a classe
// da janela do GTA.
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

// Mensagem no chat do SA-MP (CChat::AddEntry do R1, tipo 8 = aviso local).
void Chat(const char* text) {
    if (!g_command) {
        return;
    }
    void* chat = *reinterpret_cast<void**>(g_samp + SAMP_CHAT);
    if (chat) {
        reinterpret_cast<AddEntryFn>(g_samp + SAMP_CHAT_ADD_ENTRY)(chat, 8, text, nullptr, 0xFFFFFFFF, 0);
    }
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

void __cdecl CmdTrokHit(const char*) {
    InterlockedExchange(&g_togglePending, 1);
    HitLog("/trokhit digitado (imgui %s, %ld quadros, ultimo ha %lu ms, gancho: %s)", g_ready ? "pronto" : "NAO pronto",
           g_presentCalls, GetTickCount() - g_lastPresent, g_hookMode);
    if (!g_ready || GetTickCount() - g_lastPresent > 2000) {
        Chat("{FF8A8A}[Trok Hitmarker]{FFFFFF} o desenho ainda nao esta ativo. Me envie o arquivo "
             "{FFD27A}Trok Hitmarker.log{FFFFFF} (mesma pasta do .asi).");
    }
}

// O teclado e do SA-MP? (chat aberto ou dialogo do servidor na tela). Vale tambem por 150 ms depois de o chat fechar:
// o Enter que manda a mensagem nao pode cair no menu.
bool SampOwnsKeyboard() {
    if (g_command) {
        BYTE* input = *reinterpret_cast<BYTE**>(g_samp + SAMP_INPUT);
        BYTE* dialog = *reinterpret_cast<BYTE**>(g_samp + SAMP_DIALOG);
        if ((input && *reinterpret_cast<int*>(input + SAMP_INPUT_ENABLED)) ||
            (dialog && *reinterpret_cast<int*>(dialog + SAMP_DIALOG_ACTIVE))) {
            g_sampKeyboardTick = GetTickCount();
            return true;
        }
    }
    return g_sampKeyboardTick && GetTickCount() - g_sampKeyboardTick < 150;
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
        // Com o chat ou um dialogo do SA-MP aberto, o menu nao ve as teclas apertadas nem as segura (as soltas sempre
        // chegam ao ImGui, para nenhuma tecla ficar presa).
        bool pressed = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_CHAR;
        bool sampKeys = pressed && SampOwnsKeyboard();
        // O Tab e do SA-MP (abre o placar): o ImGui nunca o ve e ele sempre segue para o jogo.
        bool tabKey = wParam == VK_TAB && (pressed || msg == WM_KEYUP || msg == WM_SYSKEYUP);
        if (!sampKeys && !tabKey) {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
        }

        // Com o menu aberto o jogo nao recebe teclas nem cliques. Teclas soltas (WM_KEYUP) sempre passam: o GTA le o
        // teclado pelas mensagens da janela e uma tecla sem "soltar" ficaria presa. T e F6 (e o "t" que vem junto)
        // seguem para o SA-MP abrir o chat; o Tab (placar) segue sempre.
        bool typing = ImGui::GetIO().WantTextInput || tui::CapturingKey();
        bool chatKey = (msg == WM_KEYDOWN && (wParam == 'T' || wParam == VK_F6)) ||
                       (msg == WM_CHAR && (wParam == 't' || wParam == 'T'));
        if (menu::CapturesInput() && !game::Paused() && !sampKeys && !tabKey && !(chatKey && !typing)) {
            switch (msg) {
            case WM_KEYDOWN:
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
        HitLog("device do jogo agora e 0x%08X (exposto em 0xC97C28: 0x%08X)", reinterpret_cast<DWORD>(device),
               *reinterpret_cast<DWORD*>(GTA_DEVICE));
    } else {
        static int ignoredLogs = 0;
        g_otherDevice = device;
        if (ignoredLogs < 5) {
            ++ignoredLogs;
            HitLog("device 0x%08X ignorado (foco 0x%08X, janela 0x%08X; a do jogo e 0x%08X)",
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

bool Exists(const char* path) {
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

// Fontes: a da casa (moonloader\resource\trok\font.ttf, senao a Arial) e o Arial Black dos numeros (o do damage
// informer antigo; sem ele, a da casa).
void BuildFonts(float screenH) {
    char dir[MAX_PATH] = {};
    GameDir(dir);
    tui::BuildFonts(screenH, dir);
    char windows[MAX_PATH] = {};
    GetWindowsDirectoryA(windows, MAX_PATH);
    char house[MAX_PATH], arial[MAX_PATH], black[MAX_PATH];
    snprintf(house, sizeof(house), "%smoonloader\\resource\\trok\\font.ttf", dir);
    snprintf(arial, sizeof(arial), "%s\\Fonts\\arial.ttf", windows);
    snprintf(black, sizeof(black), "%s\\Fonts\\ariblk.ttf", windows);
    const char* text = Exists(house) ? house : arial;
    hud::AddFonts(screenH, text, Exists(black) ? black : text);
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
            HitLog("imgui: janela do jogo ainda nao encontrada -- tento de novo a cada quadro");
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
        HitLog("imgui: backend falhou -- desistindo (o jogo segue sem o Trok Hitmarker)");
        g_initFailed = true;
        return;
    }
    g_imguiDevice = device;

    g_fontScreenH = ScreenHeight();
    BuildFonts(g_fontScreenH);

    g_wndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    if (!g_wndProc) {
        HitLog("imgui: SetWindowLongPtr falhou -- sem entrada");
        g_initFailed = true;
        return;
    }

    g_ready = true;
    HitLog("imgui pronto (device 0x%08X, janela 0x%08X, tela de %.0f px)", reinterpret_cast<DWORD>(device),
           reinterpret_cast<DWORD>(g_window), g_fontScreenH);
}

// Perfil do servidor atual: ao entrar num servidor (ou trocar), carrega o [padrao] com a secao dele por cima.
void SaveNow() {
    if (g_dirty && g_profile[0] != '\x01') {
        cfg::Save(g_iniPath, g_profile[0] ? g_profile : nullptr);
    }
    g_dirty = false;
}

void FollowServer(const samp::ServerInfo& server) {
    const char* key = server.connected ? server.address : "";
    if (strcmp(key, g_profile) == 0) {
        return;
    }
    SaveNow(); // o que mudou fica no perfil do servidor de antes
    snprintf(g_profile, sizeof(g_profile), "%s", key);
    cfg::Load(g_iniPath, key[0] ? key : nullptr);
    logic::Reset();
    HitLog("perfil: %s", key[0] ? key : "padrao (fora de um servidor)");
}

void OpenFolder(const char* folder) {
    HINSTANCE r = ShellExecuteA(nullptr, "open", folder, nullptr, nullptr, SW_SHOWNORMAL);
    HitLog("abrir pasta %s: %s", folder, reinterpret_cast<INT_PTR>(r) > 32 ? "ok" : "falhou");
}

void RenderOverlay(IDirect3DDevice9* device) {
    if (!g_ready && !g_initFailed) {
        InitImGui(device);
    }
    if (!g_ready) {
        return;
    }
    if (device != g_imguiDevice) {
        HitLog("device trocou (0x%08X -> 0x%08X) -- remontando o ImGui", reinterpret_cast<DWORD>(g_imguiDevice),
               reinterpret_cast<DWORD>(device));
        images::Release();
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplDX9_Init(device);
        g_imguiDevice = device;
    }
    if (device->TestCooperativeLevel() != D3D_OK) {
        return;
    }
    // Todo quadro, para a folga de 150 ms valer mesmo se o Enter que fecha o chat vier depois de uma pausa.
    SampOwnsKeyboard();

    samp::ServerInfo server = samp::Server();
    FollowServer(server);
    const opt::Options& o = opt::Get();

    if (!g_soundTried) {
        g_soundTried = true;
        sound::Init(g_window, g_soundsDir);
    }

    if (InterlockedExchange(&g_togglePending, 0)) {
        if (game::Paused()) {
            HitLog("/trokhit ignorado: menu de pausa aberto");
        } else {
            menu::Toggle();
            HitLog("menu %s", menu::CapturesInput() ? "aberto" : "fechado");
        }
    }

    float screenH = ScreenHeight();
    if (screenH != g_fontScreenH) {
        HitLog("resolucao mudou (%.0f -> %.0f px) -- remontando as fontes", g_fontScreenH, screenH);
        g_fontScreenH = screenH;
        ImGui_ImplDX9_InvalidateDeviceObjects();
        BuildFonts(screenH);
        ImGui_ImplDX9_CreateDeviceObjects();
    }

    bool paused = game::Paused();
    bool wantCursor = menu::CapturesInput() && !paused;
    if (wantCursor) {
        SampCursor(true); // todo quadro: chat e outros mods tentam devolver o cursor
    } else if (g_cursorOn) {
        SampCursor(false);
    }
    g_cursorOn = wantCursor;

    // O ImGui roda todo quadro (mesmo sem nada na tela) para a fila de entrada nao acumular.
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime;
    images::Update(device, dt);
    sound::SetVolume(o.volume);
    sound::Update(dt);

    // A mira do jogo nos proximos quadros: escondida com a do Trok; parada e no tamanho escolhido com a do jogo.
    bool on = o.enabled && g_sampR1;
    game::SetCrosshair(on && o.crosshair == 1, on && o.crosshair == 0 && o.crosshairStatic,
                       on && o.crosshair == 0 ? o.crosshairSize / 100.0f : 1.0f);
    game::Aim aim = game::TakeAim();
    logic::Frame(dt, io.DisplaySize, aim, o, &g_state);

    bool draw = false;
    if (on && !paused) {
        hud::Draw(ImGui::GetBackgroundDrawList(), g_state, o, io.DisplaySize);
        draw = true;
    }
    if (menu::WantsFrame() && !paused) {
        if (menu::Frame()) {
            g_dirty = true;
            g_dirtyTick = GetTickCount();
        }
        draw = true;
    }
    if (g_dirty && GetTickCount() - g_dirtyTick > 800) {
        SaveNow();
    }
    switch (menu::TakeRequest()) {
    case menu::REQ_OPEN_IMAGES: OpenFolder(g_imagesDir); break;
    case menu::REQ_OPEN_SOUNDS: OpenFolder(g_soundsDir); break;
    case menu::REQ_PLAY_SOUNDS: sound::PlayAll(); break;
    default: break;
    }
    ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
    ImGui::EndFrame();
    if (draw) {
        ImGui::Render();
        ImDrawData* data = ImGui::GetDrawData();
        if (data->TotalVtxCount > 0) {
            // No Present a cena do jogo ja foi fechada: o desenho do ImGui precisa da sua propria cena.
            HRESULT scene = device->BeginScene();
            ImGui_ImplDX9_RenderDrawData(data);
            if (SUCCEEDED(scene)) {
                device->EndScene();
            }
            if (g_firstDraw) {
                g_firstDraw = false;
                HitLog("primeiro quadro desenhado (%d listas, BeginScene 0x%08X)", data->CmdListsCount,
                       static_cast<unsigned>(scene));
            }
        }
    }
    PublishCursor(!wantCursor ? 0 : cursor == ImGuiMouseCursor_Hand ? 1 : cursor == ImGuiMouseCursor_TextInput ? 2 : 0);
}

HRESULT __stdcall HookPresent(IDirect3DDevice9* device, const RECT* src, const RECT* dst, HWND wnd, const RGNDATA* dirty) {
    // g_inPresent: se outra camada nos chamar de novo dentro do mesmo Present, nao desenha duas vezes.
    if (!g_inPresent && IsGameDevice(device)) {
        g_inPresent = true;
        if (InterlockedIncrement(&g_presentCalls) == 1) {
            HitLog("primeiro Present recebido (device 0x%08X, gancho: %s)", reinterpret_cast<DWORD>(device), g_hookMode);
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

// Endereco ORIGINAL de um slot da vtable, lido do arquivo em disco do modulo dono dela. Os outros mods trocam o slot
// na memoria, entao o valor ao vivo pode ser o gancho de alguem.
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
            // A copia pode vir realocada para a base ao vivo (ASLR do sistema), para a propria vista ou nao realocada
            // (base preferida). Em todo caso o destino tem que cair dentro do modulo.
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

// O device de verdade atras de proxies (o SA-MP troca o ponteiro do jogo por um proxy): o back buffer nao passa pelo
// proxy e devolve o device real. E o mesmo caminho que o Trok Dialogs usa.
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

// Device NULLREF descartavel no d3d9.dll do sistema: so quando a vtable do device do jogo nao da para ler de um
// arquivo (o device real ficou escondido atras de um wrapper).
bool SlotsFromDummy(HMODULE d3d9, void** present, void** reset) {
    auto create = reinterpret_cast<Direct3DCreate9Fn>(reinterpret_cast<void*>(GetProcAddress(d3d9, "Direct3DCreate9")));
    IDirect3D9* d3d = create ? create(D3D_SDK_VERSION) : nullptr;
    if (!d3d) {
        HitLog("Direct3DCreate9 do sistema falhou");
        return false;
    }
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = g_module;
    wc.lpszClassName = "TrokHitD3D";
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
        HitLog("device descartavel do d3d9 falhou (0x%08X)", static_cast<unsigned>(hr));
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
        HitLog("a vtable do device real nao veio de um arquivo -- procurando com um device descartavel");
        found = d3d9 && SlotsFromDummy(d3d9, &present, &reset);
    }
    if (!found) {
        HitLog("Present original nao encontrado");
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
        HitLog("MinHook recusou o desvio (Present %d, Reset %d, ligar %d)", a, b, d);
        MH_DisableHook(present);
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
    HitLog("Present e Reset desviados em %s (Present 0x%08X)", ModuleOf(present, who, sizeof(who)),
           reinterpret_cast<DWORD>(present));
    return true;
}

// Reserva, se o Present desviado nunca for chamado: troca os slots da vtable do device exposto. So os desvios do d3d9
// saem (os do samp.dll e do GTA continuam).
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
        HitLog("vtable nao pode ser alterada -- sem desenho");
        return false;
    }
    g_reset = reinterpret_cast<ResetFn>(vtable[16]);
    g_present = reinterpret_cast<PresentFn>(vtable[17]);
    g_gameDevice = device;
    vtable[16] = reinterpret_cast<void*>(HookReset);
    vtable[17] = reinterpret_cast<void*>(HookPresent);
    VirtualProtect(vtable, sizeof(void*) * 18, old, &old);
    g_hookMode = "vtable";
    HitLog("reserva: Present e Reset trocados na vtable do device 0x%08X", reinterpret_cast<DWORD>(device));
    return true;
}

bool IsSampR1(DWORD base) {
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return nt->OptionalHeader.AddressOfEntryPoint == SAMP_R1_ENTRY;
}

// Espera o samp.dll, confere a versao, instala os ganchos (SA-MP e a mira do GTA) e registra o /trokhit.
void SetupSamp() {
    for (int i = 0; i < 300 && !g_samp; ++i) {
        g_samp = reinterpret_cast<DWORD>(GetModuleHandleA("samp.dll"));
        if (!g_samp) {
            Sleep(100);
        }
    }
    if (!g_samp) {
        HitLog("samp.dll nao apareceu (single player?) -- o Trok Hitmarker so funciona no SA-MP");
        return;
    }
    if (!IsSampR1(g_samp)) {
        HitLog("samp.dll nao e o 0.3.7 R1 -- o Trok Hitmarker fica desligado (nada foi alterado)");
        return;
    }
    samp::Events events = {logic::OnGive, logic::OnTake, logic::OnDeath};
    if (!samp::Install(g_samp, events)) {
        return;
    }
    game::Install(); // sem os ganchos da mira, o sinal vai no lugar padrao e a mira do jogo fica como esta
    g_sampR1 = true;
    for (int i = 0; i < 1200; ++i) {
        void* input = *reinterpret_cast<void**>(g_samp + SAMP_INPUT);
        void* info = *reinterpret_cast<void**>(g_samp + SAMP_INFO);
        if (input && info) {
            reinterpret_cast<AddCommandFn>(g_samp + SAMP_ADD_COMMAND)(input, "trokhit", CmdTrokHit);
            g_command = true;
            HitLog("/trokhit registrado (CInput 0x%08X)", reinterpret_cast<DWORD>(input));
            return;
        }
        Sleep(100);
    }
    HitLog("CInput do SA-MP nao apareceu -- sem /trokhit (o resto continua)");
}

bool GameInForeground() {
    HWND game = GameWindow();
    return game && GetForegroundWindow() == game && !IsIconic(game);
}

DWORD WINAPI Boot(LPVOID) {
    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        HitLog("MinHook nao iniciou (%d) -- desistindo", init);
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
        HitLog("o render nao subiu em 2 minutos -- desistindo");
        return 0;
    }
    // Como o Trok Dialogs: da tempo para o SA-MP e o moonloader montarem os proxies e ganchos deles.
    Sleep(1500);
    exposed = *reinterpret_cast<IDirect3DDevice9**>(GTA_DEVICE);
    IDirect3DDevice9* real = RealDevice(exposed);
    g_gameDevice = real;
    char a[MAX_PATH], b[MAX_PATH], c[MAX_PATH];
    void** realVtable = *reinterpret_cast<void***>(real);
    HitLog("device exposto 0x%08X (vtable em %s), real 0x%08X (vtable em %s, Present ao vivo em %s)",
           reinterpret_cast<DWORD>(exposed), ModuleOf(*reinterpret_cast<void**>(exposed), a, sizeof(a)),
           reinterpret_cast<DWORD>(real), ModuleOf(realVtable, b, sizeof(b)), ModuleOf(realVtable[17], c, sizeof(c)));

    bool detoured = HookD3D9(real);
    if (!detoured) {
        HookVtable(exposed);
    }

    SetupSamp();

    // Vigia: se o Present desviado nao estiver no caminho, troca para a vtable. So conta o tempo com o jogo em primeiro
    // plano (minimizado o GTA nao apresenta quadros).
    DWORD foreground = 0;
    bool silentLogged = false;
    for (;;) {
        Sleep(1000);
        LONG calls = g_presentCalls;
        if (GameInForeground()) {
            foreground += 1000;
        }
        if (detoured && calls == 0 && foreground >= 10000) {
            HitLog("o Present desviado nao foi chamado em 10 s de jogo -- usando a vtable do device");
            detoured = false;
            IDirect3DDevice9* now = *reinterpret_cast<IDirect3DDevice9**>(GTA_DEVICE);
            if (now) {
                HookVtable(now);
            }
        }
        if (calls > 0 && !silentLogged && GetTickCount() - g_lastPresent > 3000 && GameInForeground() &&
            !game::Paused()) {
            HitLog("vigia: o Present nao chama ha 3 s (%ld quadros ate agora, gancho: %s)", calls, g_hookMode);
            silentLogged = true;
        } else if (silentLogged && GetTickCount() - g_lastPresent < 1000) {
            HitLog("vigia: o Present voltou");
            silentLogged = false;
        }
    }
}

void Sibling(char* out, const char* name) {
    GetModuleFileNameA(g_module, out, MAX_PATH);
    char* slash = strrchr(out, '\\');
    if (slash) {
        slash[1] = 0;
        strncat(out, name, MAX_PATH - strlen(out) - 1);
    } else {
        out[0] = 0;
    }
}

} // namespace

void HitLog(const char* fmt, ...) {
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
    if (reason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }
    g_module = module;
    DisableThreadLibraryCalls(module);
    Sibling(g_logPath, "Trok Hitmarker.log");
    Sibling(g_iniPath, "Trok Hitmarker.ini");
    Sibling(g_imagesDir, "Trok Hitmarker\\mira");
    Sibling(g_soundsDir, "Trok Hitmarker\\sons");
    if (g_logPath[0]) {
        FILE* fresh = fopen(g_logPath, "w"); // um log por sessao de jogo
        if (fresh) {
            fclose(fresh);
        }
    }
    HitLog("Trok Hitmarker .asi %s -- esperando o render e o samp.dll", menu::Version());

    // Duas copias do .asi (com nomes diferentes) desenhariam tudo duas vezes.
    char marker[8];
    if (GetEnvironmentVariableA("TROK_HIT_LOADED", marker, sizeof(marker))) {
        HitLog("outra copia do Trok Hitmarker ja esta carregada -- esta ficou desligada");
        return TRUE;
    }
    SetEnvironmentVariableA("TROK_HIT_LOADED", "1");
    if (!game::VersionOk()) {
        HitLog("o gta_sa.exe nao e o 1.0 US -- o Trok Hitmarker ficou desligado (nada foi alterado)");
        return TRUE;
    }
    HitLog("gta_sa.exe 1.0 US confirmado");
    cfg::Load(g_iniPath, nullptr);
    images::Init(g_imagesDir);
    CreateThread(nullptr, 0, Boot, nullptr, 0, nullptr);
    return TRUE;
}
