// Trok DevTools (.asi) -- Victor_Trok
// "Audio tocando agora": cada som que o jogo pede (CAESoundManager::RequestNewSound, 0x4EFB10) com o slot e o som; o
// banco que esta no slot vem do carregador de bancos do jogo (o CAEMP3BankLoader do AEAudioHardware, conferido pelo
// numero de bancos do BankLkup.dat). Guarda o historico (o mais novo primeiro) para o menu e para o aviso na tela.
#pragma once

#include <windows.h>

#include <deque>
#include <string>

namespace audiospy {

enum Kind { K_GAME = 0, K_SAMP_SOUND = 1, K_SAMP_STREAM = 2 };

struct Played {
    int kind = K_GAME; // som do jogo, PlayerPlaySound do servidor ou stream (URL) do servidor
    int bank = -1, sound = -1; // -1 = nao deu para saber o banco (no PlayerPlaySound, sound = o id)
    int slot = -1, event = -1;
    bool frontend = false;
    float x = 0, y = 0, z = 0;
    DWORD first = 0, last = 0; // GetTickCount
    int count = 1;             // o mesmo som de novo em menos de 1,5 s soma aqui
    SYSTEMTIME time = {};      // hora local da primeira vez
    std::string url;           // o stream do servidor
};

struct Stats {
    int seen = 0;    // pedidos de som do jogo desde o comeco
    int ignored = 0; // passos (com o "ignorar os passos")
    int samp = 0;    // sons e streams do servidor
};

bool Install();         // gancho (o MinHook ja inicializado)
// Os sons do servidor no SA-MP 0.3.7 R1 (SAMP-API): CAudio::Play (o PlayerPlaySound) e CAudioStream::Play (a URL).
bool InstallSamp(DWORD sampBase);
void SetCapture(bool on); // desligado: o gancho nao guarda nada
void SetIgnoreFeet(bool on);
const std::deque<Played>& History();
unsigned Serial(); // muda a cada som novo ou repetido
void Clear();
bool BanksKnown(); // achou o carregador de bancos
Stats Counts();

} // namespace audiospy
