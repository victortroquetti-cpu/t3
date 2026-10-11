// Trok Chat (.asi) -- Victor_Trok
// Liga o ImGui no device do GTA SA (Present/Reset) e entrega o desenho para chat.cpp. O gancho e o mesmo dos mods da
// casa (vitrine do Trok UI, Trok Shadows Menu): o Present/Reset ORIGINAIS do modulo dono da vtable do device real, lidos
// do arquivo em disco, sao desviados com o MinHook, e o desenho acontece no fim de qualquer corrente (proxy do SA-MP,
// Trok Dialogs, Trok Radar, moonloader). Se o Present desviado nao for chamado, cai para o slot da vtable.
//
// Teclado, em tres estados:
//   - menu do /trokchat aberto: teclado e mouse sao dele (com o cursor do SA-MP), como nos menus da casa;
//   - barra aberta: as teclas apertadas e os caracteres ficam com ela (nem o GTA nem o SA-MP os veem, como na caixa do
//     SA-MP), menos as teclas F (F7, F8...; o F6 fica com ela). A tecla que abriu a barra, segurada, nao repete nela;
//   - fechada: tudo segue o caminho de sempre ate o SA-MP; quando ele abre a caixa dele (T, F6, um script), a barra do
//     Trok Chat abre no lugar (samp.cpp). O Enter que mandou a mensagem, segurado, nao chega ao jogo.
// Teclas soltas sempre passam: o GTA le o teclado pelas mensagens da janela e uma tecla sem "soltar" ficaria presa.

#include "tc.h"
#include "chat.h"
#include "menu.h"
#include "options.h"
#include "samp.h"

#include <d3d9.h>
#include <cstdio>
#include <cstring>

#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "trok_ui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

// Enderecos do gta_sa.exe 1.0 US (os mesmos que os mods da casa usam).
constexpr DWORD GTA_DEVICE = 0xC97C28;      // IDirect3DDevice9* exposto (pode ser o proxy do SA-MP)
constexpr DWORD GTA_D3D_WINDOW = 0xC9C05C;  // hDeviceWindow dos D3DPRESENT_PARAMETERS do RenderWare
constexpr DWORD GTA_WINDOW = 0xC8CF88;      // PsGlobal.window
constexpr DWORD GTA_SCREEN_H = 0xC17048;    // RsGlobal.maximumHeight
constexpr DWORD GTA_MENU_ACTIVE = 0xBA67A4; // menu de pausa aberto

typedef HRESULT(__stdcall* PresentFn)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT(__stdcall* ResetFn)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef IDirect3D9*(WINAPI* Direct3DCreate9Fn)(UINT);

PresentFn g_present = nullptr; // trampolim do MinHook (ou slot antigo da vtable no modo reserva)
ResetFn g_reset = nullptr;
void* g_presentTarget = nullptr; // o que o MinHook desviou (para desligar so estes dois)
void* g_resetTarget = nullptr;
IDirect3DDevice9* volatile g_gameDevice = nullptr; // device que o GTA apresenta (o real, atras do proxy)
IDirect3DDevice9* g_otherDevice = nullptr;
IDirect3DDevice9* g_imguiDevice = nullptr; // device em que o backend DX9 do ImGui foi montado
WNDPROC g_wndProc = nullptr;
HWND g_window = nullptr;
HMODULE g_module = nullptr;
bool g_ready = false;
bool g_initFailed = false;
bool g_inPresent = false;
bool g_firstDraw = true;
const char* g_hookMode = "nenhum";
volatile LONG g_presentCalls = 0;
volatile DWORD g_lastPresent = 0;
float g_fontScreenH = 0.0f;
WPARAM g_lastDown = 0;  // a ultima tecla apertada que passou adiante com a barra fechada
WPARAM g_heldKey = 0;   // a tecla que abriu a barra, enquanto segurada
bool g_eatChar = false; // o caractere que ela ainda vai gerar
bool g_enterHeld = false; // o Enter que mandou a mensagem, ainda apertado
bool g_cursorOn = false;

// O que o Trok Chat.ini guarda: as opcoes e a copia das do SA-MP. Gravado um segundo depois da ultima mudanca.
struct Snapshot {
    opt::Options options;
    host::ChatSettings chat;
};

bool operator!=(const Snapshot& a, const Snapshot& b) {
    return a.options.tags != b.options.tags || a.options.enabled != b.options.enabled ||
           a.chat.pageSize != b.chat.pageSize || a.chat.mode != b.chat.mode || a.chat.timestamps != b.chat.timestamps;
}

