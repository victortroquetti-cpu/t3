//=============================================================================
// Trok: modo Surto (sucessor da quarentena do Combat Arms; ver design/modo-surto.md).
//
// Estado da rodada, compartilhado: uma entidade em rede (trok_surto_estado) que o
// servidor cria quando trok_modo é "surto" e o HUD lê. Humanos e zumbis são os dois
// times do HL2MP (sem matar ninguém na troca de time).
//=============================================================================
#ifndef TROK_SURTO_H
#define TROK_SURTO_H
#ifdef _WIN32
#pragma once
#endif

#include "hl2mp_gamerules.h"

#ifdef CLIENT_DLL
	#define CTrokSurtoEstado C_TrokSurtoEstado
#endif

#define TEAM_TROK_HUMANOS	TEAM_REBELS
#define TEAM_TROK_ZUMBIS	TEAM_COMBINE

enum TrokSurtoFase_t
{
	SURTO_DESLIGADO = 0,
	SURTO_PREPARO,		// todos humanos, posicionando
	SURTO_JOGO,			// zumbis soltos, relógio correndo
	SURTO_FIM,			// mostra o vencedor, depois rodada nova
};

enum TrokSurtoVencedor_t
{
	SURTO_NINGUEM = 0,
	SURTO_HUMANOS,
	SURTO_ZUMBIS,
};

class CTrokSurtoEstado : public CBaseEntity
{
public:
	DECLARE_CLASS( CTrokSurtoEstado, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CTrokSurtoEstado();
	virtual ~CTrokSurtoEstado();

#ifndef CLIENT_DLL
	virtual int UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_ALWAYS ); }
#endif

	int GetFase( void ) const { return m_iFase; }
	float GetFimFase( void ) const { return m_flFimFase; }
	int GetVencedor( void ) const { return m_iVencedor; }
	bool IsAlarme( void ) const { return m_bAlarme; }
	bool IsAbrigos( void ) const { return m_bAbrigos; }
	int GetHumanosVivos( void ) const { return m_iHumanos; }
	int GetZumbisVivos( void ) const { return m_iZumbis; }

	CNetworkVar( int, m_iFase );
	CNetworkVar( float, m_flFimFase );		// curtime em que a fase acaba
	CNetworkVar( int, m_iVencedor );
	CNetworkVar( bool, m_bAlarme );
	CNetworkVar( bool, m_bAbrigos );
	CNetworkVar( int, m_iHumanos );
	CNetworkVar( int, m_iZumbis );
};

CTrokSurtoEstado *TrokSurto_Estado( void );
bool TrokSurto_Ativo( void );					// existe estado e a fase não é DESLIGADO
bool TrokSurto_IsZumbi( const CBaseEntity *pEnt );	// jogador no time dos zumbis, com o modo ativo
bool TrokSurto_IsHumano( const CBaseEntity *pEnt );

extern ConVar trok_modo;
extern ConVar trok_surto_zumbi_pulo;
extern ConVar trok_surto_zumbi_velocidade;

#endif // TROK_SURTO_H
