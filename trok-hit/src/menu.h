// Trok Hitmarker (.asi) -- Victor_Trok
// O menu do /trokhit, no kit da casa, e o modo Mover (as caixas do informer arrastadas com o mouse).
#pragma once

namespace menu {

// O que o menu pede e so o overlay faz (abrir pasta, tocar os sons).
enum Request { REQ_NONE, REQ_OPEN_IMAGES, REQ_OPEN_SOUNDS, REQ_PLAY_SOUNDS };

void Toggle();
void SetTab(int tab); // teste de renderizacao
void StartMove();     // teste de renderizacao
bool CapturesInput(); // menu aberto ou movendo
bool WantsFrame();    // tambem com um aviso na tela
bool Frame();         // true quando alguma opcao mudou
int TakeRequest();
const char* Version();

} // namespace menu
