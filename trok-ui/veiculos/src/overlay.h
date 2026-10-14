#pragma once

// Trok Veiculos -- desenho da placa de cada veiculo (nome, ID, vida, pneus, cadeado e as barras de vida e de
// blindagem). Nao sabe nada do SA-MP: recebe as placas prontas (samp.cpp monta) e desenha num ImDrawList. Assim o
// teste de renderizacao roda fora do jogo com placas de mentira.

#include "imgui.h"

#include <string>
#include <vector>

namespace veic {

enum Kind { KIND_OTHER = 0, KIND_CAR = 1, KIND_BIKE = 2 };

// Pneus furados, como o SA-MP guarda (GetVehicleDamageStatus): no carro, dianteiro esquerdo 8, traseiro
// esquerdo 4, dianteiro direito 2 e traseiro direito 1; na moto, frente 2 e tras 1.
enum Tire { TIRE_FL = 8, TIRE_RL = 4, TIRE_FR = 2, TIRE_RR = 1, TIRE_FRONT = 2, TIRE_REAR = 1 };

struct Card {
    ImVec2 at = ImVec2(0, 0); // meio da borda de baixo do veiculo na tela (ou da de cima, com a placa em cima)
    float distance = 0.0f;    // de voce, em metros (a mesma conta do /dl)
    float limit = 20.0f;      // alcance
    std::string name;         // nome do modelo; vazio: so o ID
    int id = 0;               // ID do veiculo no SA-MP
    int model = 0;
    int seats = 0;            // lugares de passageiro (o PassengerSeats do /dl)
    float health = 1000.0f;   // vida total; o que passa de 1000 e blindagem
    float armourMax = 0.0f;   // teto da blindagem (0: sem barra de blindagem)
    int kind = KIND_OTHER;
    unsigned tires = 0;
    bool locked = false; // trancado para voce
    bool empty = false;  // ninguem dentro
    float ghost = 0.0f;  // vida antes do ultimo dano (a parte branca que escorre); 0: nenhuma
    float ghostAge = 0.0f;
    int damage = 0; // o seu dano que acabou de acontecer (o -120); 0: nenhum
    float damageAge = 0.0f;
    float alpha = 1.0f; // o fim do "ao dar dano" apaga a placa
};

// Icones do lucide que o kit nao carrega (cadeado, escudo, chama). Chamar depois do tui::BuildFonts; depois, Build().
void AddFonts(const char* gameDir);

// Desenha as placas, da mais longe para a mais perto. time = segundos, para o fogo pulsar.
void DrawCards(ImDrawList* dl, std::vector<Card>& cards, float time);

// Previa do menu: a placa de exemplo, com as escolhas atuais, com o topo em (cx, y). Devolve a altura.
float DrawCardPreview(ImDrawList* dl, float cx, float y, const Card& card, float time);
float CardPreviewHeight(const Card& card);

} // namespace veic
