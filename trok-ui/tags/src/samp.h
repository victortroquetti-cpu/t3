#pragma once

// Trok Tags -- o lado do samp.dll 0.3.7 R1: esconde as originais (nametag, barras e AFK, textos 3D presos a
// jogadores, baloes de fala) e le os dados que o SA-MP ja tem para o overlay desenhar os nossos.
//
// O mod nunca mostra mais do que o SA-MP mostraria: a nametag so aparece para quem o proprio SA-MP decidiu
// desenhar neste quadro (o gancho do CPlayerTags::DrawLabel ve exatamente quem passou pelos limites do
// servidor: distancia, parede, nametags ligadas para aquele jogador); textos fixos e baloes usam a
// distancia e o "atras de parede" que o servidor mandou em cada um.

#include <windows.h>

#include <vector>

#include "overlay.h"

namespace samp {

struct ServerInfo {
    bool connected = false;
    char address[300] = ""; // "ip:porta" (a chave do perfil no Trok Tags.ini)
    bool nameTags = true;   // ShowNameTags do servidor
    float nameTagDistance = 70.0f;
    bool nameTagLos = false;  // nametags somem atras de parede
    int bubbles = 0;          // estrutura dos baloes: 0 ainda nao vista, 1 conferida, -1 nao conferiu (ficam os do SA-MP)
};

// Instala os ganchos (o MinHook ja tem que estar iniciado). base = samp.dll 0.3.7 R1.
bool Install(DWORD base);

// Comeco do quadro do GTA (BeginScene): recolhe os baloes novos antes de o SA-MP desenhar.
void BeginFrame();

// Liga a troca neste quadro (chamar todo quadro em que o overlay desenha). Sem essa chamada por 300 ms, as
// originais voltam sozinhas (e os baloes recolhidos voltam para o SA-MP no proximo BeginScene): o mod nunca
// deixa a tela sem nametag nem balao se o desenho dele parar.
void Keep(bool nametags, bool fixed, bool bubbles);

// Monta as pilhas deste quadro (no Present, depois de o SA-MP desenhar). userDistance = 0: limite do servidor.
void Collect(std::vector<tags::Stack>& out, float screenW, float screenH, float userDistance);

// Fim do quadro (depois do Collect).
void EndFrame();

ServerInfo Server();

// Dialogo do servidor ou placar (Tab) na tela: o overlay some (ele desenha por cima de tudo e ficaria em cima
// deles; as nametags do SA-MP ficam atras).
bool Covered();

} // namespace samp
