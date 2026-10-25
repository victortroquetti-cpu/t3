// Trok DevTools (.asi) -- Victor_Trok
// O menu (/trokdevtools ou F10) no kit da casa: abas Modelos, Texturas, Animacoes, Audios, Arquivos e Geral (so Modelos
// e Geral funcionam por enquanto), a grade de cards com busca e filtros, o card aberto com o visualizador 3D, as
// informacoes, as texturas e o "criar no mundo"; e, com o menu fechado, as informacoes do que esta na mira.
#pragma once

namespace devtools {

void Init(const char* iniPath, const char* gameDir);
void Toggle();
bool CapturesInput(); // menu aberto (o jogo nao recebe teclas nem cliques)
bool WantsFrame();    // ha algo para desenhar (menu, inspetor ou aviso)
void Frame();         // dentro de um quadro do ImGui
const char* Version();

} // namespace devtools
