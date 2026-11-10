//=============================================================================
// Trok: recebe o evento de animação do jogador (ver server/trok/trok_animevent.cpp).
//=============================================================================

#include "cbase.h"
#include "c_basetempentity.h"
#include "c_hl2mp_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_TETrokAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TETrokAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		C_HL2MP_Player *pPlayer = dynamic_cast< C_HL2MP_Player * >( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
			pPlayer->DoAnimationEvent( (PlayerAnimEvent_t)m_iEvent.Get(), m_nData );
	}

public:
	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TETrokAnimEvent, DT_TETrokAnimEvent, CTETrokAnimEvent );

BEGIN_RECV_TABLE_NOBASE( C_TETrokAnimEvent, DT_TETrokAnimEvent )
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_iEvent ) ),
	RecvPropInt( RECVINFO( m_nData ) )
END_RECV_TABLE()
