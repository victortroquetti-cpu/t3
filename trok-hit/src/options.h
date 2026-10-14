// Trok Hitmarker (.asi) -- Victor_Trok
// As opcoes (o menu do /trokhit muda, o "Trok Hitmarker.ini" guarda, uma secao por servidor). Sem windows.h: o teste
// de renderizacao compila no Linux.
#pragma once

#include "imgui.h"

namespace opt {

// Onde cada coisa fica, em fracao da tela (vale em qualquer resolucao). O modo "Mover" do menu muda estes.
struct Place {
    float x, y;
};

struct Options {
    bool enabled = true;
    int preset = 0; // estilo pronto aplicado por ultimo (so para o menu mostrar)

    // ---- Mira
    int crosshair = 1;       // 0 a do jogo, 1 ponto do Trok (como a Mira Centralizada), 2 imagem (mira\mira.png);
                             // com 1 ou 2 o GTA nao desenha a dele
    bool crosshairStatic = false; // a do jogo: nao abre enquanto atira (so o desenho; o espalhamento dos tiros e o mesmo)
    bool crosshairAlways = true; // ponto do Trok: aparece tambem sem mirar (a pe; some no carro, sniper e bazucas)
    int crosshairSize = 100; // %
    ImU32 cCrosshair = IM_COL32(255, 255, 255, 255);

    // ---- Sinal de acerto na mira
    int marker = 0;          // 0 Ponto (o ponto muda: acerto, cabeca, abate), 1 Classico (X), 2 Discreto, 3 Anel,
                             // 4 imagens (mira\acerto.png, colete.png, cabeca.png, abate.png)
    bool onAim = true;       // sinal de acerto onde a mira esta (le onde o GTA desenhou a mira neste quadro, entao
                             // segue mod que centraliza ou sobe a mira; sem opcao de lugar)
    int markerSize = 100;    // %

    // ---- Numeros de dano
    int numbers = 0;         // 0 sobem do personagem, 1 sobem na tela (ao lado do centro), 2 desligado
    bool numbersSum = true;  // tiros seguidos no mesmo alvo somam no mesmo numero
    int numbersSize = 100;   // %
    int numbersFont = 0;     // 0 Arial Black (como o damage informer antigo), 1 fonte da casa
    int source = 0;          // 0 dano real (vida + colete que o alvo perdeu), 1 dano que o GTA calcula

    // ---- Local do acerto: a marca fica na parte do corpo que o tiro pegou por alguns segundos, acompanhando o alvo
    bool hitLocal = true;
    int hitLocalTime = 2;    // s
    int hitLocalSize = 80;   // %

    // ---- Informer (o layout do damage informer, com nome e combo)
    int informer = 0;        // 0 Classico, 1 Lista, 2 desligado
    int nickGiven = 0;       // 0 cor do jogador, 1 cor fixa
    int nickTaken = 0;
    bool afk = true;         // "AFK" no lugar do dano de quem esta pausado
    int informerSize = 100;

    bool combo = true;       // (x3 - 138) ao lado do numero: acertos e dano real somado no mesmo alvo
    int comboWindow = 3;     // s sem acertar ate o combo fechar

    // ---- Recebido
    bool direction = true;   // seta vermelha em volta do centro da tela apontando de onde veio o tiro
    bool edgeFlash = true;   // borda da tela ao levar dano, na cor do que foi atingido

    // ---- Cores (ARGB do ImGui)
    // As cores do acerto seguem a Mira Centralizada: acerto vermelho, cabeca azul, abate com o X vermelho.
    ImU32 cHealth = IM_COL32(255, 64, 64, 255);    // dano na vida
    ImU32 cArmour = IM_COL32(255, 255, 255, 255);  // dano no colete
    ImU32 cHead = IM_COL32(111, 182, 255, 255);    // na cabeca
    ImU32 cKill = IM_COL32(255, 59, 59, 255);      // abate
    ImU32 cGiven = IM_COL32(91, 227, 125, 255);    // nick (cor fixa) ao dar dano
    ImU32 cTaken = IM_COL32(255, 90, 90, 255);     // nick (cor fixa) e numero ao levar dano
    ImU32 cCombo = IM_COL32(255, 96, 96, 255);     // o "(x3 · 138)" do informer classico
    ImU32 cEdgeHealth = IM_COL32(220, 28, 36, 255); // borda: levou dano na vida
    ImU32 cEdgeArmour = IM_COL32(0, 0, 0, 255);     // borda: levou dano no colete
    ImU32 cEdgeHead = IM_COL32(40, 120, 255, 255);  // borda: levou tiro na cabeca

    // ---- Sons (pasta "Trok Hitmarker\sons", mp3 ou wav com estes nomes)
    bool sHit = true, sHeadDiff = true, sTaken = true, sKill = true, sKillHead = true, sDeath = true;
    int volume = 80;

    // ---- Posicoes: tudo preso a tela (so o sinal de acerto acompanha a mira)
    Place given = {0.60f, 0.47f};   // informer: dano dado, comeca aqui (a direita do centro, fora da seta)
    Place taken = {0.40f, 0.47f};   // informer: dano levado, termina aqui (a esquerda do centro)
};

Options& Get();
const Options& Defaults();

} // namespace opt
