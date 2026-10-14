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
    int crosshair = 1;       // 0 a do jogo, 1 a do Trok: o mira\mira.png se existir, senao o ponto (como a Mira
                             // Centralizada); com a do Trok o GTA nao desenha a dele
    bool crosshairStatic = false; // a do jogo: nao abre enquanto atira (so o desenho; o espalhamento dos tiros e o mesmo)
    bool crosshairAlways = true; // ponto do Trok: aparece tambem sem mirar (a pe; some no carro, sniper e bazucas)
    int crosshairSize = 100; // %: a mira e tudo que aparece nela (acerto, cabeca, abate)

    // ---- Sinal de acerto na mira: uma imagem para cada (mira\acerto.png, colete.png, cabeca.png, abate.png); o visual
    // e o da imagem, o mod so faz o pulo, o sumir e o tamanho
    bool onAim = true;       // sinal de acerto onde a mira esta (le onde o GTA desenhou a mira neste quadro, entao
                             // segue mod que centraliza ou sobe a mira; sem opcao de lugar)
    // O que aparece na mira. Desligar colete, cabeca ou abate faz esse tiro mostrar o acerto normal; desligar o acerto
    // tira o sinal dos tiros normais.
    bool fxHit = true, fxArmour = true, fxHead = true, fxKill = true;
    int fxAnim = 0;          // animacao do sinal: 0 Pulo, 1 Expandir (como o Fortnite), 2 Soco, 3 Giro, 4 Nenhuma

    // ---- Numeros de dano
    int numbers = 0;         // 0 sobem do personagem (ao lado do acerto), 1 sobem na tela (ao lado do centro),
                             // 2 desligado, 3 nascem no local do tiro (o ponto exato onde a bala pegou)
    int numbersCount = 0;    // 0 um numero por tiro, 1 somando: um numero so por alvo que vai crescendo
    bool numbersSum = true;  // um por tiro: tiros seguidos (rajada) somam no mesmo numero
    bool splitHead = false;  // somando: o dano na cabeca num numero separado (dourado)
    int sumWindow = 2;       // s sem acertar ate a soma fechar
    int numbersAnim = 0;     // 0 Subir, 1 Arco (como o Fortnite), 2 Saltar, 3 Parado
    bool colorByHealth = false; // a cor dos numeros (e do informer) pela vida que sobrou: branco, amarelo, laranja,
                                // vermelho (a do alvo no dano dado, a sua no levado)
    int numbersSize = 100;   // %
    int numbersFont = 0;     // 0 Arial Black (como o damage informer antigo), 1 fonte da casa
    int source = 0;          // 0 dano real (vida + colete que o alvo perdeu), 1 dano que o GTA calcula

    // ---- Local do acerto: a marca fica na parte do corpo que o tiro pegou por alguns segundos, acompanhando o alvo
    bool hitLocal = true;
    int hitLocalTime = 2;    // s
    int hitLocalSize = 80;   // %: a imagem mira\corpo.png

    // ---- Informer (o layout do damage informer, com nome e combo)
    int informer = 0;        // 0 Classico, 1 Lista, 2 desligado
    int nickGiven = 0;       // 0 cor do jogador, 1 cor fixa
    int nickTaken = 0;
    bool afk = true;         // "AFK" no lugar do dano de quem esta pausado
    int informerSize = 100;

    bool combo = true;       // (x3 - 138) ao lado do numero: acertos e dano real somado no mesmo alvo
    int comboWindow = 3;     // s sem acertar ate o combo fechar
    int informerCount = 0;   // 0 cada tiro com o combo, 1 somando: um numero so que cresce (a cabeca separada se
                             // splitHead)

    // ---- Recebido
    bool direction = true;   // seta vermelha em volta do centro da tela apontando de onde veio o tiro
    bool edgeFlash = true;   // borda da tela ao levar dano, na cor do que foi atingido

    // ---- Cores (ARGB do ImGui)
    ImU32 cHealth = IM_COL32(255, 255, 255, 255);  // numero: dano na vida
    ImU32 cArmour = IM_COL32(150, 200, 255, 255);  // numero: dano no colete
    ImU32 cHead = IM_COL32(255, 200, 61, 255);     // numero: na cabeca
    ImU32 cKill = IM_COL32(255, 74, 74, 255);      // numero: abate
    ImU32 cGiven = IM_COL32(91, 227, 125, 255);    // nick (cor fixa) ao dar dano
    ImU32 cTaken = IM_COL32(255, 90, 90, 255);     // nick (cor fixa) e numero ao levar dano
    ImU32 cCombo = IM_COL32(255, 96, 96, 255);     // o "(x3 · 138)" do informer classico
    ImU32 cEdgeHealth = IM_COL32(220, 28, 36, 255); // borda: levou dano na vida
    ImU32 cEdgeArmour = IM_COL32(0, 0, 0, 255);     // borda: levou dano no colete
    ImU32 cEdgeHead = IM_COL32(40, 120, 255, 255);  // borda: levou tiro na cabeca

    // ---- Sons (pasta "Trok Hitmarker\sons", mp3 ou wav com estes nomes)
    bool sHit = true, sHeadDiff = true, sTaken = true, sKill = true, sKillHead = true, sDeath = true;
    int volume = 80;

    bool logAll = false;     // anota todos os acertos no Trok Hitmarker.log (para conferir o dano real no servidor)

    // ---- Posicoes: tudo preso a tela (so o sinal de acerto acompanha a mira)
    Place given = {0.60f, 0.47f};   // informer: dano dado, comeca aqui (a direita do centro, fora da seta)
    Place taken = {0.40f, 0.47f};   // informer: dano levado, termina aqui (a esquerda do centro)
};

Options& Get();
const Options& Defaults();

} // namespace opt
