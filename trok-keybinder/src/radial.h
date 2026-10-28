#pragma once

// Trok KeyBinder -- o menu circular (como o do FiveM) e a bolinha no jogador da mira.
//
// O atalho abre o circulo no meio da tela com as binds marcadas "No menu" (as da visao so com alguem na mira; o
// nome dele fica no meio). Clicar numa fatia roda a bind, e o numero sutil dela (1-9) tambem, como nas listas da
// casa. Segurar o atalho, apontar e soltar tambem roda. Mais de 8 binds viram paginas (roda do mouse). Esc, botao
// direito, clique no meio ou o atalho de novo fecham.

#include "imgui.h"

#include <string>
#include <vector>

namespace radial {

struct Item {
    std::string label; // nome da bind
    std::string tag;   // "Na vis\xC3\xA3o", "Ao redor"... (vazio = nada)
    int bind = -1;     // indice da bind no preset
};

void Open(const std::vector<Item>& items, const std::string& title, const std::string& subtitle, float time);
void Close();  // fecha o circulo e a lista
bool IsOpen(); // o circulo ou a lista

// Um quadro do circulo aberto. keyDown = o atalho ainda esta seguro (para o "segurar e soltar"); pressedAgain =
// o atalho foi apertado de novo (fecha). size = tamanho em % (Geral). Devolve o indice do item escolhido ou -1;
// fecha sozinho depois de escolher.
int Frame(ImDrawList* dl, ImVec2 center, float time, bool keyDown, bool pressedAgain, float size);

constexpr int PAGE = 8;

// A lista presa no jogador da mira (como os menus de alvo dos servidores RP de FiveM): sai da bolinha do meio dele,
// com o nick e o ID em cima, e segue o jogador na tela. Clique, setas + Enter ou segurar o atalho, apontar e soltar.
// Fecha com Esc, botao direito, clique fora, o atalho de novo ou o jogador sumindo da visao.
void OpenList(const std::vector<Item>& items, const std::string& title, const std::string& subtitle, float time);
bool ListOpen();
// anchor = o meio do jogador na tela neste quadro; visible = false quando ele saiu da tela ou ficou longe demais.
int ListFrame(ImDrawList* dl, ImVec2 anchor, bool visible, float time, bool keyDown, bool pressedAgain, float size);
// O meio do item i da lista no ultimo quadro (para os testes).
ImVec2 ListItemCenter(int i);

// A bolinha no meio do corpo do jogador da mira.
void DrawDot(ImDrawList* dl, ImVec2 at, float time);

} // namespace radial
