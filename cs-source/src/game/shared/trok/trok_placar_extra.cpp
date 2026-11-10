//=============================================================================
// Trok: números extras de cada jogador em rede. Ver trok_placar_extra.h.
//=============================================================================

#include "cbase.h"
#include "trok_placar_extra.h"
#include "trok_surto.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CHandle< CTrokPlacarExtra > s_hPlacarExtra;

IMPLEMENT_NETWORKCLASS_ALIASED( TrokPlacarExtra, DT_TrokPlacarExtra )

BEGIN_NETWORK_TABLE_NOBASE( CTrokPlacarExtra, DT_TrokPlacarExtra )
#ifdef CLIENT_DLL
	RecvPropArray3( RECVINFO_ARRAY( m_aSerie ), RecvPropInt( RECVINFO( m_aSerie[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aMelhorSerie ), RecvPropInt( RECVINFO( m_aMelhorSerie[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aDinheiro ), RecvPropInt( RECVINFO( m_aDinheiro[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aCompraAte ), RecvPropFloat( RECVINFO( m_aCompraAte[ 0 ] ) ) ),
	RecvPropBool( RECVINFO( m_bEconomia ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aColete ), RecvPropInt( RECVINFO( m_aColete[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aCapacete ), RecvPropInt( RECVINFO( m_aCapacete[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aEquipVel ), RecvPropFloat( RECVINFO( m_aEquipVel[ 0 ] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_aEquipFolego ), RecvPropFloat( RECVINFO( m_aEquipFolego[ 0 ] ) ) ),
#else
	SendPropArray3( SENDINFO_ARRAY3( m_aSerie ), SendPropInt( SENDINFO_ARRAY( m_aSerie ), 10, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aMelhorSerie ), SendPropInt( SENDINFO_ARRAY( m_aMelhorSerie ), 10, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aDinheiro ), SendPropInt( SENDINFO_ARRAY( m_aDinheiro ), 20, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aCompraAte ), SendPropFloat( SENDINFO_ARRAY( m_aCompraAte ), 0, SPROP_NOSCALE ) ),
	SendPropBool( SENDINFO( m_bEconomia ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aColete ), SendPropInt( SENDINFO_ARRAY( m_aColete ), 4, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aCapacete ), SendPropInt( SENDINFO_ARRAY( m_aCapacete ), 8, SPROP_UNSIGNED ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aEquipVel ), SendPropFloat( SENDINFO_ARRAY( m_aEquipVel ), 0, SPROP_NOSCALE ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_aEquipFolego ), SendPropFloat( SENDINFO_ARRAY( m_aEquipFolego ), 0, SPROP_NOSCALE ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( trok_placar_extra, CTrokPlacarExtra );

CTrokPlacarExtra::CTrokPlacarExtra()
{
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
	{
		m_aSerie.Set( i, 0 );
		m_aMelhorSerie.Set( i, 0 );
		m_aDinheiro.Set( i, 0 );
		m_aCompraAte.Set( i, 0.0f );
		m_aColete.Set( i, 0 );
		m_aCapacete.Set( i, 0 );
		m_aEquipVel.Set( i, 1.0f );
		m_aEquipFolego.Set( i, 1.0f );
	}
	m_bEconomia = false;
	s_hPlacarExtra = this;
}

CTrokPlacarExtra::~CTrokPlacarExtra()
{
	if ( s_hPlacarExtra.Get() == this )
		s_hPlacarExtra = NULL;
}

CTrokPlacarExtra *TrokPlacarExtra( void )
{
#ifndef CLIENT_DLL
	if ( !s_hPlacarExtra.Get() )
	{
		CTrokPlacarExtra *p = static_cast< CTrokPlacarExtra * >( CreateEntityByName( "trok_placar_extra" ) );
		if ( p )
			DispatchSpawn( p );
	}
#endif
	return s_hPlacarExtra.Get();
}

float TrokEquip_Velocidade( CBasePlayer *pPlayer )
{
	CTrokPlacarExtra *p = s_hPlacarExtra.Get();
	if ( !p || !pPlayer || !p->IsEconomia() || TrokSurto_IsZumbi( pPlayer ) )
		return 1.0f;
	const float f = p->GetEquipVelocidade( pPlayer->entindex() );
	return f > 0.0f ? f : 1.0f;
}