Snapshot g_saved, g_pending;
bool g_haveSaved = false;
DWORD g_changedAt = 0;

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

void BuildFonts(float screenHeight) {
    char dir[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    char* slash = strrchr(dir, '\\');
    if (slash) {
        slash[1] = 0;
    } else {
        dir[0] = 0;
    }
    tui::BuildFonts(screenHeight, dir);
}

bool MouseButton(UINT msg) {
    return (msg >= WM_LBUTTONDOWN && msg <= WM_MBUTTONDBLCLK) || (msg >= WM_XBUTTONDOWN && msg <= WM_XBUTTONDBLCLK);
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
    if (!g_ready) {
        return CallWindowProcA(g_wndProc, hwnd, msg, wParam, lParam);
    }
    if (msg == WM_KEYUP && wParam == VK_RETURN) {
        g_enterHeld = false;
    }

    if (menu::IsOpen() && !host::DialogOpen()) {
        // Menu do /trokchat: o jogo nao recebe teclas nem cliques (as teclas soltas passam).
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
        switch (msg) {
        case WM_KEYDOWN:
        case WM_CHAR:
        case WM_DEADCHAR:
        case WM_MOUSEWHEEL:
            return 0;
        case WM_SETCURSOR:
            return TRUE;
        default:
            if (MouseButton(msg)) {
                return 0;
            }
            break;
        }
        return CallWindowProcA(g_wndProc, hwnd, msg, wParam, lParam);
    }

    if (chat::IsOpen()) {
        // A tecla que abriu a barra, segurada: nao fica repetindo nela (o T segurado nao vira "ttt").
        if (msg == WM_KEYDOWN && g_heldKey && wParam == g_heldKey) {
            g_eatChar = true;
            return 0;
        }
        if (msg == WM_KEYUP && wParam == g_heldKey) {
            g_heldKey = 0;
        }
        if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
            g_enterHeld = true;
        }
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            g_eatChar = false;
        }
        if (msg == WM_CHAR && g_eatChar) {
            g_eatChar = false;
            return 0;
        }
        // O ImGui ve o teclado e a roda, mas nao os cliques: sem cursor na tela, um clique (o tiro do jogo) tiraria o
        // foco da barra.
        if (!MouseButton(msg)) {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
        }
        switch (msg) {
        case WM_KEYDOWN:
            if (wParam >= VK_F1 && wParam <= VK_F24 && wParam != VK_F6) {
                break; // F7 (modo do chat), F8 (foto) e afins continuam com o SA-MP
            }
            return 0;
        case WM_CHAR:
        case WM_DEADCHAR:
        case WM_MOUSEWHEEL:
            return 0;
        default:
            break;
        }
        return CallWindowProcA(g_wndProc, hwnd, msg, wParam, lParam);
    }

    // Fechada: o Enter que mandou a mensagem, segurado (so as repeticoes; ele ja foi solto e apertado de novo se nao for
    // repeticao), nao chega ao jogo (o boneco nao entra no carro) nem ao SA-MP.
    if (g_enterHeld && ((msg == WM_KEYDOWN && wParam == VK_RETURN && (lParam & (1 << 30))) ||
                        (msg == WM_CHAR && wParam == '\r'))) {
        return 0;
    }
    // O ImGui so ve as teclas soltas e o resto (foco, mouse); as apertadas e os caracteres nao: a tecla que abre o chat
    // nao pode chegar nele apertada.
    if (!MouseButton(msg) && msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN && msg != WM_CHAR && msg != WM_DEADCHAR &&
        msg != WM_MOUSEWHEEL) {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
    }
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        g_lastDown = wParam;
    }
    const LRESULT result = CallWindowProcA(g_wndProc, hwnd, msg, wParam, lParam);
    if (chat::IsOpen()) {
        // Abriu agora, por esta mensagem: o "t" do T (que fica com o SA-MP) ou o F6 (que nao gera caractere).
        const bool key = msg == WM_KEYDOWN || msg == WM_CHAR;
        g_heldKey = key ? g_lastDown : 0;
        g_eatChar = msg == WM_KEYDOWN;
    }
    return result;
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
        Log("device do jogo agora e 0x%08X (exposto em 0xC97C28: 0x%08X)", reinterpret_cast<DWORD>(device),
            *reinterpret_cast<DWORD*>(GTA_DEVICE));
    } else {
        static int ignoredLogs = 0;
        g_otherDevice = device;
        if (ignoredLogs < 5) {
            ++ignoredLogs;
            Log("device 0x%08X ignorado (foco 0x%08X, janela 0x%08X; a do jogo e 0x%08X)",
                reinterpret_cast<DWORD>(device), reinterpret_cast<DWORD>(cp.hFocusWindow),
                reinterpret_cast<DWORD>(pp.hDeviceWindow), reinterpret_cast<DWORD>(game));
        }
    }
    return mine;
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
            Log("janela do jogo ainda nao encontrada -- tento de novo a cada quadro");
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
        Log("backend do ImGui falhou -- o chat do SA-MP continua");
        g_initFailed = true;
        return;
    }
    g_imguiDevice = device;

    g_fontScreenH = ScreenHeight();
    BuildFonts(g_fontScreenH);

    // As opcoes (as teclas tem nome do ImGui, por isso so agora) e a copia das do SA-MP, que valem de novo.
    char path[MAX_PATH];
    GetModuleFileNameA(g_module, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    lstrcpynA(slash ? slash + 1 : path, "Trok Chat.ini", static_cast<int>(MAX_PATH - (slash ? slash + 1 - path : 0)));
    ini::SetPath(path);
    host::ChatSettings saved = host::Settings();
    if (ini::Load(&saved)) {
        host::Apply(saved);
    }

    g_wndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    if (!g_wndProc) {
        Log("SetWindowLongPtr falhou -- sem teclado; o chat do SA-MP continua");
        g_initFailed = true;
        return;
    }

    g_ready = true;
    Log("ImGui pronto (device 0x%08X, janela 0x%08X, tela de %.0f px)", reinterpret_cast<DWORD>(device),
        reinterpret_cast<DWORD>(g_window), g_fontScreenH);
}

