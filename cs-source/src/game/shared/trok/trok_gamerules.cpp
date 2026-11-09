//=============================================================================
// Trok: regras de jogo do mod (esqueleto). Ver trok_gamerules.h.
//=============================================================================

#include "cbase.h"
#include "trok_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// O servidor cria as regras por nome em InstallGameRules() (ver
// patches/sdk.patch, hl2mp_client.cpp) e o cliente cria a classe com o
// mesmo nome quando a string chega.
REGISTER_GAMERULES_CLASS( CTrokRules );

CTrokRules::CTrokRules()
{
}

CTrokRules::~CTrokRules()
{
}

const char *CTrokRules::GetGameDescription( void )
{
	return "Trok";
}

#ifndef CLIENT_DLL

ConVar trok_vida( "trok_vida", "100", FCVAR_NOTIFY, "Vida (e vida máxima) ao nascer." );
ConVar trok_colete( "trok_colete", "0", FCVAR_NOTIFY, "Colete (armadura do HL2) ao nascer." );

// Chamado dentro de CBasePlayer::Spawn, depois da vida padrão e antes das armas.
void CTrokRules::PlayerSpawn( CBasePlayer *pPlayer )
{
	BaseClass::PlayerSpawn( pPlayer );

	int iHealth = MAX( 1, trok_vida.GetInt() );
	pPlayer->SetMaxHealth( iHealth );
	pPlayer->SetHealth( iHealth );
	pPlayer->SetArmorValue( MAX( 0, trok_colete.GetInt() ) );
}

#endif
