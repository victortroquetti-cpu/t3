//=============================================================================
// Trok: faca no molde do Combat Arms (M9, inventory.md seção 8). Números no sub-bloco
// "CA" de scripts/weapon_trok_faca.txt.
//
//   Botão 1 (golpe): animação de 0,65 s que acerta duas vezes, aos 0,155 s e aos 0,402 s,
//   45 cada. Botão 2 (estocada): 1,1 s, um acerto aos 0,367 s, 75. Alcance 105.
//   Dano x U(0,9; 1,1), x1,3 vindo de trás (trok_ca_dano_costas), parte do corpo como bala.
//   Soltar o botão não para o golpe; segurando, o próximo começa sem intervalo; com os
//   dois apertados vale o botão 1. Sacar: 0,45 s. Velocidade: trok_faca_velocidade.
//
// Deriva da arma branca do HL2MP para os bots a reconhecerem como faca.
//=============================================================================

#include "cbase.h"
#include "in_buttons.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "weapon_hl2mpbasebasebludgeon.h"
#include "hl2mp_weapon_parse.h"

#ifdef CLIENT_DLL
	#include "c_hl2mp_player.h"
	#define CWeaponTrokFaca C_WeaponTrokFaca
#else
	#include "hl2mp_player.h"
	#include "ilagcompensationmanager.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar trok_ca_dano_costas;	// weapon_trok_gun.cpp

class CWeaponTrokFaca : public CBaseHL2MPBludgeonWeapon
{
public:
	DECLARE_CLASS( CWeaponTrokFaca, CBaseHL2MPBludgeonWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

	CWeaponTrokFaca();

	const TrokArmaCA_t &CA( void ) const { return GetHL2MPWpnData().m_TrokCA; }

	virtual float GetRange( void ) { return CA().flFacaAlcance; }
	virtual float GetFireRate( void ) { return CA().flFacaGolpe[ 0 ]; }
	virtual float GetDamageForActivity( Activity hitActivity ) { return CA().flFacaGolpe[ 1 ]; }

	virtual void PrimaryAttack( void ) {}
	virtual void SecondaryAttack( void ) {}
	virtual void ItemPostFrame( void );
	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

private:
	void Comecar( int iTipo );
	void Acertar( void );

	CNetworkVar( int, m_iGolpe );			// 0 parado, 1 golpe, 2 estocada
	CNetworkVar( float, m_flGolpeInicio );
	CNetworkVar( int, m_iGolpeAcertos );	// acertos já feitos neste golpe

	CWeaponTrokFaca( const CWeaponTrokFaca & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponTrokFaca, DT_WeaponTrokFaca )

BEGIN_NETWORK_TABLE( CWeaponTrokFaca, DT_WeaponTrokFaca )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iGolpe ) ),
	RecvPropTime( RECVINFO( m_flGolpeInicio ) ),
	RecvPropInt( RECVINFO( m_iGolpeAcertos ) ),
#else
	SendPropInt( SENDINFO( m_iGolpe ), 2, SPROP_UNSIGNED ),
	SendPropTime( SENDINFO( m_flGolpeInicio ) ),
	SendPropInt( SENDINFO( m_iGolpeAcertos ), 2, SPROP_UNSIGNED ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponTrokFaca )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_iGolpe, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flGolpeInicio, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_iGolpeAcertos, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_trok_faca, CWeaponTrokFaca );
PRECACHE_WEAPON_REGISTER( weapon_trok_faca );

#ifndef CLIENT_DLL
acttable_t CWeaponTrokFaca::m_acttable[] =
{
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_SLAM, true },
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_MELEE,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_MELEE,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_MELEE,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_MELEE,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_MELEE,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_MELEE,			false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_MELEE,					false },
};

IMPLEMENT_ACTTABLE( CWeaponTrokFaca );
#endif

CWeaponTrokFaca::CWeaponTrokFaca()
{
	m_iGolpe = 0;
	m_flGolpeInicio = 0.0f;
	m_iGolpeAcertos = 0;
}

bool CWeaponTrokFaca::Deploy( void )
{
	m_iGolpe = 0;
	const bool bOk = BaseClass::Deploy();
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	const float flSacar = CA().flTempoSacar;
	if ( bOk && pPlayer && flSacar > 0.0f )
	{
		pPlayer->SetNextAttack( gpGlobals->curtime + flSacar );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + flSacar;
	}
	return bOk;
}

bool CWeaponTrokFaca::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_iGolpe = 0;	// trocar perde os acertos que faltavam
	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponTrokFaca::Comecar( int iTipo )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;
	m_iGolpe = iTipo;
	m_flGolpeInicio = gpGlobals->curtime;
	m_iGolpeAcertos = 0;
	const float flDur = ( iTipo == 1 ) ? CA().flFacaGolpe[ 0 ] : CA().flFacaEstocada[ 0 ];
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + flDur;
	SendWeaponAnim( iTipo == 1 ? ACT_VM_MISSCENTER : ACT_VM_HITCENTER );
	WeaponSound( SINGLE );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
}

