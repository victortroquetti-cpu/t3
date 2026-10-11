// Trok Chat (.asi) -- Victor_Trok
// Declaracoes compartilhadas do .asi: versao, log, o "Trok Chat.ini" e o desenho na tela.
#pragma once

#include <windows.h>

#define TC_VERSION "1.2"

namespace host {
struct ChatSettings;
}

// ---------------------------------------------------------------- log (log.cpp)
void LogOpen(const char* path);
void Log(const char* fmt, ...);

// ---------------------------------------------------------------- Trok Chat.ini (ini.cpp)
namespace ini {
void SetPath(const char* path);
bool Load(host::ChatSettings* chat); // as opcoes vao para opt::Get(); true se havia a copia das do SA-MP
bool Save(const host::ChatSettings& chat);
} // namespace ini

// ---------------------------------------------------------------- desenho na tela (overlay.cpp)
void OverlayStart(HMODULE module);
