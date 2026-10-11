// Trok Chat (.asi) -- Victor_Trok
// As opcoes do Trok Chat (o menu do /trokchat muda, o "Trok Chat.ini" guarda). As do proprio SA-MP (linhas, hora e o
// F7) continuam no CChat e sao lidas e escritas por host::Settings e host::Apply; o INI guarda uma copia delas para
// valerem de novo quando o jogo abrir. Sem windows.h: o teste de renderizacao compila no Linux.
#pragma once

#include "imgui.h"

namespace opt {

struct Options {
    ImGuiKey keys[2] = {ImGuiKey_T, ImGuiKey_F6}; // as duas teclas que abrem o chat (como o T e o F6 do SA-MP)
    bool tags = true;                              // [conteudo] vira etiqueta
    bool enabled = true;                           // o Trok Chat no lugar do chat do SA-MP
};

Options& Get();
const Options& Defaults();

bool IsOpenKey(ImGuiKey key);

// Nome da tecla no INI e no menu ("T", "F6", "Enter"; "Nenhuma" sem tecla) e o contrario. Precisam do contexto do
// ImGui.
const char* KeyName(ImGuiKey key);
ImGuiKey KeyFromName(const char* name);

} // namespace opt
