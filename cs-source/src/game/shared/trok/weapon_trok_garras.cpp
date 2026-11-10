//=============================================================================
// Trok: garras do zumbi (modo Surto). Arma corpo a corpo sobre a base de arma
// branca do HL2MP (mesmo molde do weapon_crowbar do SDK). Humano morto por zumbi
// vira zumbi (regras em trok_gamerules.cpp). Números em ConVars trok_surto_garra_*.
//=============================================================================

#include "cbase.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "weapon_hl2mpbasebasebludgeon.h"
#include "in_buttons.h"
#include "trok_zumbis.h"

#ifdef CLIENT_DLL
	#include "c_hl2mp_player.h"
	#define CWeaponTrokGarras C_WeaponTrokGarras
#else
	#include "hl2mp_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar trok_surto_garra_dano( "trok_surto_garra_dano", "40", FCVAR_REPLICATED | FCVAR_NOTIFY, "Dano de cada golpe das garras do zumbi." );
ConVar trok_surto_garra_alcance( "trok_surto_garra_alcance", "70", FCVAR_REPLICATED, "Alcance (u) do golpe das garras." );
ConVar trok_surto_garra_cadencia( "trok_surto_garra_cadencia", "0.45", FCVAR_REPLICATED, "Segundos entre golpes das garras." );

class CWeaponTrokGarras : public CBaseHL2MPBludgeonWeapon
{
public:
	DECLARE_CLASS( CWeaponTrokGarras, CBaseHL2MPBludgeonWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif

	CWeaponTrokGarras() {}

	// Multiplicadores do tipo de zumbi (Crusher: mais alcance, mais lento; trok_zumbis.cpp).
	virtual float GetRange( void ) { return trok_surto_garra_alcance.GetFloat() * TrokZumbis_GarraAlcance( GetOwner() ); }
	virtual float GetFireRate( void ) { return trok_surto_garra_cadencia.GetFloat() * TrokZumbis_GarraCadencia( GetOwner() ); }
	virtual float GetDamageForActivity( Activity hitActivity ) { return trok_surto_garra_dano.GetFloat() * TrokZumbis_GarraDano( GetOwner() ); }
	virtual void SecondaryAttack( void ) {}

	virtual void AddViewKick( void )
	{
		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if ( pPlayer )
			pPlayer->ViewPunch( QAngle( SharedRandomFloat( "trokgarrax", 0.5f, 1.5f ), SharedRandomFloat( "trokgarray", -1.5f, 1.5f ), 0.0f ) );
	}

	// Garras não caem no chão quando o zumbi morre.
	virtual void Drop( const Vector &vecVelocity )
	{
#ifndef CLIENT_DLL
		UTIL_Remove( this );
#endif
	}

	CWeaponTrokGarras( const CWeaponTrokGarras & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponTrokGarras, DT_WeaponTrokGarras )

BEGIN_NETWORK_TABLE( CWeaponTrokGarras, DT_WeaponTrokGarras )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponTrokGarras )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_trok_garras, CWeaponTrokGarras );
PRECACHE_WEAPON_REGISTER( weapon_trok_garras );

#ifndef CLIENT_DLL
acttable_t CWeaponTrokGarras::m_acttable[] =
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
IMPLEMENT_ACTTABLE( CWeaponTrokGarras );
#endif
