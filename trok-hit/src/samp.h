// Trok Hitmarker (.asi) -- Victor_Trok
// O lado do samp.dll 0.3.7 R1: os ganchos do dano que o seu SA-MP manda ao servidor (dado e levado), a mensagem de
// morte e a leitura dos jogadores (nick, cor, AFK, vida e colete do sync, personagem do GTA).
#pragma once

#include <windows.h>

#include <string>

#include "imgui.h"

namespace samp {

struct Events {
    void (*giveDamage)(int id, float damage, int weapon, int part);
    void (*takeDamage)(int id, float damage, int weapon, int part);
    void (*death)(const char* killer, const char* victim, int weapon); // nomes como o SA-MP guarda (CP_ACP)
    void (*otherHit)(int target, int weapon); // a bala de OUTRO jogador acertou este jogador (bullet sync)
};

bool Install(DWORD base, const Events& events);

struct ServerInfo {
    bool connected = false;
    char address[300] = "";
};
ServerInfo Server();
bool Covered(); // dialogo do servidor ou placar na tela

struct Player {
    std::string nick; // UTF-8
    std::string raw;  // como o SA-MP guarda (para comparar com a mensagem de morte)
    ImU32 color = 0xFFFFFFFF;
    bool afk = false;
    float health = 0, armour = 0; // o que o sync dele diz agora
    DWORD ped = 0;                // CPed do GTA (0 = fora do alcance)
};
bool GetPlayer(int id, Player* out); // jogador remoto conectado
// So o que muda a cada quadro (sem montar o nick): vida e colete do sync, e o personagem do GTA.
bool Vitals(int id, float* health, float* armour);
DWORD PedOf(int id);
std::string LocalNameRaw();
int FindByName(const char* raw);

} // namespace samp
