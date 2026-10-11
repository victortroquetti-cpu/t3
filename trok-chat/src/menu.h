// Trok Chat (.asi) -- Victor_Trok
// O menu do /trokchat: as duas teclas que abrem o chat, como as mensagens aparecem e o Trok Chat ligado ou nao.
// Sem windows.h: o teste de renderizacao compila no Linux.
#pragma once

namespace menu {

void Toggle();
bool IsOpen();     // aberto: o teclado e o mouse sao dele (com o cursor do SA-MP)
bool WantsFrame(); // aberto ou com um aviso na tela
void Frame();      // dentro do quadro do ImGui

} // namespace menu
