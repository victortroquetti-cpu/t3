// Trok Shadows Menu (.asi) -- Victor_Trok
// Declaracoes compartilhadas do .asi: log, escrita na memoria do jogo, INI e as partes do mod.
#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>

#include "settings.h"

#define TSM_VERSION "1.1"

// ---------------------------------------------------------------- log (log.cpp)
void LogOpen(const char* path);
void Log(const char* fmt, ...);

// ---------------------------------------------------------------- memoria do jogo (patch.cpp)
namespace patch {
bool Readable(uintptr_t addr, size_t size);
void Write(uintptr_t addr, const void* data, size_t size);
uintptr_t CallTarget(uintptr_t site); // destino do E8/E9 no endereco, ou 0
void SetCall(uintptr_t site, const void* fn);
void SetJump(uintptr_t site, const void* fn);
} // namespace patch

// ---------------------------------------------------------------- shadows.ini (ini.cpp)
// Grava as chaves que mudaram de before para s, trocando so o valor: comentarios, ordem e o resto ficam como estao.
bool IniWrite(const char* path, const Settings& s, const Settings& before);

// ---------------------------------------------------------------- Shadows Extender (se.cpp)
void SeFrame();   // evento de quadro: procura o Shadows Extender, entra no desenho dele e aplica o que estiver pendente
void SePresent(); // a cada Present: faz o mesmo se o evento de quadro nao chegar (outro mod tomou a chamada)

// ---------------------------------------------------------------- menu na tela (overlay.cpp)
void OverlayStart(HMODULE module);
