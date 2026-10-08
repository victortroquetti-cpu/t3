#pragma once

#include <windows.h>

namespace showcase {

const char* Version();

// Monta as fontes da casa no atlas do ImGui para a altura de tela informada.
void BuildFonts(float screenHeight, HMODULE module);

void Toggle();

// true enquanto o menu (ou um dialogo de exemplo) esta aberto e precisa do mouse/teclado.
bool CapturesInput();

// true quando ha algo para desenhar (menu, modo de mover ou notificacoes na tela).
bool WantsFrame();

void Frame();

} // namespace showcase
