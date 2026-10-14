// Trok Hitmarker (.asi) -- Victor_Trok
// As imagens da pasta "Trok Hitmarker\mira" (mira, acerto, colete, cabeca, abate, corpo) como texturas do ImGui.
// Qualquer tamanho e qualquer PNG com transparencia; editou ou trocou o arquivo, vale em ate um segundo.
#pragma once

#include <windows.h>
#include <d3d9.h>

namespace images {

void Init(const char* folder);           // cria a pasta com as imagens padrao na primeira vez
void Update(IDirect3DDevice9* device, float dt); // carrega (e recarrega o que mudou na pasta)
void Release();                          // o device trocou: as texturas saem e voltam no proximo Update

} // namespace images
