// Trok Chat (.asi) -- Victor_Trok
// O chat do SA-MP 0.3.7 R1 desenhado com o kit da casa (Dear ImGui): mensagens com as cores do servidor, os [conteudos]
// virando tags, rolagem e barra de digitacao. O DllMain so abre o log e sobe a thread do desenho (overlay.cpp), que
// espera o SA-MP; sem o 0.3.7 R1 nada e alterado.

#include "tc.h"

#include <cstring>

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }
    DisableThreadLibraryCalls(instance);

    char path[MAX_PATH];
    GetModuleFileNameA(instance, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) {
        slash[1] = 0;
    } else {
        path[0] = 0;
    }
    lstrcpynA(path + strlen(path), "Trok Chat.log", static_cast<int>(MAX_PATH - strlen(path)));
    LogOpen(path);
    Log("Trok Chat " TC_VERSION);

    // Duas copias do .asi (com nomes diferentes) desenhariam dois chats.
    char marker[8];
    if (GetEnvironmentVariableA("TROK_CHAT_LOADED", marker, sizeof(marker))) {
        Log("outra copia do Trok Chat ja esta carregada -- esta ficou desligada");
        return TRUE;
    }
    SetEnvironmentVariableA("TROK_CHAT_LOADED", "1");
    OverlayStart(instance);
    return TRUE;
}
