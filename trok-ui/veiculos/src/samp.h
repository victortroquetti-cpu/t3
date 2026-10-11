#pragma once

// Trok Veiculos -- o lado do samp.dll 0.3.7 R1: le os veiculos que o SA-MP tem na sua area e monta as placas. O mod
// so le: nada do SA-MP e escondido nem trocado (o /dl continua funcionando por fora).
//
// O mod nunca mostra mais do que o /dl mostraria: so ate 20 m de voce (o alcance do /dl, medido pela mesma funcao
// do SA-MP) e so o que o /dl ja mostra (modelo, ID, vida, distancia, lugares), mais o que esta visivel no carro
// (pneu furado, trancado). Atras de parede a placa some, a nao ser que voce escolha "como o /dl".

#include <windows.h>

#include <vector>

#include "overlay.h"

namespace samp {

constexpr float DL_RANGE = 20.0f; // m: o alcance do /dl do SA-MP 0.3.7

struct ServerInfo {
    bool connected = false;
    char address[300] = ""; // "ip:porta" (a chave do perfil no Trok Veiculos.ini)
};

// samp.dll 0.3.7 R1 (nenhum gancho: o mod so le).
void Install(DWORD base);

// Monta as placas deste quadro. now = segundos (para o dano e o fim do "ao dar dano").
void Collect(std::vector<veic::Card>& out, float screenW, float screenH, float now);

ServerInfo Server();

// Dialogo do servidor ou placar (Tab) na tela: as placas somem (elas desenham por cima de tudo).
bool Covered();

} // namespace samp
