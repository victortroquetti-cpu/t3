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

enum TrokSurtoDestaque_t
{
	SURTO_DESTAQUE_NENHUM = 0,
	SURTO_DESTAQUE_INFECCOES,		// zumbi que mais infectou na rodada
	SURTO_DESTAQUE_ZUMBIS,			// humano que mais abateu zumbis na rodada
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

	// Estatísticas do mapa (por índice de jogador) e destaque da última rodada.
	int GetInfeccoes( int i ) const { return ( i > 0 && i <= MAX_PLAYERS ) ? m_aInfeccoes[ i ] : 0; }
	int GetZumbisAbatidos( int i ) const { return ( i > 0 && i <= MAX_PLAYERS ) ? m_aZumbisAbatidos[ i ] : 0; }
	int GetSobreviveu( int i ) const { return ( i > 0 && i <= MAX_PLAYERS ) ? m_aSobreviveu[ i ] : 0; }
	int GetDestaque( void ) const { return m_iDestaque; }
	int GetDestaqueTipo( void ) const { return m_iDestaqueTipo; }		// TrokSurtoDestaque_t
	int GetDestaqueValor( void ) const { return m_iDestaqueValor; }

	// Tipos de zumbi (trok_zumbis.cpp): o tipo de quem é zumbi agora, o escolhido para a
	// próxima vez que virar e até quando está lento (golpe que paralisa, nuvem tóxica).
	int GetClasseZumbi( int i ) const { return ( i > 0 && i <= MAX_PLAYERS ) ? m_aClasseZumbi[ i ] : 0; }
	int GetEscolhaZumbi( int i ) const { return ( i > 0 && i <= MAX_PLAYERS ) ? m_aEscolhaZumbi[ i ] : 0; }
	float GetLentoAte( int i ) const { return ( i > 0 && i <= MAX_PLAYERS ) ? m_aLentoAte[ i ] : 0.0f; }
	float GetLentoFator( int i ) const { return ( i > 0 && i <= MAX_PLAYERS ) ? m_aLentoFator[ i ] : 1.0f; }
	int GetDinheiroInfectado( int i ) const { return ( i > 0 && i <= MAX_PLAYERS ) ? m_aDinheiroInf[ i ] : 0; }

	CNetworkVar( int, m_iFase );
	CNetworkVar( float, m_flFimFase );		// curtime em que a fase acaba
	CNetworkVar( int, m_iVencedor );
	CNetworkVar( bool, m_bAlarme );
	CNetworkVar( bool, m_bAbrigos );
	CNetworkVar( int, m_iHumanos );
	CNetworkVar( int, m_iZumbis );
	CNetworkArray( int, m_aInfeccoes, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aZumbisAbatidos, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aSobreviveu, MAX_PLAYERS + 1 );
	CNetworkVar( int, m_iDestaque );
	CNetworkVar( int, m_iDestaqueTipo );
	CNetworkVar( int, m_iDestaqueValor );
	CNetworkArray( int, m_aClasseZumbi, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aEscolhaZumbi, MAX_PLAYERS + 1 );
	CNetworkArray( float, m_aLentoAte, MAX_PLAYERS + 1 );
	CNetworkArray( float, m_aLentoFator, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aDinheiroInf, MAX_PLAYERS + 1 );	// dinheiro de infectado (compra os tipos de zumbi)
};

CTrokSurtoEstado *TrokSurto_Estado( void );
bool TrokSurto_Ativo( void );					// existe estado e a fase não é DESLIGADO
bool TrokSurto_IsZumbi( const CBaseEntity *pEnt );	// jogador no time dos zumbis, com o modo ativo
bool TrokSurto_IsHumano( const CBaseEntity *pEnt );

// Modos do Trok (trok_modo): equipe (mata-mata em equipe), solo (exército solo, todos contra
// todos) e surto (infecção). Vale no próximo mapa.
enum TrokModo_t
{
	TROK_MODO_EQUIPE = 0,
	TROK_MODO_SOLO,
	TROK_MODO_SURTO,
	TROK_MODO_CORRIDA,		// Corrida Armada (escada de armas; shared/trok/trok_corrida.cpp)
};
int TrokModo( void );

extern ConVar trok_modo;
extern ConVar trok_renascer_tempo;
extern ConVar trok_protecao;
extern ConVar trok_surto_zumbi_pulo;
extern ConVar trok_surto_zumbi_velocidade;

#endif // TROK_SURTO_H
