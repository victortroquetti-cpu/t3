// Trok Hitmarker (.asi) -- Victor_Trok
// O desenho de tudo que aparece na tela durante a troca de tiros: sinal de acerto na mira, local do acerto no corpo,
// numeros, informer e direcao do tiro levado. Recebe o estado pronto (o que aconteceu e ha quanto tempo); quem descobre os acertos, o
// dano real e onde a mira esta e o samp.cpp. Sem windows.h: o teste de renderizacao compila no Linux.
#pragma once

#include "imgui.h"
#include "options.h"

#include <string>
#include <vector>

namespace hud {

// Partes do corpo como o SA-MP manda (3 tronco ... 9 cabeca).
enum Part { TORSO = 3, GROIN = 4, LEFT_ARM = 5, RIGHT_ARM = 6, LEFT_LEG = 7, RIGHT_LEG = 8, HEAD = 9 };
// Que cor o acerto ganha.
enum Kind { HEALTH, ARMOUR, HEADSHOT, KILL };

struct Marker {    // um acerto: o sinal na mira e a marca no corpo
    ImVec2 body;   // a parte do corpo que o tiro pegou, na tela neste quadro (segue o osso do alvo)
    bool bodyVisible = true; // a parte esta na frente da camera
    float age = 0; // s
    Kind kind = HEALTH;
    Kind under = HEALTH; // no abate: o que o tiro pegou (vale se o efeito de abate estiver desligado)
};

struct Number {    // numero de dano que sobe
    ImVec2 from;   // de onde sobe (corpo do alvo ou do lado da mira)
    float age = 0;
    int value = 0;
    Kind kind = HEALTH;
    bool estimated = false; // o alvo nao mostrou a perda (servidor esconde a vida): "~46", o valor do GTA
    float drift = 0;        // -1..1: espalha os numeros para nao subirem um em cima do outro
    float bump = 0;         // s desde o ultimo tiro somado (o numero da um pulinho)
    float stay = 0;         // s parado no lugar antes de comecar a sair (somando: enquanto a soma esta aberta)
    float life = -1;        // 0..1: a vida que sobrou ao alvo (cor pela vida); -1 = sem
    float lift = 0;         // px (tela de 1080) acima do ponto: o numero da cabeca fica em cima do do corpo
};

struct Hit {        // um acerto no informer
    float age = 0;
    int value = 0;
    Kind kind = HEALTH;
    bool estimated = false;
    float life = -1;    // 0..1: a vida que sobrou depois dele (cor pela vida)
};

struct Line {       // uma pessoa no informer (dado ou levado)
    std::string nick;
    int id = 0;
    ImU32 nickColor = 0xFFFFFFFF; // a cor do jogador no SA-MP
    bool afk = false;
    int hits = 0, total = 0;      // combo: acertos e dano somado
    int head = 0;                 // a parte do total que foi na cabeca
    bool estimated = false;       // alguma parte do total e o numero do GTA (~)
    Kind last = HEALTH;           // o ultimo acerto (a cor do total somando)
    float life = -1;              // a vida que sobrou depois do ultimo acerto (cor pela vida)
    float bump = 1;               // s desde o ultimo acerto (o total da um pulinho)
    float age = 0;                // desde o ultimo acerto
    std::vector<Hit> recent;      // numeros que ainda estao subindo
};

struct Incoming {   // tiro levado: de onde veio
    float angle = 0; // rad: 0 = na frente, + = para a direita (sentido horario na tela)
    float age = 0;
    int damage = 0;
};

struct State {
    ImVec2 crosshair;             // onde a mira esta neste quadro (no GTA puro, 53% x 40% da tela); so a mira e o
                                  // sinal de acerto usam: o resto fica preso ao centro da tela
    bool showCrosshair = false;   // ponto do Trok neste quadro (mirando, ou "sempre visivel" a pe com arma que tem mira)
    float spread = 1;             // a mira do Trok aberta pelos tiros (1 = parada), como a do jogo
    std::vector<Marker> markers;
    std::vector<Number> numbers;
    std::vector<Line> given, taken;
    std::vector<Incoming> incoming;
    float flash = 0;              // 0..1: borda da tela ao levar dano
    Kind flashKind = HEALTH;      // HEALTH, ARMOUR ou HEADSHOT: a cor da borda
};

// As imagens da pasta "Trok Hitmarker\mira" (PNG que o usuario edita; sem o arquivo, a padrao que vem no .asi). O
// overlay carrega e passa a textura. Cada imagem inteira ocupa uma caixa fixa (mira e efeitos 40 px numa tela de
// 1080, o corpo 48 px), vezes o Tamanho: qualquer resolucao de imagem serve. O visual e so o da imagem.
enum ImageSlot { IMG_CROSSHAIR, IMG_HIT, IMG_ARMOUR, IMG_HEAD, IMG_KILL, IMG_BODY, IMG_COUNT };
// u1/v1: ate onde a imagem vai na textura; aspect: largura / altura (imagem que nao e quadrada cabe inteira na caixa).
void SetImage(int slot, ImTextureID texture, float u1 = 1.0f, float v1 = 1.0f, float aspect = 1.0f);

// Fontes extras no atlas (numeros grandes em Arial Black ou na fonte da casa). Chamar antes de construir o atlas.
void AddFonts(float screenHeight, const char* houseFont, const char* numberFont);
ImU32 KindColor(Kind k, const opt::Options& o);
// Branco com a vida cheia, amarelo, laranja e vermelho perto do fim (life 0..1).
ImU32 HealthColor(float life);
// O efeito que um acerto mostra na mira, com o que estiver desligado no menu; -1 = nenhum.
int Effective(Kind kind, Kind under, const opt::Options& o);

void Draw(ImDrawList* dl, const State& s, const opt::Options& o, ImVec2 screen);

// Partes soltas (o menu desenha a previa com elas).
void DrawCrosshair(ImDrawList* dl, ImVec2 at, float scale, const opt::Options& o);
bool DrawEffect(ImDrawList* dl, ImVec2 at, float scale, float age, Kind kind, const opt::Options& o); // false: acabou
// A mira com o efeito no lugar dela (como no jogo), para as previas; age 99 = so a mira.
void DrawAimPreview(ImDrawList* dl, ImVec2 at, float scale, float age, Kind kind, const opt::Options& o);
void DrawBodyMarker(ImDrawList* dl, ImVec2 at, float scale, float age, float stay);
void DrawNumber(ImDrawList* dl, const Number& n, float scale, const opt::Options& o);

} // namespace hud
