// Trok Chat (.asi) -- Victor_Trok
// O chat do SA-MP desenhado com o kit da casa: as mensagens (com as cores {RRGGBB} do servidor e os [conteudos]
// virando tags), a rolagem e a barra de digitacao. Sem windows.h: o teste de renderizacao compila no Linux.
#pragma once

#include <cstdint>
#include <ctime>
#include <string>

namespace chat {

// Tipos de mensagem do SA-MP (CChat::ENTRY_TYPE_*).
enum Type { TYPE_CHAT = 2, TYPE_INFO = 4, TYPE_DEBUG = 8 };

struct Message {
    int type = TYPE_INFO;
    std::time_t time = 0;
    std::string prefix;                 // UTF-8: o nick, nas mensagens de jogador
    uint32_t prefixColor = 0xFFFFFFFF;  // ARGB, como o SA-MP guarda
    std::string text;                   // UTF-8, com os {RRGGBB} do servidor
    uint32_t textColor = 0xFFFFFFFF;
};

void Add(const Message& m); // de qualquer thread
void Open();                // a barra de digitacao (as teclas do chat, um script)
void Close();
bool IsOpen();
void SetInput(const char* utf8); // texto pronto na barra aberta (o de um script), com o cursor no fim
bool InputEmpty();
void Frame();               // dentro do quadro do ImGui

} // namespace chat

// O que o chat usa do SA-MP (samp.cpp; no teste de renderizacao, um falso).
namespace host {

struct ChatSettings {
    int pageSize = 10;       // /pagesize: linhas na tela
    int mode = 2;            // F7: 0 escondido com o chat fechado, 1 sem contorno, 2 com contorno
    bool timestamps = false; // /timestamp
};

ChatSettings Settings();
void Apply(const ChatSettings& s); // o menu do /trokchat muda as do SA-MP por aqui
void Send(const char* utf8);  // como a caixa de digitacao do SA-MP: comando ou mensagem
void InputOpened(bool open);  // "chat aberto" do SA-MP (o que os scripts consultam)
bool DialogOpen();            // dialogo do servidor na tela: a barra fecha
int Wheel();                  // roda do mouse do jogo neste quadro: +1 para cima, -1 para baixo

} // namespace host