// Um acerto: raio do olho até o alcance (com uma caixa pequena se o raio passar raspando).
void CWeaponTrokFaca::Acertar( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	const Vector vecIni = pPlayer->Weapon_ShootPosition();
	Vector vecFrente;
	pPlayer->EyeVectors( &vecFrente );
	const Vector vecFim = vecIni + vecFrente * GetRange();

#ifdef GAME_DLL
	CHL2MP_Player *pHL2 = ToHL2MPPlayer( pPlayer );
	if ( pHL2 )
		lagcompensation->StartLagCompensation( pHL2, pHL2->GetCurrentCommand() );
#endif
	trace_t tr;
	UTIL_TraceLine( vecIni, vecFim, MASK_SHOT_HULL, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction >= 1.0f )
	{
		// O CA testa a caixa da vítima alargada; aqui, uma caixa de +-6 u.
		static const Vector s_mins( -6, -6, -6 ), s_maxs( 6, 6, 6 );
		UTIL_TraceHull( vecIni, vecFim - vecFrente * 6.0f, s_mins, s_maxs, MASK_SHOT_HULL, pPlayer, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0f && tr.m_pEnt )
		{
			Vector vecAlvo = tr.m_pEnt->WorldSpaceCenter() - vecIni;
			VectorNormalize( vecAlvo );
			if ( DotProduct( vecAlvo, vecFrente ) < 0.70721f )
				tr.fraction = 1.0f;
		}
	}

	if ( tr.fraction < 1.0f && tr.m_pEnt )
	{
		const float flBase = ( m_iGolpe == 1 ) ? CA().flFacaGolpe[ 1 ] : CA().flFacaEstocada[ 1 ];
		const float k = CA().flDanoVariacao;
		float flDano = flBase * SharedRandomFloat( "TrokFacaDano", 1.0f - k, 1.0f + k, m_iGolpeAcertos );
		if ( tr.m_pEnt->IsPlayer() )
		{
			Vector vecF, vecR;
			AngleVectors( QAngle( 0.0f, tr.m_pEnt->GetAbsAngles()[ YAW ], 0.0f ), &vecF, &vecR, NULL );
			const float th = atan2f( DotProduct( vecF, vecFrente ), DotProduct( vecR, vecFrente ) );
			if ( th >= M_PI_F / 6.0f && th < M_PI_F )
				flDano *= trok_ca_dano_costas.GetFloat();
		}
#ifdef GAME_DLL
		CTakeDamageInfo info( pPlayer, pPlayer, flDano, DMG_CLUB );
		CalculateMeleeDamageForce( &info, vecFrente, tr.endpos );
		tr.m_pEnt->DispatchTraceAttack( info, vecFrente, &tr );
		ApplyMultiDamage();
		TraceAttackToTriggers( info, tr.startpos, tr.endpos, vecFrente );
#endif
		WeaponSound( tr.m_pEnt->IsPlayer() || tr.m_pEnt->IsNPC() ? MELEE_HIT : MELEE_HIT_WORLD );
		ImpactEffect( tr );
	}
#ifdef GAME_DLL
	if ( pHL2 )
	{
		lagcompensation->FinishLagCompensation( pHL2 );
		pHL2->OnMyWeaponFired( this );
	}
#endif
}

void CWeaponTrokFaca::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	if ( m_iGolpe != 0 )
	{
		const float t = gpGlobals->curtime - m_flGolpeInicio;
		float flAcertos[ 2 ];
		int nAcertos;
		float flDur;
		if ( m_iGolpe == 1 )
		{
			flAcertos[ 0 ] = CA().flFacaGolpe[ 2 ];
			flAcertos[ 1 ] = CA().flFacaGolpe[ 3 ];
			nAcertos = ( flAcertos[ 1 ] > 0.0f ) ? 2 : 1;
			flDur = CA().flFacaGolpe[ 0 ];
		}
		else
		{
			flAcertos[ 0 ] = CA().flFacaEstocada[ 2 ];
			flAcertos[ 1 ] = 0.0f;
			nAcertos = 1;
			flDur = CA().flFacaEstocada[ 0 ];
		}
		while ( m_iGolpeAcertos < nAcertos && t >= flAcertos[ m_iGolpeAcertos ] )
		{
			Acertar();
			m_iGolpeAcertos = m_iGolpeAcertos + 1;
		}
		if ( t < flDur )
			return;
		m_iGolpe = 0;
		// Segurando, o próximo golpe começa na hora (botão 1 vale mais que o 2).
		if ( pPlayer->m_nButtons & IN_ATTACK )
			Comecar( 1 );
		else if ( pPlayer->m_nButtons & IN_ATTACK2 )
			Comecar( 2 );
		return;
	}

	if ( ( pPlayer->m_nButtons & IN_ATTACK ) && m_flNextPrimaryAttack <= gpGlobals->curtime )
		Comecar( 1 );
	else if ( ( pPlayer->m_nButtons & IN_ATTACK2 ) && m_flNextSecondaryAttack <= gpGlobals->curtime )
		Comecar( 2 );
	else
		WeaponIdle();
}
