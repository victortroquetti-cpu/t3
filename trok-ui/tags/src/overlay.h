#pragma once

// Trok Tags -- desenho do que fica em cima dos personagens (nametag, baloes, textos fixos e indicadores).
// Nao sabe nada do SA-MP: recebe as pilhas prontas (samp.cpp monta) e desenha num ImDrawList. Assim o
// teste de renderizacao roda fora do jogo com pilhas de mentira.

#include "imgui.h"

#include <string>
#include <vector>

namespace tags {

struct Nametag {
    std::string name; // como o SA-MP mostra (pode comecar com [TAG]); UTF-8
    int id = 0;
    ImU32 color = IM_COL32_WHITE; // cor do jogador
    float health = 100.0f;
    float armour = 0.0f;
    bool afk = false;
    float afkSeconds = -1.0f; // ha quanto tempo esta parado (-1: nao sabe)
    int ping = 0;
    bool typing = false;
};

// Texto 3D que o servidor prendeu no jogador (UTF-8, com as cores {RRGGBB} dele).
struct FixedText {
    std::string text;
    ImU32 color = IM_COL32_WHITE;
    std::string before; // sigla do nome, antes do texto (so na linha que fica no lugar da nametag escondida)
    std::string after;  // ID do jogador, depois do texto (idem)
};

// Balao de fala (SetPlayerChatBubble).
struct Bubble {
    std::string text;
    ImU32 color = IM_COL32_WHITE;
    float alpha = 1.0f;  // some nos ultimos instantes
    bool typing = false; // balao de "digitando...": vira os tres pontinhos
};

// Tudo o que vai em cima de um jogador neste quadro.
struct Stack {
    ImVec2 at = ImVec2(0, 0); // ponto na tela logo acima da cabeca (base da pilha)
    float distance = 0.0f;    // da camera, em metros
    float limit = 70.0f;      // distancia maxima da nametag (a do servidor ou a sua, a menor)
    bool hasTag = false;
    Nametag tag;
    std::vector<FixedText> fixed; // de cima para baixo
    bool hasBubble = false;
    Bubble bubble;
};

// Icones do lucide que o kit nao carrega (lua do AFK). Chamar depois do tui::BuildFonts; depois, Build().
void AddFonts(const char* gameDir);

// Desenha as pilhas, da mais longe para a mais perto. time = segundos, para as animacoes.
void DrawStacks(ImDrawList* dl, std::vector<Stack>& stacks, float time);

// "digitando...", "typing...", "..." e parecidos (sem as cores).
bool IsTypingText(const char* text);

// Sigla do nome do jogador: mode 1 = as duas primeiras letras (VI), 2 = as iniciais de cada parte (VT, de
// Victor_Trok). A etiqueta do comeco ([PM]Nome) fica de fora.
std::string NameInitials(const std::string& name, int mode);

// Previa da aba Fixos do menu: dois textos fixos de exemplo com as escolhas atuais, numa caixa de largura w
// a partir de (x, y). Devolve a altura usada.
float DrawFixedPreview(ImDrawList* dl, float x, float y, float w);

} // namespace tags
