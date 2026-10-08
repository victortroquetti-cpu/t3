// Trok Shadows (.asi) -- Victor_Trok
// Sombras melhores para o GTA SA 1.0 US (e SA-MP): refaz o Shadows Extender 2.0 do DK22Pac com codigo aberto,
// sem d3dx9_43.dll, com o INI aplicado na hora e log do que foi feito.
//
// Ordem: o DllMain confere a versao do jogo e liga 4 eventos (chamadas do jogo encadeadas). Depois que o
// RenderWare inicia, o mod le o INI e aplica tudo (hooks.cpp); quando o jogo termina de iniciar, religa a
// atualizacao das sombras em tempo real (o SA-MP desliga); a cada quadro confere se o INI mudou.

#include "trok.h"

#include <cstring>

namespace {

HMODULE g_module = nullptr;
bool g_ready = false;
char g_iniPath[MAX_PATH] = "";
char g_oldIniPath[MAX_PATH] = "";
FILETIME g_iniTime = {};
DWORD g_lastCheck = 0;

// Pasta do .asi + nome do arquivo.
void SiblingPath(char* out, const char* fileName) {
    char path[MAX_PATH];
    GetModuleFileNameA(g_module, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) {
        slash[1] = 0;
    } else {
        path[0] = 0;
    }
    lstrcpynA(out, path, MAX_PATH);
    const size_t len = strlen(out);
    lstrcpynA(out + len, fileName, static_cast<int>(MAX_PATH - len));
}

bool SampLoaded() {
    return GetModuleHandleA("samp.dll") != nullptr;
}

// O Shadows Extender mexe nos mesmos lugares: com os dois, o jogo trava ou fica com as sombras erradas.
bool OldModLoaded() {
    return GetModuleHandleA("shadows.asi") != nullptr;
}

void OnInitRw() {
    if (OldModLoaded()) {
        g_ready = false;
        Log("conflito: o shadows.asi (Shadows Extender) tambem esta instalado -- o Trok Shadows ficou desligado");
        MessageBoxW(nullptr,
                    L"O shadows.asi (Shadows Extender) também está instalado.\n"
                    L"O Trok Shadows ficou desligado para não brigar com ele.\n\n"
                    L"Tire o shadows.asi da pasta do jogo (o Trok Shadows faz tudo o que ele fazia).",
                    L"Trok Shadows", MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
        return;
    }
    ConfigLoad(g_cfg, g_iniPath, g_oldIniPath, SampLoaded());
    ConfigUpgrade(g_iniPath, g_cfg);
    ConfigFileTime(g_iniPath, &g_iniTime);
    Log("INI: %s", g_iniPath);
    ConfigLog(g_cfg);
    HooksApply();
    Log("pronto");
}

void OnInitGame() {
    HooksRestoreRealtimeUpdate();
}

void CheckIniChanged() {
    const DWORD now = GetTickCount();
    if (now - g_lastCheck < 1000) {
        return;
    }
    g_lastCheck = now;
    FILETIME t;
    if (!ConfigFileTime(g_iniPath, &t) || !CompareFileTime(&t, &g_iniTime)) {
        return;
    }
    g_iniTime = t;
    const Config before = g_cfg;
    Config next;
    ConfigLoad(next, g_iniPath, g_oldIniPath, SampLoaded());
    if (!next.autoReload) {
        if (before.autoReload) {
            Log("INI salvo com recarregar=0: as mudancas valem quando o jogo abrir de novo");
        }
        g_cfg.autoReload = false;
        return;
    }
    // MaxShadows, Raster, Blur e Gradient so sao usados quando o jogo cria as sombras: mudam ao reiniciar.
    g_cfg = next;
    Log("INI recarregado");
    ConfigLog(g_cfg);
    HooksApplyLive(before);
}

} // namespace

extern "C" void TrokOnEvent(int id) {
    if (!g_ready) {
        return;
    }
    switch (id) {
    case EVENT_INIT_RW:
        OnInitRw();
        break;
    case EVENT_SHUTDOWN_RW:
        HooksShutdown();
        break;
    case EVENT_INIT_GAME:
        OnInitGame();
        break;
    case EVENT_GAME_PROCESS:
        HooksFrame();
        CheckIniChanged();
        break;
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }
    g_module = instance;
    DisableThreadLibraryCalls(instance);

    char logPath[MAX_PATH];
    SiblingPath(logPath, "Trok Shadows.log");
    SiblingPath(g_iniPath, "Trok Shadows.ini");
    SiblingPath(g_oldIniPath, "shadows.ini");
    LogOpen(logPath);
    Log("Trok Shadows " TROK_SHADOWS_VERSION);

    // Duas copias do .asi (com nomes diferentes) remendariam o jogo duas vezes.
    char marker[8];
    if (GetEnvironmentVariableA("TROK_SHADOWS_LOADED", marker, sizeof(marker))) {
        Log("outra copia do Trok Shadows ja esta carregada -- esta ficou desligada");
        return TRUE;
    }
    SetEnvironmentVariableA("TROK_SHADOWS_LOADED", "1");

    if (!HooksVersionOk()) {
        Log("o gta_sa.exe nao e o 1.0 US -- o Trok Shadows ficou desligado (nada foi alterado)");
        return TRUE;
    }
    Log("gta_sa.exe 1.0 US confirmado");
    g_ready = true;
    HooksInstallEvents();
    return TRUE;
}
