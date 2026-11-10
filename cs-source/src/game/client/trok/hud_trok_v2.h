//=============================================================================
// Trok: HUD em jogo, versão 2 (prévia em design/ui.md, seção HUD; pesquisa em
// design/hud-pesquisa.md). Ganchos para os outros elementos do HUD.
//=============================================================================
#ifndef HUD_TROK_V2_H
#define HUD_TROK_V2_H
#ifdef _WIN32
#pragma once
#endif

// true com trok_hud_estilo 2 (padrão): o HUD novo desenha a partida e o antigo
// (hud_trok.cpp) só desenha a tela de morte.
bool TrokHudV2_Ativo( void );

// Prompt de interação no estilo do HUD novo ("[E] PEGAR FUZIL"), desenhado na hora (chamar
// de dentro de um Paint). y em fração da altura da tela.
void TrokHudV2_Prompt( const char *pszTecla, const char *pszVerbo, const char *pszObjeto, float flY = 0.62f );

// Outros sistemas empilham um evento à direita da mira ("ABATE +$300", "DEGRAU 5").
// pszValor pode ser NULL. Cor: alfa 0 = branco.
void TrokHudV2_Evento( const char *pszRotulo, const char *pszValor = NULL, const char *pszDetalhe = NULL, Color cor = Color( 0, 0, 0, 0 ) );

#endif // HUD_TROK_V2_H
