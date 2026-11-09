//=============================================================================
// Trok: arma de fogo guiada por script. Ver weapon_trok_gun.cpp.
//=============================================================================

#ifndef WEAPON_TROK_GUN_H
#define WEAPON_TROK_GUN_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_hl2mpbase.h"
#include "hl2mp_weapon_parse.h"

#ifdef CLIENT_DLL
	#define CWeaponTrokGun C_WeaponTrokGun
#endif

class CWeaponTrokGun : public CWeaponHL2MPBase
{
public:
	DECLARE_CLASS( CWeaponTrokGun, CWeaponHL2MPBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponTrokGun();

	virtual void Precache( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual bool Reload( void );
	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void Drop( const Vector &vecVelocity );
	virtual const Vector &GetBulletSpread( void );
	virtual float GetFireRate( void );
	virtual acttable_t *ActivityList( int &iActivityCount );
	virtual void AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );

	bool IsADS( void ) const { return m_bADS; }
	float GetMoveSpeedScale( void ) const;		// peso da arma × (mirando ? lentidão da mira : 1)
	float CurrentSpreadDegrees( void ) const;
	bool IsAutomatic( void ) const;
#ifdef CLIENT_DLL
	// Para o HUD: 0 = normal, 1 = correndo com a arma abaixada / mirando pelo cano.
	float GetSprintBlend( void ) const { return m_flViewmodelSprint; }
	float GetADSBlend( void ) const { return m_flViewmodelADS; }
#endif
	const CHL2MPSWeaponInfo &Info( void ) const { return GetHL2MPWpnData(); }

private:
	CBasePlayer *GetPlayerOwner( void ) const { return ToBasePlayer( GetOwner() ); }
	bool IsOwnerSprinting( void ) const;
	void SetADS( bool bOn );
	float DamageAtDistance( float flDistance ) const;

	CNetworkVar( float, m_flExtraSpread );		// graus somados pelos tiros seguidos
	CNetworkVar( int, m_iShotsInRow );
	CNetworkVar( float, m_flLastShotTime );
	CNetworkVar( bool, m_bTriggerHeld );
	CNetworkVar( bool, m_bADS );
	CNetworkVar( float, m_flSprintEndTime );		// quando parou de correr
	CNetworkVar( bool, m_bWasSprinting );

	float m_flViewmodelSprint;					// só cliente: 0 = normal, 1 = correndo
	float m_flViewmodelADS;						// só cliente: 0 = quadril, 1 = mira

	CWeaponTrokGun( const CWeaponTrokGun & );
};

// Multiplicador de velocidade da arma na mão do jogador (1 se não for arma do Trok).
float TrokWeapon_MoveSpeedScale( CBasePlayer *pPlayer );

#endif // WEAPON_TROK_GUN_H
