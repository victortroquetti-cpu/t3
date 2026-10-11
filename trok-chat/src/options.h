// Trok Chat (.asi) -- Victor_Trok
// As opcoes do Trok Chat (o menu do /trokchat muda, o "Trok Chat.ini" guarda). As do proprio SA-MP (linhas, hora e o
// F7) continuam no CChat e sao lidas e escritas por host::Settings e host::Apply; o INI guarda uma copia delas para
// valerem de novo quando o jogo abrir. Sem windows.h: o teste de renderizacao compila no Linux.
#pragma once

namespace opt {

struct Options {
    bool tags = true;    // [conteudo] vira etiqueta
    bool enabled = true; // o Trok Chat no lugar do chat do SA-MP
    // Canto de cima a esquerda do chat, em fracao da tela (vale em qualquer resolucao); -1 = no canto de sempre.
    float posX = -1.0f, posY = -1.0f;
};

Options& Get();
const Options& Defaults();

} // namespace opt
