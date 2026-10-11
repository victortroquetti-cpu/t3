#pragma once

// Trok Tags -- escolhas do jogador. Ficam no Trok Tags.ini, ao lado do .asi: a secao [padrao] vale para
// servidores novos e cada servidor (ip:porta) ganha a sua, com o que foi escolhido nele.

#include "imgui.h"

namespace tags {

struct Config {
    // Nametag
    bool enabled = true; // desligado: as originais do SA-MP voltam
    int style = 0;       // 0 pilula, 1 limpo
    int colorMode = 0;   // 0 cor do jogador na faixa, 1 no nome
    bool showId = true;
    bool bars = true;
    bool nameTags = true; // [PM]Nome vira etiqueta
    int size = 100;       // %
    int distance = 0;     // m; 0 = o limite do servidor
    bool fade = true;

    // Baloes de fala
    bool bubbles = true;
    int bubbleWidth = 100; // % da largura padrao
    bool bubbleTags = true;
    bool ooc = true;
    int bubbleOpacity = 86; // %

    // Textos fixos (texto 3D preso ao jogador)
    bool fixed = true;
    bool fixedTags = true; // [conteudo] vira etiqueta
    int fixedWord = 0;     // "PALAVRA:" no comeco (o "RG:"): 0 etiqueta, 1 texto, 2 escondida
    int fixedChip = 0;     // 0 cor do texto, 1 azul, 2 branco
    int fixedText = 0;     // 0 cor do servidor, 1 branco
    // Na linha que fica no lugar da nametag, quando o servidor nao mostra a nametag daquele jogador.
    int fixedInitials = 0; // sigla do nome: 0 nao, 1 duas primeiras letras, 2 iniciais (Nome_Sobrenome)
    bool fixedId = false;  // ID do jogador no fim da linha
    int fixedIdColor = 0;  // sigla e ID: 0 azul, 1 branco, 2 a cor do texto

    // Indicadores
    bool afk = true;
    bool afkTime = true;
    bool typing = true;
    bool ping = true;
    int pingLimit = 250; // ms
    bool lowHealth = true;
};

extern Config cfg;

// Le e grava o Trok Tags.ini. server = "ip:porta" ou nullptr (fora de um servidor).
void LoadConfig(const char* iniPath, const char* server);
void SaveConfig(const char* iniPath, const char* server);

} // namespace tags
