//=============================================================================
// Trok: economia estilo CS (servidor). Desligada por padrão (trok_economia 0).
//
// Dinheiro por jogador (em rede, trok_placar_extra), ganho por abate conforme a classe da
// arma, bônus de vitória/derrota nos modos de rodada, teto, compra num tempo e numa área
// (func_buyzone dos mapas do CS:S ou perto de onde nasceu), comando "trok_comprar <item>",
// equipamento que vale no jogo (colete: armadura + fôlego + velocidade; capacete: tiro na
// cabeça). Preços e itens em scripts/trok_loja.txt; regras em cfg/economia.cfg.
// Ver design/economia.md.
//=============================================================================
#ifndef TROK_ECONOMIA_H
#define TROK_ECONOMIA_H
#ifdef _WIN32
#pragma once
#endif

class CBasePlayer;
class CTakeDamageInfo;
class CCommand;

bool TrokEconomia_Ligada( void );

// Ganchos (trok_gamerules.cpp)
void TrokEconomia_PlayerSpawn( CBasePlayer *pPlayer );								// prazo e lugar da compra
void TrokEconomia_PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );	// ganho, perde o equipamento
void TrokEconomia_FimRodada( int iTimeVencedor );									// bônus de rodada (0 = empate)
bool TrokEconomia_ClientCommand( CBasePlayer *pPlayer, const CCommand &args );		// trok_comprar
// Bônus da série de abates (trok_killstreak.cpp): dinheiro de humano; zumbi no Surto ganha Fúria.
void TrokEconomia_BonusSerie( CBasePlayer *pKiller, int iMulti, int iSerie, bool bVinganca, bool bPrimeiro );
void TrokEconomia_SerieNovaVida( CBasePlayer *pPlayer );

// Gancho em CHL2MP_Player::GiveDefaultItems (hl2mp_player.cpp): com a economia ligada dá só
// o kit inicial (pistola) e o que o jogador comprou e não perdeu. true = já deu tudo.
bool TrokEconomia_DarEquipamento( CBasePlayer *pPlayer );

// Gancho no tiro na cabeça (server/player.cpp, TraceAttack): multiplicador do crítico com
// o capacete comprado (recebe o sk_player_head e devolve o que vale).
float TrokEquip_MultCabeca( CBasePlayer *pVictim, float flMult );

#endif // TROK_ECONOMIA_H
