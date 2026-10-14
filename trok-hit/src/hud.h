// Trok Hitmarker (.asi) -- Victor_Trok
// O desenho de tudo que aparece na tela durante a troca de tiros: hitmarker, numeros, informer, combo, direcao do
// tiro, aviso de abate e resumo da morte. Recebe o estado pronto (o que aconteceu e ha quanto tempo); quem descobre
// os acertos e o dano real e o samp.cpp. Sem windows.h: o teste de renderizacao compila no Linux.
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

struct Marker {    // hitmarker
    ImVec2 body;   // onde o tiro pegou, na tela (para "no corpo")
    float age = 0; // s
    Kind kind = HEALTH;
};

struct Number {    // numero de dano que sobe
    ImVec2 from;   // de onde sobe (corpo do alvo ou do lado da mira)
    float age = 0;
    int value = 0;
    Kind kind = HEALTH;
    bool estimated = false; // o alvo nao mostrou a perda (servidor esconde a vida): "~46", o valor do GTA
    float drift = 0;        // -1..1: espalha os numeros para nao subirem um em cima do outro
    float bump = 0;         // s desde o ultimo tiro somado (o numero da um pulinho)
};

struct Hit {        // um acerto no informer
    float age = 0;
    int value = 0;
    Kind kind = HEALTH;
    bool estimated = false;
};

struct Line {       // uma pessoa no informer (dado ou levado)
    std::string nick;
    int id = 0;
    ImU32 nickColor = 0xFFFFFFFF; // a cor do jogador no SA-MP
    std::string weapon;
    bool afk = false;
    int hits = 0, total = 0;      // combo: acertos e dano somado
    float age = 0;                // desde o ultimo acerto
    std::vector<Hit> recent;      // numeros que ainda estao subindo
};

struct Combo {      // o alvo atual
    std::string nick;
    int id = 0;
    ImU32 nickColor = 0xFFFFFFFF;
    int damage = 0, hits = 0, heads = 0;
    int pool = 200;               // vida + colete quando o combo comecou (100 so com vida)
    int health = 100, armour = 0; // o que o alvo tem agora (do sync)
    int ghost = 0;                // o que acabou de sair (fica branco e escorre)
    float ghostAge = 1;
    float age = 0, window = 3;    // desde o ultimo acerto / quanto o combo espera
    bool kill = false, killHead = false;
    int zones[10] = {};           // acertos por parte do corpo
    std::string weapons;          // "Doze, M4"
};

struct Incoming {   // tiro levado: de onde veio
    float angle = 0; // rad: 0 = na frente, + = para a direita (sentido horario na tela)
    float age = 0;
    int damage = 0;
    std::string nick, weapon;
    ImU32 nickColor = 0xFFFFFFFF;
};

struct Banner {     // abate
    std::string nick;
    ImU32 nickColor = 0xFFFFFFFF;
    std::string weapon;
    bool head = false;
    int streak = 1;
    float age = 0;
};

struct Recap {      // resumo da morte
    std::string killer, weapon;
    ImU32 killerColor = 0xFFFFFFFF;
    bool head = false;
    int given = 0, givenHits = 0, taken = 0, takenHits = 0, takenHeads = 0;
    std::vector<std::string> hits; // os ultimos tiros levados: "M4 10"
    float age = 0;
};

struct State {
    ImVec2 crosshair;             // a mira do GTA (53% x 40% da tela, nao o centro)
    std::vector<Marker> markers;
    std::vector<Number> numbers;
    std::vector<Line> given, taken;
    bool hasCombo = false;
    Combo combo;
    std::vector<Incoming> incoming;
    float flash = 0;              // 0..1: borda vermelha
    bool hasBanner = false;
    Banner banner;
    bool hasRecap = false;
    Recap recap;
};

// Fontes extras no atlas (numeros grandes em Arial Black ou na fonte da casa). Chamar antes de construir o atlas.
void AddFonts(float screenHeight, const char* houseFont, const char* numberFont);
ImU32 KindColor(Kind k, const opt::Options& o);

void Draw(ImDrawList* dl, const State& s, const opt::Options& o, ImVec2 screen);

// Partes soltas (o menu desenha a previa com elas).
void DrawMarker(ImDrawList* dl, ImVec2 at, int style, float scale, float age, Kind kind, const opt::Options& o);
void DrawNumber(ImDrawList* dl, const Number& n, float scale, const opt::Options& o);
void DrawCombo(ImDrawList* dl, ImVec2 center, const Combo& c, float scale, const opt::Options& o);
void DrawBodyMap(ImDrawList* dl, ImVec2 topLeft, float height, const int* zones, const opt::Options& o, float alpha);

} // namespace hud
