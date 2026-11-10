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
	virtual void ItemBusyFrame( void );
	virtual bool SendWeaponAnim( int iActivity );
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
	bool HasADS( void ) const;
	bool UsaCA( void ) const;					// segue a lei de tiro do Combat Arms (script "modelo_ca" 1)
	int CAConeAtual( void ) const;				// cone do CA agora, em milésimos (padrão quadrado)
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
	int GetADSFov( CBasePlayer *pPlayer ) const;
#ifdef CLIENT_DLL
	void ClaraoSemAnimacao( void );
#endif
	float DamageAtDistance( float flDistance ) const;

	// Modelo do Combat Arms (weapon_firing.md): medidores de dispersão a 80 Hz, cone
	// quadrado, dano em degraus, recuo de câmera do jogador, tempos de recarga e de sacar.
	void CATique( bool bAtirou );
	void CAAtirar( CBasePlayer *pPlayer );
	void CARecuo( CBasePlayer *pPlayer );
	void CARecuoVolta( CBasePlayer *pPlayer );
	float CAFaixa( float flDistancia ) const;
	float CASaidaCorrida( void ) const;
	bool SendWeaponAnimTempo( int iActivity, float flTempo );
	void CARecargaBalaAvanca( CBasePlayer *pPlayer );
	bool m_bCAAtirouNoTique;					// tiro neste tique (o 1º subpasso conta o tiro)

	CNetworkVar( float, m_flExtraSpread );		// graus somados pelos tiros seguidos
	CNetworkVar( int, m_iShotsInRow );
	CNetworkVar( float, m_flLastShotTime );
	CNetworkVar( bool, m_bTriggerHeld );
	CNetworkVar( bool, m_bADS );
	CNetworkVar( float, m_flSprintEndTime );		// quando parou de correr
	CNetworkVar( bool, m_bWasSprinting );
	CNetworkVar( float, m_flCAFogo );			// CA: medidor de dispersão do tiro (F, 0..1)
	CNetworkVar( float, m_flCAMov );			// CA: medidor de dispersão do movimento (M, 0..1)
	CNetworkVar( int, m_iRecargaFase );			// recarga bala por bala: 0 não, 1 início, 2 bala, 3 fim
	CNetworkVar( float, m_flRecargaProx );		// ... quando acaba a fase atual

	float m_flViewmodelSprint;					// só cliente: 0 = normal, 1 = correndo
	float m_flBobPhase;							// só cliente: fase do passo (anda com a distância, não com o tempo)
	float m_flBobAmount;						// só cliente: força do balanço (sobe com a velocidade)
	float m_flViewmodelADS;						// só cliente: 0 = quadril, 1 = mira
	float m_flCoiceTempo;						// só cliente: último tiro mirando (coice procedural)

	CWeaponTrokGun( const CWeaponTrokGun & );
};

// Multiplicador de velocidade da arma na mão do jogador (1 se não for arma do Trok).
float TrokWeapon_MoveSpeedScale( CBasePlayer *pPlayer );

#endif // WEAPON_TROK_GUN_H
