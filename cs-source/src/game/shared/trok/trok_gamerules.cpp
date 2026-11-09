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