// Grava o Trok Chat.ini um segundo depois da ultima mudanca (do menu ou do /pagesize, /timestamp e F7 do SA-MP).
void Persist() {
    const Snapshot now = {opt::Get(), host::Settings()};
    if (!g_haveSaved) {
        g_saved = g_pending = now; // o que veio do INI (ou os padroes): nada a gravar
        g_haveSaved = true;
        return;
    }
    if (now != g_pending) {
        g_pending = now;
        g_changedAt = GetTickCount();
    }
    if (g_pending != g_saved && GetTickCount() - g_changedAt > 1000) {
        if (ini::Save(g_pending.chat)) {
            g_saved = g_pending;
        } else {
            g_saved = g_pending; // nao tenta de novo a cada quadro; o aviso fica no menu
            tui::Toast("Trok Chat", "N\xC3\xA3o deu para gravar o Trok Chat.ini (pasta s\xC3\xB3 para leitura?).");
        }
    }
}

void RenderOverlay(IDirect3DDevice9* device) {
    // Na primeira vez com o SA-MP pronto, desvia o chat dele (na thread do jogo); depois, o texto de scripts.
    samp::Frame();
    if (!samp::Ready()) {
        return;
    }
    if (!g_ready && !g_initFailed) {
        InitImGui(device);
    }
    if (!g_ready) {
        return; // sem o ImGui o chat do SA-MP continua na tela (samp::Heartbeat nunca chega)
    }
    if (device != g_imguiDevice) {
        Log("device trocou (0x%08X -> 0x%08X) -- remontando o ImGui", reinterpret_cast<DWORD>(g_imguiDevice),
            reinterpret_cast<DWORD>(device));
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplDX9_Init(device);
        g_imguiDevice = device;
    }
    if (device->TestCooperativeLevel() != D3D_OK) {
        return;
    }
    samp::Heartbeat();

    float screenH = ScreenHeight();
    if (screenH != g_fontScreenH) {
        Log("resolucao mudou (%.0f -> %.0f px) -- remontando as fontes", g_fontScreenH, screenH);
        g_fontScreenH = screenH;
        ImGui_ImplDX9_InvalidateDeviceObjects();
        BuildFonts(screenH);
        ImGui_ImplDX9_CreateDeviceObjects();
    }

    const bool paused = PauseMenuOpen();
    if (samp::TakeMenuRequest()) {
        if (paused) {
            Log("menu: ignorado (menu de pausa aberto)");
        } else {
            menu::Toggle();
            Log("menu %s", menu::IsOpen() ? "aberto" : "fechado");
        }
    }
    const bool active = samp::Active();
    if (chat::IsOpen() && (!active || paused || menu::IsOpen())) {
        chat::Close();
    }

    // Cursor do SA-MP com o menu aberto (todo quadro: outros mods tentam devolver o cursor).
    const bool wantCursor = menu::IsOpen() && !paused;
    if (wantCursor) {
        samp::Cursor(true);
    } else if (g_cursorOn) {
        samp::Cursor(false);
    }
    g_cursorOn = wantCursor;

    // O ImGui roda todo quadro (mesmo sem nada na tela) para a fila de entrada nao acumular e o estado das teclas
    // ficar em dia; o chat so aparece com o Trok Chat no lugar e fora do menu de pausa (o SA-MP tambem o esconde nele).
    const bool drawChat = active && !paused;
    const bool drawMenu = menu::WantsFrame() && !paused;
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (drawChat) {
        chat::Frame();
    }
    if (drawMenu) {
        menu::Frame();
    }
    const ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
    ImGui::EndFrame();
    PublishCursor(!wantCursor ? 0 : cursor == ImGuiMouseCursor_Hand ? 1 : cursor == ImGuiMouseCursor_TextInput ? 2 : 0);
    Persist();
    if (!drawChat && !drawMenu) {
        return;
    }
    ImGui::Render();
    ImDrawData* data = ImGui::GetDrawData();
    if (data->TotalVtxCount == 0) {
        return;
    }
    // No Present a cena do jogo ja foi fechada: o desenho do ImGui precisa da sua propria cena.
    HRESULT scene = device->BeginScene();
    ImGui_ImplDX9_RenderDrawData(data);
    if (SUCCEEDED(scene)) {
        device->EndScene();
    }
    if (g_firstDraw) {
        g_firstDraw = false;
        Log("primeiro quadro desenhado (%d listas, BeginScene 0x%08X)", data->CmdListsCount,
            static_cast<unsigned>(scene));
    }
}

