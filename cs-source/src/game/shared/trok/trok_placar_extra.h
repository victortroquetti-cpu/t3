//=============================================================================
// Trok: números extras de cada jogador, em rede (uma entidade só, trok_placar_extra).
//
// - série de abates desta vida e a maior série no mapa (killstreak, trok_killstreak.cpp);
// - dinheiro e até quando pode comprar (economia, server/trok/trok_economia.cpp);
// - equipamento comprado: nível do colete, capacete, multiplicadores de velocidade e fôlego
//   (o movimento lê a velocidade dos dois lados, por isso vai em rede).
// O servidor cria a entidade no começo do mapa; ela sobrevive à limpeza entre as rodadas
// (lista s_PreserveEnts do hl2mp_gamerules.cpp, patch). O HUD e o placar só leem.
//=============================================================================
#ifndef TROK_PLACAR_EXTRA_H
#define TROK_PLACAR_EXTRA_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
	#define CTrokPlacarExtra C_TrokPlacarExtra
#endif

class CTrokPlacarExtra : public CBaseEntity
{
public:
	DECLARE_CLASS( CTrokPlacarExtra, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CTrokPlacarExtra();
	virtual ~CTrokPlacarExtra();

#ifndef CLIENT_DLL
	virtual int UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_ALWAYS ); }
#endif

	static bool Valido( int i ) { return i > 0 && i <= MAX_PLAYERS; }

	int GetSerie( int i ) const { return Valido( i ) ? m_aSerie[ i ] : 0; }				// abates seguidos nesta vida
	int GetMelhorSerie( int i ) const { return Valido( i ) ? m_aMelhorSerie[ i ] : 0; }	// maior série no mapa
	int GetDinheiro( int i ) const { return Valido( i ) ? m_aDinheiro[ i ] : 0; }
	float GetCompraAte( int i ) const { return Valido( i ) ? m_aCompraAte[ i ] : 0.0f; }	// curtime; 0 = não compra
	bool IsEconomia( void ) const { return m_bEconomia; }
	int GetColete( int i ) const { return Valido( i ) ? m_aColete[ i ] : 0; }				// nível (0 = sem)
	int GetCapacete( int i ) const { return Valido( i ) ? m_aCapacete[ i ] : 0; }			// tiros que aguenta (255 = não quebra; 0 = sem)
	float GetEquipVelocidade( int i ) const { return Valido( i ) ? m_aEquipVel[ i ] : 1.0f; }
	float GetEquipFolego( int i ) const { return Valido( i ) ? m_aEquipFolego[ i ] : 1.0f; }

	CNetworkArray( int, m_aSerie, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aMelhorSerie, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aDinheiro, MAX_PLAYERS + 1 );
	CNetworkArray( float, m_aCompraAte, MAX_PLAYERS + 1 );
	CNetworkVar( bool, m_bEconomia );
	CNetworkArray( int, m_aColete, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aCapacete, MAX_PLAYERS + 1 );
	CNetworkArray( float, m_aEquipVel, MAX_PLAYERS + 1 );
	CNetworkArray( float, m_aEquipFolego, MAX_PLAYERS + 1 );
};

// No servidor cria a entidade se ainda não existe; no cliente pode voltar NULL.
CTrokPlacarExtra *TrokPlacarExtra( void );

// Multiplicador de velocidade do equipamento comprado (colete). 1 sem economia, sem colete ou
// zumbi do Surto. Chamado pelo movimento (trok_gamemovement.cpp), nos dois lados.
float TrokEquip_Velocidade( CBasePlayer *pPlayer );

#endif // TROK_PLACAR_EXTRA_H
