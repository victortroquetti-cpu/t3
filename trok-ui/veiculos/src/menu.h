#pragma once

// Trok Veiculos -- menu (/trokveiculos), no kit da casa.

#include "samp.h"

namespace menu {

const char* Version();

void Toggle();

// true com o menu aberto (ele precisa do mouse e do teclado).
bool CapturesInput();

// Desenha o menu, se aberto. Devolve true quando alguma escolha mudou (para gravar no Trok Veiculos.ini).
bool Frame(const samp::ServerInfo& server);

} // namespace menu
