#pragma once

// Trok Veiculos -- escolhas do jogador. Ficam no Trok Veiculos.ini, ao lado do .asi: a secao [padrao] vale para
// servidores novos e cada servidor (ip:porta) ganha a sua, com o que foi escolhido nele.

namespace veic {

struct Config {
    // Exibir
    bool enabled = true;
    int show = 0;          // 0 sempre, 1 so depois de um dano seu
    int damageHold = 6;    // s: quanto tempo a placa fica depois do seu ultimo dano
    bool hideMine = true;  // o veiculo em que voce esta (dirigindo ou de carona)
    bool empty = true;     // veiculos sem ninguem dentro
    int position = 0;      // 0 embaixo do veiculo, 1 em cima
    int size = 100;        // %
    int distance = 0;      // m; 0 = o alcance do /dl (20 m)
    int walls = 0;         // 0 some atras de parede, 1 como o /dl (aparece atraves da parede)
    bool fade = true;      // vai apagando no fim do alcance

    // Barras
    bool armour = true;    // blindagem: a vida acima de 1000
    int armourCap = 0;     // teto da blindagem: 0 o maior valor visto naquele veiculo, 1 fixo
    int armourFixed = 4000;
    bool stateColors = true; // cores da fumaca e do fogo
    bool fireLine = true;    // a marca dos 250 (abaixo dela o carro pega fogo)
    bool damageBar = true;   // o pedaco que saiu fica branco e escorre
    bool damageNumber = true;

    // Informacoes
    bool name = true;
    bool id = true;
    bool healthNumber = true;
    bool tires = true;
    bool lock = true;
    bool detailModel = false;
    bool detailDistance = false;
    bool detailSeats = false;
};

extern Config cfg;

// Le e grava o Trok Veiculos.ini. server = "ip:porta" ou nullptr (fora de um servidor).
void LoadConfig(const char* iniPath, const char* server);
void SaveConfig(const char* iniPath, const char* server);

} // namespace veic