HRESULT __stdcall HookPresent(IDirect3DDevice9* device, const RECT* src, const RECT* dst, HWND wnd,
                              const RGNDATA* dirty) {
    // g_inPresent: se outra camada nos chamar de novo dentro do mesmo Present, nao desenha duas vezes.
    if (!g_inPresent && IsGameDevice(device)) {
        g_inPresent = true;
        if (InterlockedIncrement(&g_presentCalls) == 1) {
            Log("primeiro Present recebido (device 0x%08X, gancho: %s)", reinterpret_cast<DWORD>(device), g_hookMode);
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

// Present (slot 17) e Reset (slot 16) originais de uma vtable que mora no modulo indicado.
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
        Log("Direct3DCreate9 do sistema falhou");
        return false;
    }
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = g_module;
    wc.lpszClassName = "TrokChatD3D";
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
                                g_module, nullptr);

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.hDeviceWindow = hwnd;
    IDirect3DDevice9* device = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hwnd,
                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &pp,
                                   &device);
    bool ok = false;
    if (SUCCEEDED(hr) && device) {
        void** vtable = *reinterpret_cast<void***>(device);
        ok = OwnerOf(vtable) == d3d9 && SlotsFromVtable(d3d9, vtable, present, reset);
        device->Release();
    } else {
        Log("device descartavel do d3d9 falhou (0x%08X)", static_cast<unsigned>(hr));
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
        Log("a vtable do device real nao veio de um arquivo -- procurando com um device descartavel");
        found = d3d9 && SlotsFromDummy(d3d9, &present, &reset);
    }
    if (!found) {
        Log("Present original nao encontrado");
        return false;
    }
    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        Log("MinHook nao iniciou (%d)", init);
        return false;
    }
    MH_STATUS a = MH_CreateHook(present, reinterpret_cast<void*>(HookPresent), reinterpret_cast<void**>(&g_present));
    MH_STATUS b = MH_CreateHook(reset, reinterpret_cast<void*>(HookReset), reinterpret_cast<void**>(&g_reset));
    g_hookMode = "Present desviado"; // antes de ligar: o primeiro Present pode chegar antes do proximo Log
    MH_STATUS c = MH_UNKNOWN, d = MH_UNKNOWN;
    if (a == MH_OK && b == MH_OK) {
        // Reset antes do Present: com o Present ligado o desenho comeca, e o Reset tem que estar no lugar.
        c = MH_EnableHook(reset);
        d = c == MH_OK ? MH_EnableHook(present) : MH_UNKNOWN;
    }
    if (d != MH_OK) {
        Log("MinHook recusou o desvio (Present %d, Reset %d, ligar %d/%d)", a, b, c, d);
        MH_DisableHook(present);
        MH_DisableHook(reset);
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
    Log("Present e Reset desviados em %s (Present 0x%08X, Reset 0x%08X)", ModuleOf(present, who, sizeof(who)),
        reinterpret_cast<DWORD>(present), reinterpret_cast<DWORD>(reset));
    return true;
}

