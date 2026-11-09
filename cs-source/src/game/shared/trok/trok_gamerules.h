//=============================================================================
// Trok: regras de jogo do mod.
//
// Herda do HL2MP (jogador, armas e bots do HL2MP continuam valendo) e acrescenta:
// - vida e colete ao nascer (trok_vida, trok_colete);
// - o modo Surto (trok_modo surto): infecção no estilo da quarentena do Combat Arms,
//   com rodadas, zumbis hospedeiros, relógio, alarme, abrigos e grades. O estado da
//   rodada vai ao cliente pela entidade trok_surto_estado (trok_surto.h).
//=============================================================================

#ifndef TROK_GAMERULES_H
#define TROK_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "hl2mp_gamerules.h"
#include "trok_surto.h"

#ifdef CLIENT_DLL
	#define CTrokRules C_TrokRules
#else
	class CHL2MP_Player;
#endif

class CTrokRules : public CHL2MPRules
{
public:
	DECLARE_CLASS( CTrokRules, CHL2MPRules );

	CTrokRules();
	virtual ~CTrokRules();

	virtual const char *GetGameDescription( void );

#ifndef CLIENT_DLL
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual void Think( void );
	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual bool FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );
	virtual bool CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem );
	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );

	bool IsSurto( void ) const { return m_bSurto; }

	// Chamados por código da Valve (patches/sdk.patch):
	void OnPlayerDamaged( CBasePlayer *pVictim, const CTakeDamageInfo &info );	// player.cpp, depois do dano
	bool IgnoresHeadshot( CBasePlayer *pVictim ) const;							// player.cpp, TraceAttack

private:
	void SurtoThink( void );
	void SurtoPreparo( void );
	void SurtoComecar( void );
	void SurtoFim( int iVencedor );
	void SurtoVirarZumbi( CBasePlayer *pPlayer, bool bHospedeiro );
	void SurtoAplicarZumbi( CBasePlayer *pPlayer );
	void SurtoAplicarHumano( CBasePlayer *pPlayer );
	void SurtoTrocarTime( CBasePlayer *pPlayer, int iTeam );
	void SurtoContar( int &iHumanos, int &iZumbis );
	void SurtoRegenerar( void );
	void SurtoEspectadores( void );
	void SurtoFolegoZumbi( void );
	void SurtoTirarDeEspectador( CBasePlayer *pPlayer );

	bool m_bSurto;
	CHandle< CTrokSurtoEstado > m_hEstado;
	float m_flSurtoTick;
	float m_flVirar[ MAX_PLAYERS + 1 ];			// quando renasce como zumbi (0 = não vai)
	Vector m_vecMorte[ MAX_PLAYERS + 1 ];			// onde morreu (renasce ali)
	bool m_bHospedeiro[ MAX_PLAYERS + 1 ];
	bool m_bNaRodada[ MAX_PLAYERS + 1 ];			// estava no jogo quando a rodada começou
	float m_flUltimoDano[ MAX_PLAYERS + 1 ];
	float m_flProxRegen[ MAX_PLAYERS + 1 ];
	float m_flMorte[ MAX_PLAYERS + 1 ];			// quando morreu (para ir assistir)
	int m_iRodadaInfec[ MAX_PLAYERS + 1 ];			// infecções nesta rodada
	int m_iRodadaZumbis[ MAX_PLAYERS + 1 ];			// zumbis abatidos nesta rodada
	float m_flUltimoThink;
#endif
};

inline CTrokRules *TrokRules( void )
{
	return static_cast< CTrokRules * >( g_pGameRules );
}

#endif // TROK_GAMERULES_H
