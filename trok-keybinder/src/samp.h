#pragma once

// Trok KeyBinder -- o lado do samp.dll 0.3.7 R1 e do gta_sa.exe 1.0 US: quem esta conectado e perto, o estado do
// jogador local (vida, colete, veiculo, arma, interior), quem esta na mira, o que chega no chat e o envio das
// mensagens (CInput::Send, o mesmo caminho do que o jogador digita).

#include <windows.h>

#include <string>
#include <vector>

#include "engine.h"
#include "imgui.h"

namespace samp {

struct ServerInfo {
    bool connected = false;
    std::string address;  // "ip:porta" (a chave do preset)
    std::string hostname; // o nome do servidor (UTF-8)
};

// Liga o gancho do chat (o MinHook ja tem que estar iniciado). base = samp.dll 0.3.7 R1.
bool Install(DWORD base);

ServerInfo Server();
bool InputReady();     // o CInput existe (da para mandar)
bool ChatOpen();       // a caixa do chat (ou a barra do Trok Chat) aberta
bool DialogOpen();     // dialogo do servidor
bool ScoreboardOpen(); // placar (Tab)
int CursorMode();      // CGame: 0 = sem cursor do SA-MP

// Manda como se o jogador tivesse digitado (comando ou fala). UTF-8; cortado em 128 bytes.
void Send(const std::string& utf8);
// Aviso so para o jogador (tipo 8 do chat, como os mods da casa). Nao conta como mensagem para os gatilhos.
void Notice(const char* utf8);

kb::Me ReadMe(bool connected);

// Conectados (sem o local), com a distancia de quem esta no stream. A lista dos nomes e refeita a cada 250 ms.
void Players(DWORD now, std::vector<kb::Player>* out);
std::string LocalName();
int LocalId();

// O jogador mais perto da linha do centro da tela, ate maxDistance e sem parede no meio. dot = o meio do corpo
// dele na tela (a bolinha).
bool FindTarget(float maxDistance, float screenW, float screenH, kb::Player* out, ImVec2* dot);

// O meio do corpo de um jogador na tela (a lista presa nele segue isto). false = fora do stream, da tela ou mais
// longe que maxDistance.
bool PlayerDot(int id, float maxDistance, float screenW, float screenH, ImVec2* dot);

// Mensagens que chegaram no chat (falas e mensagens do servidor) desde a ultima chamada.
void TakeChat(std::vector<kb::ChatLine>* out);

} // namespace samp