// Reserva, se o Present desviado nunca for chamado: troca o slot da vtable do device exposto. Desliga so o desvio do
// Present/Reset (os do chat do SA-MP ficam).
bool HookVtable(IDirect3DDevice9* device) {
    if (g_presentTarget) {
        MH_DisableHook(g_presentTarget);
        MH_DisableHook(g_resetTarget);
    }
    void** vtable = *reinterpret_cast<void***>(device);
    DWORD old;
    if (!VirtualProtect(&vtable[16], sizeof(void*) * 2, PAGE_EXECUTE_READWRITE, &old)) {
        Log("vtable nao pode ser alterada -- sem desenho");
        return false;
    }
    g_reset = reinterpret_cast<ResetFn>(vtable[16]);
    g_present = reinterpret_cast<PresentFn>(vtable[17]);
    g_gameDevice = device;
    vtable[16] = reinterpret_cast<void*>(HookReset);
    vtable[17] = reinterpret_cast<void*>(HookPresent);
    VirtualProtect(&vtable[16], sizeof(void*) * 2, old, &old);
    g_hookMode = "vtable";
    Log("reserva: Present e Reset trocados na vtable do device 0x%08X", reinterpret_cast<DWORD>(device));
    return true;
}

bool GameInForeground() {
    HWND game = GameWindow();
    return game && GetForegroundWindow() == game && !IsIconic(game);
}

DWORD WINAPI Boot(LPVOID) {
    // Primeiro o SA-MP: sem o 0.3.7 R1 (single player, outra versao) o Trok Chat nao encosta em nada.
    if (!samp::Start()) {
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
        Log("o render nao subiu em 2 minutos -- o chat do SA-MP continua");
        return 0;
    }
    // Como o Trok Dialogs: da tempo para o SA-MP e o moonloader montarem os proxies e ganchos deles.
    Sleep(1500);
    exposed = *reinterpret_cast<IDirect3DDevice9**>(GTA_DEVICE);
    IDirect3DDevice9* real = RealDevice(exposed);
    g_gameDevice = real;
    char a[MAX_PATH], b[MAX_PATH], c[MAX_PATH];
    void** realVtable = *reinterpret_cast<void***>(real);
    Log("device exposto 0x%08X (vtable em %s), real 0x%08X (vtable em %s, Present ao vivo em %s)",
        reinterpret_cast<DWORD>(exposed), ModuleOf(*reinterpret_cast<void**>(exposed), a, sizeof(a)),
        reinterpret_cast<DWORD>(real), ModuleOf(realVtable, b, sizeof(b)), ModuleOf(realVtable[17], c, sizeof(c)));

    bool detoured = HookD3D9(real);
    if (!detoured) {
        HookVtable(exposed);
    }

    // Vigia: se o Present desviado nao estiver no caminho, troca para a vtable. So conta o tempo com o jogo em
    // primeiro plano (minimizado o GTA nao apresenta quadros).
    DWORD foreground = 0;
    bool silentLogged = false;
    for (;;) {
        Sleep(1000);
        LONG calls = g_presentCalls;
        if (GameInForeground()) {
            foreground += 1000;
        }
        if (detoured && calls == 0 && foreground >= 10000) {
            Log("o Present desviado nao foi chamado em 10 s de jogo -- usando a vtable do device");
            detoured = false;
            IDirect3DDevice9* now = *reinterpret_cast<IDirect3DDevice9**>(GTA_DEVICE);
            if (now) {
                HookVtable(now);
            }
        }
        if (calls > 0 && !silentLogged && GetTickCount() - g_lastPresent > 3000 && GameInForeground() &&
            !PauseMenuOpen()) {
            Log("vigia: o Present nao chama ha 3 s (%ld quadros ate agora, gancho: %s)", calls, g_hookMode);
            silentLogged = true;
        } else if (silentLogged && GetTickCount() - g_lastPresent < 1000) {
            Log("vigia: o Present voltou");
            silentLogged = false;
        }
    }
}

} // namespace

void OverlayStart(HMODULE module) {
    g_module = module;
    CreateThread(nullptr, 0, Boot, nullptr, 0, nullptr);
}
