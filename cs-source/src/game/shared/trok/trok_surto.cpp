//=============================================================================
// Trok: modo Surto, estado em rede. Ver trok_surto.h. As regras ficam em
// trok_gamerules.cpp (servidor) e o HUD em client/trok/hud_trok.cpp.
//=============================================================================

#include "cbase.h"
#include "trok_surto.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar trok_modo( "trok_modo", "equipe", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Modo de jogo do próximo mapa: equipe (mata-mata em equipe), solo (exército solo, todos contra todos) ou surto (infecção)." );
ConVar trok_renascer_tempo( "trok_renascer_tempo", "5", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Equipe e solo: segundos morto até renascer sozinho (com contagem na tela)." );
ConVar trok_protecao( "trok_protecao", "3", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Equipe e solo: segundos de proteção ao nascer (acaba antes se atirar)." );

int TrokModo( void )
{
	const char *psz = trok_modo.GetString();
	if ( !V_stricmp( psz, "surto" ) )
		return TROK_MODO_SURTO;
	if ( !V_stricmp( psz, "solo" ) || !V_stricmp( psz, "ffa" ) )
		return TROK_MODO_SOLO;
	return TROK_MODO_EQUIPE; // "equipe", "tdm" e qualquer outro
}

ConVar trok_surto_zumbi_pulo( "trok_surto_zumbi_pulo", "1.5", FCVAR_REPLICATED | FCVAR_NOTIFY, "Multiplica a altura do pulo dos zumbis." );
ConVar trok_surto_zumbi_velocidade( "trok_surto_zumbi_velocidade", "1.1", FCVAR_REPLICATED | FCVAR_NOTIFY, "Multiplica a velocidade dos zumbis (com as garras na mão)." );

static CHandle< CTrokSurtoEstado > s_hEstado;

IMPLEMENT_NETWORKCLASS_ALIASED( TrokSurtoEstado, DT_TrokSurtoEstado )

BEGIN_NETWORK_TABLE_NOBASE( CTrokSurtoEstado, DT_TrokSurtoEstado )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iFase ) ),
	RecvPropFloat( RECVINFO( m_flFimFase ) ),
	RecvPropInt( RECVINFO( m_iVencedor ) ),
	RecvPropBool( RECVINFO( m_bAlarme ) ),
	RecvPropBool( RECVINFO( m_bAbrigos ) ),
	RecvPropInt( RECVINFO( m_iHumanos ) ),
	RecvPropInt( RECVINFO( m_iZumbis ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aInfeccoes ), RecvPropInt( RECVINFO( m_aInfeccoes[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aZumbisAbatidos ), RecvPropInt( RECVINFO( m_aZumbisAbatidos[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aSobreviveu ), RecvPropInt( RECVINFO( m_aSobreviveu[ 0 ] ) ) ),
	RecvPropInt( RECVINFO( m_iDestaque ) ),
	RecvPropInt( RECVINFO( m_iDestaqueTipo ) ),
	RecvPropInt( RECVINFO( m_iDestaqueValor ) ),
#else
	SendPropInt( SENDINFO( m_iFase ), 3, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flFimFase ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_iVencedor ), 2, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bAlarme ) ),
	SendPropBool( SENDINFO( m_bAbrigos ) ),
	SendPropInt( SENDINFO( m_iHumanos ), 7, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iZumbis ), 7, SPROP_UNSIGNED ),
	SendPropArray3( SENDINFO_ARRAY3( m_aInfeccoes ), SendPropInt( SENDINFO_ARRAY( m_aInfeccoes ), 10, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aZumbisAbatidos ), SendPropInt( SENDINFO_ARRAY( m_aZumbisAbatidos ), 10, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aSobreviveu ), SendPropInt( SENDINFO_ARRAY( m_aSobreviveu ), 8, SPROP_UNSIGNED ) ),
	SendPropInt( SENDINFO( m_iDestaque ), 7, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDestaqueTipo ), 2, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iDestaqueValor ), 8, SPROP_UNSIGNED ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( trok_surto_estado, CTrokSurtoEstado );

CTrokSurtoEstado::CTrokSurtoEstado()
{
	m_iFase = SURTO_DESLIGADO;
	m_flFimFase = 0.0f;
	m_iVencedor = SURTO_NINGUEM;
	m_bAlarme = false;
	m_bAbrigos = false;
	m_iHumanos = 0;
	m_iZumbis = 0;
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
	{
		m_aInfeccoes.Set( i, 0 );
		m_aZumbisAbatidos.Set( i, 0 );
		m_aSobreviveu.Set( i, 0 );
	}
	m_iDestaque = 0;
	m_iDestaqueTipo = SURTO_DESTAQUE_NENHUM;
	m_iDestaqueValor = 0;
	s_hEstado = this;
}

CTrokSurtoEstado::~CTrokSurtoEstado()
{
	if ( s_hEstado.Get() == this )
		s_hEstado = NULL;
}

CTrokSurtoEstado *TrokSurto_Estado( void )
{
	return s_hEstado.Get();
}

bool TrokSurto_Ativo( void )
{
	CTrokSurtoEstado *p = s_hEstado.Get();
	return p && p->GetFase() != SURTO_DESLIGADO;
}

bool TrokSurto_IsZumbi( const CBaseEntity *pEnt )
{
	return pEnt && pEnt->IsPlayer() && TrokSurto_Ativo() && pEnt->GetTeamNumber() == TEAM_TROK_ZUMBIS;
}

bool TrokSurto_IsHumano( const CBaseEntity *pEnt )
{
	return pEnt && pEnt->IsPlayer() && TrokSurto_Ativo() && pEnt->GetTeamNumber() == TEAM_TROK_HUMANOS;
}
