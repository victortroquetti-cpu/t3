#pragma once

// Trok KeyBinder -- o menu circular (como o do FiveM) e a bolinha no jogador da mira.
//
// O atalho abre o circulo no meio da tela com as binds marcadas "No menu" (as da visao so com alguem na mira; o
// nome dele fica no meio). Clicar numa fatia roda a bind, e o numero sutil dela (1-9) tambem, como nas listas da
// casa. Segurar o atalho, apontar e soltar tambem roda. Mais de 8 binds viram paginas (roda do mouse). Uma fatia de
// grupo abre o circulo do grupo; ali, o meio e o botao direito voltam. Esc, botao direito, clique no meio ou o atalho
// de novo fecham.

#include "binds.h"
#include "imgui.h"

#include <string>
#include <vector>

namespace radial {

struct Item {
    std::string label; // nome da bind
    std::string tag;   // "Na vis\xC3\xA3o", "Ao redor"... (vazio = nada)
    int bind = -1;     // indice da bind no preset
    int icon = -1;     // icons:: (-1 = sem icone)
    std::vector<Item> children; // grupo: abre outro circulo com estes (o meio volta)
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
// com o nick e o ID (so o numero) em cima, e segue o jogador na tela. Clique, numero (1-9), setas ou roda do mouse +
// Enter, ou segurar o atalho, apontar e soltar.
// Fecha com Esc, botao direito, clique fora, o atalho de novo ou o jogador sumindo da visao.
// id = o numero que fica ao lado do nome (vazio = nenhum); o mesmo no Open.
void OpenList(const std::vector<Item>& items, const std::string& title, const std::string& id, float time);
bool ListOpen();
// anchor = o meio do jogador na tela neste quadro; visible = false quando ele saiu da tela ou ficou longe demais.
int ListFrame(ImDrawList* dl, ImVec2 anchor, bool visible, float time, bool keyDown, bool pressedAgain, float size);
// O meio do item i da lista no ultimo quadro (para os testes).
ImVec2 ListItemCenter(int i);

// Os itens do circulo: as binds "No menu" que valem agora (no veiculo / a pe), na ordem do preset; com alvo, as da
// visao primeiro; as do mesmo grupo viram uma fatia de grupo (no lugar da primeira delas).
std::vector<Item> BuildCircle(const kb::Preset& p, bool target, bool inVehicle);
// Os itens da lista presa no jogador: as binds da visao "No menu" que valem agora.
std::vector<Item> BuildList(const kb::Preset& p, bool inVehicle);

// A bolinha no meio do corpo do jogador da mira.
void DrawDot(ImDrawList* dl, ImVec2 at, float time);

} // namespace radial
