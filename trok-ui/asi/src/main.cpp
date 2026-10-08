// Trok UI Showcase (.asi) -- Victor_Trok
// Liga o ImGui no device do GTA SA (Present/Reset), registra /trokui no SA-MP 0.3.7 R1
// e entrega o desenho para showcase.cpp. Sem SA-MP (single player) o menu abre no F10.

#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdarg>

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "showcase.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

// Enderecos do gta_sa.exe 1.0 US (os mesmos que os mods da casa usam).
constexpr DWORD GTA_DEVICE = 0xC97C28;      // IDirect3DDevice9*
constexpr DWORD GTA_WINDOW = 0xC8CF88;      // HWND da janela do jogo
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
typedef void(__cdecl* CmdProc)(const char*);
typedef void(__thiscall* AddCommandFn)(void*, const char*, CmdProc);
typedef void(__thiscall* SetCursorFn)(void*, int, BOOL);
typedef void(__thiscall* UnlockCamFn)(void*);

PresentFn g_present = nullptr;
ResetFn g_reset = nullptr;
WNDPROC g_wndProc = nullptr;
HWND g_window = nullptr;
HMODULE g_module = nullptr;
DWORD g_samp = 0;
bool g_sampR1 = false;
bool g_ready = false;
bool g_initFailed = false;
bool g_cursorOn = false;
volatile LONG g_togglePending = 0;
float g_fontScreenH = 0.0f;
char g_logPath[MAX_PATH] = {};

