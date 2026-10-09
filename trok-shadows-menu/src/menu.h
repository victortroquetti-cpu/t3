// Trok Shadows Menu (.asi) -- Victor_Trok
// O menu na tela. Desenhado com o kit da casa (trok-ui/asi/src/trok_ui.h) dentro do Present (overlay.cpp).
#pragma once

namespace menu {

// Monta as fontes da casa no atlas do ImGui para a altura de tela informada.
void BuildFonts(float screenHeight);

void Toggle();

// true enquanto o menu esta aberto e precisa do mouse e do teclado.
bool CapturesInput();

// true quando ha algo para desenhar (menu ou notificacoes).
bool WantsFrame();

void Frame();

} // namespace menu
