//=============================================================================
// Trok: regras de jogo do mod (esqueleto).
//
// Por enquanto só herda do HL2MP: jogador, armas e bots do HL2MP continuam
// valendo. É o ponto de entrada para os sistemas que forem refeitos: cada um
// sai do HL2MP e passa a morar em trok/ aos poucos.
//
// Sem variáveis de rede próprias ainda: o cliente recebe a tabela
// DT_HL2MPRules pelo proxy do HL2MP. Quando precisar mandar estado para o
// HUD, crie um proxy trok_gamerules com tabela própria (skill source-mod,
// references/rede-e-predicao.md).
//=============================================================================

#ifndef TROK_GAMERULES_H
#define TROK_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "hl2mp_gamerules.h"

#ifdef CLIENT_DLL
	#define CTrokRules C_TrokRules
#endif

class CTrokRules : public CHL2MPRules
{
public:
	DECLARE_CLASS( CTrokRules, CHL2MPRules );

	CTrokRules();
	virtual ~CTrokRules();

	virtual const char *GetGameDescription( void );

#ifndef CLIENT_DLL
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
#endif
};

inline CTrokRules *TrokRules( void )
{
	return static_cast< CTrokRules * >( g_pGameRules );
}

#endif // TROK_GAMERULES_H
