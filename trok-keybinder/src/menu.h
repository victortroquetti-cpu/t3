#pragma once

// Trok KeyBinder -- o menu (/trokbinder), no kit da casa: uma aba por classe de bind (Padrao, Automatico, Ao redor,
// Na visao), Macro (em breve), Presets (o preset de cada servidor) e Geral.

#include "binds.h"

#include <string>
#include <vector>

namespace menu {

const char* Version();

void Toggle();
bool IsOpen();

struct Context {
    kb::Data* data = nullptr;
    int preset = 0;            // o preset em uso (indice em data->presets)
    bool connected = false;
    std::string address;       // "ip:porta" do servidor
    std::string hostname;
    std::vector<kb::Player> players; // conectados, com a distancia de quem esta perto
};

// Desenha o menu, se aberto. true = algo mudou (gravar o arquivo).
bool Frame(Context& c);

} // namespace menu
