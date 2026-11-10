//=============================================================================
// Trok: série de abates (killstreak) como no Combat Arms.
//
// Fonte: design/chimera/specs/combat_arms/kill_streaks.md (Parte A, as regras; Parte B, os
// números do CA). Resumo:
// - o servidor conta, por jogador, o "multi-abate": abates de inimigo, cada um a menos de
//   trok_serie_janela (CA: 5 s) do anterior; abate de aliado e suicídio não contam nem zeram;
// - em cada abate o servidor manda a todos o evento "trok_serie" (resource/modevents.res) com
//   essa contagem, se foi na cabeça, se foi o primeiro abate, vingança e tiro de sorte, a série
//   desta vida e o total de abates do matador;
// - o cliente de quem matou escolhe UM evento por abate (primeiro abate > vingança > passo da
//   série > cabeça > abate simples), mostra a faixa animada no alto da tela e toca no máximo
//   uma voz; todos veem a linha do lado esquerdo para primeiro abate e passos da série
//   (client/trok/hud_trok_killstreak.cpp; textos, sons, níveis e tempos em
//   scripts/trok_anunciador.txt).
// O CA não tem recompensa de série (nada de ataque aéreo etc.); a série desta vida e a maior
// do mapa ficam em rede (trok_placar_extra) para o placar e a tela de morte.
// Regras e números em cfg/killstreak.cfg.
//=============================================================================
#ifndef TROK_KILLSTREAK_H
#define TROK_KILLSTREAK_H
#ifdef _WIN32
#pragma once
#endif

// Níveis da faixa (CA, kill_streaks.md B4): um pedido de nível menor que o da faixa que está
// na tela é descartado; igual ou maior troca na hora. Os valores ficam no script do
// anunciador; estes são os padrões.
enum TrokSerieNivel_t
{
	SERIE_NIVEL_ABATE = 1,		// abate simples ("ABATE" / "N ABATES")
	SERIE_NIVEL_ESPECIAL = 5,	// passos da série, tiro na cabeça
	SERIE_NIVEL_EVENTO = 6,		// primeiro abate, vingança
	SERIE_NIVEL_SORTE = 11,		// tiro de sorte
};

extern ConVar trok_serie_voz;		// replicada: o servidor liga ou desliga a voz do anunciador

#ifndef CLIENT_DLL
class CBasePlayer;
class CTakeDamageInfo;

// Ganchos chamados pelas regras (trok_gamerules.cpp).
void TrokSerie_PlayerSpawn( CBasePlayer *pPlayer );
void TrokSerie_PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );	// depois do BaseClass::PlayerKilled
void TrokSerie_RodadaNova( void );													// modos de rodada (Surto)
#endif

#endif // TROK_KILLSTREAK_H
