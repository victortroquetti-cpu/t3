// Trok DevTools (.asi) -- Victor_Trok
// "Audio tocando agora": cada som que o jogo pede (CAESoundManager::RequestNewSound, 0x4EFB10) com o slot e o som; o
// banco que esta no slot vem do carregador de bancos do jogo (o CAEMP3BankLoader do AEAudioHardware, conferido pelo
// numero de bancos do BankLkup.dat). Guarda o historico (o mais novo primeiro) para o menu e para o aviso na tela.
#pragma once

#include <windows.h>

#include <deque>

namespace audiospy {

struct Played {
    int bank = -1, sound = -1; // -1 = nao deu para saber o banco
    int slot = -1, event = -1;
    bool frontend = false;
    float x = 0, y = 0, z = 0;
    DWORD first = 0, last = 0; // GetTickCount
    int count = 1;             // o mesmo som de novo em menos de 1,5 s soma aqui
    SYSTEMTIME time = {};      // hora local da primeira vez
};

bool Install();         // gancho (o MinHook ja inicializado)
void SetCapture(bool on); // desligado: o gancho nao guarda nada
void SetIgnoreFeet(bool on);
const std::deque<Played>& History();
unsigned Serial(); // muda a cada som novo ou repetido
void Clear();
bool BanksKnown(); // achou o carregador de bancos

} // namespace audiospy
