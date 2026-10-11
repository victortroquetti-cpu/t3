// Trok Chat (.asi) -- Victor_Trok
// Declaracoes compartilhadas do .asi: versao, log e o desenho na tela.
#pragma once

#include <windows.h>

#define TC_VERSION "1.0"

// ---------------------------------------------------------------- log (log.cpp)
void LogOpen(const char* path);
void Log(const char* fmt, ...);

// ---------------------------------------------------------------- desenho na tela (overlay.cpp)
void OverlayStart(HMODULE module);
