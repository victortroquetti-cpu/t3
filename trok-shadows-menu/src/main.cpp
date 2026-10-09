// Trok Shadows Menu (.asi) -- Victor_Trok
// Complemento do Shadows Extender 2.0 (shadows.asi, DK22Pac). O shadows.asi fica como esta; este .asi:
//   - com a sombra desfocada, onde duas sombras se cruzam (o piloto e a moto, dois jogadores) escurece uma vez so;
//   - corrige o DisplayShadowsAtLowSettings do [STENCIL_SHADOWS], que o Shadows Extender nunca lia;
//   - poe um menu na tela (/sombras no SA-MP, F11 sem ele) para mudar o shadows.ini com o jogo aberto.
//
// Ordem: o DllMain liga o evento de quadro (Idle chamando CGame::Process) e o desenho do menu. A cada quadro o mod
// procura o Shadows Extender; quando ele termina de iniciar (no evento do RenderWare dele), o mod le os valores em
// uso e entra no desenho da sombra dele (se.cpp).

#include "tsm.h"
#include "game.h"

#include <cstring>

extern "C" {
extern uintptr_t g_tsmFrameOriginal;
void TsmFrameStub();
}

namespace {

void SiblingPath(HMODULE module, char* out, const char* fileName) {
    char path[MAX_PATH];
    GetModuleFileNameA(module, path, MAX_PATH);
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

} // namespace

extern "C" void TsmOnFrame() {
    SeFrame();
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }
    DisableThreadLibraryCalls(instance);

    char logPath[MAX_PATH];
    SiblingPath(instance, logPath, "Trok Shadows Menu.log");
    LogOpen(logPath);
    Log("Trok Shadows Menu " TSM_VERSION);

    // Duas copias do .asi (com nomes diferentes) ligariam tudo duas vezes.
    char marker[8];
    if (GetEnvironmentVariableA("TROK_SHADOWS_MENU_LOADED", marker, sizeof(marker))) {
        Log("outra copia do Trok Shadows Menu ja esta carregada -- esta ficou desligada");
        return TRUE;
    }
    SetEnvironmentVariableA("TROK_SHADOWS_MENU_LOADED", "1");

    // Idle chamando CGame::Process: o evento de quadro do mod (outro mod que ja desviou a chamada fica encadeado).
    if (!patch::Readable(0x53E981, 5) || game::At<uint8_t>(0x53E981) != 0xE8) {
        Log("o gta_sa.exe nao e o 1.0 US -- o Trok Shadows Menu ficou desligado (nada foi alterado)");
        return TRUE;
    }
    g_tsmFrameOriginal = patch::CallTarget(0x53E981);
    char owner[MAX_PATH] = "o jogo";
    HMODULE ownerModule = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(g_tsmFrameOriginal), &ownerModule) &&
        ownerModule && ownerModule != GetModuleHandleA(nullptr)) {
        char path[MAX_PATH];
        GetModuleFileNameA(ownerModule, path, MAX_PATH);
        const char* slash = strrchr(path, '\\');
        lstrcpynA(owner, slash ? slash + 1 : path, MAX_PATH);
    }
    Log("evento de quadro em 0x53E981 (antes chamava 0x%08X, de %s)", static_cast<unsigned>(g_tsmFrameOriginal), owner);
    patch::SetCall(0x53E981, reinterpret_cast<const void*>(&TsmFrameStub));
    OverlayStart(instance);
    return TRUE;
}
