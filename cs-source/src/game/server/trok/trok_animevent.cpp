//=============================================================================
// Trok: evento de animação do jogador (tiro, recarga, pulo...) mandado aos outros
// clientes, que o tocam no CTrokPlayerAnimState de cada um. Mesma receita do
// HL2MP antigo e do NEOTOKYO;Rebuild (Source 1 SDK License).
//=============================================================================

#include "cbase.h"
#include "basetempentity.h"
#include "Multiplayer/multiplayer_animstate.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CTETrokAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTETrokAnimEvent, CBaseTempEntity );
	DECLARE_SERVERCLASS();

	CTETrokAnimEvent( const char *name ) : CBaseTempEntity( name ) {}

	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTETrokAnimEvent, DT_TETrokAnimEvent )
	SendPropEHandle( SENDINFO( m_hPlayer ) ),
	SendPropInt( SENDINFO( m_iEvent ), Q_log2( PLAYERANIMEVENT_COUNT ) + 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nData ), 32 )
END_SEND_TABLE()

static CTETrokAnimEvent g_TETrokAnimEvent( "TrokAnimEvent" );

// Para todos que veem o jogador, menos ele mesmo quando o evento veio da predição dele.
void TE_TrokAnimEvent( CBasePlayer *pPlayer, PlayerAnimEvent_t event, int nData )
{
	CPVSFilter filter( (const Vector &)pPlayer->EyePosition() );
	filter.UsePredictionRules();

	g_TETrokAnimEvent.m_hPlayer = pPlayer;
	g_TETrokAnimEvent.m_iEvent = event;
	g_TETrokAnimEvent.m_nData = nData;
	g_TETrokAnimEvent.Create( filter, 0 );
}
