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

    // ---- Mira: o hitmarker
    int marker = 0;          // 0 Classico (X), 1 Discreto, 2 Anel, 3 desligado
    int markerWhere = 0;     // 0 na mira, 1 no corpo (onde o tiro pegou), 2 nos dois
    int markerSize = 100;    // %
    bool killBanner = true;  // aviso de abate embaixo da mira, com a sequencia
    bool killStreak = true;

    // ---- Numeros de dano
    int numbers = 0;         // 0 sobem do personagem, 1 sobem na tela (ao lado da mira), 2 desligado
    bool numbersSum = true;  // tiros seguidos no mesmo alvo somam no mesmo numero
    int numbersSize = 100;   // %
    int numbersFont = 0;     // 0 Arial Black (como o damage informer antigo), 1 fonte da casa
    int source = 0;          // 0 dano real (vida + colete que o alvo perdeu), 1 dano que o GTA calcula

    // ---- Informer (o layout do damage informer, com nome e combo)
    int informer = 0;        // 0 Classico, 1 Lista, 2 desligado
    int nickGiven = 0;       // 0 cor do jogador, 1 cor fixa
    int nickTaken = 0;
    bool weapon = true;      // nome da arma ao lado do nick
    bool afk = true;         // "AFK" no lugar do dano de quem esta pausado
    int informerSize = 100;

    // ---- Combo no alvo (cartao com vida + colete reais e o dano somado)
    bool combo = true;
    int comboWindow = 3;     // s sem acertar ate o combo fechar
    bool bodyMap = true;     // bonequinho com as partes acertadas

    // ---- Recebido
    bool direction = true;   // arco apontando de onde veio o tiro
    bool directionName = true;
    bool edgeFlash = true;   // borda vermelha ao levar dano
    bool deathRecap = true;  // resumo da troca quando voce morre

    // ---- Cores (ARGB do ImGui)
    ImU32 cHealth = IM_COL32(255, 255, 255, 255);  // dano na vida
    ImU32 cArmour = IM_COL32(150, 200, 255, 255);  // dano no colete
    ImU32 cHead = IM_COL32(255, 200, 61, 255);     // na cabeca
    ImU32 cKill = IM_COL32(255, 74, 74, 255);      // abate
    ImU32 cGiven = IM_COL32(91, 227, 125, 255);    // nick (cor fixa) ao dar dano
    ImU32 cTaken = IM_COL32(255, 90, 90, 255);     // nick (cor fixa) e numero ao levar dano
    ImU32 cCombo = IM_COL32(255, 96, 96, 255);     // o "(x3 · 138)" do informer classico

    // ---- Sons (pasta "Trok Hitmarker\sons", mp3 ou wav com estes nomes)
    bool sHit = true, sHeadDiff = true, sTaken = true, sKill = true, sKillHead = true, sDeath = true;
    int volume = 80;

    // ---- Posicoes
    Place given = {0.60f, 0.40f};   // informer: dano dado (o lugar do damage informer antigo)
    Place taken = {0.30f, 0.40f};   // informer: dano levado
    Place comboAt = {0.53f, 0.50f}; // cartao do combo (embaixo da mira)
    Place banner = {0.50f, 0.68f};  // aviso de abate
    Place recap = {0.50f, 0.60f};   // resumo da morte
};

Options& Get();
const Options& Defaults();

} // namespace opt