void Log(const char* fmt, ...) {
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

float ScreenHeight() {
    int h = *reinterpret_cast<int*>(GTA_SCREEN_H);
    return h > 1 ? static_cast<float>(h) : 1080.0f;
}

bool PauseMenuOpen() {
    return *reinterpret_cast<BYTE*>(GTA_MENU_ACTIVE) != 0;
}

void SampCursor(bool on) {
    if (!g_sampR1) {
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

void __cdecl CmdTrokUI(const char*) {
    InterlockedExchange(&g_togglePending, 1);
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
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

        if (!g_sampR1 && msg == WM_KEYDOWN && wParam == VK_F10 && !(lParam & (1 << 30))) {
            InterlockedExchange(&g_togglePending, 1);
            return 0;
        }

        // Com o menu aberto o jogo nao recebe teclas nem cliques. Teclas soltas (WM_KEYUP) sempre passam:
        // o GTA le o teclado pelas mensagens da janela e uma tecla sem "soltar" ficaria presa.
        if (showcase::CapturesInput() && !PauseMenuOpen()) {
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

void InitImGui(IDirect3DDevice9* device) {
    g_window = *reinterpret_cast<HWND*>(GTA_WINDOW);
    if (!g_window || !IsWindow(g_window)) {
        return;
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.MouseDrawCursor = false;

    if (!ImGui_ImplWin32_Init(g_window) || !ImGui_ImplDX9_Init(device)) {
        Log("imgui: backend falhou -- desistindo");
        g_initFailed = true;
        return;
    }

    g_fontScreenH = ScreenHeight();
    showcase::BuildFonts(g_fontScreenH, g_module);

    g_wndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    if (!g_wndProc) {
        Log("imgui: SetWindowLongPtr falhou -- sem entrada");
        g_initFailed = true;
        return;
    }

    g_ready = true;
    Log("imgui pronto (device 0x%08X, janela 0x%08X, tela de %.0f px)",
        reinterpret_cast<DWORD>(device), reinterpret_cast<DWORD>(g_window), g_fontScreenH);
}

HRESULT __stdcall HookPresent(IDirect3DDevice9* device, const RECT* src, const RECT* dst, HWND wnd,
                              const RGNDATA* dirty) {
    if (!g_ready && !g_initFailed) {
        InitImGui(device);
    }

    if (g_ready && device->TestCooperativeLevel() == D3D_OK) {
        if (InterlockedExchange(&g_togglePending, 0)) {
            if (PauseMenuOpen()) {
                Log("/trokui ignorado: menu de pausa aberto");
            } else {
                showcase::Toggle();
            }
        }

        float screenH = ScreenHeight();
        if (screenH != g_fontScreenH) {
            Log("resolucao mudou (%.0f -> %.0f px) -- remontando as fontes", g_fontScreenH, screenH);
            g_fontScreenH = screenH;
            ImGui_ImplDX9_InvalidateDeviceObjects();
            showcase::BuildFonts(screenH, g_module);
            ImGui_ImplDX9_CreateDeviceObjects();
        }

        bool paused = PauseMenuOpen();
        bool wantCursor = showcase::CapturesInput() && !paused;
        if (wantCursor) {
            SampCursor(true); // todo quadro: chat e outros mods tentam devolver o cursor
        } else if (g_cursorOn) {
            SampCursor(false);
        }
        g_cursorOn = wantCursor;
        ImGui::GetIO().MouseDrawCursor = wantCursor && !g_sampR1;

        // O ImGui roda todo quadro (mesmo sem nada na tela) para a fila de entrada nao acumular
        // e o estado das teclas ficar em dia; so desenha quando ha algo a mostrar.
        bool draw = showcase::WantsFrame() && !paused;
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (draw) {
            showcase::Frame();
        }
        ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
        ImGui::EndFrame();
        if (draw) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        }
        PublishCursor(!wantCursor ? 0
                      : cursor == ImGuiMouseCursor_Hand ? 1
                      : cursor == ImGuiMouseCursor_TextInput ? 2 : 0);
    }
    return g_present(device, src, dst, wnd, dirty);
}

HRESULT __stdcall HookReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
    if (g_ready) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
    HRESULT result = g_reset(device, params);
    if (g_ready && SUCCEEDED(result)) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    return result;
}

bool IsSampR1(DWORD base) {
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return nt->OptionalHeader.AddressOfEntryPoint == SAMP_R1_ENTRY;
}

DWORD WINAPI Boot(LPVOID) {
    IDirect3DDevice9* device = nullptr;
    for (int i = 0; i < 1200 && !device; ++i) {
        device = *reinterpret_cast<IDirect3DDevice9**>(GTA_DEVICE);
        if (!device) {
            Sleep(100);
        }
    }
    if (!device) {
        Log("o render nao subiu em 2 minutos -- desistindo");
        return 0;
    }

    void** vtable = *reinterpret_cast<void***>(device);
    DWORD old;
    if (!VirtualProtect(&vtable[16], sizeof(void*) * 2, PAGE_EXECUTE_READWRITE, &old)) {
        Log("vtable nao pode ser alterada -- desistindo");
        return 0;
    }
    g_reset = reinterpret_cast<ResetFn>(vtable[16]);
    g_present = reinterpret_cast<PresentFn>(vtable[17]);
    vtable[16] = reinterpret_cast<void*>(HookReset);
    vtable[17] = reinterpret_cast<void*>(HookPresent);
    VirtualProtect(&vtable[16], sizeof(void*) * 2, old, &old);
    Log("Present e Reset encadeados (device 0x%08X)", reinterpret_cast<DWORD>(device));

    for (int i = 0; i < 300 && !g_samp; ++i) {
        g_samp = reinterpret_cast<DWORD>(GetModuleHandleA("samp.dll"));
        if (!g_samp) {
            Sleep(100);
        }
    }
    if (!g_samp) {
        Log("samp.dll nao apareceu (single player?) -- o menu abre no F10");
        return 0;
    }
    if (!IsSampR1(g_samp)) {
        Log("samp.dll nao e o 0.3.7 R1 -- sem /trokui, o menu abre no F10");
        return 0;
    }

    for (int i = 0; i < 1200; ++i) {
        void* input = *reinterpret_cast<void**>(g_samp + SAMP_INPUT);
        void* info = *reinterpret_cast<void**>(g_samp + SAMP_INFO);
        if (input && info) {
            reinterpret_cast<AddCommandFn>(g_samp + SAMP_ADD_COMMAND)(input, "trokui", CmdTrokUI);
            g_sampR1 = true;
            Log("/trokui registrado (CInput 0x%08X)", reinterpret_cast<DWORD>(input));
            return 0;
        }
        Sleep(100);
    }
    Log("CInput do SA-MP nao apareceu -- sem /trokui, o menu abre no F10");
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        GetModuleFileNameA(module, g_logPath, MAX_PATH);
        char* slash = strrchr(g_logPath, '\\');
        if (slash) {
            strcpy(slash + 1, "Trok UI Showcase.log");
        } else {
            g_logPath[0] = 0;
        }
        Log("Trok UI Showcase .asi v%s -- esperando o render e o samp.dll", showcase::Version());
        CreateThread(nullptr, 0, Boot, nullptr, 0, nullptr);
    }
    return TRUE;
}
